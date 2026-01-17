/**
 * Type definitions for Indian stock broker integrations
 * Supports NSE, BSE, NFO, MCX and other Indian exchanges
 */

// ============================================================================
// Exchange Types
// ============================================================================

export type IndianExchange =
  | 'NSE'       // National Stock Exchange - Equity
  | 'BSE'       // Bombay Stock Exchange - Equity
  | 'NFO'       // NSE Futures & Options
  | 'BFO'       // BSE Futures & Options
  | 'CDS'       // Currency Derivatives Segment
  | 'BCD'       // BSE Currency Derivatives
  | 'MCX'       // Multi Commodity Exchange
  | 'NSE_INDEX' // NSE Indices (NIFTY, BANKNIFTY)
  | 'BSE_INDEX' // BSE Indices (SENSEX)
  | 'MCX_INDEX' // MCX Indices
  | 'CDS_INDEX';// Currency Indices

export type InstrumentType =
  | 'EQ'        // Equity
  | 'FUT'       // Futures
  | 'CE'        // Call Option
  | 'PE'        // Put Option
  | 'INDEX'     // Index
  | 'FUTIDX'    // Index Futures
  | 'OPTIDX'    // Index Options
  | 'FUTSTK'    // Stock Futures
  | 'OPTSTK';   // Stock Options

export type ProductType =
  | 'CNC'       // Cash and Carry (Delivery)
  | 'MIS'       // Margin Intraday Square-off
  | 'NRML'      // Normal (Carry Forward)
  | 'CO'        // Cover Order
  | 'BO';       // Bracket Order

export type OrderType =
  | 'MARKET'    // Market Order
  | 'LIMIT'     // Limit Order
  | 'SL'        // Stop Loss
  | 'SL-M';     // Stop Loss Market

export type TransactionType = 'BUY' | 'SELL';

export type OrderValidity =
  | 'DAY'       // Valid for the day
  | 'IOC'       // Immediate or Cancel
  | 'GTC'       // Good Till Cancelled (not supported by all)
  | 'GTD';      // Good Till Date

export type OrderStatus =
  | 'PENDING'
  | 'OPEN'
  | 'COMPLETE'
  | 'CANCELLED'
  | 'REJECTED'
  | 'MODIFIED'
  | 'TRIGGER_PENDING'
  | 'AMO_SUBMITTED';

// ============================================================================
// Symbol & Instrument Types
// ============================================================================

export interface SymbolToken {
  symbol: string;           // Standardized symbol (e.g., 'RELIANCE')
  brokerSymbol: string;     // Broker-specific symbol
  name: string;             // Full name
  exchange: IndianExchange;
  brokerExchange: string;   // Broker-specific exchange code
  token: string;            // Instrument token
  expiry?: string;          // Expiry date for derivatives (DD-MMM-YY)
  strike?: number;          // Strike price for options
  lotSize: number;
  instrumentType: InstrumentType;
  tickSize: number;
}

export interface SymbolSearchResult {
  symbol: string;
  name: string;
  exchange: IndianExchange;
  token: string;
  instrumentType: InstrumentType;
  expiry?: string;
  strike?: number;
}

// ============================================================================
// Order Types
// ============================================================================

export interface OrderParams {
  symbol: string;
  exchange: IndianExchange;
  transactionType: TransactionType;
  orderType: OrderType;
  productType: ProductType;
  quantity: number;
  price?: number;           // Required for LIMIT orders
  triggerPrice?: number;    // Required for SL/SL-M orders
  disclosedQuantity?: number;
  validity?: OrderValidity;
  tag?: string;             // Order tag for tracking
  amo?: boolean;            // After Market Order
}

export interface OrderResponse {
  success: boolean;
  orderId?: string;
  message?: string;
  errorCode?: string;
}

export interface Order {
  orderId: string;
  exchangeOrderId?: string;
  symbol: string;
  exchange: IndianExchange;
  transactionType: TransactionType;
  orderType: OrderType;
  productType: ProductType;
  quantity: number;
  filledQuantity: number;
  pendingQuantity: number;
  price: number;
  averagePrice: number;
  triggerPrice?: number;
  status: OrderStatus;
  statusMessage?: string;
  orderTimestamp: number;
  exchangeTimestamp?: number;
  tag?: string;
}

export interface ModifyOrderParams {
  orderId: string;
  orderType?: OrderType;
  quantity?: number;
  price?: number;
  triggerPrice?: number;
  validity?: OrderValidity;
}

