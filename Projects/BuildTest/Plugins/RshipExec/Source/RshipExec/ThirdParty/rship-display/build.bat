@echo off
setlocal
set ROOT_DIR=%~dp0
cd /d %ROOT_DIR%

cargo build --release -p rship-display-ffi
if errorlevel 1 goto :error

cargo build --release -p rship-display-cli
if errorlevel 1 goto :error

echo Built rship-display FFI and CLI artifacts in %ROOT_DIR%target\release
goto :eof

:error
echo Build failed. Ensure Rust is installed: https://rustup.rs
exit /b 1
