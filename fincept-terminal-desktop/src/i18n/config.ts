// File: src/i18n/config.ts
// i18next configuration for internationalization

import i18next from 'i18next';
import { initReactI18next } from 'react-i18next';
import LanguageDetector from 'i18next-browser-languagedetector';
import Backend from 'i18next-http-backend';
import { invoke } from '@tauri-apps/api/core';

// Custom storage backend using SQLite
const sqliteStorage = {
  name: 'sqliteStorage',
  async: true,

  // Async read
  read: async (language: string, namespace: string, callback: (error: any, data: any) => void) => {
    // Not used by i18next-browser-languagedetector
    callback(null, null);
  },

  // Async write
  save: async (language: string, namespace: string, data: any) => {
    // Not used by i18next-browser-languagedetector
  }
};

// Custom language detector using SQLite
const sqliteLanguageDetector = {
  name: 'sqliteDetector',
  lookup: async (): Promise<string | undefined> => {
    try {
      const lang = await invoke<string>('storage_get_optional', { key: 'i18nextLng' });
      return lang || undefined;
    } catch (error) {
      console.error('[i18n] Failed to get language from SQLite:', error);
      return undefined;
    }
  },
  cacheUserLanguage: async (lng: string) => {
    try {
      await invoke('storage_set', { key: 'i18nextLng', value: lng });
    } catch (error) {
      console.error('[i18n] Failed to save language to SQLite:', error);
    }
  }
};

i18next
  .use(Backend) // Load translations from public folder
  .use(LanguageDetector) // Auto-detect user language
  .use(initReactI18next) // Pass i18n to react-i18next
  .init({
    fallbackLng: 'en',
    supportedLngs: ['en', 'es', 'fr', 'de', 'zh', 'ja', 'hi', 'ar', 'pt', 'ru', 'bn', 'ko', 'tr', 'it', 'id', 'vi', 'th', 'pl', 'uk', 'ro'],

    backend: {
      loadPath: '/locales/{{lng}}/{{ns}}.json',
    },

    detection: {
      order: ['querystring', 'cookie', 'navigator'],
      caches: [], // Disabled - using SQLite for persistence
    },

    interpolation: {
      escapeValue: false, // React already escapes
    },

    defaultNS: 'common',
    ns: [
      'common', 'auth', 'dashboard', 'settings', 'tabs',
      'chat', 'economics', 'equityResearch', 'forum', 'geopolitics',
      'loginAudit', 'maritime', 'markets', 'news', 'portfolio',
      'profile', 'screener', 'trading', 'watchlist', 'dataMapping',
      'dataSources', 'docs', 'notes', 'support', 'nodeEditor',
      'reportBuilder', 'alphaArena', 'codeEditor', 'agentConfig',
      'dbnomics', 'excel', 'marketplace', 'backtesting', 'akshare'
    ],

    react: {
      useSuspense: false, // Prevent suspense issues with Tauri
    },
  });

// Load saved language from SQLite on init
(async () => {
  try {
    const savedLang = await invoke<string>('storage_get_optional', { key: 'i18nextLng' });
    const supportedLngs = i18next.options.supportedLngs;
    if (savedLang && Array.isArray(supportedLngs) && supportedLngs.includes(savedLang)) {
      await i18next.changeLanguage(savedLang);
    }
  } catch (error) {
    console.error('[i18n] Failed to load saved language:', error);
  }
})();

// Save language to SQLite on change
i18next.on('languageChanged', async (lng) => {
  try {
    await invoke('storage_set', { key: 'i18nextLng', value: lng });
  } catch (error) {
    console.error('[i18n] Failed to save language:', error);
  }
});

export default i18next;
