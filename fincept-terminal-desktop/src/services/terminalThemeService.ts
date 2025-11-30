// Terminal Theme Service
// Manages terminal appearance settings: fonts, sizes, colors

export interface FontSettings {
  family: string;
  baseSize: number;
  weight: 'normal' | 'semibold' | 'bold';
  italic: boolean;
}

export interface ColorTheme {
  name: string;
  primary: string;      // Replaces BLOOMBERG_ORANGE
  secondary: string;    // Replaces BLOOMBERG_GREEN
  success: string;      // Success/positive states (same as secondary)
  alert: string;        // Replaces BLOOMBERG_RED
  warning: string;      // BLOOMBERG_YELLOW
  info: string;         // BLOOMBERG_BLUE
  accent: string;       // BLOOMBERG_CYAN
  purple: string;       // BLOOMBERG_PURPLE
  text: string;         // BLOOMBERG_WHITE
  textMuted: string;    // BLOOMBERG_GRAY
  background: string;   // BLOOMBERG_DARK_BG
  panel: string;        // BLOOMBERG_PANEL_BG
}

export interface TerminalTheme {
  font: FontSettings;
  colors: ColorTheme;
}

export interface FontSizes {
  heading: string;      // H1
  subheading: string;   // H2
  body: string;         // Normal text
  small: string;        // Labels, metadata
  tiny: string;         // Very small text
}

// Predefined color themes
export const COLOR_THEMES: Record<string, ColorTheme> = {
  'bloomberg-classic': {
    name: 'Bloomberg Classic',
    primary: '#FFA500',
    secondary: '#00C800',
    success: '#00C800',
    alert: '#FF0000',
    warning: '#FFFF00',
    info: '#6496FA',
    accent: '#00FFFF',
    purple: '#C864FF',
    text: '#FFFFFF',
    textMuted: '#787878',
    background: '#000000',
    panel: '#0a0a0a'
  },
  'matrix-green': {
    name: 'Matrix Green',
    primary: '#00FF41',
    secondary: '#008F11',
    success: '#00FF41',
    alert: '#FF0000',
    warning: '#FFD700',
    info: '#00D4AA',
    accent: '#00FFAA',
    purple: '#00FF88',
    text: '#00FF41',
    textMuted: '#005500',
    background: '#000000',
    panel: '#001100'
  },
  'blue-terminal': {
    name: 'Blue Terminal',
    primary: '#00D4FF',
    secondary: '#00FF88',
    success: '#6BCF7F',
    alert: '#FF6B6B',
    warning: '#FFD93D',
    info: '#6BCF7F',
    accent: '#A0E7E5',
    purple: '#B4A7FF',
    text: '#E8F4F8',
    textMuted: '#5A7C8C',
    background: '#000814',
    panel: '#001D3D'
  },
  'amber-retro': {
    name: 'Amber Retro',
    primary: '#FFAA00',
    secondary: '#00FF00',
    success: '#00FF00',
    alert: '#FF0000',
    warning: '#FFCC00',
    info: '#88CCFF',
    accent: '#FFDD88',
    purple: '#FF88CC',
    text: '#FFBB33',
    textMuted: '#886600',
    background: '#000000',
    panel: '#0a0600'
  },
  'purple-neon': {
    name: 'Purple Neon',
    primary: '#C864FF',
    secondary: '#00FFA3',
    success: '#00FFA3',
    alert: '#FF4D6D',
    warning: '#FFD23F',
    info: '#64D2FF',
    accent: '#FF6AC1',
    purple: '#9B4FD8',
    text: '#F0E6FF',
    textMuted: '#8B7BA8',
    background: '#0D0221',
    panel: '#1A0B3C'
  }
};

// Font family options
export const FONT_FAMILIES = [
  { value: 'Consolas', label: 'Consolas' },
  { value: 'Courier New', label: 'Courier New' },
  { value: 'Monaco', label: 'Monaco' },
  { value: 'Fira Code', label: 'Fira Code' },
  { value: 'JetBrains Mono', label: 'JetBrains Mono' },
  { value: 'Source Code Pro', label: 'Source Code Pro' }
];

const STORAGE_KEY = 'terminal-theme-settings';

class TerminalThemeService {
  private theme: TerminalTheme;

  constructor() {
    this.theme = this.loadTheme();
  }

  // Load theme from localStorage or use defaults
  private loadTheme(): TerminalTheme {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored);
        // Merge with defaults to handle missing properties
        return {
          font: {
            family: parsed.font?.family || 'Consolas',
            baseSize: parsed.font?.baseSize || 11,
            weight: parsed.font?.weight || 'normal',
            italic: parsed.font?.italic || false
          },
          colors: parsed.colors || COLOR_THEMES['bloomberg-classic']
        };
      }
    } catch (error) {
      console.error('Failed to load theme from localStorage:', error);
    }

    // Default theme
    return {
      font: {
        family: 'Consolas',
        baseSize: 11,
        weight: 'normal',
        italic: false
      },
      colors: COLOR_THEMES['bloomberg-classic']
    };
  }

  // Save theme to localStorage
  saveTheme(theme: TerminalTheme): void {
    this.theme = theme;
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(theme));
    } catch (error) {
      console.error('Failed to save theme to localStorage:', error);
    }
  }

  // Get current theme
  getTheme(): TerminalTheme {
    return this.theme;
  }

  // Update font settings
  updateFont(font: Partial<FontSettings>): void {
    this.theme.font = { ...this.theme.font, ...font };
    this.saveTheme(this.theme);
  }

  // Update color theme
  updateColors(themeKey: string): void {
    if (COLOR_THEMES[themeKey]) {
      this.theme.colors = COLOR_THEMES[themeKey];
      this.saveTheme(this.theme);
    }
  }

  // Get calculated font sizes based on base size
  getFontSizes(): FontSizes {
    const base = this.theme.font.baseSize;
    return {
      heading: `${base + 4}px`,
      subheading: `${base + 2}px`,
      body: `${base}px`,
      small: `${base - 1}px`,
      tiny: `${base - 2}px`
    };
  }

  // Get font family with fallback
  getFontFamily(): string {
    return `${this.theme.font.family}, monospace`;
  }

  // Get font weight
  getFontWeight(): string {
    const weights = {
      normal: '400',
      semibold: '600',
      bold: '700'
    };
    return weights[this.theme.font.weight];
  }

  // Get font style
  getFontStyle(): string {
    return this.theme.font.italic ? 'italic' : 'normal';
  }

  // Reset to defaults
  resetToDefault(): void {
    this.theme = {
      font: {
        family: 'Consolas',
        baseSize: 11,
        weight: 'normal',
        italic: false
      },
      colors: COLOR_THEMES['bloomberg-classic']
    };
    this.saveTheme(this.theme);
  }

  // Get current color theme key
  getCurrentThemeKey(): string {
    const currentColors = this.theme.colors;
    for (const [key, theme] of Object.entries(COLOR_THEMES)) {
      if (theme.primary === currentColors.primary) {
        return key;
      }
    }
    return 'bloomberg-classic';
  }
}

export const terminalThemeService = new TerminalThemeService();
