@echo off
REM Build the Rust NDI sender library for Windows
REM Run this once before building the UE plugin

echo Building rship-ndi-sender...
cargo build --release

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS: Library built at target\release\rship_ndi_sender.lib
    echo You can now build the UE plugin.
) else (
    echo.
    echo FAILED: Build failed. Make sure Rust is installed: https://rustup.rs
)
