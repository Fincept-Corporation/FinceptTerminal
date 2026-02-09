/**
 * Fortitudo Panel Constants
 * Color palette, sample data, and configurations
 */

// Fincept Professional Color Palette
// Components should prefer useTerminalTheme() hook for runtime values
export const FINCEPT = {
  ORANGE: 'var(--ft-color-primary, #FF8800)',
  WHITE: 'var(--ft-color-text, #FFFFFF)',
  RED: 'var(--ft-color-alert, #FF3B3B)',
  GREEN: 'var(--ft-color-success, #00D66F)',
  GRAY: 'var(--ft-color-text-muted, #787878)',
  DARK_BG: 'var(--ft-color-background, #000000)',
  PANEL_BG: 'var(--ft-color-panel, #0F0F0F)',
  HEADER_BG: 'var(--ft-color-panel, #1A1A1A)',
  CYAN: 'var(--ft-color-accent, #00E5FF)',
  YELLOW: 'var(--ft-color-warning, #FFD700)',
  BLUE: 'var(--ft-color-info, #0088FF)',
  PURPLE: 'var(--ft-color-purple, #9D4EDD)',
  BORDER: 'var(--ft-border-color, #2A2A2A)',
  HOVER: '#1F1F1F',
  MUTED: 'var(--ft-color-text-muted, #4A4A4A)'
};

// Pie chart colors for allocations
export const PIE_COLORS = [
  FINCEPT.ORANGE,
  FINCEPT.CYAN,
  FINCEPT.GREEN,
  FINCEPT.PURPLE,
  FINCEPT.YELLOW,
  FINCEPT.BLUE,
  FINCEPT.RED
];

// Sample portfolio data for demonstration
export const SAMPLE_RETURNS: Record<string, Record<string, number>> = {
  'Stocks': Object.fromEntries(
    Array.from({ length: 200 }, (_, i) => [
      new Date(2024, 0, i + 1).toISOString().split('T')[0],
      0.0003 + (Math.random() - 0.5) * 0.02
    ])
  ),
  'Bonds': Object.fromEntries(
    Array.from({ length: 200 }, (_, i) => [
      new Date(2024, 0, i + 1).toISOString().split('T')[0],
      0.0001 + (Math.random() - 0.5) * 0.005
    ])
  ),
  'Commodities': Object.fromEntries(
    Array.from({ length: 200 }, (_, i) => [
      new Date(2024, 0, i + 1).toISOString().split('T')[0],
      0.0002 + (Math.random() - 0.5) * 0.015
    ])
  ),
  'Cash': Object.fromEntries(
    Array.from({ length: 200 }, (_, i) => [
      new Date(2024, 0, i + 1).toISOString().split('T')[0],
      0.0001 + (Math.random() - 0.5) * 0.001
    ])
  )
};

export const SAMPLE_WEIGHTS = [0.4, 0.3, 0.2, 0.1];

// Analysis modes available
export const ANALYSIS_MODES = [
  { id: 'portfolio', label: 'PORTFOLIO RISK', iconName: 'Shield' },
  { id: 'options', label: 'OPTION PRICING', iconName: 'DollarSign' },
  { id: 'optimization', label: 'OPTIMIZATION', iconName: 'Target' },
  { id: 'entropy', label: 'ENTROPY', iconName: 'Layers' }
] as const;
