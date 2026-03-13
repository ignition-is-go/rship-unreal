use std::collections::{HashMap, HashSet};

use crate::model::{
    DisplayProfile, DisplaySnapshot, IdentityResolution, OverlapPolicy, RectI32, RectU32,
    ValidationIssue, ValidationReport, ValidationSeverity,
};

pub fn validate_profile(
    profile: &DisplayProfile,
    snapshot: Option<&DisplaySnapshot>,
) -> ValidationReport {
    validate_profile_with_identity(profile, snapshot, None)
}

pub fn validate_profile_with_identity(
    profile: &DisplayProfile,
    snapshot: Option<&DisplaySnapshot>,
    identity: Option<&IdentityResolution>,
) -> ValidationReport {
    let mut report = ValidationReport {
        ok: true,
        issues: Vec::new(),
    };

    if profile.profile_id.trim().is_empty() {
        push_issue(
            &mut report,
            ValidationSeverity::Error,
            "profile.id.missing",
            "Profile id is required",
        );
    }

    let mut required_ids = HashSet::new();
    for required in &profile.required_displays {
        let required = required.trim();
        if required.is_empty() {
            push_issue(
                &mut report,
                ValidationSeverity::Error,
                "required_display.id.missing",
                "Required display id cannot be empty",
            );
            continue;
        }
        if !required_ids.insert(required.to_string()) {
            push_issue(
                &mut report,
                ValidationSeverity::Error,
                "required_display.id.duplicate",
                &format!("Duplicate required display '{}'", required),
            );
        }
    }

    let topology_map = validate_topology(profile, snapshot, &mut report);
    let topology_ids: HashSet<String> = topology_map.keys().cloned().collect();

    if profile.pixel_routes.is_empty() {
        push_issue(
            &mut report,
            ValidationSeverity::Warning,
            "routes.empty",
            "Profile has no pixel routes",
        );
    }

    let observed_names: HashSet<String> = snapshot
        .map(|s| {
            s.displays
                .iter()
                .map(|d| d.os_display_name.clone())
                .collect()
        })
        .unwrap_or_default();

    let resolved_canonical: HashSet<String> = identity
        .map(|i| {
            i.matches
                .iter()
                .map(|m| m.canonical_display_id.clone())
                .collect()
        })
        .unwrap_or_default();

    validate_mosaics(
        profile,
        &observed_names,
        &topology_ids,
        &resolved_canonical,
        &mut report,
    );

    let mut route_ids = HashSet::new();
    for route in &profile.pixel_routes {
        if route.route_id.trim().is_empty() {
            push_issue(
                &mut report,
                ValidationSeverity::Error,
                "routes.id.missing",
                "Pixel route id cannot be empty",
            );
        } else if !route_ids.insert(route.route_id.clone()) {
            push_issue(
                &mut report,
                ValidationSeverity::Error,
                "routes.id.duplicate",
                &format!("Duplicate pixel route id '{}'", route.route_id),
            );
        }

        if route.dest_display_id.trim().is_empty() {
            push_issue(
                &mut report,
                ValidationSeverity::Error,
                "routes.dest_display.missing_id",
                &format!(
                    "Route '{}' destination display id cannot be empty",
                    route.route_id
                ),
            );
        }

        if route.source_rect_px.w == 0 || route.source_rect_px.h == 0 {
            push_issue(
                &mut report,
                ValidationSeverity::Error,
                "routes.source_rect.invalid",
                &format!("Route '{}' source rect must be non-zero", route.route_id),
            );
        }

        if route.dest_rect_px.w == 0 || route.dest_rect_px.h == 0 {
            push_issue(
                &mut report,
                ValidationSeverity::Error,
                "routes.dest_rect.invalid",
                &format!(
                    "Route '{}' destination rect must be non-zero",
                    route.route_id
                ),
            );
        }

        if let Some(topology_rect) = topology_map.get(&route.dest_display_id) {
            let max_w = topology_rect.w.max(0) as u32;
            let max_h = topology_rect.h.max(0) as u32;
            let rect_right = route.dest_rect_px.x.saturating_add(route.dest_rect_px.w);
            let rect_bottom = route.dest_rect_px.y.saturating_add(route.dest_rect_px.h);
            if rect_right > max_w || rect_bottom > max_h {
                push_issue(
                    &mut report,
                    ValidationSeverity::Error,
                    "routes.dest_rect.out_of_bounds",
                    &format!(
                        "Route '{}' destination rect exceeds display '{}' bounds ({}x{})",
                        route.route_id, route.dest_display_id, max_w, max_h
                    ),
                );
            }
        }

        if should_check_presence(snapshot, &topology_ids, &resolved_canonical)
            && !is_resolved_id(
                &route.dest_display_id,
                &observed_names,
                &topology_ids,
                &resolved_canonical,
            )
        {
            let severity = strict_or_warning(profile.strict);
            push_issue(
                &mut report,
                severity,
                "route.dest_display.missing",
                &format!(
                    "Route '{}' destination display '{}' is unresolved",
                    route.route_id, route.dest_display_id
                ),
            );
        }
    }

    if profile.overlap_policy == OverlapPolicy::Forbid {
        let mut by_display: HashMap<&str, Vec<(&str, &RectU32)>> = HashMap::new();
        for route in &profile.pixel_routes {
            if route.enabled {
                by_display
                    .entry(route.dest_display_id.as_str())
                    .or_default()
                    .push((route.route_id.as_str(), &route.dest_rect_px));
            }
        }

        for (display_id, routes) in by_display {
            for i in 0..routes.len() {
                for j in (i + 1)..routes.len() {
                    if rect_overlap(routes[i].1, routes[j].1) {
                        push_issue(
                            &mut report,
                            ValidationSeverity::Error,
                            "routes.overlap",
                            &format!(
                                "Routes '{}' and '{}' overlap on destination display '{}'",
                                routes[i].0, routes[j].0, display_id
                            ),
                        );
                    }
                }
            }
        }
    }

    if should_check_presence(snapshot, &topology_ids, &resolved_canonical) {
        for required in &profile.required_displays {
            if !is_resolved_id(
                required,
                &observed_names,
                &topology_ids,
                &resolved_canonical,
            ) {
                push_issue(
                    &mut report,
                    strict_or_warning(profile.strict),
                    "required_display.missing",
                    &format!("Required display '{}' is unresolved", required),
                );
            }
        }
    }

    report.ok = !report
        .issues
        .iter()
        .any(|issue| issue.severity == ValidationSeverity::Error);
    report
}

