//! High-performance NDI sender with C FFI for Unreal Engine integration.
//!
//! This library provides a thread-safe NDI video sender that accepts raw RGBA frames
//! from Unreal Engine's GPU readback and transmits them via NDI.
//!
//! Uses runtime dynamic loading of NDI library - no compile-time NDI SDK dependency.
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
//! NDI Runtime (dynamically loaded)
//! ```

use crossbeam_channel::{bounded, Receiver, Sender, TrySendError};
use libloading::{Library, Symbol};
use once_cell::sync::OnceCell;
use parking_lot::Mutex;
use std::ffi::{c_char, c_int, c_void, CStr, CString};
use std::ptr;
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicU64, Ordering};
use std::sync::Arc;
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

// ============================================================================
// NDI SDK TYPES (from Processing.NDI.Lib.h)
// These match the official NDI SDK structures
// ============================================================================

/// NDI video frame format types
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum NDILibFourCCVideoType {
    UYVY = 0x59565955,  // YCbCr color space, 4:2:2
    UYVA = 0x41565955,  // YCbCr + Alpha, 4:2:2:4
    P216 = 0x36313250,  // YCbCr, 4:2:2, 16bpp
    PA16 = 0x36314150,  // YCbCr + Alpha, 4:2:2:4, 16bpp
    YV12 = 0x32315659,  // YCbCr, 4:2:0 (planar)
    I420 = 0x30323449,  // YCbCr, 4:2:0 (planar)
    NV12 = 0x3231564E,  // YCbCr, 4:2:0 (semi-planar)
    BGRA = 0x41524742,  // BGRA, 8bpp per component
    BGRX = 0x58524742,  // BGRX, 8bpp per component (no alpha)
    RGBA = 0x41424752,  // RGBA, 8bpp per component
    RGBX = 0x58424752,  // RGBX, 8bpp per component (no alpha)
}

/// NDI frame format type (progressive/interlaced)
#[repr(i32)]
#[derive(Clone, Copy, Debug)]
pub enum NDILibFrameFormatType {
    Progressive = 1,
    Interleaved = 0,
    Field0 = 2,
    Field1 = 3,
}

/// NDI video frame descriptor (matches NDIlib_video_frame_v2_t)
#[repr(C)]
pub struct NDILibVideoFrameV2 {
    pub xres: c_int,
    pub yres: c_int,
    pub four_cc: NDILibFourCCVideoType,
    pub frame_rate_n: c_int,
    pub frame_rate_d: c_int,
    pub picture_aspect_ratio: f32,
    pub frame_format_type: NDILibFrameFormatType,
    pub timecode: i64,
    pub p_data: *mut u8,
    pub line_stride_in_bytes: c_int,
    pub p_metadata: *const c_char,
    pub timestamp: i64,
}

/// NDI send create descriptor (matches NDIlib_send_create_t)
#[repr(C)]
pub struct NDILibSendCreate {
    pub p_ndi_name: *const c_char,
    pub p_groups: *const c_char,
    pub clock_video: bool,
    pub clock_audio: bool,
}

// ============================================================================
// NDI FUNCTION SIGNATURES
// ============================================================================

type NDILibInitializeFn = unsafe extern "C" fn() -> bool;
type NDILibDestroyFn = unsafe extern "C" fn();
type NDILibSendCreateFn = unsafe extern "C" fn(*const NDILibSendCreate) -> *mut c_void;
type NDILibSendDestroyFn = unsafe extern "C" fn(*mut c_void);
type NDILibSendSendVideoV2Fn = unsafe extern "C" fn(*mut c_void, *const NDILibVideoFrameV2);
type NDILibSendGetNoConnectionsFn = unsafe extern "C" fn(*mut c_void, u32) -> c_int;

// ============================================================================
// NDI LIBRARY WRAPPER
// ============================================================================

struct NDILibrary {
    #[allow(dead_code)]
    lib: Library,
    initialize: NDILibInitializeFn,
    destroy: NDILibDestroyFn,
    send_create: NDILibSendCreateFn,
    send_destroy: NDILibSendDestroyFn,
    send_video_v2: NDILibSendSendVideoV2Fn,
    send_get_no_connections: NDILibSendGetNoConnectionsFn,
}

