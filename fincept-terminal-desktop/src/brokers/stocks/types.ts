/**
 * Unified Stock Broker Types
 *
 * Base types for all stock brokers globally (India, US, Europe, Asia, etc.)
 * Region-specific features handled via metadata and feature flags
 */

// ============================================================================
// REGION & MARKET TYPES
// ============================================================================

export type Region = 'india' | 'us' | 'europe' | 'uk' | 'asia' | 'australia' | 'canada';

export type Currency = 'INR' | 'USD' | 'EUR' | 'GBP' | 'JPY' | 'AUD' | 'CAD' | 'SGD' | 'HKD';

export interface MarketHours {
  open: string;          // "09:15" (24hr format)
  close: string;         // "15:30"
  timezone: string;      // "Asia/Kolkata"
  preMarketOpen?: string;
  preMarketClose?: string;
  postMarketOpen?: string;
  postMarketClose?: string;
}

export type MarketStatus = 'PRE_OPEN' | 'OPEN' | 'CLOSED' | 'POST_MARKET' | 'HOLIDAY';

// ============================================================================
// EXCHANGE TYPES
// ============================================================================

export interface Exchange {
  code: string;          // "NSE", "NYSE", "LSE"
  name: string;          // "National Stock Exchange"
  country: string;       // "IN", "US", "GB"
  currency: Currency;
  marketHours: MarketHours;
}

// Common exchanges by region
export type IndianExchange = 'NSE' | 'BSE' | 'NFO' | 'BFO' | 'CDS' | 'BCD' | 'MCX';
export type USExchange = 'NYSE' | 'NASDAQ' | 'AMEX' | 'ARCA' | 'BATS';
export type EuropeExchange = 'LSE' | 'XETRA' | 'EURONEXT' | 'SIX';

// Union of all exchanges
export type StockExchange = IndianExchange | USExchange | EuropeExchange | string;

// ============================================================================
// INSTRUMENT TYPES
// ============================================================================

export type InstrumentType =
  | 'EQUITY'       // Stocks
  | 'ETF'          // Exchange Traded Funds
  | 'INDEX'        // Index
  | 'FUTURE'       // Futures
  | 'OPTION'       // Options
  | 'BOND'         // Bonds
  | 'MUTUAL_FUND'; // Mutual Funds

export type OptionType = 'CE' | 'PE' | 'CALL' | 'PUT';

export interface Instrument {
  symbol: string;              // "RELIANCE", "AAPL"
  name: string;                // "Reliance Industries Ltd"
  exchange: StockExchange;
  instrumentType: InstrumentType;
  currency: Currency;
  token?: string;              // Broker-specific token
  isin?: string;               // International Securities ID
  lotSize: number;
  tickSize: number;
  // Derivatives specific
  expiry?: string;             // "2024-01-25"
  strike?: number;
  optionType?: OptionType;
  underlying?: string;
}

// ============================================================================
// ORDER TYPES
// ============================================================================

export type OrderSide = 'BUY' | 'SELL';

export type OrderType =
  | 'MARKET'
  | 'LIMIT'
  | 'STOP'           // Stop loss
  | 'STOP_LIMIT'     // Stop loss limit
  | 'TRAILING_STOP';

export type OrderValidity =
  | 'DAY'            // Valid for the day
  | 'IOC'            // Immediate or Cancel
  | 'GTC'            // Good Till Cancelled
  | 'GTD';           // Good Till Date

export type OrderStatus =
  | 'PENDING'
  | 'OPEN'
  | 'PARTIALLY_FILLED'
  | 'FILLED'
  | 'CANCELLED'
  | 'REJECTED'
  | 'EXPIRED'
  | 'TRIGGER_PENDING';   // For stop orders

// Product types (vary by region but have common concepts)
export type ProductType =
  | 'CASH'           // Delivery/Investment (CNC in India)
  | 'INTRADAY'       // Day trading (MIS in India)
  | 'MARGIN'         // Margin trading (NRML in India)
  | 'COVER'          // Cover order
  | 'BRACKET';       // Bracket order

// ============================================================================
// ORDER PARAMS & RESPONSE
// ============================================================================

export interface OrderParams {
  symbol: string;
  exchange: StockExchange;
  side: OrderSide;
  quantity: number;
  orderType: OrderType;
  productType: ProductType;
  price?: number;              // For limit orders
  triggerPrice?: number;       // For stop orders
  validity?: OrderValidity;
  disclosedQuantity?: number;  // Iceberg orders
  tag?: string;                // Order tag/reference
  // Region-specific (optional)
  amo?: boolean;               // After Market Order (India)
  squareOff?: number;          // Bracket order target
  stopLoss?: number;           // Bracket order SL
}

export interface ModifyOrderParams {
  orderId: string;
  quantity?: number;
  price?: number;
  triggerPrice?: number;
  orderType?: OrderType;
  validity?: OrderValidity;
}

export interface OrderResponse {
  success: boolean;
  orderId?: string;
  message?: string;
  errorCode?: string;
}