// ============================================================================
// Position & Holdings Types
// ============================================================================

export interface Position {
  symbol: string;
  exchange: IndianExchange;
  productType: ProductType;
  quantity: number;
  averagePrice: number;
  lastPrice: number;
  pnl: number;
  pnlPercentage: number;
  dayPnl: number;
  buyQuantity: number;
  sellQuantity: number;
  buyValue: number;
  sellValue: number;
  multiplier: number;
}

export interface Holding {
  symbol: string;
  exchange: IndianExchange;
  isin: string;
  quantity: number;
  averagePrice: number;
  lastPrice: number;
  pnl: number;
  pnlPercentage: number;
  dayChange: number;
  dayChangePercentage: number;
  pledgedQuantity: number;
  collateralQuantity: number;
}

// ============================================================================
// Account & Margin Types
// ============================================================================

export interface FundsData {
  availableCash: number;
  usedMargin: number;
  availableMargin: number;
  collateral: number;
  unrealizedMtm: number;
  realizedMtm: number;
}

export interface MarginRequired {
  total: number;
  var: number;
  span: number;
  exposure: number;
  premium: number;
}

// ============================================================================
// Market Data Types
// ============================================================================

export interface Quote {
  symbol: string;
  exchange: IndianExchange;
  lastPrice: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  change: number;
  changePercent: number;
  averagePrice?: number;
  lastTradedQuantity?: number;
  totalBuyQuantity?: number;
  totalSellQuantity?: number;
  openInterest?: number;
  timestamp: number;
}

export interface MarketDepthLevel {
  price: number;
  quantity: number;
  orders: number;
}

export interface MarketDepth {
  buy: MarketDepthLevel[];
  sell: MarketDepthLevel[];
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

// ============================================================================
// WebSocket Types
// ============================================================================

export type SubscriptionMode =
  | 'ltp'       // Last Traded Price only
  | 'quote'     // Quote data (OHLC + Volume)
  | 'full';     // Full data including market depth

export interface TickData {
  symbol: string;
  exchange: IndianExchange;
  token: string;
  mode: SubscriptionMode;
  lastPrice: number;
  timestamp: number;
  // Quote mode fields
  open?: number;
  high?: number;
  low?: number;
  close?: number;
  volume?: number;
  averagePrice?: number;
  lastTradedQuantity?: number;
  totalBuyQuantity?: number;
  totalSellQuantity?: number;
  openInterest?: number;
  // Full mode fields
  depth?: MarketDepth;
}

export interface WebSocketConfig {
  apiKey: string;
  accessToken: string;
  onTick?: (tick: TickData) => void;
  onConnect?: () => void;
  onDisconnect?: () => void;
  onError?: (error: Error) => void;
  reconnectAttempts?: number;
  reconnectDelay?: number;
}

// ============================================================================
// Authentication Types
// ============================================================================

export interface BrokerCredentials {
  brokerId: string;
  apiKey: string;
  apiSecret?: string;
  accessToken?: string;
  refreshToken?: string;
  userId?: string;
  password?: string;         // For brokers requiring password
  totp?: string;            // For 2FA
  expiresAt?: number;       // Token expiry timestamp
}

export interface AuthResponse {
  success: boolean;
  accessToken?: string;
  refreshToken?: string;
  userId?: string;
  expiresAt?: number;
  message?: string;
  errorCode?: string;
}

export interface AuthCallbackParams {
  requestToken?: string;
  code?: string;
  state?: string;
}

// ============================================================================
// Broker Adapter Interface
// ============================================================================

export interface IIndianBrokerAdapter {
  // Broker identification
  readonly brokerId: string;
  readonly brokerName: string;
  readonly isConnected: boolean;

  // Authentication
  authenticate(credentials: BrokerCredentials): Promise<AuthResponse>;
  handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse>;
  logout(): Promise<void>;
  refreshSession(): Promise<AuthResponse>;

  // Account
  getFunds(): Promise<FundsData>;
  getMarginRequired(params: OrderParams): Promise<MarginRequired>;
  calculateMargin(orders: MarginOrderParams[]): Promise<MarginCalculation>;

  // Orders
  placeOrder(params: OrderParams): Promise<OrderResponse>;
  modifyOrder(params: ModifyOrderParams): Promise<OrderResponse>;
  cancelOrder(orderId: string): Promise<OrderResponse>;
  getOrders(): Promise<Order[]>;
  getOrderHistory(orderId: string): Promise<Order[]>;
  getTradeBook(): Promise<Trade[]>;
  cancelAllOrders(): Promise<BulkOperationResult>;

