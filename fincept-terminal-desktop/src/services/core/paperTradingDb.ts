/**
 * SQLite Service — Paper Trading Database Operations
 *
 * Types and Tauri invoke wrappers for the paper trading engine,
 * extracted from sqliteService.ts.
 */

import { invoke } from '@tauri-apps/api/core';

// ==================== PAPER TRADING ====================

// Portfolio Types
export interface PaperTradingPortfolio {
  id: string;
  name: string;
  provider: string;
  initial_balance: number;
  current_balance: number;
  currency: string;
  margin_mode: string;
  leverage: number;
  created_at: string;
  updated_at: string;
}

export interface PaperTradingPosition {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: 'long' | 'short';
  entry_price: number;
  quantity: number;
  position_value?: number;
  current_price?: number;
  unrealized_pnl?: number;
  realized_pnl: number;
  leverage: number;
  margin_mode: string;
  liquidation_price?: number;
  opened_at: string;
  closed_at?: string;
  status: 'open' | 'closed';
}

export interface PaperTradingOrder {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: 'buy' | 'sell';
  order_type: 'market' | 'limit' | 'stop_market' | 'stop_limit';
  quantity: number;
  price?: number;
  stop_price?: number;
  filled_quantity: number;
  avg_fill_price?: number;
  status: 'pending' | 'filled' | 'partial' | 'cancelled' | 'rejected' | 'triggered';
  time_in_force: string;
  post_only: boolean;
  reduce_only: boolean;
  created_at: string;
  filled_at?: string;
  updated_at: string;
}

export interface PaperTradingTrade {
  id: string;
  portfolio_id: string;
  order_id: string;
  symbol: string;
  side: string;
  price: number;
  quantity: number;
  fee: number;
  fee_rate: number;
  is_maker: boolean;
  timestamp: string;
}

export interface MarginBlock {
  id: string;
  portfolio_id: string;
  order_id: string;
  symbol: string;
  blocked_amount: number;
  created_at: string;
}

export interface PaperTradingHolding {
  id: string;
  portfolio_id: string;
  symbol: string;
  quantity: number;
  average_price: number;
  invested_value: number;
  current_price: number;
  current_value: number;
  pnl: number;
  pnl_percent: number;
  t1_quantity: number;
  available_quantity: number;
  created_at: string;
  updated_at: string;
}

export interface PortfolioStats {
  total_value: number;
  available_margin: number;
  blocked_margin: number;
  realized_pnl: number;
  unrealized_pnl: number;
  total_pnl: number;
  open_positions: number;
  total_trades: number;
}

// Portfolio Operations
export const createPortfolio = async (id: string, name: string, provider: string, initialBalance: number, currency: string, marginMode: string, leverage: number): Promise<PaperTradingPortfolio> => {
  return await invoke('db_create_portfolio', { id, name, provider, initialBalance, currency, marginMode, leverage });
};

export const getPortfolio = async (id: string): Promise<PaperTradingPortfolio> => {
  return await invoke('db_get_portfolio', { id });
};

export const listPortfolios = async (): Promise<PaperTradingPortfolio[]> => {
  return await invoke<PaperTradingPortfolio[]>('db_list_portfolios');
};

export const updatePortfolioBalance = async (id: string, newBalance: number): Promise<void> => {
  await invoke('db_update_portfolio_balance', { id, newBalance });
};

export const deletePortfolio = async (id: string): Promise<void> => {
  await invoke('db_delete_portfolio', { id });
};

// Position Operations
export const createPosition = async (
  id: string,
  portfolioId: string,
  symbol: string,
  side: string,
  entryPrice: number,
  quantity: number,
  leverage: number,
  marginMode: string
): Promise<void> => {
  await invoke('db_create_position', { id, portfolioId, symbol, side, entryPrice, quantity, leverage, marginMode });
};

export const getPosition = async (id: string): Promise<PaperTradingPosition> => {
  return await invoke('db_get_position', { id });
};

export const getPositionBySymbol = async (portfolioId: string, symbol: string, status: string): Promise<PaperTradingPosition | null> => {
  return await invoke('db_get_position_by_symbol', { portfolioId, symbol, status });
};

export const getPositionBySymbolAndSide = async (portfolioId: string, symbol: string, side: string, status: string): Promise<PaperTradingPosition | null> => {
  return await invoke('db_get_position_by_symbol_and_side', { portfolioId, symbol, side, status });
};

export const getPortfolioPositions = async (portfolioId: string, status?: string): Promise<PaperTradingPosition[]> => {
  return await invoke<PaperTradingPosition[]>('db_get_portfolio_positions', { portfolioId, status });
};

export const updatePosition = async (
  id: string,
  quantity?: number,
  entryPrice?: number,
  currentPrice?: number,
  unrealizedPnl?: number,
  realizedPnl?: number,
  liquidationPrice?: number,
  status?: string,
  closedAt?: string
): Promise<void> => {
  await invoke('db_update_position', { id, quantity, entryPrice, currentPrice, unrealizedPnl, realizedPnl, liquidationPrice, status, closedAt });
};

export const deletePosition = async (id: string): Promise<void> => {
  await invoke('db_delete_position', { id });
};

