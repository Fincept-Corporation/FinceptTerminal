/**
 * Dhan Constants
 */

import type { StockBrokerMetadata, StockExchange } from '../../types';

// API Endpoints
export const DHAN_API_BASE = 'https://api.dhan.co';
export const DHAN_AUTH_BASE = 'https://auth.dhan.co';

// Broker Metadata
export const DHAN_METADATA: StockBrokerMetadata = {
  id: 'dhan',
  name: 'dhan',
  displayName: 'Dhan',
  website: 'https://dhan.co',
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
    quotesPerSecond: 1, // Dhan has strict rate limits
  },
  defaultSymbols: ['RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK'],
};

// Exchange mapping (internal → Dhan API)
export const DHAN_EXCHANGE_MAP: Record<StockExchange, string> = {
  NSE: 'NSE_EQ',
  BSE: 'BSE_EQ',
  NFO: 'NSE_FNO',
  BFO: 'BSE_FNO',
  MCX: 'MCX_COMM',
  CDS: 'NSE_CURRENCY',
  BCD: 'BSE_CURRENCY',
  // Index exchanges
  NSE_INDEX: 'IDX_I',
  BSE_INDEX: 'IDX_I',
};

// Reverse exchange mapping (Dhan API → internal)
export const DHAN_EXCHANGE_REVERSE_MAP: Record<string, StockExchange> = {
  NSE_EQ: 'NSE',
  BSE_EQ: 'BSE',
  NSE_FNO: 'NFO',
  BSE_FNO: 'BFO',
  MCX_COMM: 'MCX',
  NSE_CURRENCY: 'CDS',
  BSE_CURRENCY: 'BCD',
  IDX_I: 'NSE_INDEX',
};

// Product type mapping (internal → Dhan API)
export const DHAN_PRODUCT_MAP: Record<string, string> = {
  CASH: 'CNC',
  CNC: 'CNC',
  INTRADAY: 'INTRADAY',
  MIS: 'INTRADAY',
  MARGIN: 'MARGIN',
  NRML: 'MARGIN',
};

// Reverse product type mapping (Dhan API → internal)
export const DHAN_PRODUCT_REVERSE_MAP: Record<string, string> = {
  CNC: 'CASH',
  INTRADAY: 'INTRADAY',
  MARGIN: 'MARGIN',
};

// Order type mapping (internal → Dhan API)
// Internal: MARKET, LIMIT, STOP, STOP_LIMIT
// Dhan: MARKET, LIMIT, STOP_LOSS, STOP_LOSS_MARKET
export const DHAN_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  STOP: 'STOP_LOSS',
  STOP_LIMIT: 'STOP_LOSS',  // Dhan doesn't have stop-limit, use stop-loss
  // Legacy mappings for compatibility
  SL: 'STOP_LOSS',
  'SL-M': 'STOP_LOSS_MARKET',
  STOP_LOSS: 'STOP_LOSS',
  STOP_LOSS_MARKET: 'STOP_LOSS_MARKET',
};

// Interval mapping for historical data
export const DHAN_INTERVAL_MAP: Record<string, string> = {
  '1m': '1',
  '5m': '5',
  '15m': '15',
  '25m': '25',
  '1h': '60',
  '1D': 'D',
  'D': 'D',
};
