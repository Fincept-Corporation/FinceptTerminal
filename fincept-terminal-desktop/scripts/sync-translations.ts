#!/usr/bin/env bun
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

type NestedObject = { [key: string]: string | NestedObject };

// Extract translation keys from code
function extractTranslationKeysFromCode(): Map<string, Set<string>> {
  const files = glob.sync(`${SRC_DIR}/**/*.{ts,tsx}`, { ignore: '**/node_modules/**' });
  const keysMap = new Map<string, Set<string>>(); // namespace -> Set of keys

  // Patterns to match t() calls
  const nsPattern = /useTranslation\(['"]([\w]+)['"]\)/g;

  files.forEach(file => {
    const content = fs.readFileSync(file, 'utf-8');

    // Find default namespace from useTranslation('namespace')
    let defaultNs = 'common';
    let nsMatch: RegExpExecArray | null;
    while ((nsMatch = nsPattern.exec(content)) !== null) {
      defaultNs = nsMatch[1];
    }

    // Extract keys with namespace prefix: t('namespace:key')
    let match: RegExpExecArray | null;
    const pattern2Regex = /\bt\(['"]([\w]+):([\w.]+)['"]\)/g;
    while ((match = pattern2Regex.exec(content)) !== null) {
      const namespace = match[1];
      const key = match[2];

      if (!keysMap.has(namespace)) {
        keysMap.set(namespace, new Set());
      }
      keysMap.get(namespace)!.add(key);
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
        keysMap.get(defaultNs)!.add(fullKey);
      }
    }
  });

  return keysMap;
}

// Get nested value from object using dot notation
function getNestedValue(obj: NestedObject, dotPath: string): string | NestedObject | undefined {
  return dotPath.split('.').reduce<string | NestedObject | undefined>(
    (current, key) => (current && typeof current === 'object' ? (current as NestedObject)[key] : undefined),
    obj
  );
}

// Set nested value in object using dot notation
function setNestedValue(obj: NestedObject, dotPath: string, value: string): void {
  const keys = dotPath.split('.');
  const lastKey = keys.pop()!;
  const target = keys.reduce<NestedObject>((current, key) => {
    if (!(key in current)) {
      current[key] = {};
    }
    return current[key] as NestedObject;
  }, obj);
  target[lastKey] = value;
}

// Delete nested key from object
function deleteNestedKey(obj: NestedObject, dotPath: string): void {
  const keys = dotPath.split('.');
  const lastKey = keys.pop()!;
  const target = keys.reduce<NestedObject | undefined>(
    (current, key) => (current ? (current[key] as NestedObject) : undefined),
    obj
  );
  if (target && lastKey in target) {
    delete target[lastKey];
  }
}

// Clean empty objects recursively
function cleanEmptyObjects(obj: NestedObject): void {
  Object.keys(obj).forEach(key => {
    if (obj[key] && typeof obj[key] === 'object' && !Array.isArray(obj[key])) {
      cleanEmptyObjects(obj[key] as NestedObject);
      if (Object.keys(obj[key] as NestedObject).length === 0) {
        delete obj[key];
      }
    }
  });
}

// Get all keys from nested object
function getAllKeys(obj: NestedObject, prefix: string = ''): string[] {
  let keys: string[] = [];
  for (const [key, value] of Object.entries(obj)) {
    const fullKey = prefix ? `${prefix}.${key}` : key;
    if (typeof value === 'object' && value !== null && !Array.isArray(value)) {
      keys = keys.concat(getAllKeys(value as NestedObject, fullKey));
    } else {
      keys.push(fullKey);
    }
  }
  return keys;
}

// Sync translations
function syncTranslations(): void {
  console.log('\u{1F504} Starting translation sync...\n');

  // Step 1: Extract keys from code
  console.log('\u{1F4D6} Step 1: Extracting translation keys from code...');
  const codeKeys = extractTranslationKeysFromCode();

  let totalCodeKeys = 0;
  codeKeys.forEach(keys => totalCodeKeys += keys.size);
  console.log(`   Found ${totalCodeKeys} translation keys in code\n`);

  // Step 2: Load English (source) translations
  console.log('\u{1F4DA} Step 2: Loading English translation files...');
  const sourceTranslations = new Map<string, NestedObject>();

  codeKeys.forEach((_keys, namespace) => {
    const filePath = path.join(LOCALES_DIR, SOURCE_LANG, `${namespace}.json`);
    if (fs.existsSync(filePath)) {
      const rawContent = fs.readFileSync(filePath, 'utf-8').replace(/^\uFEFF/, ''); // Remove BOM
      const content: NestedObject = JSON.parse(rawContent);
      sourceTranslations.set(namespace, content);
    } else {
      sourceTranslations.set(namespace, {});
      console.log(`   \u26A0\uFE0F  Creating new namespace: ${namespace}`);
    }
  });
  console.log(`   Loaded ${sourceTranslations.size} namespaces\n`);

  // Step 3: Sync English with code keys
  console.log('\u{1F527} Step 3: Syncing English translations with code...');
  let enAdded = 0;
  let enRemoved = 0;

  codeKeys.forEach((keys, namespace) => {
    const translation = sourceTranslations.get(namespace)!;
    const existingKeys = new Set(getAllKeys(translation));

    // Add missing keys to English
    keys.forEach(key => {
      if (!existingKeys.has(key)) {
        setNestedValue(translation, key, `[EN] ${key}`);
        console.log(`   \u2705 Added to EN [${namespace}]: ${key}`);
        enAdded++;
      }
    });

    // Remove unused keys from English
    existingKeys.forEach(key => {
      if (!keys.has(key)) {
        deleteNestedKey(translation, key);
        console.log(`   \u{1F5D1}\uFE0F  Removed from EN [${namespace}]: ${key}`);
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
  console.log('\u{1F30D} Step 4: Syncing all other languages...');
  const otherLanguages = LANGUAGES.filter(lang => lang !== SOURCE_LANG);
  let totalSynced = 0;

  otherLanguages.forEach(lang => {
    let langAdded = 0;
    let langRemoved = 0;

    codeKeys.forEach((_keys, namespace) => {
      const sourceTranslation = sourceTranslations.get(namespace)!;
      const targetFilePath = path.join(LOCALES_DIR, lang, `${namespace}.json`);

      // Load or create target translation
      let targetTranslation: NestedObject = {};
      if (fs.existsSync(targetFilePath)) {
        try {
          const content = fs.readFileSync(targetFilePath, 'utf-8').replace(/^\uFEFF/, ''); // Remove BOM
          targetTranslation = JSON.parse(content);
        } catch (error) {
          console.log(`   \u26A0\uFE0F  Corrupted file detected: ${lang}/${namespace}.json - recreating...`);
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
  console.log('\u{1F4CB} SYNC SUMMARY');
  console.log('='.repeat(60));
  console.log(`\u2705 Code contains ${totalCodeKeys} translation keys`);
  console.log(`\u{1F4DD} English: +${enAdded} added, -${enRemoved} removed`);
  console.log(`\u{1F30D} Other languages: ${totalSynced} keys synced`);
  console.log(`\n\u2728 Translation sync complete!\n`);
  console.log('\u26A0\uFE0F  NOTE: Placeholder text added for new keys.');
  console.log('   Format: [LANG] original_key_name');
  console.log('   Please translate these manually or use AI translation.\n');
}

// Run sync
syncTranslations();
