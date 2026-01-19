/**
 * Interactive Brokers (IBKR) Constants
 *
 * API configuration, URL endpoints, and broker metadata for IBKR integration
 */

import type { StockBrokerMetadata, StockExchange, Currency, ProductType } from '../../types';

// ============================================================================
// API ENDPOINTS
// ============================================================================

// Client Portal Web API (OAuth users)
export const IBKR_API_BASE = 'https://api.ibkr.com/v1/api';
export const IBKR_WEBSOCKET_URL = 'wss://api.ibkr.com/v1/api/ws';

// Local Gateway (individual users running CP Gateway)
export const IBKR_GATEWAY_BASE = 'https://localhost:5000/v1/api';
export const IBKR_GATEWAY_WEBSOCKET_URL = 'wss://localhost:5000/v1/api/ws';

// TWS/Gateway Socket Ports
export const IBKR_TWS_LIVE_PORT = 7496;
export const IBKR_TWS_PAPER_PORT = 7497;
export const IBKR_GATEWAY_LIVE_PORT = 4001;
export const IBKR_GATEWAY_PAPER_PORT = 4002;

// ============================================================================
// ORDER TYPE MAPPINGS
// ============================================================================

export const IBKR_ORDER_TYPE_MAP: Record<string, string> = {
  // Standard -> IBKR
  'market': 'MKT',
  'limit': 'LMT',
  'stop': 'STP',
  'stop_limit': 'STOP_LIMIT',
  'trailing_stop': 'TRAIL',
  'trailing_stop_limit': 'TRAILLMT',
  'market_on_close': 'MOC',
  'limit_on_close': 'LOC',
  'market_if_touched': 'MIT',
  'limit_if_touched': 'LIT',
  'midprice': 'MIDPRICE',
};

export const IBKR_ORDER_TYPE_REVERSE_MAP: Record<string, string> = {
  // IBKR -> Standard
  'MKT': 'market',
  'LMT': 'limit',
  'STP': 'stop',
  'STOP_LIMIT': 'stop_limit',
  'TRAIL': 'trailing_stop',
  'TRAILLMT': 'trailing_stop_limit',
  'MOC': 'market_on_close',
  'LOC': 'limit_on_close',
  'MIT': 'market_if_touched',
  'LIT': 'limit_if_touched',
  'MIDPRICE': 'midprice',
};

// ============================================================================
// ORDER STATUS MAPPINGS
// ============================================================================

export const IBKR_ORDER_STATUS_MAP: Record<string, string> = {
  // IBKR -> Standard
  'PendingSubmit': 'pending',
  'PendingCancel': 'pending_cancel',
  'PreSubmitted': 'pending',
  'Submitted': 'open',
  'ApiCancelled': 'cancelled',
  'Cancelled': 'cancelled',
  'Filled': 'filled',
  'Inactive': 'rejected',
  'WarnState': 'pending',
};

export const IBKR_ORDER_STATUS_REVERSE_MAP: Record<string, string> = {
  // Standard -> IBKR
  'pending': 'PendingSubmit',
  'open': 'Submitted',
  'cancelled': 'Cancelled',
  'filled': 'Filled',
  'rejected': 'Inactive',
  'pending_cancel': 'PendingCancel',
};

// ============================================================================
// SIDE MAPPINGS
// ============================================================================

export const IBKR_SIDE_MAP: Record<string, string> = {
  // Standard -> IBKR
  'buy': 'BUY',
  'sell': 'SELL',
};

export const IBKR_SIDE_REVERSE_MAP: Record<string, string> = {
  // IBKR -> Standard
  'BUY': 'buy',
  'SELL': 'sell',
};

// ============================================================================
// TIME IN FORCE MAPPINGS
// ============================================================================

export const IBKR_TIME_IN_FORCE_MAP: Record<string, string> = {
  // Standard -> IBKR
  'day': 'DAY',
  'gtc': 'GTC',
  'opg': 'OPG',
  'ioc': 'IOC',
  'gtd': 'GTD',
  'fok': 'IOC', // IBKR uses IOC for FOK-like behavior
};

export const IBKR_TIME_IN_FORCE_REVERSE_MAP: Record<string, string> = {
  // IBKR -> Standard
  'DAY': 'day',
  'GTC': 'gtc',
  'OPG': 'opg',
  'IOC': 'ioc',
  'GTD': 'gtd',
  'PAX': 'gtc', // Crypto
  'OVT': 'gtc', // Overnight
  'OND': 'day', // Overnight + DAY
};

// ============================================================================
// ASSET CLASS MAPPINGS
// ============================================================================

