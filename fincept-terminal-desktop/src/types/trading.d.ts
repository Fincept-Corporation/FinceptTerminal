// File: src/types/trading.d.ts
// Trading and paper trading type definitions

export interface VirtualPortfolio {
  id: string;
  name: string;
  provider: string;
  initialBalance: number;
  balance: number;
  totalPnL: number;
  positions: Position[];
  createdAt: string;
  updatedAt: string;
}

export interface Position {
  id: string;
  portfolioId: string;
  symbol: string;
  side: 'long' | 'short';
  entryPrice: number;
  quantity: number;
  currentPrice?: number;
  unrealizedPnL?: number;
  realizedPnL: number;
  openedAt: string;
  closedAt?: string;
  status: 'open' | 'closed';
}

export type OrderSide = 'buy' | 'sell';
export type OrderType = 'market' | 'limit' | 'stop_market' | 'stop_limit';
export type OrderStatus = 'pending' | 'filled' | 'partial' | 'cancelled' | 'rejected' | 'triggered';

export interface OrderRequest {
  symbol: string;
  side: OrderSide;
  type: OrderType;
  quantity: number;
  price?: number;       // Required for limit and stop_limit
  stopPrice?: number;   // Required for stop_market and stop_limit
}

export interface Order {
  id: string;
  portfolioId: string;
  symbol: string;
  side: OrderSide;
  type: OrderType;
  quantity: number;
  price?: number;
  stopPrice?: number;
  filledQuantity: number;
  avgFillPrice?: number;
  status: OrderStatus;
  createdAt: string;
  filledAt?: string;
}

export interface Trade {
  id: string;
  portfolioId: string;
  orderId: string;
  symbol: string;
  side: OrderSide;
  price: number;
  quantity: number;
  fee: number;
  timestamp: string;
}

export interface OrderResult {
  success: boolean;
  orderId?: string;
  error?: string;
}

export interface PaperTradingConfig {
  portfolioId: string;
  feeRate: number;      // Default: 0.0002 (0.02%)
  slippage: number;     // Default: 0.0001 (0.01%)
}

export interface MarketPrice {
  symbol: string;
  price: number;
  timestamp: number;
}

/**
 * Database row types (matching SQLite schema)
 */
export interface DbPortfolio {
  id: string;
  name: string;
  provider: string;
  initial_balance: number;
  current_balance: number;
  created_at: string;
  updated_at: string;
}

export interface DbPosition {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: 'long' | 'short';
  entry_price: number;
  quantity: number;
  current_price: number | null;
  unrealized_pnl: number | null;
  realized_pnl: number;
  opened_at: string;
  closed_at: string | null;
  status: 'open' | 'closed';
}

export interface DbOrder {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: OrderSide;
  type: OrderType;
  quantity: number;
  price: number | null;
  stop_price: number | null;
  filled_quantity: number;
  avg_fill_price: number | null;
  status: OrderStatus;
  created_at: string;
  filled_at: string | null;
}

export interface DbTrade {
  id: string;
  portfolio_id: string;
  order_id: string;
  symbol: string;
  side: OrderSide;
  price: number;
  quantity: number;
  fee: number;
  timestamp: string;
}
