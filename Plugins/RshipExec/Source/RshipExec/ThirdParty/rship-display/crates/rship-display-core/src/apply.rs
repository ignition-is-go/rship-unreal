use crate::model::{ApplyResult, DisplayPlan};

pub fn apply_plan_simulated(plan: &DisplayPlan, dry_run: bool) -> ApplyResult {
    let mut result = ApplyResult {
        success: true,
        dry_run,
        ..Default::default()
    };

    for step in &plan.steps {
        if dry_run {
            result
                .applied_steps
                .push(format!("{} (dry-run)", step.step_id));
            continue;
        }

        result.applied_steps.push(step.step_id.clone());
    }

    if !plan.warnings.is_empty() {
        result.warnings.extend(plan.warnings.clone());
    }

    result
}
