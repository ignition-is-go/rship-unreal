use std::env;
use std::path::PathBuf;
use std::process;

fn main() {
    let crate_dir = match env::var("CARGO_MANIFEST_DIR") {
        Ok(value) => value,
        Err(err) => {
            eprintln!("Unable to resolve CARGO_MANIFEST_DIR: {}", err);
            process::exit(1);
        }
    };
    let out_dir = PathBuf::from(&crate_dir).join("include");

    // Create include directory if it doesn't exist
    if let Err(err) = std::fs::create_dir_all(&out_dir) {
        eprintln!("Failed to create include directory {}: {}", out_dir.display(), err);
        process::exit(1);
    }

    // Generate C header using cbindgen
    let config = match cbindgen::Config::from_file("cbindgen.toml") {
        Ok(config) => config,
        Err(err) => {
            eprintln!("Failed to load cbindgen.toml: {}", err);
            process::exit(1);
        }
    };

    let header = match cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .with_config(config)
        .generate()
    {
        Ok(header) => header,
        Err(err) => {
            eprintln!("Unable to generate C bindings: {}", err);
            process::exit(1);
        }
    };

    if let Err(err) = header.write_to_file(out_dir.join("rship_ndi_sender.h")) {
        eprintln!("Failed to write rship_ndi_sender.h: {}", err);
        process::exit(1);
    }

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=cbindgen.toml");
}
