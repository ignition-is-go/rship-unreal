use std::fs;
use std::path::PathBuf;
use std::time::Duration;

use clap::{Parser, Subcommand};
use rship_display_core::{
    build_known_from_snapshot, build_pixel_ledger, parse_json, plan_profile, resolve_identity,
    validate_profile, DisplayPin, DisplayPlan, DisplayProfile, DisplaySnapshot, KnownDisplay,
};
use rship_display_windows::{apply_plan, collect_snapshot};
use serde_json::json;

#[derive(Debug, Parser)]
#[command(name = "rship-display-cli")]
#[command(about = "Deterministic display management CLI for Windows-first workflows")]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Debug, Subcommand)]
enum Command {
    Snapshot {
        #[arg(long)]
        out: Option<PathBuf>,
        #[arg(long, default_value_t = false)]
        pretty: bool,
    },
    BuildKnown {
        #[arg(long)]
        snapshot: PathBuf,
        #[arg(long)]
        out: Option<PathBuf>,
        #[arg(long, default_value_t = false)]
        pretty: bool,
    },
    Resolve {
        #[arg(long)]
        known: PathBuf,
        #[arg(long)]
        snapshot: PathBuf,
        #[arg(long)]
        pins: Option<PathBuf>,
        #[arg(long)]
        out: Option<PathBuf>,
        #[arg(long, default_value_t = false)]
        pretty: bool,
    },
    Validate {
        #[arg(long)]
        profile: PathBuf,
        #[arg(long)]
        snapshot: Option<PathBuf>,
        #[arg(long)]
        out: Option<PathBuf>,
        #[arg(long, default_value_t = false)]
        pretty: bool,
    },
    Plan {
        #[arg(long)]
        profile: PathBuf,
        #[arg(long)]
        snapshot: PathBuf,
        #[arg(long)]
        known: Option<PathBuf>,
        #[arg(long)]
        out: Option<PathBuf>,
        #[arg(long, default_value_t = false)]
        pretty: bool,
    },
    Ledger {
        #[arg(long)]
        profile: PathBuf,
        #[arg(long)]
        snapshot: PathBuf,
        #[arg(long)]
        known: Option<PathBuf>,
        #[arg(long)]
        pins: Option<PathBuf>,
        #[arg(long)]
        out: Option<PathBuf>,
        #[arg(long, default_value_t = false)]
        pretty: bool,
    },
    Apply {
        #[arg(long)]
        plan: PathBuf,
        #[arg(long, default_value_t = false)]
        dry_run: bool,
        #[arg(long)]
        out: Option<PathBuf>,
        #[arg(long, default_value_t = false)]
        pretty: bool,
    },
    Watch {
        #[arg(long, default_value_t = 2)]
        interval_secs: u64,
    },
}

