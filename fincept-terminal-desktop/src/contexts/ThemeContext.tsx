import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react';
import { terminalThemeService, TerminalTheme, FontSizes, ColorTheme } from '@/services/terminalThemeService';

interface ThemeContextValue {
  theme: TerminalTheme;
  colors: ColorTheme;
  fontSize: FontSizes;
  fontFamily: string;
  fontWeight: string;
  fontStyle: string;
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

  const value: ThemeContextValue = {
    theme,
    colors,
    fontSize,
    fontFamily,
    fontWeight,
    fontStyle,
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
