/**
 * Paper Trading Module - Standalone Types
 *
 * Universal paper trading system that works with any broker/exchange
 * Supports crypto, stocks, forex, commodities, etc.
 */

import type { Order, Balance, Position } from 'ccxt';

// Re-export common types for convenience
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

export interface OrderParams {
  timeInForce?: 'GTC' | 'IOC' | 'FOK' | 'PO';
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
// PAPER TRADING CONFIG
// ============================================================================

export interface PaperTradingConfig {
  // Core settings
  portfolioId: string;
  portfolioName: string;
  provider: string; // Any provider: 'kraken', 'hyperliquid', 'zerodha', 'binance', etc.
  assetClass?: 'crypto' | 'stocks' | 'forex' | 'commodities'; // Optional classification
  initialBalance: number;
  currency?: string; // Default: 'USD'

  // Execution settings
  fees: {
    maker: number; // e.g., 0.0002 (0.02%)
    taker: number; // e.g., 0.0005 (0.05%)
  };

  slippage: {
    market: number; // e.g., 0.001 (0.1%)
    limit: number; // Usually 0
  };

  // Risk settings (optional)
  maxPositionSize?: number;
  maxLeverage?: number;
  defaultLeverage?: number; // Default: 1
  marginMode?: 'cross' | 'isolated'; // Default: 'cross'

  // Simulation settings (optional)
  simulatedLatency?: number; // ms delay (default: 0)
  partialFills?: boolean; // Enable partial order fills (default: false)
  enableMarginTrading?: boolean; // Enable leverage trading (default: false)

  // WebSocket settings
  enableRealtimeUpdates?: boolean; // Use real-time prices (default: true)
  priceUpdateInterval?: number; // Fallback polling interval in ms (default: 1000)
}

// ============================================================================
// PAPER TRADING STATE
// ============================================================================

export interface PaperTradingPortfolio {
  id: string;
  name: string;
  provider: string;
  initialBalance: number;
  currentBalance: number;
  currency: string;
  marginMode: 'cross' | 'isolated';
  leverage: number;
  createdAt: string;
  updatedAt: string;

  // Runtime state (not persisted)
  totalPnL?: number;
  unrealizedPnL?: number;
  realizedPnL?: number;
  totalValue?: number; // balance + unrealized PnL
  marginUsed?: number;
  marginAvailable?: number;
}

export interface PaperTradingPosition {
  id: string;
  portfolioId: string;
  symbol: string;
  side: 'long' | 'short';
  entryPrice: number;
  quantity: number;
  positionValue: number;
  currentPrice: number | null;
  unrealizedPnl: number | null;
  realizedPnl: number;
  leverage: number;
  marginMode: 'cross' | 'isolated';
  liquidationPrice: number | null;
  openedAt: string;
  closedAt: string | null;
  status: 'open' | 'closed';
}

// Paper trading order (compatible with CCXT Order)
export interface PaperTradingOrder {
  id: string;
  portfolioId: string;
  clientOrderId?: string;
  timestamp: number;
  datetime: string;
  lastTradeTimestamp?: number;
  symbol: string;
  type: string;
  side: string;
  price?: number;
  amount: number;
  cost?: number;
  average?: number;
  filled: number;
  remaining: number;
  status: string;
  fee?: any;
  trades?: any[];
  info: any;
  // Paper trading specific
  stopPrice?: number;
  trailingAmount?: number | null;
  trailingPercent?: number | null;
  icebergQty?: number | null;
  triggerPrice?: number | null;
  timeInForce?: string;
  postOnly?: boolean;
  reduceOnly?: boolean;
  leverage?: number;
  marginMode?: 'cross' | 'isolated';
}

export interface PaperTradingTrade {
  id: string;
  portfolioId: string;
  orderId: string;
  symbol: string;
  side: OrderSide;
  price: number;
  quantity: number;
  fee: number;
  feeRate: number;
  isMaker: boolean;
  timestamp: string;
}

// ============================================================================
// ORDER MATCHING
// ============================================================================

export interface OrderMatchResult {
  success: boolean;
  order: PaperTradingOrder;
  trades: PaperTradingTrade[];
  position?: PaperTradingPosition;
  balance: number;
  error?: string;
}

export interface PriceSnapshot {
  symbol: string;
  bid: number;
  ask: number;
  last: number;
  timestamp: number;
}

// ============================================================================
// MARGIN & LIQUIDATION
// ============================================================================

export interface MarginRequirement {
  symbol: string;
  side: 'long' | 'short';
  quantity: number;
  price: number;
  leverage: number;
  marginMode: 'cross' | 'isolated';
  initialMargin: number;
  maintenanceMargin: number;
  liquidationPrice: number | null;
}

export interface LiquidationCheck {
  shouldLiquidate: boolean;
  position: PaperTradingPosition;
  currentPrice: number;
  liquidationPrice: number;
  marginRatio: number;
  reason?: string;
}

// ============================================================================
// STATISTICS
// ============================================================================

export interface PaperTradingStats {
  portfolioId: string;
  totalTrades: number;
  winningTrades: number;
  losingTrades: number;
  winRate: number;
  totalPnL: number;
  realizedPnL: number;
  unrealizedPnL: number;
  totalFees: number;
  averageWin: number;
  averageLoss: number;
  largestWin: number;
  largestLoss: number;
  profitFactor: number;
  sharpeRatio: number | null;
  maxDrawdown: number | null;
  avgHoldingPeriod: number | null; // in minutes
}

// ============================================================================
// DATABASE TYPES
// ============================================================================

export interface DBPortfolio {
  id: string;
  name: string;
  provider: string;
  initial_balance: number;
  current_balance: number;
  currency: string;
  margin_mode: 'cross' | 'isolated';
  leverage: number;
  created_at: string;
  updated_at: string;
}

export interface DBPosition {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: 'long' | 'short';
  entry_price: number;
  quantity: number;
  position_value: number;
  current_price: number | null;
  unrealized_pnl: number | null;
  realized_pnl: number;
  leverage: number;
  margin_mode: 'cross' | 'isolated';
  liquidation_price: number | null;
  opened_at: string;
  closed_at: string | null;
  status: 'open' | 'closed';
}

export interface DBOrder {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: 'buy' | 'sell';
  type: string;
  quantity: number;
  price: number | null;
  stop_price: number | null;
  filled_quantity: number;
  avg_fill_price: number | null;
  status: string;
  time_in_force: string | null;
  post_only: number; // SQLite boolean (0/1)
  reduce_only: number;
  trailing_percent: number | null;
  trailing_amount: number | null;
  iceberg_qty: number | null;
  leverage: number | null;
  margin_mode: string | null;
  created_at: string;
  filled_at: string | null;
  updated_at: string;
}

export interface DBTrade {
  id: string;
  portfolio_id: string;
  order_id: string;
  symbol: string;
  side: 'buy' | 'sell';
  price: number;
  quantity: number;
  fee: number;
  fee_rate: number;
  is_maker: number; // SQLite boolean (0/1)
  timestamp: string;
}
