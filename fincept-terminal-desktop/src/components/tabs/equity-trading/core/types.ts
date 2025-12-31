/**
 * Unified Type Definitions for Equity Trading System
 * Provides broker-agnostic interfaces for trading operations
 */

// ==================== BROKER TYPES ====================

export type BrokerType = 'fyers' | 'kite';

export enum BrokerStatus {
  DISCONNECTED = 'DISCONNECTED',
  CONNECTING = 'CONNECTING',
  CONNECTED = 'CONNECTED',
  AUTHENTICATED = 'AUTHENTICATED',
  ERROR = 'ERROR',
}

export enum OrderSide {
  BUY = 'BUY',
  SELL = 'SELL',
}

export enum OrderType {
  MARKET = 'MARKET',
  LIMIT = 'LIMIT',
  STOP_LOSS = 'STOP_LOSS',
  STOP_LOSS_MARKET = 'STOP_LOSS_MARKET',
}

export enum OrderStatus {
  PENDING = 'PENDING',
  OPEN = 'OPEN',
  COMPLETE = 'COMPLETE',
  CANCELLED = 'CANCELLED',
  REJECTED = 'REJECTED',
  EXPIRED = 'EXPIRED',
}

export enum ProductType {
  CNC = 'CNC',           // Cash and Carry (Delivery)
  MIS = 'MIS',           // Margin Intraday Square-off
  NRML = 'NRML',         // Normal (F&O)
  MARGIN = 'MARGIN',     // Margin product
  INTRADAY = 'INTRADAY', // Intraday
}

export enum Validity {
  DAY = 'DAY',
  IOC = 'IOC',  // Immediate or Cancel
  TTL = 'TTL',  // Time to Live
}

// ==================== ORDER TYPES ====================

export interface UnifiedOrder {
  id: string;
  brokerId: BrokerType;
  symbol: string;
  exchange: string;
  side: OrderSide;
  type: OrderType;
  quantity: number;
  price?: number;
  triggerPrice?: number;
  product: ProductType;
  validity: Validity;
  status: OrderStatus;
  filledQuantity: number;
  pendingQuantity: number;
  averagePrice: number;
  orderTime: Date;
  updateTime?: Date;
  tag?: string;
  message?: string;
}

export interface OrderRequest {
  symbol: string;
  exchange: string;
  side: OrderSide;
  type: OrderType;
  quantity: number;
  price?: number;
  triggerPrice?: number;
  product: ProductType;
  validity?: Validity;
  disclosedQuantity?: number;
  tag?: string;
}

export interface OrderResponse {
  success: boolean;
  orderId?: string;
  message: string;
  data?: any;
}

// ==================== POSITION TYPES ====================

export interface UnifiedPosition {
  id: string;
  brokerId: BrokerType;
  symbol: string;
  exchange: string;
  product: ProductType;
  quantity: number;
  averagePrice: number;
  lastPrice: number;
  pnl: number;
  realizedPnl: number;
  unrealizedPnl: number;
  side: OrderSide;
  value: number;
}

// ==================== HOLDING TYPES ====================

export interface UnifiedHolding {
  id: string;
  brokerId: BrokerType;
  symbol: string;
  exchange: string;
  isin?: string;
  quantity: number;
  averagePrice: number;
  lastPrice: number;
  pnl: number;
  pnlPercentage: number;
  currentValue: number;
  investedValue: number;
}

// ==================== MARKET DATA TYPES ====================

export interface UnifiedQuote {
  symbol: string;
  exchange: string;
  lastPrice: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  change: number;
  changePercent: number;
  timestamp: Date;
  bid?: number;
  ask?: number;
  bidQty?: number;
  askQty?: number;
  lastTradedQty?: number;
  totalBuyQty?: number;
  totalSellQty?: number;
}

export interface MarketDepthLevel {
  price: number;
  quantity: number;
  orders: number;
}

export interface UnifiedMarketDepth {
  symbol: string;
  exchange: string;
  bids: MarketDepthLevel[];
  asks: MarketDepthLevel[];
  lastPrice: number;
  timestamp: Date;
}

export interface Candle {
  timestamp: Date;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export type TimeInterval = '1m' | '3m' | '5m' | '15m' | '30m' | '1h' | '1d';

// ==================== ACCOUNT TYPES ====================

export interface UnifiedFunds {
  brokerId: BrokerType;
  availableCash: number;
  usedMargin: number;
  availableMargin: number;
  totalCollateral: number;
  equity: number;
  timestamp: Date;
}

export interface UnifiedProfile {
  brokerId: BrokerType;
  userId: string;
  name: string;
  email: string;
  exchanges: string[];
  products: string[];
}

// ==================== WEBSOCKET TYPES ====================

export type WebSocketEvent =
  | 'order_update'
  | 'trade_update'
  | 'position_update'
  | 'quote_update'
  | 'depth_update'
  | 'connection_status';

export interface WebSocketMessage<T = any> {
  event: WebSocketEvent;
  brokerId: BrokerType;
  data: T;
  timestamp: Date;
}

// ==================== AUTH TYPES ====================

export interface BrokerCredentials {
  brokerId: BrokerType;
  apiKey: string;
  apiSecret: string;
  accessToken?: string;
  additionalData?: Record<string, any>;
}

export interface AuthStatus {
  brokerId: BrokerType;
  status: BrokerStatus;
  authenticated: boolean;
  lastAuth?: Date;
  expiresAt?: Date;
  error?: string;
}

// ==================== ERROR TYPES ====================

export class BrokerError extends Error {
  constructor(
    public brokerId: BrokerType,
    public code: string,
    message: string,
    public originalError?: any
  ) {
    super(message);
    this.name = 'BrokerError';
  }
}

export class AuthenticationError extends BrokerError {
  constructor(brokerId: BrokerType, message: string = 'Authentication failed') {
    super(brokerId, 'AUTH_ERROR', message);
    this.name = 'AuthenticationError';
  }
}

export class OrderExecutionError extends BrokerError {
  constructor(brokerId: BrokerType, message: string, public orderId?: string) {
    super(brokerId, 'ORDER_ERROR', message);
    this.name = 'OrderExecutionError';
  }
}

// ==================== MULTI-BROKER TYPES ====================

export interface MultiBrokerOrder extends OrderRequest {
  brokers: BrokerType[];
}

export interface MultiBrokerOrderResult {
  success: boolean;
  results: Record<BrokerType, OrderResponse>;
}

export interface BrokerComparison {
  symbol: string;
  data: Map<BrokerType, UnifiedQuote | UnifiedMarketDepth>;
  latency: Map<BrokerType, number>;
  brokers: Record<BrokerType, {
    quote?: UnifiedQuote;
    depth?: UnifiedMarketDepth;
    latency: number;
  }>;
}

// ==================== PLUGIN SYSTEM TYPES ====================

export interface PluginHook<T = any> {
  name: string;
  execute: (data: T) => Promise<void> | void;
}

export interface OrderPluginContext {
  order: UnifiedOrder;
  broker: BrokerType;
  cancel: () => void;
  modify: (updates: Partial<OrderRequest>) => void;
}

export type OrderHook = PluginHook<OrderPluginContext>;
