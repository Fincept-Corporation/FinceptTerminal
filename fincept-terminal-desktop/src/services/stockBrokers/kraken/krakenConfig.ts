/**
 * Kraken Configuration and Constants
 */

// API Endpoints
export const KRAKEN_ENDPOINTS = {
  REST: {
    PRODUCTION: 'https://api.kraken.com',
    TESTNET: 'https://api.kraken.com', // Kraken doesn't have a public testnet
    FUTURES: 'https://futures.kraken.com',
    FUTURES_TESTNET: 'https://demo-futures.kraken.com',
  },
  WEBSOCKET: {
    PUBLIC: 'wss://ws.kraken.com/v2',
    PRIVATE: 'wss://ws-auth.kraken.com/v2',
    FUTURES: 'wss://futures.kraken.com/ws/v1',
    FUTURES_TESTNET: 'wss://demo-futures.kraken.com/ws/v1',
    BETA_PUBLIC: 'wss://beta-ws.kraken.com/v2',
    BETA_PRIVATE: 'wss://beta-ws-auth.kraken.com/v2',
  },
};

// API Versions
export const KRAKEN_API_VERSIONS = {
  REST: '0',
  WEBSOCKET: '2',
  FUTURES: 'v1',
};

// Rate Limits (milliseconds)
export const KRAKEN_RATE_LIMITS = {
  REST: {
    PUBLIC: 1000, // 1 request per second
    PRIVATE: 3000, // 1 request per 3 seconds
    TRADING: 1000, // 1 request per second for trading
  },
  WEBSOCKET: {
    CONNECTION_LIMIT: 10,
    SUBSCRIPTION_LIMIT: 100,
    MESSAGE_RATE: 100, // Max messages per second
  },
};

// Order Types
export const KRAKEN_ORDER_TYPES = {
  MARKET: 'market',
  LIMIT: 'limit',
  STOP_LOSS: 'stop-loss',
  TAKE_PROFIT: 'take-profit',
  STOP_LOSS_LIMIT: 'stop-loss-limit',
  TAKE_PROFIT_LIMIT: 'take-profit-limit',
  SETTLE_POSITION: 'settle-position',
} as const;

// Order Sides
export const KRAKEN_ORDER_SIDES = {
  BUY: 'buy',
  SELL: 'sell',
} as const;

// Time in Force
export const KRAKEN_TIME_IN_FORCE = {
  GTC: 'GTC', // Good Till Cancelled
  IOC: 'IOC', // Immediate Or Cancel
  FOK: 'FOK', // Fill Or Kill
} as const;

// Asset Classes
export const KRAKEN_ASSET_CLASSES = {
  CURRENCY: 'currency',
  SPOT: 'spot',
  MARGIN: 'margin',
  FUTURES: 'futures',
  OPTION: 'option',
} as const;

// WebSocket Channels
export const KRAKEN_WEBSOCKET_CHANNELS = {
  // Public Channels
  TICKER: 'ticker',
  TRADES: 'trade',
  BOOK: 'book',
  SPREAD: 'spread',
  OHLC: 'ohlc',
  INSTRUMENT: 'instrument',

  // Private Channels
  OPEN_ORDERS: 'openOrders',
  OWN_TRADES: 'ownTrades',
  BALANCE: 'balance',
  POSITIONS: 'positions',
  MARGIN: 'margin',
  LEDGERS: 'ledgers',
} as const;

// WebSocket Statuses
export const KRAKEN_WEBSOCKET_STATUS = {
  CONNECTED: 'connected',
  CONNECTING: 'connecting',
  DISCONNECTED: 'disconnected',
  ERROR: 'error',
  RECONNECTING: 'reconnecting',
} as const;

