# Uninstalling Fincept Terminal

These scripts remove Fincept Terminal **and everything it created** for the current user, whatever way it
was installed. Run one of them; without `--yes` it first lists everything it found and asks before
removing anything.

| Platform | Run | Shipped as |
|---|---|---|
| Windows | double-click `uninstall.cmd` (or `uninstall.cmd -DryRun`) | `<install folder>\uninstall.cmd` + `uninstall.ps1` |
| macOS | `bash "/Volumes/FinceptTerminal/Uninstall Fincept Terminal.command"` | in the DMG, next to the app |
| Linux | `bash /opt/fincept-terminal/uninstall.sh` | `.deb`/`.rpm`: `/opt/fincept-terminal/uninstall.sh`; IFW `.run`: `<install>/uninstall.sh`; `cmake --install`: `<prefix>/share/fincept-terminal/uninstall.sh` |

No copy at hand (AppImage, Flatpak, a deleted install)? Run it straight from GitHub:

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/fincept-qt/packaging/uninstall/uninstall.sh)
```

```powershell
irm https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/fincept-qt/packaging/uninstall/uninstall.ps1 -OutFile $env:TEMP\fincept-uninstall.ps1
powershell -ExecutionPolicy Bypass -File $env:TEMP\fincept-uninstall.ps1
```

## Options

| `uninstall.sh` | `uninstall.ps1` / `.cmd` | Effect |
|---|---|---|
| `-n`, `--dry-run` | `-DryRun` | list what would be removed; change nothing |
| `-y`, `--yes` | `-Yes` | no confirmation prompt |
| `--keep-data` | `-KeepData` | remove the application only (reinstall later and keep everything) |
| `--data-only` | `-DataOnly` | factory reset: remove data and settings, keep the application |
| `--shared-caches` | `-SharedCaches` | also remove caches shared with other Python tools (py-yfinance, matplotlib, yt-dlp, nltk_data, `.qlib`, `.edgar`) |
| `--data-dir DIR` | `-DataDir DIR` | also remove a data folder moved with `FINCEPT_DATA_DIR` (the variable itself is picked up) |

Exit status: `0` done or nothing to do, `1` something could not be removed (it is listed), `2` cancelled.

## What is removed

**Application** — Windows: the installer (IFW) install, via its maintenance tool, plus shortcuts and the
Apps & Features entry; per-machine installs ask for one administrator approval. macOS: `FinceptTerminal.app`
(checked by bundle id `com.fincept.terminal`) and IFW installs. Linux: the `fincept-terminal` `.deb`/`.rpm`,
the `in.fincept.FinceptTerminal` Flatpak (`--delete-data`), downloaded AppImages
(`FinceptTerminal-*-linux-*.run` in Applications, Downloads, Desktop, `~/.local/bin`, `/opt`), IFW
installs, `cmake --install` prefixes, desktop entries and icons.

**Data** — the data root with every profile, databases (including the encrypted API keys), logs,
layouts, caches, crash dumps, the Python runtime, both venvs and the uv cache:

| | Data root | Settings |
|---|---|---|
| Windows | `%LOCALAPPDATA%\com.fincept.terminal` | `HKCU\Software\Fincept\FinceptTerminal` |
| macOS | `~/Library/Application Support/com.fincept.terminal` | `~/Library/Preferences/com.fincept.FinceptTerminal.plist` |
| Linux | `~/.local/share/com.fincept.terminal` | `~/.config/Fincept/FinceptTerminal.conf` |

Plus: legacy data folders from older versions, Qt caches, Credential Manager / Keychain / libsecret
items written by versions up to 4.0.2, temp files (`fincept_*`, the `fincept-<hash>` instance socket),
folders the bundled Python scripts create in the home folder (`.fincept`, `.oecd_cache`,
`.cache/fincept-terminal`, `agents.db`, `fincept-dev`), and Claude Code history of the coding-agent runs.

**Never touched** — MCP client configs (Claude Desktop, Claude Code, Codex, Gemini, Cursor, Windsurf):
the script lists the ones that still mention Fincept so you can delete those entries yourself. Screenshots
and exports you saved elsewhere are your files and are kept. Shared Python caches are kept unless asked.

## Safety

- Folders are removed only when they carry Fincept markers (data root files, the app's bundle id or exe,
  the maintenance tool); a `FINCEPT_DATA_DIR` that does not look like Fincept data is skipped.
- Home, system and shared parent folders are refused outright, and Windows uses `rmdir`, which never
  follows junctions out of the tree.
- A CMake build folder is never mistaken for an install, and only processes running from the install or
  data folders are stopped.
- Started with `sudo` on macOS/Linux, the script cleans `$SUDO_USER`'s data, not root's.

When adding a new on-disk location to the app, add it to `uninstall.sh`, `uninstall.ps1` and
`../installer/packages/com.fincept.terminal.core/meta/installscript.qs`.
