/**
 * Functime Panel Constants
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

// Sample panel data for demonstration (kept for backward compatibility)
// Note: Default behavior now uses live yfinance data
export const SAMPLE_DATA = [
  { entity_id: 'AAPL', time: '2024-01-02', value: 185.64 },
  { entity_id: 'AAPL', time: '2024-01-03', value: 184.25 },
  { entity_id: 'AAPL', time: '2024-01-04', value: 181.91 },
  { entity_id: 'AAPL', time: '2024-01-05', value: 181.18 },
  { entity_id: 'AAPL', time: '2024-01-08', value: 185.56 },
  { entity_id: 'AAPL', time: '2024-01-09', value: 185.14 },
  { entity_id: 'AAPL', time: '2024-01-10', value: 186.19 },
  { entity_id: 'AAPL', time: '2024-01-11', value: 185.59 },
  { entity_id: 'AAPL', time: '2024-01-12', value: 185.92 },
  { entity_id: 'AAPL', time: '2024-01-16', value: 183.63 },
  { entity_id: 'AAPL', time: '2024-01-17', value: 182.68 },
  { entity_id: 'AAPL', time: '2024-01-18', value: 188.63 },
  { entity_id: 'AAPL', time: '2024-01-19', value: 191.56 },
  { entity_id: 'AAPL', time: '2024-01-22', value: 193.89 },
  { entity_id: 'AAPL', time: '2024-01-23', value: 195.18 },
  { entity_id: 'AAPL', time: '2024-01-24', value: 194.50 },
  { entity_id: 'AAPL', time: '2024-01-25', value: 194.17 },
  { entity_id: 'AAPL', time: '2024-01-26', value: 192.42 },
  { entity_id: 'AAPL', time: '2024-01-29', value: 191.73 },
  { entity_id: 'AAPL', time: '2024-01-30', value: 188.04 }
];

// Table pagination constants
export const INITIAL_ROWS = 10;
export const EXPANDED_ROWS = 50;
