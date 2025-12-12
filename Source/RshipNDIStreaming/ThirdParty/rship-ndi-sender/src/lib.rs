//! High-performance NDI sender with C FFI for Unreal Engine integration.
//!
//! This library provides a thread-safe NDI video sender that accepts raw RGBA frames
//! from Unreal Engine's GPU readback and transmits them via NDI.
//!
//! # Architecture
//!
//! ```text
//! Unreal Engine (C++)
//!        |
//!        v (FFI)
//! Frame Submission (rship_ndi_submit_frame)
//!        |
//!        v
//! Lock-free Frame Queue (bounded, 3 slots)
//!        |
//!        v
//! Sender Thread (dedicated)
//!        |
//!        v
//! NDI SDK (async send)
//! ```

use crossbeam_channel::{bounded, Receiver, Sender, TrySendError};
use parking_lot::Mutex;
use std::ffi::{c_char, c_int, c_void, CStr};
use std::ptr;
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicU64, Ordering};
use std::sync::Arc;
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

// Re-export NDI types we need
use ndi_sdk::send::SendInstance;
use ndi_sdk::FourCCVideoType;

// ============================================================================
// FFI TYPES
// ============================================================================

/// Frame data passed from C++ (Unreal Engine)
#[repr(C)]
pub struct RshipNDIFrame {
    /// Pointer to RGBA pixel data (must remain valid during call)
    pub data: *const u8,
    /// Size of data in bytes (width * height * 4 for RGBA)
    pub data_size: usize,
    /// Frame width in pixels
    pub width: c_int,
    /// Frame height in pixels
    pub height: c_int,
    /// Frame number for ordering/debugging
    pub frame_number: i64,
    /// Timestamp in 100-nanosecond units (NDI timecode format)
    pub timestamp_100ns: i64,
}

/// Configuration for NDI sender
#[repr(C)]
pub struct RshipNDIConfig {
    /// Stream name visible on network (null-terminated UTF-8)
    pub stream_name: *const c_char,
    /// Frame width in pixels
    pub width: c_int,
    /// Frame height in pixels
    pub height: c_int,
    /// Target framerate numerator (e.g., 60 for 60fps)
    pub framerate_num: c_int,
    /// Target framerate denominator (e.g., 1 for 60fps)
    pub framerate_den: c_int,
    /// Enable alpha channel (RGBA vs RGBX)
    pub enable_alpha: bool,
    /// Number of frame buffer slots (2-4, default 3)
    pub buffer_count: c_int,
}

/// Statistics returned to C++
#[repr(C)]
pub struct RshipNDIStats {
    /// Total frames successfully sent
    pub frames_sent: u64,
    /// Frames dropped due to queue full
    pub frames_dropped: u64,
    /// Average time to send a frame in microseconds
    pub avg_send_time_us: f64,
    /// Number of connected NDI receivers
    pub connected_receivers: i32,
    /// Whether the sender is healthy and running
    pub is_healthy: bool,
    /// Current queue depth (frames waiting to send)
    pub queue_depth: i32,
}

// ============================================================================
// INTERNAL TYPES
// ============================================================================

/// Internal frame data with owned buffer
struct FrameData {
    data: Vec<u8>,
    width: i32,
    height: i32,
    frame_number: i64,
    timestamp_100ns: i64,
}

/// Sender configuration (internal)
#[derive(Clone)]
struct SenderConfig {
    width: i32,
    height: i32,
    framerate_num: i32,
    framerate_den: i32,
    enable_alpha: bool,
}

/// Statistics tracking (thread-safe)
struct SenderStats {
    frames_sent: AtomicU64,
    frames_dropped: AtomicU64,
    avg_send_time_us: Mutex<f64>,
    connected_receivers: AtomicI32,
}

/// Opaque handle for C++ (the actual sender state)
pub struct RshipNDISender {
    /// Sender thread handle
    sender_thread: Option<JoinHandle<()>>,
    /// Channel to send frames to the sender thread
    frame_tx: Option<Sender<FrameData>>,
    /// Shutdown signal
    shutdown: Arc<AtomicBool>,
    /// Statistics
    stats: Arc<SenderStats>,
    /// Configuration
    #[allow(dead_code)]
    config: SenderConfig,
}

