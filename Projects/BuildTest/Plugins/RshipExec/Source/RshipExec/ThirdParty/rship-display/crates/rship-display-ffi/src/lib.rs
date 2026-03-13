use std::ffi::{c_char, CStr, CString};

use rship_display_core::{
    build_known_from_snapshot, build_pixel_ledger, parse_json, plan_profile, resolve_identity,
    to_json_string, validate_profile, DisplayPin, DisplayPlan, DisplayProfile, DisplaySnapshot,
    KnownDisplay,
};
use rship_display_windows::{apply_plan, collect_snapshot};
use serde_json::json;

fn to_c_string_owned(text: String) -> *mut c_char {
    let sanitized = text.replace('\0', "\\u0000");
    match CString::new(sanitized) {
        Ok(s) => s.into_raw(),
        Err(_) => CString::new("{\"ok\":false,\"error\":\"Failed to create CString\"}")
            .expect("CString literal")
            .into_raw(),
    }
}

fn json_success<T: serde::Serialize>(value: &T) -> *mut c_char {
    let payload = json!({
        "ok": true,
        "data": value,
    });
    to_c_string_owned(payload.to_string())
}

fn json_error(message: &str) -> *mut c_char {
    let payload = json!({
        "ok": false,
        "error": message,
    });
    to_c_string_owned(payload.to_string())
}

unsafe fn read_required_utf8(ptr: *const c_char, arg_name: &str) -> Result<String, String> {
    if ptr.is_null() {
        return Err(format!("{} pointer was null", arg_name));
    }

    let c_str = CStr::from_ptr(ptr);
    c_str
        .to_str()
        .map(|s| s.to_string())
        .map_err(|err| format!("{} was not valid UTF-8: {}", arg_name, err))
}

unsafe fn read_optional_json<T: serde::de::DeserializeOwned>(
    ptr: *const c_char,
    arg_name: &str,
) -> Result<Option<T>, String> {
    if ptr.is_null() {
        return Ok(None);
    }

    let raw = read_required_utf8(ptr, arg_name)?;
    if raw.trim().is_empty() {
        return Ok(None);
    }

    parse_json::<T>(&raw)
        .map(Some)
        .map_err(|err| format!("Failed to parse {} JSON: {}", arg_name, err))
}

fn panic_guard<F>(f: F) -> *mut c_char
where
    F: FnOnce() -> *mut c_char + std::panic::UnwindSafe,
{
    match std::panic::catch_unwind(f) {
        Ok(ptr) => ptr,
        Err(_) => json_error("Rust panic while executing display FFI call"),
    }
}

#[no_mangle]
pub extern "C" fn rship_display_version() -> *mut c_char {
    panic_guard(|| json_success(&json!({ "version": env!("CARGO_PKG_VERSION") })))
}

#[no_mangle]
pub unsafe extern "C" fn rship_display_free_string(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }

    let _ = CString::from_raw(ptr);
}

#[no_mangle]
pub unsafe extern "C" fn rship_display_collect_snapshot_json() -> *mut c_char {
    panic_guard(|| match collect_snapshot() {
        Ok(snapshot) => json_success(&snapshot),
        Err(err) => json_error(&err),
    })
}

#[no_mangle]
pub unsafe extern "C" fn rship_display_build_known_from_snapshot_json(
    snapshot_json: *const c_char,
) -> *mut c_char {
    panic_guard(|| {
        let snapshot_raw = match read_required_utf8(snapshot_json, "snapshot_json") {
            Ok(value) => value,
            Err(err) => return json_error(&err),
        };

        let snapshot: DisplaySnapshot = match parse_json(&snapshot_raw) {
            Ok(value) => value,
            Err(err) => return json_error(&format!("Failed to parse snapshot_json: {}", err)),
        };

        let known = build_known_from_snapshot(&snapshot);
        json_success(&known)
    })
}

