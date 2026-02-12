use std::env;
use std::path::PathBuf;

fn main() {
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let crate_path = PathBuf::from(&crate_dir);

    let include_dir = crate_path
        .parent()
        .and_then(|p| p.parent())
        .map(|p| p.join("include"))
        .expect("Unable to resolve rship-display include directory");

    std::fs::create_dir_all(&include_dir).expect("Failed to create include directory");

    let config =
        cbindgen::Config::from_file("cbindgen.toml").expect("Failed to read cbindgen.toml");

    cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .with_config(config)
        .generate()
        .expect("Failed to generate cbindgen header")
        .write_to_file(include_dir.join("rship_display.h"));

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=cbindgen.toml");
}
