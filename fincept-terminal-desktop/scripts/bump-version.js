#!/usr/bin/env node

/**
 * Version Bump Script for Fincept Terminal
 *
 * Usage:
 *   node scripts/bump-version.js <new-version>
 *   npm run bump-version <new-version>
 *
 * Example:
 *   node scripts/bump-version.js 3.0.11
 *
 * This script updates version numbers across all configuration files:
 * - package.json
 * - src-tauri/tauri.conf.json
 * - src-tauri/Cargo.toml
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

// Get __dirname equivalent in ES modules
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Get the new version from command line arguments
const newVersion = process.argv[2];

if (!newVersion) {
  console.error('❌ Error: No version specified');
  console.log('Usage: node scripts/bump-version.js <version>');
  console.log('Example: node scripts/bump-version.js 3.0.11');
  process.exit(1);
}

// Validate version format (semantic versioning: x.y.z)
const versionRegex = /^\d+\.\d+\.\d+$/;
if (!versionRegex.test(newVersion)) {
  console.error('❌ Error: Invalid version format');
  console.log('Version must follow semantic versioning format: x.y.z');
  console.log('Example: 3.0.11');
  process.exit(1);
}

console.log(`\n🚀 Bumping version to ${newVersion}...\n`);

// File paths
const rootDir = path.join(__dirname, '..');
const packageJsonPath = path.join(rootDir, 'package.json');
const tauriConfPath = path.join(rootDir, 'src-tauri', 'tauri.conf.json');
const cargoTomlPath = path.join(rootDir, 'src-tauri', 'Cargo.toml');

let successCount = 0;
let failCount = 0;

// Update package.json
try {
  console.log('📦 Updating package.json...');
  const packageJson = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
  const oldVersion = packageJson.version;
  packageJson.version = newVersion;
  fs.writeFileSync(packageJsonPath, JSON.stringify(packageJson, null, 2) + '\n');
  console.log(`   ✅ Updated from ${oldVersion} to ${newVersion}`);
  successCount++;
} catch (error) {
  console.error(`   ❌ Failed to update package.json: ${error.message}`);
  failCount++;
}

// Update tauri.conf.json
try {
  console.log('⚙️  Updating src-tauri/tauri.conf.json...');
  const tauriConf = JSON.parse(fs.readFileSync(tauriConfPath, 'utf8'));
  const oldVersion = tauriConf.version;
  tauriConf.version = newVersion;
  fs.writeFileSync(tauriConfPath, JSON.stringify(tauriConf, null, 2) + '\n');
  console.log(`   ✅ Updated from ${oldVersion} to ${newVersion}`);
  successCount++;
} catch (error) {
  console.error(`   ❌ Failed to update tauri.conf.json: ${error.message}`);
  failCount++;
}

// Update Cargo.toml
try {
  console.log('🦀 Updating src-tauri/Cargo.toml...');
  let cargoToml = fs.readFileSync(cargoTomlPath, 'utf8');
  const versionMatch = cargoToml.match(/^version = "([^"]+)"/m);

  if (versionMatch) {
    const oldVersion = versionMatch[1];
    cargoToml = cargoToml.replace(
      /^version = "[^"]+"/m,
      `version = "${newVersion}"`
    );
    fs.writeFileSync(cargoTomlPath, cargoToml);
    console.log(`   ✅ Updated from ${oldVersion} to ${newVersion}`);
    successCount++;
  } else {
    throw new Error('Version line not found in Cargo.toml');
  }
} catch (error) {
  console.error(`   ❌ Failed to update Cargo.toml: ${error.message}`);
  failCount++;
}

// Summary
console.log('\n' + '='.repeat(50));
if (failCount === 0) {
  console.log(`✨ Version bump complete! ${successCount}/3 files updated successfully.`);
  console.log(`\n📋 Next steps:`);
  console.log(`   1. Review the changes: git diff`);
  console.log(`   2. Commit the changes: git add . && git commit -m "chore: bump version to ${newVersion}"`);
  console.log(`   3. Create a tag: git tag v${newVersion}`);
  console.log(`   4. Push: git push && git push --tags`);
  console.log(`   5. GitHub Actions will automatically build and create a release`);
} else {
  console.error(`⚠️  Version bump completed with errors: ${successCount} succeeded, ${failCount} failed`);
  console.error('Please fix the errors and try again.');
  process.exit(1);
}
