@echo off
set PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1
cd src-tauri
cargo check --message-format=short 2>&1 | findstr /V "cargo:rustc"
pause