fn validate_topology(
    profile: &DisplayProfile,
    snapshot: Option<&DisplaySnapshot>,
    report: &mut ValidationReport,
) -> HashMap<String, RectI32> {
    let mut topology = HashMap::new();
    let observed_rects: HashMap<String, RectI32> = snapshot
        .map(|s| {
            s.displays
                .iter()
                .filter_map(|d| {
                    d.current_rect_px
                        .as_ref()
                        .map(|r| (d.os_display_name.clone(), r.clone()))
                })
                .collect()
        })
        .unwrap_or_default();

    for expected in &profile.topology.expected_rects {
        let id = expected.canonical_display_id.trim();
        if id.is_empty() {
            push_issue(
                report,
                ValidationSeverity::Error,
                "topology.id.missing",
                "Topology expected rect contains empty canonical_display_id",
            );
            continue;
        }

        if expected.rect_px.w <= 0 || expected.rect_px.h <= 0 {
            push_issue(
                report,
                ValidationSeverity::Error,
                "topology.rect.invalid",
                &format!("Topology rect for '{}' must be non-zero", id),
            );
        }

        if topology
            .insert(id.to_string(), expected.rect_px.clone())
            .is_some()
        {
            push_issue(
                report,
                ValidationSeverity::Error,
                "topology.id.duplicate",
                &format!("Duplicate topology canonical_display_id '{}'", id),
            );
        }

        if let Some(observed) = observed_rects.get(id) {
            if observed != &expected.rect_px {
                push_issue(
                    report,
                    if profile.strict && profile.topology.strict {
                        ValidationSeverity::Error
                    } else {
                        ValidationSeverity::Warning
                    },
                    "topology.rect.mismatch",
                    &format!(
                        "Display '{}' current rect ({},{},{},{}) != expected ({},{},{},{})",
                        id,
                        observed.x,
                        observed.y,
                        observed.w,
                        observed.h,
                        expected.rect_px.x,
                        expected.rect_px.y,
                        expected.rect_px.w,
                        expected.rect_px.h
                    ),
                );
            }
        }
    }

    topology
}

