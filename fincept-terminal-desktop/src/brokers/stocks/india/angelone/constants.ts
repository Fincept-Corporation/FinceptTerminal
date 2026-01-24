/**
 * Angel One (AngelBroking) Constants
 *
 * API endpoints, mappings, and configuration for Angel One Smart API
 * Based on OpenAlgo broker integration patterns
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

export const ANGELONE_API_BASE_URL = 'https://apiconnect.angelbroking.com';
export const ANGELONE_LOGIN_URL = 'https://smartapi.angelbroking.com/publisher-login';
export const ANGELONE_API_VERSION = 'v1';

export const ANGELONE_ENDPOINTS = {
  // Auth
  LOGIN: '/rest/auth/angelbroking/user/v1/loginByPassword',
  GENERATE_TOKEN: '/rest/auth/angelbroking/user/v1/generateToken',
  LOGOUT: '/rest/secure/angelbroking/user/v1/logout',
  GET_PROFILE: '/rest/secure/angelbroking/user/v1/getProfile',

  // Orders
  PLACE_ORDER: '/rest/secure/angelbroking/order/v1/placeOrder',
  MODIFY_ORDER: '/rest/secure/angelbroking/order/v1/modifyOrder',
  CANCEL_ORDER: '/rest/secure/angelbroking/order/v1/cancelOrder',
  ORDER_BOOK: '/rest/secure/angelbroking/order/v1/getOrderBook',
  TRADE_BOOK: '/rest/secure/angelbroking/order/v1/getTradeBook',

  // Portfolio
  POSITIONS: '/rest/secure/angelbroking/order/v1/getPosition',
  HOLDINGS: '/rest/secure/angelbroking/portfolio/v1/getAllHolding',

  // Account
  RMS: '/rest/secure/angelbroking/user/v1/getRMS',

  // Market Data
  QUOTE: '/rest/secure/angelbroking/market/v1/quote/',
  LTP: '/rest/secure/angelbroking/order/v1/getLtpData',
  HISTORICAL: '/rest/secure/angelbroking/historical/v1/getCandleData',
  OI_DATA: '/rest/secure/angelbroking/historical/v1/getOIData',

  // Search
  SEARCH: '/rest/secure/angelbroking/order/v1/searchScrip',

  // Margin
  MARGIN: '/rest/secure/angelbroking/margin/v1/batch',
} as const;

// ============================================================================
// WEBSOCKET CONFIGURATION
// ============================================================================

export const ANGELONE_WS_URL = 'wss://smartapisocket.angelone.in/smart-stream';

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const ANGELONE_EXCHANGE_MAP: Record<string, string> = {
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

// Exchange segment codes for WebSocket
export const ANGELONE_EXCHANGE_SEGMENTS: Record<string, number> = {
  NSE: 1,   // NSE Cash
  NFO: 2,   // NSE F&O
  BSE: 3,   // BSE Cash
  BFO: 4,   // BSE F&O
  MCX: 5,   // MCX
  CDS: 7,   // Currency
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

// OpenAlgo/Fincept -> Angel One
export const ANGELONE_PRODUCT_MAP: Record<string, string> = {
  CASH: 'DELIVERY',
  CNC: 'DELIVERY',
  INTRADAY: 'INTRADAY',
  MIS: 'INTRADAY',
  MARGIN: 'CARRYFORWARD',
  NRML: 'CARRYFORWARD',
};

// Angel One -> OpenAlgo/Fincept
export const ANGELONE_PRODUCT_REVERSE_MAP: Record<string, string> = {
  DELIVERY: 'CASH',
  INTRADAY: 'INTRADAY',
  CARRYFORWARD: 'MARGIN',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

// OpenAlgo/Fincept -> Angel One
export const ANGELONE_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  STOP: 'STOPLOSS_LIMIT',
  STOP_LIMIT: 'STOPLOSS_LIMIT',
  STOP_MARKET: 'STOPLOSS_MARKET',
  SL: 'STOPLOSS_LIMIT',
  'SL-M': 'STOPLOSS_MARKET',
};

// Angel One -> OpenAlgo/Fincept
export const ANGELONE_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  STOPLOSS_LIMIT: 'STOP_LIMIT',
  STOPLOSS_MARKET: 'STOP_MARKET',
};

// ============================================================================
// VARIETY MAPPINGS
// ============================================================================

// Pricetype -> Angel One Variety
export const ANGELONE_VARIETY_MAP: Record<string, string> = {
  MARKET: 'NORMAL',
  LIMIT: 'NORMAL',
  STOP: 'STOPLOSS',
  STOP_LIMIT: 'STOPLOSS',
  STOP_MARKET: 'STOPLOSS',
  SL: 'STOPLOSS',
  'SL-M': 'STOPLOSS',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const ANGELONE_STATUS_MAP: Record<string, string> = {
  complete: 'FILLED',
  open: 'OPEN',
  pending: 'PENDING',
  rejected: 'REJECTED',
  cancelled: 'CANCELLED',
  'trigger pending': 'TRIGGER_PENDING',
  'validation pending': 'PENDING',
  'put order req received': 'PENDING',
  'modify pending': 'PENDING',
  'cancel pending': 'PENDING',
  'open pending': 'PENDING',
  'not modified': 'OPEN',
  'not cancelled': 'OPEN',
  'modify validation pending': 'PENDING',
  'trigger validation pending': 'TRIGGER_PENDING',
  'amo submitted': 'PENDING',
  'amo cancelled': 'CANCELLED',
  'amo req received': 'PENDING',
};

// ============================================================================
// VALIDITY MAPPINGS
// ============================================================================

export const ANGELONE_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
};

// ============================================================================
// TIMEFRAME MAPPINGS
// ============================================================================

// Standard timeframe -> Angel One interval
export const ANGELONE_INTERVAL_MAP: Record<string, string> = {
  '1m': 'ONE_MINUTE',
  '3m': 'THREE_MINUTE',
  '5m': 'FIVE_MINUTE',
  '10m': 'TEN_MINUTE',
  '15m': 'FIFTEEN_MINUTE',
  '30m': 'THIRTY_MINUTE',
  '1h': 'ONE_HOUR',
  '1d': 'ONE_DAY',
  D: 'ONE_DAY',
};

// Angel One interval limits (max days per request)
export const ANGELONE_INTERVAL_LIMITS: Record<string, number> = {
  '1m': 30,
  '3m': 60,
  '5m': 100,
  '10m': 100,
  '15m': 200,
  '30m': 200,
  '1h': 400,
  '1d': 2000,
  D: 2000,
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const ANGELONE_METADATA: StockBrokerMetadata = {
  id: 'angelone',
  name: 'angelone',
  displayName: 'Angel One',
  logo: '/brokers/angelone.png',
  website: 'https://www.angelone.in',

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
    gtt: true,
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

  authType: 'totp',

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 1, // Angel One has 1 request per second for quotes
  },

  fees: {
    equity: { brokerage: 0, stt: 0.001 },
    intraday: { brokerage: 20 },
    fno: { brokerage: 20 },
  },

  defaultSymbols: [
    // NIFTY 50 Components (Jan 2026)
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
  ],
};

// ============================================================================
// API HEADERS TEMPLATE
// ============================================================================

export function getAngelOneHeaders(authToken: string, apiKey: string): Record<string, string> {
  return {
    'Authorization': `Bearer ${authToken}`,
    'Content-Type': 'application/json',
    'Accept': 'application/json',
    'X-UserType': 'USER',
    'X-SourceID': 'WEB',
    'X-ClientLocalIP': 'CLIENT_LOCAL_IP',
    'X-ClientPublicIP': 'CLIENT_PUBLIC_IP',
    'X-MACAddress': 'MAC_ADDRESS',
    'X-PrivateKey': apiKey,
  };
}
