/**
 * Saxo Bank Constants
 *
 * API endpoints, mappings, and configuration for Saxo Bank OpenAPI
 * Documentation: https://www.developer.saxo/openapi/learn
 */

import type { StockBrokerMetadata } from '../../types';

// ============================================================================
// API CONFIGURATION
// ============================================================================

// Simulation Environment (Paper Trading)
export const SAXO_SIM_API_BASE = 'https://gateway.saxobank.com/sim/openapi';
export const SAXO_SIM_AUTH_ENDPOINT = 'https://sim.logonvalidation.net/authorize';
export const SAXO_SIM_TOKEN_ENDPOINT = 'https://sim.logonvalidation.net/token';
export const SAXO_SIM_STREAM_URL = 'wss://streaming.saxobank.com/sim/openapi/streamingws/connect';

// Live Environment (Production)
export const SAXO_LIVE_API_BASE = 'https://gateway.saxobank.com/openapi';
export const SAXO_LIVE_AUTH_ENDPOINT = 'https://live.logonvalidation.net/authorize';
export const SAXO_LIVE_TOKEN_ENDPOINT = 'https://live.logonvalidation.net/token';
export const SAXO_LIVE_STREAM_URL = 'wss://streaming.saxobank.com/openapi/streamingws/connect';

// API Version
export const SAXO_API_VERSION = 'v1';

// ============================================================================
// API ENDPOINTS
// ============================================================================