  // Smart Orders
  placeSmartOrder(params: SmartOrderParams): Promise<OrderResponse>;

  // Positions & Holdings
  getPositions(): Promise<Position[]>;
  getHoldings(): Promise<Holding[]>;
  getOpenPosition(symbol: string, exchange: IndianExchange, product: ProductType): Promise<OpenPositionResult>;
  closeAllPositions(): Promise<BulkOperationResult>;

  // Market Data
  getQuote(symbol: string, exchange: IndianExchange): Promise<Quote>;
  getQuotes(symbols: Array<{ symbol: string; exchange: IndianExchange }>): Promise<Quote[]>;
  getOHLCV(
    symbol: string,
    exchange: IndianExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]>;
  getMarketDepth(symbol: string, exchange: IndianExchange): Promise<MarketDepth>;

  // Symbol Management
  searchSymbols(query: string, exchange?: IndianExchange): Promise<SymbolSearchResult[]>;
  getSymbolToken(symbol: string, exchange: IndianExchange): Promise<SymbolToken | null>;

  // WebSocket
  connectWebSocket(config: WebSocketConfig): Promise<void>;
  disconnectWebSocket(): Promise<void>;
  subscribe(symbol: string, exchange: IndianExchange, mode: SubscriptionMode): Promise<void>;
  unsubscribe(symbol: string, exchange: IndianExchange): Promise<void>;

  // Master Contract
  downloadMasterContract(): Promise<void>;
  getMasterContractLastUpdated(): Promise<Date | null>;
}

// ============================================================================
// Trade Book Types
// ============================================================================

export interface Trade {
  tradeId: string;
  orderId: string;
  symbol: string;
  exchange: IndianExchange;
  transactionType: TransactionType;
  quantity: number;
  price: number;
  product: ProductType;
  tradeTime: Date;
  exchangeOrderId?: string;
  exchangeTradeId?: string;
}

// ============================================================================
// Smart Order Types
// ============================================================================

export interface SmartOrderParams {
  symbol: string;
  exchange: IndianExchange;
  transactionType: TransactionType;
  quantity: number;
  product: ProductType;
  orderType: OrderType;
  price?: number;
  triggerPrice?: number;
  positionSize?: number;  // Current position size to adjust order
}

export interface OpenPositionResult {
  symbol: string;
  exchange: IndianExchange;
  product: ProductType;
  quantity: number;
  position?: Position;
}

// ============================================================================
// Bulk Operation Types
// ============================================================================

export interface BulkOperationResult {
  success: boolean;
  totalCount: number;
  successCount: number;
  failedCount: number;
  results: Array<{
    success: boolean;
    orderId?: string;
    error?: string;
  }>;
}

// ============================================================================
// Margin Calculation Types
// ============================================================================

export interface MarginOrderParams {
  symbol: string;
  exchange: IndianExchange;
  transactionType: TransactionType;
  quantity: number;
  product: ProductType;
  orderType: OrderType;
  price?: number;
  triggerPrice?: number;
}

export interface MarginCalculation {
  totalMargin: number;
  initialMargin: number;
  exposureMargin: number;
  spanMargin?: number;
  optionPremium?: number;
  leverage?: number;
  orders: Array<{
    symbol: string;
    margin: number;
    pnl?: number;
  }>;
}

// ============================================================================
// Broker Metadata
// ============================================================================

export interface IndianBrokerMetadata {
  id: string;
  name: string;
  displayName: string;
  logo?: string;
  website: string;
  supportedExchanges: IndianExchange[];
  supportedProducts: ProductType[];
  supportedOrderTypes: OrderType[];
  features: {
    webSocket: boolean;
    amo: boolean;
    gtt: boolean;
    bracketOrder: boolean;
    coverOrder: boolean;
    marginCalculator: boolean;
  };
  authType: 'oauth' | 'totp' | 'password' | 'api_key';
  rateLimit: {
    ordersPerSecond: number;
    quotesPerSecond: number;
  };
}

// ============================================================================
// API Response Types
// ============================================================================

export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
  errorCode?: string;
  timestamp: number;
}

export interface ApiError {
  code: string;
  message: string;
  details?: Record<string, unknown>;
}
