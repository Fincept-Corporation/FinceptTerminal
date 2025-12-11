/**
 * Shared Types for Trading UI
 * Exchange-agnostic type definitions
 */

import type { OrderType, OrderSide } from '../../../brokers/crypto/types';

// ============================================================================
// UNIFIED ORDER TYPES
// ============================================================================

export interface UnifiedOrderRequest {
  symbol: string;
  type: OrderType;
  side: OrderSide;
  quantity: number;
  price?: number;

  // Advanced order parameters
  stopPrice?: number;
  takeProfitPrice?: number;
  stopLossPrice?: number;
  trailingPercent?: number;
  trailingAmount?: number;

  // Margin & leverage
  leverage?: number;
  marginMode?: 'cross' | 'isolated';

  // Order options
  reduceOnly?: boolean;
  postOnly?: boolean;
  timeInForce?: 'GTC' | 'IOC' | 'FOK' | 'PO';

  // Iceberg orders
  icebergQty?: number;

  // Client reference
  clientOrderId?: string;
}

// ============================================================================
// EXCHANGE CAPABILITIES
// ============================================================================

export interface ExchangeOrderCapabilities {
  // Order types
  supportsMarket: boolean;
  supportsLimit: boolean;
  supportsStop: boolean;
  supportsStopLimit: boolean;
  supportsTrailingStop: boolean;
  supportsIceberg: boolean;

  // Advanced features
  supportsStopLoss: boolean;
  supportsTakeProfit: boolean;
  supportsPostOnly: boolean;
  supportsReduceOnly: boolean;
  supportsTimeInForce: boolean;

  // Margin & leverage
  supportsMargin: boolean;
  supportsLeverage: boolean;
  maxLeverage?: number;

  // Order management
  supportsOrderEdit: boolean;
  supportsBatchOrders: boolean;
  supportsCancelAll: boolean;
  supportsDeadManSwitch: boolean;

  // Available options
  supportedOrderTypes: OrderType[];
  supportedTimeInForce: string[];
}

// ============================================================================
// POSITION TYPES
// ============================================================================

export interface UnifiedPosition {
  symbol: string;
  side: 'long' | 'short';
  quantity: number;
  entryPrice: number;
  currentPrice: number;

  // P&L
  unrealizedPnl: number;
  realizedPnl?: number;
  pnlPercent: number;

  // Margin info
  leverage?: number;
  marginMode?: 'cross' | 'isolated';
  liquidationPrice?: number;

  // Position value
  notionalValue: number;
  marginUsed?: number;

  // Metadata
  openTime?: number;
  positionId?: string;
}

// ============================================================================
// ORDER TYPES
// ============================================================================

export interface UnifiedOrder {
  id: string;
  symbol: string;
  type: OrderType;
  side: OrderSide;
  quantity: number;
  price?: number;
  stopPrice?: number;

  // Status
  status: 'open' | 'closed' | 'canceled' | 'expired' | 'rejected' | 'pending';
  filled: number;
  remaining: number;

  // Timing
  timestamp: number;
  createdAt: string;
  updatedAt?: string;

  // Execution
  average?: number;
  cost?: number;
  fee?: {
    cost: number;
    currency: string;
  };

  // Metadata
  clientOrderId?: string;
  timeInForce?: string;
  reduceOnly?: boolean;
  postOnly?: boolean;
}

// ============================================================================
// TRADE TYPES
// ============================================================================

export interface UnifiedTrade {
  id: string;
  orderId: string;
  symbol: string;
  side: OrderSide;
  price: number;
  quantity: number;
  cost: number;

  // Fees
  fee: {
    cost: number;
    currency: string;
    rate?: number;
  };

  // Timing
  timestamp: number;
  datetime: string;

  // Trade type
  takerOrMaker: 'taker' | 'maker';

  // Metadata
  info?: any;
}

// ============================================================================
// BALANCE & ACCOUNT
// ============================================================================

export interface UnifiedBalance {
  currency: string;
  free: number;
  used: number;
  total: number;
}

export interface UnifiedAccount {
  balances: UnifiedBalance[];
  totalEquity: number;
  totalMargin?: number;
  availableMargin?: number;
  marginLevel?: number;
}

// ============================================================================
// MARKET DATA
// ============================================================================

export interface UnifiedTicker {
  symbol: string;
  last: number;
  bid: number;
  ask: number;
  high: number;
  low: number;
  open?: number;
  close?: number;
  volume: number;
  quoteVolume?: number;
  change: number;
  changePercent: number;
  timestamp: number;
}

export interface OrderBookLevel {
  price: number;
  size: number;
  count?: number;
}

export interface UnifiedOrderBook {
  symbol: string;
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  timestamp: number;
  checksum?: number;
}

// ============================================================================
// CHART DATA
// ============================================================================

export interface OHLCV {
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export type Timeframe = '1m' | '3m' | '5m' | '15m' | '30m' | '1h' | '2h' | '4h' | '6h' | '12h' | '1d' | '1w' | '1M';

// ============================================================================
// UI STATE TYPES
// ============================================================================

export interface TradingUIState {
  selectedSymbol: string;
  selectedTimeframe: Timeframe;
  chartType: 'candlestick' | 'line' | 'area';
  showDepthChart: boolean;
  showVolumeProfile: boolean;
  activeBottomTab: 'positions' | 'orders' | 'trades' | 'history' | 'stats';
}

// ============================================================================
// ERROR TYPES
// ============================================================================

export interface TradingError {
  code: string;
  message: string;
  userMessage: string;
  exchange?: string;
  timestamp: number;
  details?: any;
}

// ============================================================================
// STATISTICS
// ============================================================================

export interface TradingStatistics {
  // Account
  initialBalance: number;
  currentBalance: number;
  totalEquity: number;

  // P&L
  totalPnL: number;
  realizedPnL: number;
  unrealizedPnL: number;
  returnPercent: number;

  // Trading
  totalTrades: number;
  winningTrades: number;
  losingTrades: number;
  winRate: number;

  // Performance
  largestWin?: number;
  largestLoss?: number;
  averageWin?: number;
  averageLoss?: number;
  profitFactor?: number;
  sharpeRatio?: number;

  // Fees
  totalFees: number;
}
