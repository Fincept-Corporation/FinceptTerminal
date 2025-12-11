// File: src/components/settings/LanguageSelector.tsx
// Bloomberg Terminal-styled language selector

import React from 'react';
import { useLanguage } from '@/contexts/LanguageContext';
import { useTranslation } from 'react-i18next';
import { Globe, Check } from 'lucide-react';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

export const LanguageSelector: React.FC = () => {
  const { currentLanguage, changeLanguage, availableLanguages } = useLanguage();
  const { t } = useTranslation('settings');

  return (
    <div style={{
      fontFamily: '"IBM Plex Mono", "Consolas", monospace'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        marginBottom: '24px',
        paddingBottom: '12px',
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`
      }}>
        <Globe size={20} color={BLOOMBERG.ORANGE} style={{ filter: `drop-shadow(0 0 4px ${BLOOMBERG.ORANGE})` }} />
        <h2 style={{
          color: BLOOMBERG.ORANGE,
          fontSize: '16px',
          fontWeight: 700,
          letterSpacing: '1px',
          margin: 0,
          textShadow: `0 0 10px ${BLOOMBERG.ORANGE}40`
        }}>
          {t('language.title', 'LANGUAGE SETTINGS')}
        </h2>
      </div>

      {/* Description */}
      <div style={{
        marginBottom: '20px',
        padding: '12px',
        backgroundColor: BLOOMBERG.HEADER_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        borderLeft: `3px solid ${BLOOMBERG.CYAN}`
      }}>
        <p style={{
          color: BLOOMBERG.GRAY,
          fontSize: '10px',
          margin: 0,
          lineHeight: '1.6'
        }}>
          {t('language.selectLanguage', 'Select your preferred language for the terminal interface')}
        </p>
      </div>

      {/* Language Grid */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
        gap: '12px',
        marginBottom: '24px'
      }}>
        {availableLanguages.map((lang) => {
          const isActive = currentLanguage === lang.code;

          return (
            <button
              key={lang.code}
              onClick={() => changeLanguage(lang.code)}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '14px',
                backgroundColor: isActive ? BLOOMBERG.PANEL_BG : BLOOMBERG.HEADER_BG,
                border: `1px solid ${isActive ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                borderLeft: `3px solid ${isActive ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                cursor: 'pointer',
                transition: 'all 0.2s',
                position: 'relative',
                overflow: 'hidden'
              }}
              onMouseEnter={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                  e.currentTarget.style.borderColor = BLOOMBERG.MUTED;
                }
              }}
              onMouseLeave={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.HEADER_BG;
                  e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                }
              }}
            >
              {/* Flag */}
              <span style={{
                fontSize: '32px',
                lineHeight: 1,
                filter: isActive ? 'brightness(1.2)' : 'brightness(0.8)'
              }}>
                {lang.flag}
              </span>

              {/* Language Info */}
              <div style={{ flex: 1, textAlign: 'left' }}>
                <div style={{
                  color: isActive ? BLOOMBERG.ORANGE : BLOOMBERG.WHITE,
                  fontSize: '12px',
                  fontWeight: 700,
                  marginBottom: '2px',
                  letterSpacing: '0.5px'
                }}>
                  {lang.nativeName}
                </div>
                <div style={{
                  color: BLOOMBERG.GRAY,
                  fontSize: '9px',
                  fontWeight: 500
                }}>
                  {lang.name}
                </div>
              </div>

              {/* Active Indicator */}
              {isActive && (
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}>
                  <Check size={14} color={BLOOMBERG.GREEN} />
                  <div style={{
                    width: '8px',
                    height: '8px',
                    borderRadius: '50%',
                    backgroundColor: BLOOMBERG.GREEN,
                    boxShadow: `0 0 8px ${BLOOMBERG.GREEN}`
                  }} />
                </div>
              )}

              {/* Hover Glow Effect */}
              {isActive && (
                <div style={{
                  position: 'absolute',
                  top: 0,
                  left: 0,
                  right: 0,
                  height: '1px',
                  background: `linear-gradient(90deg, transparent, ${BLOOMBERG.ORANGE}, transparent)`,
                  opacity: 0.5
                }} />
              )}
            </button>
          );
        })}
      </div>

      {/* Current Language Status */}
      <div style={{
        padding: '16px',
        backgroundColor: BLOOMBERG.HEADER_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        borderTop: `2px solid ${BLOOMBERG.GREEN}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{
            width: '6px',
            height: '6px',
            borderRadius: '50%',
            backgroundColor: BLOOMBERG.GREEN,
            boxShadow: `0 0 8px ${BLOOMBERG.GREEN}`
          }} />
          <span style={{
            color: BLOOMBERG.GRAY,
            fontSize: '10px',
            fontWeight: 600,
            letterSpacing: '0.5px'
          }}>
            {t('language.currentLanguage', 'CURRENT LANGUAGE')}:
          </span>
        </div>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <span style={{
            fontSize: '18px'
          }}>
            {availableLanguages.find(lang => lang.code === currentLanguage)?.flag || '🌐'}
          </span>
          <span style={{
            color: BLOOMBERG.CYAN,
            fontSize: '12px',
            fontWeight: 700,
            letterSpacing: '0.5px'
          }}>
            {availableLanguages.find(lang => lang.code === currentLanguage)?.nativeName || 'English'}
          </span>
        </div>
      </div>

      {/* Info Panel */}
      <div style={{
        marginTop: '20px',
        padding: '12px 16px',
        backgroundColor: `${BLOOMBERG.CYAN}10`,
        border: `1px solid ${BLOOMBERG.CYAN}40`,
        borderLeft: `3px solid ${BLOOMBERG.CYAN}`
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'flex-start',
          gap: '10px'
        }}>
          <Globe size={14} color={BLOOMBERG.CYAN} style={{ marginTop: '1px', flexShrink: 0 }} />
          <div style={{
            color: BLOOMBERG.GRAY,
            fontSize: '9px',
            lineHeight: '1.5'
          }}>
            Language changes will apply immediately across the entire terminal interface.
            All menus, labels, and system messages will be displayed in the selected language.
          </div>
        </div>
      </div>

      {/* Statistics */}
      <div style={{
        marginTop: '20px',
        display: 'grid',
        gridTemplateColumns: 'repeat(3, 1fr)',
        gap: '12px'
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: BLOOMBERG.HEADER_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          textAlign: 'center'
        }}>
          <div style={{
            color: BLOOMBERG.YELLOW,
            fontSize: '20px',
            fontWeight: 700,
            marginBottom: '4px'
          }}>
            {availableLanguages.length}
          </div>
          <div style={{
            color: BLOOMBERG.GRAY,
            fontSize: '9px',
            fontWeight: 600,
            letterSpacing: '0.5px'
          }}>
            LANGUAGES
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: BLOOMBERG.HEADER_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          textAlign: 'center'
        }}>
          <div style={{
            color: BLOOMBERG.GREEN,
            fontSize: '20px',
            fontWeight: 700,
            marginBottom: '4px'
          }}>
            100%
          </div>
          <div style={{
            color: BLOOMBERG.GRAY,
            fontSize: '9px',
            fontWeight: 600,
            letterSpacing: '0.5px'
          }}>
            COVERAGE
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: BLOOMBERG.HEADER_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          textAlign: 'center'
        }}>
          <div style={{
            color: BLOOMBERG.CYAN,
            fontSize: '20px',
            fontWeight: 700,
            marginBottom: '4px'
          }}>
            RTL
          </div>
          <div style={{
            color: BLOOMBERG.GRAY,
            fontSize: '9px',
            fontWeight: 600,
            letterSpacing: '0.5px'
          }}>
            SUPPORTED
          </div>
        </div>
      </div>
    </div>
  );
};
