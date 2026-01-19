/**
 * Kotak Neo Constants
 *
 * API endpoints and mappings for Kotak Neo broker integration.
 * Based on OpenAlgo Kotak implementation.
 */

import type { StockBrokerMetadata, StockExchange } from '../../types';

// API Endpoints
export const KOTAK_AUTH_BASE = 'https://gw-napi.kotaksecurities.com';
export const KOTAK_API_BASE = 'https://gw-napi.kotaksecurities.com';
export const KOTAK_QUOTES_BASE = 'https://lapi.kotaksecurities.com';

// Broker Metadata
export const KOTAK_METADATA: StockBrokerMetadata = {
  id: 'kotak',
  name: 'kotak',
  displayName: 'Kotak Neo',
  logo: 'https://fincept.in/brokers/kotak.png',
  website: 'https://www.kotaksecurities.com',
  region: 'india',
  country: 'IN',
  currency: 'INR',
  exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'BCD', 'MCX'],
  marketHours: {
    open: '09:15',
    close: '15:30',
    timezone: 'Asia/Kolkata',
  },
  features: {
    webSocket: true,
    amo: true,
    gtt: false, // Kotak Neo doesn't support GTT
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
  authType: 'totp', // Kotak uses TOTP + MPIN
  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 5,
  },
  defaultSymbols: ['RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK'],
};

// Exchange mapping (internal → Kotak Neo API)
export const KOTAK_EXCHANGE_MAP: Record<StockExchange, string> = {
  NSE: 'nse_cm',
  BSE: 'bse_cm',
  NFO: 'nse_fo',
  BFO: 'bse_fo',
  CDS: 'cde_fo',
  BCD: 'bcs_fo',
  MCX: 'mcx_fo',
  // Index exchanges (internal convention)
  NSE_INDEX: 'nse_cm',
  BSE_INDEX: 'bse_cm',
};

// Reverse exchange mapping (Kotak API → internal)
export const KOTAK_EXCHANGE_REVERSE_MAP: Record<string, StockExchange> = {
  nse_cm: 'NSE',
  bse_cm: 'BSE',
  nse_fo: 'NFO',
  bse_fo: 'BFO',
  cde_fo: 'CDS',
  bcs_fo: 'BCD',
  mcx_fo: 'MCX',
};

// Product type mapping (internal → Kotak API)
export const KOTAK_PRODUCT_MAP: Record<string, string> = {
  CASH: 'CNC',
  CNC: 'CNC',
  INTRADAY: 'MIS',
  MIS: 'MIS',
  MARGIN: 'NRML',
  NRML: 'NRML',
};

// Reverse product type mapping (Kotak API → internal)
export const KOTAK_PRODUCT_REVERSE_MAP: Record<string, string> = {
  CNC: 'CASH',
  MIS: 'INTRADAY',
  NRML: 'MARGIN',
};

// Order type mapping (internal → Kotak API)
// Internal: MARKET, LIMIT, STOP, STOP_LIMIT
// Kotak: MKT, L, SL, SL-M
export const KOTAK_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MKT',
  LIMIT: 'L',
  STOP: 'SL',
  STOP_LIMIT: 'SL',
  // Legacy mappings for compatibility
  SL: 'SL',
  'SL-M': 'SL-M',
};

// Reverse order type mapping (Kotak API → internal)
export const KOTAK_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  MKT: 'MARKET',
  L: 'LIMIT',
  SL: 'STOP',
  'SL-M': 'STOP',
};

// Order validity mapping
export const KOTAK_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
};