export const SAXO_ENDPOINTS = {
  // Root Services
  ROOT_SESSIONS: '/root/v1/sessions',
  ROOT_USER: '/root/v1/user',
  ROOT_FEATURES: '/root/v1/features',
  ROOT_DIAGNOSTICS: '/root/v1/diagnostics',

  // Port (Account & Client)
  PORT_ACCOUNTS: '/port/v1/accounts',
  PORT_ACCOUNT: '/port/v1/accounts/:accountKey',
  PORT_BALANCES: '/port/v1/balances',
  PORT_BALANCE: '/port/v1/balances/:accountKey',
  PORT_CLIENTS: '/port/v1/clients',
  PORT_CLIENT: '/port/v1/clients/me',
  PORT_USERS: '/port/v1/users',
  PORT_USER: '/port/v1/users/me',
  PORT_CLOSED_POSITIONS: '/port/v1/closedpositions',
  PORT_EXPOSURE: '/port/v1/exposure/instruments',
  PORT_NET_POSITIONS: '/port/v1/netpositions',
  PORT_NET_POSITION: '/port/v1/netpositions/:netPositionId',
  PORT_ORDERS: '/port/v1/orders',
  PORT_ORDER: '/port/v1/orders/:orderId',
  PORT_POSITIONS: '/port/v1/positions',
  PORT_POSITION: '/port/v1/positions/:positionId',

  // Trading
  TRADE_ORDERS: '/trade/v2/orders',
  TRADE_ORDER: '/trade/v2/orders/:orderId',
  TRADE_INFO_PRICES: '/trade/v1/infoprices',
  TRADE_INFO_PRICE: '/trade/v1/infoprices/:uic/:assetType',
  TRADE_INFO_PRICES_LIST: '/trade/v1/infoprices/list',
  TRADE_MESSAGES: '/trade/v1/messages',
  TRADE_OPTION_CHAIN: '/trade/v1/optionschain/subscriptions',
  TRADE_POSITIONS: '/trade/v1/positions',
  TRADE_POSITION: '/trade/v1/positions/:positionId',

  // Reference Data
  REF_COUNTRIES: '/ref/v1/countries',
  REF_CULTURES: '/ref/v1/cultures',
  REF_CURRENCIES: '/ref/v1/currencies',
  REF_EXCHANGES: '/ref/v1/exchanges',
  REF_EXCHANGE: '/ref/v1/exchanges/:exchangeId',
  REF_INSTRUMENTS: '/ref/v1/instruments',
  REF_INSTRUMENT: '/ref/v1/instruments/:uic/:assetType',
  REF_INSTRUMENT_DETAILS: '/ref/v1/instruments/details/:uic/:assetType',
  REF_STANDARD_DATES: '/ref/v1/standarddates',
  REF_TIME_ZONES: '/ref/v1/timezones',

  // Chart Data
  CHART_CHARTS: '/chart/v1/charts',
  CHART_CHART: '/chart/v1/charts/:contextId/:referenceId',

  // Account History
  HIST_PERFORMANCE: '/hist/v1/performance/:accountKey',
  HIST_PERFORMANCE_TIME_SERIES: '/hist/v1/performance/timeseries/:accountKey',
  HIST_AUDIT_ORDERS: '/hist/v2/audit/orderactivities',
  HIST_CLOSED_POSITIONS: '/hist/v1/closedpositions',

  // Asset Transfers
  TRANSFER_BENEFICIARIES: '/at/v1/beneficiaries',
  TRANSFER_CASH: '/at/v1/cashwithdrawals',
  TRANSFER_DEPOSITS: '/at/v1/deposits',
  TRANSFER_INTERACCOUNT: '/at/v1/interaccounttransfers',

  // Streaming Subscriptions
  STREAM_PRICES: '/trade/v1/prices/subscriptions',
  STREAM_ORDERS: '/port/v1/orders/subscriptions',
  STREAM_POSITIONS: '/port/v1/positions/subscriptions',
  STREAM_BALANCES: '/port/v1/balances/subscriptions',
  STREAM_NET_POSITIONS: '/port/v1/netpositions/subscriptions',
} as const;

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const SAXO_EXCHANGE_MAP: Record<string, string> = {
  // Major European Exchanges
  'XPAR': 'Euronext Paris',
  'XETR': 'XETRA (Frankfurt)',
  'XFRA': 'Frankfurt Stock Exchange',
  'XMAD': 'Bolsa de Madrid',
  'XAMS': 'Euronext Amsterdam',
  'XBRU': 'Euronext Brussels',
  'XLIS': 'Euronext Lisbon',
  'XMIL': 'Borsa Italiana',
  'XLON': 'London Stock Exchange',
  'XSWX': 'SIX Swiss Exchange',
  'XCSE': 'Nasdaq Copenhagen',
  'XSTO': 'Nasdaq Stockholm',
  'XHEL': 'Nasdaq Helsinki',
  'XOSL': 'Oslo Børs',
  'XWAR': 'Warsaw Stock Exchange',
  'XPRA': 'Prague Stock Exchange',
  'XBUD': 'Budapest Stock Exchange',
  'XIST': 'Borsa Istanbul',
  'XATH': 'Athens Stock Exchange',
  'XDUB': 'Euronext Dublin',
  // US Exchanges
  'XNYS': 'NYSE',
  'XNAS': 'NASDAQ',
  'XASE': 'NYSE American',
  'ARCX': 'NYSE Arca',
  // Asia Pacific
  'XTKS': 'Tokyo Stock Exchange',
  'XHKG': 'Hong Kong Stock Exchange',
  'XASX': 'Australian Securities Exchange',
  'XSES': 'Singapore Exchange',
};

