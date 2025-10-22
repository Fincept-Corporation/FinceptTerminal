# Fix Icon Display Issue

## Problem
After building, the installer/executable shows a generic CD icon instead of your custom icon.

## Root Causes
1. **Windows Icon Cache** - Windows caches icons aggressively
2. **ICO File Format** - The `.ico` file must contain multiple embedded resolutions
3. **Build Cache** - Tauri may cache old icons

## Solution

### Step 1: Regenerate icon.ico with all sizes

You need to regenerate `icon.ico` to ensure it contains all required sizes:
- 16x16
- 32x32
- 48x48
- 64x64
- 128x128
- 256x256

**Option A: Use Tauri CLI (Recommended)**
```bash
npm install -g @tauri-apps/cli

# This will regenerate all icon sizes from icon.png
npx @tauri-apps/cli icon src-tauri/icons/icon.png
```

**Option B: Use Online Tool**
1. Go to https://www.icoconverter.com/ or https://convertico.com/
2. Upload your `src-tauri/icons/icon.png`
3. Select all sizes: 16, 32, 48, 64, 128, 256
4. Download the generated `icon.ico`
5. Replace `src-tauri/icons/icon.ico`

**Option C: Use ImageMagick (if installed)**
```bash
magick convert icon.png -define icon:auto-resize=256,128,64,48,32,16 icon.ico
```

### Step 2: Clear Build Cache

```bash
# Clean Tauri build cache
npm run tauri build -- --clean

# Or manually delete the target folder
rmdir /s /q src-tauri\target
```

### Step 3: Clear Windows Icon Cache

**Method 1: Using Task Manager**
1. Open Task Manager (Ctrl+Shift+Esc)
2. Find "Windows Explorer" process
3. Right-click → "Restart"

**Method 2: Using Command Prompt (Administrator)**
```cmd
cd /d %userprofile%\AppData\Local
attrib -h IconCache.db
del IconCache.db
start explorer
```

**Method 3: Delete Icon Cache Files (Most thorough)**
```powershell
# Run PowerShell as Administrator
taskkill /f /im explorer.exe
cd $env:LOCALAPPDATA
del IconCache.db /a
cd Microsoft\Windows\Explorer
del *.db /a
start explorer
```

### Step 4: Rebuild the Application

```bash
# Full clean rebuild
npm run build
npm run tauri build
```

### Step 5: Test the New Installer

1. Find the installer in `src-tauri/target/release/bundle/nsis/` or `msi/`
2. **Right-click the installer file → Properties → Check if icon shows correctly**
3. Install on a clean/different machine if possible (icons cache on development machines)
4. After install, check:
   - Desktop shortcut icon
   - Start menu icon
   - Installed app icon in Programs & Features

## Verification Checklist

- [ ] `icon.ico` contains multiple sizes (check file size > 30KB)
- [ ] Cleared Windows icon cache
- [ ] Deleted `src-tauri/target` folder
- [ ] Rebuilt with `npm run tauri build`
- [ ] Checked installer file icon (right-click → Properties)
- [ ] Tested installer on different machine if available

## Additional Tips

### If Icon Still Doesn't Show:

1. **Check icon.ico file size**
   - Should be > 30KB (contains multiple sizes)
   - If < 10KB, it likely only has one size

2. **Verify icon is embedded in .exe**
   ```bash
   # Check if icon is in the built executable
   cd src-tauri/target/release
   # Icon should appear in file properties
   ```

3. **Test on Clean Windows Installation**
   - Use Windows Sandbox or VM
   - This eliminates cache issues completely

4. **Double-check paths in tauri.conf.json**
   ```json
   "icon": [
     "icons/32x32.png",
     "icons/64x64.png",
     "icons/128x128.png",
     "icons/128x128@2x.png",
     "icons/icon.icns",
     "icons/icon.ico"
   ]
   ```

5. **Ensure icon.ico is properly formatted**
   - Must be true `.ico` format, not renamed `.png`
   - Must contain multiple embedded bitmaps

## Quick Test Command

```bash
# One-liner to clean and rebuild
npm run build && rmdir /s /q src-tauri\\target && npm run tauri build
```

## Common Mistakes to Avoid

❌ Don't just rename `icon.png` to `icon.ico` - it won't work
❌ Don't skip clearing the Windows icon cache
❌ Don't test on the same machine without clearing cache
✅ Do regenerate icon.ico with all sizes embedded
✅ Do test on a clean machine or VM
✅ Do clear build cache before rebuilding
