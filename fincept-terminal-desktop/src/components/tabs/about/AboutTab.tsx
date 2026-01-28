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

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

const sectionHeaderStyle: React.CSSProperties = {
  padding: '12px',
  backgroundColor: FINCEPT.HEADER_BG,
  borderBottom: `1px solid ${FINCEPT.BORDER}`,
  display: 'flex',
  alignItems: 'center',
  gap: '8px',
};

const sectionLabelStyle: React.CSSProperties = {
  fontSize: '9px',
  fontWeight: 700,
  color: FINCEPT.GRAY,
  letterSpacing: '0.5px',
  textTransform: 'uppercase' as const,
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
};

const panelStyle: React.CSSProperties = {
  backgroundColor: FINCEPT.PANEL_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  borderRadius: '2px',
};

const bodyTextStyle: React.CSSProperties = {
  fontSize: '10px',
  color: FINCEPT.WHITE,
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
};

const mutedTextStyle: React.CSSProperties = {
  fontSize: '10px',
  color: FINCEPT.MUTED,
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
};

const bulletStyle: React.CSSProperties = {
  fontSize: '10px',
  color: FINCEPT.GRAY,
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
};

const linkStyle: React.CSSProperties = {
  color: FINCEPT.CYAN,
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
  textDecoration: 'none',
  display: 'inline-flex',
  alignItems: 'center',
  gap: '4px',
  transition: 'all 0.2s',
};

const resourceLinkStyle: React.CSSProperties = {
  backgroundColor: FINCEPT.HEADER_BG,
  border: `1px solid ${FINCEPT.BORDER}`,
  borderRadius: '2px',
  padding: '8px 12px',
  display: 'flex',
  alignItems: 'center',
  gap: '8px',
  color: FINCEPT.CYAN,
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
  textDecoration: 'none',
  transition: 'all 0.2s',
  cursor: 'pointer',
};

const BulletItem: React.FC<{ text: string }> = ({ text }) => (
  <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
    <span style={{ color: FINCEPT.ORANGE, fontSize: '10px', lineHeight: '16px' }}>&#x25A0;</span>
    <span style={bulletStyle}>{text}</span>
  </div>
);