#[no_mangle]
pub unsafe extern "C" fn rship_display_resolve_identity_json(
    known_json: *const c_char,
    snapshot_json: *const c_char,
    pins_json: *const c_char,
) -> *mut c_char {
    panic_guard(|| {
        let known_raw = match read_required_utf8(known_json, "known_json") {
            Ok(value) => value,
            Err(err) => return json_error(&err),
        };
        let snapshot_raw = match read_required_utf8(snapshot_json, "snapshot_json") {
            Ok(value) => value,
            Err(err) => return json_error(&err),
        };

        let known: Vec<KnownDisplay> = match parse_json(&known_raw) {
            Ok(value) => value,
            Err(err) => return json_error(&format!("Failed to parse known_json: {}", err)),
        };
        let snapshot: DisplaySnapshot = match parse_json(&snapshot_raw) {
            Ok(value) => value,
            Err(err) => return json_error(&format!("Failed to parse snapshot_json: {}", err)),
        };
        let pins: Vec<DisplayPin> = match read_optional_json(pins_json, "pins_json") {
            Ok(Some(value)) => value,
            Ok(None) => Vec::new(),
            Err(err) => return json_error(&err),
        };

        let resolution = resolve_identity(&known, &snapshot, &pins);
        json_success(&resolution)
    })
}

#[no_mangle]
pub unsafe extern "C" fn rship_display_validate_profile_json(
    profile_json: *const c_char,
    snapshot_json: *const c_char,
) -> *mut c_char {
    panic_guard(|| {
        let profile_raw = match read_required_utf8(profile_json, "profile_json") {
            Ok(value) => value,
            Err(err) => return json_error(&err),
        };

        let profile: DisplayProfile = match parse_json(&profile_raw) {
            Ok(value) => value,
            Err(err) => return json_error(&format!("Failed to parse profile_json: {}", err)),
        };

        let snapshot = match read_optional_json::<DisplaySnapshot>(snapshot_json, "snapshot_json") {
            Ok(value) => value,
            Err(err) => return json_error(&err),
        };

        let report = validate_profile(&profile, snapshot.as_ref());
        json_success(&report)
    })
}

#[no_mangle]
pub unsafe extern "C" fn rship_display_plan_profile_json(
    profile_json: *const c_char,
    snapshot_json: *const c_char,
    known_json: *const c_char,
) -> *mut c_char {
    panic_guard(|| {
        let profile_raw = match read_required_utf8(profile_json, "profile_json") {
            Ok(value) => value,
            Err(err) => return json_error(&err),
        };
        let snapshot_raw = match read_required_utf8(snapshot_json, "snapshot_json") {
            Ok(value) => value,
            Err(err) => return json_error(&err),
        };

        let profile: DisplayProfile = match parse_json(&profile_raw) {
            Ok(value) => value,
            Err(err) => return json_error(&format!("Failed to parse profile_json: {}", err)),
        };
        let snapshot: DisplaySnapshot = match parse_json(&snapshot_raw) {
            Ok(value) => value,
            Err(err) => return json_error(&format!("Failed to parse snapshot_json: {}", err)),
        };

        let known_displays: Vec<KnownDisplay> = match read_optional_json(known_json, "known_json") {
            Ok(Some(value)) => value,
            Ok(None) => build_known_from_snapshot(&snapshot),
            Err(err) => return json_error(&err),
        };

        let (plan, identity, validation) = plan_profile(&profile, &snapshot, &known_displays);
        let ledger = build_pixel_ledger(&profile, &identity, Some(&snapshot));
        let payload = json!({
            "plan": plan,
            "identity": identity,
            "validation": validation,
            "ledger": ledger,
        });

        json_success(&payload)
    })
}

#[no_mangle]
pub unsafe extern "C" fn rship_display_apply_plan_json(
    plan_json: *const c_char,
    dry_run: bool,
) -> *mut c_char {
    panic_guard(|| {
        let plan_raw = match read_required_utf8(plan_json, "plan_json") {
            Ok(value) => value,
            Err(err) => return json_error(&err),
        };

        let plan: DisplayPlan = match parse_json(&plan_raw) {
            Ok(value) => value,
            Err(err) => return json_error(&format!("Failed to parse plan_json: {}", err)),
        };

        match apply_plan(&plan, dry_run) {
            Ok(result) => json_success(&result),
            Err(err) => json_error(&err),
        }
    })
}

#[allow(dead_code)]
fn _roundtrip_json<T: serde::Serialize>(value: &T) -> Result<String, String> {
    to_json_string(value).map_err(|err| err.to_string())
}
