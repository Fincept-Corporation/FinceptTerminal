#!/usr/bin/env node

/**
 * Version Sync Script
 *
 * This script ensures version consistency across all files in the project:
 * - package.json (source of truth)
 * - src/constants/version.ts
 * - src-tauri/tauri.conf.json
 * - src-tauri/Cargo.toml
 *
 * Usage:
 *   bun run sync-version
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootDir = path.resolve(__dirname, '..');

// Colors for console output
const colors = {
  reset: '\x1b[0m',
  bright: '\x1b[1m',
  green: '\x1b[32m',
  yellow: '\x1b[33m',
  blue: '\x1b[34m',
  red: '\x1b[31m',
};

function log(message, color = colors.reset) {
  console.log(`${color}${message}${colors.reset}`);
}

function logSuccess(message) {
  log(`✓ ${message}`, colors.green);
}

function logWarning(message) {
  log(`⚠ ${message}`, colors.yellow);
}

function logError(message) {
  log(`✗ ${message}`, colors.red);
}

function logInfo(message) {
  log(`ℹ ${message}`, colors.blue);
}

// Read package.json to get the version
function getPackageVersion() {
  const packageJsonPath = path.join(rootDir, 'package.json');
  try {
    const packageJson = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
    return packageJson.version;
  } catch (error) {
    logError(`Failed to read package.json: ${error.message}`);
    process.exit(1);
  }
}

// Update src/constants/version.ts
function updateVersionConstant(version) {
  const versionFilePath = path.join(rootDir, 'src', 'constants', 'version.ts');

  try {
    let content = fs.readFileSync(versionFilePath, 'utf8');

    // Replace the APP_VERSION constant
    const updatedContent = content.replace(
      /export const APP_VERSION = ['"][\d.]+['"]/,
      `export const APP_VERSION = '${version}'`
    );

    if (content === updatedContent) {
      logWarning('version.ts already has the correct version');
    } else {
      fs.writeFileSync(versionFilePath, updatedContent, 'utf8');
      logSuccess(`Updated src/constants/version.ts to v${version}`);
    }
  } catch (error) {
    logError(`Failed to update version.ts: ${error.message}`);
  }
}

// Update src-tauri/tauri.conf.json
function updateTauriConfig(version) {
  const tauriConfigPath = path.join(rootDir, 'src-tauri', 'tauri.conf.json');

  try {
    const config = JSON.parse(fs.readFileSync(tauriConfigPath, 'utf8'));

    if (config.version === version) {
      logWarning('tauri.conf.json already has the correct version');
    } else {
      config.version = version;
      fs.writeFileSync(tauriConfigPath, JSON.stringify(config, null, 2) + '\n', 'utf8');
      logSuccess(`Updated src-tauri/tauri.conf.json to v${version}`);
    }
  } catch (error) {
    logError(`Failed to update tauri.conf.json: ${error.message}`);
  }
}

// Update src-tauri/Cargo.toml
function updateCargoToml(version) {
  const cargoTomlPath = path.join(rootDir, 'src-tauri', 'Cargo.toml');

  try {
    let content = fs.readFileSync(cargoTomlPath, 'utf8');

    // Replace the version in [package] section (first occurrence)
    const lines = content.split('\n');
    let updated = false;

    for (let i = 0; i < lines.length; i++) {
      if (lines[i].startsWith('version = ') && !updated) {
        const currentVersion = lines[i].match(/version = "(.+)"/)?.[1];
        if (currentVersion !== version) {
          lines[i] = `version = "${version}"`;
          updated = true;
        }
        break;
      }
    }

    if (!updated) {
      logWarning('Cargo.toml already has the correct version');
    } else {
      fs.writeFileSync(cargoTomlPath, lines.join('\n'), 'utf8');
      logSuccess(`Updated src-tauri/Cargo.toml to v${version}`);
    }
  } catch (error) {
    logError(`Failed to update Cargo.toml: ${error.message}`);
  }
}

// Main function
function main() {
  log('\n' + '='.repeat(60), colors.bright);
  log('  VERSION SYNC SCRIPT', colors.bright);
  log('='.repeat(60) + '\n', colors.bright);

  const version = getPackageVersion();
  logInfo(`Source version from package.json: v${version}\n`);

  updateVersionConstant(version);
  updateTauriConfig(version);
  updateCargoToml(version);

  log('\n' + '='.repeat(60), colors.bright);
  logSuccess('Version sync completed!');
  log('='.repeat(60) + '\n', colors.bright);

  logInfo('Version has been synced across all files.');
  logInfo(`Current version: ${colors.bright}v${version}${colors.reset}\n`);
}

// Run the script
main();