export const SAXO_EXCHANGE_REVERSE_MAP: Record<string, string> = {
  'Euronext Paris': 'XPAR',
  'XETRA': 'XETR',
  'Frankfurt': 'XFRA',
  'Madrid': 'XMAD',
  'Euronext Amsterdam': 'XAMS',
  'Euronext Brussels': 'XBRU',
  'Euronext Lisbon': 'XLIS',
  'Milan': 'XMIL',
  'London': 'XLON',
  'Swiss': 'XSWX',
  'Copenhagen': 'XCSE',
  'Stockholm': 'XSTO',
  'Helsinki': 'XHEL',
  'Oslo': 'XOSL',
  'Warsaw': 'XWAR',
  'Prague': 'XPRA',
  'Budapest': 'XBUD',
  'Istanbul': 'XIST',
  'Athens': 'XATH',
  'Dublin': 'XDUB',
  'NYSE': 'XNYS',
  'NASDAQ': 'XNAS',
  'AMEX': 'XASE',
  'Tokyo': 'XTKS',
  'Hong Kong': 'XHKG',
  'Sydney': 'XASX',
  'Singapore': 'XSES',
};

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const SAXO_ORDER_TYPE_MAP: Record<string, string> = {
  MARKET: 'Market',
  LIMIT: 'Limit',
  STOP: 'Stop',
  STOP_LIMIT: 'StopLimit',
  TRAILING_STOP: 'TrailingStop',
  TRAILING_STOP_LIMIT: 'TrailingStopLimit',
};

export const SAXO_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  Market: 'MARKET',
  Limit: 'LIMIT',
  Stop: 'STOP',
  StopLimit: 'STOP_LIMIT',
  TrailingStop: 'TRAILING_STOP',
  TrailingStopLimit: 'TRAILING_STOP_LIMIT',
  StopIfTraded: 'STOP',
  StopIfBid: 'STOP',
  StopIfAsked: 'STOP',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const SAXO_STATUS_MAP: Record<string, string> = {
  Working: 'OPEN',
  Filled: 'FILLED',
  PartiallyFilled: 'PARTIALLY_FILLED',
  Expired: 'EXPIRED',
  Cancelled: 'CANCELLED',
  Rejected: 'REJECTED',
  Placed: 'PENDING',
  PendingFill: 'PENDING',
  PendingModify: 'PENDING',
  PendingCancel: 'PENDING',
  PendingDelete: 'PENDING',
};

// ============================================================================
// SIDE MAPPINGS
// ============================================================================

export const SAXO_SIDE_MAP: Record<string, string> = {
  BUY: 'Buy',
  SELL: 'Sell',
};

export const SAXO_SIDE_REVERSE_MAP: Record<string, string> = {
  Buy: 'BUY',
  Sell: 'SELL',
};

// ============================================================================
// VALIDITY (DURATION) MAPPINGS
// ============================================================================

export const SAXO_VALIDITY_MAP: Record<string, string> = {
  DAY: 'DayOrder',
  IOC: 'ImmediateOrCancel',
  GTC: 'GoodTillCancel',
  GTD: 'GoodTillDate',
  FOK: 'FillOrKill',
  OPG: 'AtTheOpening',
  CLS: 'AtTheClose',
};

export const SAXO_VALIDITY_REVERSE_MAP: Record<string, string> = {
  DayOrder: 'DAY',
  ImmediateOrCancel: 'IOC',
  GoodTillCancel: 'GTC',
  GoodTillDate: 'GTD',
  GoodForPeriod: 'GTD',
  FillOrKill: 'FOK',
  AtTheOpening: 'DAY',
  AtTheClose: 'DAY',
};

// ============================================================================
// ASSET TYPE MAPPINGS
// ============================================================================

export const SAXO_ASSET_TYPE_MAP: Record<string, string> = {
  STOCK: 'Stock',
  ETF: 'Etf',
  FUND: 'Fund',
  BOND: 'Bond',
  OPTION: 'StockOption',
  FUTURE: 'ContractFutures',
  CFD: 'CfdOnStock',
  FOREX: 'FxSpot',
};

export const SAXO_ASSET_TYPE_REVERSE_MAP: Record<string, string> = {
  Stock: 'STOCK',
  Etf: 'ETF',
  Etn: 'ETF',
  Etc: 'ETF',
  Fund: 'FUND',
  MutualFund: 'FUND',
  Bond: 'BOND',
  StockOption: 'OPTION',
  StockIndexOption: 'OPTION',
  FuturesOption: 'OPTION',
  ContractFutures: 'FUTURE',
  CfdOnStock: 'CFD',
  CfdOnIndex: 'CFD',
  CfdOnFutures: 'CFD',
  CfdOnEtc: 'CFD',
  CfdOnEtn: 'CFD',
  CfdOnEtf: 'CFD',
  CfdOnFunds: 'CFD',
  CfdOnRights: 'CFD',
  FxSpot: 'FOREX',
  FxForwards: 'FOREX',
  FxSwap: 'FOREX',
  FxVanillaOption: 'FOREX',
};

