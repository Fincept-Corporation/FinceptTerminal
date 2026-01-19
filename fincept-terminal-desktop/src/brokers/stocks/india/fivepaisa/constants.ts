/**
 * 5Paisa Constants
 *
 * API endpoints, mappings, and configuration for 5Paisa Trading API
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

export const FIVEPAISA_API_BASE_URL = 'https://Openapi.5paisa.com';
export const FIVEPAISA_LOGIN_URL = 'https://dev-openapi.5paisa.com/WebVendorLogin/VLogin/Index';

export const FIVEPAISA_ENDPOINTS = {
  // Auth
  TOTP_LOGIN: '/VendorsAPI/Service1.svc/TOTPLogin',
  GET_ACCESS_TOKEN: '/VendorsAPI/Service1.svc/GetAccessToken',

  // Orders
  PLACE_ORDER: '/VendorsAPI/Service1.svc/V1/PlaceOrderRequest',
  MODIFY_ORDER: '/VendorsAPI/Service1.svc/V1/ModifyOrderRequest',
  CANCEL_ORDER: '/VendorsAPI/Service1.svc/V1/CancelOrderRequest',
  ORDER_BOOK: '/VendorsAPI/Service1.svc/V3/OrderBook',
  TRADE_BOOK: '/VendorsAPI/Service1.svc/V1/TradeBook',

  // Portfolio
  POSITIONS: '/VendorsAPI/Service1.svc/V2/NetPositionNetWise',
  HOLDINGS: '/VendorsAPI/Service1.svc/V3/Holding',

  // Account
  MARGIN: '/VendorsAPI/Service1.svc/V4/Margin',

  // Market Data
  MARKET_DEPTH: '/VendorsAPI/Service1.svc/V2/MarketDepth',
  HISTORICAL_DATA: '/VendorsAPI/Service1.svc/V1/HistoricalData',
} as const;

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

// Standard exchange to 5Paisa Exch code
export const FIVEPAISA_EXCHANGE_MAP: Record<string, string> = {
  NSE: 'N',
  BSE: 'B',
  NFO: 'N',
  BFO: 'B',
  CDS: 'N',
  BCD: 'B',
  MCX: 'M',
  NSE_INDEX: 'N',
  BSE_INDEX: 'B',
};

// 5Paisa Exch code to standard exchange
export const FIVEPAISA_EXCHANGE_REVERSE_MAP: Record<string, string> = {
  N: 'NSE',
  B: 'BSE',
  M: 'MCX',
};

// ============================================================================
// EXCHANGE TYPE MAPPINGS
// ============================================================================

// Standard exchange to 5Paisa ExchangeType
export const FIVEPAISA_EXCHANGE_TYPE_MAP: Record<string, string> = {
  NSE: 'C',       // Cash
  BSE: 'C',       // Cash
  NFO: 'D',       // Derivative
  BFO: 'D',       // Derivative
  CDS: 'U',       // Currency
  BCD: 'U',       // Currency
  MCX: 'D',       // Derivative (Commodity)
  NSE_INDEX: 'C',
  BSE_INDEX: 'C',
};

export const FIVEPAISA_EXCHANGE_TYPE_REVERSE_MAP: Record<string, string> = {
  C: 'CASH',
  D: 'DERIVATIVE',
  U: 'CURRENCY',
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

// Standard product to 5Paisa IsIntraday boolean (stored as string for mapping)
export const FIVEPAISA_PRODUCT_MAP: Record<string, boolean> = {
  CASH: false,
  CNC: false,
  NRML: false,
  MARGIN: false,
  INTRADAY: true,
  MIS: true,
};

export const FIVEPAISA_PRODUCT_REVERSE_MAP: Record<string, string> = {
  'true': 'INTRADAY',
  'false': 'CASH',
};

// ============================================================================
// ORDER SIDE MAPPINGS
// ============================================================================

export const FIVEPAISA_ORDER_SIDE_MAP: Record<string, string> = {
  BUY: 'B',
  SELL: 'S',
};

export const FIVEPAISA_ORDER_SIDE_REVERSE_MAP: Record<string, string> = {
  B: 'BUY',
  S: 'SELL',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const FIVEPAISA_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'N',
  LIMIT: 'L',
  STOP: 'SL',
  STOP_LIMIT: 'SL-M',
};

export const FIVEPAISA_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  N: 'MARKET',
  L: 'LIMIT',
  SL: 'STOP',
  'SL-M': 'STOP_LIMIT',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const FIVEPAISA_STATUS_MAP: Record<string, string> = {
  Pending: 'PENDING',
  'Fully Executed': 'FILLED',
  'Partially Executed': 'PARTIALLY_FILLED',
  Cancelled: 'CANCELLED',
  Rejected: 'REJECTED',
  'Trigger Pending': 'TRIGGER_PENDING',
  'Order not placed': 'REJECTED',
  Open: 'OPEN',
  'Requested': 'PENDING',
};

// ============================================================================
// VALIDITY MAPPINGS
// ============================================================================

export const FIVEPAISA_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
  GTC: 'GTC',
};

// ============================================================================
// TIMEFRAME MAPPINGS (Resolution)
// ============================================================================

export const FIVEPAISA_INTERVAL_MAP: Record<string, string> = {
  '1m': '1m',
  '5m': '5m',
  '10m': '10m',
  '15m': '15m',
  '30m': '30m',
  '1h': '60m',
  '1d': '1d',
  '1w': '1w',
  '1M': '1M',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const FIVEPAISA_METADATA: StockBrokerMetadata = {
  id: 'fivepaisa',
  name: 'fivepaisa',
  displayName: '5Paisa',
  logo: 'https://fincept.in/brokers/5paisa.png',
  website: 'https://www.5paisa.com',

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

  authType: 'totp', // Two-step TOTP authentication

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 5,
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
