/**
 * Fyers Constants
 *
 * API endpoints, mappings, and configuration for Fyers API v3
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

export const FYERS_API_BASE_URL = 'https://api-t1.fyers.in';
export const FYERS_LOGIN_URL = 'https://api-t1.fyers.in/api/v3/generate-authcode';
export const FYERS_API_VERSION = 'v3';

export const FYERS_ENDPOINTS = {
  // Auth
  VALIDATE_AUTHCODE: '/api/v3/validate-authcode',
  GENERATE_AUTHCODE: '/api/v3/generate-authcode',

  // Orders
  ORDERS: '/api/v3/orders',
  TRADEBOOK: '/api/v3/tradebook',

  // Portfolio
  POSITIONS: '/api/v3/positions',
  HOLDINGS: '/api/v3/holdings',

  // Account
  FUNDS: '/api/v3/funds',
  PROFILE: '/api/v3/profile',

  // Market Data
  QUOTES: '/data-rest/v3/quotes',
  HISTORICAL: '/data-rest/v3/history',
  MARKET_DEPTH: '/data-rest/v3/depth',
  SYMBOL_MASTER: '/data-rest/v3/symbol-master',
} as const;

// ============================================================================
// WEBSOCKET CONFIGURATION
// ============================================================================

export const FYERS_WS_URL = 'wss://socket.fyers.in/hsm/v1-5/prod';

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const FYERS_EXCHANGE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NFO: 'NFO',
  BFO: 'BFO',
  CDS: 'CDS',
  MCX: 'MCX',
  NSE_INDEX: 'NSE',
  BSE_INDEX: 'BSE',
};

// Exchange segment codes (used in symbol format)
export const FYERS_EXCHANGE_SEGMENTS: Record<string, string> = {
  NSE: '10:10',      // NSE Cash
  NFO: '10:11',      // NSE F&O
  BSE: '12:10',      // BSE Cash
  BFO: '12:11',      // BSE F&O
  CDS: '10:12',      // Currency Derivatives
  MCX: '11:20',      // MCX F&O
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

export const FYERS_PRODUCT_MAP: Record<string, string> = {
  CASH: 'CNC',
  INTRADAY: 'INTRADAY',
  MARGIN: 'MARGIN',
  COVER: 'CO',
  BRACKET: 'BO',
};

export const FYERS_PRODUCT_REVERSE_MAP: Record<string, string> = {
  CNC: 'CASH',
  INTRADAY: 'INTRADAY',
  MARGIN: 'MARGIN',
  CO: 'COVER',
  BO: 'BRACKET',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

// Fyers order types: 1=Limit, 2=Market, 3=Stop Loss Limit, 4=Stop Loss Market
export const FYERS_ORDER_TYPE_MAP: Record<string, number> = {
  MARKET: 2,
  LIMIT: 1,
  STOP_LIMIT: 3,
  STOP_MARKET: 4,
};

export const FYERS_ORDER_TYPE_REVERSE_MAP: Record<number, string> = {
  1: 'LIMIT',
  2: 'MARKET',
  3: 'STOP_LIMIT',
  4: 'STOP_MARKET',
};

// ============================================================================
// ORDER SIDE MAPPINGS
// ============================================================================

// Fyers side: 1=Buy, -1=Sell
export const FYERS_SIDE_MAP: Record<string, number> = {
  BUY: 1,
  SELL: -1,
};

export const FYERS_SIDE_REVERSE_MAP: Record<string, string> = {
  '1': 'BUY',
  '-1': 'SELL',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

// Fyers status codes
export const FYERS_STATUS_MAP: Record<number, string> = {
  1: 'PENDING',      // Pending
  2: 'FILLED',       // Filled/Executed
  3: 'CANCELLED',    // Cancelled
  4: 'REJECTED',     // Rejected
  5: 'REJECTED',     // Invalid Order
  6: 'OPEN',         // Open/Placed
  7: 'PENDING',      // Pending
};

// ============================================================================
// VALIDITY MAPPINGS
// ============================================================================

export const FYERS_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const FYERS_METADATA: StockBrokerMetadata = {
  id: 'fyers',
  name: 'fyers',
  displayName: 'Fyers',
  logo: '/brokers/fyers.png',
  website: 'https://fyers.in',

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

  authType: 'oauth',

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 5,
  },

  fees: {
    equity: { brokerage: 0, stt: 0.001 },
    intraday: { brokerage: 0.0003 },
    fno: { brokerage: 20 },
  },

  defaultSymbols: [
    'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK',
    'SBIN', 'BHARTIARTL', 'HINDUNILVR', 'ITC', 'KOTAKBANK',
    'LT', 'AXISBANK', 'MARUTI', 'TATAMOTORS', 'WIPRO',
  ],
};
