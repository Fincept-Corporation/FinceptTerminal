#!/usr/bin/env node
// Merge extracted hardcoded strings into existing translation files
// This creates a comprehensive translation file with all strings

import fs from 'fs';
import path from 'path';

const LOCALES_DIR = 'public/locales/en';
const HARDCODED_FILE = 'hardcoded-strings.json';

// Read existing translations
function readExistingTranslations(namespace) {
  const filePath = path.join(LOCALES_DIR, `${namespace}.json`);
  if (fs.existsSync(filePath)) {
    const content = fs.readFileSync(filePath, 'utf-8').replace(/^\uFEFF/, '');
    try {
      return JSON.parse(content);
    } catch (e) {
      console.log(`  ⚠️ Could not parse ${namespace}.json, starting fresh`);
      return {};
    }
  }
  return {};
}

// Flatten nested object to dot notation
function flattenObject(obj, prefix = '') {
  const result = {};
  for (const [key, value] of Object.entries(obj)) {
    const newKey = prefix ? `${prefix}.${key}` : key;
    if (typeof value === 'object' && value !== null && !Array.isArray(value)) {
      Object.assign(result, flattenObject(value, newKey));
    } else {
      result[newKey] = value;
    }
  }
  return result;
}

// Unflatten dot notation to nested object
function unflattenObject(obj) {
  const result = {};
  for (const [key, value] of Object.entries(obj)) {
    const keys = key.split('.');
    let current = result;
    for (let i = 0; i < keys.length - 1; i++) {
      if (!(keys[i] in current)) {
        current[keys[i]] = {};
      }
      current = current[keys[i]];
    }
    current[keys[keys.length - 1]] = value;
  }
  return result;
}

// Clean key name
function cleanKey(key) {
  return key
    .replace(/[^a-zA-Z0-9]/g, '')
    .replace(/^(\d)/, '_$1'); // Don't start with number
}

// Filter out non-UI strings
function isUIString(text) {
  if (!text || typeof text !== 'string') return false;
  if (text.length < 2 || text.length > 500) return false;

  // Skip patterns that are clearly not UI text
  const skipPatterns = [
    /^\d+(\.\d+)?$/, // Numbers only
    /^[a-z_]+$/, // lowercase_snake_case (likely code)
    /^https?:\/\//, // URLs
    /^[a-z]+\.[a-z]+/, // file.ext
    /^\{.*\}$/, // JSON-like
    /^[<>\/]/, // HTML
    /^import\s/, // imports
    /^export\s/, // exports
    /Promise/, // Promise type
    /^\s*$/, // whitespace only
    /^[A-Z][a-z]+[A-Z]/, // camelCase
  ];

  return !skipPatterns.some(p => p.test(text));
}

async function main() {
  console.log('📝 Merging hardcoded strings into translation files...\n');

  if (!fs.existsSync(HARDCODED_FILE)) {
    console.log('❌ hardcoded-strings.json not found. Run extract-hardcoded.js first.');
    process.exit(1);
  }

  const hardcoded = JSON.parse(fs.readFileSync(HARDCODED_FILE, 'utf-8'));
  let totalAdded = 0;
  let totalSkipped = 0;

  for (const [namespace, strings] of Object.entries(hardcoded)) {
    console.log(`\n[${namespace}]`);

    // Read existing translations
    const existing = readExistingTranslations(namespace);
    const existingFlat = flattenObject(existing);
    const existingValues = new Set(Object.values(existingFlat));

    let added = 0;
    let skipped = 0;

    // Create new section for extracted strings
    if (!existing.extracted) {
      existing.extracted = {};
    }

    for (const [key, value] of Object.entries(strings)) {
      // Skip if value already exists
      if (existingValues.has(value)) {
        skipped++;
        continue;
      }

      // Skip non-UI strings
      if (!isUIString(value)) {
        skipped++;
        continue;
      }

      // Add to extracted section
      const cleanedKey = cleanKey(key);
      if (!existing.extracted[cleanedKey]) {
        existing.extracted[cleanedKey] = value;
        added++;
      }
    }

    // Save updated translations
    const outputPath = path.join(LOCALES_DIR, `${namespace}.json`);
    fs.writeFileSync(outputPath, JSON.stringify(existing, null, 2) + '\n', 'utf-8');

    console.log(`  ✅ Added: ${added}, Skipped: ${skipped}`);
    totalAdded += added;
    totalSkipped += skipped;
  }

  console.log('\n' + '='.repeat(60));
  console.log('📋 SUMMARY');
  console.log('='.repeat(60));
  console.log(`Total strings added: ${totalAdded}`);
  console.log(`Total strings skipped: ${totalSkipped}`);
  console.log('\n✅ Translation files updated!');
  console.log('\n⚠️  NEXT STEPS:');
  console.log('1. Review the "extracted" sections in each translation file');
  console.log('2. Reorganize keys as needed');
  console.log('3. Update component files to use t() for these strings');
  console.log('4. Run: bun run i18n:sync to propagate to all languages');
}

main().catch(console.error);