export const IBKR_ASSET_CLASS_MAP: Record<string, string> = {
  'STK': 'stock',
  'OPT': 'option',
  'FUT': 'future',
  'CASH': 'forex',
  'CFD': 'cfd',
  'BOND': 'bond',
  'FUND': 'fund',
  'WAR': 'warrant',
  'CRYPTO': 'crypto',
};

// ============================================================================
// MARKET DATA FIELD TAGS
// ============================================================================

export const IBKR_MARKET_DATA_FIELDS = {
  // Price fields
  LAST_PRICE: '31',
  BID_PRICE: '84',
  ASK_PRICE: '86',
  BID_SIZE: '88',
  ASK_SIZE: '85',
  VOLUME: '87',
  HIGH: '70',
  LOW: '71',
  OPEN: '7294',
  CLOSE: '7289',
  CHANGE: '82',
  CHANGE_PCT: '83',

  // Identifiers
  SYMBOL: '55',
  CONID: '6008',
  EXCHANGE: '6004',
  SEC_TYPE: '6070',
  COMPANY_NAME: '7051',

  // Market info
  MARK_PRICE: '7088',
  PREV_CLOSE: '7762',
  VOLUME_AVG: '7311',
  MARKET_CAP: '7698',
  PE_RATIO: '7699',
  EPS: '7700',
  BETA: '7720',

  // 52-week range
  HIGH_52_WEEK: '7309',
  LOW_52_WEEK: '7310',
  HIGH_26_WEEK: '7741',
  LOW_26_WEEK: '7742',
  HIGH_13_WEEK: '7743',
  LOW_13_WEEK: '7744',

  // Moving averages
  EMA_200: '7674',
  EMA_100: '7675',
  EMA_50: '7676',
  EMA_20: '7677',

  // Options specific
  IMPLIED_VOL: '7607',
  HIST_VOL: '7636',
  PUT_CALL_INTEREST: '7633',
  PUT_CALL_VOLUME: '7635',

  // Misc
  SHORTABLE: '7295',
  HALTED: '7296',
  DIVIDENDS: '7068',
  DIVIDENDS_TTM: '7184',
};

// Default fields for market data snapshot
export const IBKR_DEFAULT_SNAPSHOT_FIELDS = [
  IBKR_MARKET_DATA_FIELDS.LAST_PRICE,
  IBKR_MARKET_DATA_FIELDS.BID_PRICE,
  IBKR_MARKET_DATA_FIELDS.ASK_PRICE,
  IBKR_MARKET_DATA_FIELDS.BID_SIZE,
  IBKR_MARKET_DATA_FIELDS.ASK_SIZE,
  IBKR_MARKET_DATA_FIELDS.VOLUME,
  IBKR_MARKET_DATA_FIELDS.HIGH,
  IBKR_MARKET_DATA_FIELDS.LOW,
  IBKR_MARKET_DATA_FIELDS.CHANGE,
  IBKR_MARKET_DATA_FIELDS.CHANGE_PCT,
  IBKR_MARKET_DATA_FIELDS.SYMBOL,
  IBKR_MARKET_DATA_FIELDS.COMPANY_NAME,
];

// ============================================================================
// EXCHANGE MAPPINGS
// ============================================================================

export const IBKR_EXCHANGE_MAP: Record<string, string> = {
  'NYSE': 'NYSE',
  'NASDAQ': 'NASDAQ',
  'AMEX': 'AMEX',
  'ARCA': 'ARCA',
  'BATS': 'BATS',
  'IEX': 'IEX',
  'SMART': 'SMART', // IBKR smart routing
  'LSE': 'LSE',
  'TSE': 'TSE',
  'HKEX': 'HKFE',
  'SGX': 'SGX',
  'ASX': 'ASX',
};

// ============================================================================
// HISTORICAL DATA TIMEFRAMES
// ============================================================================

export const IBKR_BAR_SIZES: Record<string, string> = {
  '1m': '1min',
  '2m': '2mins',
  '3m': '3mins',
  '5m': '5mins',
  '10m': '10mins',
  '15m': '15mins',
  '30m': '30mins',
  '1h': '1hour',
  '2h': '2hours',
  '3h': '3hours',
  '4h': '4hours',
  '8h': '8hours',
  '1d': '1day',
  '1w': '1week',
  '1M': '1month',
};

export const IBKR_DURATION_PERIODS: Record<string, string> = {
  '1D': '1 D',
  '1W': '1 W',
  '1M': '1 M',
  '3M': '3 M',
  '6M': '6 M',
  '1Y': '1 Y',
  '2Y': '2 Y',
  '3Y': '3 Y',
  '5Y': '5 Y',
  '10Y': '10 Y',
};