fn validate_mosaics(
    profile: &DisplayProfile,
    observed_names: &HashSet<String>,
    topology_ids: &HashSet<String>,
    resolved_canonical: &HashSet<String>,
    report: &mut ValidationReport,
) {
    let mut mosaic_ids = HashSet::new();
    for mosaic in &profile.mosaics {
        let mosaic_id = mosaic.id.trim();
        if mosaic_id.is_empty() {
            push_issue(
                report,
                ValidationSeverity::Error,
                "mosaic.id.missing",
                "Mosaic id cannot be empty",
            );
        } else if !mosaic_ids.insert(mosaic_id.to_string()) {
            push_issue(
                report,
                ValidationSeverity::Error,
                "mosaic.id.duplicate",
                &format!("Duplicate mosaic id '{}'", mosaic_id),
            );
        }

        if mosaic.members.is_empty() {
            push_issue(
                report,
                ValidationSeverity::Error,
                "mosaic.members.empty",
                &format!("Mosaic '{}' has no members", mosaic.id),
            );
        }

        let mut member_ids = HashSet::new();
        for member in &mosaic.members {
            if member.trim().is_empty() {
                push_issue(
                    report,
                    ValidationSeverity::Error,
                    "mosaic.member.empty",
                    &format!("Mosaic '{}' has an empty member id", mosaic.id),
                );
                continue;
            }

            if !member_ids.insert(member.clone()) {
                push_issue(
                    report,
                    ValidationSeverity::Error,
                    "mosaic.member.duplicate",
                    &format!("Mosaic '{}' has duplicate member '{}'", mosaic.id, member),
                );
            }

            if should_check_presence(None, topology_ids, resolved_canonical)
                && !is_resolved_id(member, observed_names, topology_ids, resolved_canonical)
            {
                push_issue(
                    report,
                    strict_or_warning(profile.strict),
                    "mosaic.member.missing",
                    &format!("Mosaic '{}' member '{}' is unresolved", mosaic.id, member),
                );
            }
        }

        if let Some(rows) = mosaic.rows {
            if rows == 0 {
                push_issue(
                    report,
                    ValidationSeverity::Error,
                    "mosaic.rows.invalid",
                    &format!("Mosaic '{}' rows must be > 0", mosaic.id),
                );
            }
        }
        if let Some(cols) = mosaic.cols {
            if cols == 0 {
                push_issue(
                    report,
                    ValidationSeverity::Error,
                    "mosaic.cols.invalid",
                    &format!("Mosaic '{}' cols must be > 0", mosaic.id),
                );
            }
        }

        if let (Some(rows), Some(cols)) = (mosaic.rows, mosaic.cols) {
            if (rows as usize).saturating_mul(cols as usize) != mosaic.members.len() {
                push_issue(
                    report,
                    strict_or_warning(profile.strict),
                    "mosaic.layout.member_count_mismatch",
                    &format!(
                        "Mosaic '{}' rows*cols ({}) does not match member count ({})",
                        mosaic.id,
                        rows as usize * cols as usize,
                        mosaic.members.len()
                    ),
                );
            }
        }

        if let Some(w) = mosaic.expected_canvas_width {
            if w == 0 {
                push_issue(
                    report,
                    ValidationSeverity::Error,
                    "mosaic.canvas_width.invalid",
                    &format!("Mosaic '{}' expected_canvas_width must be > 0", mosaic.id),
                );
            }
        }
        if let Some(h) = mosaic.expected_canvas_height {
            if h == 0 {
                push_issue(
                    report,
                    ValidationSeverity::Error,
                    "mosaic.canvas_height.invalid",
                    &format!("Mosaic '{}' expected_canvas_height must be > 0", mosaic.id),
                );
            }
        }
    }
}

