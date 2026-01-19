/**
 * Shoonya (Finvasia) Constants
 *
 * API endpoints, mappings, and configuration for Shoonya/Finvasia API
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

export const SHOONYA_API_BASE_URL = 'https://api.shoonya.com';
export const SHOONYA_API_VERSION = 'NorenWClientTP';

export const SHOONYA_ENDPOINTS = {
  // Auth
  QUICK_AUTH: '/NorenWClientTP/QuickAuth',

  // Orders
  PLACE_ORDER: '/NorenWClientTP/PlaceOrder',
  MODIFY_ORDER: '/NorenWClientTP/ModifyOrder',
  CANCEL_ORDER: '/NorenWClientTP/CancelOrder',
  ORDER_BOOK: '/NorenWClientTP/OrderBook',
  TRADE_BOOK: '/NorenWClientTP/TradeBook',
  SINGLE_ORDER_HISTORY: '/NorenWClientTP/SingleOrdHist',

  // Portfolio
  POSITIONS: '/NorenWClientTP/PositionBook',
  HOLDINGS: '/NorenWClientTP/Holdings',
  LIMITS: '/NorenWClientTP/Limits',

  // Market Data
  QUOTES: '/NorenWClientTP/GetQuotes',
  TP_SERIES: '/NorenWClientTP/TPSeries',
  EOD_CHART_DATA: '/NorenWClientTP/EODChartData',
  GET_INDEX_LIST: '/NorenWClientTP/GetIndexList',
  GET_OPTION_CHAIN: '/NorenWClientTP/GetOptionChain',
} as const;

// ============================================================================
// WEBSOCKET CONFIGURATION
// ============================================================================

export const SHOONYA_WS_URL = 'wss://api.shoonya.com/NorenWSTP';

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const SHOONYA_EXCHANGE_MAP: Record<string, string> = {
  NSE: 'NSE',
  BSE: 'BSE',
  NFO: 'NFO',
  BFO: 'BFO',
  CDS: 'CDS',
  MCX: 'MCX',
  NSE_INDEX: 'NSE',
  BSE_INDEX: 'BSE',
};

// Exchange codes (used in API)
export const SHOONYA_EXCHANGE_CODES: Record<string, string> = {
  NSE: 'NSE',
  NFO: 'NFO',
  CDS: 'CDS',
  MCX: 'MCX',
  BSE: 'BSE',
  BFO: 'BFO',
  NCX: 'NCX',
  BCD: 'BCD',
};

// ============================================================================
// PRODUCT TYPE MAPPINGS
// ============================================================================

// Shoonya: C = CNC, M = NRML, I = MIS
export const SHOONYA_PRODUCT_MAP: Record<string, string> = {
  CASH: 'C',
  CNC: 'C',
  NRML: 'M',
  MARGIN: 'M',
  INTRADAY: 'I',
  MIS: 'I',
  BRACKET: 'B',
  COVER: 'H',
};

export const SHOONYA_PRODUCT_REVERSE_MAP: Record<string, string> = {
  C: 'CASH',
  M: 'NRML',
  I: 'INTRADAY',
  B: 'BRACKET',
  H: 'COVER',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

// Shoonya: LMT, MKT, SL-LMT, SL-MKT
export const SHOONYA_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'MKT',
  LIMIT: 'LMT',
  STOP_LIMIT: 'SL-LMT',
  STOP_MARKET: 'SL-MKT',
};

export const SHOONYA_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  LMT: 'LIMIT',
  MKT: 'MARKET',
  'SL-LMT': 'STOP_LIMIT',
  'SL-MKT': 'STOP_MARKET',
};

// ============================================================================
// ORDER SIDE MAPPINGS
// ============================================================================

// Shoonya: B = Buy, S = Sell
export const SHOONYA_SIDE_MAP: Record<string, string> = {
  BUY: 'B',
  SELL: 'S',
};

export const SHOONYA_SIDE_REVERSE_MAP: Record<string, string> = {
  B: 'BUY',
  S: 'SELL',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const SHOONYA_STATUS_MAP: Record<string, string> = {
  PENDING: 'PENDING',
  OPEN: 'OPEN',
  COMPLETE: 'FILLED',
  REJECTED: 'REJECTED',
  CANCELLED: 'CANCELLED',
  TRIGGER_PENDING: 'TRIGGER_PENDING',
};

// ============================================================================
// VALIDITY MAPPINGS
// ============================================================================

export const SHOONYA_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DAY',
  IOC: 'IOC',
  EOS: 'EOS',  // End of Session
};

// ============================================================================
// RESOLUTION MAPPINGS (for historical data)
// ============================================================================

export const SHOONYA_RESOLUTION_MAP: Record<string, string> = {
  '1m': '1',
  '3m': '3',
  '5m': '5',
  '10m': '10',
  '15m': '15',
  '30m': '30',
  '1h': '60',
  '2h': '120',
  '4h': '240',
  '1d': 'D',
  '1w': 'D',  // Weekly not directly supported
  '1M': 'D',  // Monthly not directly supported
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const SHOONYA_METADATA: StockBrokerMetadata = {
  id: 'shoonya',
  name: 'shoonya',
  displayName: 'Shoonya (Finvasia)',
  logo: '/brokers/shoonya.png',
  website: 'https://shoonya.com',

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
    amo: true,            // After Market Orders
    gtt: false,           // Good Till Triggered
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

  authType: 'totp',  // TOTP-based authentication with SHA256

  rateLimit: {
    ordersPerSecond: 10,
    quotesPerSecond: 10,
  },

  fees: {
    equity: { brokerage: 0, stt: 0.001 },  // Zero brokerage for equity
    intraday: { brokerage: 0 },             // Zero brokerage for intraday
    fno: { brokerage: 0 },                  // Zero brokerage for F&O
  },

  defaultSymbols: [
    'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK',
    'SBIN', 'BHARTIARTL', 'HINDUNILVR', 'ITC', 'KOTAKBANK',
    'LT', 'AXISBANK', 'MARUTI', 'TATAMOTORS', 'WIPRO',
  ],
};

// ============================================================================
// MASTER CONTRACT URLS
// ============================================================================

export const SHOONYA_MASTER_CONTRACT_URLS: Record<string, string> = {
  NSE: 'https://api.shoonya.com/NSE_symbols.txt.zip',
  NFO: 'https://api.shoonya.com/NFO_symbols.txt.zip',
  CDS: 'https://api.shoonya.com/CDS_symbols.txt.zip',
  MCX: 'https://api.shoonya.com/MCX_symbols.txt.zip',
  BSE: 'https://api.shoonya.com/BSE_symbols.txt.zip',
  BFO: 'https://api.shoonya.com/BFO_symbols.txt.zip',
};
