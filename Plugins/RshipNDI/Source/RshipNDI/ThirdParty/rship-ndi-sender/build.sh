#!/bin/bash
# Build the Rust NDI sender library for Mac/Linux
# Run this once before building the UE plugin

echo "Building rship-ndi-sender..."
cargo build --release

if [ $? -eq 0 ]; then
    echo ""
    echo "SUCCESS: Library built at target/release/librship_ndi_sender.a"
    echo "You can now build the UE plugin."
else
    echo ""
    echo "FAILED: Build failed. Make sure Rust is installed: https://rustup.rs"
fi