fn strict_or_warning(strict: bool) -> ValidationSeverity {
    if strict {
        ValidationSeverity::Error
    } else {
        ValidationSeverity::Warning
    }
}

fn should_check_presence(
    snapshot: Option<&DisplaySnapshot>,
    topology_ids: &HashSet<String>,
    resolved_canonical: &HashSet<String>,
) -> bool {
    snapshot.is_some() || !topology_ids.is_empty() || !resolved_canonical.is_empty()
}

fn is_resolved_id(
    id: &str,
    observed_names: &HashSet<String>,
    topology_ids: &HashSet<String>,
    resolved_canonical: &HashSet<String>,
) -> bool {
    observed_names.contains(id) || topology_ids.contains(id) || resolved_canonical.contains(id)
}

fn push_issue(
    report: &mut ValidationReport,
    severity: ValidationSeverity,
    code: &str,
    message: &str,
) {
    report.issues.push(ValidationIssue {
        severity,
        code: code.to_string(),
        message: message.to_string(),
    });
}

fn rect_overlap(a: &RectU32, b: &RectU32) -> bool {
    let ax2 = a.x.saturating_add(a.w);
    let ay2 = a.y.saturating_add(a.h);
    let bx2 = b.x.saturating_add(b.w);
    let by2 = b.y.saturating_add(b.h);

    !(ax2 <= b.x || bx2 <= a.x || ay2 <= b.y || by2 <= a.y)
}

#[cfg(test)]
mod tests {
    use crate::model::{
        DisplayDescriptor, DisplayExpectedRect, DisplayProfile, DisplaySnapshot,
        DisplayTopologyProfile, PixelRoute, RectI32, RectU32,
    };

    use super::validate_profile;

    fn base_profile() -> DisplayProfile {
        DisplayProfile {
            profile_id: "test".to_string(),
            strict: true,
            topology: DisplayTopologyProfile {
                strict: true,
                expected_rects: vec![DisplayExpectedRect {
                    canonical_display_id: "wall-left".to_string(),
                    rect_px: RectI32 {
                        x: 0,
                        y: 0,
                        w: 1920,
                        h: 1080,
                    },
                }],
            },
            pixel_routes: vec![PixelRoute {
                route_id: "route-1".to_string(),
                source_canvas_id: "ctx".to_string(),
                source_rect_px: RectU32 {
                    x: 0,
                    y: 0,
                    w: 1920,
                    h: 1080,
                },
                dest_display_id: "wall-left".to_string(),
                dest_rect_px: RectU32 {
                    x: 0,
                    y: 0,
                    w: 1920,
                    h: 1080,
                },
                transform: Default::default(),
                sampling: Default::default(),
                priority: 0,
                enabled: true,
            }],
            ..Default::default()
        }
    }

    #[test]
    fn route_dest_can_resolve_via_topology_canonical_id() {
        let profile = base_profile();
        let snapshot = DisplaySnapshot {
            displays: vec![DisplayDescriptor {
                os_display_name: "DISPLAY1".to_string(),
                ..Default::default()
            }],
            ..Default::default()
        };

        let report = validate_profile(&profile, Some(&snapshot));
        assert!(report.ok, "expected no hard errors: {:?}", report.issues);
    }

    #[test]
    fn detects_topology_duplicates() {
        let mut profile = base_profile();
        profile.topology.expected_rects.push(DisplayExpectedRect {
            canonical_display_id: "wall-left".to_string(),
            rect_px: RectI32 {
                x: 1920,
                y: 0,
                w: 1920,
                h: 1080,
            },
        });

        let report = validate_profile(&profile, None);
        assert!(!report.ok);
        assert!(report
            .issues
            .iter()
            .any(|i| i.code == "topology.id.duplicate"));
    }
}
