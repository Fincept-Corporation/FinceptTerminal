# Setup Scripts

## Python Setup

### Windows
Run from project root:
```powershell
.\scripts\setup-python-windows.ps1
```

Or with Git Bash:
```bash
bash scripts/setup-python-windows.sh
```

### What it does:
1. Downloads Python 3.12.8 embeddable
2. Extracts to `src-tauri/resources/python-windows/`
3. Installs pip
4. Installs dependencies from `requirements.txt` (locked versions)
5. Tests installation

### Why we need this:
- Bundled Python is **500MB+** - too large for git
- Prevents version conflicts from manual pip installs
- Ensures reproducible builds across machines
- Locked dependencies (`==` not `>=`) prevent breaking changes

### First time setup:
```bash
cd fincept-terminal-desktop
bash scripts/setup-python-windows.sh
```

### If you get dependency errors:
1. Delete `src-tauri/resources/python-windows/`
2. Re-run setup script
3. **Never** run `pip install` inside bundled Python directly