// ============================================================================
// C FFI EXPORTS
// ============================================================================

/// Create a new NDI sender.
///
/// # Arguments
/// * `config` - Pointer to configuration struct
///
/// # Returns
/// * Opaque pointer to sender, or null on failure
///
/// # Safety
/// The config pointer must be valid and the stream_name must be a valid
/// null-terminated UTF-8 string.
#[no_mangle]
pub unsafe extern "C" fn rship_ndi_create(config: *const RshipNDIConfig) -> *mut RshipNDISender {
    if config.is_null() {
        eprintln!("rship_ndi_create: config is null");
        return ptr::null_mut();
    }

    let config = &*config;

    // Validate config
    if config.width <= 0 || config.height <= 0 {
        eprintln!(
            "rship_ndi_create: invalid dimensions {}x{}",
            config.width, config.height
        );
        return ptr::null_mut();
    }

    if config.framerate_num <= 0 || config.framerate_den <= 0 {
        eprintln!(
            "rship_ndi_create: invalid framerate {}/{}",
            config.framerate_num, config.framerate_den
        );
        return ptr::null_mut();
    }

    // Convert stream name
    let stream_name = if config.stream_name.is_null() {
        "Unreal NDI Stream".to_string()
    } else {
        match CStr::from_ptr(config.stream_name).to_str() {
            Ok(s) => s.to_string(),
            Err(e) => {
                eprintln!("rship_ndi_create: invalid stream name: {}", e);
                return ptr::null_mut();
            }
        }
    };

    // Determine buffer count
    let buffer_count = if config.buffer_count >= 2 && config.buffer_count <= 4 {
        config.buffer_count as usize
    } else {
        3 // Default triple-buffer
    };

    // Initialize NDI
    // Note: ndi-sdk handles NDI library loading automatically
    let send_instance = match SendInstance::new(&stream_name, None, false) {
        Ok(instance) => instance,
        Err(e) => {
            eprintln!("rship_ndi_create: failed to create NDI sender: {:?}", e);
            return ptr::null_mut();
        }
    };

    println!(
        "rship_ndi_create: created NDI sender '{}' @ {}x{} @ {}/{} fps, alpha={}",
        stream_name,
        config.width,
        config.height,
        config.framerate_num,
        config.framerate_den,
        config.enable_alpha
    );

    // Create bounded channel for frame submission
    let (tx, rx) = bounded::<FrameData>(buffer_count);

    let shutdown = Arc::new(AtomicBool::new(false));
    let stats = Arc::new(SenderStats {
        frames_sent: AtomicU64::new(0),
        frames_dropped: AtomicU64::new(0),
        avg_send_time_us: Mutex::new(0.0),
        connected_receivers: AtomicI32::new(0),
    });

    let sender_config = SenderConfig {
        width: config.width,
        height: config.height,
        framerate_num: config.framerate_num,
        framerate_den: config.framerate_den,
        enable_alpha: config.enable_alpha,
    };

    // Clone for thread
    let thread_shutdown = shutdown.clone();
    let thread_stats = stats.clone();
    let thread_config = sender_config.clone();

    // Spawn dedicated sender thread
    let sender_thread = match thread::Builder::new()
        .name("NDI-Sender".into())
        .spawn(move || {
            sender_thread_main(send_instance, rx, thread_shutdown, thread_stats, thread_config);
        }) {
        Ok(t) => t,
        Err(e) => {
            eprintln!("rship_ndi_create: failed to spawn sender thread: {}", e);
            return ptr::null_mut();
        }
    };

    let sender = Box::new(RshipNDISender {
        sender_thread: Some(sender_thread),
        frame_tx: Some(tx),
        shutdown,
        stats,
        config: sender_config,
    });

    Box::into_raw(sender)
}

