import { terminalThemeService } from '@/services/terminalThemeService';

export const getColors = () => {
  const theme = terminalThemeService.getTheme();
  return {
    ORANGE: theme.colors.primary,
    WHITE: theme.colors.text,
    GREEN: theme.colors.secondary,
    BLUE: theme.colors.info,
    CYAN: theme.colors.accent,
    YELLOW: theme.colors.warning,
    GRAY: theme.colors.textMuted,
    DARK_BG: theme.colors.background,
    PANEL_BG: theme.colors.panel,
    CODE_BG: '#1a1a1a'
  };
};

export const COLORS = getColors();