impl NDILibrary {
    fn load() -> Result<Self, String> {
        // Platform-specific library names
        // Platform-specific library names to search
        // Order: bundled with plugin -> system install -> standard paths
        #[cfg(target_os = "windows")]
        let lib_names = [
            // Standard NDI Tools install location (most common - added to PATH by installer)
            "Processing.NDI.Lib.x64.dll",
            // NDI 6 SDK/Runtime locations
            "C:\\Program Files\\NDI\\NDI 6 SDK\\Bin\\x64\\Processing.NDI.Lib.x64.dll",
            "C:\\Program Files\\NDI\\NDI 6 Runtime\\v6\\Processing.NDI.Lib.x64.dll",
            // NDI 5 SDK/Runtime locations
            "C:\\Program Files\\NDI\\NDI 5 SDK\\Bin\\x64\\Processing.NDI.Lib.x64.dll",
            "C:\\Program Files\\NDI\\NDI 5 Runtime\\v5\\Processing.NDI.Lib.x64.dll",
            // Legacy NewTek paths
            "C:\\Program Files\\NewTek\\NDI SDK\\Bin\\x64\\Processing.NDI.Lib.x64.dll",
            "C:\\Program Files\\NewTek\\NDI 5 SDK\\Bin\\x64\\Processing.NDI.Lib.x64.dll",
        ];

        #[cfg(target_os = "macos")]
        let lib_names = [
            // NDI SDK for macOS install location
            "/Library/NDI SDK for macOS/lib/macOS/libndi.dylib",
            // Homebrew or manual install
            "/usr/local/lib/libndi.dylib",
            "/opt/homebrew/lib/libndi.dylib",
            // Dynamic linker search
            "libndi.dylib",
        ];

        #[cfg(target_os = "linux")]
        let lib_names = [
            // NDI 6 and 5 shared object
            "libndi.so.6",
            "libndi.so.5",
            "libndi.so",
            // Standard library paths
            "/usr/lib/libndi.so",
            "/usr/local/lib/libndi.so",
            "/usr/lib/x86_64-linux-gnu/libndi.so",
        ];

        let mut last_error = String::new();

        for lib_name in lib_names {
            match unsafe { Library::new(lib_name) } {
                Ok(lib) => {
                    // Load all required functions - get raw pointers first to avoid borrow issues
                    unsafe {
                        let initialize_sym: Symbol<NDILibInitializeFn> =
                            match lib.get(b"NDIlib_initialize\0") {
                                Ok(s) => s,
                                Err(e) => {
                                    last_error = format!("Failed to load NDIlib_initialize: {}", e);
                                    continue;
                                }
                            };
                        let initialize = *initialize_sym;

                        let destroy_sym: Symbol<NDILibDestroyFn> =
                            match lib.get(b"NDIlib_destroy\0") {
                                Ok(s) => s,
                                Err(e) => {
                                    last_error = format!("Failed to load NDIlib_destroy: {}", e);
                                    continue;
                                }
                            };
                        let destroy = *destroy_sym;

                        let send_create_sym: Symbol<NDILibSendCreateFn> =
                            match lib.get(b"NDIlib_send_create\0") {
                                Ok(s) => s,
                                Err(e) => {
                                    last_error = format!("Failed to load NDIlib_send_create: {}", e);
                                    continue;
                                }
                            };
                        let send_create = *send_create_sym;

                        let send_destroy_sym: Symbol<NDILibSendDestroyFn> =
                            match lib.get(b"NDIlib_send_destroy\0") {
                                Ok(s) => s,
                                Err(e) => {
                                    last_error = format!("Failed to load NDIlib_send_destroy: {}", e);
                                    continue;
                                }
                            };
                        let send_destroy = *send_destroy_sym;

                        let send_video_v2_sym: Symbol<NDILibSendSendVideoV2Fn> =
                            match lib.get(b"NDIlib_send_send_video_v2\0") {
                                Ok(s) => s,
                                Err(e) => {
                                    last_error = format!("Failed to load NDIlib_send_send_video_v2: {}", e);
                                    continue;
                                }
                            };
                        let send_video_v2 = *send_video_v2_sym;

                        let send_get_no_connections_sym: Symbol<NDILibSendGetNoConnectionsFn> =
                            match lib.get(b"NDIlib_send_get_no_connections\0") {
                                Ok(s) => s,
                                Err(e) => {
                                    last_error = format!("Failed to load NDIlib_send_get_no_connections: {}", e);
                                    continue;
                                }
                            };
                        let send_get_no_connections = *send_get_no_connections_sym;

                        return Ok(NDILibrary {
                            lib,
                            initialize,
                            destroy,
                            send_create,
                            send_destroy,
                            send_video_v2,
                            send_get_no_connections,
                        });
                    }
                }
                Err(e) => {
                    last_error = format!("{}: {}", lib_name, e);
                }
            }
        }

        Err(format!("Failed to load NDI library. Last error: {}", last_error))
    }
}

