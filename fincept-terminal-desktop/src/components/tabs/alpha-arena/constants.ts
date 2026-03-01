/**
 * Alpha Arena — Shared Constants
 *
 * Fincept terminal design tokens, symbol lists, and competition modes
 * used by AlphaArenaTab and its sub-components.
 */

import type { CompetitionMode } from './types';

// =============================================================================
// Fincept Terminal Design Constants
// =============================================================================

export const FINCEPT = {
  ORANGE: '#FF8800',
  ORANGE_HOVER: '#FF9900',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  YELLOW: '#FFD700',
  CYAN: '#00E5FF',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  BORDER_LIGHT: '#3A3A3A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
} as const;

export const TERMINAL_FONT = '"IBM Plex Mono", "Consolas", "Monaco", monospace';

export const TERMINAL_STYLES = `
  @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');
  .alpha-arena-terminal *::-webkit-scrollbar { width: 6px; height: 6px; }
  .alpha-arena-terminal *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
  .alpha-arena-terminal *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; }
  .alpha-arena-terminal *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
  @keyframes alpha-pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
  }
  @keyframes alpha-spin {
    from { transform: rotate(0deg); }
    to { transform: rotate(360deg); }
  }
`;

export const TRADING_SYMBOLS = [
  { value: 'BTC/USD', label: 'Bitcoin (BTC/USD)' },
  { value: 'ETH/USD', label: 'Ethereum (ETH/USD)' },
  { value: 'BNB/USD', label: 'BNB (BNB/USD)' },
  { value: 'SOL/USD', label: 'Solana (SOL/USD)' },
  { value: 'XRP/USD', label: 'Ripple (XRP/USD)' },
  { value: 'ADA/USD', label: 'Cardano (ADA/USD)' },
  { value: 'DOGE/USD', label: 'Dogecoin (DOGE/USD)' },
  { value: 'AVAX/USD', label: 'Avalanche (AVAX/USD)' },
  { value: 'DOT/USD', label: 'Polkadot (DOT/USD)' },
  { value: 'POL/USD', label: 'Polygon (POL/USD)' },
  { value: 'LTC/USD', label: 'Litecoin (LTC/USD)' },
  { value: 'SHIB/USD', label: 'Shiba Inu (SHIB/USD)' },
  { value: 'TRX/USD', label: 'TRON (TRX/USD)' },
  { value: 'LINK/USD', label: 'Chainlink (LINK/USD)' },
  { value: 'UNI/USD', label: 'Uniswap (UNI/USD)' },
  { value: 'ATOM/USD', label: 'Cosmos (ATOM/USD)' },
  { value: 'XLM/USD', label: 'Stellar (XLM/USD)' },
  { value: 'ETC/USD', label: 'Ethereum Classic (ETC/USD)' },
  { value: 'BCH/USD', label: 'Bitcoin Cash (BCH/USD)' },
  { value: 'NEAR/USD', label: 'NEAR Protocol (NEAR/USD)' },
  { value: 'APT/USD', label: 'Aptos (APT/USD)' },
  { value: 'ARB/USD', label: 'Arbitrum (ARB/USD)' },
  { value: 'OP/USD', label: 'Optimism (OP/USD)' },
  { value: 'LDO/USD', label: 'Lido DAO (LDO/USD)' },
  { value: 'FIL/USD', label: 'Filecoin (FIL/USD)' },
  { value: 'ICP/USD', label: 'Internet Computer (ICP/USD)' },
  { value: 'INJ/USD', label: 'Injective (INJ/USD)' },
  { value: 'STX/USD', label: 'Stacks (STX/USD)' },
  { value: 'MKR/USD', label: 'Maker (MKR/USD)' },
  { value: 'AAVE/USD', label: 'Aave (AAVE/USD)' },
];

export const COMPETITION_MODES: { value: CompetitionMode; label: string; desc: string }[] = [
  { value: 'baseline', label: 'Baseline', desc: 'Standard trading with balanced risk' },
  { value: 'monk', label: 'Monk', desc: 'Conservative - capital preservation' },
  { value: 'situational', label: 'Situational', desc: 'Competitive - models aware of others' },
  { value: 'max_leverage', label: 'Max Leverage', desc: 'Aggressive - maximum positions' },
];
