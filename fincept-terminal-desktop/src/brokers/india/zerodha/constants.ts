/**
 * Zerodha (Kite) specific constants
 */

import type { IndianBrokerMetadata } from '../types';

// ============================================================================
// API Configuration
// ============================================================================

export const ZERODHA_API_BASE_URL = 'https://api.kite.trade';
export const ZERODHA_LOGIN_URL = 'https://kite.zerodha.com/connect/login';
export const ZERODHA_WS_URL = 'wss://ws.kite.trade';

export const ZERODHA_API_VERSION = '3';

// ============================================================================
// Endpoints
// ============================================================================

export const ZERODHA_ENDPOINTS = {
  // Authentication
  SESSION: '/session/token',
  USER_PROFILE: '/user/profile',
  USER_MARGINS: '/user/margins',

  // Orders
  ORDERS: '/orders',
  ORDERS_REGULAR: '/orders/regular',
  ORDERS_AMO: '/orders/amo',
  TRADES: '/trades',

  // Portfolio
  POSITIONS: '/portfolio/positions',
  HOLDINGS: '/portfolio/holdings',

  // Market Data
  QUOTE: '/quote',
  OHLC: '/quote/ohlc',
  LTP: '/quote/ltp',
  INSTRUMENTS: '/instruments',
  HISTORICAL: '/instruments/historical',

  // GTT
  GTT: '/gtt/triggers',
} as const;

// ============================================================================
// Broker Metadata
// ============================================================================

export const ZERODHA_METADATA: IndianBrokerMetadata = {
  id: 'zerodha',
  name: 'zerodha',
  displayName: 'Zerodha (Kite)',
  logo: 'zerodha-logo.svg',
  website: 'https://zerodha.com',
  supportedExchanges: [
    'NSE',
    'BSE',
    'NFO',
    'BFO',
    'CDS',
    'BCD',
    'MCX',
    'NSE_INDEX',
    'BSE_INDEX',
    'MCX_INDEX',
    'CDS_INDEX',
  ],
  supportedProducts: ['CNC', 'MIS', 'NRML', 'CO', 'BO'],
  supportedOrderTypes: ['MARKET', 'LIMIT', 'SL', 'SL-M'],
  features: {
    webSocket: true,
    amo: true,
    gtt: true,
    bracketOrder: true,
    coverOrder: true,
    marginCalculator: true,
  },
  authType: 'oauth',
  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 1,
  },
};

// ============================================================================
// Exchange Mappings
// ============================================================================

export const ZERODHA_EXCHANGE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NFO: 'NFO',
  BFO: 'BFO',
  CDS: 'CDS',
  BCD: 'BCD',
  MCX: 'MCX',
  NSE_INDEX: 'NSE',
  BSE_INDEX: 'BSE',
  MCX_INDEX: 'MCX',
  CDS_INDEX: 'CDS',
};

// ============================================================================
// WebSocket Constants
// ============================================================================

export const ZERODHA_WS_CONFIG = {
  // Subscription modes
  MODE_LTP: 'ltp',
  MODE_QUOTE: 'quote',
  MODE_FULL: 'full',

  // Connection settings
  PING_INTERVAL: null, // Zerodha sends heartbeats
  KEEPALIVE_INTERVAL: 30000, // 30 seconds
  PING_TIMEOUT: 10000,
  CONNECTION_TIMEOUT: 10000,
  MAX_MESSAGE_SIZE: 10 * 1024 * 1024, // 10MB

  // Subscription batching
  MAX_TOKENS_PER_SUBSCRIBE: 200,
  SUBSCRIPTION_DELAY: 2000, // 2 seconds between batches
  MAX_INSTRUMENTS_PER_CONNECTION: 3000,

  // Reconnection
  MAX_RECONNECT_ATTEMPTS: 50,
  MAX_RECONNECT_DELAY: 60000,
  INITIAL_RECONNECT_DELAY: 2000,
} as const;

// ============================================================================
// Timeframe Mappings
// ============================================================================

export const ZERODHA_TIMEFRAME_MAP: Record<string, string> = {
  '1m': 'minute',
  '3m': '3minute',
  '5m': '5minute',
  '10m': '10minute',
  '15m': '15minute',
  '30m': '30minute',
  '1h': '60minute',
  '1d': 'day',
  '1w': 'week',
  '1M': 'month',
};

// ============================================================================
// Error Code Mappings
// ============================================================================

export const ZERODHA_ERROR_CODES: Record<string, string> = {
  TokenException: 'AUTH002', // Token expired
  InputException: 'ERR002', // Invalid input
  OrderException: 'ORD001', // Order error
  NetworkException: 'CONN001', // Network error
  GeneralException: 'ERR001', // General error
};
