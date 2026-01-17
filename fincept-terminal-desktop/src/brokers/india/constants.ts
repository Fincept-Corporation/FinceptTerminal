/**
 * Constants for Indian stock broker integrations
 */

import type { IndianExchange, ProductType, OrderType, TimeFrame } from './types';

// ============================================================================
// Market Timing
// ============================================================================

export const MARKET_TIMING = {
  NSE: {
    preOpen: { start: '09:00', end: '09:08' },
    normal: { start: '09:15', end: '15:30' },
    postClose: { start: '15:40', end: '16:00' },
    timezone: 'Asia/Kolkata',
  },
  BSE: {
    preOpen: { start: '09:00', end: '09:08' },
    normal: { start: '09:15', end: '15:30' },
    postClose: { start: '15:40', end: '16:00' },
    timezone: 'Asia/Kolkata',
  },
  MCX: {
    morning: { start: '09:00', end: '17:00' },
    evening: { start: '17:00', end: '23:30' },
    timezone: 'Asia/Kolkata',
  },
  CDS: {
    normal: { start: '09:00', end: '17:00' },
    timezone: 'Asia/Kolkata',
  },
} as const;

// ============================================================================
// Exchange Mappings
// ============================================================================

export const EXCHANGE_DISPLAY_NAMES: Record<IndianExchange, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NFO: 'NSE F&O',
  BFO: 'BSE F&O',
  CDS: 'Currency',
  BCD: 'BSE Currency',
  MCX: 'MCX',
  NSE_INDEX: 'NSE Index',
  BSE_INDEX: 'BSE Index',
  MCX_INDEX: 'MCX Index',
  CDS_INDEX: 'Currency Index',
};

export const EQUITY_EXCHANGES: IndianExchange[] = ['NSE', 'BSE'];
export const DERIVATIVE_EXCHANGES: IndianExchange[] = ['NFO', 'BFO', 'MCX', 'CDS', 'BCD'];
export const INDEX_EXCHANGES: IndianExchange[] = ['NSE_INDEX', 'BSE_INDEX', 'MCX_INDEX', 'CDS_INDEX'];

// ============================================================================
// Product Type Mappings
// ============================================================================

export const PRODUCT_DISPLAY_NAMES: Record<ProductType, string> = {
  CNC: 'Delivery',
  MIS: 'Intraday',
  NRML: 'Carry Forward',
  CO: 'Cover Order',
  BO: 'Bracket Order',
};

export const PRODUCT_DESCRIPTIONS: Record<ProductType, string> = {
  CNC: 'Cash and Carry - Take delivery of shares',
  MIS: 'Margin Intraday - Auto squared off at 3:20 PM',
  NRML: 'Normal - Carry forward for F&O positions',
  CO: 'Cover Order - Compulsory stop loss',
  BO: 'Bracket Order - Target and stop loss',
};

// ============================================================================
// Order Type Mappings
// ============================================================================

export const ORDER_TYPE_DISPLAY_NAMES: Record<OrderType, string> = {
  MARKET: 'Market',
  LIMIT: 'Limit',
  SL: 'Stop Loss',
  'SL-M': 'Stop Loss Market',
};

// ============================================================================
// Index Symbols Mapping
// ============================================================================

export const INDEX_SYMBOL_MAP: Record<string, string> = {
  'NIFTY 50': 'NIFTY',
  'NIFTY NEXT 50': 'NIFTYNXT50',
  'NIFTY FIN SERVICE': 'FINNIFTY',
  'NIFTY BANK': 'BANKNIFTY',
  'NIFTY MID SELECT': 'MIDCPNIFTY',
  'INDIA VIX': 'INDIAVIX',
  'SENSEX': 'SENSEX',
  'BANKEX': 'BANKEX',
  'SNSX50': 'SENSEX50',
};

export const POPULAR_INDICES = [
  { symbol: 'NIFTY', name: 'Nifty 50', exchange: 'NSE_INDEX' as IndianExchange },
  { symbol: 'BANKNIFTY', name: 'Bank Nifty', exchange: 'NSE_INDEX' as IndianExchange },
  { symbol: 'FINNIFTY', name: 'Fin Nifty', exchange: 'NSE_INDEX' as IndianExchange },
  { symbol: 'MIDCPNIFTY', name: 'Midcap Nifty', exchange: 'NSE_INDEX' as IndianExchange },
  { symbol: 'SENSEX', name: 'Sensex', exchange: 'BSE_INDEX' as IndianExchange },
  { symbol: 'BANKEX', name: 'Bankex', exchange: 'BSE_INDEX' as IndianExchange },
];

// ============================================================================
// Timeframe Mappings
// ============================================================================

export const TIMEFRAME_MINUTES: Record<TimeFrame, number> = {
  '1m': 1,
  '3m': 3,
  '5m': 5,
  '10m': 10,
  '15m': 15,
  '30m': 30,
  '1h': 60,
  '2h': 120,
  '4h': 240,
  '1d': 1440,
  '1w': 10080,
  '1M': 43200,
};

export const TIMEFRAME_DISPLAY_NAMES: Record<TimeFrame, string> = {
  '1m': '1 Minute',
  '3m': '3 Minutes',
  '5m': '5 Minutes',
  '10m': '10 Minutes',
  '15m': '15 Minutes',
  '30m': '30 Minutes',
  '1h': '1 Hour',
  '2h': '2 Hours',
  '4h': '4 Hours',
  '1d': 'Daily',
  '1w': 'Weekly',
  '1M': 'Monthly',
};

