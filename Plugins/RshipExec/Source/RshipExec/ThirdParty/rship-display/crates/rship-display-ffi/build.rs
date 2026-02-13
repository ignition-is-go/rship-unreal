use std::env;
use std::process;
use std::path::PathBuf;

fn main() {
    let crate_dir = match env::var("CARGO_MANIFEST_DIR") {
        Ok(value) => value,
        Err(err) => {
            eprintln!("Unable to resolve CARGO_MANIFEST_DIR: {}", err);
            process::exit(1);
        }
    };
    let crate_path = PathBuf::from(&crate_dir);

    let include_dir = match crate_path
        .parent()
        .and_then(|p| p.parent())
        .map(|p| p.join("include"))
    {
        Some(path) => path,
        None => {
            eprintln!("Unable to resolve rship-display include directory from {}", crate_dir);
            process::exit(1);
        }
    };

    if let Err(err) = std::fs::create_dir_all(&include_dir) {
        eprintln!("Failed to create include directory {}: {}", include_dir.display(), err);
        process::exit(1);
    }

    let config = match cbindgen::Config::from_file("cbindgen.toml") {
        Ok(config) => config,
        Err(err) => {
            eprintln!("Failed to read cbindgen.toml: {}", err);
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
            eprintln!("Failed to generate cbindgen header: {}", err);
            process::exit(1);
        }
    };

    if let Err(err) = header.write_to_file(include_dir.join("rship_display.h")) {
        eprintln!("Failed to write rship_display.h: {}", err);
        process::exit(1);
    }

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=cbindgen.toml");
}