// Error Codes
export const KRAKEN_ERROR_CODES = {
  // General Errors
  INVALID_API_KEY: 'EAPI:Invalid key',
  INVALID_SIGNATURE: 'EAPI:Invalid signature',
  INVALID_NONCE: 'EAPI:Invalid nonce',

  // Trading Errors
  INSUFFICIENT_FUNDS: 'EGeneral:Insufficient funds',
  INVALID_ORDER: 'EGeneral:Invalid arguments',
  ORDER_LIMIT_EXCEEDED: 'EGeneral:Too many requests',
  MARKET_CLOSED: 'EGeneral:Market in cancel only mode',

  // Account Errors
  ACCOUNT_SUSPENDED: 'EGeneral:Account suspended',
  PERMISSION_DENIED: 'EGeneral:Permission denied',

  // Network Errors
  RATE_LIMIT: 'EOrder:Rate limit exceeded',
  SERVICE_UNAVAILABLE: 'EGeneral:Service unavailable',
} as const;

// Default Configuration
export const KRAKEN_DEFAULT_CONFIG = {
  // Connection settings
  timeout: 30000, // 30 seconds
  retryAttempts: 3,
  retryDelay: 1000, // 1 second

  // WebSocket settings
  websocketTimeout: 60000, // 1 minute
  reconnectInterval: 5000, // 5 seconds
  maxReconnectAttempts: 10,

  // Rate limiting
  enableRateLimiting: true,
  rateLimitBuffer: 100, // 100ms buffer between requests

  // Data settings
  defaultOrderBookDepth: 100,
  defaultOHLCVLimit: 720, // 720 candles (default)

  // Trading settings
  defaultTimeInForce: 'GTC',
  validateOrders: true,

  // WebSocket settings
  enableWebSocketCompression: true,
  websocketPingInterval: 30000, // 30 seconds
  maxSubscriptionsPerConnection: 50,
};

// Environment Detection
export const isDevelopment = process.env.NODE_ENV === 'development';
export const isProduction = process.env.NODE_ENV === 'production';

// Feature Flags
export const KRAKEN_FEATURES = {
  // API Features
  SPOT_TRADING: true,
  MARGIN_TRADING: true,
  FUTURES_TRADING: true,
  STAKING: true,
  LENDING: true,

  // WebSocket Features
  REAL_TIME_DATA: true,
  WEBSOCKET_TRADING: true,

  // Advanced Features
  BATCH_ORDERS: true,
  CONDITIONAL_ORDERS: true,
  ICEBERG_ORDERS: true,
  TRAILING_STOPS: false, // Not supported by Kraken

  // UI Features
  ADVANCED_CHARTING: true,
  PORTFOLIO_TRACKING: true,
  TAX_REPORTING: true,
} as const;

// Validation Rules
export const KRAKEN_VALIDATION = {
  // API Key format
  API_KEY_REGEX: /^[A-Za-z0-9]{20,}$/,
  SECRET_MIN_LENGTH: 32,

  // Symbol format
  SYMBOL_REGEX: /^[A-Z]{3,4}\/[A-Z]{3,4}$/,

  // Order validation
  MIN_ORDER_AMOUNT: 0.0001,
  MAX_ORDER_PRECISION: 8,

  // WebSocket validation
  MAX_SYMBOLS_PER_SUBSCRIPTION: 100,
  MAX_MESSAGE_SIZE: 1024 * 1024, // 1MB
} as const;

// Currency Pairs (Major Pairs)
export const KRAKEN_MAJOR_PAIRS = [
  'BTC/USD',
  'ETH/USD',
  'BTC/EUR',
  'ETH/EUR',
  'BTC/USDT',
  'ETH/USDT',
  'LTC/USD',
  'XRP/USD',
  'ADA/USD',
  'DOT/USD',
];

// Trading Hours
export const KRAKEN_TRADING_HOURS = {
  SPOT: '24/7',
  MARGIN: '24/7',
  FUTURES: '24/7',
} as const;

// Fee Tiers
export const KRAKEN_FEE_TIERS = {
  MAKER: [0.26, 0.24, 0.22, 0.20, 0.18, 0.16, 0.14, 0.12, 0.10, 0.08],
  TAKER: [0.26, 0.24, 0.22, 0.20, 0.18, 0.16, 0.14, 0.12, 0.10, 0.08],
  VOLUME_THRESHOLDS: [
    0, 50000, 100000, 250000, 500000, 1000000, 2500000, 5000000, 10000000, Infinity
  ],
} as const;