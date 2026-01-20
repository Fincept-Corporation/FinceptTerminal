// Fincept Professional Color Palette for Crypto Trading
export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
} as const;

// Top 30 most traded crypto pairs
export const DEFAULT_CRYPTO_WATCHLIST = [
  'BTC/USD', 'ETH/USD', 'BNB/USD', 'SOL/USD', 'XRP/USD',
  'ADA/USD', 'DOGE/USD', 'AVAX/USD', 'DOT/USD', 'MATIC/USD',
  'LTC/USD', 'SHIB/USD', 'TRX/USD', 'LINK/USD', 'UNI/USD',
  'ATOM/USD', 'XLM/USD', 'ETC/USD', 'BCH/USD', 'NEAR/USD',
  'APT/USD', 'ARB/USD', 'OP/USD', 'LDO/USD', 'FIL/USD',
  'ICP/USD', 'INJ/USD', 'STX/USD', 'MKR/USD', 'AAVE/USD'
];

// CSS styles for the crypto trading terminal
export const CRYPTO_TERMINAL_STYLES = `
  @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

  *::-webkit-scrollbar { width: 6px; height: 6px; }
  *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
  *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
  *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }

  .terminal-glow {
    text-shadow: 0 0 10px ${FINCEPT.ORANGE}40;
  }

  .price-flash {
    animation: flash 0.3s ease-out;
  }

  @keyframes flash {
    0% { background-color: ${FINCEPT.YELLOW}40; }
    100% { background-color: transparent; }
  }

  @keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
  }
`;
