use chrono::Utc;
use serde::{Deserialize, Serialize};
use serde_json::Value;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub struct RectI32 {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

impl Default for RectI32 {
    fn default() -> Self {
        Self {
            x: 0,
            y: 0,
            w: 0,
            h: 0,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub struct RectU32 {
    pub x: u32,
    pub y: u32,
    pub w: u32,
    pub h: u32,
}

impl Default for RectU32 {
    fn default() -> Self {
        Self {
            x: 0,
            y: 0,
            w: 0,
            h: 0,
        }
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum ConnectorType {
    Unknown,
    Hdmi,
    DisplayPort,
    Dvi,
    Vga,
    Embedded,
}

impl Default for ConnectorType {
    fn default() -> Self {
        Self::Unknown
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum DisplayRotation {
    Deg0,
    Deg90,
    Deg180,
    Deg270,
}

impl Default for DisplayRotation {
    fn default() -> Self {
        Self::Deg0
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct DisplayDescriptor {
    pub os_display_name: String,
    pub adapter_id: Option<u64>,
    pub target_id: Option<u32>,
    pub monitor_device_path: Option<String>,
    pub pnp_id: Option<String>,
    pub friendly_name: Option<String>,
    pub edid_vendor: Option<String>,
    pub edid_product_code: Option<u16>,
    pub edid_serial: Option<u32>,
    pub edid_hash: Option<String>,
    pub connector: ConnectorType,
    pub native_width: Option<u32>,
    pub native_height: Option<u32>,
    pub native_refresh_hz: Option<f32>,
    pub current_rect_px: Option<RectI32>,
    pub current_rotation: DisplayRotation,
    pub hdr_enabled: Option<bool>,
    pub bits_per_color: Option<u8>,
    pub is_active: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct DisplayPath {
    pub source_display_name: Option<String>,
    pub target_display_name: Option<String>,
    pub source_rect_px: Option<RectI32>,
    pub target_rect_px: Option<RectI32>,
    pub active: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DisplaySnapshot {
    pub timestamp_utc: String,
    pub machine_id: Option<String>,
    pub displays: Vec<DisplayDescriptor>,
    pub paths: Vec<DisplayPath>,
    pub metadata: Value,
}

impl Default for DisplaySnapshot {
    fn default() -> Self {
        Self {
            timestamp_utc: now_utc(),
            machine_id: None,
            displays: Vec::new(),
            paths: Vec::new(),
            metadata: Value::Object(Default::default()),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct DisplayIdentityEvidence {
    pub os_display_name: Option<String>,
    pub adapter_id: Option<u64>,
    pub target_id: Option<u32>,
    pub monitor_device_path: Option<String>,
    pub pnp_id: Option<String>,
    pub friendly_name: Option<String>,
    pub edid_vendor: Option<String>,
    pub edid_product_code: Option<u16>,
    pub edid_serial: Option<u32>,
    pub edid_hash: Option<String>,
    pub native_width: Option<u32>,
    pub native_height: Option<u32>,
    pub native_refresh_hz: Option<f32>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct KnownDisplay {
    pub canonical_display_id: String,
    pub evidence: DisplayIdentityEvidence,
    pub aliases: Vec<String>,
    pub confidence: f32,
    pub first_seen_utc: Option<String>,
    pub last_seen_utc: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct DisplayPin {
    pub canonical_display_id: String,
    pub monitor_device_path: Option<String>,
    pub pnp_id: Option<String>,
    pub adapter_id: Option<u64>,
    pub target_id: Option<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum MosaicBackend {
    None,
    Nvidia,
    Amd,
    Software,
}

impl Default for MosaicBackend {
    fn default() -> Self {
        Self::None
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct MosaicGroup {
    pub id: String,
    pub members: Vec<String>,
    pub rows: Option<u32>,
    pub cols: Option<u32>,
    pub expected_canvas_width: Option<u32>,
    pub expected_canvas_height: Option<u32>,
    pub backend: MosaicBackend,
    pub must_be_single_os_display: bool,
    pub allow_software_fallback: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum RouteTransform {
    None,
    Rotate90,
    Rotate180,
    Rotate270,
    FlipX,
    FlipY,
}

impl Default for RouteTransform {
    fn default() -> Self {
        Self::None
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum SamplingMode {
    Nearest,
    Linear,
    Cubic,
}

impl Default for SamplingMode {
    fn default() -> Self {
        Self::Linear
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PixelRoute {
    pub route_id: String,
    pub source_canvas_id: String,
    pub source_rect_px: RectU32,
    pub dest_display_id: String,
    pub dest_rect_px: RectU32,
    pub transform: RouteTransform,
    pub sampling: SamplingMode,
    pub priority: i32,
    pub enabled: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct PixelLedgerEntry {
    pub route_id: String,
    pub source_canvas_id: String,
    pub source_rect_px: RectU32,
    pub canonical_dest_display_id: String,
    pub observed_dest_display_name: Option<String>,
    pub dest_rect_px: RectU32,
    pub transform: RouteTransform,
    pub sampling: SamplingMode,
    pub priority: i32,
    pub enabled: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct PixelLedger {
    pub generated_at_utc: String,
    pub profile_id: Option<String>,
    pub entries: Vec<PixelLedgerEntry>,
    pub unresolved_destinations: Vec<String>,
    pub warnings: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum OverlapPolicy {
    Forbid,
    AllowWithPriority,
}

impl Default for OverlapPolicy {
    fn default() -> Self {
        Self::Forbid
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct DisplayTopologyProfile {
    pub strict: bool,
    pub expected_rects: Vec<DisplayExpectedRect>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct DisplayExpectedRect {
    pub canonical_display_id: String,
    pub rect_px: RectI32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DisplayProfile {
    pub profile_id: String,
    pub name: String,
    pub strict: bool,
    pub required_displays: Vec<String>,
    pub overlap_policy: OverlapPolicy,
    pub topology: DisplayTopologyProfile,
    pub mosaics: Vec<MosaicGroup>,
    pub pixel_routes: Vec<PixelRoute>,
    pub pins: Vec<DisplayPin>,
    pub metadata: Value,
}

impl Default for DisplayProfile {
    fn default() -> Self {
        Self {
            profile_id: String::new(),
            name: String::new(),
            strict: true,
            required_displays: Vec::new(),
            overlap_policy: OverlapPolicy::Forbid,
            topology: DisplayTopologyProfile::default(),
            mosaics: Vec::new(),
            pixel_routes: Vec::new(),
            pins: Vec::new(),
            metadata: Value::Object(Default::default()),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum ValidationSeverity {
    Info,
    Warning,
    Error,
}

impl Default for ValidationSeverity {
    fn default() -> Self {
        Self::Info
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct ValidationIssue {
    pub severity: ValidationSeverity,
    pub code: String,
    pub message: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct ValidationReport {
    pub ok: bool,
    pub issues: Vec<ValidationIssue>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum DisplayPlanStepKind {
    ResolveIdentity,
    SetTopology,
    SetMode,
    EnableMosaic,
    DisableMosaic,
    ApplyPixelRoute,
    Verify,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DisplayPlanStep {
    pub step_id: String,
    pub kind: DisplayPlanStepKind,
    pub required: bool,
    pub target_id: Option<String>,
    pub payload: Value,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DisplayPlan {
    pub plan_id: String,
    pub created_at_utc: String,
    pub profile_id: Option<String>,
    pub warnings: Vec<String>,
    pub steps: Vec<DisplayPlanStep>,
}

impl Default for DisplayPlan {
    fn default() -> Self {
        Self {
            plan_id: String::new(),
            created_at_utc: now_utc(),
            profile_id: None,
            warnings: Vec::new(),
            steps: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct ApplyResult {
    pub success: bool,
    pub dry_run: bool,
    pub applied_steps: Vec<String>,
    pub failed_steps: Vec<String>,
    pub warnings: Vec<String>,
    pub errors: Vec<String>,
    pub post_snapshot: Option<DisplaySnapshot>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct IdentityMatch {
    pub canonical_display_id: String,
    pub observed_display_name: String,
    pub observed_index: usize,
    pub score: i32,
    pub confidence: f32,
    pub reasons: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct IdentityResolution {
    pub matches: Vec<IdentityMatch>,
    pub unresolved_known: Vec<String>,
    pub unresolved_observed: Vec<String>,
    pub warnings: Vec<String>,
}

pub fn now_utc() -> String {
    Utc::now().to_rfc3339()
}