export interface Order {
  orderId: string;
  symbol: string;
  exchange: StockExchange;
  side: OrderSide;
  quantity: number;
  filledQuantity: number;
  pendingQuantity: number;
  price: number;
  averagePrice: number;
  triggerPrice?: number;
  orderType: OrderType;
  productType: ProductType;
  validity: OrderValidity;
  status: OrderStatus;
  statusMessage?: string;
  placedAt: Date;
  updatedAt?: Date;
  exchangeOrderId?: string;
  tag?: string;
}

// ============================================================================
// TRADE TYPES
// ============================================================================

export interface Trade {
  tradeId: string;
  orderId: string;
  symbol: string;
  exchange: StockExchange;
  side: OrderSide;
  quantity: number;
  price: number;
  productType: ProductType;
  tradedAt: Date;
  exchangeTradeId?: string;
}

// ============================================================================
// POSITION & HOLDINGS
// ============================================================================

export interface Position {
  symbol: string;
  exchange: StockExchange;
  productType: ProductType;
  quantity: number;              // Net quantity (+ve long, -ve short)
  buyQuantity: number;
  sellQuantity: number;
  buyValue: number;
  sellValue: number;
  averagePrice: number;
  lastPrice: number;
  pnl: number;
  pnlPercent: number;
  dayPnl: number;
  overnight?: boolean;           // Carried forward position
}

export interface Holding {
  symbol: string;
  exchange: StockExchange;
  quantity: number;
  averagePrice: number;
  lastPrice: number;
  investedValue: number;
  currentValue: number;
  pnl: number;
  pnlPercent: number;
  isin?: string;
  // Pledged/collateral info
  pledgedQuantity?: number;
  collateralQuantity?: number;
  t1Quantity?: number;           // T+1 holdings (India specific)
}

// ============================================================================
// FUNDS & MARGIN
// ============================================================================

export interface Funds {
  availableCash: number;
  usedMargin: number;
  availableMargin: number;
  totalBalance: number;
  currency: Currency;
  // Segment-wise (optional)
  equityAvailable?: number;
  commodityAvailable?: number;
  // Collateral
  collateral?: number;
  // MTM
  unrealizedPnl?: number;
  realizedPnl?: number;
}

export interface MarginRequired {
  totalMargin: number;
  initialMargin: number;
  exposureMargin?: number;
  spanMargin?: number;
  optionPremium?: number;
  // Breakdown per order (for basket)
  orders?: Array<{
    symbol: string;
    margin: number;
  }>;
}

// ============================================================================
// MARKET DATA
// ============================================================================

export interface Quote {
  symbol: string;
  exchange: StockExchange;
  lastPrice: number;
  open: number;
  high: number;
  low: number;
  close: number;
  previousClose: number;
  change: number;
  changePercent: number;
  volume: number;
  value?: number;                // Traded value
  averagePrice?: number;
  bid: number;
  bidQty: number;
  ask: number;
  askQty: number;
  timestamp: number;
  // Derivatives specific
  openInterest?: number;
  oiChange?: number;
  // Circuit limits (India specific)
  upperCircuit?: number;
  lowerCircuit?: number;
  // Error message if quote fetch failed
  error?: string;
}

export interface OHLCV {
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export type TimeFrame =
  | '1m' | '3m' | '5m' | '10m' | '15m' | '30m'
  | '1h' | '2h' | '4h'
  | '1d' | '1w' | '1M';

export interface DepthLevel {
  price: number;
  quantity: number;
  orders?: number;
}

export interface MarketDepth {
  bids: DepthLevel[];
  asks: DepthLevel[];
  timestamp?: number;
}

// ============================================================================
// WEBSOCKET TYPES
// ============================================================================

export type SubscriptionMode = 'ltp' | 'quote' | 'full';

export interface TickData {
  symbol: string;
  exchange: StockExchange;
  token?: string;
  mode: SubscriptionMode;
  lastPrice: number;
  timestamp: number;
  // Quote mode
  open?: number;
  high?: number;
  low?: number;
  close?: number;
  change?: number;
  changePercent?: number;
  volume?: number;
  // Full mode
  bid?: number;
  bidQty?: number;
  ask?: number;
  askQty?: number;
  depth?: MarketDepth;
  openInterest?: number;
}

export interface WebSocketConfig {
  autoReconnect?: boolean;
  reconnectInterval?: number;
  maxReconnectAttempts?: number;
  heartbeatInterval?: number;
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

export interface BrokerCredentials {
  apiKey: string;
  apiSecret?: string;
  accessToken?: string;
  refreshToken?: string;
  userId?: string;
  // Region specific
  totpSecret?: string;         // For TOTP-based auth
  password?: string;           // For password-based auth
  pin?: string;                // Trading PIN
}

export interface AuthResponse {
  success: boolean;
  accessToken?: string;
  refreshToken?: string;
  userId?: string;
  expiresAt?: Date;
  message?: string;
  errorCode?: string;
}

export interface AuthCallbackParams {
  requestToken?: string;
  authCode?: string;
  state?: string;
}

// ============================================================================
// BULK OPERATIONS
// ============================================================================

export interface BulkOperationResult {
  success: boolean;
  totalCount: number;
  successCount: number;
  failedCount: number;
  results: Array<{
    success: boolean;
    orderId?: string;
    symbol?: string;
    error?: string;
  }>;
}

// ============================================================================
// SMART ORDER
// ============================================================================

export interface SmartOrderParams extends OrderParams {
  positionSize?: number;       // Current position to adjust order logic
}

// ============================================================================
// BROKER ADAPTER INTERFACE
// ============================================================================

export interface IStockBrokerAdapter {
  // Identification
  readonly brokerId: string;
  readonly brokerName: string;
  readonly region: Region;
  readonly isConnected: boolean;