// ============================================================================
// Lot Sizes for Popular F&O Stocks
// ============================================================================

export const DEFAULT_LOT_SIZES: Record<string, number> = {
  NIFTY: 50,
  BANKNIFTY: 15,
  FINNIFTY: 40,
  MIDCPNIFTY: 75,
  SENSEX: 10,
  BANKEX: 15,
  RELIANCE: 250,
  TCS: 150,
  INFY: 300,
  HDFCBANK: 550,
  ICICIBANK: 700,
  SBIN: 750,
  TATAMOTORS: 575,
  TATASTEEL: 550,
};

// ============================================================================
// Error Codes
// ============================================================================

export const ERROR_CODES = {
  // Authentication errors
  AUTH_INVALID_CREDENTIALS: 'AUTH001',
  AUTH_TOKEN_EXPIRED: 'AUTH002',
  AUTH_SESSION_EXPIRED: 'AUTH003',
  AUTH_TOTP_REQUIRED: 'AUTH004',
  AUTH_TOTP_INVALID: 'AUTH005',

  // Order errors
  ORDER_INVALID_SYMBOL: 'ORD001',
  ORDER_INVALID_QUANTITY: 'ORD002',
  ORDER_INSUFFICIENT_FUNDS: 'ORD003',
  ORDER_MARKET_CLOSED: 'ORD004',
  ORDER_PRICE_OUT_OF_RANGE: 'ORD005',
  ORDER_MODIFICATION_NOT_ALLOWED: 'ORD006',
  ORDER_CANCELLATION_NOT_ALLOWED: 'ORD007',

  // Connection errors
  CONNECTION_FAILED: 'CONN001',
  CONNECTION_TIMEOUT: 'CONN002',
  WEBSOCKET_DISCONNECTED: 'CONN003',

  // Rate limit errors
  RATE_LIMIT_EXCEEDED: 'RATE001',

  // General errors
  UNKNOWN_ERROR: 'ERR001',
  INVALID_REQUEST: 'ERR002',
  SERVER_ERROR: 'ERR003',
} as const;

export const ERROR_MESSAGES: Record<string, string> = {
  [ERROR_CODES.AUTH_INVALID_CREDENTIALS]: 'Invalid credentials provided',
  [ERROR_CODES.AUTH_TOKEN_EXPIRED]: 'Access token has expired. Please login again.',
  [ERROR_CODES.AUTH_SESSION_EXPIRED]: 'Session has expired. Please login again.',
  [ERROR_CODES.AUTH_TOTP_REQUIRED]: 'TOTP verification required',
  [ERROR_CODES.AUTH_TOTP_INVALID]: 'Invalid TOTP code',
  [ERROR_CODES.ORDER_INVALID_SYMBOL]: 'Invalid symbol or symbol not found',
  [ERROR_CODES.ORDER_INVALID_QUANTITY]: 'Invalid quantity specified',
  [ERROR_CODES.ORDER_INSUFFICIENT_FUNDS]: 'Insufficient funds for this order',
  [ERROR_CODES.ORDER_MARKET_CLOSED]: 'Market is closed',
  [ERROR_CODES.ORDER_PRICE_OUT_OF_RANGE]: 'Price is outside circuit limits',
  [ERROR_CODES.ORDER_MODIFICATION_NOT_ALLOWED]: 'Order cannot be modified in current state',
  [ERROR_CODES.ORDER_CANCELLATION_NOT_ALLOWED]: 'Order cannot be cancelled in current state',
  [ERROR_CODES.CONNECTION_FAILED]: 'Failed to connect to broker',
  [ERROR_CODES.CONNECTION_TIMEOUT]: 'Connection timed out',
  [ERROR_CODES.WEBSOCKET_DISCONNECTED]: 'WebSocket connection lost',
  [ERROR_CODES.RATE_LIMIT_EXCEEDED]: 'Rate limit exceeded. Please try again later.',
  [ERROR_CODES.UNKNOWN_ERROR]: 'An unknown error occurred',
  [ERROR_CODES.INVALID_REQUEST]: 'Invalid request parameters',
  [ERROR_CODES.SERVER_ERROR]: 'Server error. Please try again later.',
};

// ============================================================================
// WebSocket Constants
// ============================================================================

export const WEBSOCKET_CONFIG = {
  MAX_RECONNECT_ATTEMPTS: 50,
  RECONNECT_DELAY_MS: 2000,
  MAX_RECONNECT_DELAY_MS: 60000,
  PING_INTERVAL_MS: 30000,
  CONNECTION_TIMEOUT_MS: 10000,
  MAX_SUBSCRIPTIONS_PER_CONNECTION: 3000,
  SUBSCRIPTION_BATCH_SIZE: 200,
  SUBSCRIPTION_BATCH_DELAY_MS: 2000,
} as const;

// ============================================================================
// API Rate Limits (Default values, brokers may vary)
// ============================================================================

export const DEFAULT_RATE_LIMITS = {
  ORDERS_PER_SECOND: 10,
  QUOTES_PER_SECOND: 1,
  HISTORICAL_DATA_PER_SECOND: 3,
} as const;

// ============================================================================
// Database Table Names
// ============================================================================

export const DB_TABLES = {
  BROKER_CREDENTIALS: 'indian_broker_credentials',
  MASTER_CONTRACTS: 'indian_master_contracts',
  ORDER_HISTORY: 'indian_order_history',
  TRADE_LOG: 'indian_trade_log',
} as const;