// Global NDI library instance
static NDI_LIBRARY: OnceCell<Result<NDILibrary, String>> = OnceCell::new();

fn get_ndi_library() -> Result<&'static NDILibrary, &'static str> {
    NDI_LIBRARY
        .get_or_init(|| NDILibrary::load())
        .as_ref()
        .map_err(|e| e.as_str())
}

// ============================================================================
// FFI TYPES (exported to C++)
// ============================================================================

/// Frame data passed from C++ (Unreal Engine)
#[repr(C)]
pub struct RshipNDIFrame {
    /// Pointer to RGBA pixel data (must remain valid during call)
    pub data: *const u8,
    /// Size of data in bytes (actual buffer size, may include row padding)
    pub data_size: usize,
    /// Frame width in pixels
    pub width: c_int,
    /// Frame height in pixels
    pub height: c_int,
    /// Line stride in bytes (bytes per row, may be > width*4 due to GPU alignment)
    pub line_stride_bytes: c_int,
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
    line_stride_bytes: i32,
    #[allow(dead_code)]
    frame_number: i64,
    timestamp_100ns: i64,
}

/// Sender configuration (internal)
#[derive(Clone)]
struct SenderConfig {
    stream_name: String,
    #[allow(dead_code)]
    width: i32,
    #[allow(dead_code)]
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

