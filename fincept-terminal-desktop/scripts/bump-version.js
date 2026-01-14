#!/usr/bin/env node

/**
 * Version Bump Script
 *
 * This script increments the version number in package.json and syncs it across all files.
 *
 * Usage:
 *   bun run bump-version [patch|minor|major]
 *
 * Examples:
 *   bun run bump-version patch  -> 3.1.5 -> 3.1.6
 *   bun run bump-version minor  -> 3.1.5 -> 3.2.0
 *   bun run bump-version major  -> 3.1.5 -> 4.0.0
 *
 * Default: patch
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { execSync } from 'child_process';

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
  cyan: '\x1b[36m',
};

function log(message, color = colors.reset) {
  console.log(`${color}${message}${colors.reset}`);
}

function logSuccess(message) {
  log(`✓ ${message}`, colors.green);
}

function logError(message) {
  log(`✗ ${message}`, colors.red);
}

function logInfo(message) {
  log(`ℹ ${message}`, colors.blue);
}

// Parse semver version
function parseVersion(version) {
  const match = version.match(/^(\d+)\.(\d+)\.(\d+)$/);
  if (!match) {
    throw new Error(`Invalid version format: ${version}`);
  }
  return {
    major: parseInt(match[1], 10),
    minor: parseInt(match[2], 10),
    patch: parseInt(match[3], 10),
  };
}

// Increment version based on type
function incrementVersion(version, type = 'patch') {
  const parts = parseVersion(version);

  switch (type) {
    case 'major':
      parts.major += 1;
      parts.minor = 0;
      parts.patch = 0;
      break;
    case 'minor':
      parts.minor += 1;
      parts.patch = 0;
      break;
    case 'patch':
    default:
      parts.patch += 1;
      break;
  }

  return `${parts.major}.${parts.minor}.${parts.patch}`;
}

// Update package.json
function updatePackageJson(newVersion) {
  const packageJsonPath = path.join(rootDir, 'package.json');

  try {
    const packageJson = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
    const oldVersion = packageJson.version;

    packageJson.version = newVersion;
    fs.writeFileSync(packageJsonPath, JSON.stringify(packageJson, null, 2) + '\n', 'utf8');

    logSuccess(`Updated package.json: v${oldVersion} → v${newVersion}`);
    return oldVersion;
  } catch (error) {
    logError(`Failed to update package.json: ${error.message}`);
    process.exit(1);
  }
}

// Run sync-version script
function runSyncVersion() {
  try {
    logInfo('\nSyncing version across all files...\n');
    execSync('bun run sync-version', { stdio: 'inherit', cwd: rootDir });
  } catch (error) {
    logError('Failed to run sync-version script');
    process.exit(1);
  }
}

// Main function
function main() {
  const args = process.argv.slice(2);
  const bumpType = args[0] || 'patch';

  if (!['patch', 'minor', 'major'].includes(bumpType)) {
    logError(`Invalid bump type: ${bumpType}`);
    logInfo('Usage: bun run bump-version [patch|minor|major]');
    process.exit(1);
  }

  log('\n' + '='.repeat(60), colors.bright);
  log('  VERSION BUMP SCRIPT', colors.bright);
  log('='.repeat(60) + '\n', colors.bright);

  // Read current version
  const packageJsonPath = path.join(rootDir, 'package.json');
  const packageJson = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
  const currentVersion = packageJson.version;

  // Calculate new version
  const newVersion = incrementVersion(currentVersion, bumpType);

  logInfo(`Bump type: ${colors.bright}${bumpType}${colors.reset}`);
  logInfo(`Current version: ${colors.yellow}v${currentVersion}${colors.reset}`);
  logInfo(`New version: ${colors.green}v${newVersion}${colors.reset}\n`);

  // Update package.json
  updatePackageJson(newVersion);

  // Run sync-version to update all other files
  runSyncVersion();

  log('\n' + '='.repeat(60), colors.bright);
  logSuccess('Version bump completed!');
  log('='.repeat(60) + '\n', colors.bright);

  log(`${colors.cyan}Next steps:${colors.reset}`);
  log('  1. Review the changes: git diff');
  log('  2. Commit the changes: git add . && git commit -m "chore: bump version to v' + newVersion + '"');
  log('  3. Tag the release: git tag v' + newVersion);
  log('  4. Push changes: git push && git push --tags\n');
}

// Run the script
main();
