/**
 * Tradier Constants
 *
 * API endpoints, mappings, and configuration for Tradier Brokerage API
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

// Production (Live Trading)
export const TRADIER_LIVE_API_BASE = 'https://api.tradier.com/v1';
export const TRADIER_LIVE_STREAM_URL = 'https://stream.tradier.com/v1';

// Sandbox (Paper Trading - 15 min delayed data)
export const TRADIER_SANDBOX_API_BASE = 'https://sandbox.tradier.com/v1';

// API Version
export const TRADIER_API_VERSION = 'v1';

// ============================================================================
// API ENDPOINTS
// ============================================================================

export const TRADIER_ENDPOINTS = {
  // User/Profile
  USER_PROFILE: '/user/profile',

  // Account
  ACCOUNT_BALANCES: '/accounts/{account_id}/balances',
  ACCOUNT_POSITIONS: '/accounts/{account_id}/positions',
  ACCOUNT_HISTORY: '/accounts/{account_id}/history',
  ACCOUNT_GAINLOSS: '/accounts/{account_id}/gainloss',

  // Orders
  ORDERS: '/accounts/{account_id}/orders',
  ORDER: '/accounts/{account_id}/orders/{order_id}',

  // Market Data
  QUOTES: '/markets/quotes',
  OPTION_CHAINS: '/markets/options/chains',
  OPTION_STRIKES: '/markets/options/strikes',
  OPTION_EXPIRATIONS: '/markets/options/expirations',
  OPTION_LOOKUP: '/markets/options/lookup',
  HISTORICAL: '/markets/history',
  TIMESALES: '/markets/timesales',
  CLOCK: '/markets/clock',
  CALENDAR: '/markets/calendar',
  SEARCH: '/markets/search',
  LOOKUP: '/markets/lookup',
  ETB: '/markets/etb', // Easy to borrow list

  // Streaming
  STREAM_SESSION: '/markets/events/session',
  STREAM_EVENTS: '/markets/events',

  // Watchlists
  WATCHLISTS: '/watchlists',
  WATCHLIST: '/watchlists/{id}',
  WATCHLIST_SYMBOLS: '/watchlists/{id}/symbols',
} as const;

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const TRADIER_EXCHANGE_MAP: Record<string, string> = {
  NYSE: 'N',
  NASDAQ: 'Q',
  AMEX: 'A',
  ARCA: 'P',
  BATS: 'Z',
  OTC: 'U',
};

export const TRADIER_EXCHANGE_REVERSE_MAP: Record<string, string> = {
  N: 'NYSE',
  Q: 'NASDAQ',
  A: 'AMEX',
  P: 'ARCA',
  Z: 'BATS',
  U: 'OTC',
  C: 'NASDAQ', // National Stock Exchange
  X: 'NASDAQ', // NASDAQ OMX PSX
  V: 'NYSE', // Investors Exchange
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const TRADIER_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'market',
  LIMIT: 'limit',
  STOP: 'stop',
  STOP_LIMIT: 'stop_limit',
};

export const TRADIER_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  market: 'MARKET',
  limit: 'LIMIT',
  stop: 'STOP',
  stop_limit: 'STOP_LIMIT',
  debit: 'LIMIT', // Options debit spread
  credit: 'LIMIT', // Options credit spread
  even: 'MARKET', // Options even
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const TRADIER_STATUS_MAP: Record<string, string> = {
  open: 'OPEN',
  partially_filled: 'PARTIALLY_FILLED',
  filled: 'FILLED',
  expired: 'EXPIRED',
  canceled: 'CANCELLED',
  pending: 'PENDING',
  rejected: 'REJECTED',
  error: 'REJECTED',
};

// ============================================================================
// SIDE MAPPINGS
// ============================================================================

export const TRADIER_SIDE_MAP: Record<string, string> = {
  BUY: 'buy',
  SELL: 'sell',
};

export const TRADIER_SIDE_REVERSE_MAP: Record<string, string> = {
  buy: 'BUY',
  sell: 'SELL',
  buy_to_open: 'BUY',
  sell_to_open: 'SELL',
  buy_to_close: 'BUY',
  sell_to_close: 'SELL',
};

// ============================================================================
// VALIDITY (TIME IN FORCE) MAPPINGS
// ============================================================================

export const TRADIER_VALIDITY_MAP: Record<string, string> = {
  DAY: 'day',
  GTC: 'gtc',
  IOC: 'day', // Tradier doesn't support IOC, map to day
  GTD: 'gtc', // Tradier doesn't support GTD, map to gtc
  PRE: 'pre', // Pre-market
  POST: 'post', // Post-market (extended hours)
};

export const TRADIER_VALIDITY_REVERSE_MAP: Record<string, string> = {
  day: 'DAY',
  gtc: 'GTC',
  pre: 'DAY',
  post: 'DAY',
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

export const TRADIER_PRODUCT_MAP: Record<string, string> = {
  CASH: 'cash',
  MARGIN: 'margin',
  INTRADAY: 'margin',
};

export const TRADIER_PRODUCT_REVERSE_MAP: Record<string, string> = {
  cash: 'CASH',
  margin: 'MARGIN',
  pdt: 'MARGIN', // Pattern Day Trader
};

// ============================================================================
// TIMEFRAME MAPPINGS
// ============================================================================

export const TRADIER_INTERVAL_MAP: Record<string, string> = {
  '1m': 'minute',
  '5m': '5min',
  '15m': '15min',
  '1d': 'daily',
  '1w': 'weekly',
  '1M': 'monthly',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const TRADIER_METADATA: StockBrokerMetadata = {
  id: 'tradier',
  name: 'tradier',
  displayName: 'Tradier',
  logo: 'https://fincept.in/brokers/tradier.png',
  website: 'https://tradier.com',

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
    gtt: true, // GTC orders supported
    bracketOrder: false, // Basic bracket not directly supported
    coverOrder: false,
    marginCalculator: false, // No margin preview API
    optionsChain: true, // Full options support!
    paperTrading: true, // Sandbox environment
  },

  tradingFeatures: {
    marketOrders: true,
    limitOrders: true,
    stopOrders: true,
    stopLimitOrders: true,
    trailingStopOrders: false, // Not supported
  },

  productTypes: ['CASH', 'MARGIN'],

  authType: 'api_key', // Bearer token

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 120, // 120 requests per minute
  },

  fees: {
    equity: { brokerage: 0, stt: 0 }, // Commission-free stocks
    intraday: { brokerage: 0 },
    fno: { brokerage: 0.35 }, // $0.35 per contract for options
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

export const TRADIER_ERROR_CODES = {
  INVALID_TOKEN: 'oauth.token_invalid',
  EXPIRED_TOKEN: 'oauth.token_expired',
  INVALID_ACCOUNT: 'account.invalid',
  INVALID_SYMBOL: 'symbol.invalid',
  INSUFFICIENT_FUNDS: 'order.insufficient_funds',
  ORDER_NOT_FOUND: 'order.not_found',
  MARKET_CLOSED: 'market.closed',
  INVALID_QUANTITY: 'order.invalid_quantity',
  INVALID_PRICE: 'order.invalid_price',
} as const;

// ============================================================================
// DEFAULT FIELDS FOR QUOTES
// ============================================================================

export const TRADIER_DEFAULT_QUOTE_FIELDS = [
  'symbol',
  'description',
  'exch',
  'type',
  'last',
  'change',
  'volume',
  'open',
  'high',
  'low',
  'close',
  'bid',
  'ask',
  'bidsize',
  'asksize',
  'change_percentage',
  'average_volume',
  'prevclose',
  'week_52_high',
  'week_52_low',
];
