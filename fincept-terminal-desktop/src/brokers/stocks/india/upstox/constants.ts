/**
 * Upstox Constants
 */

import type { StockBrokerMetadata, StockExchange } from '../../types';

// API Endpoints
export const UPSTOX_API_BASE_V2 = 'https://api.upstox.com/v2';
export const UPSTOX_API_BASE_V3 = 'https://api.upstox.com/v3';
export const UPSTOX_LOGIN_URL = 'https://api.upstox.com/v2/login/authorization/dialog';

// Broker Metadata
export const UPSTOX_METADATA: StockBrokerMetadata = {
  id: 'upstox',
  name: 'upstox',
  displayName: 'Upstox',
  website: 'https://upstox.com',
  region: 'india',
  country: 'IN',
  currency: 'INR',
  exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'MCX', 'CDS'],
  marketHours: {
    open: '09:15',
    close: '15:30',
    timezone: 'Asia/Kolkata',
  },
  features: {
    webSocket: true,
    amo: true,
    gtt: true,
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
  authType: 'oauth',
  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 10,
  },
  defaultSymbols: ['RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK'],
};

// Exchange mapping (internal → Upstox API)
export const UPSTOX_EXCHANGE_MAP: Record<StockExchange, string> = {
  NSE: 'NSE_EQ',
  BSE: 'BSE_EQ',
  NFO: 'NSE_FO',
  BFO: 'BSE_FO',
  MCX: 'MCX_FO',
  CDS: 'NCD_FO',
  // Index exchanges
  NSE_INDEX: 'NSE_INDEX',
  BSE_INDEX: 'BSE_INDEX',
};

// Reverse exchange mapping (Upstox API → internal)
export const UPSTOX_EXCHANGE_REVERSE_MAP: Record<string, StockExchange> = {
  NSE_EQ: 'NSE',
  BSE_EQ: 'BSE',
  NSE_FO: 'NFO',
  BSE_FO: 'BFO',
  MCX_FO: 'MCX',
  NCD_FO: 'CDS',
  NSE_INDEX: 'NSE',
  BSE_INDEX: 'BSE',
};

// Product type mapping (internal → Upstox API)
export const UPSTOX_PRODUCT_MAP: Record<string, string> = {
  CNC: 'D',    // Delivery
  MIS: 'I',    // Intraday
  NRML: 'D',   // Delivery for F&O
};

// Reverse product type mapping
export const UPSTOX_PRODUCT_REVERSE_MAP: Record<string, Record<string, string>> = {
  D: {
    NSE: 'CNC',
    BSE: 'CNC',
    NFO: 'NRML',
    BFO: 'NRML',
    MCX: 'NRML',
    CDS: 'NRML',
  },
  I: {
    NSE: 'MIS',
    BSE: 'MIS',
    NFO: 'MIS',
    BFO: 'MIS',
    MCX: 'MIS',
    CDS: 'MIS',
  },
};

// Order type mapping
export const UPSTOX_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  SL: 'SL',
  'SL-M': 'SL-M',
  'STOP_LOSS': 'SL',
  'STOP_LOSS_MARKET': 'SL-M',
};

// Interval mapping for historical data
export const UPSTOX_INTERVAL_MAP: Record<string, { unit: string; interval: string }> = {
  '1m': { unit: 'minute', interval: '1' },
  '5m': { unit: 'minute', interval: '5' },
  '15m': { unit: 'minute', interval: '15' },
  '30m': { unit: 'minute', interval: '30' },
  '1h': { unit: 'minute', interval: '60' },
  '1D': { unit: 'day', interval: '1' },
  '1W': { unit: 'week', interval: '1' },
  '1M': { unit: 'month', interval: '1' },
};
