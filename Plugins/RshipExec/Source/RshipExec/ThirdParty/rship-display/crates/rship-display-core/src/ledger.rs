use std::collections::{HashMap, HashSet};

use crate::model::{
    now_utc, DisplayProfile, DisplaySnapshot, IdentityResolution, PixelLedger, PixelLedgerEntry,
};

pub fn build_pixel_ledger(
    profile: &DisplayProfile,
    identity: &IdentityResolution,
    snapshot: Option<&DisplaySnapshot>,
) -> PixelLedger {
    let mut ledger = PixelLedger {
        generated_at_utc: now_utc(),
        profile_id: if profile.profile_id.trim().is_empty() {
            None
        } else {
            Some(profile.profile_id.clone())
        },
        ..Default::default()
    };

    let identity_map: HashMap<&str, &str> = identity
        .matches
        .iter()
        .map(|m| {
            (
                m.canonical_display_id.as_str(),
                m.observed_display_name.as_str(),
            )
        })
        .collect();
    let observed_names: HashSet<&str> = snapshot
        .map(|s| {
            s.displays
                .iter()
                .map(|d| d.os_display_name.as_str())
                .collect()
        })
        .unwrap_or_default();

    for route in &profile.pixel_routes {
        let canonical_dest = route.dest_display_id.clone();
        let observed_dest =
            resolve_observed_display(&canonical_dest, &identity_map, &observed_names);

        if observed_dest.is_none() {
            ledger.unresolved_destinations.push(canonical_dest.clone());
        }

        ledger.entries.push(PixelLedgerEntry {
            route_id: route.route_id.clone(),
            source_canvas_id: route.source_canvas_id.clone(),
            source_rect_px: route.source_rect_px.clone(),
            canonical_dest_display_id: canonical_dest,
            observed_dest_display_name: observed_dest,
            dest_rect_px: route.dest_rect_px.clone(),
            transform: route.transform.clone(),
            sampling: route.sampling.clone(),
            priority: route.priority,
            enabled: route.enabled,
        });
    }

    ledger.unresolved_destinations.sort();
    ledger.unresolved_destinations.dedup();

    if !identity.unresolved_known.is_empty() {
        ledger.warnings.push(format!(
            "Identity unresolved {} known display(s)",
            identity.unresolved_known.len()
        ));
    }
    if !identity.unresolved_observed.is_empty() {
        ledger.warnings.push(format!(
            "Identity unresolved {} observed display(s)",
            identity.unresolved_observed.len()
        ));
    }
    if !ledger.unresolved_destinations.is_empty() {
        ledger.warnings.push(format!(
            "{} route destination display id(s) could not be resolved",
            ledger.unresolved_destinations.len()
        ));
    }

    ledger
}

fn resolve_observed_display(
    canonical_dest: &str,
    identity_map: &HashMap<&str, &str>,
    observed_names: &HashSet<&str>,
) -> Option<String> {
    if let Some(mapped) = identity_map.get(canonical_dest) {
        return Some((*mapped).to_string());
    }

    if observed_names.contains(canonical_dest) {
        return Some(canonical_dest.to_string());
    }

    None
}

#[cfg(test)]
mod tests {
    use crate::model::{
        DisplayDescriptor, DisplayProfile, DisplaySnapshot, IdentityMatch, IdentityResolution,
        PixelRoute, RectU32,
    };

    use super::build_pixel_ledger;

    #[test]
    fn ledger_maps_canonical_route_destination_to_observed_display() {
        let profile = DisplayProfile {
            profile_id: "wall".to_string(),
            pixel_routes: vec![PixelRoute {
                route_id: "r-1".to_string(),
                source_canvas_id: "ctx".to_string(),
                source_rect_px: RectU32 {
                    x: 0,
                    y: 0,
                    w: 100,
                    h: 100,
                },
                dest_display_id: "left".to_string(),
                dest_rect_px: RectU32 {
                    x: 0,
                    y: 0,
                    w: 100,
                    h: 100,
                },
                transform: Default::default(),
                sampling: Default::default(),
                priority: 0,
                enabled: true,
            }],
            ..Default::default()
        };
        let identity = IdentityResolution {
            matches: vec![IdentityMatch {
                canonical_display_id: "left".to_string(),
                observed_display_name: "DISPLAY2".to_string(),
                ..Default::default()
            }],
            ..Default::default()
        };
        let snapshot = DisplaySnapshot {
            displays: vec![DisplayDescriptor {
                os_display_name: "DISPLAY2".to_string(),
                ..Default::default()
            }],
            ..Default::default()
        };

        let ledger = build_pixel_ledger(&profile, &identity, Some(&snapshot));
        assert_eq!(ledger.entries.len(), 1);
        assert_eq!(
            ledger.entries[0].observed_dest_display_name.as_deref(),
            Some("DISPLAY2")
        );
        assert!(ledger.unresolved_destinations.is_empty());
    }
}