// ============================================================================
// WEBSOCKET TOPICS
// ============================================================================

export const IBKR_WS_TOPICS = {
  // Solicited (subscribe/unsubscribe)
  MARKET_DATA: 'md',           // smd+CONID+{fields} / umd+CONID+{}
  HISTORICAL_DATA: 'mh',       // smh+CONID+{params} / umh+CONID+{}
  ORDERS: 'or',                // sor+{} / uor+{}
  TRADES: 'tr',                // str+{} / utr+{}
  PNL: 'pl',                   // spl+{} / upl+{}
  ACCOUNT_SUMMARY: 'sd',       // ssd+{} / usd+{}
  ACCOUNT_LEDGER: 'ld',        // sld+{} / uld+{}
  PRICE_LADDER: 'bd',          // sbd+CONID+{} / ubd+CONID+{}

  // Unsolicited (automatic)
  ACCOUNT: 'act',
  AUTH_STATUS: 'sts',
  BULLETIN: 'blt',
  NOTIFICATION: 'ntf',
  SYSTEM: 'system',
};

// ============================================================================
// RATE LIMITS
// ============================================================================

export const IBKR_RATE_LIMITS = {
  // Global
  OAUTH_REQUESTS_PER_SEC: 50,
  GATEWAY_REQUESTS_PER_SEC: 10,

  // Endpoint specific
  ORDERS_PER_5_SEC: 1,
  SCANNER_PARAMS_PER_15_MIN: 1,
  PERFORMANCE_PER_15_MIN: 1,
  SSO_VALIDATE_PER_MIN: 1,
  TICKLE_PER_SEC: 1,

  // Market data
  MAX_CONIDS_PER_SNAPSHOT: 100,
  MAX_FIELDS_PER_SNAPSHOT: 50,
  MAX_HISTORICAL_CONCURRENT: 5,

  // WebSocket
  KEEPALIVE_INTERVAL_MS: 60000, // 60 seconds
};

// ============================================================================
// ERROR CODES
// ============================================================================

export const IBKR_ERROR_CODES: Record<number, string> = {
  0: 'Success',
  401: 'Unauthorized - Invalid credentials or session expired',
  429: 'Too Many Requests - Rate limit exceeded',
  500: 'Internal Server Error',
  503: 'Service Unavailable',
  1100: 'Connectivity restored - data lost',
  1101: 'Connectivity restored - data maintained',
  1102: 'Connectivity between IB and server is broken',
  2104: 'Market data farm connection is OK',
  2106: 'Historical data farm connection is OK',
  2158: 'Security definition data farm connection is OK',
};

// ============================================================================
// BROKER METADATA
// ============================================================================

export const IBKR_METADATA: StockBrokerMetadata = {
  id: 'ibkr',
  name: 'ibkr',
  displayName: 'Interactive Brokers',
  logo: 'https://fincept.in/brokers/ibkr.png',
  website: 'https://www.interactivebrokers.com',

  region: 'us',
  country: 'US',
  currency: 'USD' as Currency,
  exchanges: ['NYSE', 'NASDAQ', 'AMEX', 'ARCA', 'BATS', 'IEX', 'SMART'] as StockExchange[],
  marketHours: {
    open: '09:30',
    close: '16:00',
    timezone: 'America/New_York',
    preMarketOpen: '04:00',
    preMarketClose: '09:30',
    postMarketOpen: '16:00',
    postMarketClose: '20:00',
  },

  features: {
    webSocket: true,
    amo: false,
    gtt: true,
    bracketOrder: true,
    coverOrder: false,
    marginCalculator: true,
    optionsChain: true,
    paperTrading: true,
  },

  tradingFeatures: {
    marketOrders: true,
    limitOrders: true,
    stopOrders: true,
    stopLimitOrders: true,
    trailingStopOrders: true,
  },

  productTypes: ['CASH', 'MARGIN'] as ProductType[],

  authType: 'oauth',

  rateLimit: {
    ordersPerSecond: 0.2,
    quotesPerSecond: 50,
  },

  fees: {
    equity: { brokerage: 0.005 },
  },

  defaultSymbols: [
    'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'TSLA',
    'META', 'NVDA', 'BRK.B', 'JPM', 'V',
    'JNJ', 'WMT', 'PG', 'MA', 'HD',
  ],
};

// ============================================================================
// API ENDPOINTS
// ============================================================================

