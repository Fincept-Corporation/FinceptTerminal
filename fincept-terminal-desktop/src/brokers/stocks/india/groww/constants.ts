/**
 * Groww Constants
 *
 * API endpoints, mappings, and configuration for Groww Trading API
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

export const GROWW_API_BASE_URL = 'https://api.groww.in';
export const GROWW_LOGIN_URL = 'https://groww.in/login';

export const GROWW_ENDPOINTS = {
  // Auth
  ACCESS_TOKEN: '/v1/token/api/access',

  // Orders
  ORDER_CREATE: '/v1/order/create',
  ORDER_MODIFY: '/v1/order/modify',
  ORDER_CANCEL: '/v1/order/cancel',
  ORDER_LIST: '/v1/order/list',
  ORDER_TRADES: '/v1/order/trades',

  // Portfolio
  POSITIONS: '/v1/positions/user',
  HOLDINGS: '/v1/holdings/detail/user',

  // Account
  MARGINS: '/v1/margins/detail/user',

  // Market Data
  QUOTE: '/v1/live-data/quote',
  HISTORICAL: '/v1/historical/candle/range',
} as const;

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const GROWW_EXCHANGE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NFO: 'NSE',  // NFO uses NSE exchange with FNO segment
  BFO: 'BSE',  // BFO uses BSE exchange with FNO segment
};

export const GROWW_EXCHANGE_REVERSE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
};

// ============================================================================
// SEGMENT MAPPINGS
// ============================================================================

export const GROWW_SEGMENT_MAP: Record<string, string> = {
  NSE: 'CASH',
  BSE: 'CASH',
  NFO: 'FNO',
  BFO: 'FNO',
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

export const GROWW_PRODUCT_MAP: Record<string, string> = {
  CASH: 'CNC',
  INTRADAY: 'MIS',
  MARGIN: 'NRML',
};

export const GROWW_PRODUCT_REVERSE_MAP: Record<string, string> = {
  CNC: 'CASH',
  MIS: 'INTRADAY',
  NRML: 'MARGIN',
  INTRADAY: 'INTRADAY',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const GROWW_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  STOP: 'STOP_LOSS_LIMIT',
  STOP_LIMIT: 'STOP_LOSS_MARKET',
  SL: 'STOP_LOSS_LIMIT',
  'SL-M': 'STOP_LOSS_MARKET',
};

export const GROWW_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  STOP_LOSS_LIMIT: 'STOP',
  STOP_LOSS_MARKET: 'STOP_LIMIT',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const GROWW_STATUS_MAP: Record<string, string> = {
  NEW: 'PENDING',
  ACKED: 'OPEN',
  OPEN: 'OPEN',
  TRIGGER_PENDING: 'TRIGGER_PENDING',
  APPROVED: 'OPEN',
  EXECUTED: 'FILLED',
  COMPLETED: 'FILLED',
  CANCELLED: 'CANCELLED',
  REJECTED: 'REJECTED',
  FAILED: 'REJECTED',
  CANCELLATION_REQUESTED: 'PENDING',
  MODIFICATION_REQUESTED: 'OPEN',
  DELIVERY_AWAITED: 'FILLED',
};

// ============================================================================
// VALIDITY MAPPINGS
// ============================================================================

export const GROWW_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
  GTC: 'DAY',  // Groww doesn't support GTC, defaulting to DAY
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const GROWW_METADATA: StockBrokerMetadata = {
  id: 'groww',
  name: 'groww',
  displayName: 'Groww',
  logo: 'https://fincept.in/brokers/groww.png',
  website: 'https://groww.in',

  region: 'india',
  country: 'IN',
  currency: 'INR',
  exchanges: ['NSE', 'BSE', 'NFO', 'BFO'],
  marketHours: {
    open: '09:15',
    close: '15:30',
    timezone: 'Asia/Kolkata',
    preMarketOpen: '09:00',
    preMarketClose: '09:08',
  },

  features: {
    webSocket: false,  // Groww uses REST API polling
    amo: true,
    gtt: false,
    bracketOrder: false,
    coverOrder: false,
    marginCalculator: false,
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

  authType: 'totp',

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 5,
  },

  fees: {
    equity: { brokerage: 0, stt: 0.001 },
    intraday: { brokerage: 20 },
    fno: { brokerage: 20 },
  },

  defaultSymbols: [
    'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK',
    'SBIN', 'BHARTIARTL', 'HINDUNILVR', 'ITC', 'KOTAKBANK',
    'LT', 'AXISBANK', 'MARUTI', 'TATAMOTORS', 'WIPRO',
  ],
};
