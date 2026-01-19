/**
 * Alpaca Constants
 *
 * API endpoints, mappings, and configuration for Alpaca Trading API
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

// Live Trading
export const ALPACA_LIVE_API_BASE = 'https://api.alpaca.markets';
export const ALPACA_LIVE_DATA_API_BASE = 'https://data.alpaca.markets';
export const ALPACA_LIVE_STREAM_URL = 'wss://stream.data.alpaca.markets';
export const ALPACA_LIVE_TRADE_STREAM_URL = 'wss://api.alpaca.markets/stream';

// Paper Trading (Sandbox)
export const ALPACA_PAPER_API_BASE = 'https://paper-api.alpaca.markets';
export const ALPACA_PAPER_DATA_API_BASE = 'https://data.alpaca.markets'; // Same as live
export const ALPACA_PAPER_STREAM_URL = 'wss://stream.data.sandbox.alpaca.markets';
export const ALPACA_PAPER_TRADE_STREAM_URL = 'wss://paper-api.alpaca.markets/stream';

// API Version
export const ALPACA_API_VERSION = 'v2';
export const ALPACA_DATA_API_VERSION = 'v2';

export const ALPACA_ENDPOINTS = {
  // Account
  ACCOUNT: '/v2/account',
  ACCOUNT_ACTIVITIES: '/v2/account/activities',
  ACCOUNT_PORTFOLIO_HISTORY: '/v2/account/portfolio/history',
  ACCOUNT_CONFIGURATIONS: '/v2/account/configurations',

  // Orders
  ORDERS: '/v2/orders',
  ORDER_BY_ID: '/v2/orders/:order_id',
  ORDER_BY_CLIENT_ID: '/v2/orders:by_client_order_id',

  // Positions
  POSITIONS: '/v2/positions',
  POSITION_BY_SYMBOL: '/v2/positions/:symbol',
  CLOSE_ALL_POSITIONS: '/v2/positions?cancel_orders=true',

  // Assets
  ASSETS: '/v2/assets',
  ASSET_BY_SYMBOL: '/v2/assets/:symbol',

  // Watchlists
  WATCHLISTS: '/v2/watchlists',

  // Calendar & Clock
  CALENDAR: '/v2/calendar',
  CLOCK: '/v2/clock',

  // Market Data (Data API)
  BARS: '/v2/stocks/:symbol/bars',
  BARS_MULTI: '/v2/stocks/bars',
  QUOTES: '/v2/stocks/:symbol/quotes',
  QUOTES_MULTI: '/v2/stocks/quotes',
  TRADES: '/v2/stocks/:symbol/trades',
  TRADES_MULTI: '/v2/stocks/trades',
  SNAPSHOT: '/v2/stocks/:symbol/snapshot',
  SNAPSHOTS: '/v2/stocks/snapshots',
  LATEST_QUOTES: '/v2/stocks/quotes/latest',
  LATEST_TRADES: '/v2/stocks/trades/latest',
  LATEST_BARS: '/v2/stocks/bars/latest',
} as const;

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const ALPACA_EXCHANGE_MAP: Record<string, string> = {
  NYSE: 'NYSE',
  NASDAQ: 'NASDAQ',
  AMEX: 'AMEX',
  ARCA: 'ARCA',
  BATS: 'BATS',
  // Alpaca uses these internally
  OTC: 'OTC',
  CRYPTO: 'CRYPTO',
};

export const ALPACA_EXCHANGE_REVERSE_MAP: Record<string, string> = {
  NYSE: 'NYSE',
  NASDAQ: 'NASDAQ',
  AMEX: 'AMEX',
  ARCA: 'ARCA',
  BATS: 'BATS',
  N: 'NYSE',
  Q: 'NASDAQ',
  A: 'AMEX',
  Z: 'BATS',
  P: 'ARCA',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const ALPACA_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'market',
  LIMIT: 'limit',
  STOP: 'stop',
  STOP_LIMIT: 'stop_limit',
  TRAILING_STOP: 'trailing_stop',
};

export const ALPACA_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  market: 'MARKET',
  limit: 'LIMIT',
  stop: 'STOP',
  stop_limit: 'STOP_LIMIT',
  trailing_stop: 'TRAILING_STOP',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const ALPACA_STATUS_MAP: Record<string, string> = {
  new: 'OPEN',
  partially_filled: 'PARTIALLY_FILLED',
  filled: 'FILLED',
  done_for_day: 'FILLED',
  canceled: 'CANCELLED',
  expired: 'EXPIRED',
  replaced: 'CANCELLED',
  pending_cancel: 'PENDING',
  pending_replace: 'PENDING',
  pending_new: 'PENDING',
  accepted: 'OPEN',
  stopped: 'TRIGGER_PENDING',
  rejected: 'REJECTED',
  suspended: 'PENDING',
  calculated: 'PENDING',
};

// ============================================================================
// SIDE MAPPINGS
// ============================================================================

export const ALPACA_SIDE_MAP: Record<string, string> = {
  BUY: 'buy',
  SELL: 'sell',
};

export const ALPACA_SIDE_REVERSE_MAP: Record<string, string> = {
  buy: 'BUY',
  sell: 'SELL',
};

// ============================================================================
// VALIDITY (TIME IN FORCE) MAPPINGS
// ============================================================================

export const ALPACA_VALIDITY_MAP: Record<string, string> = {
  DAY: 'day',
  IOC: 'ioc',
  GTC: 'gtc',
  GTD: 'gtc', // Alpaca doesn't have GTD, map to GTC
  OPG: 'opg', // Market on Open
  CLS: 'cls', // Market on Close
  FOK: 'fok', // Fill or Kill
};

export const ALPACA_VALIDITY_REVERSE_MAP: Record<string, string> = {
  day: 'DAY',
  ioc: 'IOC',
  gtc: 'GTC',
  opg: 'DAY',
  cls: 'DAY',
  fok: 'IOC',
};

// ============================================================================
// PRODUCT TYPE MAPPINGS (US doesn't use product types like India)
// ============================================================================

export const ALPACA_PRODUCT_MAP: Record<string, string> = {
  CASH: 'cash', // Regular cash account
  MARGIN: 'margin', // Margin account
  INTRADAY: 'margin', // Map intraday to margin (day trading)
};

export const ALPACA_PRODUCT_REVERSE_MAP: Record<string, string> = {
  cash: 'CASH',
  margin: 'MARGIN',
};

// ============================================================================
// TIMEFRAME MAPPINGS
// ============================================================================

export const ALPACA_TIMEFRAME_MAP: Record<string, string> = {
  '1m': '1Min',
  '3m': '3Min',
  '5m': '5Min',
  '10m': '10Min',
  '15m': '15Min',
  '30m': '30Min',
  '1h': '1Hour',
  '2h': '2Hour',
  '4h': '4Hour',
  '1d': '1Day',
  '1w': '1Week',
  '1M': '1Month',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const ALPACA_METADATA: StockBrokerMetadata = {
  id: 'alpaca',
  name: 'alpaca',
  displayName: 'Alpaca',
  logo: 'https://fincept.in/brokers/alpaca.png',
  website: 'https://alpaca.markets',

  region: 'us',
  country: 'US',
  currency: 'USD',
  exchanges: ['NYSE', 'NASDAQ', 'AMEX', 'ARCA', 'BATS'],
  marketHours: {
    open: '09:30',
    close: '16:00',
    timezone: 'America/New_York',
    preMarketOpen: '04:00',
    preMarketClose: '09:30',
    postMarketOpen: '16:00',
    postMarketClose: '20:00',
  },

  features: {
    webSocket: true,
    amo: false, // US doesn't use AMO concept
    gtt: true, // GTC orders
    bracketOrder: true,
    coverOrder: false,
    marginCalculator: true,
    optionsChain: false, // Basic stocks only for now
    paperTrading: true, // Alpaca has built-in paper trading!
  },

  tradingFeatures: {
    marketOrders: true,
    limitOrders: true,
    stopOrders: true,
    stopLimitOrders: true,
    trailingStopOrders: true, // Alpaca supports trailing stops
  },

  productTypes: ['CASH', 'MARGIN'],

  authType: 'api_key', // API Key + Secret

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 200, // Alpaca has generous rate limits
  },

  fees: {
    equity: { brokerage: 0, stt: 0 }, // Commission-free
    intraday: { brokerage: 0 },
    fno: { brokerage: 0 },
  },

  defaultSymbols: [
    'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA',
    'META', 'TSLA', 'BRK.B', 'JPM', 'V',
    'UNH', 'JNJ', 'XOM', 'PG', 'MA',
  ],
};

// ============================================================================
// ERROR CODES
// ============================================================================

export const ALPACA_ERROR_CODES = {
  INVALID_CREDENTIALS: 40110000,
  INSUFFICIENT_BUYING_POWER: 40310000,
  ORDER_NOT_FOUND: 40410000,
  ACCOUNT_BLOCKED: 40310001,
  TRADING_HALTED: 40310002,
  ASSET_NOT_TRADABLE: 40310003,
  MARKET_CLOSED: 40310004,
  INSUFFICIENT_QTY: 40310005,
  PATTERN_DAY_TRADER: 40310006,
} as const;
