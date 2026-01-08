#!/usr/bin/env node
// Translation audit script - finds missing and unused translation keys

import fs from 'fs';
import path from 'path';
import { glob } from 'glob';

const LOCALES_DIR = 'public/locales';
const SRC_DIR = 'src';
const LANGUAGES = ['en', 'es', 'fr', 'de', 'zh', 'ja', 'hi', 'ar', 'pt', 'ru', 'bn', 'ko', 'tr', 'it', 'id', 'vi', 'th', 'pl', 'uk', 'ro'];
const NAMESPACES = [
  'common', 'auth', 'dashboard', 'settings', 'tabs', 'chat', 'economics',
  'equityResearch', 'forum', 'geopolitics', 'loginAudit', 'maritime', 'markets',
  'news', 'portfolio', 'profile', 'screener', 'trading', 'watchlist', 'dataMapping',
  'dataSources', 'docs', 'notes', 'support', 'nodeEditor', 'reportBuilder',
  'alphaArena', 'codeEditor', 'agentConfig', 'dbnomics', 'excel', 'marketplace', 'backtesting'
];

// Extract all translation keys from code
function extractKeysFromCode() {
  const files = glob.sync(`${SRC_DIR}/**/*.{ts,tsx}`, { ignore: '**/node_modules/**' });
  const usedKeys = new Map(); // namespace -> Set of keys

  // Regex patterns to match t() calls
  const patterns = [
    /t\(['"]([^:'"]+):([^'"]+)['"]\)/g,  // t('namespace:key')
    /t\(['"]([^'"]+)['"]\)/g,              // t('key') - uses default namespace
  ];

  files.forEach(file => {
    const content = fs.readFileSync(file, 'utf-8');

    // Pattern 1: t('namespace:key')
    let match;
    const pattern1 = /t\(['"]([^:'"]+):([^'"]+)['"]/g;
    while ((match = pattern1.exec(content)) !== null) {
      const namespace = match[1];
      const key = match[2];
      if (!usedKeys.has(namespace)) {
        usedKeys.set(namespace, new Set());
      }
      usedKeys.get(namespace).add(key);
    }

    // Pattern 2: t('key') - common namespace
    const pattern2 = /t\(['"]([^:'"]+)['"]/g;
    while ((match = pattern2.exec(content)) !== null) {
      const key = match[1];
      if (!key.includes(':')) { // Make sure it's not already caught
        if (!usedKeys.has('common')) {
          usedKeys.set('common', new Set());
        }
        usedKeys.get('common').add(key);
      }
    }
  });

  return usedKeys;
}

// Get all keys from translation files
function getTranslationKeys(lang = 'en') {
  const keys = new Map(); // namespace -> Set of keys

  NAMESPACES.forEach(namespace => {
    const filePath = path.join(LOCALES_DIR, lang, `${namespace}.json`);
    if (fs.existsSync(filePath)) {
      const content = JSON.parse(fs.readFileSync(filePath, 'utf-8'));
      keys.set(namespace, new Set(extractAllKeys(content)));
    }
  });

  return keys;
}

// Recursively extract all keys from nested JSON
function extractAllKeys(obj, prefix = '') {
  let keys = [];

  for (const [key, value] of Object.entries(obj)) {
    const fullKey = prefix ? `${prefix}.${key}` : key;

    if (typeof value === 'object' && value !== null && !Array.isArray(value)) {
      keys = keys.concat(extractAllKeys(value, fullKey));
    } else {
      keys.push(fullKey);
    }
  }

  return keys;
}

// Find hardcoded strings (simple heuristic)
function findHardcodedStrings() {
  const files = glob.sync(`${SRC_DIR}/**/*.{ts,tsx}`, { ignore: '**/node_modules/**' });
  const suspiciousStrings = [];

  // Look for JSX text that might need translation
  const jsxTextPattern = />\s*([A-Z][A-Za-z\s]{3,30})\s*</g;

  files.forEach(file => {
    const content = fs.readFileSync(file, 'utf-8');
    let match;

    while ((match = jsxTextPattern.exec(content)) !== null) {
      const text = match[1].trim();
      // Skip common words and check if it's likely a label
      if (text.split(' ').length <= 5 && !text.includes('const') && !text.includes('function')) {
        suspiciousStrings.push({ file: path.relative(process.cwd(), file), text });
      }
    }
  });

  return suspiciousStrings.slice(0, 50); // Limit to first 50
}

// Main audit function
function audit() {
  console.log('🔍 Starting translation audit...\n');

  const usedKeys = extractKeysFromCode();
  const translationKeys = getTranslationKeys('en');

  console.log('📊 STATISTICS');
  console.log('='.repeat(60));
  console.log(`Languages: ${LANGUAGES.length}`);
  console.log(`Namespaces: ${NAMESPACES.length}`);

  let totalUsed = 0;
  let totalAvailable = 0;
  usedKeys.forEach(keys => totalUsed += keys.size);
  translationKeys.forEach(keys => totalAvailable += keys.size);

  console.log(`Translation keys in code: ${totalUsed}`);
  console.log(`Translation keys in files: ${totalAvailable}\n`);

  // Find missing keys (used in code but not in translation files)
  console.log('❌ MISSING TRANSLATIONS (used in code, not in translation files)');
  console.log('='.repeat(60));
  let missingCount = 0;

  usedKeys.forEach((keys, namespace) => {
    const translatedKeys = translationKeys.get(namespace) || new Set();
    const missing = [...keys].filter(key => !translatedKeys.has(key));

    if (missing.length > 0) {
      console.log(`\n[${namespace}] - ${missing.length} missing keys:`);
      missing.forEach(key => console.log(`  - ${key}`));
      missingCount += missing.length;
    }
  });

  if (missingCount === 0) {
    console.log('✅ No missing keys found!\n');
  } else {
    console.log(`\n⚠️  Total missing: ${missingCount} keys\n`);
  }

  // Find unused keys (in translation files but not used in code)
  console.log('🗑️  UNUSED TRANSLATIONS (in files, not used in code)');
  console.log('='.repeat(60));
  let unusedCount = 0;

  translationKeys.forEach((keys, namespace) => {
    const usedInCode = usedKeys.get(namespace) || new Set();
    const unused = [...keys].filter(key => !usedInCode.has(key));

    if (unused.length > 0) {
      console.log(`\n[${namespace}] - ${unused.length} unused keys:`);
      unused.slice(0, 10).forEach(key => console.log(`  - ${key}`));
      if (unused.length > 10) {
        console.log(`  ... and ${unused.length - 10} more`);
      }
      unusedCount += unused.length;
    }
  });

  if (unusedCount === 0) {
    console.log('✅ No unused keys found!\n');
  } else {
    console.log(`\n📝 Total unused: ${unusedCount} keys (consider removing or they may be dynamic)\n`);
  }

  // Find potentially hardcoded strings
  console.log('🔤 POTENTIALLY HARDCODED STRINGS (sample)');
  console.log('='.repeat(60));
  const hardcoded = findHardcodedStrings();

  if (hardcoded.length > 0) {
    const grouped = {};
    hardcoded.forEach(({ file, text }) => {
      if (!grouped[file]) grouped[file] = [];
      grouped[file].push(text);
    });

    Object.entries(grouped).slice(0, 10).forEach(([file, texts]) => {
      console.log(`\n${file}:`);
      texts.forEach(text => console.log(`  - "${text}"`));
    });
    console.log('\n⚠️  Review these strings to see if they need translation');
  } else {
    console.log('✅ No obvious hardcoded strings found!\n');
  }

  // Summary
  console.log('\n' + '='.repeat(60));
  console.log('📋 SUMMARY');
  console.log('='.repeat(60));
  console.log(`✅ Translation coverage: ${totalAvailable} keys across ${NAMESPACES.length} namespaces`);
  console.log(`🔍 Keys used in code: ${totalUsed}`);
  console.log(`❌ Missing translations: ${missingCount}`);
  console.log(`🗑️  Unused translations: ${unusedCount}`);
  console.log(`🔤 Potential hardcoded strings found: ${hardcoded.length}`);
  console.log('\n✨ Audit complete!\n');
}

// Run audit
audit();
