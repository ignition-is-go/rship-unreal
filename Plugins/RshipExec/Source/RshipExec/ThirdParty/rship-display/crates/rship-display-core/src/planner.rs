use serde_json::json;
use uuid::Uuid;

use crate::identity::resolve_identity;
use crate::model::{
    DisplayPlan, DisplayPlanStep, DisplayPlanStepKind, DisplayProfile, DisplaySnapshot,
    IdentityResolution, KnownDisplay, ValidationReport,
};
use crate::validate::validate_profile_with_identity;

pub fn plan_profile(
    profile: &DisplayProfile,
    snapshot: &DisplaySnapshot,
    known_displays: &[KnownDisplay],
) -> (DisplayPlan, IdentityResolution, ValidationReport) {
    let identity = resolve_identity(known_displays, snapshot, &profile.pins);
    let validation = validate_profile_with_identity(profile, Some(snapshot), Some(&identity));

    let mut plan = DisplayPlan::default();
    plan.plan_id = Uuid::new_v4().to_string();
    if !profile.profile_id.is_empty() {
        plan.profile_id = Some(profile.profile_id.clone());
    }

    plan.steps.push(DisplayPlanStep {
        step_id: format!("resolve-{}", Uuid::new_v4()),
        kind: DisplayPlanStepKind::ResolveIdentity,
        required: true,
        target_id: None,
        payload: serde_json::to_value(&identity).unwrap_or_else(|_| json!({})),
    });

    if !profile.topology.expected_rects.is_empty() {
        plan.steps.push(DisplayPlanStep {
            step_id: format!("topology-{}", Uuid::new_v4()),
            kind: DisplayPlanStepKind::SetTopology,
            required: profile.strict,
            target_id: None,
            payload: serde_json::to_value(&profile.topology).unwrap_or_else(|_| json!({})),
        });
    }

    for mosaic in &profile.mosaics {
        plan.steps.push(DisplayPlanStep {
            step_id: format!("mosaic-{}", Uuid::new_v4()),
            kind: DisplayPlanStepKind::EnableMosaic,
            required: profile.strict,
            target_id: Some(mosaic.id.clone()),
            payload: serde_json::to_value(mosaic).unwrap_or_else(|_| json!({})),
        });
    }

    for route in &profile.pixel_routes {
        if route.enabled {
            plan.steps.push(DisplayPlanStep {
                step_id: format!("route-{}", route.route_id),
                kind: DisplayPlanStepKind::ApplyPixelRoute,
                required: true,
                target_id: Some(route.dest_display_id.clone()),
                payload: serde_json::to_value(route).unwrap_or_else(|_| json!({})),
            });
        }
    }

    plan.steps.push(DisplayPlanStep {
        step_id: format!("verify-{}", Uuid::new_v4()),
        kind: DisplayPlanStepKind::Verify,
        required: true,
        target_id: None,
        payload: json!({
            "strict": profile.strict,
            "requiredDisplays": profile.required_displays,
        }),
    });

    if !validation.ok {
        plan.warnings
            .push("Profile has validation errors; apply may fail in strict mode".to_string());
    }
    let validation_error_count = validation
        .issues
        .iter()
        .filter(|issue| issue.severity == crate::model::ValidationSeverity::Error)
        .count();
    let validation_warning_count = validation
        .issues
        .iter()
        .filter(|issue| issue.severity == crate::model::ValidationSeverity::Warning)
        .count();
    if validation_error_count > 0 {
        plan.warnings.push(format!(
            "Validation reported {} error(s)",
            validation_error_count
        ));
    }
    if validation_warning_count > 0 {
        plan.warnings.push(format!(
            "Validation reported {} warning(s)",
            validation_warning_count
        ));
    }

    if !identity.unresolved_known.is_empty() {
        plan.warnings.push(format!(
            "{} known displays were unresolved during identity resolution",
            identity.unresolved_known.len()
        ));
    }

    if !identity.unresolved_observed.is_empty() {
        plan.warnings.push(format!(
            "{} observed displays were not mapped to known canonical IDs",
            identity.unresolved_observed.len()
        ));
    }

    (plan, identity, validation)
}
