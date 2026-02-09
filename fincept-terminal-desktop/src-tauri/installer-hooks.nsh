; Custom NSIS installer hooks for Fincept Terminal
; See: https://v2.tauri.app/distribute/windows-installer/

!macro NSIS_HOOK_PREINSTALL
  ; Runs before copying files, setting registry keys, and creating shortcuts
!macroend

!macro NSIS_HOOK_POSTINSTALL
  ; Runs after all files are copied, registry keys set, and shortcuts created
!macroend

!macro NSIS_HOOK_PREUNINSTALL
  ; Runs before removing files, registry keys, and shortcuts
!macroend

!macro NSIS_HOOK_POSTUNINSTALL
  ; Runs after files, registry keys, and shortcuts have been removed
!macroend
