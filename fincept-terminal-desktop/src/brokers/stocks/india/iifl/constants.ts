/**
 * IIFL Constants
 *
 * API endpoints, mappings, and configuration for IIFL XTS Trading API
 * Reference: https://symphonyfintech.com/xts-trading-front-end-api/
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

export const IIFL_BASE_URL = 'https://ttblaze.iifl.com';
export const IIFL_INTERACTIVE_URL = 'https://ttblaze.iifl.com/interactive';
export const IIFL_MARKET_DATA_URL = 'https://ttblaze.iifl.com/apimarketdata';

export const IIFL_ENDPOINTS = {
  // Auth
  SESSION: '/user/session',
  FEED_LOGIN: '/auth/login',
  BALANCE: '/user/balance',

  // Orders
  ORDERS: '/orders',
  TRADES: '/orders/trades',

  // Portfolio
  POSITIONS: '/portfolio/positions',
  HOLDINGS: '/portfolio/holdings',

  // Market Data
  QUOTES: '/instruments/quotes',
  OHLC: '/instruments/ohlc',
  MASTER: '/instruments/master',
  INDEX_LIST: '/instruments/indexlist',
} as const;

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

// Standard exchange to IIFL exchange segment string
export const IIFL_EXCHANGE_MAP: Record<string, string> = {
  NSE: 'NSECM',
  BSE: 'BSECM',
  NFO: 'NSEFO',
  BFO: 'BSEFO',
  CDS: 'NSECD',
  MCX: 'MCXFO',
  NSE_INDEX: 'NSECM',
  BSE_INDEX: 'BSECM',
};

// IIFL exchange segment to standard exchange
export const IIFL_EXCHANGE_REVERSE_MAP: Record<string, string> = {
  NSECM: 'NSE',
  BSECM: 'BSE',
  NSEFO: 'NFO',
  BSEFO: 'BFO',
  NSECD: 'CDS',
  MCXFO: 'MCX',
};

// ============================================================================
// EXCHANGE SEGMENT ID MAPPINGS (for Market Data API)
// ============================================================================

export const IIFL_EXCHANGE_SEGMENT_ID: Record<string, number> = {
  NSE: 1,
  NSE_INDEX: 1,
  NFO: 2,
  CDS: 3,
  BSE: 11,
  BSE_INDEX: 11,
  BFO: 12,
  MCX: 51,
};

export const IIFL_SEGMENT_ID_TO_EXCHANGE: Record<number, string> = {
  1: 'NSE',
  2: 'NFO',
  3: 'CDS',
  11: 'BSE',
  12: 'BFO',
  51: 'MCX',
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

// Standard product type to IIFL product type
export const IIFL_PRODUCT_MAP: Record<string, string> = {
  CASH: 'CNC',
  CNC: 'CNC',
  INTRADAY: 'MIS',
  MIS: 'MIS',
  MARGIN: 'NRML',
  NRML: 'NRML',
};

// IIFL product type to standard
export const IIFL_PRODUCT_REVERSE_MAP: Record<string, string> = {
  CNC: 'CASH',
  MIS: 'INTRADAY',
  NRML: 'MARGIN',
};

// ============================================================================
// ORDER SIDE MAPPINGS
// ============================================================================

export const IIFL_ORDER_SIDE_MAP: Record<string, string> = {
  BUY: 'BUY',
  SELL: 'SELL',
};

export const IIFL_ORDER_SIDE_REVERSE_MAP: Record<string, string> = {
  BUY: 'BUY',
  SELL: 'SELL',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const IIFL_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  STOP: 'STOPLIMIT',
  STOP_LIMIT: 'STOPLIMIT',
  STOP_MARKET: 'STOPMARKET',
  SL: 'STOPLIMIT',
  'SL-M': 'STOPMARKET',
};

export const IIFL_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  STOPLIMIT: 'STOP_LIMIT',
  STOPMARKET: 'STOP_MARKET',
  Limit: 'LIMIT',
  Market: 'MARKET',
  StopLimit: 'STOP_LIMIT',
  StopMarket: 'STOP_MARKET',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const IIFL_STATUS_MAP: Record<string, string> = {
  New: 'OPEN',
  Filled: 'FILLED',
  PartiallyFilled: 'PARTIALLY_FILLED',
  'Partially Filled': 'PARTIALLY_FILLED',
  Cancelled: 'CANCELLED',
  Rejected: 'REJECTED',
  'Trigger Pending': 'TRIGGER_PENDING',
  Pending: 'PENDING',
  Open: 'OPEN',
  PendingNew: 'PENDING',
};

// ============================================================================
// VALIDITY MAPPINGS
// ============================================================================

export const IIFL_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
  GTC: 'GTC',
  GTD: 'GTD',
};

// ============================================================================
// TIMEFRAME MAPPINGS (Compression Value)
// ============================================================================

export const IIFL_INTERVAL_MAP: Record<string, string> = {
  '1s': '1',
  '1m': '60',
  '2m': '120',
  '3m': '180',
  '5m': '300',
  '10m': '600',
  '15m': '900',
  '30m': '1800',
  '60m': '3600',
  '1h': '3600',
  '1d': 'D',
  '1D': 'D',
  D: 'D',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const IIFL_METADATA: StockBrokerMetadata = {
  id: 'iifl',
  name: 'iifl',
  displayName: 'IIFL Securities',
  logo: 'https://fincept.in/brokers/iifl.png',
  website: 'https://www.iifl.com',

  region: 'india',
  country: 'IN',
  currency: 'INR',
  exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
  marketHours: {
    open: '09:15',
    close: '15:30',
    timezone: 'Asia/Kolkata',
    preMarketOpen: '09:00',
    preMarketClose: '09:08',
  },

  features: {
    webSocket: true,
    amo: true,
    gtt: false,
    bracketOrder: true,
    coverOrder: true,
    marginCalculator: true,
    optionsChain: true,
    paperTrading: false,
  },

  tradingFeatures: {
    marketOrders: true,
    limitOrders: true,
    stopOrders: true,
    stopLimitOrders: true,
    trailingStopOrders: false,
  },

  productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

  authType: 'oauth', // API Key + Secret authentication

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 50, // XTS allows 50 instruments per request
  },

  fees: {
    equity: { brokerage: 20, stt: 0.001 },
    intraday: { brokerage: 20 },
    fno: { brokerage: 20 },
  },

  defaultSymbols: [
    'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK',
    'SBIN', 'BHARTIARTL', 'HINDUNILVR', 'ITC', 'KOTAKBANK',
    'LT', 'AXISBANK', 'MARUTI', 'TATAMOTORS', 'WIPRO',
  ],
};
