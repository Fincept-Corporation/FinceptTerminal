#!/usr/bin/env node
// Extract hardcoded strings from React components and generate translation keys

import fs from 'fs';
import path from 'path';
import { glob } from 'glob';

const SRC_DIR = 'src';
const LOCALES_DIR = 'public/locales/en';

// Patterns to find hardcoded strings in JSX
const patterns = [
  // JSX text content: >Some Text<
  />\s*([A-Z][A-Za-z0-9\s\-\.,'!?:]+)\s*</g,
  // Placeholder attributes: placeholder="text"
  /placeholder=["']([^"']+)["']/g,
  // Title attributes: title="text"
  /title=["']([^"']+)["']/g,
  // Alert/confirm calls: alert("text") or confirm("text")
  /(?:alert|confirm)\(["']([^"']+)["']\)/g,
  // setError calls: setError("text") or setError('text')
  /setError\(["']([^"']+)["']\)/g,
];

// Strings to ignore (code patterns, not UI text)
const ignorePatterns = [
  /^[a-z_]+$/,           // lowercase only (likely code)
  /^[A-Z_]+$/,           // CONSTANTS
  /^\d+$/,               // numbers only
  /^https?:\/\//,        // URLs
  /^[a-z]+\.[a-z]+/,     // file paths
  /^\s*$/,               // whitespace
  /^[<>\/]/,             // HTML tags
  /className/,           // React className
  /^\{/,                 // JSX expressions
  /^import/,             // imports
  /^export/,             // exports
  /^const /,             // const declarations
  /^function/,           // function declarations
  /Promise/,             // Promise type
  /^[a-z]+:/,            // object keys
];

function shouldIgnore(str) {
  if (str.length < 3 || str.length > 200) return true;
  return ignorePatterns.some(pattern => pattern.test(str));
}

function extractStrings(content, filePath) {
  const strings = [];
  const seen = new Set();

  // Pattern for JSX string literals that look like UI text
  const jsxTextPattern = />\s*([A-Z][A-Za-z0-9\s\-\.,!?:'"()]+)\s*</g;
  let match;

  while ((match = jsxTextPattern.exec(content)) !== null) {
    const text = match[1].trim();
    if (!shouldIgnore(text) && !seen.has(text)) {
      seen.add(text);
      strings.push({ text, type: 'jsx-text', file: filePath });
    }
  }

  // Pattern for string attributes
  const attrPatterns = [
    { pattern: /placeholder=["']([^"']{3,})["']/g, type: 'placeholder' },
    { pattern: /title=["']([^"']{3,})["']/g, type: 'title' },
    { pattern: /aria-label=["']([^"']{3,})["']/g, type: 'aria-label' },
  ];

  attrPatterns.forEach(({ pattern, type }) => {
    while ((match = pattern.exec(content)) !== null) {
      const text = match[1].trim();
      if (!shouldIgnore(text) && !seen.has(text)) {
        seen.add(text);
        strings.push({ text, type, file: filePath });
      }
    }
  });

  // Pattern for error/alert messages
  const errorPatterns = [
    /setError\(["']([^"']{5,})["']\)/g,
    /alert\(["']([^"']{5,})["']\)/g,
    /confirm\(["']([^"']{5,})["']\)/g,
  ];

  errorPatterns.forEach(pattern => {
    while ((match = pattern.exec(content)) !== null) {
      const text = match[1].trim();
      if (!shouldIgnore(text) && !seen.has(text)) {
        seen.add(text);
        strings.push({ text, type: 'message', file: filePath });
      }
    }
  });

  return strings;
}

function getNamespaceFromFile(filePath) {
  const fileName = path.basename(filePath, '.tsx');

  if (filePath.includes('/auth/')) return 'auth';
  if (filePath.includes('/settings/')) return 'settings';
  if (filePath.includes('/dashboard/')) return 'dashboard';

  // Tab files
  const tabMap = {
    'DashboardTab': 'dashboard',
    'SettingsTab': 'settings',
    'ChatTab': 'chat',
    'NewsTab': 'news',
    'MarketsTab': 'markets',
    'WatchlistTab': 'watchlist',
    'TradingTab': 'trading',
    'PortfolioTab': 'portfolio',
    'ForumTab': 'forum',
    'ProfileTab': 'profile',
    'ScreenerTab': 'screener',
    'BacktestingTabNew': 'backtesting',
    'GeopoliticsTab': 'geopolitics',
    'MaritimeTab': 'maritime',
    'EconomicsTab': 'economics',
    'DBnomicsTab': 'dbnomics',
    'ExcelTab': 'excel',
    'CodeEditorTab': 'codeEditor',
    'NodeEditorTab': 'nodeEditor',
    'ReportBuilderTab': 'reportBuilder',
    'NotesTab': 'notes',
    'SupportTicketTab': 'support',
    'MarketplaceTab': 'marketplace',
    'AgentConfigTab': 'agentConfig',
    'AlphaArenaTab': 'alphaArena',
    'DerivativesTab': 'derivatives',
    'LoginAuditDashboard': 'loginAudit',
  };

  return tabMap[fileName] || 'common';
}

function generateKey(text, namespace) {
  // Convert text to camelCase key
  const key = text
    .toLowerCase()
    .replace(/[^a-z0-9\s]/g, '')
    .trim()
    .split(/\s+/)
    .slice(0, 4)
    .map((word, i) => i === 0 ? word : word.charAt(0).toUpperCase() + word.slice(1))
    .join('');

  return key || 'text';
}

async function main() {
  console.log('🔍 Scanning for hardcoded strings...\n');

  const files = glob.sync(`${SRC_DIR}/**/*.tsx`, { ignore: '**/node_modules/**' });
  const allStrings = [];
  const byNamespace = new Map();

  for (const file of files) {
    const content = fs.readFileSync(file, 'utf-8');

    // Skip files that already have useTranslation with good coverage
    const hasTranslation = content.includes('useTranslation');
    const tCalls = (content.match(/\bt\(/g) || []).length;

    const strings = extractStrings(content, file);

    if (strings.length > 0) {
      const namespace = getNamespaceFromFile(file);

      if (!byNamespace.has(namespace)) {
        byNamespace.set(namespace, []);
      }

      strings.forEach(s => {
        s.namespace = namespace;
        s.key = generateKey(s.text, namespace);
        byNamespace.get(namespace).push(s);
        allStrings.push(s);
      });
    }
  }

  console.log(`Found ${allStrings.length} potential hardcoded strings\n`);

  // Group by namespace and show summary
  console.log('BY NAMESPACE:');
  console.log('-'.repeat(60));

  byNamespace.forEach((strings, namespace) => {
    console.log(`\n[${namespace}] - ${strings.length} strings`);
    strings.slice(0, 5).forEach(s => {
      console.log(`  "${s.text.substring(0, 50)}${s.text.length > 50 ? '...' : ''}" (${s.type})`);
    });
    if (strings.length > 5) {
      console.log(`  ... and ${strings.length - 5} more`);
    }
  });

  // Generate translation keys file
  const output = {};
  byNamespace.forEach((strings, namespace) => {
    output[namespace] = {};
    const seen = new Set();

    strings.forEach(s => {
      let key = s.key;
      let counter = 1;
      while (seen.has(key)) {
        key = `${s.key}${counter++}`;
      }
      seen.add(key);
      output[namespace][key] = s.text;
    });
  });

  fs.writeFileSync('hardcoded-strings.json', JSON.stringify(output, null, 2));
  console.log('\n\n✅ Saved to hardcoded-strings.json');
  console.log('Review this file and merge needed keys into translation files.');
}

main().catch(console.error);
