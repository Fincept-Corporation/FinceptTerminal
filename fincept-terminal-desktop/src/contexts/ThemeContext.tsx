import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react';
import { terminalThemeService, TerminalTheme, FontSizes, ColorTheme, LayoutSettings, EffectSettings } from '@/services/core/terminalThemeService';

interface ThemeContextValue {
  theme: TerminalTheme;
  colors: ColorTheme;
  fontSize: FontSizes;
  fontFamily: string;
  fontWeight: string;
  fontStyle: string;
  layout: LayoutSettings;
  effects: EffectSettings;
  updateTheme: (theme: TerminalTheme) => void;
  resetTheme: () => void;
}

const ThemeContext = createContext<ThemeContextValue | undefined>(undefined);

export const ThemeProvider: React.FC<{ children: ReactNode }> = ({ children }) => {
  const [theme, setTheme] = useState<TerminalTheme>(terminalThemeService.getTheme());

  // Update theme
  const updateTheme = (newTheme: TerminalTheme) => {
    terminalThemeService.saveTheme(newTheme);
    setTheme(newTheme);
  };

  // Reset theme to default
  const resetTheme = () => {
    terminalThemeService.resetToDefault();
    setTheme(terminalThemeService.getTheme());
  };

  // Computed values
  const colors = theme.colors;
  const fontSize = terminalThemeService.getFontSizes();
  const fontFamily = terminalThemeService.getFontFamily();
  const fontWeight = terminalThemeService.getFontWeight();
  const fontStyle = terminalThemeService.getFontStyle();
  const layout = theme.layout;
  const effects = theme.effects;

  // Sync theme values to CSS custom properties on :root
  useEffect(() => {
    const root = document.documentElement;

    // Font variables
    root.style.setProperty('--ft-font-family', fontFamily);
    root.style.setProperty('--ft-font-size-heading', fontSize.heading);
    root.style.setProperty('--ft-font-size-subheading', fontSize.subheading);
    root.style.setProperty('--ft-font-size-body', fontSize.body);
    root.style.setProperty('--ft-font-size-small', fontSize.small);
    root.style.setProperty('--ft-font-size-tiny', fontSize.tiny);
    root.style.setProperty('--ft-font-weight', fontWeight);
    root.style.setProperty('--ft-font-style', fontStyle);

    // Color variables
    root.style.setProperty('--ft-color-primary', colors.primary);
    root.style.setProperty('--ft-color-secondary', colors.secondary);
    root.style.setProperty('--ft-color-success', colors.success);
    root.style.setProperty('--ft-color-alert', colors.alert);
    root.style.setProperty('--ft-color-warning', colors.warning);
    root.style.setProperty('--ft-color-info', colors.info);
    root.style.setProperty('--ft-color-accent', colors.accent);
    root.style.setProperty('--ft-color-purple', colors.purple);
    root.style.setProperty('--ft-color-text', colors.text);
    root.style.setProperty('--ft-color-text-muted', colors.textMuted);
    root.style.setProperty('--ft-color-background', colors.background);
    root.style.setProperty('--ft-color-panel', colors.panel);

    // Layout variables
    root.style.setProperty('--ft-spacing-unit', `${terminalThemeService.getSpacingUnit()}px`);
    root.style.setProperty('--ft-border-radius', `${layout.borderRadius}px`);
    root.style.setProperty('--ft-border-width', terminalThemeService.getBorderWidth());
    root.style.setProperty('--ft-border-color', terminalThemeService.getBorderColor());
    root.style.setProperty('--ft-line-height', String(layout.lineHeight));
    root.style.setProperty('--ft-letter-spacing', `${layout.letterSpacing}px`);

    // Effect variables
    root.style.setProperty('--ft-widget-opacity', String(effects.widgetOpacity));
    root.style.setProperty('--ft-glow-enabled', effects.glowEnabled ? '1' : '0');
    root.style.setProperty('--ft-glow-intensity', String(effects.glowIntensity));
  }, [theme, colors, fontSize, fontFamily, fontWeight, fontStyle, layout, effects]);

  const value: ThemeContextValue = {
    theme,
    colors,
    fontSize,
    fontFamily,
    fontWeight,
    fontStyle,
    layout,
    effects,
    updateTheme,
    resetTheme
  };

  return (
    <ThemeContext.Provider value={value}>
      {children}
    </ThemeContext.Provider>
  );
};

// Custom hook to use theme
export const useTerminalTheme = () => {
  const context = useContext(ThemeContext);
  if (!context) {
    throw new Error('useTerminalTheme must be used within ThemeProvider');
  }
  return context;
};
