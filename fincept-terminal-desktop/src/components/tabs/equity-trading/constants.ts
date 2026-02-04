/**
 * Equity Trading Tab - Shared Constants & Utilities
 */
import type { StockExchange } from '@/brokers/stocks/types';

// Fincept Professional Color Palette
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
};

// Currency configuration based on exchange/symbol
export const CURRENCY_CONFIG: Record<string, { symbol: string; code: string; locale: string }> = {
  INR: { symbol: '\u20B9', code: 'INR', locale: 'en-IN' },
  USD: { symbol: '$', code: 'USD', locale: 'en-US' },
  EUR: { symbol: '\u20AC', code: 'EUR', locale: 'de-DE' },
  GBP: { symbol: '\u00A3', code: 'GBP', locale: 'en-GB' },
  JPY: { symbol: '\u00A5', code: 'JPY', locale: 'ja-JP' },
  HKD: { symbol: 'HK$', code: 'HKD', locale: 'zh-HK' },
  SGD: { symbol: 'S$', code: 'SGD', locale: 'en-SG' },
  AUD: { symbol: 'A$', code: 'AUD', locale: 'en-AU' },
  CAD: { symbol: 'C$', code: 'CAD', locale: 'en-CA' },
};

// Map exchange to default currency
export const EXCHANGE_CURRENCY: Record<string, string> = {
  NSE: 'INR', BSE: 'INR', NFO: 'INR', BFO: 'INR', MCX: 'INR', CDS: 'INR',
  NYSE: 'USD', NASDAQ: 'USD', AMEX: 'USD',
  LSE: 'GBP',
  XETRA: 'EUR', EURONEXT: 'EUR',
  TSX: 'CAD',
  ASX: 'AUD',
  HKEX: 'HKD',
  SGX: 'SGD',
  TSE: 'JPY',
};

// Detect currency from symbol suffix (for YFinance symbols like RELIANCE.NS)
export const getCurrencyFromSymbol = (symbol: string, exchange?: string): string => {
  if (symbol.endsWith('.NS') || symbol.endsWith('.BO')) return 'INR';
  if (symbol.endsWith('.L')) return 'GBP';
  if (symbol.endsWith('.DE')) return 'EUR';
  if (symbol.endsWith('.TO')) return 'CAD';
  if (symbol.endsWith('.AX')) return 'AUD';
  if (symbol.endsWith('.HK')) return 'HKD';
  if (symbol.endsWith('.T')) return 'JPY';
  if (exchange) return EXCHANGE_CURRENCY[exchange] || 'USD';
  return 'USD';
};

// Format currency value
export const formatCurrency = (value: number, currencyCode: string = 'INR', compact: boolean = false): string => {
  const config = CURRENCY_CONFIG[currencyCode] || CURRENCY_CONFIG.USD;

  if (compact && Math.abs(value) >= 1000) {
    if (Math.abs(value) >= 10000000) {
      return `${config.symbol}${(value / 10000000).toFixed(2)}Cr`;
    } else if (Math.abs(value) >= 100000) {
      return `${config.symbol}${(value / 100000).toFixed(2)}L`;
    } else if (Math.abs(value) >= 1000) {
      return `${config.symbol}${(value / 1000).toFixed(1)}K`;
    }
  }

  return `${config.symbol}${value.toLocaleString(config.locale, {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  })}`;
};

// NIFTY 50 Components - Full watchlist
export const DEFAULT_WATCHLIST = [
  'HDFCBANK', 'ICICIBANK', 'SBIN', 'KOTAKBANK', 'AXISBANK',
  'BAJFINANCE', 'BAJAJFINSV', 'HDFCLIFE', 'SBILIFE', 'INDUSINDBK',
  'TCS', 'INFY', 'WIPRO', 'HCLTECH', 'TECHM', 'LTIM',
  'RELIANCE', 'ONGC', 'NTPC', 'POWERGRID', 'ADANIPORTS', 'ADANIENT',
  'MARUTI', 'TATAMOTORS', 'M&M', 'BAJAJ-AUTO', 'HEROMOTOCO', 'EICHERMOT',
  'HINDUNILVR', 'ITC', 'NESTLEIND', 'BRITANNIA', 'TATACONSUM',
  'TATASTEEL', 'JSWSTEEL', 'HINDALCO', 'COALINDIA',
  'SUNPHARMA', 'DRREDDY', 'CIPLA', 'APOLLOHOSP', 'DIVISLAB',
  'LT', 'BHARTIARTL', 'ULTRACEMCO', 'GRASIM', 'TITAN',
  'ASIANPAINT', 'BPCL', 'SHRIRAMFIN', 'TRENT',
];

// Market indices
export const MARKET_INDICES = [
  { symbol: 'NIFTY 50', exchange: 'NSE' as StockExchange },
  { symbol: 'SENSEX', exchange: 'BSE' as StockExchange },
  { symbol: 'BANKNIFTY', exchange: 'NSE' as StockExchange },
  { symbol: 'NIFTYIT', exchange: 'NSE' as StockExchange },
];

// Terminal styles
export const EQUITY_TERMINAL_STYLES = `
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

  @keyframes spin {
    from { transform: rotate(0deg); }
    to { transform: rotate(360deg); }
  }
`;