// Order Operations
export const createOrder = async (
  id: string,
  portfolioId: string,
  symbol: string,
  side: string,
  orderType: string,
  quantity: number,
  price: number | null,
  timeInForce: string
): Promise<void> => {
  await invoke('db_create_order', { id, portfolioId, symbol, side, orderType, quantity, price, timeInForce });
};

export const getOrder = async (id: string): Promise<PaperTradingOrder> => {
  return await invoke('db_get_order', { id });
};

export const getPendingOrders = async (portfolioId?: string): Promise<PaperTradingOrder[]> => {
  return await invoke<PaperTradingOrder[]>('db_get_pending_orders', { portfolioId });
};

export const getPortfolioOrders = async (portfolioId: string, status?: string): Promise<PaperTradingOrder[]> => {
  return await invoke<PaperTradingOrder[]>('db_get_portfolio_orders', { portfolioId, status });
};

export const updateOrder = async (
  id: string,
  filledQuantity?: number,
  avgFillPrice?: number,
  status?: string,
  filledAt?: string
): Promise<void> => {
  await invoke('db_update_order', { id, filledQuantity, avgFillPrice, status, filledAt });
};

export const deleteOrder = async (id: string): Promise<void> => {
  await invoke('db_delete_order', { id });
};

// Trade Operations
export const createTrade = async (
  id: string,
  portfolioId: string,
  orderId: string,
  symbol: string,
  side: string,
  price: number,
  quantity: number,
  fee: number,
  feeRate: number,
  isMaker: boolean
): Promise<void> => {
  await invoke('db_create_trade', { id, portfolioId, orderId, symbol, side, price, quantity, fee, feeRate, isMaker });
};

export const getTrade = async (id: string): Promise<PaperTradingTrade> => {
  return await invoke('db_get_trade', { id });
};

export const getOrderTrades = async (orderId: string): Promise<PaperTradingTrade[]> => {
  return await invoke<PaperTradingTrade[]>('db_get_order_trades', { orderId });
};

export const getPortfolioTrades = async (portfolioId: string, limit?: number): Promise<PaperTradingTrade[]> => {
  return await invoke<PaperTradingTrade[]>('db_get_portfolio_trades', { portfolioId, limit });
};

export const deleteTrade = async (id: string): Promise<void> => {
  await invoke('db_delete_trade', { id });
};

// Margin Block Operations
export const createMarginBlock = async (
  id: string,
  portfolioId: string,
  orderId: string,
  symbol: string,
  blockedAmount: number
): Promise<void> => {
  await invoke('db_create_margin_block', { id, portfolioId, orderId, symbol, blockedAmount });
};

export const getMarginBlocks = async (portfolioId: string): Promise<MarginBlock[]> => {
  return await invoke<MarginBlock[]>('db_get_margin_blocks', { portfolioId });
};

export const getMarginBlockByOrder = async (orderId: string): Promise<MarginBlock | null> => {
  return await invoke('db_get_margin_block_by_order', { orderId });
};

export const deleteMarginBlock = async (orderId: string): Promise<number> => {
  return await invoke<number>('db_delete_margin_block', { orderId });
};

export const getTotalBlockedMargin = async (portfolioId: string): Promise<number> => {
  return await invoke<number>('db_get_total_blocked_margin', { portfolioId });
};

export const getAvailableMargin = async (portfolioId: string): Promise<number> => {
  return await invoke<number>('db_get_available_margin', { portfolioId });
};

// Holdings Operations (T+1 Settlement)
export const createHolding = async (
  id: string,
  portfolioId: string,
  symbol: string,
  quantity: number,
  averagePrice: number
): Promise<void> => {
  await invoke('db_create_holding', { id, portfolioId, symbol, quantity, averagePrice });
};

export const getHoldings = async (portfolioId: string): Promise<PaperTradingHolding[]> => {
  return await invoke<PaperTradingHolding[]>('db_get_holdings', { portfolioId });
};

export const getHoldingBySymbol = async (portfolioId: string, symbol: string): Promise<PaperTradingHolding | null> => {
  return await invoke('db_get_holding_by_symbol', { portfolioId, symbol });
};

export const updateHolding = async (
  id: string,
  quantity?: number,
  averagePrice?: number,
  currentPrice?: number,
  t1Quantity?: number,
  availableQuantity?: number
): Promise<void> => {
  await invoke('db_update_holding', { id, quantity, averagePrice, currentPrice, t1Quantity, availableQuantity });
};

export const deleteHolding = async (id: string): Promise<void> => {
  await invoke('db_delete_holding', { id });
};

export const processT1Settlement = async (portfolioId: string): Promise<number> => {
  return await invoke<number>('db_process_t1_settlement', { portfolioId });
};

// Execution Engine Operations
export const fillOrder = async (
  orderId: string,
  fillPrice: number,
  fillQuantity: number,
  fee: number,
  feeRate: number
): Promise<string> => {
  return await invoke<string>('db_fill_order', { orderId, fillPrice, fillQuantity, fee, feeRate });
};

export const getPortfolioStats = async (portfolioId: string): Promise<PortfolioStats> => {
  return await invoke<PortfolioStats>('db_get_portfolio_stats', { portfolioId });
};

export const resetPortfolio = async (portfolioId: string, initialBalance: number): Promise<void> => {
  await invoke('db_reset_portfolio', { portfolioId, initialBalance });
};
