use rship_display_core::{ApplyResult, DisplayPlan, DisplaySnapshot};

pub fn collect_snapshot() -> Result<DisplaySnapshot, String> {
    platform::collect_snapshot_impl()
}

pub fn apply_plan(plan: &DisplayPlan, dry_run: bool) -> Result<ApplyResult, String> {
    platform::apply_plan_impl(plan, dry_run)
}

#[cfg(not(windows))]
mod platform {
    use rship_display_core::{apply_plan_simulated, ApplyResult, DisplayPlan, DisplaySnapshot};

    pub fn collect_snapshot_impl() -> Result<DisplaySnapshot, String> {
        Err("rship-display-windows: snapshot is only supported on Windows targets".to_string())
    }

    pub fn apply_plan_impl(plan: &DisplayPlan, dry_run: bool) -> Result<ApplyResult, String> {
        let mut result = apply_plan_simulated(plan, dry_run);
        if !dry_run {
            result.success = false;
            result.errors.push(
                "rship-display-windows: apply is only supported on Windows targets".to_string(),
            );
        }
        Ok(result)
    }
}

#[cfg(windows)]
mod platform {
    use std::collections::HashMap;
    use std::mem::size_of;

    use rship_display_core::{
        apply_plan_simulated, now_utc, ApplyResult, ConnectorType, DisplayDescriptor, DisplayPath,
        DisplayPlan, DisplayPlanStepKind, DisplayRotation, DisplaySnapshot, MosaicBackend,
        MosaicGroup, RectI32,
    };
    use rship_display_vendor_nv::apply_mosaic_group;
    use serde_json::Value;
    use windows::core::PCWSTR;
    use windows::Win32::Foundation::{HWND, POINTL};
    use windows::Win32::Graphics::Gdi::{
        ChangeDisplaySettingsExW, EnumDisplayDevicesW, EnumDisplaySettingsExW, CDS_NORESET,
        CDS_UPDATEREGISTRY, DEVMODEW, DISPLAY_DEVICEW, DISPLAY_DEVICE_ATTACHED_TO_DESKTOP,
        DISPLAY_DEVICE_MIRRORING_DRIVER, DISP_CHANGE_SUCCESSFUL, DM_PELSHEIGHT, DM_PELSWIDTH,
        DM_POSITION, ENUM_CURRENT_SETTINGS, ENUM_DISPLAY_SETTINGS_FLAGS,
    };
    use windows::Win32::UI::WindowsAndMessaging::EDD_GET_DEVICE_INTERFACE_NAME;