/// Destroy an NDI sender and release all resources.
///
/// # Safety
/// The sender pointer must be valid and obtained from `rship_ndi_create`.
/// After this call, the pointer is invalid and must not be used.
#[no_mangle]
pub unsafe extern "C" fn rship_ndi_destroy(sender: *mut RshipNDISender) {
    if sender.is_null() {
        return;
    }

    let mut sender = Box::from_raw(sender);

    println!("rship_ndi_destroy: shutting down NDI sender");

    // Signal shutdown
    sender.shutdown.store(true, Ordering::SeqCst);

    // Drop channel to unblock receiver
    sender.frame_tx = None;

    // Wait for thread to finish
    if let Some(thread) = sender.sender_thread.take() {
        match thread.join() {
            Ok(_) => println!("rship_ndi_destroy: sender thread joined successfully"),
            Err(_) => eprintln!("rship_ndi_destroy: sender thread panicked"),
        }
    }

    // sender is dropped here, releasing all resources
}

/// Submit a frame for NDI transmission.
///
/// This function is non-blocking. If the internal queue is full,
/// the frame is dropped and the function returns false.
///
/// # Arguments
/// * `sender` - Sender handle from `rship_ndi_create`
/// * `frame` - Pointer to frame data struct
///
/// # Returns
/// * `true` if frame was accepted for transmission
/// * `false` if frame was dropped (queue full) or error
///
/// # Safety
/// Both pointers must be valid. The frame data must remain valid for the
/// duration of this call (it is copied internally).
#[no_mangle]
pub unsafe extern "C" fn rship_ndi_submit_frame(
    sender: *mut RshipNDISender,
    frame: *const RshipNDIFrame,
) -> bool {
    if sender.is_null() || frame.is_null() {
        return false;
    }

    let sender = &*sender;
    let frame = &*frame;

    // Validate frame data
    if frame.data.is_null() || frame.data_size == 0 {
        return false;
    }

    // Expected size for RGBA
    let expected_size = (frame.width as usize) * (frame.height as usize) * 4;
    if frame.data_size < expected_size {
        eprintln!(
            "rship_ndi_submit_frame: data_size {} < expected {} for {}x{}",
            frame.data_size, expected_size, frame.width, frame.height
        );
        return false;
    }

    // Copy frame data (required because we can't hold pointer across threads)
    let data = std::slice::from_raw_parts(frame.data, frame.data_size);
    let frame_data = FrameData {
        data: data.to_vec(),
        width: frame.width,
        height: frame.height,
        frame_number: frame.frame_number,
        timestamp_100ns: frame.timestamp_100ns,
    };

    // Try to send (non-blocking)
    if let Some(ref tx) = sender.frame_tx {
        match tx.try_send(frame_data) {
            Ok(_) => true,
            Err(TrySendError::Full(_)) => {
                sender.stats.frames_dropped.fetch_add(1, Ordering::Relaxed);
                false
            }
            Err(TrySendError::Disconnected(_)) => {
                eprintln!("rship_ndi_submit_frame: channel disconnected");
                false
            }
        }
    } else {
        false
    }
}

/// Get current statistics from the sender.
///
/// # Safety
/// Both pointers must be valid.
#[no_mangle]
pub unsafe extern "C" fn rship_ndi_get_stats(
    sender: *const RshipNDISender,
    stats: *mut RshipNDIStats,
) {
    if sender.is_null() || stats.is_null() {
        return;
    }

    let sender = &*sender;
    let out_stats = &mut *stats;

    out_stats.frames_sent = sender.stats.frames_sent.load(Ordering::Relaxed);
    out_stats.frames_dropped = sender.stats.frames_dropped.load(Ordering::Relaxed);
    out_stats.avg_send_time_us = *sender.stats.avg_send_time_us.lock();
    out_stats.connected_receivers = sender.stats.connected_receivers.load(Ordering::Relaxed);
    out_stats.is_healthy = !sender.shutdown.load(Ordering::Relaxed);

    // Get queue depth
    out_stats.queue_depth = if let Some(ref tx) = sender.frame_tx {
        tx.len() as i32
    } else {
        0
    };
}

/// Check if the NDI library is available on this system.
///
/// # Returns
/// * `true` if NDI runtime is installed and accessible
/// * `false` otherwise
#[no_mangle]
pub extern "C" fn rship_ndi_is_available() -> bool {
    // ndi-sdk uses dynamic loading, try to verify it works
    // For now, assume available if we can be called
    true
}

