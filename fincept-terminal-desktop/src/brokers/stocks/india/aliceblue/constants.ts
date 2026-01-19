/**
 * Alice Blue Constants
 *
 * API endpoints, mappings, and configuration for Alice Blue Trading API
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

export const ALICEBLUE_API_BASE_URL = 'https://ant.aliceblueonline.com/rest/AliceBlueAPIService/api';
export const ALICEBLUE_LOGIN_URL = 'https://ant.aliceblueonline.com';

export const ALICEBLUE_ENDPOINTS = {
  // Auth
  GET_USER_SID: '/customer/getUserSID',

  // Orders
  PLACE_ORDER: '/placeOrder/executePlaceOrder',
  MODIFY_ORDER: '/placeOrder/modifyOrder',
  CANCEL_ORDER: '/placeOrder/cancelOrder',
  ORDER_BOOK: '/placeOrder/fetchOrderBook',
  TRADE_BOOK: '/placeOrder/fetchTradeBook',

  // Portfolio
  POSITIONS: '/positionAndHoldings/positionBook',
  HOLDINGS: '/positionAndHoldings/holdings',

  // Account
  MARGINS: '/limits/getRmsLimits',

  // Market Data
  SCRIP_QUOTE: '/ScripDetails/getScripQuoteDetails',
  HISTORICAL: '/chart/history',
} as const;

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const ALICEBLUE_EXCHANGE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NFO: 'NFO',
  BFO: 'BFO',
  CDS: 'CDS',
  MCX: 'MCX',
  NSE_INDEX: 'NSE',
  BSE_INDEX: 'BSE',
  MCX_INDEX: 'MCX',
};

export const ALICEBLUE_EXCHANGE_REVERSE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NFO: 'NFO',
  BFO: 'BFO',
  CDS: 'CDS',
  MCX: 'MCX',
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

export const ALICEBLUE_PRODUCT_MAP: Record<string, string> = {
  CASH: 'CNC',
  INTRADAY: 'MIS',
  MARGIN: 'NRML',
  CNC: 'CNC',
  MIS: 'MIS',
  NRML: 'NRML',
};

export const ALICEBLUE_PRODUCT_REVERSE_MAP: Record<string, string> = {
  CNC: 'CASH',
  MIS: 'INTRADAY',
  NRML: 'MARGIN',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const ALICEBLUE_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MKT',
  LIMIT: 'L',
  STOP: 'SL',
  STOP_LIMIT: 'SL-M',
  SL: 'SL',
  'SL-M': 'SL-M',
};

export const ALICEBLUE_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  MKT: 'MARKET',
  L: 'LIMIT',
  SL: 'STOP',
  'SL-M': 'STOP_LIMIT',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const ALICEBLUE_STATUS_MAP: Record<string, string> = {
  open: 'OPEN',
  'trigger pending': 'TRIGGER_PENDING',
  complete: 'FILLED',
  rejected: 'REJECTED',
  cancelled: 'CANCELLED',
  'pending': 'PENDING',
  'after market order req received': 'AMO_PENDING',
};

// ============================================================================
// VALIDITY MAPPINGS
// ============================================================================

export const ALICEBLUE_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
};

// ============================================================================
// TIMEFRAME MAPPINGS
// ============================================================================

export const ALICEBLUE_INTERVAL_MAP: Record<string, string> = {
  '1m': '1',
  '1d': 'D',
  'D': 'D',
  'day': 'D',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const ALICEBLUE_METADATA: StockBrokerMetadata = {
  id: 'aliceblue',
  name: 'aliceblue',
  displayName: 'Alice Blue',
  logo: 'https://fincept.in/brokers/aliceblue.png',
  website: 'https://aliceblueonline.com',

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

  authType: 'enckey', // SHA256 checksum-based auth with encryption key

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 5,
  },

  fees: {
    equity: { brokerage: 15, stt: 0.001 },
    intraday: { brokerage: 15 },
    fno: { brokerage: 15 },
  },

  defaultSymbols: [
    'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK',
    'SBIN', 'BHARTIARTL', 'HINDUNILVR', 'ITC', 'KOTAKBANK',
    'LT', 'AXISBANK', 'MARUTI', 'TATAMOTORS', 'WIPRO',
  ],
};
