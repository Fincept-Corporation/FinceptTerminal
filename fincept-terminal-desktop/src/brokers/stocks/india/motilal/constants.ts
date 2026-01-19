/**
 * Motilal Oswal Constants
 *
 * API endpoints, mappings, and configuration for Motilal Oswal Trading API
 * Reference: https://openapi.motilaloswal.com
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

export const MOTILAL_BASE_URL = 'https://openapi.motilaloswal.com';

export const MOTILAL_ENDPOINTS = {
  // Auth
  LOGIN: '/rest/login/v3/authdirectapi',

  // Orders
  PLACE_ORDER: '/rest/trans/v1/placeorder',
  MODIFY_ORDER: '/rest/trans/v2/modifyorder',
  CANCEL_ORDER: '/rest/trans/v1/cancelorder',
  ORDER_BOOK: '/rest/book/v2/getorderbook',
  TRADE_BOOK: '/rest/book/v1/gettradebook',

  // Portfolio
  POSITIONS: '/rest/book/v1/getposition',
  HOLDINGS: '/rest/report/v1/getdpholding',
  MARGIN: '/rest/report/v1/getreportmargindetail',

  // Market Data
  LTP: '/rest/report/v1/getltpdata',
  SEARCH: '/rest/report/v1/getsearchscrip',
  MASTER: '/rest/report/v1/getcontractmaster',
} as const;

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

// Standard exchange to Motilal exchange
export const MOTILAL_EXCHANGE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NFO: 'NSEFO',
  BFO: 'BSEFO',
  CDS: 'NSECD',
  MCX: 'MCX',
  NSE_INDEX: 'NSE',
  BSE_INDEX: 'BSE',
};

// Motilal exchange to standard exchange
export const MOTILAL_EXCHANGE_REVERSE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NSEFO: 'NFO',
  BSEFO: 'BFO',
  NSECD: 'CDS',
  MCX: 'MCX',
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

// Standard product type to Motilal product type
// Cash segment: CNC -> DELIVERY, MIS -> VALUEPLUS
// F&O segment: NRML -> NORMAL, MIS -> NORMAL
export const MOTILAL_PRODUCT_MAP: Record<string, Record<string, string>> = {
  // Cash segment (NSE, BSE)
  CASH: {
    CNC: 'DELIVERY',
    CASH: 'DELIVERY',
    MIS: 'VALUEPLUS',
    INTRADAY: 'VALUEPLUS',
    NRML: 'VALUEPLUS',
    MARGIN: 'VALUEPLUS',
  },
  // F&O segment (NFO, BFO, CDS, MCX)
  FO: {
    NRML: 'NORMAL',
    MARGIN: 'NORMAL',
    MIS: 'NORMAL',
    INTRADAY: 'NORMAL',
    CNC: 'NORMAL',
    CASH: 'NORMAL',
  },
};

// Motilal product type to standard
export const MOTILAL_PRODUCT_REVERSE_MAP: Record<string, string> = {
  DELIVERY: 'CASH',
  VALUEPLUS: 'INTRADAY',
  NORMAL: 'MARGIN',
  BTST: 'INTRADAY',
  MTF: 'MARGIN',
};

// ============================================================================
// ORDER SIDE MAPPINGS
// ============================================================================

export const MOTILAL_ORDER_SIDE_MAP: Record<string, string> = {
  BUY: 'BUY',
  SELL: 'SELL',
};

export const MOTILAL_ORDER_SIDE_REVERSE_MAP: Record<string, string> = {
  BUY: 'BUY',
  SELL: 'SELL',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const MOTILAL_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  STOP: 'STOPLOSS',
  STOP_LIMIT: 'STOPLOSS',
  STOP_MARKET: 'STOPLOSS',
  SL: 'STOPLOSS',
  'SL-M': 'STOPLOSS',
};

export const MOTILAL_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  Market: 'MARKET',
  LIMIT: 'LIMIT',
  Limit: 'LIMIT',
  STOPLOSS: 'STOP_LIMIT',
  Stoploss: 'STOP_LIMIT',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const MOTILAL_STATUS_MAP: Record<string, string> = {
  Traded: 'FILLED',
  Complete: 'FILLED',
  Confirm: 'OPEN',
  Sent: 'OPEN',
  Open: 'OPEN',
  Rejected: 'REJECTED',
  Error: 'REJECTED',
  Cancel: 'CANCELLED',
};

// ============================================================================
// VALIDITY MAPPINGS
// ============================================================================

export const MOTILAL_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
  GTC: 'GTC',
  GTD: 'GTD',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const MOTILAL_METADATA: StockBrokerMetadata = {
  id: 'motilal',
  name: 'motilal',
  displayName: 'Motilal Oswal',
  logo: 'https://fincept.in/brokers/motilal.png',
  website: 'https://www.motilaloswal.com',

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
    gtt: false, // GTT not mentioned in OpenAlgo
    bracketOrder: false,
    coverOrder: false,
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

  authType: 'password', // Password + 2FA + optional TOTP

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 20,
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
