import React, { useState } from 'react';
import { Save, RefreshCw, Terminal, Type, Palette, Clock, Check } from 'lucide-react';
import { terminalThemeService, FONT_FAMILIES, COLOR_THEMES, FontSettings } from '@/services/core/terminalThemeService';
import { saveSetting } from '@/services/core/sqliteService';
import { useTranslation } from 'react-i18next';
import type { SettingsColors } from '../types';
import { DEFAULT_KEY_MAPPINGS } from '../hooks';

interface TerminalAppearanceSectionProps {
  colors: SettingsColors;
  theme: any;
  updateTheme: (theme: any) => void;
  resetTheme: () => void;
  showMessage: (type: 'success' | 'error', text: string) => void;
  keyMappings: Record<string, string>;
  setKeyMappings: (mappings: Record<string, string>) => void;
  defaultTimezone: { id: string; label: string; shortLabel: string };
  setDefaultTimezone: (id: string) => void;
  timezoneOptions: Array<{ id: string; label: string; shortLabel: string }>;
}

export function TerminalAppearanceSection({
  colors,
  theme,
  updateTheme,
  resetTheme,
  showMessage,
  keyMappings,
  setKeyMappings,
  defaultTimezone,
  setDefaultTimezone,
  timezoneOptions,
}: TerminalAppearanceSectionProps) {
  const { t } = useTranslation('settings');
  const [fontFamily, setFontFamily] = useState(theme.font.family);
  const [baseSize, setBaseSize] = useState(theme.font.baseSize);
  const [fontWeight, setFontWeight] = useState(theme.font.weight);
  const [fontItalic, setFontItalic] = useState(theme.font.italic);
  const [selectedTheme, setSelectedTheme] = useState(terminalThemeService.getCurrentThemeKey());

  const handleSaveTerminalAppearance = async () => {
    const newTheme = {
      font: {
        family: fontFamily,
        baseSize: baseSize,
        weight: fontWeight,
        italic: fontItalic
      },
      colors: COLOR_THEMES[selectedTheme]
    };
    updateTheme(newTheme);
    await saveSetting('fincept_key_mappings', JSON.stringify(keyMappings), 'settings');
    showMessage('success', 'Terminal appearance and key mappings saved successfully');
  };

  const handleResetTerminalAppearance = async () => {
    resetTheme();
    const defaultTheme = terminalThemeService.getTheme();
    setFontFamily(defaultTheme.font.family);
    setBaseSize(defaultTheme.font.baseSize);
    setFontWeight(defaultTheme.font.weight);
    setFontItalic(defaultTheme.font.italic);
    setSelectedTheme('fincept-classic');
    setKeyMappings(DEFAULT_KEY_MAPPINGS);
    await saveSetting('fincept_key_mappings', '', 'settings');
    showMessage('success', 'Reset to default appearance and key mappings');
  };

  return (
    <div style={{ fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        marginBottom: '24px',
        paddingBottom: '12px',
        borderBottom: `2px solid ${colors.primary}`
      }}>
        <Terminal size={20} color={colors.primary} style={{ filter: `drop-shadow(0 0 4px ${colors.primary})` }} />
        <h2 style={{
          color: colors.primary,
          fontSize: '16px',
          fontWeight: 700,
          letterSpacing: '1px',
          margin: 0,
          textShadow: `0 0 10px ${colors.primary}40`
        }}>
          {t('terminal.title')}
        </h2>
      </div>

      {/* Description */}
      <div style={{
        marginBottom: '20px',
        padding: '12px',
        backgroundColor: '#1A1A1A',
        border: '1px solid #2A2A2A',
        borderLeft: `3px solid ${colors.secondary}`
      }}>
        <p style={{
          color: colors.textMuted,
          fontSize: '10px',
          margin: 0,
          lineHeight: '1.6'
        }}>
          {t('terminal.description')}
        </p>
      </div>

      {/* Font Settings */}
      <div style={{
        background: '#0F0F0F',
        border: '1px solid #2A2A2A',
        borderLeft: `3px solid ${colors.primary}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '20px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
          <Type size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
          <span style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', letterSpacing: '0.5px' }}>{t('terminal.currentSettings')}</span>
        </div>

        {/* Font Family */}
        <div style={{ marginBottom: '16px' }}>
          <label style={{ color: colors.text, fontSize: '10px', display: 'block', marginBottom: '6px', fontWeight: 600, letterSpacing: '0.5px' }}>{t('terminal.fontFamily')}</label>
          <select value={fontFamily} onChange={(e) => setFontFamily(e.target.value)} style={{
            width: '100%',
            padding: '10px',
            background: colors.background,
            border: `1px solid #2A2A2A`,
            color: colors.text,
            borderRadius: '3px',
            fontSize: '11px',
            fontWeight: 500,
            cursor: 'pointer',
            appearance: 'none',
            backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23FFA500' d='M6 9L1 4h10z'/%3E%3C/svg%3E")`,
            backgroundRepeat: 'no-repeat',
            backgroundPosition: 'right 10px center',
            paddingRight: '32px'
          }}>
            {FONT_FAMILIES.map(f => <option key={f.value} value={f.value}>{f.label}</option>)}
          </select>
        </div>

        {/* Base Font Size */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
            <label style={{ color: colors.text, fontSize: '10px', fontWeight: 600, letterSpacing: '0.5px' }}>BASE FONT SIZE</label>
            <span style={{
              color: colors.primary,
              fontSize: '12px',
              fontWeight: 'bold',
              padding: '2px 8px',
              background: `${colors.primary}20`,
              borderRadius: '3px',
              border: `1px solid ${colors.primary}40`
            }}>{baseSize}px</span>
          </div>
          <input type="range" min="9" max="18" value={baseSize} onChange={(e) => setBaseSize(Number(e.target.value))} style={{
            width: '100%',
            height: '6px',
            background: `linear-gradient(to right, ${colors.primary}40 0%, ${colors.primary}40 ${((baseSize - 9) / 9) * 100}%, #2A2A2A ${((baseSize - 9) / 9) * 100}%, #2A2A2A 100%)`,
            borderRadius: '3px',
            outline: 'none',
            cursor: 'pointer'
          }} />
          <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '6px' }}>
            <span style={{ color: colors.textMuted, fontSize: '9px', fontWeight: 500 }}>Small (9px)</span>
            <span style={{ color: colors.textMuted, fontSize: '9px', fontWeight: 500 }}>Large (18px)</span>
          </div>
        </div>

        {/* Font Weight */}
        <div style={{ marginBottom: '16px' }}>
          <label style={{ color: colors.text, fontSize: '10px', display: 'block', marginBottom: '6px', fontWeight: 600, letterSpacing: '0.5px' }}>FONT WEIGHT</label>
          <select value={fontWeight} onChange={(e) => setFontWeight(e.target.value as FontSettings['weight'])} style={{
            width: '100%',
            padding: '10px',
            background: colors.background,
            border: '1px solid #2A2A2A',
            color: colors.text,
            borderRadius: '3px',
            fontSize: '11px',
            fontWeight: 500,
            cursor: 'pointer',
            appearance: 'none',
            backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23FFA500' d='M6 9L1 4h10z'/%3E%3C/svg%3E")`,
            backgroundRepeat: 'no-repeat',
            backgroundPosition: 'right 10px center',
            paddingRight: '32px'
          }}>
            <option value="normal">Normal (400)</option>
            <option value="semibold">Semi-Bold (600)</option>
            <option value="bold">Bold (700)</option>
          </select>
        </div>

        {/* Font Style */}
        <div>
          <label style={{
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
            cursor: 'pointer',
            padding: '10px',
            background: fontItalic ? `${colors.primary}10` : 'transparent',
            border: `1px solid ${fontItalic ? colors.primary : '#2A2A2A'}`,
            borderRadius: '3px',
            transition: 'all 0.2s'
          }}>
            <input type="checkbox" checked={fontItalic} onChange={(e) => setFontItalic(e.target.checked)} style={{ width: '16px', height: '16px', cursor: 'pointer' }} />
            <span style={{ color: colors.text, fontSize: '10px', fontWeight: 600, letterSpacing: '0.5px' }}>ENABLE ITALIC STYLE</span>
          </label>
        </div>
      </div>

      {/* Color Theme */}
      <div style={{
        background: '#0F0F0F',
        border: '1px solid #2A2A2A',
        borderLeft: `3px solid ${colors.primary}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '20px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
          <Palette size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
          <span style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', letterSpacing: '0.5px' }}>COLOR THEME</span>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', gap: '12px' }}>
          {Object.entries(COLOR_THEMES).map(([key, themeObj]) => {
            const isActive = selectedTheme === key;
            return (
              <button
                key={key}
                onClick={() => setSelectedTheme(key)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '12px',
                  padding: '12px',
                  background: isActive ? '#1A1A1A' : 'transparent',
                  border: `1px solid ${isActive ? colors.primary : '#2A2A2A'}`,
                  borderLeft: `3px solid ${isActive ? colors.primary : '#2A2A2A'}`,
                  borderRadius: '3px',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  position: 'relative',
                  overflow: 'hidden'
                }}
              >
                {/* Color Swatches */}
                <div style={{ display: 'flex', gap: '6px' }}>
                  <div style={{
                    width: '24px',
                    height: '24px',
                    background: themeObj.primary,
                    border: '1px solid #2A2A2A',
                    borderRadius: '3px',
                    boxShadow: isActive ? `0 0 8px ${themeObj.primary}60` : 'none'
                  }} title="Primary" />
                  <div style={{
                    width: '24px',
                    height: '24px',
                    background: themeObj.secondary,
                    border: '1px solid #2A2A2A',
                    borderRadius: '3px',
                    boxShadow: isActive ? `0 0 8px ${themeObj.secondary}60` : 'none'
                  }} title="Secondary" />
                  <div style={{
                    width: '24px',
                    height: '24px',
                    background: themeObj.alert,
                    border: '1px solid #2A2A2A',
                    borderRadius: '3px',
                    boxShadow: isActive ? `0 0 8px ${themeObj.alert}60` : 'none'
                  }} title="Alert" />
                </div>

                <span style={{
                  color: isActive ? colors.primary : colors.text,
                  fontSize: '11px',
                  flex: 1,
                  fontWeight: isActive ? 700 : 500,
                  letterSpacing: '0.5px',
                  textAlign: 'left'
                }}>{themeObj.name}</span>

                {isActive && (
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px'
                  }}>
                    <Check size={14} color={colors.success} />
                    <div style={{
                      width: '6px',
                      height: '6px',
                      borderRadius: '50%',
                      background: colors.success,
                      boxShadow: `0 0 6px ${colors.success}`
                    }} />
                  </div>
                )}

                {isActive && (
                  <div style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    right: 0,
                    height: '1px',
                    background: `linear-gradient(90deg, transparent, ${colors.primary}, transparent)`,
                    opacity: 0.5
                  }} />
                )}
              </button>
            );
          })}
        </div>
      </div>

      {/* Default Timezone Selection */}
      <div style={{
        background: '#0F0F0F',
        border: '1px solid #2A2A2A',
        borderLeft: `3px solid ${colors.primary}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '20px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
          <Clock size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
          <span style={{ color: colors.text, fontWeight: 'bold', fontSize: '12px' }}>
            DEFAULT TIMEZONE
          </span>
        </div>
        <p style={{ color: colors.textMuted, fontSize: '10px', marginBottom: '12px' }}>
          Set the default timezone displayed in the navigation bar. Dashboard widgets can use their own timezone settings.
        </p>
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(4, 1fr)',
          gap: '8px',
          maxHeight: '280px',
          overflowY: 'auto',
          paddingRight: '4px'
        }}>
          {timezoneOptions.map((tz) => (
            <button
              key={tz.id}
              onClick={() => setDefaultTimezone(tz.id)}
              style={{
                background: defaultTimezone.id === tz.id ? `${colors.primary}20` : '#1A1A1A',
                border: `1px solid ${defaultTimezone.id === tz.id ? colors.primary : '#2A2A2A'}`,
                borderRadius: '4px',
                padding: '12px 8px',
                cursor: 'pointer',
                textAlign: 'center',
                transition: 'all 0.2s'
              }}
            >
              <div style={{
                color: defaultTimezone.id === tz.id ? colors.primary : colors.text,
                fontWeight: 'bold',
                fontSize: '11px',
                marginBottom: '4px'
              }}>
                {tz.shortLabel}
              </div>
              <div style={{
                color: colors.textMuted,
                fontSize: '9px'
              }}>
                {tz.label.split('(')[0].trim()}
              </div>
            </button>
          ))}
        </div>
      </div>

      {/* Key Mapping Configuration */}
      <div style={{
        background: '#0F0F0F',
        border: '1px solid #2A2A2A',
        borderLeft: `3px solid ${colors.primary}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '20px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
          <Terminal size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
          <span style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', letterSpacing: '0.5px' }}>FUNCTION KEY MAPPINGS</span>
        </div>

        <div style={{
          marginBottom: '12px',
          padding: '10px',
          backgroundColor: '#1A1A1A',
          border: '1px solid #2A2A2A',
          borderRadius: '3px'
        }}>
          <p style={{
            color: colors.textMuted,
            fontSize: '9px',
            margin: 0,
            lineHeight: '1.5'
          }}>
            Configure which tab or action each function key (F1-F12) should trigger. Changes will be reflected in the status bar and keyboard shortcuts.
          </p>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: '12px' }}>
          {Object.entries(keyMappings).map(([key, action]) => (
            <div key={key} style={{
              background: '#1A1A1A',
              border: '1px solid #2A2A2A',
              borderRadius: '3px',
              padding: '12px'
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '8px' }}>
                <span style={{
                  backgroundColor: colors.primary,
                  color: '#000',
                  padding: '4px 8px',
                  fontSize: '9px',
                  fontWeight: 'bold',
                  borderRadius: '2px',
                  minWidth: '40px',
                  textAlign: 'center'
                }}>
                  {key}
                </span>
                <span style={{ color: colors.textMuted, fontSize: '10px' }}>→</span>
              </div>
              <select
                value={action}
                onChange={(e) => setKeyMappings({ ...keyMappings, [key]: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px',
                  background: colors.background,
                  border: '1px solid #2A2A2A',
                  color: colors.text,
                  borderRadius: '3px',
                  fontSize: '10px',
                  fontWeight: 500,
                  cursor: 'pointer',
                  appearance: 'none',
                  backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23FFA500' d='M6 9L1 4h10z'/%3E%3C/svg%3E")`,
                  backgroundRepeat: 'no-repeat',
                  backgroundPosition: 'right 8px center',
                  paddingRight: '28px'
                }}
              >
                <option value="dashboard">Dashboard</option>
                <option value="markets">Markets</option>
                <option value="news">News</option>
                <option value="portfolio">Portfolio</option>
                <option value="analytics">Analytics</option>
                <option value="watchlist">Watchlist</option>
                <option value="equity-research">Equity Research</option>
                <option value="screener">Screener</option>
                <option value="trading">Trading</option>
                <option value="chat">AI Chat</option>
                <option value="fullscreen">Toggle Fullscreen</option>
                <option value="profile">Profile</option>
                <option value="settings">Settings</option>
                <option value="forum">Forum</option>
                <option value="marketplace">Marketplace</option>
              </select>
            </div>
          ))}
        </div>
      </div>

      {/* Live Preview Panel */}
      <div style={{
        background: theme.colors.background,
        border: `2px solid ${theme.colors.primary}`,
        borderRadius: '4px',
        padding: '20px',
        marginBottom: '20px',
        position: 'relative',
        overflow: 'hidden'
      }}>
        <div style={{
          position: 'absolute',
          top: 0,
          left: 0,
          right: 0,
          height: '2px',
          background: `linear-gradient(90deg, transparent, ${theme.colors.primary}, transparent)`,
          opacity: 0.5
        }} />

        <div style={{
          color: theme.colors.textMuted,
          fontSize: '9px',
          fontWeight: 600,
          letterSpacing: '1px',
          marginBottom: '12px',
          opacity: 0.7
        }}>
          LIVE PREVIEW
        </div>

        <div style={{
          color: theme.colors.primary,
          fontSize: `${baseSize + 2}px`,
          fontFamily: `${fontFamily}, monospace`,
          fontWeight: fontWeight === 'normal' ? '400' : fontWeight === 'semibold' ? '600' : '700',
          fontStyle: fontItalic ? 'italic' : 'normal',
          marginBottom: '12px',
          letterSpacing: '0.5px'
        }}>
          FINCEPT TERMINAL PREVIEW
        </div>

        <div style={{
          color: theme.colors.text,
          fontSize: `${baseSize}px`,
          fontFamily: `${fontFamily}, monospace`,
          fontWeight: fontWeight === 'normal' ? '400' : fontWeight === 'semibold' ? '600' : '700',
          fontStyle: fontItalic ? 'italic' : 'normal',
          marginBottom: '8px',
          lineHeight: '1.5'
        }}>
          Body Text: Real-time market analysis and portfolio tracking
        </div>

        <div style={{
          color: theme.colors.textMuted,
          fontSize: `${baseSize - 1}px`,
          fontFamily: `${fontFamily}, monospace`,
          fontWeight: fontWeight === 'normal' ? '400' : fontWeight === 'semibold' ? '600' : '700',
          fontStyle: fontItalic ? 'italic' : 'normal',
          marginBottom: '12px',
          lineHeight: '1.5'
        }}>
          Small Text: Market data, timestamps, and metadata
        </div>

        <div style={{ display: 'flex', gap: '16px', marginTop: '12px', flexWrap: 'wrap' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <div style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              background: theme.colors.success,
              boxShadow: `0 0 8px ${theme.colors.success}`
            }} />
            <span style={{ color: theme.colors.success, fontSize: '10px', fontWeight: 600 }}>CONNECTED</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <div style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              background: theme.colors.alert,
              boxShadow: `0 0 8px ${theme.colors.alert}`
            }} />
            <span style={{ color: theme.colors.alert, fontSize: '10px', fontWeight: 600 }}>ALERT</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <div style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              background: theme.colors.warning,
              boxShadow: `0 0 8px ${theme.colors.warning}`
            }} />
            <span style={{ color: theme.colors.warning, fontSize: '10px', fontWeight: 600 }}>WARNING</span>
          </div>
        </div>
      </div>

      {/* Action Buttons */}
      <div style={{ display: 'flex', gap: '12px' }}>
        <button onClick={handleSaveTerminalAppearance} style={{
          flex: 1,
          background: colors.primary,
          color: '#000000',
          border: 'none',
          padding: '14px',
          fontSize: '11px',
          fontWeight: 'bold',
          letterSpacing: '0.5px',
          borderRadius: '4px',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '8px',
          transition: 'all 0.2s',
          boxShadow: `0 0 12px ${colors.primary}40`
        }}>
          <Save size={16} /> SAVE CONFIGURATION
        </button>
        <button onClick={handleResetTerminalAppearance} style={{
          background: 'transparent',
          color: colors.textMuted,
          border: `1px solid #2A2A2A`,
          padding: '14px 20px',
          fontSize: '11px',
          fontWeight: 'bold',
          letterSpacing: '0.5px',
          borderRadius: '4px',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          transition: 'all 0.2s'
        }}>
          <RefreshCw size={16} /> RESET
        </button>
      </div>
    </div>
  );
}
