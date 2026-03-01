/**
 * CodeEditorTab - Design System, Interfaces, and Constants
 */

// ─── FINCEPT Design System ──────────────────────────────────────────────────
export const F = {
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
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
} as const;

export const FONT = '"IBM Plex Mono", "Consolas", monospace';

// ─── Interfaces ─────────────────────────────────────────────────────────────
export interface EditorFile {
  id: string;
  name: string;
  content: string;
  language: 'finscript' | 'python' | 'javascript' | 'json';
  unsaved: boolean;
  cursorLine: number;
  cursorCol: number;
}

export interface SearchState {
  isOpen: boolean;
  query: string;
  replaceQuery: string;
  showReplace: boolean;
  matchCount: number;
  currentMatch: number;
}

// ─── Default FinScript content ───────────────────────────────────────────────
export const DEFAULT_FINSCRIPT = `// FinScript - EMA/RSI Crossover Strategy
// Toggle LIVE DATA in toolbar to use real Yahoo Finance market data
// In DEMO mode, synthetic data is generated for instant execution

// Calculate indicators on AAPL
ema_fast = ema(AAPL, 12)
ema_slow = ema(AAPL, 26)
rsi_val = rsi(AAPL, 14)

// Get latest values
fast = last(ema_fast)
slow = last(ema_slow)
rsi_now = last(rsi_val)

// Print analysis
print "EMA(12):", fast
print "EMA(26):", slow
print "RSI(14):", rsi_now

// Trading logic
if fast > slow {
    if rsi_now < 70 {
        buy "Bullish crossover + RSI not overbought"
    }
}

if fast < slow {
    if rsi_now > 30 {
        sell "Bearish crossover + RSI not oversold"
    }
}

// Visualize
plot_candlestick AAPL, "AAPL Price"
plot_line ema_fast, "EMA (12)", "cyan"
plot_line ema_slow, "EMA (26)", "orange"
plot rsi_val, "RSI (14)"
`;
