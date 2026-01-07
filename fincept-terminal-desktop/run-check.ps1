$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY = "1"
Set-Location "C:\Users\avira\FinceptTerminal\fincept-terminal-desktop\src-tauri"
cargo check 2>&1 | Tee-Object -FilePath "build-output.txt"
