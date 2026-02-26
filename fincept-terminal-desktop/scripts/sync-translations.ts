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

// Extract translation keys from code, including fallback values from t('key', 'fallback')
function extractTranslationKeysFromCode(): Map<string, Map<string, string | null>> {
  const files = glob.sync(`${SRC_DIR}/**/*.{ts,tsx}`, { ignore: '**/node_modules/**' });
  // namespace -> Map<key, fallback value | null>
  const keysMap = new Map<string, Map<string, string | null>>();

  const nsPattern = /useTranslation\(['"]([\w]+)['"]\)/g;

  files.forEach(file => {
    const content = fs.readFileSync(file, 'utf-8');

    // Find default namespace from useTranslation('namespace')
    let defaultNs = 'common';
    let nsMatch: RegExpExecArray | null;
    nsPattern.lastIndex = 0;
    while ((nsMatch = nsPattern.exec(content)) !== null) {
      defaultNs = nsMatch[1];
    }

    const addKey = (ns: string, key: string, fallback: string | null) => {
      if (!keysMap.has(ns)) keysMap.set(ns, new Map());
      const nsMap = keysMap.get(ns)!;
      // Only set fallback if we don't already have one for this key
      if (!nsMap.has(key) || (nsMap.get(key) === null && fallback !== null)) {
        nsMap.set(key, fallback);
      }
    };

    let match: RegExpExecArray | null;

    // t('ns:key') or t('ns:key', 'fallback')
    const nsKeyPattern = /\bt\(\s*['"`]([\w]+):([\w.]+)['"`](?:\s*,\s*['"`]([^'"`]*)['"`])?\s*\)/g;
    while ((match = nsKeyPattern.exec(content)) !== null) {
      addKey(match[1], match[2], match[3] ?? null);
    }

    // t('key') or t('key', 'fallback') — uses default namespace
    const simpleKeyPattern = /\bt\(\s*['"`]([\w.]+)['"`](?:\s*,\s*['"`]([^'"`]*)['"`])?\s*\)/g;
    while ((match = simpleKeyPattern.exec(content)) !== null) {
      const fullKey = match[1];
      if (!fullKey.includes(':')) {
        addKey(defaultNs, fullKey, match[2] ?? null);
      }
    }

    // Dynamic key maps: objects whose values are translation keys passed to t()
    // e.g. const MAP: { [k: string]: string } = { 'SYM': 'widgets.indexSP500' }
    //      then used as t(MAP[symbol])
    // Strategy: find any string literal value matching `<declaredNs>.<word>` inside
    // a const/variable block that also contains t( somewhere in the same file.
    // Only scan files that actually call t() with a dynamic variable: t(VARNAME[...])
    const dynamicTCallPattern = /\bt\(\s*\w+[\w.\[\]'"]+\s*\)/g;
    if (dynamicTCallPattern.test(content)) {
      // Extract all string literal values that look like `namespace.key`
      const mapValuePattern = /:\s*['"`]((\w+)\.(\w+(?:\.\w+)*))['"`]/g;
      while ((match = mapValuePattern.exec(content)) !== null) {
        const fullKey = match[1];   // e.g. "widgets.indexSP500"
        const ns = match[2];        // e.g. "widgets" -> but namespace is "dashboard"
        // The value namespace (e.g. "widgets") is the key prefix, not the i18n namespace.
        // The actual i18n namespace is defaultNs. Only add if the prefix matches
        // what the file's t() calls use (i.e., keys in the defaultNs start with this prefix).
        // We verify by checking existing t('prefix.something') calls exist in the file.
        const prefixUsed = new RegExp(`\\bt\\(['"\`]${ns}\\.`).test(content);
        if (prefixUsed) {
          addKey(defaultNs, fullKey, null);
        }
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

  // Step 3: Sync English with code keys (ADD only — never remove existing EN keys)
  console.log('\u{1F527} Step 3: Syncing English translations with code...');
  let enAdded = 0;

  codeKeys.forEach((keys, namespace) => {
    const translation = sourceTranslations.get(namespace)!;
    const existingKeys = new Set(getAllKeys(translation));

    // Add missing keys to English using the fallback value from the t() call when available
    keys.forEach((fallback, key) => {
      if (!existingKeys.has(key)) {
        // Use the fallback text from code if present, otherwise use a placeholder
        const value = (fallback && fallback.trim()) ? fallback.trim() : `[MISSING] ${key}`;
        setNestedValue(translation, key, value);
        console.log(`   \u2705 Added to EN [${namespace}]: ${key} = "${value}"`);
        enAdded++;
      }
    });

    // NOTE: We intentionally do NOT remove keys from EN that aren't found in
    // the current code scan. They may be used via dynamic keys, in Python scripts,
    // or in code paths not caught by the regex. Manual cleanup only.

    // Save English file
    const filePath = path.join(LOCALES_DIR, SOURCE_LANG, `${namespace}.json`);
    fs.mkdirSync(path.dirname(filePath), { recursive: true });
    fs.writeFileSync(filePath, JSON.stringify(translation, null, 2) + '\n', 'utf-8');
  });

  console.log(`   Added ${enAdded} keys\n`);

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
  console.log(`\u{1F4DD} English: +${enAdded} added (existing keys never removed)`);
  console.log(`\u{1F30D} Other languages: ${totalSynced} keys synced`);
  console.log(`\n\u2728 Translation sync complete!\n`);
  console.log('\u26A0\uFE0F  NOTE: Placeholder text added for new keys.');
  console.log('   Format: [LANG] original_key_name');
  console.log('   Please translate these manually or use AI translation.\n');
}

// Run sync
syncTranslations();
