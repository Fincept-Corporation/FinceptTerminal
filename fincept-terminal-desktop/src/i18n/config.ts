// File: src/i18n/config.ts
// i18next configuration for internationalization

import i18next from 'i18next';
import { initReactI18next } from 'react-i18next';
import LanguageDetector from 'i18next-browser-languagedetector';

// Import translation files
import enCommon from '../../public/locales/en/common.json';
import enAuth from '../../public/locales/en/auth.json';
import enDashboard from '../../public/locales/en/dashboard.json';
import enSettings from '../../public/locales/en/settings.json';
import enTabs from '../../public/locales/en/tabs.json';

const resources = {
  en: {
    common: enCommon,
    auth: enAuth,
    dashboard: enDashboard,
    settings: enSettings,
    tabs: enTabs,
  },
  // Other languages will be added here
};

i18next
  .use(LanguageDetector) // Auto-detect user language
  .use(initReactI18next) // Pass i18n to react-i18next
  .init({
    resources,
    fallbackLng: 'en',
    supportedLngs: ['en', 'es', 'fr', 'de', 'zh', 'ja', 'hi', 'ar', 'pt', 'ru', 'bn', 'ko', 'tr', 'it', 'id', 'vi', 'th', 'pl', 'uk', 'ro'],

    detection: {
      order: ['localStorage', 'navigator'],
      caches: ['localStorage'],
      lookupLocalStorage: 'i18nextLng',
    },

    interpolation: {
      escapeValue: false, // React already escapes
    },

    defaultNS: 'common',
    ns: ['common', 'auth', 'dashboard', 'settings', 'tabs'],

    react: {
      useSuspense: false, // Prevent suspense issues with Tauri
    },
  });

export default i18next;