    // Check NDI library is available
    if let Err(e) = get_ndi_library() {
        eprintln!("rship_ndi_create: NDI library not available: {}", e);
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

    let sender_config = SenderConfig {
        stream_name: stream_name.clone(),
        width: config.width,
        height: config.height,
        framerate_num: config.framerate_num,
        framerate_den: config.framerate_den,
        enable_alpha: config.enable_alpha,
    };

    println!(
        "rship_ndi_create: creating NDI sender '{}' @ {}x{} @ {}/{} fps, alpha={}",
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

    // Clone for thread
    let thread_shutdown = shutdown.clone();
    let thread_stats = stats.clone();
    let thread_config = sender_config.clone();

    // Spawn dedicated sender thread
    let sender_thread = match thread::Builder::new()
        .name("NDI-Sender".into())
        .spawn(move || {
            sender_thread_main(rx, thread_shutdown, thread_stats, thread_config);
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

    // Determine line stride (use provided value, or calculate tight packing)
    let line_stride = if frame.line_stride_bytes > 0 {
        frame.line_stride_bytes as usize
    } else {
        (frame.width as usize) * 4  // Default to tight packing
    };

    // Expected size based on actual stride
    let expected_size = line_stride * (frame.height as usize);
    if frame.data_size < expected_size {
        eprintln!(
            "rship_ndi_submit_frame: data_size {} < expected {} for {}x{} (stride {})",
            frame.data_size, expected_size, frame.width, frame.height, line_stride
        );
        return false;
    }

    // Copy frame data (required because we can't hold pointer across threads)
    let data = std::slice::from_raw_parts(frame.data, frame.data_size);
    let frame_data = FrameData {
        data: data.to_vec(),
        width: frame.width,
        height: frame.height,
        line_stride_bytes: line_stride as i32,
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
    get_ndi_library().is_ok()
}

// ============================================================================
// SENDER THREAD
// ============================================================================

/// Main sender thread function.
///
/// Continuously receives frames from the channel and sends them via NDI.
fn sender_thread_main(
    rx: Receiver<FrameData>,
    shutdown: Arc<AtomicBool>,
    stats: Arc<SenderStats>,
    config: SenderConfig,
) {
    println!("sender_thread_main: starting NDI initialization");

    // Get NDI library
    let ndi = match get_ndi_library() {
        Ok(lib) => lib,
        Err(e) => {
            eprintln!("sender_thread_main: failed to get NDI library: {}", e);
            return;
        }
    };

    // Initialize NDI
    let initialized = unsafe { (ndi.initialize)() };
    if !initialized {
        eprintln!("sender_thread_main: NDIlib_initialize failed");
        return;
    }

    // Create NDI sender
    let stream_name_c = match CString::new(config.stream_name.as_str()) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("sender_thread_main: invalid stream name: {}", e);
            unsafe { (ndi.destroy)() };
            return;
        }
    };

    let create_desc = NDILibSendCreate {
        p_ndi_name: stream_name_c.as_ptr(),
        p_groups: ptr::null(),
        clock_video: true,
        clock_audio: false,
    };

    let ndi_sender = unsafe { (ndi.send_create)(&create_desc) };
    if ndi_sender.is_null() {
        eprintln!("sender_thread_main: NDIlib_send_create failed");
        unsafe { (ndi.destroy)() };
        return;
    }

    println!(
        "sender_thread_main: NDI sender '{}' created successfully",
        config.stream_name
    );

    let four_cc = if config.enable_alpha {
        NDILibFourCCVideoType::RGBA
    } else {
        NDILibFourCCVideoType::RGBX
    };

    // Rolling average for send time
    let mut send_times: Vec<f64> = Vec::with_capacity(60);
    let mut last_connection_check = Instant::now();

    while !shutdown.load(Ordering::Relaxed) {
        // Wait for frame with timeout (allows checking shutdown)
        match rx.recv_timeout(Duration::from_millis(100)) {
            Ok(frame_data) => {
                let start = Instant::now();

                // Use actual line stride from frame (handles GPU memory alignment)
                let line_stride = frame_data.line_stride_bytes;

                // Create NDI video frame descriptor
                let video_frame = NDILibVideoFrameV2 {
                    xres: frame_data.width,
                    yres: frame_data.height,
                    four_cc,
                    frame_rate_n: config.framerate_num,
                    frame_rate_d: config.framerate_den,
                    picture_aspect_ratio: frame_data.width as f32 / frame_data.height as f32,
                    frame_format_type: NDILibFrameFormatType::Progressive,
                    timecode: frame_data.timestamp_100ns,
                    p_data: frame_data.data.as_ptr() as *mut u8,
                    line_stride_in_bytes: line_stride,
                    p_metadata: ptr::null(),
                    timestamp: frame_data.timestamp_100ns,
                };

                // Send frame
                unsafe { (ndi.send_video_v2)(ndi_sender, &video_frame) };

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
            let num_connections = unsafe { (ndi.send_get_no_connections)(ndi_sender, 0) };
            stats
                .connected_receivers
                .store(num_connections, Ordering::Relaxed);
            last_connection_check = Instant::now();
        }
    }

    println!("sender_thread_main: cleaning up");

    // Cleanup
    unsafe {
        (ndi.send_destroy)(ndi_sender);
        (ndi.destroy)();
    }

    println!("sender_thread_main: exiting");
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

    #[test]
    fn test_four_cc_values() {
        // Verify FourCC values match NDI SDK
        assert_eq!(NDILibFourCCVideoType::RGBA as i32, 0x41424752);
        assert_eq!(NDILibFourCCVideoType::RGBX as i32, 0x58424752);
        assert_eq!(NDILibFourCCVideoType::BGRA as i32, 0x41524742);
    }
}