fn main() -> Result<(), String> {
    let cli = Cli::parse();

    match cli.command {
        Command::Snapshot { out, pretty } => {
            let snapshot = collect_snapshot()?;
            write_json(&snapshot, out, pretty)
        }
        Command::BuildKnown {
            snapshot,
            out,
            pretty,
        } => {
            let snapshot: DisplaySnapshot = read_json_file(&snapshot)?;
            let known = build_known_from_snapshot(&snapshot);
            write_json(&known, out, pretty)
        }
        Command::Resolve {
            known,
            snapshot,
            pins,
            out,
            pretty,
        } => {
            let known: Vec<KnownDisplay> = read_json_file(&known)?;
            let snapshot: DisplaySnapshot = read_json_file(&snapshot)?;
            let pins: Vec<DisplayPin> = if let Some(path) = pins {
                read_json_file(&path)?
            } else {
                Vec::new()
            };
            let resolution = resolve_identity(&known, &snapshot, &pins);
            write_json(&resolution, out, pretty)
        }
        Command::Validate {
            profile,
            snapshot,
            out,
            pretty,
        } => {
            let profile: DisplayProfile = read_json_file(&profile)?;
            let snapshot = if let Some(path) = snapshot {
                Some(read_json_file::<DisplaySnapshot>(&path)?)
            } else {
                None
            };
            let report = validate_profile(&profile, snapshot.as_ref());
            write_json(&report, out, pretty)
        }
        Command::Plan {
            profile,
            snapshot,
            known,
            out,
            pretty,
        } => {
            let profile: DisplayProfile = read_json_file(&profile)?;
            let snapshot: DisplaySnapshot = read_json_file(&snapshot)?;
            let known_displays: Vec<KnownDisplay> = if let Some(path) = known {
                read_json_file(&path)?
            } else {
                build_known_from_snapshot(&snapshot)
            };

            let (plan, identity, validation) = plan_profile(&profile, &snapshot, &known_displays);
            let ledger = build_pixel_ledger(&profile, &identity, Some(&snapshot));
            let payload = json!({
                "plan": plan,
                "identity": identity,
                "validation": validation,
                "ledger": ledger,
            });
            write_json_value(payload, out, pretty)
        }
        Command::Ledger {
            profile,
            snapshot,
            known,
            pins,
            out,
            pretty,
        } => {
            let profile: DisplayProfile = read_json_file(&profile)?;
            let snapshot: DisplaySnapshot = read_json_file(&snapshot)?;
            let known_displays: Vec<KnownDisplay> = if let Some(path) = known {
                read_json_file(&path)?
            } else {
                build_known_from_snapshot(&snapshot)
            };
            let pins: Vec<DisplayPin> = if let Some(path) = pins {
                read_json_file(&path)?
            } else {
                Vec::new()
            };

            let identity = resolve_identity(&known_displays, &snapshot, &pins);
            let ledger = build_pixel_ledger(&profile, &identity, Some(&snapshot));
            let payload = json!({
                "identity": identity,
                "ledger": ledger,
            });
            write_json_value(payload, out, pretty)
        }
        Command::Apply {
            plan,
            dry_run,
            out,
            pretty,
        } => {
            let plan: DisplayPlan = read_json_file(&plan)?;
            let result = apply_plan(&plan, dry_run)?;
            write_json(&result, out, pretty)
        }
        Command::Watch { interval_secs } => loop {
            match collect_snapshot() {
                Ok(snapshot) => {
                    let compact = serde_json::to_string(&snapshot)
                        .map_err(|err| format!("Failed to serialize snapshot: {}", err))?;
                    println!("{}", compact);
                }
                Err(err) => {
                    let payload = json!({ "ok": false, "error": err });
                    println!("{}", payload);
                }
            }

            std::thread::sleep(Duration::from_secs(interval_secs.max(1)));
        },
    }
}

fn read_json_file<T: serde::de::DeserializeOwned>(path: &PathBuf) -> Result<T, String> {
    let raw = fs::read_to_string(path)
        .map_err(|err| format!("Failed to read {}: {}", path.display(), err))?;
    parse_json::<T>(&raw).map_err(|err| err.to_string())
}

fn write_json<T: serde::Serialize>(
    value: &T,
    out: Option<PathBuf>,
    pretty: bool,
) -> Result<(), String> {
    if pretty {
        let text = serde_json::to_string_pretty(value)
            .map_err(|err| format!("JSON encode failed: {}", err))?;
        write_text(text, out)
    } else {
        let text =
            serde_json::to_string(value).map_err(|err| format!("JSON encode failed: {}", err))?;
        write_text(text, out)
    }
}

fn write_json_value(
    value: serde_json::Value,
    out: Option<PathBuf>,
    pretty: bool,
) -> Result<(), String> {
    if pretty {
        let text = serde_json::to_string_pretty(&value)
            .map_err(|err| format!("JSON encode failed: {}", err))?;
        write_text(text, out)
    } else {
        write_text(value.to_string(), out)
    }
}

fn write_text(text: String, out: Option<PathBuf>) -> Result<(), String> {
    if let Some(path) = out {
        fs::write(&path, text).map_err(|err| format!("Failed to write {}: {}", path.display(), err))
    } else {
        println!("{}", text);
        Ok(())
    }
}
