import React, { useState } from 'react';
import { Save, RefreshCw, Terminal, Type, Palette, Clock, Check, Layout, Sparkles } from 'lucide-react';
import { terminalThemeService, FONT_FAMILIES, COLOR_THEMES, FontSettings, LayoutSettings, EffectSettings, ColorTheme } from '@/services/core/terminalThemeService';
import { saveSetting } from '@/services/core/sqliteService';
import { useTranslation } from 'react-i18next';
import type { SettingsColors } from '../types';
import { DEFAULT_KEY_MAPPINGS } from '../hooks';

// Fincept Design System Colors
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
};

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

  // Layout settings
  const [density, setDensity] = useState<LayoutSettings['density']>(theme.layout?.density || 'default');
  const [borderRadius, setBorderRadius] = useState(theme.layout?.borderRadius ?? 2);
  const [borderStyle, setBorderStyle] = useState<LayoutSettings['borderStyle']>(theme.layout?.borderStyle || 'normal');
  const [lineHeight, setLineHeight] = useState(theme.layout?.lineHeight ?? 1.4);
  const [letterSpacing, setLetterSpacing] = useState(theme.layout?.letterSpacing ?? 0);

  // Effect settings
  const [widgetOpacity, setWidgetOpacity] = useState(theme.effects?.widgetOpacity ?? 1);
  const [glowEnabled, setGlowEnabled] = useState(theme.effects?.glowEnabled ?? false);
  const [glowIntensity, setGlowIntensity] = useState(theme.effects?.glowIntensity ?? 0.5);

  // Custom color mode
  const [customColors, setCustomColors] = useState<ColorTheme>({
    ...COLOR_THEMES['fincept-classic'],
    ...(theme.colors?.isCustom ? theme.colors : {})
  });
  const [isCustomColorMode, setIsCustomColorMode] = useState(theme.colors?.isCustom || false);

  const handleSaveTerminalAppearance = async () => {
    const colorTheme = isCustomColorMode
      ? { ...customColors, isCustom: true }
      : COLOR_THEMES[selectedTheme];

    const newTheme = {
      font: {
        family: fontFamily,
        baseSize: baseSize,
        weight: fontWeight,
        italic: fontItalic
      },
      colors: colorTheme,
      layout: {
        density,
        borderRadius,
        borderStyle,
        lineHeight,
        letterSpacing
      },
      effects: {
        widgetOpacity,
        glowEnabled,
        glowIntensity
      }
    };
    updateTheme(newTheme);
    await saveSetting('fincept_key_mappings', JSON.stringify(keyMappings), 'settings');
    showMessage('success', t('terminal.saveSuccess'));
  };

  const handleResetTerminalAppearance = async () => {
    resetTheme();
    const defaultTheme = terminalThemeService.getTheme();
    setFontFamily(defaultTheme.font.family);
    setBaseSize(defaultTheme.font.baseSize);
    setFontWeight(defaultTheme.font.weight);
    setFontItalic(defaultTheme.font.italic);
    setSelectedTheme('fincept-classic');
    setIsCustomColorMode(false);
    setDensity(defaultTheme.layout.density);
    setBorderRadius(defaultTheme.layout.borderRadius);
    setBorderStyle(defaultTheme.layout.borderStyle);
    setLineHeight(defaultTheme.layout.lineHeight);
    setLetterSpacing(defaultTheme.layout.letterSpacing);
    setWidgetOpacity(defaultTheme.effects.widgetOpacity);
    setGlowEnabled(defaultTheme.effects.glowEnabled);
    setGlowIntensity(defaultTheme.effects.glowIntensity);
    setKeyMappings(DEFAULT_KEY_MAPPINGS);
    await saveSetting('fincept_key_mappings', '', 'settings');
    showMessage('success', t('terminal.resetSuccess'));
  };

  return (
    <div style={{ fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        marginBottom: '16px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
          <Terminal size={14} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            {t('terminal.title')}
          </span>
        </div>
        <p style={{
          fontSize: '10px',
          color: FINCEPT.GRAY,
          margin: 0,
          lineHeight: 1.5
        }}>
          {t('terminal.description')}
        </p>
      </div>

      {/* Font Settings */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        padding: '12px',
        marginBottom: '12px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '12px' }}>
          <Type size={12} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.WHITE,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            {t('terminal.currentSettings')}
          </span>
        </div>

        {/* Font Family */}
        <div style={{ marginBottom: '12px' }}>
          <label style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            display: 'block',
            marginBottom: '4px'
          }}>
            {t('terminal.fontFamily')}
          </label>
          <select value={fontFamily} onChange={(e) => setFontFamily(e.target.value)} style={{
            width: '100%',
            padding: '8px 10px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.WHITE,
            borderRadius: '2px',
            fontSize: '10px',
            fontFamily: '"IBM Plex Mono", monospace',
            cursor: 'pointer',
            outline: 'none'
          }}>
            {FONT_FAMILIES.map(f => <option key={f.value} value={f.value}>{f.label}</option>)}
          </select>
        </div>

        {/* Base Font Size */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
            <label style={{ color: colors.text, fontSize: 'var(--ft-font-size-small)', fontWeight: 600, letterSpacing: '0.5px' }}>{t('terminal.baseFontSize')}</label>
            <span style={{
              color: colors.primary,
              fontSize: 'var(--ft-font-size-subheading)',
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
            <span style={{ color: colors.textMuted, fontSize: 'var(--ft-font-size-tiny)', fontWeight: 500 }}>{t('terminal.fontSizeSmall')}</span>
            <span style={{ color: colors.textMuted, fontSize: 'var(--ft-font-size-tiny)', fontWeight: 500 }}>{t('terminal.fontSizeLarge')}</span>
          </div>
        </div>

        {/* Font Weight */}
        <div style={{ marginBottom: '16px' }}>
          <label style={{ color: colors.text, fontSize: 'var(--ft-font-size-small)', display: 'block', marginBottom: '6px', fontWeight: 600, letterSpacing: '0.5px' }}>{t('terminal.fontWeight')}</label>
          <select value={fontWeight} onChange={(e) => setFontWeight(e.target.value as FontSettings['weight'])} style={{
            width: '100%',
            padding: '10px',
            background: colors.background,
            border: '1px solid #2A2A2A',
            color: colors.text,
            borderRadius: '3px',
            fontSize: 'var(--ft-font-size-body)',
            fontWeight: 500,
            cursor: 'pointer',
            appearance: 'none',
            backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23FFA500' d='M6 9L1 4h10z'/%3E%3C/svg%3E")`,
            backgroundRepeat: 'no-repeat',
            backgroundPosition: 'right 10px center',
            paddingRight: '32px'
          }}>
            <option value="normal">{t('terminal.weightNormal')}</option>
            <option value="semibold">{t('terminal.weightSemibold')}</option>
            <option value="bold">{t('terminal.weightBold')}</option>
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
            <span style={{ color: colors.text, fontSize: 'var(--ft-font-size-small)', fontWeight: 600, letterSpacing: '0.5px' }}>{t('terminal.enableItalic')}</span>
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
          <span style={{ color: colors.primary, fontSize: 'var(--ft-font-size-subheading)', fontWeight: 'bold', letterSpacing: '0.5px' }}>{t('terminal.colorTheme')}</span>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', gap: '12px' }}>
          {Object.entries(COLOR_THEMES).map(([key, themeObj]) => {
            const isActive = selectedTheme === key && !isCustomColorMode;
            return (
              <button
                key={key}
                onClick={() => { setSelectedTheme(key); setIsCustomColorMode(false); }}
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
                  fontSize: 'var(--ft-font-size-body)',
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

          {/* Custom Colors Button */}
          <button
            onClick={() => setIsCustomColorMode(true)}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '12px',
              padding: '12px',
              background: isCustomColorMode ? '#1A1A1A' : 'transparent',
              border: `1px solid ${isCustomColorMode ? colors.primary : '#2A2A2A'}`,
              borderLeft: `3px solid ${isCustomColorMode ? colors.primary : '#2A2A2A'}`,
              borderRadius: '3px',
              cursor: 'pointer',
              transition: 'all 0.2s',
              position: 'relative',
              overflow: 'hidden'
            }}
          >
            <div style={{ display: 'flex', gap: '6px' }}>
              <div style={{
                width: '24px',
                height: '24px',
                background: 'conic-gradient(#FF0000, #FF8800, #FFFF00, #00FF00, #0088FF, #8800FF, #FF0000)',
                border: '1px solid #2A2A2A',
                borderRadius: '3px',
                boxShadow: isCustomColorMode ? '0 0 8px rgba(255,255,255,0.3)' : 'none'
              }} title="Custom" />
            </div>
            <span style={{
              color: isCustomColorMode ? colors.primary : colors.text,
              fontSize: 'var(--ft-font-size-body)',
              flex: 1,
              fontWeight: isCustomColorMode ? 700 : 500,
              letterSpacing: '0.5px',
              textAlign: 'left'
            }}>{t('terminal.customColors')}</span>
            {isCustomColorMode && (
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
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
            {isCustomColorMode && (
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
        </div>
      </div>

      {/* Custom Color Picker */}
      {isCustomColorMode && (
        <div style={{
          background: '#0F0F0F',
          border: '1px solid #2A2A2A',
          borderLeft: `3px solid ${colors.primary}`,
          borderRadius: '4px',
          padding: '16px',
          marginBottom: '20px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
            <Palette size={18} color={colors.primary} />
            <span style={{ color: colors.primary, fontSize: 'var(--ft-font-size-subheading)', fontWeight: 'bold' }}>
              {t('terminal.customColorsTitle')}
            </span>
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '10px' }}>
            {([
              { key: 'primary', label: t('terminal.colorPrimary') },
              { key: 'secondary', label: t('terminal.colorSecondary') },
              { key: 'success', label: t('terminal.colorSuccess') },
              { key: 'alert', label: t('terminal.colorAlert') },
              { key: 'warning', label: t('terminal.colorWarning') },
              { key: 'info', label: t('terminal.colorInfo') },
              { key: 'accent', label: t('terminal.colorAccent') },
              { key: 'purple', label: t('terminal.colorPurple') },
              { key: 'text', label: t('terminal.colorText') },
              { key: 'textMuted', label: t('terminal.colorTextMuted') },
              { key: 'background', label: t('terminal.colorBackground') },
              { key: 'panel', label: t('terminal.colorPanel') },
            ] as { key: keyof ColorTheme; label: string }[]).map(({ key, label }) => (
              <div key={key} style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <input
                  type="color"
                  value={customColors[key] as string || '#000000'}
                  onChange={(e) => setCustomColors({ ...customColors, [key]: e.target.value })}
                  style={{ width: '28px', height: '28px', border: '1px solid #2A2A2A', borderRadius: '2px', cursor: 'pointer', background: 'none' }}
                />
                <div style={{ flex: 1 }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, textTransform: 'uppercase', letterSpacing: '0.5px' }}>{label}</div>
                  <input
                    type="text"
                    value={customColors[key] as string || ''}
                    onChange={(e) => setCustomColors({ ...customColors, [key]: e.target.value })}
                    style={{ width: '100%', background: '#000', border: '1px solid #2A2A2A', color: '#FFF', fontSize: '9px', padding: '3px 6px', fontFamily: 'monospace' }}
                  />
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Layout & Spacing */}
      <div style={{
        background: '#0F0F0F',
        border: '1px solid #2A2A2A',
        borderLeft: `3px solid ${colors.primary}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '20px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
          <Layout size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
          <span style={{ color: colors.primary, fontSize: 'var(--ft-font-size-subheading)', fontWeight: 'bold', letterSpacing: '0.5px' }}>
            {t('terminal.layoutSpacing')}
          </span>
        </div>

        {/* Content Density */}
        <div style={{ marginBottom: '16px' }}>
          <label style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textTransform: 'uppercase', display: 'block', marginBottom: '6px' }}>
            {t('terminal.contentDensity')}
          </label>
          <div style={{ display: 'flex', gap: '8px' }}>
            {(['compact', 'default', 'comfortable'] as const).map((d) => (
              <button
                key={d}
                onClick={() => setDensity(d)}
                style={{
                  flex: 1,
                  padding: '8px',
                  background: density === d ? `${colors.primary}20` : '#1A1A1A',
                  border: `1px solid ${density === d ? colors.primary : '#2A2A2A'}`,
                  color: density === d ? colors.primary : FINCEPT.WHITE,
                  fontSize: '10px',
                  fontWeight: density === d ? 700 : 500,
                  cursor: 'pointer',
                  borderRadius: '2px'
                }}
              >
                {t(`terminal.density${d.charAt(0).toUpperCase() + d.slice(1)}`)}
              </button>
            ))}
          </div>
        </div>

        {/* Border Radius */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
            <label style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textTransform: 'uppercase' }}>
              {t('terminal.borderRadius')}
            </label>
            <span style={{ color: colors.primary, fontSize: '10px', fontWeight: 'bold' }}>{borderRadius}px</span>
          </div>
          <input type="range" min="0" max="12" value={borderRadius} onChange={(e) => setBorderRadius(Number(e.target.value))} style={{
            width: '100%', height: '6px',
            background: `linear-gradient(to right, ${colors.primary}40 0%, ${colors.primary}40 ${(borderRadius / 12) * 100}%, #2A2A2A ${(borderRadius / 12) * 100}%, #2A2A2A 100%)`,
            borderRadius: '3px', outline: 'none', cursor: 'pointer'
          }} />
        </div>

        {/* Border Style */}
        <div style={{ marginBottom: '16px' }}>
          <label style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textTransform: 'uppercase', display: 'block', marginBottom: '6px' }}>
            {t('terminal.borderStyle')}
          </label>
          <div style={{ display: 'flex', gap: '8px' }}>
            {(['none', 'subtle', 'normal', 'prominent'] as const).map((s) => (
              <button
                key={s}
                onClick={() => setBorderStyle(s)}
                style={{
                  flex: 1,
                  padding: '8px',
                  background: borderStyle === s ? `${colors.primary}20` : '#1A1A1A',
                  border: `1px solid ${borderStyle === s ? colors.primary : '#2A2A2A'}`,
                  color: borderStyle === s ? colors.primary : FINCEPT.WHITE,
                  fontSize: '9px',
                  fontWeight: borderStyle === s ? 700 : 500,
                  cursor: 'pointer',
                  borderRadius: '2px'
                }}
              >
                {t(`terminal.border${s.charAt(0).toUpperCase() + s.slice(1)}`)}
              </button>
            ))}
          </div>
        </div>

        {/* Line Height */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
            <label style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textTransform: 'uppercase' }}>
              {t('terminal.lineHeight')}
            </label>
            <span style={{ color: colors.primary, fontSize: '10px', fontWeight: 'bold' }}>{lineHeight.toFixed(1)}</span>
          </div>
          <input type="range" min="12" max="20" value={lineHeight * 10} onChange={(e) => setLineHeight(Number(e.target.value) / 10)} style={{
            width: '100%', height: '6px',
            background: `linear-gradient(to right, ${colors.primary}40 0%, ${colors.primary}40 ${((lineHeight - 1.2) / 0.8) * 100}%, #2A2A2A ${((lineHeight - 1.2) / 0.8) * 100}%, #2A2A2A 100%)`,
            borderRadius: '3px', outline: 'none', cursor: 'pointer'
          }} />
        </div>

        {/* Letter Spacing */}
        <div>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
            <label style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textTransform: 'uppercase' }}>
              {t('terminal.letterSpacing')}
            </label>
            <span style={{ color: colors.primary, fontSize: '10px', fontWeight: 'bold' }}>{letterSpacing.toFixed(1)}px</span>
          </div>
          <input type="range" min="0" max="20" value={letterSpacing * 10} onChange={(e) => setLetterSpacing(Number(e.target.value) / 10)} style={{
            width: '100%', height: '6px',
            background: `linear-gradient(to right, ${colors.primary}40 0%, ${colors.primary}40 ${(letterSpacing / 2) * 100}%, #2A2A2A ${(letterSpacing / 2) * 100}%, #2A2A2A 100%)`,
            borderRadius: '3px', outline: 'none', cursor: 'pointer'
          }} />
        </div>
      </div>

      {/* Effects */}
      <div style={{
        background: '#0F0F0F',
        border: '1px solid #2A2A2A',
        borderLeft: `3px solid ${colors.primary}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '20px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
          <Sparkles size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
          <span style={{ color: colors.primary, fontSize: 'var(--ft-font-size-subheading)', fontWeight: 'bold', letterSpacing: '0.5px' }}>
            {t('terminal.effects')}
          </span>
        </div>

        {/* Widget Opacity */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
            <label style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textTransform: 'uppercase' }}>
              {t('terminal.widgetOpacity')}
            </label>
            <span style={{ color: colors.primary, fontSize: '10px', fontWeight: 'bold' }}>{Math.round(widgetOpacity * 100)}%</span>
          </div>
          <input type="range" min="50" max="100" value={widgetOpacity * 100} onChange={(e) => setWidgetOpacity(Number(e.target.value) / 100)} style={{
            width: '100%', height: '6px',
            background: `linear-gradient(to right, ${colors.primary}40 0%, ${colors.primary}40 ${((widgetOpacity - 0.5) / 0.5) * 100}%, #2A2A2A ${((widgetOpacity - 0.5) / 0.5) * 100}%, #2A2A2A 100%)`,
            borderRadius: '3px', outline: 'none', cursor: 'pointer'
          }} />
        </div>

        {/* Glow Toggle */}
        <div style={{ marginBottom: glowEnabled ? '16px' : '0' }}>
          <label style={{
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
            cursor: 'pointer',
            padding: '10px',
            background: glowEnabled ? `${colors.primary}10` : 'transparent',
            border: `1px solid ${glowEnabled ? colors.primary : '#2A2A2A'}`,
            borderRadius: '3px',
            transition: 'all 0.2s'
          }}>
            <input type="checkbox" checked={glowEnabled} onChange={(e) => setGlowEnabled(e.target.checked)} style={{ width: '16px', height: '16px', cursor: 'pointer' }} />
            <span style={{ color: colors.text, fontSize: 'var(--ft-font-size-small)', fontWeight: 600, letterSpacing: '0.5px' }}>
              {t('terminal.enableGlow')}
            </span>
          </label>
        </div>

        {/* Glow Intensity (only when enabled) */}
        {glowEnabled && (
          <div>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
              <label style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textTransform: 'uppercase' }}>
                {t('terminal.glowIntensity')}
              </label>
              <span style={{ color: colors.primary, fontSize: '10px', fontWeight: 'bold' }}>{Math.round(glowIntensity * 100)}%</span>
            </div>
            <input type="range" min="10" max="100" value={glowIntensity * 100} onChange={(e) => setGlowIntensity(Number(e.target.value) / 100)} style={{
              width: '100%', height: '6px',
              background: `linear-gradient(to right, ${colors.primary}40 0%, ${colors.primary}40 ${glowIntensity * 100}%, #2A2A2A ${glowIntensity * 100}%, #2A2A2A 100%)`,
              borderRadius: '3px', outline: 'none', cursor: 'pointer'
            }} />
          </div>
        )}
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
          <span style={{ color: colors.text, fontWeight: 'bold', fontSize: 'var(--ft-font-size-subheading)' }}>
            {t('terminal.defaultTimezone')}
          </span>
        </div>
        <p style={{ color: colors.textMuted, fontSize: 'var(--ft-font-size-small)', marginBottom: '12px' }}>
          {t('terminal.timezoneDescription')}
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
                fontSize: 'var(--ft-font-size-body)',
                marginBottom: '4px'
              }}>
                {tz.shortLabel}
              </div>
              <div style={{
                color: colors.textMuted,
                fontSize: 'var(--ft-font-size-tiny)'
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
          <span style={{ color: colors.primary, fontSize: 'var(--ft-font-size-subheading)', fontWeight: 'bold', letterSpacing: '0.5px' }}>{t('terminal.functionKeys')}</span>
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
            fontSize: 'var(--ft-font-size-tiny)',
            margin: 0,
            lineHeight: '1.5'
          }}>
            {t('terminal.functionKeysDescription')}
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
                  fontSize: 'var(--ft-font-size-tiny)',
                  fontWeight: 'bold',
                  borderRadius: '2px',
                  minWidth: '40px',
                  textAlign: 'center'
                }}>
                  {key}
                </span>
                <span style={{ color: colors.textMuted, fontSize: 'var(--ft-font-size-small)' }}>→</span>
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
                  fontSize: 'var(--ft-font-size-small)',
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
        background: (isCustomColorMode ? customColors.background : COLOR_THEMES[selectedTheme]?.background) || theme.colors.background,
        border: `2px solid ${(isCustomColorMode ? customColors.primary : COLOR_THEMES[selectedTheme]?.primary) || theme.colors.primary}`,
        borderRadius: `${borderRadius}px`,
        padding: '20px',
        marginBottom: '20px',
        position: 'relative',
        overflow: 'hidden',
        opacity: widgetOpacity
      }}>
        {glowEnabled && (
          <div style={{
            position: 'absolute',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            boxShadow: `inset 0 0 ${20 * glowIntensity}px ${((isCustomColorMode ? customColors.primary : COLOR_THEMES[selectedTheme]?.primary) || theme.colors.primary)}30`,
            pointerEvents: 'none'
          }} />
        )}
        <div style={{
          position: 'absolute',
          top: 0,
          left: 0,
          right: 0,
          height: '2px',
          background: `linear-gradient(90deg, transparent, ${(isCustomColorMode ? customColors.primary : COLOR_THEMES[selectedTheme]?.primary) || theme.colors.primary}, transparent)`,
          opacity: 0.5
        }} />

        <div style={{
          color: (isCustomColorMode ? customColors.textMuted : COLOR_THEMES[selectedTheme]?.textMuted) || theme.colors.textMuted,
          fontSize: 'var(--ft-font-size-tiny)',
          fontWeight: 600,
          letterSpacing: '1px',
          marginBottom: '12px',
          opacity: 0.7
        }}>
          {t('terminal.livePreview')}
        </div>

        <div style={{
          color: (isCustomColorMode ? customColors.primary : COLOR_THEMES[selectedTheme]?.primary) || theme.colors.primary,
          fontSize: `${baseSize + 2}px`,
          fontFamily: `${fontFamily}, monospace`,
          fontWeight: fontWeight === 'normal' ? '400' : fontWeight === 'semibold' ? '600' : '700',
          fontStyle: fontItalic ? 'italic' : 'normal',
          marginBottom: '12px',
          letterSpacing: `${letterSpacing}px`,
          lineHeight: lineHeight,
          textShadow: glowEnabled ? `0 0 ${6 * glowIntensity}px ${(isCustomColorMode ? customColors.primary : COLOR_THEMES[selectedTheme]?.primary) || theme.colors.primary}` : 'none'
        }}>
          {t('terminal.previewTitle')}
        </div>

        <div style={{
          color: (isCustomColorMode ? customColors.text : COLOR_THEMES[selectedTheme]?.text) || theme.colors.text,
          fontSize: `${baseSize}px`,
          fontFamily: `${fontFamily}, monospace`,
          fontWeight: fontWeight === 'normal' ? '400' : fontWeight === 'semibold' ? '600' : '700',
          fontStyle: fontItalic ? 'italic' : 'normal',
          marginBottom: '8px',
          lineHeight: lineHeight,
          letterSpacing: `${letterSpacing}px`
        }}>
          {t('terminal.previewBody')}
        </div>

        <div style={{
          color: (isCustomColorMode ? customColors.textMuted : COLOR_THEMES[selectedTheme]?.textMuted) || theme.colors.textMuted,
          fontSize: `${baseSize - 1}px`,
          fontFamily: `${fontFamily}, monospace`,
          fontWeight: fontWeight === 'normal' ? '400' : fontWeight === 'semibold' ? '600' : '700',
          fontStyle: fontItalic ? 'italic' : 'normal',
          marginBottom: '12px',
          lineHeight: lineHeight,
          letterSpacing: `${letterSpacing}px`
        }}>
          {t('terminal.previewSmall')}
        </div>

        <div style={{ display: 'flex', gap: '16px', marginTop: '12px', flexWrap: 'wrap' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <div style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              background: (isCustomColorMode ? customColors.success : COLOR_THEMES[selectedTheme]?.success) || theme.colors.success,
              boxShadow: glowEnabled ? `0 0 ${8 * glowIntensity}px ${(isCustomColorMode ? customColors.success : COLOR_THEMES[selectedTheme]?.success) || theme.colors.success}` : `0 0 8px ${theme.colors.success}`
            }} />
            <span style={{ color: (isCustomColorMode ? customColors.success : COLOR_THEMES[selectedTheme]?.success) || theme.colors.success, fontSize: 'var(--ft-font-size-small)', fontWeight: 600 }}>{t('terminal.statusConnected')}</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <div style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              background: (isCustomColorMode ? customColors.alert : COLOR_THEMES[selectedTheme]?.alert) || theme.colors.alert,
              boxShadow: glowEnabled ? `0 0 ${8 * glowIntensity}px ${(isCustomColorMode ? customColors.alert : COLOR_THEMES[selectedTheme]?.alert) || theme.colors.alert}` : `0 0 8px ${theme.colors.alert}`
            }} />
            <span style={{ color: (isCustomColorMode ? customColors.alert : COLOR_THEMES[selectedTheme]?.alert) || theme.colors.alert, fontSize: 'var(--ft-font-size-small)', fontWeight: 600 }}>{t('terminal.statusAlert')}</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <div style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              background: (isCustomColorMode ? customColors.warning : COLOR_THEMES[selectedTheme]?.warning) || theme.colors.warning,
              boxShadow: glowEnabled ? `0 0 ${8 * glowIntensity}px ${(isCustomColorMode ? customColors.warning : COLOR_THEMES[selectedTheme]?.warning) || theme.colors.warning}` : `0 0 8px ${theme.colors.warning}`
            }} />
            <span style={{ color: (isCustomColorMode ? customColors.warning : COLOR_THEMES[selectedTheme]?.warning) || theme.colors.warning, fontSize: 'var(--ft-font-size-small)', fontWeight: 600 }}>{t('terminal.statusWarning')}</span>
          </div>
        </div>
      </div>

      {/* Action Buttons */}
      <div style={{ display: 'flex', gap: '8px' }}>
        {/* Primary Button */}
        <button onClick={handleSaveTerminalAppearance} style={{
          flex: 1,
          padding: '8px 16px',
          backgroundColor: FINCEPT.ORANGE,
          color: FINCEPT.DARK_BG,
          border: 'none',
          borderRadius: '2px',
          fontSize: '9px',
          fontWeight: 700,
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '6px',
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
          transition: 'all 0.2s'
        }}>
          <Save size={12} /> {t('terminal.saveConfig')}
        </button>
        {/* Secondary Button */}
        <button onClick={handleResetTerminalAppearance} style={{
          padding: '8px 16px',
          backgroundColor: 'transparent',
          border: `1px solid ${FINCEPT.BORDER}`,
          color: FINCEPT.GRAY,
          borderRadius: '2px',
          fontSize: '9px',
          fontWeight: 700,
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
          transition: 'all 0.2s',
          whiteSpace: 'nowrap'
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.borderColor = FINCEPT.ORANGE;
          e.currentTarget.style.color = FINCEPT.WHITE;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.borderColor = FINCEPT.BORDER;
          e.currentTarget.style.color = FINCEPT.GRAY;
        }}>
          <RefreshCw size={12} /> {t('terminal.reset')}
        </button>
      </div>
    </div>
  );
}
