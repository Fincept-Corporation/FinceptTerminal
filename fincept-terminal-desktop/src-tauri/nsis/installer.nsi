; Custom NSIS installer script for Fincept Terminal
; This ensures complete cleanup on uninstall including AppData and user settings

!include MUI2.nsh

; Custom uninstaller section to remove all leftover files
Section "Uninstall"
  ; Remove application files (default Tauri behavior)
  ; This section is merged with Tauri's default uninstall

  ; Remove user data directories
  ; AppData\Local\com.fincept.terminal (Tauri app data)
  RMDir /r "$LOCALAPPDATA\com.fincept.terminal"

  ; AppData\Roaming\com.fincept.terminal (user settings, logs, etc.)
  RMDir /r "$APPDATA\com.fincept.terminal"

  ; Remove any leftover configuration files
  Delete "$APPDATA\fincept-terminal\*.*"
  RMDir "$APPDATA\fincept-terminal"

  ; Remove temp files if any
  Delete "$TEMP\fincept-terminal-*.*"

  ; Clean up any leftover registry entries
  DeleteRegKey HKCU "Software\Fincept Corporation\Fincept Terminal"
  DeleteRegKey HKCU "Software\com.fincept.terminal"

  ; Remove start menu shortcuts
  Delete "$SMPROGRAMS\Fincept Terminal\*.*"
  RMDir "$SMPROGRAMS\Fincept Terminal"

  ; Remove desktop shortcut if exists
  Delete "$DESKTOP\Fincept Terminal.lnk"

  ; Remove from installed programs
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\{com.fincept.terminal}"

SectionEnd

; Custom installer section (optional)
Section "Install"
  ; Default Tauri install behavior will handle the main installation
SectionEnd