export const IBKR_ENDPOINTS = {
  // Session/Auth
  AUTH_STATUS: '/iserver/auth/status',
  SSO_INIT: '/iserver/auth/ssodh/init',
  SSO_VALIDATE: '/sso/validate',
  TICKLE: '/tickle',
  LOGOUT: '/logout',
  REAUTHENTICATE: '/iserver/reauthenticate',

  // Account
  ACCOUNTS: '/iserver/accounts',
  ACCOUNT_SWITCH: '/iserver/account',
  PORTFOLIO_ACCOUNTS: '/portfolio/accounts',
  PORTFOLIO_SUBACCOUNTS: '/portfolio/subaccounts',
  ACCOUNT_SUMMARY: '/portfolio/{accountId}/summary',
  ACCOUNT_LEDGER: '/portfolio/{accountId}/ledger',
  ACCOUNT_ALLOCATION: '/portfolio/{accountId}/allocation',
  ACCOUNT_PNL: '/iserver/account/pnl/partitioned',

  // Orders
  ORDERS: '/iserver/account/orders',
  ORDER_PLACE: '/iserver/account/{accountId}/orders',
  ORDER_WHATIF: '/iserver/account/{accountId}/orders/whatif',
  ORDER_STATUS: '/iserver/account/order/status/{orderId}',
  ORDER_MODIFY: '/iserver/account/{accountId}/order/{orderId}',
  ORDER_CANCEL: '/iserver/account/{accountId}/order/{orderId}',
  ORDER_REPLY: '/iserver/reply/{replyId}',
  SUPPRESS_QUESTIONS: '/iserver/questions/suppress',

  // Positions
  POSITIONS: '/portfolio/{accountId}/positions/{pageId}',
  POSITIONS_REALTIME: '/portfolio2/{accountId}/positions',
  POSITION: '/portfolio/{accountId}/position/{conid}',
  COMBO_POSITIONS: '/portfolio/{accountId}/combo/positions',

  // Market Data
  MARKET_DATA_SNAPSHOT: '/iserver/marketdata/snapshot',
  MARKET_DATA_HISTORY: '/iserver/marketdata/history',
  MARKET_DATA_UNSUBSCRIBE: '/iserver/marketdata/unsubscribe',
  MARKET_DATA_UNSUBSCRIBE_ALL: '/iserver/marketdata/unsubscribeall',
  REG_SNAPSHOT: '/md/regsnapshot',
  HMDS_INIT: '/hmds/auth/init',

  // Contracts/Assets
  SECDEF_SEARCH: '/iserver/secdef/search',
  SECDEF_INFO: '/iserver/secdef/info',
  SECDEF_STRIKES: '/iserver/secdef/strikes',
  CONTRACT_INFO: '/iserver/contract/{conid}/info',
  CONTRACT_RULES: '/iserver/contract/{conid}/rules',
  TRSRV_SECDEF: '/trsrv/secdef',
  ALL_CONIDS: '/trsrv/all-conids',
  CURRENCY_PAIRS: '/iserver/currency/pairs',

  // Watchlists
  WATCHLIST_CREATE: '/iserver/watchlist',
  WATCHLISTS: '/iserver/watchlists',
  WATCHLIST: '/iserver/watchlist',

  // Portfolio History/Performance
  ALL_PERIODS: '/pa/allperiods',
  PERFORMANCE: '/pa/performance',
  TRANSACTIONS: '/pa/transactions',

  // Alerts
  ALERT_CREATE: '/iserver/account/{accountId}/alert',
  ALERTS: '/iserver/account/{accountId}/alerts',
  ALERT: '/iserver/account/alert/{orderId}',
  ALERT_ACTIVATE: '/iserver/account/{accountId}/alert/activate',
  ALERT_DELETE: '/iserver/account/{accountId}/alert/{alertId}',

  // Scanner
  SCANNER_PARAMS: '/iserver/scanner/params',
  SCANNER_RUN: '/iserver/scanner/run',
  HMDS_SCANNER: '/hmds/scanner',

  // FA Allocation
  FA_ALLOCATION_ACCOUNTS: '/iserver/account/allocation/accounts',
  FA_ALLOCATION_GROUPS: '/iserver/account/allocation/group',
  FA_ALLOCATION_GROUP_SINGLE: '/iserver/account/allocation/group/single',
  FA_ALLOCATION_GROUP_DELETE: '/iserver/account/allocation/group/delete',

  // Notifications
  FYI_UNREAD: '/fyi/unreadnumber',
  FYI_SETTINGS: '/fyi/settings',
  FYI_DISCLAIMER: '/fyi/disclaimer/{typecode}',
  FYI_NOTIFICATIONS: '/fyi/notifications',
  FYI_NOTIFICATION: '/fyi/notifications/{id}',
};
