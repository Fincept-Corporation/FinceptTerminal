/**
 * Unified Exchange Types
 *
 * Professional-grade type definitions for all exchange operations
 * Used across trading, paper trading, and agentic trading systems
 */

import type { Exchange, Market, Ticker, OrderBook, Trade, OHLCV, Balance, Order, Position } from 'ccxt';

// Re-export CCXT types for convenience
export type { Order, Position, Balance, Market, Ticker, OrderBook, Trade, OHLCV };

// ============================================================================
// CORE EXCHANGE INTERFACE
// ============================================================================

export interface IExchangeAdapter {
  // Metadata
  id: string;
  name: string;

  // Connection Management
  connect(): Promise<void>;
  disconnect(): Promise<void>;
  isConnected(): boolean;

  // Authentication
  authenticate(credentials: ExchangeCredentials): Promise<void>;
  isAuthenticated(): boolean;

  // Market Data
  fetchMarkets(): Promise<Market[]>;
  fetchTicker(symbol: string): Promise<Ticker>;
  fetchTickers(symbols?: string[]): Promise<Record<string, Ticker>>;
  fetchOrderBook(symbol: string, limit?: number): Promise<OrderBook>;
  fetchTrades(symbol: string, limit?: number): Promise<Trade[]>;
  fetchOHLCV(symbol: string, timeframe: string, limit?: number): Promise<OHLCV[]>;

  // Account & Balance
  fetchBalance(): Promise<Balance>;
  fetchPositions(symbols?: string[]): Promise<Position[]>;

  // Trading
  createOrder(symbol: string, type: OrderType, side: OrderSide, amount: number, price?: number, params?: OrderParams): Promise<Order>;
  cancelOrder(orderId: string, symbol: string): Promise<Order>;
  cancelAllOrders(symbol?: string): Promise<Order[]>;
  fetchOrder(orderId: string, symbol: string): Promise<Order>;
  fetchOpenOrders(symbol?: string): Promise<Order[]>;
  fetchClosedOrders(symbol?: string, limit?: number): Promise<Order[]>;

  // Real-time (WebSocket)
  watchTicker?(symbol: string): AsyncGenerator<Ticker>;
  watchOrderBook?(symbol: string, limit?: number): AsyncGenerator<OrderBook>;
  watchTrades?(symbol: string): AsyncGenerator<Trade[]>;
  watchBalance?(): AsyncGenerator<Balance>;
  watchOrders?(symbol?: string): AsyncGenerator<Order[]>;

  // Event System
  on(event: ExchangeEvent, callback: (data: ExchangeEventData) => void): void;
  off(event: ExchangeEvent, callback: (data: ExchangeEventData) => void): void;
  emit(event: ExchangeEvent, data: ExchangeEventData): void;

  // Capabilities
  getCapabilities(): ExchangeCapabilities;
}

// ============================================================================
// CREDENTIALS & AUTHENTICATION
// ============================================================================

export interface ExchangeCredentials {
  apiKey: string;
  secret: string;
  password?: string; // Some exchanges require
  uid?: string; // Some exchanges require
  privateKey?: string; // For some DEX integrations
  walletAddress?: string;
  testnet?: boolean;
}

export interface ExchangeConfig {
  exchange: string;
  credentials?: ExchangeCredentials;
  sandbox?: boolean;
  enableRateLimit?: boolean;
  timeout?: number;
  proxy?: string;
  options?: Record<string, any>;
}

// ============================================================================
// ORDER TYPES
// ============================================================================

export type OrderType =
  | 'market'
  | 'limit'
  | 'stop'
  | 'stop_market'
  | 'stop_limit'
  | 'trailing_stop'
  | 'iceberg'
  | 'twap'
  | 'vwap';

export type OrderSide = 'buy' | 'sell';

export type OrderStatus =
  | 'open'
  | 'closed'
  | 'canceled'
  | 'expired'
  | 'rejected'
  | 'pending';

export type TimeInForce = 'GTC' | 'IOC' | 'FOK' | 'PO';

