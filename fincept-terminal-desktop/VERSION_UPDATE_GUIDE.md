# Version Update Guide

## Current Version: 3.0.0

## How to Update Version

When releasing a new version, update the following files:

### 1. Configuration Files (Required for Build)

Update these 3 files with the new version number:

1. **`src-tauri/tauri.conf.json`** (line 4)
   ```json
   "version": "3.0.0"
   ```

2. **`src-tauri/Cargo.toml`** (line 3)
   ```toml
   version = "3.0.0"
   ```

3. **`package.json`** (line 4)
   ```json
   "version": "3.0.0"
   ```

### 2. UI Display Constant (Required for In-App Display)

4. **`src/constants/version.ts`** (line 5)
   ```typescript
   export const APP_VERSION = '3.0.0';
   ```

## Version Display Locations

The version is automatically displayed in:

1. **Command Bar** - Shows `v3.0.0` in green next to the timestamp
2. **Help Menu** - "About Finxept v3.0.0" menu item
3. **Installer Filename** - `Fincept-Terminal_3.0.0_x64-setup.exe`
4. **Windows Program Properties** - Version info in installed apps

## Version Numbering Convention

Follow **Semantic Versioning** (SemVer):

- **Major.Minor.Patch** format (e.g., 3.0.0)
- **Major**: Breaking changes or major features (3.0.0 → 4.0.0)
- **Minor**: New features, backwards compatible (3.0.0 → 3.1.0)
- **Patch**: Bug fixes, minor improvements (3.0.0 → 3.0.1)

### Examples:
- `3.0.0` → `3.0.1` - Bug fix release
- `3.0.0` → `3.1.0` - New feature added
- `3.0.0` → `4.0.0` - Major redesign or breaking changes

## Microsoft Store Versioning

For Microsoft Store submissions:
- Use 4-part versioning: `Major.Minor.Build.Revision`
- Example: `3.0.0.0` or `3.0.1.5`
- Each submission must have a higher version than the previous
- Store may take 24-48 hours for review

## Build Commands

After updating version:

```bash
# Development build
npm run tauri dev

# Production build (creates installers with version in filename)
npm run tauri build
```

## Version Update Checklist

- [ ] Update `src-tauri/tauri.conf.json`
- [ ] Update `src-tauri/Cargo.toml`
- [ ] Update `package.json`
- [ ] Update `src/constants/version.ts`
- [ ] Test development build: `npm run tauri dev`
- [ ] Build production installers: `npm run tauri build`
- [ ] Test installers on clean machine
- [ ] Verify version displays correctly in app
- [ ] Create git tag: `git tag v3.0.0`
- [ ] Push tag: `git push origin v3.0.0`
- [ ] Create GitHub release with installers

## Quick Version Bump Script (Optional)

You can create a script to update all files at once:

```bash
# Example: bump-version.sh
NEW_VERSION="3.1.0"

# Update tauri.conf.json
sed -i 's/"version": "[^"]*"/"version": "'$NEW_VERSION'"/' src-tauri/tauri.conf.json

# Update Cargo.toml
sed -i 's/version = "[^"]*"/version = "'$NEW_VERSION'"/' src-tauri/Cargo.toml

# Update package.json
sed -i 's/"version": "[^"]*"/"version": "'$NEW_VERSION'"/' package.json

# Update version.ts
sed -i "s/APP_VERSION = '[^']*'/APP_VERSION = '$NEW_VERSION'/" src/constants/version.ts

echo "✅ Version updated to $NEW_VERSION"
```

## Notes

- The **`tauri.conf.json`** version is the primary source for installers
- The **`src/constants/version.ts`** file is used for UI display only
- Keep all versions in sync to avoid confusion
- Always test after version updates
- Version is embedded in installer filename automatically
