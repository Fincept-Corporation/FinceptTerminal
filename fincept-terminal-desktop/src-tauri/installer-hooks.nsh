; Fincept Terminal - NSIS Installer Hooks
; Custom cleanup during uninstallation

!macro NSIS_HOOK_PREINSTALL
  ; Hook executed before installation starts
  ; Add any pre-installation logic here if needed
!macroend

!macro NSIS_HOOK_POSTINSTALL
  ; Hook executed after installation completes
  ; Add any post-installation logic here if needed
!macroend

!macro NSIS_HOOK_PREUNINSTALL
  ; Hook executed before uninstallation starts
  ; This is where we clean up application data

  ; Remove AppData directory (user-specific data)
  RMDir /r "$APPDATA\com.fincept.terminal"

  ; Remove LocalAppData directory (cache, logs, databases)
  RMDir /r "$LOCALAPPDATA\com.fincept.terminal"

  ; Remove any configuration files
  Delete "$APPDATA\fincept-terminal.config"

  ; Clean up any temp files
  RMDir /r "$TEMP\fincept-terminal"

!macroend

!macro NSIS_HOOK_POSTUNINSTALL
  ; Hook executed after uninstallation completes
  ; Final cleanup after files are removed

  ; Remove any leftover registry keys (if any were created)
  DeleteRegKey HKCU "Software\Fincept Corporation\FinceptTerminal"

  ; Remove from Add/Remove Programs if any artifacts remain
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FinceptTerminal"

  ; Show cleanup confirmation message
  MessageBox MB_OK "Fincept Terminal has been completely removed from your system.$\n$\nAll application data, settings, and cache files have been deleted."

!macroend