export interface OrderParams {
  timeInForce?: TimeInForce;
  postOnly?: boolean;
  reduceOnly?: boolean;
  stopPrice?: number;
  triggerPrice?: number;
  trailingAmount?: number;
  trailingPercent?: number;
  icebergQty?: number;
  clientOrderId?: string;
  leverage?: number;
  marginMode?: 'cross' | 'isolated';
  [key: string]: any;
}

// ============================================================================
// MARKET DATA TYPES
// ============================================================================

export interface ExtendedTicker extends Ticker {
  fundingRate?: number;
  fundingTimestamp?: number;
  openInterest?: number;
  volume24h?: number;
  volumeQuote?: number;
}

export interface ExtendedOrderBook {
  bids: Array<[number, number]>;
  asks: Array<[number, number]>;
  symbol?: string;
  checksum?: number;
  timestamp?: number;
  datetime?: string;
  nonce?: number;
}

// ============================================================================
// TRADING SESSION
// ============================================================================

export interface TradingSession {
  sessionId: string;
  exchange: string;
  mode: 'live' | 'paper' | 'backtest';
  startTime: number;
  endTime?: number;
  initialBalance: number;
  currentBalance: number;
  pnl: number;
  trades: number;
  winRate: number;
  sharpeRatio?: number;
}

// ============================================================================
// POSITION & RISK MANAGEMENT
// ============================================================================

export interface ExtendedPosition extends Position {
  entryPrice: number;
  markPrice: number;
  liquidationPrice?: number;
  leverage: number;
  marginMode: 'cross' | 'isolated';
  unrealizedPnl: number;
  realizedPnl: number;
  marginRatio?: number;
  maintenanceMargin?: number;
  timestamp: number;
}

export interface RiskMetrics {
  totalExposure: number;
  availableMargin: number;
  marginLevel: number;
  maxDrawdown: number;
  positionCount: number;
  largestPosition: number;
  concentrationRisk: number;
}

// ============================================================================
// EXCHANGE CAPABILITIES
// ============================================================================

export interface ExchangeCapabilities {
  // Trading Features
  spot: boolean;
  margin: boolean;
  futures: boolean;
  options: boolean;
  swap: boolean;

  // Order Types
  supportedOrderTypes: OrderType[];
  supportedTimeInForce: TimeInForce[];

  // Advanced Features
  leverageTrading: boolean;
  stopOrders: boolean;
  conditionalOrders: boolean;
  algoOrders: boolean;

  // Market Data
  realtimeData: boolean;
  historicalData: boolean;
  websocketSupport: boolean;

  // Account
  multiAccount: boolean;
  subAccounts: boolean;
  portfolioMargin: boolean;

  // API Features
  restApi: boolean;
  websocketApi: boolean;
  fixApi: boolean;

  // Rate Limits
  rateLimits: {
    public: number;
    private: number;
    trading: number;
  };
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

export class ExchangeError extends Error {
  constructor(
    message: string,
    public code: string,
    public exchange: string,
    public details?: any
  ) {
    super(message);
    this.name = 'ExchangeError';
  }
}

export class AuthenticationError extends ExchangeError {
  constructor(message: string, exchange: string, details?: any) {
    super(message, 'AUTH_ERROR', exchange, details);
    this.name = 'AuthenticationError';
  }
}

export class RateLimitError extends ExchangeError {
  constructor(message: string, exchange: string, details?: any) {
    super(message, 'RATE_LIMIT', exchange, details);
    this.name = 'RateLimitError';
  }
}

export class InsufficientFunds extends ExchangeError {
  constructor(message: string, exchange: string, details?: any) {
    super(message, 'INSUFFICIENT_FUNDS', exchange, details);
    this.name = 'InsufficientFunds';
  }
}

export class OrderNotFound extends ExchangeError {
  constructor(message: string, exchange: string, details?: any) {
    super(message, 'ORDER_NOT_FOUND', exchange, details);
    this.name = 'OrderNotFound';
  }
}

// ============================================================================
// EVENTS
// ============================================================================

export type ExchangeEvent =
  | 'connected'
  | 'disconnected'
  | 'authenticated'
  | 'error'
  | 'ticker'
  | 'orderbook'
  | 'trade'
  | 'order'
  | 'balance'
  | 'position'
  | 'reset';

export interface ExchangeEventData {
  event: ExchangeEvent;
  exchange: string;
  timestamp: number;
  data: any;
}