    pub fn collect_snapshot_impl() -> Result<DisplaySnapshot, String> {
        let mut snapshot = DisplaySnapshot::default();
        snapshot.timestamp_utc = now_utc();
        snapshot.machine_id = std::env::var("COMPUTERNAME").ok();

        let mut adapter_index: u32 = 0;

        loop {
            let mut adapter = DISPLAY_DEVICEW::default();
            adapter.cb = size_of::<DISPLAY_DEVICEW>() as u32;

            let found = unsafe {
                EnumDisplayDevicesW(
                    PCWSTR::null(),
                    adapter_index,
                    &mut adapter,
                    EDD_GET_DEVICE_INTERFACE_NAME,
                )
            }
            .as_bool();

            if !found {
                break;
            }

            adapter_index += 1;

            if (adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0 {
                continue;
            }
            if (adapter.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) != 0 {
                continue;
            }

            let display_name = wide_to_string(&adapter.DeviceName);
            let friendly_name = wide_to_string(&adapter.DeviceString);
            let normalized_display_name = normalize_device_name(&display_name);

            let mut monitor_device_path = None;
            let mut pnp_id = None;
            let mut monitor_name = None;

            let mut monitor = DISPLAY_DEVICEW::default();
            monitor.cb = size_of::<DISPLAY_DEVICEW>() as u32;
            let has_monitor = unsafe {
                EnumDisplayDevicesW(
                    PCWSTR(adapter.DeviceName.as_ptr()),
                    0,
                    &mut monitor,
                    EDD_GET_DEVICE_INTERFACE_NAME,
                )
            }
            .as_bool();

            if has_monitor {
                let monitor_id = wide_to_string(&monitor.DeviceID);
                if !monitor_id.is_empty() {
                    pnp_id = Some(monitor_id.clone());
                    monitor_device_path = Some(monitor_id);
                }
                let monitor_friendly = wide_to_string(&monitor.DeviceString);
                if !monitor_friendly.is_empty() {
                    monitor_name = Some(monitor_friendly);
                }
            }

            let mode = query_current_mode(&normalized_display_name).ok();
            let current_rect = mode.as_ref().map(|m| RectI32 {
                x: m.x,
                y: m.y,
                w: m.width as i32,
                h: m.height as i32,
            });

            snapshot.displays.push(DisplayDescriptor {
                os_display_name: normalized_display_name.clone(),
                adapter_id: Some(adapter_index as u64),
                target_id: None,
                monitor_device_path,
                pnp_id,
                friendly_name: monitor_name.or_else(|| {
                    if friendly_name.is_empty() {
                        None
                    } else {
                        Some(friendly_name)
                    }
                }),
                edid_vendor: None,
                edid_product_code: None,
                edid_serial: None,
                edid_hash: None,
                connector: ConnectorType::Unknown,
                native_width: mode.as_ref().map(|m| m.width),
                native_height: mode.as_ref().map(|m| m.height),
                native_refresh_hz: mode.as_ref().map(|m| m.refresh_hz),
                current_rect_px: current_rect.clone(),
                current_rotation: DisplayRotation::Deg0,
                hdr_enabled: None,
                bits_per_color: None,
                is_active: true,
            });

            snapshot.paths.push(DisplayPath {
                source_display_name: Some(normalized_display_name.clone()),
                target_display_name: Some(normalized_display_name),
                source_rect_px: current_rect.clone(),
                target_rect_px: current_rect,
                active: true,
            });
        }

        Ok(snapshot)
    }

    pub fn apply_plan_impl(plan: &DisplayPlan, dry_run: bool) -> Result<ApplyResult, String> {
        if dry_run {
            return Ok(apply_plan_simulated(plan, true));
        }

        let mut result = ApplyResult {
            success: true,
            dry_run: false,
            warnings: plan.warnings.clone(),
            ..Default::default()
        };

        let identity_map = extract_identity_map(plan);
        let mut topology_targets: Vec<(String, RectI32, bool)> = Vec::new();
        let mut topology_step_ids = Vec::new();
        let mut mosaic_targets: Vec<(MosaicGroup, bool, String)> = Vec::new();
        let mut verify_steps: Vec<(String, bool)> = Vec::new();

        for step in &plan.steps {
            match step.kind {
                DisplayPlanStepKind::ResolveIdentity => {
                    result.applied_steps.push(step.step_id.clone());
                }
                DisplayPlanStepKind::SetTopology => {
                    topology_step_ids.push(step.step_id.clone());
                    if let Some(entries) = extract_expected_rects(&step.payload) {
                        topology_targets.extend(
                            entries
                                .into_iter()
                                .map(|(canonical_id, rect)| (canonical_id, rect, step.required)),
                        );
                    } else if step.required {
                        result.success = false;
                        result.errors.push(format!(
                            "Unable to parse set-topology payload for step {}",
                            step.step_id
                        ));
                    } else {
                        result.warnings.push(format!(
                            "Skipping malformed optional set-topology payload in step {}",
                            step.step_id
                        ));
                    }
                }
                DisplayPlanStepKind::EnableMosaic => {
                    match serde_json::from_value::<MosaicGroup>(step.payload.clone()) {
                        Ok(group) => {
                            mosaic_targets.push((group, step.required, step.step_id.clone()))
                        }
                        Err(err) if step.required => {
                            result.success = false;
                            result.errors.push(format!(
                                "Unable to parse enable-mosaic payload for step {}: {}",
                                step.step_id, err
                            ));
                        }
                        Err(err) => {
                            result.warnings.push(format!(
                                "Skipping malformed optional enable-mosaic payload in step {}: {}",
                                step.step_id, err
                            ));
                        }
                    }
                }
                DisplayPlanStepKind::ApplyPixelRoute => {
                    result.warnings.push(format!(
                        "Step {} is a pixel-route contract; no direct Win32 topology mutation is required",
                        step.step_id
                    ));
                    result.applied_steps.push(step.step_id.clone());
                }
                DisplayPlanStepKind::Verify => {
                    verify_steps.push((step.step_id.clone(), step.required));
                }
                DisplayPlanStepKind::SetMode | DisplayPlanStepKind::DisableMosaic => {
                    result.warnings.push(format!(
                        "Step {} ({:?}) is not implemented in Windows adapter yet",
                        step.step_id, step.kind
                    ));
                    if step.required {
                        result.success = false;
                        result.errors.push(format!(
                            "Required step {} ({:?}) is not implemented",
                            step.step_id, step.kind
                        ));
                    }
                }
            }
        }

        if !result.errors.is_empty() {
            return Ok(result);
        }

        for (group, required, step_id) in &mosaic_targets {
            let apply_result = match group.backend {
                MosaicBackend::Nvidia => apply_mosaic_group(group),
                MosaicBackend::None | MosaicBackend::Software => Ok(()),
                _ => Err(format!(
                    "Mosaic backend {:?} is not implemented yet for group {}",
                    group.backend, group.id
                )),
            };

            if let Err(err) = apply_result {
                if *required {
                    result.success = false;
                    result.errors.push(err);
                    return Ok(result);
                }
                result.warnings.push(err);
            } else {
                result.applied_steps.push(step_id.clone());
            }
        }

        if !topology_targets.is_empty() {
            let apply_outcome = apply_topology(&topology_targets, &identity_map, &mut result);
            if let Err(err) = apply_outcome {
                result.success = false;
                result.errors.push(err);
                return Ok(result);
            }
            result.applied_steps.extend(topology_step_ids);
        } else {
            result.warnings.push(
                "Plan contains no set-topology targets; no topology mutation executed".to_string(),
            );
        }

        if !verify_steps.is_empty() {
            match collect_snapshot_impl() {
                Ok(post_snapshot) => {
                    let verify_ok = verify_topology_targets(
                        &topology_targets,
                        &identity_map,
                        &post_snapshot,
                        &mut result,
                    );
                    result.post_snapshot = Some(post_snapshot);

                    if verify_ok {
                        for (step_id, _) in verify_steps {
                            result.applied_steps.push(step_id);
                        }
                    } else if verify_steps.iter().any(|(_, required)| *required) {
                        result.success = false;
                    }
                }
                Err(err) => {
                    if verify_steps.iter().any(|(_, required)| *required) {
                        result.success = false;
                        result.errors.push(format!(
                            "Required verify step failed to collect post snapshot: {}",
                            err
                        ));
                    } else {
                        result.warnings.push(format!(
                            "Verify step skipped: unable to collect post snapshot: {}",
                            err
                        ));
                    }
                }
            }
        }

        Ok(result)
    }

    fn apply_topology(
        targets: &[(String, RectI32, bool)],
        identity_map: &HashMap<String, String>,
        result: &mut ApplyResult,
    ) -> Result<(), String> {
        let mut originals: Vec<(String, DEVMODEW)> = Vec::new();

        for (canonical_id, rect, required) in targets {
            let device_name = resolve_device_name(canonical_id, identity_map);
            let normalized = normalize_device_name(&device_name);
            let original_mode = match query_devmode(&normalized) {
                Ok(mode) => mode,
                Err(err) => {
                    if *required {
                        return Err(err);
                    }
                    result.warnings.push(format!(
                        "Optional topology target {} skipped: {}",
                        canonical_id, err
                    ));
                    continue;
                }
            };

            if original_mode.dmSize == 0 {
                if *required {
                    return Err(format!("Display {} returned invalid DEVMODE", normalized));
                }
                result.warnings.push(format!(
                    "Optional topology target {} returned invalid DEVMODE",
                    canonical_id
                ));
                continue;
            }

            let mut requested_mode = original_mode.clone();
            if let Err(err) = set_position_and_mode(&mut requested_mode, rect) {
                if *required {
                    return Err(err);
                }
                result.warnings.push(format!(
                    "Optional topology target {} skipped: {}",
                    canonical_id, err
                ));
                continue;
            }

            if let Err(err) = stage_mode(&normalized, &requested_mode) {
                if *required {
                    return Err(with_optional_rollback(
                        format!(
                            "Failed to stage required target '{}' ({})",
                            canonical_id, err
                        ),
                        rollback_modes(&originals),
                    ));
                }
                result.warnings.push(format!(
                    "Optional topology target {} skipped during stage: {}",
                    canonical_id, err
                ));
                continue;
            }

            originals.push((normalized.clone(), original_mode));
        }

        if originals.is_empty() {
            result
                .warnings
                .push("No topology targets were staged; commit skipped".to_string());
            return Ok(());
        }

        if let Err(err) = commit_modes() {
            return Err(with_optional_rollback(
                format!("Failed to commit staged display modes: {}", err),
                rollback_modes(&originals),
            ));
        }

        Ok(())
    }

    fn query_current_mode(device_name: &str) -> Result<ModeInfo, String> {
        let mode = query_devmode(device_name)?;
        let pos = unsafe { mode.Anonymous1.Anonymous2.dmPosition };
        let refresh_hz = if mode.dmDisplayFrequency > 0 {
            mode.dmDisplayFrequency as f32
        } else {
            0.0
        };
        Ok(ModeInfo {
            x: pos.x,
            y: pos.y,
            width: mode.dmPelsWidth,
            height: mode.dmPelsHeight,
            refresh_hz,
        })
    }

    fn query_devmode(device_name: &str) -> Result<DEVMODEW, String> {
        let mut mode = DEVMODEW::default();
        mode.dmSize = size_of::<DEVMODEW>() as u16;

        let device_wide = to_wide(device_name);
        let ok = unsafe {
            EnumDisplaySettingsExW(
                PCWSTR(device_wide.as_ptr()),
                ENUM_CURRENT_SETTINGS,
                &mut mode,
                ENUM_DISPLAY_SETTINGS_FLAGS(0),
            )
        }
        .as_bool();

        if !ok {
            return Err(format!("EnumDisplaySettingsExW failed for {}", device_name));
        }

        Ok(mode)
    }

    fn set_position_and_mode(mode: &mut DEVMODEW, rect: &RectI32) -> Result<(), String> {
        if rect.w <= 0 || rect.h <= 0 {
            return Err(format!(
                "Invalid rect {}x{} for display position update",
                rect.w, rect.h
            ));
        }

        mode.dmPelsWidth = rect.w as u32;
        mode.dmPelsHeight = rect.h as u32;
        mode.dmFields = mode.dmFields | DM_POSITION | DM_PELSWIDTH | DM_PELSHEIGHT;

        unsafe {
            let mut display_settings = mode.Anonymous1.Anonymous2;
            display_settings.dmPosition = POINTL {
                x: rect.x,
                y: rect.y,
            };
            mode.Anonymous1.Anonymous2 = display_settings;
        }

        Ok(())
    }

    fn stage_mode(device_name: &str, mode: &DEVMODEW) -> Result<(), String> {
        let device_wide = to_wide(device_name);
        let disp = unsafe {
            ChangeDisplaySettingsExW(
                PCWSTR(device_wide.as_ptr()),
                Some(mode as *const DEVMODEW),
                HWND(std::ptr::null_mut()),
                CDS_UPDATEREGISTRY | CDS_NORESET,
                None,
            )
        };
        if disp != DISP_CHANGE_SUCCESSFUL {
            return Err(format!(
                "ChangeDisplaySettingsExW stage failed for {} (code {})",
                device_name, disp.0
            ));
        }
        Ok(())
    }

    fn commit_modes() -> Result<(), String> {
        let disp = unsafe {
            ChangeDisplaySettingsExW(
                PCWSTR::null(),
                None,
                HWND(std::ptr::null_mut()),
                CDS_UPDATEREGISTRY,
                None,
            )
        };
        if disp != DISP_CHANGE_SUCCESSFUL {
            return Err(format!(
                "ChangeDisplaySettingsExW commit failed (code {})",
                disp.0
            ));
        }
        Ok(())
    }

    fn rollback_modes(originals: &[(String, DEVMODEW)]) -> Result<(), String> {
        if originals.is_empty() {
            return Ok(());
        }

        let mut errors = Vec::new();
        for (device_name, mode) in originals {
            if let Err(err) = stage_mode(device_name, mode) {
                errors.push(format!(
                    "rollback stage failed for {}: {}",
                    device_name, err
                ));
            }
        }

        if errors.is_empty() {
            if let Err(err) = commit_modes() {
                errors.push(format!("rollback commit failed: {}", err));
            }
        }

        if errors.is_empty() {
            Ok(())
        } else {
            Err(errors.join("; "))
        }
    }

    fn with_optional_rollback(primary_error: String, rollback: Result<(), String>) -> String {
        match rollback {
            Ok(_) => primary_error,
            Err(rollback_err) => format!("{}; rollback failed: {}", primary_error, rollback_err),
        }
    }

    fn verify_topology_targets(
        targets: &[(String, RectI32, bool)],
        identity_map: &HashMap<String, String>,
        snapshot: &DisplaySnapshot,
        result: &mut ApplyResult,
    ) -> bool {
        let mut ok = true;
        for (canonical_id, expected_rect, required) in targets {
            let observed_name = resolve_device_name(canonical_id, identity_map);
            let normalized_name = normalize_device_name(&observed_name);
            let matched = snapshot
                .displays
                .iter()
                .find(|display| normalize_device_name(&display.os_display_name) == normalized_name);

            let Some(display) = matched else {
                let message = format!(
                    "Post-apply verify missing display '{}' resolved as '{}'",
                    canonical_id, normalized_name
                );
                if *required {
                    result.errors.push(message);
                    ok = false;
                } else {
                    result.warnings.push(message);
                }
                continue;
            };

            let Some(observed_rect) = display.current_rect_px.as_ref() else {
                let message = format!(
                    "Post-apply verify missing rect for display '{}' ({})",
                    canonical_id, display.os_display_name
                );
                if *required {
                    result.errors.push(message);
                    ok = false;
                } else {
                    result.warnings.push(message);
                }
                continue;
            };

            if observed_rect != expected_rect {
                let message = format!(
                    "Post-apply verify mismatch for '{}': observed ({},{},{},{}) expected ({},{},{},{})",
                    canonical_id,
                    observed_rect.x,
                    observed_rect.y,
                    observed_rect.w,
                    observed_rect.h,
                    expected_rect.x,
                    expected_rect.y,
                    expected_rect.w,
                    expected_rect.h
                );
                if *required {
                    result.errors.push(message);
                    ok = false;
                } else {
                    result.warnings.push(message);
                }
            }
        }

        ok
    }

    fn extract_identity_map(plan: &DisplayPlan) -> HashMap<String, String> {
        let mut map = HashMap::new();

        for step in &plan.steps {
            if step.kind != DisplayPlanStepKind::ResolveIdentity {
                continue;
            }

            let Some(matches) = step.payload.get("matches").and_then(|v| v.as_array()) else {
                continue;
            };

            for item in matches {
                let canonical = item
                    .get("canonical_display_id")
                    .and_then(|v| v.as_str())
                    .map(str::to_string);
                let observed = item
                    .get("observed_display_name")
                    .and_then(|v| v.as_str())
                    .map(str::to_string);

                if let (Some(canonical), Some(observed)) = (canonical, observed) {
                    map.insert(canonical, observed);
                }
            }
        }

        map
    }

    fn extract_expected_rects(payload: &Value) -> Option<Vec<(String, RectI32)>> {
        let arr = payload.get("expected_rects")?.as_array()?;
        let mut out = Vec::new();

        for entry in arr {
            let canonical = entry.get("canonical_display_id")?.as_str()?.to_string();
            let rect = entry.get("rect_px")?;
            let x = rect.get("x")?.as_i64()? as i32;
            let y = rect.get("y")?.as_i64()? as i32;
            let w = rect.get("w")?.as_i64()? as i32;
            let h = rect.get("h")?.as_i64()? as i32;
            out.push((canonical, RectI32 { x, y, w, h }));
        }

        Some(out)
    }

    fn resolve_device_name(
        canonical_or_device: &str,
        identity_map: &HashMap<String, String>,
    ) -> String {
        identity_map
            .get(canonical_or_device)
            .cloned()
            .unwrap_or_else(|| canonical_or_device.to_string())
    }

    fn to_wide(text: &str) -> Vec<u16> {
        text.encode_utf16().chain(std::iter::once(0)).collect()
    }

    fn normalize_device_name(input: &str) -> String {
        if input.starts_with(r"\\.\") {
            return input.to_string();
        }
        if input.starts_with("DISPLAY") {
            return format!(r"\\.\{}", input);
        }
        input.to_string()
    }

    fn wide_to_string(buf: &[u16]) -> String {
        let end = buf.iter().position(|&c| c == 0).unwrap_or(buf.len());
        String::from_utf16_lossy(&buf[..end]).trim().to_string()
    }

    #[derive(Debug, Clone, Copy)]
    struct ModeInfo {
        x: i32,
        y: i32,
        width: u32,
        height: u32,
        refresh_hz: f32,
    }
}