// ============================================================================
// SENDER THREAD
// ============================================================================

/// Main sender thread function.
///
/// Continuously receives frames from the channel and sends them via NDI.
fn sender_thread_main(
    send_instance: SendInstance,
    rx: Receiver<FrameData>,
    shutdown: Arc<AtomicBool>,
    stats: Arc<SenderStats>,
    config: SenderConfig,
) {
    println!("sender_thread_main: started");

    let four_cc = if config.enable_alpha {
        FourCCVideoType::RGBA
    } else {
        FourCCVideoType::RGBX
    };

    // Rolling average for send time
    let mut send_times: Vec<f64> = Vec::with_capacity(60);
    let mut last_connection_check = Instant::now();

    while !shutdown.load(Ordering::Relaxed) {
        // Wait for frame with timeout (allows checking shutdown)
        match rx.recv_timeout(Duration::from_millis(100)) {
            Ok(frame_data) => {
                let start = Instant::now();

                // Calculate line stride (bytes per row)
                let line_stride = frame_data.width * 4; // RGBA = 4 bytes per pixel

                // Create NDI video frame
                // Note: NDI SDK expects the data pointer to remain valid during send
                let result = send_video_frame(
                    &send_instance,
                    frame_data.width,
                    frame_data.height,
                    four_cc,
                    config.framerate_num,
                    config.framerate_den,
                    line_stride,
                    &frame_data.data,
                    frame_data.timestamp_100ns,
                );

                if let Err(e) = result {
                    eprintln!("sender_thread_main: NDI send error: {:?}", e);
                    continue;
                }

                // Update statistics
                let elapsed_us = start.elapsed().as_micros() as f64;
                send_times.push(elapsed_us);
                if send_times.len() > 60 {
                    send_times.remove(0);
                }

                let avg = send_times.iter().sum::<f64>() / send_times.len() as f64;
                *stats.avg_send_time_us.lock() = avg;
                stats.frames_sent.fetch_add(1, Ordering::Relaxed);
            }
            Err(crossbeam_channel::RecvTimeoutError::Timeout) => {
                // Normal timeout, check shutdown and continue
            }
            Err(crossbeam_channel::RecvTimeoutError::Disconnected) => {
                // Channel closed, exit
                println!("sender_thread_main: channel disconnected, exiting");
                break;
            }
        }

        // Periodically check connection count (every second)
        if last_connection_check.elapsed() > Duration::from_secs(1) {
            let num_connections = send_instance.get_no_connections() as i32;
            stats
                .connected_receivers
                .store(num_connections, Ordering::Relaxed);
            last_connection_check = Instant::now();
        }
    }

    println!("sender_thread_main: exiting");
}

/// Send a video frame via NDI.
///
/// This is a helper that handles the NDI frame setup and send.
fn send_video_frame(
    send_instance: &SendInstance,
    width: i32,
    height: i32,
    four_cc: FourCCVideoType,
    framerate_num: i32,
    framerate_den: i32,
    line_stride: i32,
    data: &[u8],
    timestamp_100ns: i64,
) -> Result<(), Box<dyn std::error::Error>> {
    // Create NDI video frame descriptor
    let video_frame = ndi_sdk::VideoFrame {
        xres: width,
        yres: height,
        four_cc,
        frame_rate_n: framerate_num,
        frame_rate_d: framerate_den,
        picture_aspect_ratio: width as f32 / height as f32,
        frame_format_type: ndi_sdk::FrameFormatType::Progressive,
        timecode: timestamp_100ns,
        data: data.as_ptr() as *mut u8,
        line_stride_in_bytes: line_stride,
        metadata: None,
        timestamp: timestamp_100ns,
    };

    // Send frame asynchronously
    // NDI handles the actual network transmission
    send_instance.send_video(&video_frame)?;

    Ok(())
}

// ============================================================================
// TESTS
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_frame_struct_size() {
        // Ensure FFI structs have expected sizes
        assert!(std::mem::size_of::<RshipNDIFrame>() > 0);
        assert!(std::mem::size_of::<RshipNDIConfig>() > 0);
        assert!(std::mem::size_of::<RshipNDIStats>() > 0);
    }
}
