/**
 * Zerodha Constants
 *
 * API endpoints, mappings, and configuration for Zerodha Kite Connect
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

export const ZERODHA_API_BASE_URL = 'https://api.kite.trade';
export const ZERODHA_LOGIN_URL = 'https://kite.zerodha.com/connect/login';
export const ZERODHA_API_VERSION = '3';

export const ZERODHA_ENDPOINTS = {
  // Auth
  SESSION_TOKEN: '/session/token',
  USER_PROFILE: '/user/profile',

  // Orders
  ORDERS: '/orders',
  TRADES: '/trades',

  // Portfolio
  POSITIONS: '/portfolio/positions',
  HOLDINGS: '/portfolio/holdings',

  // Account
  MARGINS: '/user/margins',

  // Market Data
  QUOTE: '/quote',
  HISTORICAL: '/instruments/historical',
  INSTRUMENTS: '/instruments',

  // GTT
  GTT: '/gtt/triggers',

  // Margins
  MARGIN_ORDERS: '/margins/orders',
  MARGIN_BASKET: '/margins/basket',
} as const;

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const ZERODHA_EXCHANGE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NFO: 'NFO',
  BFO: 'BFO',
  CDS: 'CDS',
  BCD: 'BCD',
  MCX: 'MCX',
  NSE_INDEX: 'NSE',
  BSE_INDEX: 'BSE',
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

export const ZERODHA_PRODUCT_MAP: Record<string, string> = {
  CASH: 'CNC',
  INTRADAY: 'MIS',
  MARGIN: 'NRML',
  COVER: 'CO',
  BRACKET: 'BO',
};

export const ZERODHA_PRODUCT_REVERSE_MAP: Record<string, string> = {
  CNC: 'CASH',
  MIS: 'INTRADAY',
  NRML: 'MARGIN',
  CO: 'COVER',
  BO: 'BRACKET',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const ZERODHA_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  STOP: 'SL',
  STOP_LIMIT: 'SL-M',
};

export const ZERODHA_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  MARKET: 'MARKET',
  LIMIT: 'LIMIT',
  SL: 'STOP',
  'SL-M': 'STOP_LIMIT',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const ZERODHA_STATUS_MAP: Record<string, string> = {
  PENDING: 'PENDING',
  OPEN: 'OPEN',
  COMPLETE: 'FILLED',
  CANCELLED: 'CANCELLED',
  REJECTED: 'REJECTED',
  MODIFIED: 'OPEN',
  'TRIGGER PENDING': 'TRIGGER_PENDING',
  'AMO SUBMITTED': 'PENDING',
  'PUT ORDER REQ RECEIVED': 'PENDING',
  'VALIDATION PENDING': 'PENDING',
};

// ============================================================================
// VALIDITY MAPPINGS
// ============================================================================

export const ZERODHA_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
  GTC: 'GTC',
  GTD: 'GTD',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const ZERODHA_METADATA: StockBrokerMetadata = {
  id: 'zerodha',
  name: 'zerodha',
  displayName: 'Zerodha (Kite)',
  logo: '/brokers/zerodha.png',
  website: 'https://zerodha.com',

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