const AboutTab: React.FC = () => {
  const { t } = useTranslation('about');

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.DARK_BG }}>
      <TabHeader
        title={t('title')}
        subtitle={t('subtitle')}
      />

      <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        <div style={{ maxWidth: '960px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '12px' }}>

          {/* Version Info Panel */}
          <div style={panelStyle}>
            <div style={sectionHeaderStyle}>
              <Info size={12} style={{ color: FINCEPT.ORANGE }} />
              <span style={sectionLabelStyle}>VERSION INFORMATION</span>
            </div>
            <div style={{ padding: '12px', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
              <div>
                <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
                  {VERSION_INFO.name}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED, fontFamily: '"IBM Plex Mono", "Consolas", monospace', marginTop: '4px', letterSpacing: '0.5px', textTransform: 'uppercase' }}>
                  {t('version.subtitle')}
                </div>
              </div>
              <div style={{ textAlign: 'right' }}>
                <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.ORANGE, fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
                  v{VERSION_INFO.version}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED, fontFamily: '"IBM Plex Mono", "Consolas", monospace', marginTop: '4px' }}>
                  {VERSION_INFO.buildDate}
                </div>
              </div>
            </div>
            <div style={{ padding: '8px 12px', borderTop: `1px solid ${FINCEPT.BORDER}`, backgroundColor: FINCEPT.HEADER_BG }}>
              <span style={{ fontSize: '9px', color: FINCEPT.MUTED, fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
                {VERSION_INFO.copyright}
              </span>
            </div>
          </div>

          {/* License Information - Two Column */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            {/* Open Source */}
            <div style={panelStyle}>
              <div style={sectionHeaderStyle}>
                <FileText size={12} style={{ color: FINCEPT.GREEN }} />
                <span style={sectionLabelStyle}>{t('openSource.title')}</span>
              </div>
              <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
                <BulletItem text={t('openSource.license')} />
                <BulletItem text={t('openSource.personal')} />
                <BulletItem text={t('openSource.sharing')} />
                <BulletItem text={t('openSource.network')} />
              </div>
              <div style={{ padding: '8px 12px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
                <a
                  href="https://www.gnu.org/licenses/agpl-3.0.html"
                  target="_blank"
                  rel="noopener noreferrer"
                  style={linkStyle}
                >
                  <span>gnu.org/licenses/agpl-3.0</span>
                  <ExternalLink size={10} />
                </a>
              </div>
            </div>

            {/* Commercial */}
            <div style={panelStyle}>
              <div style={sectionHeaderStyle}>
                <Award size={12} style={{ color: FINCEPT.ORANGE }} />
                <span style={sectionLabelStyle}>{t('commercial.title')}</span>
              </div>
              <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
                <BulletItem text={t('commercial.required')} />
                <BulletItem text={t('commercial.noSharing')} />
                <BulletItem text={t('commercial.support')} />
                <BulletItem text={t('commercial.custom')} />
              </div>
              <div style={{ padding: '8px 12px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
                <a
                  href="mailto:support@fincept.in?subject=Commercial License Inquiry"
                  style={linkStyle}
                >
                  <Mail size={10} />
                  <span>support@fincept.in</span>
                </a>
              </div>
            </div>
          </div>

          {/* Trademarks */}
          <div style={panelStyle}>
            <div style={sectionHeaderStyle}>
              <Shield size={12} style={{ color: FINCEPT.ORANGE }} />
              <span style={sectionLabelStyle}>{t('trademarks.title')}</span>
            </div>
            <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <p style={bodyTextStyle}>
                {t('trademarks.description')}
              </p>
              <p style={mutedTextStyle}>
                {t('trademarks.permission')}
              </p>
            </div>
          </div>

          {/* Resources Links */}
          <div style={panelStyle}>
            <div style={sectionHeaderStyle}>
              <Globe size={12} style={{ color: FINCEPT.ORANGE }} />
              <span style={sectionLabelStyle}>{t('resources.title')}</span>
            </div>
            <div style={{ padding: '12px', display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal"
                target="_blank"
                rel="noopener noreferrer"
                style={resourceLinkStyle}
              >
                <Github size={12} />
                <span>{t('resources.repository')}</span>
              </a>

              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE"
                target="_blank"
                rel="noopener noreferrer"
                style={resourceLinkStyle}
              >
                <FileText size={12} />
                <span>{t('resources.license')}</span>
              </a>

              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md"
                target="_blank"
                rel="noopener noreferrer"
                style={resourceLinkStyle}
              >
                <Award size={12} />
                <span>{t('resources.commercial')}</span>
              </a>

              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/TRADEMARK.md"
                target="_blank"
                rel="noopener noreferrer"
                style={resourceLinkStyle}
              >
                <Shield size={12} />
                <span>{t('resources.trademarks')}</span>
              </a>

              <a
                href="https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/CLA.md"
                target="_blank"
                rel="noopener noreferrer"
                style={resourceLinkStyle}
              >
                <FileText size={12} />
                <span>{t('resources.cla')}</span>
              </a>

              <a
                href="https://fincept.in"
                target="_blank"
                rel="noopener noreferrer"
                style={resourceLinkStyle}
              >
                <Globe size={12} />
                <span>{t('resources.website')}</span>
              </a>
            </div>
          </div>

          {/* Contact Grid */}
          <div style={panelStyle}>
            <div style={sectionHeaderStyle}>
              <Mail size={12} style={{ color: FINCEPT.ORANGE }} />
              <span style={sectionLabelStyle}>{t('contact.title')}</span>
            </div>
            <div style={{ padding: '12px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <div>
                <div style={{ ...sectionLabelStyle, marginBottom: '4px' }}>{t('contact.general')}</div>
                <a href="mailto:support@fincept.in" style={linkStyle}>
                  support@fincept.in
                </a>
              </div>
              <div>
                <div style={{ ...sectionLabelStyle, marginBottom: '4px' }}>{t('contact.commercial')}</div>
                <a href="mailto:support@fincept.in" style={linkStyle}>
                  support@fincept.in
                </a>
              </div>
              <div>
                <div style={{ ...sectionLabelStyle, marginBottom: '4px' }}>{t('contact.security')}</div>
                <a href="mailto:support@fincept.in" style={linkStyle}>
                  support@fincept.in
                </a>
              </div>
              <div>
                <div style={{ ...sectionLabelStyle, marginBottom: '4px' }}>{t('contact.legal')}</div>
                <a href="mailto:support@fincept.in" style={linkStyle}>
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
