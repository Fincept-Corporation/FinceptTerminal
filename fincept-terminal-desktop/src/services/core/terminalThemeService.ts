// Terminal Theme Service
// Manages terminal appearance settings: fonts, sizes, colors

import { saveSetting, getSetting } from './sqliteService';

export interface FontSettings {
  family: string;
  baseSize: number;
  weight: 'normal' | 'semibold' | 'bold';
  italic: boolean;
}

export interface ColorTheme {
  name: string;
  isCustom?: boolean;
  primary: string;
  secondary: string;
  success: string;
  alert: string;
  warning: string;
  info: string;
  accent: string;
  purple: string;
  text: string;
  textMuted: string;
  background: string;
  panel: string;
}

export interface LayoutSettings {
  density: 'compact' | 'default' | 'comfortable';
  borderRadius: number;
  borderStyle: 'none' | 'subtle' | 'normal' | 'prominent';
  lineHeight: number;
  letterSpacing: number;
}

export interface EffectSettings {
  widgetOpacity: number;
  glowEnabled: boolean;
  glowIntensity: number;
}

export interface TerminalTheme {
  font: FontSettings;
  colors: ColorTheme;
  layout: LayoutSettings;
  effects: EffectSettings;
}

export interface FontSizes {
  heading: string;
  subheading: string;
  body: string;
  small: string;
  tiny: string;
}

// Predefined color themes
export const COLOR_THEMES: Record<string, ColorTheme> = {
  'fincept-classic': {
    name: 'Fincept Classic',
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
  private themeLoaded: boolean = false;

  constructor() {
    this.theme = this.getDefaultTheme();
    this.loadTheme();
  }

  // Get default theme
  private getDefaultTheme(): TerminalTheme {
    return {
      font: {
        family: 'Consolas',
        baseSize: 11,
        weight: 'normal',
        italic: false
      },
      colors: COLOR_THEMES['fincept-classic'],
      layout: {
        density: 'default',
        borderRadius: 2,
        borderStyle: 'normal',
        lineHeight: 1.4,
        letterSpacing: 0
      },
      effects: {
        widgetOpacity: 1,
        glowEnabled: false,
        glowIntensity: 0.5
      }
    };
  }

  // Load theme from storage or use defaults (async)
  private async loadTheme(): Promise<void> {
    try {
      const stored = await getSetting(STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored);
        const defaults = this.getDefaultTheme();
        this.theme = {
          font: {
            family: parsed.font?.family || defaults.font.family,
            baseSize: parsed.font?.baseSize || defaults.font.baseSize,
            weight: parsed.font?.weight || defaults.font.weight,
            italic: parsed.font?.italic || defaults.font.italic
          },
          colors: parsed.colors || defaults.colors,
          layout: {
            density: parsed.layout?.density || defaults.layout.density,
            borderRadius: parsed.layout?.borderRadius ?? defaults.layout.borderRadius,
            borderStyle: parsed.layout?.borderStyle || defaults.layout.borderStyle,
            lineHeight: parsed.layout?.lineHeight ?? defaults.layout.lineHeight,
            letterSpacing: parsed.layout?.letterSpacing ?? defaults.layout.letterSpacing
          },
          effects: {
            widgetOpacity: parsed.effects?.widgetOpacity ?? defaults.effects.widgetOpacity,
            glowEnabled: parsed.effects?.glowEnabled ?? defaults.effects.glowEnabled,
            glowIntensity: parsed.effects?.glowIntensity ?? defaults.effects.glowIntensity
          }
        };
      }
    } catch (error) {
      console.error('Failed to load theme from storage:', error);
    } finally {
      this.themeLoaded = true;
    }
  }

  // Save theme to storage
  async saveTheme(theme: TerminalTheme): Promise<void> {
    this.theme = theme;
    try {
      await saveSetting(STORAGE_KEY, JSON.stringify(theme), 'theme');
    } catch (error) {
      console.error('Failed to save theme to storage:', error);
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
    this.theme = this.getDefaultTheme();
    this.saveTheme(this.theme);
  }

  // Get spacing unit based on density
  getSpacingUnit(): number {
    const units = { compact: 4, default: 6, comfortable: 8 };
    return units[this.theme.layout.density];
  }

  // Get border width based on style
  getBorderWidth(): string {
    const widths = { none: '0px', subtle: '1px', normal: '1px', prominent: '2px' };
    return widths[this.theme.layout.borderStyle];
  }

  // Get border color based on style
  getBorderColor(): string {
    const colors = { none: 'transparent', subtle: '#1A1A1A', normal: '#2A2A2A', prominent: '#3A3A3A' };
    return colors[this.theme.layout.borderStyle];
  }

  // Get current color theme key
  getCurrentThemeKey(): string {
    const currentColors = this.theme.colors;
    for (const [key, theme] of Object.entries(COLOR_THEMES)) {
      if (theme.primary === currentColors.primary) {
        return key;
      }
    }
    return 'fincept-classic';
  }
}

export const terminalThemeService = new TerminalThemeService();
