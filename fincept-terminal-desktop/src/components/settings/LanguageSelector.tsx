// File: src/components/settings/LanguageSelector.tsx
// Language selector component for the Settings tab

import React from 'react';
import { useLanguage } from '@/contexts/LanguageContext';
import { useTranslation } from 'react-i18next';
import { Globe } from 'lucide-react';

export const LanguageSelector: React.FC = () => {
  const { currentLanguage, changeLanguage, availableLanguages } = useLanguage();
  const { t } = useTranslation('settings');

  return (
    <div className="space-y-4">
      <div className="flex items-center gap-2 mb-4">
        <Globe className="h-5 w-5 text-blue-400" />
        <h3 className="text-lg font-medium text-white">{t('language.title')}</h3>
      </div>

      <div className="space-y-3">
        <label className="text-sm text-zinc-400">{t('language.selectLanguage')}</label>

        <div className="grid grid-cols-2 gap-3">
          {availableLanguages.map((lang) => (
            <button
              key={lang.code}
              onClick={() => changeLanguage(lang.code)}
              className={`
                flex items-center gap-3 p-3 rounded-lg border transition-all
                ${
                  currentLanguage === lang.code
                    ? 'bg-blue-500/20 border-blue-500 text-white'
                    : 'bg-zinc-800/50 border-zinc-700 text-zinc-300 hover:bg-zinc-800 hover:border-zinc-600'
                }
              `}
            >
              <span className="text-2xl">{lang.flag}</span>
              <div className="text-left">
                <div className="font-medium text-sm">{lang.nativeName}</div>
                <div className="text-xs text-zinc-500">{lang.name}</div>
              </div>
              {currentLanguage === lang.code && (
                <div className="ml-auto">
                  <div className="w-2 h-2 bg-blue-500 rounded-full"></div>
                </div>
              )}
            </button>
          ))}
        </div>

        <div className="mt-4 p-3 bg-zinc-800/30 border border-zinc-700 rounded-lg">
          <div className="flex items-center gap-2 text-xs text-zinc-400">
            <span className="font-medium">{t('language.currentLanguage')}:</span>
            <span className="text-white">
              {availableLanguages.find(lang => lang.code === currentLanguage)?.nativeName || 'English'}
            </span>
          </div>
        </div>
      </div>
    </div>
  );
};
