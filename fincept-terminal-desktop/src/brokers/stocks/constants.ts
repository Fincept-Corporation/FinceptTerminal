/**
 * Stock Broker Constants
 *
 * Global constants for stock trading across all regions
 */

import type { Region, Currency, MarketHours, StockExchange } from './types';

// ============================================================================
// MARKET HOURS BY REGION
// ============================================================================

export const MARKET_HOURS: Record<string, MarketHours> = {
  // India
  NSE: {
    open: '09:15',
    close: '15:30',
    timezone: 'Asia/Kolkata',
    preMarketOpen: '09:00',
    preMarketClose: '09:08',
  },
  BSE: {
    open: '09:15',
    close: '15:30',
    timezone: 'Asia/Kolkata',
    preMarketOpen: '09:00',
    preMarketClose: '09:08',
  },
  MCX: {
    open: '09:00',
    close: '23:30',
    timezone: 'Asia/Kolkata',
  },

  // US
  NYSE: {
    open: '09:30',
    close: '16:00',
    timezone: 'America/New_York',
    preMarketOpen: '04:00',
    preMarketClose: '09:30',
    postMarketOpen: '16:00',
    postMarketClose: '20:00',
  },
  NASDAQ: {
    open: '09:30',
    close: '16:00',
    timezone: 'America/New_York',
    preMarketOpen: '04:00',
    preMarketClose: '09:30',
    postMarketOpen: '16:00',
    postMarketClose: '20:00',
  },

  // Europe
  LSE: {
    open: '08:00',
    close: '16:30',
    timezone: 'Europe/London',
  },
  XETRA: {
    open: '09:00',
    close: '17:30',
    timezone: 'Europe/Berlin',
  },
};

// ============================================================================
// CURRENCY BY REGION
// ============================================================================

export const REGION_CURRENCY: Record<Region, Currency> = {
  india: 'INR',
  us: 'USD',
  europe: 'EUR',
  uk: 'GBP',
  asia: 'JPY',
  australia: 'AUD',
  canada: 'CAD',
};

// ============================================================================
// EXCHANGES BY REGION
// ============================================================================

export const REGION_EXCHANGES: Record<Region, StockExchange[]> = {
  india: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
  us: ['NYSE', 'NASDAQ', 'AMEX', 'ARCA', 'BATS'],
  europe: ['LSE', 'XETRA', 'EURONEXT', 'SIX'],
  uk: ['LSE'],
  asia: ['TSE', 'HKEX', 'SGX', 'KRX'],
  australia: ['ASX'],
  canada: ['TSX', 'TSXV'],
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

// Map standard product types to broker-specific codes
export const PRODUCT_TYPE_MAP = {
  india: {
    CASH: 'CNC',
    INTRADAY: 'MIS',
    MARGIN: 'NRML',
    COVER: 'CO',
    BRACKET: 'BO',
  },
  us: {
    CASH: 'CASH',
    INTRADAY: 'DAY',
    MARGIN: 'MARGIN',
  },
} as const;

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const ORDER_STATUS_DISPLAY: Record<string, { label: string; color: string }> = {
  PENDING: { label: 'Pending', color: 'yellow' },
  OPEN: { label: 'Open', color: 'blue' },
  PARTIALLY_FILLED: { label: 'Partial', color: 'cyan' },
  FILLED: { label: 'Filled', color: 'green' },
  CANCELLED: { label: 'Cancelled', color: 'gray' },
  REJECTED: { label: 'Rejected', color: 'red' },
  EXPIRED: { label: 'Expired', color: 'gray' },
  TRIGGER_PENDING: { label: 'Trigger Pending', color: 'orange' },
};

// ============================================================================
// ERROR CODES
// ============================================================================

export const ERROR_CODES = {
  // Authentication
  AUTH_FAILED: 'AUTH001',
  TOKEN_EXPIRED: 'AUTH002',
  INVALID_CREDENTIALS: 'AUTH003',

  // Orders
  ORDER_FAILED: 'ORD001',
  INSUFFICIENT_MARGIN: 'ORD002',
  INVALID_QUANTITY: 'ORD003',
  MARKET_CLOSED: 'ORD004',
  CIRCUIT_LIMIT: 'ORD005',

  // Network
  NETWORK_ERROR: 'NET001',
  TIMEOUT: 'NET002',
  RATE_LIMIT: 'NET003',

  // General
  UNKNOWN_ERROR: 'ERR999',
} as const;

export const ERROR_MESSAGES: Record<string, string> = {
  [ERROR_CODES.AUTH_FAILED]: 'Authentication failed',
  [ERROR_CODES.TOKEN_EXPIRED]: 'Session expired. Please login again.',
  [ERROR_CODES.INVALID_CREDENTIALS]: 'Invalid credentials',
  [ERROR_CODES.ORDER_FAILED]: 'Order placement failed',
  [ERROR_CODES.INSUFFICIENT_MARGIN]: 'Insufficient margin for this order',
  [ERROR_CODES.INVALID_QUANTITY]: 'Invalid quantity',
  [ERROR_CODES.MARKET_CLOSED]: 'Market is closed',
  [ERROR_CODES.CIRCUIT_LIMIT]: 'Price is at circuit limit',
  [ERROR_CODES.NETWORK_ERROR]: 'Network error. Please check your connection.',
  [ERROR_CODES.TIMEOUT]: 'Request timed out',
  [ERROR_CODES.RATE_LIMIT]: 'Rate limit exceeded. Please try again.',
  [ERROR_CODES.UNKNOWN_ERROR]: 'An unknown error occurred',
};

// ============================================================================
// TIMEFRAME MAPPINGS
// ============================================================================

export const TIMEFRAME_LABELS: Record<string, string> = {
  '1m': '1 Min',
  '3m': '3 Min',
  '5m': '5 Min',
  '10m': '10 Min',
  '15m': '15 Min',
  '30m': '30 Min',
  '1h': '1 Hour',
  '2h': '2 Hour',
  '4h': '4 Hour',
  '1d': 'Daily',
  '1w': 'Weekly',
  '1M': 'Monthly',
};

// ============================================================================
// RATE LIMITING
// ============================================================================

export const DEFAULT_RATE_LIMITS = {
  ordersPerSecond: 10,
  quotesPerSecond: 5,
  minRequestInterval: 100, // ms between requests
};

// ============================================================================
// WEBSOCKET DEFAULTS
// ============================================================================

export const WEBSOCKET_DEFAULTS = {
  autoReconnect: true,
  reconnectInterval: 5000,
  maxReconnectAttempts: 5,
  heartbeatInterval: 30000,
};
