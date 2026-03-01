/**
 * Market Simulation Tab - Constants, Design System, and Helpers
 */

// ============================================================================
// FINCEPT Design System
// ============================================================================
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
};

export const FONT = '"IBM Plex Mono", "Consolas", monospace';

// CSS keyframes for animations (injected once)
const KEYFRAMES_ID = 'market-sim-keyframes';
if (typeof document !== 'undefined' && !document.getElementById(KEYFRAMES_ID)) {
  const style = document.createElement('style');
  style.id = KEYFRAMES_ID;
  style.textContent = `
    @keyframes spin {
      from { transform: rotate(0deg); }
      to { transform: rotate(360deg); }
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
  `;
  document.head.appendChild(style);
}

// ============================================================================
// Helpers
// ============================================================================
export const formatPrice = (p: number) => (p / 100).toFixed(2);
export const formatQty = (q: number) => q.toLocaleString();
export const formatPnl = (pnl: number) => {
  const sign = pnl >= 0 ? '+' : '';
  return `${sign}$${(pnl / 100).toFixed(2)}`;
};
export const formatPct = (p: number) => `${(p * 100).toFixed(2)}%`;
export const formatTime = (nanos: number) => {
  const totalSeconds = Math.floor(nanos / 1_000_000_000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
};

// Helper to extract enum variant name from Rust serde serialization
export const getEnumVariant = (value: unknown): string => {
  if (typeof value === 'string') return value;
  if (typeof value === 'object' && value !== null) {
    const keys = Object.keys(value);
    return keys.length > 0 ? keys[0] : 'Unknown';
  }
  return String(value);
};

export const PARTICIPANT_COLORS: Record<string, string> = {
  MarketMaker: F.BLUE,
  HFT: F.PURPLE,
  StatArb: F.CYAN,
  Momentum: F.YELLOW,
  MeanReversion: F.GREEN,
  NoiseTrader: F.GRAY,
  InformedTrader: F.ORANGE,
  Institutional: '#00BFFF',
  RetailTrader: F.MUTED,
  ToxicFlow: F.RED,
  Spoofer: '#FF6B6B',
  Arbitrageur: '#7B68EE',
  SniperBot: '#FF4500',
};

export const PHASE_LABELS: Record<string, { label: string; color: string }> = {
  PreOpen: { label: 'PRE-OPEN', color: F.MUTED },
  OpeningAuction: { label: 'OPENING AUCTION', color: F.YELLOW },
  ContinuousTrading: { label: 'CONTINUOUS TRADING', color: F.GREEN },
  VolatilityAuction: { label: 'VOLATILITY AUCTION', color: F.RED },
  ClosingAuction: { label: 'CLOSING AUCTION', color: F.YELLOW },
  PostClose: { label: 'POST CLOSE', color: F.MUTED },
  Halted: { label: 'HALTED', color: F.RED },
};
