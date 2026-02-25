use std::collections::{HashMap, HashSet};

use crate::model::{
    DisplayDescriptor, DisplayIdentityEvidence, DisplayPin, DisplaySnapshot, IdentityMatch,
    IdentityResolution, KnownDisplay,
};

#[derive(Debug, Clone)]
struct Candidate {
    known_idx: usize,
    observed_idx: usize,
    score: i32,
    reasons: Vec<String>,
}

pub fn build_known_from_snapshot(snapshot: &DisplaySnapshot) -> Vec<KnownDisplay> {
    snapshot
        .displays
        .iter()
        .enumerate()
        .map(|(idx, display)| KnownDisplay {
            canonical_display_id: format!("display-{}", idx + 1),
            evidence: descriptor_to_evidence(display),
            aliases: vec![display.os_display_name.clone()],
            confidence: 1.0,
            first_seen_utc: Some(snapshot.timestamp_utc.clone()),
            last_seen_utc: Some(snapshot.timestamp_utc.clone()),
        })
        .collect()
}

pub fn resolve_identity(
    known: &[KnownDisplay],
    snapshot: &DisplaySnapshot,
    pins: &[DisplayPin],
) -> IdentityResolution {
    let mut resolution = IdentityResolution::default();
    if known.is_empty() {
        resolution.unresolved_observed = snapshot
            .displays
            .iter()
            .map(|d| d.os_display_name.clone())
            .collect();
        return resolution;
    }

    let mut pinned_lookup: HashMap<&str, &DisplayPin> = HashMap::new();
    for pin in pins {
        pinned_lookup.insert(pin.canonical_display_id.as_str(), pin);
    }

    let mut candidates: Vec<Candidate> = Vec::new();
    for (known_idx, known_display) in known.iter().enumerate() {
        for (observed_idx, observed_display) in snapshot.displays.iter().enumerate() {
            let (score, reasons) = score_candidate(
                known_display,
                observed_display,
                pinned_lookup
                    .get(known_display.canonical_display_id.as_str())
                    .copied(),
            );
            if score > 0 {
                candidates.push(Candidate {
                    known_idx,
                    observed_idx,
                    score,
                    reasons,
                });
            }
        }
    }

    candidates.sort_by(|a, b| b.score.cmp(&a.score));

    let mut used_known = HashSet::new();
    let mut used_observed = HashSet::new();

    for candidate in candidates {
        if used_known.contains(&candidate.known_idx)
            || used_observed.contains(&candidate.observed_idx)
        {
            continue;
        }

        let known_display = &known[candidate.known_idx];
        let observed_display = &snapshot.displays[candidate.observed_idx];
        let confidence = (candidate.score as f32 / 140.0).clamp(0.0, 1.0);

        resolution.matches.push(IdentityMatch {
            canonical_display_id: known_display.canonical_display_id.clone(),
            observed_display_name: observed_display.os_display_name.clone(),
            observed_index: candidate.observed_idx,
            score: candidate.score,
            confidence,
            reasons: candidate.reasons,
        });

        if confidence < 0.5 {
            resolution.warnings.push(format!(
                "Low confidence match for {} -> {} ({:.2})",
                known_display.canonical_display_id, observed_display.os_display_name, confidence
            ));
        }

        used_known.insert(candidate.known_idx);
        used_observed.insert(candidate.observed_idx);
    }

    for (idx, known_display) in known.iter().enumerate() {
        if !used_known.contains(&idx) {
            resolution
                .unresolved_known
                .push(known_display.canonical_display_id.clone());
        }
    }

    for (idx, observed_display) in snapshot.displays.iter().enumerate() {
        if !used_observed.contains(&idx) {
            resolution
                .unresolved_observed
                .push(observed_display.os_display_name.clone());
        }
    }

    resolution
}

pub fn descriptor_to_evidence(display: &DisplayDescriptor) -> DisplayIdentityEvidence {
    DisplayIdentityEvidence {
        os_display_name: Some(display.os_display_name.clone()),
        adapter_id: display.adapter_id,
        target_id: display.target_id,
        monitor_device_path: display.monitor_device_path.clone(),
        pnp_id: display.pnp_id.clone(),
        friendly_name: display.friendly_name.clone(),
        edid_vendor: display.edid_vendor.clone(),
        edid_product_code: display.edid_product_code,
        edid_serial: display.edid_serial,
        edid_hash: display.edid_hash.clone(),
        native_width: display.native_width,
        native_height: display.native_height,
        native_refresh_hz: display.native_refresh_hz,
    }
}

