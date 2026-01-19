// File: src/components/tabs/AboutTab.tsx
// Fincept-style About & Legal Information Interface

import React from 'react';
import { TabHeader } from '@/components/common/TabHeader';
import { TabFooter } from '@/components/common/TabFooter';
import { VERSION_INFO } from '@/constants/version';
import { useTranslation } from 'react-i18next';
import {
  Info, FileText, Scale, Award, Shield, ExternalLink, Mail, Globe, Github
} from 'lucide-react';

const COLORS = {
  ORANGE: '#ea580c',
  WHITE: '#ffffff',
  GREEN: '#10b981',
  GRAY: '#6b7280',
  DARK_BG: '#000000',
  PANEL_BG: '#0a0a0a',
  HEADER_BG: '#1a1a1a',
  CYAN: '#06b6d4',
  BORDER: '#262626',
  MUTED: '#737373'
};

const AboutTab: React.FC = () => {
  const { t } = useTranslation('about');

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: COLORS.DARK_BG }}>
      <TabHeader
        title={t('title')}
        subtitle={t('subtitle')}
      />

      <div className="flex-1 overflow-y-auto p-4">
        <div className="max-w-6xl mx-auto space-y-4">

          {/* Version Info - Compact */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }} className="p-4">
            <div className="flex items-center justify-between">
              <div className="flex items-center space-x-3">
                <Info className="w-5 h-5" style={{ color: COLORS.ORANGE }} />
                <div>
                  <div className="text-lg font-bold font-mono" style={{ color: COLORS.WHITE }}>
                    {VERSION_INFO.name}
                  </div>
                  <div className="text-xs font-mono" style={{ color: COLORS.MUTED }}>
                    {t('version.subtitle')}
                  </div>
                </div>
              </div>
              <div className="text-right">
                <div className="text-xl font-mono font-bold" style={{ color: COLORS.ORANGE }}>
                  v{VERSION_INFO.version}
                </div>
                <div className="text-xs font-mono" style={{ color: COLORS.MUTED }}>
                  {VERSION_INFO.buildDate}
                </div>
              </div>
            </div>
            <div className="mt-3 pt-3" style={{ borderTop: `1px solid ${COLORS.BORDER}` }}>
              <div className="text-xs font-mono" style={{ color: COLORS.MUTED }}>
                {VERSION_INFO.copyright}
              </div>
            </div>
          </div>

          {/* License Information - Two Column */}
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
            {/* Open Source */}
            <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }} className="p-4">
              <div className="flex items-center space-x-2 mb-3">
                <FileText className="w-4 h-4" style={{ color: COLORS.GREEN }} />
                <h3 className="font-bold font-mono text-sm" style={{ color: COLORS.WHITE }}>{t('openSource.title')}</h3>
              </div>
              <div className="space-y-2 text-xs font-mono" style={{ color: COLORS.GRAY }}>
                <div className="flex items-start">
                  <span style={{ color: COLORS.ORANGE }} className="mr-2">•</span>
                  <span>{t('openSource.license')}</span>
                </div>
                <div className="flex items-start">
                  <span style={{ color: COLORS.ORANGE }} className="mr-2">•</span>
                  <span>{t('openSource.personal')}</span>
                </div>
                <div className="flex items-start">
                  <span style={{ color: COLORS.ORANGE }} className="mr-2">•</span>
                  <span>{t('openSource.sharing')}</span>
                </div>
                <div className="flex items-start">
                  <span style={{ color: COLORS.ORANGE }} className="mr-2">•</span>
                  <span>{t('openSource.network')}</span>
                </div>
              </div>
              <div className="mt-3 pt-3" style={{ borderTop: `1px solid ${COLORS.BORDER}` }}>
                <a
                  href="https://www.gnu.org/licenses/agpl-3.0.html"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="inline-flex items-center space-x-1 text-xs font-mono hover:underline"
                  style={{ color: COLORS.CYAN }}
                >
                  <span>gnu.org/licenses/agpl-3.0</span>
                  <ExternalLink className="w-3 h-3" />
                </a>
              </div>
            </div>

            {/* Commercial */}
            <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }} className="p-4">
              <div className="flex items-center space-x-2 mb-3">
                <Award className="w-4 h-4" style={{ color: COLORS.ORANGE }} />
                <h3 className="font-bold font-mono text-sm" style={{ color: COLORS.WHITE }}>{t('commercial.title')}</h3>
              </div>
              <div className="space-y-2 text-xs font-mono" style={{ color: COLORS.GRAY }}>
                <div className="flex items-start">
                  <span style={{ color: COLORS.ORANGE }} className="mr-2">•</span>
                  <span>{t('commercial.required')}</span>
                </div>
                <div className="flex items-start">
                  <span style={{ color: COLORS.ORANGE }} className="mr-2">•</span>
                  <span>{t('commercial.noSharing')}</span>
                </div>
                <div className="flex items-start">
                  <span style={{ color: COLORS.ORANGE }} className="mr-2">•</span>
                  <span>{t('commercial.support')}</span>
                </div>
                <div className="flex items-start">
                  <span style={{ color: COLORS.ORANGE }} className="mr-2">•</span>
                  <span>{t('commercial.custom')}</span>
                </div>
              </div>
              <div className="mt-3 pt-3" style={{ borderTop: `1px solid ${COLORS.BORDER}` }}>
                <a
                  href="mailto:support@fincept.in?subject=Commercial License Inquiry"
                  className="inline-flex items-center space-x-1 text-xs font-mono hover:underline"
                  style={{ color: COLORS.CYAN }}
                >
                  <Mail className="w-3 h-3" />
                  <span>support@fincept.in</span>
                </a>
              </div>
            </div>
          </div>

          {/* Trademarks - Compact */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }} className="p-4">
            <div className="flex items-start space-x-3">
              <Shield className="w-4 h-4 mt-0.5" style={{ color: COLORS.ORANGE }} />
              <div className="flex-1">
                <h3 className="font-bold font-mono text-sm mb-2" style={{ color: COLORS.WHITE }}>{t('trademarks.title')}</h3>
                <p className="text-xs font-mono mb-2" style={{ color: COLORS.GRAY }}>
                  {t('trademarks.description')}
                </p>
                <p className="text-xs font-mono" style={{ color: COLORS.MUTED }}>
                  {t('trademarks.permission')}
                </p>
              </div>
            </div>
          </div>

          {/* Links Grid - Compact */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }} className="p-4">
            <div className="flex items-center space-x-2 mb-3">
              <Globe className="w-4 h-4" style={{ color: COLORS.ORANGE }} />
              <h3 className="font-bold font-mono text-sm" style={{ color: COLORS.WHITE }}>{t('resources.title')}</h3>
            </div>
            <div className="grid grid-cols-2 md:grid-cols-3 gap-2">
              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal"
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-center space-x-2 p-2 hover:bg-opacity-50 transition-colors text-xs font-mono"
                style={{ backgroundColor: COLORS.HEADER_BG, color: COLORS.CYAN, border: `1px solid ${COLORS.BORDER}` }}
              >
                <Github className="w-3 h-3" />
                <span>{t('resources.repository')}</span>
              </a>

              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE"
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-center space-x-2 p-2 hover:bg-opacity-50 transition-colors text-xs font-mono"
                style={{ backgroundColor: COLORS.HEADER_BG, color: COLORS.CYAN, border: `1px solid ${COLORS.BORDER}` }}
              >
                <FileText className="w-3 h-3" />
                <span>{t('resources.license')}</span>
              </a>

              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md"
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-center space-x-2 p-2 hover:bg-opacity-50 transition-colors text-xs font-mono"
                style={{ backgroundColor: COLORS.HEADER_BG, color: COLORS.CYAN, border: `1px solid ${COLORS.BORDER}` }}
              >
                <Award className="w-3 h-3" />
                <span>{t('resources.commercial')}</span>
              </a>

              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/TRADEMARK.md"
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-center space-x-2 p-2 hover:bg-opacity-50 transition-colors text-xs font-mono"
                style={{ backgroundColor: COLORS.HEADER_BG, color: COLORS.CYAN, border: `1px solid ${COLORS.BORDER}` }}
              >
                <Shield className="w-3 h-3" />
                <span>{t('resources.trademarks')}</span>
              </a>

              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/CLA.md"
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-center space-x-2 p-2 hover:bg-opacity-50 transition-colors text-xs font-mono"
                style={{ backgroundColor: COLORS.HEADER_BG, color: COLORS.CYAN, border: `1px solid ${COLORS.BORDER}` }}
              >
                <FileText className="w-3 h-3" />
                <span>{t('resources.cla')}</span>
              </a>

              <a
                href="https://fincept.in"
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-center space-x-2 p-2 hover:bg-opacity-50 transition-colors text-xs font-mono"
                style={{ backgroundColor: COLORS.HEADER_BG, color: COLORS.CYAN, border: `1px solid ${COLORS.BORDER}` }}
              >
                <Globe className="w-3 h-3" />
                <span>{t('resources.website')}</span>
              </a>
            </div>
          </div>

          {/* Contact - Compact Grid */}
          <div style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }} className="p-4">
            <div className="flex items-center space-x-2 mb-3">
              <Mail className="w-4 h-4" style={{ color: COLORS.ORANGE }} />
              <h3 className="font-bold font-mono text-sm" style={{ color: COLORS.WHITE }}>{t('contact.title')}</h3>
            </div>
            <div className="grid grid-cols-2 gap-3 text-xs font-mono">
              <div>
                <div className="font-semibold mb-1" style={{ color: COLORS.GRAY }}>{t('contact.general')}</div>
                <a href="mailto:support@fincept.in" style={{ color: COLORS.CYAN }} className="hover:underline">
                  support@fincept.in
                </a>
              </div>
              <div>
                <div className="font-semibold mb-1" style={{ color: COLORS.GRAY }}>{t('contact.commercial')}</div>
                <a href="mailto:support@fincept.in" style={{ color: COLORS.CYAN }} className="hover:underline">
                  support@fincept.in
                </a>
              </div>
              <div>
                <div className="font-semibold mb-1" style={{ color: COLORS.GRAY }}>{t('contact.security')}</div>
                <a href="mailto:support@fincept.in" style={{ color: COLORS.CYAN }} className="hover:underline">
                  support@fincept.in
                </a>
              </div>
              <div>
                <div className="font-semibold mb-1" style={{ color: COLORS.GRAY }}>{t('contact.legal')}</div>
                <a href="mailto:support@fincept.in" style={{ color: COLORS.CYAN }} className="hover:underline">
                  support@fincept.in
                </a>
              </div>
            </div>
          </div>

        </div>
      </div>

      <TabFooter tabName="About" />
    </div>
  );
};

export default AboutTab;