  // Authentication
  setCredentials?(apiKey: string, apiSecret: string): void;  // Set credentials before OAuth
  getAuthUrl?(): string;       // For OAuth brokers
  authenticate(credentials: BrokerCredentials): Promise<AuthResponse>;
  handleAuthCallback?(params: AuthCallbackParams): Promise<AuthResponse>;
  logout(): Promise<void>;
  refreshSession?(): Promise<AuthResponse>;

  // Account
  getFunds(): Promise<Funds>;
  getMarginRequired(params: OrderParams): Promise<MarginRequired>;
  calculateMargin(orders: OrderParams[]): Promise<MarginRequired>;

  // Orders
  placeOrder(params: OrderParams): Promise<OrderResponse>;
  modifyOrder(params: ModifyOrderParams): Promise<OrderResponse>;
  cancelOrder(orderId: string): Promise<OrderResponse>;
  getOrders(): Promise<Order[]>;
  getOrderHistory?(orderId: string): Promise<Order[]>;
  getTradeBook(): Promise<Trade[]>;

  // Bulk Operations
  cancelAllOrders(): Promise<BulkOperationResult>;
  closeAllPositions(): Promise<BulkOperationResult>;

  // Smart Orders
  placeSmartOrder(params: SmartOrderParams): Promise<OrderResponse>;

  // Positions & Holdings
  getPositions(): Promise<Position[]>;
  getHoldings(): Promise<Holding[]>;
  getOpenPosition(symbol: string, exchange: StockExchange, productType: ProductType): Promise<Position | null>;

  // Market Data
  getQuote(symbol: string, exchange: StockExchange): Promise<Quote>;
  getQuotes(instruments: Array<{ symbol: string; exchange: StockExchange }>): Promise<Quote[]>;
  getOHLCV(symbol: string, exchange: StockExchange, timeframe: TimeFrame, from: Date, to: Date): Promise<OHLCV[]>;
  getMarketDepth(symbol: string, exchange: StockExchange): Promise<MarketDepth>;

  // Symbol Search
  searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]>;
  getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null>;

  // WebSocket
  connectWebSocket(config?: WebSocketConfig): Promise<void>;
  disconnectWebSocket(): Promise<void>;
  subscribe(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void>;
  unsubscribe(symbol: string, exchange: StockExchange): Promise<void>;
  onTick(callback: (tick: TickData) => void): void;
  offTick(callback: (tick: TickData) => void): void;
  onDepth(callback: (depth: MarketDepth & { symbol: string; exchange: StockExchange }) => void): void;
  offDepth(callback: (depth: MarketDepth & { symbol: string; exchange: StockExchange }) => void): void;

  // Master Contract (Symbol Database)
  downloadMasterContract?(): Promise<void>;
  getMasterContractLastUpdated?(): Promise<Date | null>;
}

// ============================================================================
// BROKER METADATA
// ============================================================================

export interface StockBrokerMetadata {
  id: string;
  name: string;
  displayName: string;
  logo?: string;
  website: string;

  // Region & Market
  region: Region;
  country: string;             // ISO country code
  currency: Currency;
  exchanges: StockExchange[];
  marketHours: MarketHours;

  // Features
  features: {
    webSocket: boolean;
    amo: boolean;              // After Market Orders
    gtt: boolean;              // Good Till Triggered
    bracketOrder: boolean;
    coverOrder: boolean;
    marginCalculator: boolean;
    optionsChain: boolean;
    paperTrading: boolean;
  };

  // Trading capabilities
  tradingFeatures: {
    marketOrders: boolean;
    limitOrders: boolean;
    stopOrders: boolean;
    stopLimitOrders: boolean;
    trailingStopOrders: boolean;
  };

  // Product types supported
  productTypes: ProductType[];

  // Auth type
  authType: 'oauth' | 'totp' | 'password' | 'api_key' | 'enckey';

  // Rate limits
  rateLimit: {
    ordersPerSecond: number;
    quotesPerSecond: number;
  };

  // Fees
  fees?: {
    equity?: { brokerage: number; stt?: number };
    intraday?: { brokerage: number };
    fno?: { brokerage: number };
  };

  // Default symbols for this broker
  defaultSymbols: string[];
}

// ============================================================================
// API RESPONSE WRAPPER
// ============================================================================

export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
  errorCode?: string;
  timestamp: number;
}
