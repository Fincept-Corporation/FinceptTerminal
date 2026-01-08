#!/usr/bin/env node
// Auto-translate placeholder texts using AI or Google Translate API
// Finds all [LANG] placeholder texts and translates them

import fs from 'fs';
import path from 'path';

const LOCALES_DIR = 'public/locales';
const LANGUAGES = ['es', 'fr', 'de', 'zh', 'ja', 'hi', 'ar', 'pt', 'ru', 'bn', 'ko', 'tr', 'it', 'id', 'vi', 'th', 'pl', 'uk', 'ro'];

// Language name mapping for better AI prompting
const LANG_NAMES = {
  'es': 'Spanish', 'fr': 'French', 'de': 'German', 'zh': 'Chinese',
  'ja': 'Japanese', 'hi': 'Hindi', 'ar': 'Arabic', 'pt': 'Portuguese',
  'ru': 'Russian', 'bn': 'Bengali', 'ko': 'Korean', 'tr': 'Turkish',
  'it': 'Italian', 'id': 'Indonesian', 'vi': 'Vietnamese', 'th': 'Thai',
  'pl': 'Polish', 'uk': 'Ukrainian', 'ro': 'Romanian'
};

// Get nested value from object
function getNestedValue(obj, path) {
  return path.split('.').reduce((current, key) => current?.[key], obj);
}

// Set nested value in object
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

// Get all keys from nested object with their values
function getAllKeysWithValues(obj, prefix = '') {
  let entries = [];
  for (const [key, value] of Object.entries(obj)) {
    const fullKey = prefix ? `${prefix}.${key}` : key;
    if (typeof value === 'object' && value !== null && !Array.isArray(value)) {
      entries = entries.concat(getAllKeysWithValues(value, fullKey));
    } else {
      entries.push({ key: fullKey, value });
    }
  }
  return entries;
}

// Find placeholder entries that need translation
function findPlaceholders(lang) {
  const placeholders = new Map(); // namespace -> [{ key, value }]
  const langDir = path.join(LOCALES_DIR, lang);

  if (!fs.existsSync(langDir)) {
    return placeholders;
  }

  const files = fs.readdirSync(langDir).filter(f => f.endsWith('.json'));

  files.forEach(file => {
    const namespace = file.replace('.json', '');
    const filePath = path.join(langDir, file);
    const content = JSON.parse(fs.readFileSync(filePath, 'utf-8'));

    const entries = getAllKeysWithValues(content);
    const needsTranslation = entries.filter(entry =>
      typeof entry.value === 'string' &&
      entry.value.startsWith(`[${lang.toUpperCase()}]`)
    );

    if (needsTranslation.length > 0) {
      placeholders.set(namespace, needsTranslation);
    }
  });

  return placeholders;
}

// Generate translation instructions
function generateInstructions() {
  console.log('\n' + '='.repeat(70));
  console.log('🤖 AUTO-TRANSLATION HELPER');
  console.log('='.repeat(70));
  console.log('\nThis script helps you translate placeholder texts efficiently.\n');

  let totalPlaceholders = 0;
  const report = [];

  LANGUAGES.forEach(lang => {
    const placeholders = findPlaceholders(lang);
    let count = 0;

    placeholders.forEach(entries => {
      count += entries.length;
    });

    if (count > 0) {
      totalPlaceholders += count;
      report.push({ lang, count, placeholders });
    }
  });

  if (totalPlaceholders === 0) {
    console.log('✅ No placeholder texts found! All translations are complete.\n');
    return;
  }

  console.log(`Found ${totalPlaceholders} placeholder texts across ${report.length} languages.\n`);

  // Show breakdown
  console.log('BREAKDOWN BY LANGUAGE:');
  console.log('-'.repeat(70));
  report.forEach(({ lang, count }) => {
    console.log(`${LANG_NAMES[lang].padEnd(15)} (${lang}): ${count.toString().padStart(4)} placeholders`);
  });

  console.log('\n' + '='.repeat(70));
  console.log('TRANSLATION OPTIONS:');
  console.log('='.repeat(70));

  console.log('\n📝 OPTION 1: Manual Translation');
  console.log('-'.repeat(70));
  console.log('Navigate to: public/locales/{lang}/{namespace}.json');
  console.log('Find entries starting with [LANG] and translate them.\n');

  console.log('📝 OPTION 2: AI-Assisted Translation (Recommended)');
  console.log('-'.repeat(70));
  console.log('Use ChatGPT, Claude, or similar AI with this prompt:\n');

  // Generate sample for first language
  if (report.length > 0) {
    const { lang, placeholders } = report[0];
    const namespace = [...placeholders.keys()][0];
    const entries = placeholders.get(namespace).slice(0, 5);

    console.log(`"Translate these UI strings to ${LANG_NAMES[lang]}:\n`);
    entries.forEach(({ key, value }) => {
      const cleanValue = value.replace(`[${lang.toUpperCase()}] `, '');
      console.log(`- "${cleanValue}" (context: ${key})`);
    });
    console.log('"\n');
  }

  console.log('📝 OPTION 3: Export for Translation Service');
  console.log('-'.repeat(70));
  console.log('Export placeholder texts to CSV for professional translation:\n');

  // Generate CSV export
  const csvPath = 'translations-to-translate.csv';
  const csvLines = ['Language,Namespace,Key,Original Text'];

  report.forEach(({ lang, placeholders }) => {
    placeholders.forEach((entries, namespace) => {
      entries.forEach(({ key, value }) => {
        const cleanValue = value.replace(`[${lang.toUpperCase()}] `, '').replace(/"/g, '""');
        csvLines.push(`${lang},${namespace},${key},"${cleanValue}"`);
      });
    });
  });

  fs.writeFileSync(csvPath, csvLines.join('\n'), 'utf-8');
  console.log(`✅ Exported to: ${csvPath}`);
  console.log('   Send this file to translators and import results back.\n');

  console.log('=' .repeat(70));
  console.log('NEXT STEPS:');
  console.log('='.repeat(70));
  console.log('1. Translate the placeholder texts using one of the options above');
  console.log('2. Update the JSON files in public/locales/{lang}/');
  console.log('3. Run "bun run i18n:audit" to verify all translations');
  console.log('4. Commit the updated translation files\n');

  // Generate example of what translated file should look like
  console.log('EXAMPLE - Before and After:');
  console.log('-'.repeat(70));
  if (report.length > 0 && report[0].placeholders.size > 0) {
    const { lang, placeholders } = report[0];
    const namespace = [...placeholders.keys()][0];
    const entry = placeholders.get(namespace)[0];

    console.log(`\nFile: public/locales/${lang}/${namespace}.json`);
    console.log('\nBefore:');
    console.log(`  "${entry.key}": "${entry.value}"`);
    console.log('\nAfter (example):');
    const cleanValue = entry.value.replace(`[${lang.toUpperCase()}] `, '');
    console.log(`  "${entry.key}": "${cleanValue} in ${LANG_NAMES[lang]}"`);
  }

  console.log('\n✨ Ready to translate!\n');
}

// Run
generateInstructions();