fn score_candidate(
    known_display: &KnownDisplay,
    observed_display: &DisplayDescriptor,
    pin: Option<&DisplayPin>,
) -> (i32, Vec<String>) {
    let mut score = 0;
    let mut reasons = Vec::new();
    let evidence = &known_display.evidence;

    if let Some(pin) = pin {
        if pin
            .monitor_device_path
            .as_ref()
            .zip(observed_display.monitor_device_path.as_ref())
            .map(|(a, b)| a == b)
            .unwrap_or(false)
        {
            score += 100;
            reasons.push("pin:monitor_device_path".to_string());
        }
        if pin
            .pnp_id
            .as_ref()
            .zip(observed_display.pnp_id.as_ref())
            .map(|(a, b)| a == b)
            .unwrap_or(false)
        {
            score += 90;
            reasons.push("pin:pnp_id".to_string());
        }
        if pin
            .adapter_id
            .zip(observed_display.adapter_id)
            .map(|(a, b)| a == b)
            .unwrap_or(false)
            && pin
                .target_id
                .zip(observed_display.target_id)
                .map(|(a, b)| a == b)
                .unwrap_or(false)
        {
            score += 85;
            reasons.push("pin:adapter_target".to_string());
        }
    }

    if evidence
        .edid_serial
        .zip(observed_display.edid_serial)
        .map(|(a, b)| a == b)
        .unwrap_or(false)
        && evidence
            .edid_vendor
            .as_ref()
            .zip(observed_display.edid_vendor.as_ref())
            .map(|(a, b)| a == b)
            .unwrap_or(false)
        && evidence
            .edid_product_code
            .zip(observed_display.edid_product_code)
            .map(|(a, b)| a == b)
            .unwrap_or(false)
    {
        score += 60;
        reasons.push("edid_serial_vendor_product".to_string());
    }

    if evidence
        .edid_hash
        .as_ref()
        .zip(observed_display.edid_hash.as_ref())
        .map(|(a, b)| a == b)
        .unwrap_or(false)
    {
        score += 50;
        reasons.push("edid_hash".to_string());
    }

    if evidence
        .monitor_device_path
        .as_ref()
        .zip(observed_display.monitor_device_path.as_ref())
        .map(|(a, b)| a == b)
        .unwrap_or(false)
    {
        score += 40;
        reasons.push("monitor_device_path".to_string());
    }

    if evidence
        .adapter_id
        .zip(observed_display.adapter_id)
        .map(|(a, b)| a == b)
        .unwrap_or(false)
        && evidence
            .target_id
            .zip(observed_display.target_id)
            .map(|(a, b)| a == b)
            .unwrap_or(false)
    {
        score += 35;
        reasons.push("adapter_target".to_string());
    }

    if evidence
        .pnp_id
        .as_ref()
        .zip(observed_display.pnp_id.as_ref())
        .map(|(a, b)| a == b)
        .unwrap_or(false)
    {
        score += 30;
        reasons.push("pnp_id".to_string());
    }

    if evidence
        .friendly_name
        .as_ref()
        .zip(observed_display.friendly_name.as_ref())
        .map(|(a, b)| a == b)
        .unwrap_or(false)
    {
        score += 12;
        reasons.push("friendly_name".to_string());
    }

    if evidence
        .native_width
        .zip(observed_display.native_width)
        .map(|(a, b)| a == b)
        .unwrap_or(false)
        && evidence
            .native_height
            .zip(observed_display.native_height)
            .map(|(a, b)| a == b)
            .unwrap_or(false)
    {
        score += 8;
        reasons.push("native_resolution".to_string());
    }

    if evidence
        .os_display_name
        .as_ref()
        .map(|name| name == &observed_display.os_display_name)
        .unwrap_or(false)
        || known_display
            .aliases
            .iter()
            .any(|alias| alias == &observed_display.os_display_name)
    {
        score += 5;
        reasons.push("display_name_or_alias".to_string());
    }

    (score, reasons)
}

#[cfg(test)]
mod tests {
    use crate::model::{ConnectorType, DisplayDescriptor, DisplaySnapshot};

    use super::{build_known_from_snapshot, resolve_identity};

    fn make_snapshot() -> DisplaySnapshot {
        let mut snapshot = DisplaySnapshot::default();
        snapshot.displays = vec![
            DisplayDescriptor {
                os_display_name: "DISPLAY1".to_string(),
                monitor_device_path: Some("MONITOR_A".to_string()),
                pnp_id: Some("PNP_A".to_string()),
                edid_hash: Some("hash-a".to_string()),
                edid_vendor: Some("ABC".to_string()),
                edid_product_code: Some(100),
                edid_serial: Some(111),
                connector: ConnectorType::DisplayPort,
                native_width: Some(1920),
                native_height: Some(1080),
                ..Default::default()
            },
            DisplayDescriptor {
                os_display_name: "DISPLAY2".to_string(),
                monitor_device_path: Some("MONITOR_B".to_string()),
                pnp_id: Some("PNP_B".to_string()),
                edid_hash: Some("hash-b".to_string()),
                edid_vendor: Some("ABC".to_string()),
                edid_product_code: Some(101),
                edid_serial: Some(222),
                connector: ConnectorType::DisplayPort,
                native_width: Some(1920),
                native_height: Some(1080),
                ..Default::default()
            },
        ];
        snapshot
    }

    #[test]
    fn identity_resolution_matches_known_displays() {
        let snapshot = make_snapshot();
        let known = build_known_from_snapshot(&snapshot);
        let resolved = resolve_identity(&known, &snapshot, &[]);

        assert_eq!(resolved.matches.len(), 2);
        assert!(resolved.unresolved_known.is_empty());
        assert!(resolved.unresolved_observed.is_empty());
    }
}
