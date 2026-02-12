use rship_display_core::MosaicGroup;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct NvMosaicCapability {
    pub available: bool,
    pub reason: String,
}

pub fn probe_capability() -> NvMosaicCapability {
    #[cfg(windows)]
    {
        NvMosaicCapability {
            available: false,
            reason: "NVIDIA mosaic API integration not yet linked".to_string(),
        }
    }

    #[cfg(not(windows))]
    {
        NvMosaicCapability {
            available: false,
            reason: "NVIDIA mosaic backend is only supported on Windows".to_string(),
        }
    }
}

pub fn apply_mosaic_group(group: &MosaicGroup) -> Result<(), String> {
    let capability = probe_capability();
    if !capability.available {
        return Err(format!(
            "Unable to apply NVIDIA mosaic group '{}': {}",
            group.id, capability.reason
        ));
    }

    Ok(())
}
