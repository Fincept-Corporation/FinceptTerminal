#!/usr/bin/env node
// Auto-sync translations across all languages
// This script:
// 1. Scans code for t() calls to find actual UI translation keys
// 2. Compares with English (source) translation files
// 3. Syncs missing keys to all other languages
// 4. Reports what was added/removed

import fs from 'fs';
import path from 'path';
import { glob } from 'glob';

const LOCALES_DIR = 'public/locales';
const SRC_DIR = 'src';
const SOURCE_LANG = 'en';
const LANGUAGES = ['en', 'es', 'fr', 'de', 'zh', 'ja', 'hi', 'ar', 'pt', 'ru', 'bn', 'ko', 'tr', 'it', 'id', 'vi', 'th', 'pl', 'uk', 'ro'];

// Extract translation keys from code
function extractTranslationKeysFromCode() {
  const files = glob.sync(`${SRC_DIR}/**/*.{ts,tsx}`, { ignore: '**/node_modules/**' });
  const keysMap = new Map(); // namespace -> Set of keys

  // Patterns to match t() calls
  const nsPattern = /useTranslation\(['"]([\w]+)['"]\)/g;
  const keyPattern1 = /\bt\(['"]([\w.]+)['"]\)/g; // t('key.subkey')
  const keyPattern2 = /\bt\(['"]([\w]+):([\w.]+)['"]\)/g; // t('namespace:key')

  files.forEach(file => {
    const content = fs.readFileSync(file, 'utf-8');

    // Find default namespace from useTranslation('namespace')
    let defaultNs = 'common';
    let nsMatch;
    while ((nsMatch = nsPattern.exec(content)) !== null) {
      defaultNs = nsMatch[1];
    }

    // Extract keys with namespace prefix: t('namespace:key')
    let match;
    const pattern2Regex = /\bt\(['"]([\w]+):([\w.]+)['"]\)/g;
    while ((match = pattern2Regex.exec(content)) !== null) {
      const namespace = match[1];
      const key = match[2];

      if (!keysMap.has(namespace)) {
        keysMap.set(namespace, new Set());
      }
      keysMap.get(namespace).add(key);
    }

    // Extract simple keys: t('key') - use default namespace
    const pattern1Regex = /\bt\(['"]([\w.]+)['"]\)/g;
    while ((match = pattern1Regex.exec(content)) !== null) {
      const fullKey = match[1];

      // Skip if it already has namespace (caught above)
      if (!fullKey.includes(':')) {
        if (!keysMap.has(defaultNs)) {
          keysMap.set(defaultNs, new Set());
        }
        keysMap.get(defaultNs).add(fullKey);
      }
    }
  });

  return keysMap;
}

// Get nested value from object using dot notation
function getNestedValue(obj, path) {
  return path.split('.').reduce((current, key) => current?.[key], obj);
}

// Set nested value in object using dot notation
function setNestedValue(obj, path, value) {
  const keys = path.split('.');
  const lastKey = keys.pop();
  const target = keys.reduce((current, key) => {
    if (!(key in current)) {
      current[key] = {};
    }
    return current[key];
  }, obj);
  target[lastKey] = value;
}

// Delete nested key from object
function deleteNestedKey(obj, path) {
  const keys = path.split('.');
  const lastKey = keys.pop();
  const target = keys.reduce((current, key) => current?.[key], obj);
  if (target && lastKey in target) {
    delete target[lastKey];
  }
}

// Clean empty objects recursively
function cleanEmptyObjects(obj) {
  Object.keys(obj).forEach(key => {
    if (obj[key] && typeof obj[key] === 'object' && !Array.isArray(obj[key])) {
      cleanEmptyObjects(obj[key]);
      if (Object.keys(obj[key]).length === 0) {
        delete obj[key];
      }
    }
  });
}

// Get all keys from nested object
function getAllKeys(obj, prefix = '') {
  let keys = [];
  for (const [key, value] of Object.entries(obj)) {
    const fullKey = prefix ? `${prefix}.${key}` : key;
    if (typeof value === 'object' && value !== null && !Array.isArray(value)) {
      keys = keys.concat(getAllKeys(value, fullKey));
    } else {
      keys.push(fullKey);
    }
  }
  return keys;
}

// Sync translations
function syncTranslations() {
  console.log('🔄 Starting translation sync...\n');

  // Step 1: Extract keys from code
  console.log('📖 Step 1: Extracting translation keys from code...');
  const codeKeys = extractTranslationKeysFromCode();

  let totalCodeKeys = 0;
  codeKeys.forEach(keys => totalCodeKeys += keys.size);
  console.log(`   Found ${totalCodeKeys} translation keys in code\n`);

  // Step 2: Load English (source) translations
  console.log('📚 Step 2: Loading English translation files...');
  const sourceTranslations = new Map();

  codeKeys.forEach((keys, namespace) => {
    const filePath = path.join(LOCALES_DIR, SOURCE_LANG, `${namespace}.json`);
    if (fs.existsSync(filePath)) {
      const rawContent = fs.readFileSync(filePath, 'utf-8').replace(/^\uFEFF/, ''); // Remove BOM
      const content = JSON.parse(rawContent);
      sourceTranslations.set(namespace, content);
    } else {
      sourceTranslations.set(namespace, {});
      console.log(`   ⚠️  Creating new namespace: ${namespace}`);
    }
  });
  console.log(`   Loaded ${sourceTranslations.size} namespaces\n`);

  // Step 3: Sync English with code keys
  console.log('🔧 Step 3: Syncing English translations with code...');
  let enAdded = 0;
  let enRemoved = 0;

  codeKeys.forEach((keys, namespace) => {
    const translation = sourceTranslations.get(namespace);
    const existingKeys = new Set(getAllKeys(translation));

    // Add missing keys to English
    keys.forEach(key => {
      if (!existingKeys.has(key)) {
        setNestedValue(translation, key, `[EN] ${key}`);
        console.log(`   ✅ Added to EN [${namespace}]: ${key}`);
        enAdded++;
      }
    });

    // Remove unused keys from English
    existingKeys.forEach(key => {
      if (!keys.has(key)) {
        deleteNestedKey(translation, key);
        console.log(`   🗑️  Removed from EN [${namespace}]: ${key}`);
        enRemoved++;
      }
    });

    // Clean empty objects
    cleanEmptyObjects(translation);

    // Save English file
    const filePath = path.join(LOCALES_DIR, SOURCE_LANG, `${namespace}.json`);
    fs.writeFileSync(filePath, JSON.stringify(translation, null, 2) + '\n', 'utf-8');
  });

  console.log(`   Added ${enAdded} keys, removed ${enRemoved} keys\n`);

  // Step 4: Sync all other languages
  console.log('🌍 Step 4: Syncing all other languages...');
  const otherLanguages = LANGUAGES.filter(lang => lang !== SOURCE_LANG);
  let totalSynced = 0;

  otherLanguages.forEach(lang => {
    let langAdded = 0;
    let langRemoved = 0;

    codeKeys.forEach((keys, namespace) => {
      const sourceTranslation = sourceTranslations.get(namespace);
      const targetFilePath = path.join(LOCALES_DIR, lang, `${namespace}.json`);

      // Load or create target translation
      let targetTranslation = {};
      if (fs.existsSync(targetFilePath)) {
        try {
          const content = fs.readFileSync(targetFilePath, 'utf-8').replace(/^\uFEFF/, ''); // Remove BOM
          targetTranslation = JSON.parse(content);
        } catch (error) {
          console.log(`   ⚠️  Corrupted file detected: ${lang}/${namespace}.json - recreating...`);
          targetTranslation = {};
        }
      }

      const existingKeys = new Set(getAllKeys(targetTranslation));

      // Add missing keys from English
      getAllKeys(sourceTranslation).forEach(key => {
        if (!existingKeys.has(key)) {
          const sourceValue = getNestedValue(sourceTranslation, key);
          setNestedValue(targetTranslation, key, `[${lang.toUpperCase()}] ${sourceValue}`);
          langAdded++;
        }
      });

      // Remove keys not in English
      existingKeys.forEach(key => {
        if (!getAllKeys(sourceTranslation).includes(key)) {
          deleteNestedKey(targetTranslation, key);
          langRemoved++;
        }
      });

      // Clean empty objects
      cleanEmptyObjects(targetTranslation);

      // Ensure directory exists
      const targetDir = path.join(LOCALES_DIR, lang);
      if (!fs.existsSync(targetDir)) {
        fs.mkdirSync(targetDir, { recursive: true });
      }

      // Save target file
      fs.writeFileSync(targetFilePath, JSON.stringify(targetTranslation, null, 2) + '\n', 'utf-8');
    });

    if (langAdded > 0 || langRemoved > 0) {
      console.log(`   ${lang}: +${langAdded} keys, -${langRemoved} keys`);
      totalSynced += langAdded;
    }
  });

  console.log(`\n   Total synced across ${otherLanguages.length} languages: ${totalSynced} keys\n`);

  // Summary
  console.log('=' .repeat(60));
  console.log('📋 SYNC SUMMARY');
  console.log('='.repeat(60));
  console.log(`✅ Code contains ${totalCodeKeys} translation keys`);
  console.log(`📝 English: +${enAdded} added, -${enRemoved} removed`);
  console.log(`🌍 Other languages: ${totalSynced} keys synced`);
  console.log(`\n✨ Translation sync complete!\n`);
  console.log('⚠️  NOTE: Placeholder text added for new keys.');
  console.log('   Format: [LANG] original_key_name');
  console.log('   Please translate these manually or use AI translation.\n');
}

// Run sync
syncTranslations();