// ============================================================================
// TIMEFRAME (HORIZON) MAPPINGS
// ============================================================================

export const SAXO_TIMEFRAME_MAP: Record<string, number> = {
  '1m': 1,
  '5m': 5,
  '10m': 10,
  '15m': 15,
  '30m': 30,
  '1h': 60,
  '2h': 120,
  '4h': 240,
  '6h': 360,
  '8h': 480,
  '1d': 1440,
  '1w': 10080,
  '1M': 43200,
};

export const SAXO_TIMEFRAME_REVERSE_MAP: Record<number, string> = {
  1: '1m',
  5: '5m',
  10: '10m',
  15: '15m',
  30: '30m',
  60: '1h',
  120: '2h',
  240: '4h',
  360: '6h',
  480: '8h',
  1440: '1d',
  10080: '1w',
  43200: '1M',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const SAXO_METADATA: StockBrokerMetadata = {
  id: 'saxobank',
  name: 'saxobank',
  displayName: 'Saxo Bank',
  logo: 'https://fincept.in/brokers/saxobank.png',
  website: 'https://www.home.saxo',

  region: 'europe',
  country: 'DK', // Headquartered in Denmark
  currency: 'EUR',
  exchanges: [
    // Major European Exchanges
    'XPAR', 'XETR', 'XFRA', 'XMAD', 'XAMS', 'XBRU', 'XLIS', 'XMIL',
    'XLON', 'XSWX', 'XCSE', 'XSTO', 'XHEL', 'XOSL', 'XWAR', 'XPRA',
    'XBUD', 'XIST', 'XATH', 'XDUB',
    // US Exchanges
    'XNYS', 'XNAS', 'XASE', 'ARCX',
    // Asia Pacific
    'XTKS', 'XHKG', 'XASX', 'XSES',
  ],
  marketHours: {
    // Using CET (Central European Time) as reference
    open: '09:00',
    close: '17:30',
    timezone: 'Europe/Copenhagen',
    preMarketOpen: '08:00',
    preMarketClose: '09:00',
    postMarketOpen: '17:30',
    postMarketClose: '20:00',
  },

  features: {
    webSocket: true,
    amo: true, // After Market Orders supported
    gtt: true, // Good Till Date orders
    bracketOrder: true, // Related orders supported
    coverOrder: false,
    marginCalculator: true,
    optionsChain: true,
    paperTrading: true, // Simulation environment available
  },

  tradingFeatures: {
    marketOrders: true,
    limitOrders: true,
    stopOrders: true,
    stopLimitOrders: true,
    trailingStopOrders: true,
  },

  productTypes: ['CASH', 'MARGIN', 'INTRADAY'],

  authType: 'oauth', // OAuth 2.0 Authorization Code Grant

  rateLimit: {
    ordersPerSecond: 1, // Conservative: 1 order per second
    quotesPerSecond: 2, // 120 requests per minute per service group = 2/sec
  },

  fees: {
    equity: {
      brokerage: 0.1, // Varies by market, this is approximate %
      stt: 0,
    },
    intraday: {
      brokerage: 0.05,
    },
    fno: {
      brokerage: 0.05,
    },
  },

  defaultSymbols: [
    // European Blue Chips
    'ASML:XAMS',    // ASML Holding (Netherlands)
    'SAP:XETR',     // SAP (Germany)
    'LVMH:XPAR',    // LVMH (France)
    'NESN:XSWX',    // Nestlé (Switzerland)
    'NOVO:XCSE',    // Novo Nordisk (Denmark)
    'SHEL:XLON',    // Shell (UK)
    'SAN:XMAD',     // Banco Santander (Spain)
    'ENEL:XMIL',    // Enel (Italy)
    'OR:XPAR',      // L'Oréal (France)
    'SIE:XETR',     // Siemens (Germany)
    'AZN:XLON',     // AstraZeneca (UK)
    'MC:XPAR',      // LVMH (France)
    'ROG:XSWX',     // Roche (Switzerland)
    'NOVN:XSWX',    // Novartis (Switzerland)
    'ULVR:XLON',    // Unilever (UK)
  ],
};

// ============================================================================
// ERROR CODES
// ============================================================================

export const SAXO_ERROR_CODES = {
  // Authentication Errors
  INVALID_TOKEN: 'InvalidToken',
  TOKEN_EXPIRED: 'TokenExpired',
  UNAUTHORIZED: 'Unauthorized',
  FORBIDDEN: 'Forbidden',

  // Order Errors
  ORDER_NOT_FOUND: 'OrderNotFound',
  INSUFFICIENT_FUNDS: 'InsufficientFunds',
  INVALID_ORDER: 'InvalidOrder',
  ORDER_REJECTED: 'OrderRejected',
  MARKET_CLOSED: 'MarketClosed',
  INSTRUMENT_NOT_TRADABLE: 'InstrumentNotTradable',

  // Rate Limit Errors
  RATE_LIMIT_EXCEEDED: 'RateLimitExceeded',
  TOO_MANY_REQUESTS: 'TooManyRequests',

  // General Errors
  BAD_REQUEST: 'BadRequest',
  NOT_FOUND: 'NotFound',
  INTERNAL_ERROR: 'InternalError',
  SERVICE_UNAVAILABLE: 'ServiceUnavailable',
} as const;

// ============================================================================
// STREAMING FIELD GROUPS
// ============================================================================

export const SAXO_PRICE_FIELD_GROUPS = [
  'Commissions',
  'DisplayAndFormat',
  'Greeks',
  'HistoricalChanges',
  'InstrumentPriceDetails',
  'MarginImpact',
  'MarketDepth',
  'PriceInfo',
  'PriceInfoDetails',
  'Quote',
] as const;

export const SAXO_ORDER_FIELD_GROUPS = [
  'DisplayAndFormat',
  'ExchangeInfo',
  'Greeks',
] as const;

export const SAXO_POSITION_FIELD_GROUPS = [
  'DisplayAndFormat',
  'ExchangeInfo',
  'Greeks',
  'PositionBase',
  'PositionIdOnly',
  'PositionView',
] as const;

// ============================================================================
// COUNTRY TO EXCHANGE MAPPING (European coverage)
// ============================================================================

export const SAXO_COUNTRY_EXCHANGES: Record<string, string[]> = {
  FR: ['XPAR'],           // France
  DE: ['XETR', 'XFRA'],   // Germany
  ES: ['XMAD'],           // Spain
  NL: ['XAMS'],           // Netherlands
  BE: ['XBRU'],           // Belgium
  PT: ['XLIS'],           // Portugal
  IT: ['XMIL'],           // Italy
  GB: ['XLON'],           // United Kingdom
  CH: ['XSWX'],           // Switzerland
  DK: ['XCSE'],           // Denmark
  SE: ['XSTO'],           // Sweden
  FI: ['XHEL'],           // Finland
  NO: ['XOSL'],           // Norway
  PL: ['XWAR'],           // Poland
  CZ: ['XPRA'],           // Czech Republic
  HU: ['XBUD'],           // Hungary
  TR: ['XIST'],           // Turkey
  GR: ['XATH'],           // Greece
  IE: ['XDUB'],           // Ireland
  US: ['XNYS', 'XNAS'],   // United States
  JP: ['XTKS'],           // Japan
  HK: ['XHKG'],           // Hong Kong
  AU: ['XASX'],           // Australia
  SG: ['XSES'],           // Singapore
};
