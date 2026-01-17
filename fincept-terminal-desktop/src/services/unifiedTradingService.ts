/**
 * Unified Trading Service
 *
 * Handles both LIVE and PAPER trading modes for all stock brokers.
 * Provides a unified API that routes to either real broker or paper trading engine.
 */

import { invoke } from '@tauri-apps/api/core';

// ============================================================================
// Types
// ============================================================================

export type TradingMode = 'live' | 'paper';

export interface TradingSession {
  broker: string;
  mode: TradingMode;
  paper_portfolio_id: string | null;
  is_connected: boolean;
}

export interface UnifiedOrder {
  symbol: string;
  exchange: string;
  side: 'buy' | 'sell';
  order_type: 'market' | 'limit' | 'stop' | 'stop_limit';
  quantity: number;
  price?: number;
  stop_price?: number;
  product_type?: 'CNC' | 'MIS' | 'NRML';
}

export interface UnifiedOrderResponse {
  success: boolean;
  order_id?: string;
  message?: string;
  mode: TradingMode;
}

export interface UnifiedPosition {
  symbol: string;
  exchange: string;
  side: 'long' | 'short';
  quantity: number;
  entry_price: number;
  current_price: number;
  unrealized_pnl: number;
  realized_pnl: number;
  mode: TradingMode;
}

export interface PaperOrder {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: string;
  order_type: string;
  quantity: number;
  price: number | null;
  stop_price: number | null;
  filled_qty: number;
  avg_price: number | null;
  status: 'pending' | 'filled' | 'partial' | 'cancelled';
  reduce_only: boolean;
  created_at: string;
  filled_at: string | null;
}

export interface PaperPortfolio {
  id: string;
  name: string;
  initial_balance: number;
  balance: number;
  currency: string;
  leverage: number;
  margin_mode: string;
  fee_rate: number;
  created_at: string;
}

export interface PaperStats {
  total_pnl: number;
  win_rate: number;
  total_trades: number;
  winning_trades: number;
  losing_trades: number;
  largest_win: number;
  largest_loss: number;
}

// ============================================================================
// Session Management
// ============================================================================

/**
 * Initialize a trading session for a broker
 */
export async function initTradingSession(
  broker: string,
  mode: TradingMode,
  paperPortfolioId?: string
): Promise<TradingSession> {
  return invoke<TradingSession>('init_trading_session', {
    broker,
    mode,
    paperPortfolioId,
  });
}

/**
 * Get current trading session
 */
export async function getTradingSession(): Promise<TradingSession | null> {
  return invoke<TradingSession | null>('get_trading_session');
}

/**
 * Switch between live and paper trading modes
 */
export async function switchTradingMode(mode: TradingMode): Promise<TradingSession> {
  return invoke<TradingSession>('switch_trading_mode', { mode });
}

// ============================================================================
// Order Management
// ============================================================================

/**
 * Place an order (routes to live broker or paper trading based on mode)
 */
export async function placeOrder(order: UnifiedOrder): Promise<UnifiedOrderResponse> {
  return invoke<UnifiedOrderResponse>('unified_place_order', { order });
}

/**
 * Get positions (paper or live based on mode)
 */
export async function getPositions(): Promise<UnifiedPosition[]> {
  return invoke<UnifiedPosition[]>('unified_get_positions');
}

/**
 * Get orders (paper or live based on mode)
 */
export async function getOrders(status?: string): Promise<PaperOrder[]> {
  return invoke<PaperOrder[]>('unified_get_orders', { status });
}

/**
 * Cancel an order
 */
export async function cancelOrder(orderId: string): Promise<void> {
  return invoke<void>('unified_cancel_order', { orderId });
}

// ============================================================================
// Paper Trading Specific
// ============================================================================

/**
 * Create a paper trading portfolio
 */
export async function createPaperPortfolio(
  name: string,
  balance: number,
  currency = 'INR',
  leverage = 1.0,
  marginMode = 'cross',
  feeRate = 0.0003
): Promise<PaperPortfolio> {
  return invoke<PaperPortfolio>('pt_create_portfolio', {
    name,
    balance,
    currency,
    leverage,
    marginMode,
    feeRate,
  });
}

/**
 * List all paper trading portfolios
 */
export async function listPaperPortfolios(): Promise<PaperPortfolio[]> {
  return invoke<PaperPortfolio[]>('pt_list_portfolios');
}

/**
 * Get paper trading statistics
 */
export async function getPaperStats(portfolioId: string): Promise<PaperStats> {
  return invoke<PaperStats>('pt_get_stats', { portfolioId });
}

/**
 * Get paper portfolio balance/funds info
 */
export async function getPaperFunds(): Promise<{
  totalBalance: number;
  availableCash: number;
  usedMargin: number;
  availableMargin: number;
  currency: string;
} | null> {
  try {
    const portfolios = await listPaperPortfolios();
    if (portfolios.length === 0) {
      // Create a default paper portfolio if none exists
      const defaultPortfolio = await createPaperPortfolio('Default Paper Portfolio', 1000000, 'INR');
      return {
        totalBalance: defaultPortfolio.balance,
        availableCash: defaultPortfolio.balance,
        usedMargin: 0,
        availableMargin: defaultPortfolio.balance,
        currency: defaultPortfolio.currency,
      };
    }

    // Use the first portfolio
    const portfolio = portfolios[0];
    const positions = await getPositions();

    // Calculate used margin from open positions
    const usedMargin = positions.reduce((acc, p) => acc + (p.entry_price * p.quantity), 0);

    return {
      totalBalance: portfolio.initial_balance,
      availableCash: portfolio.balance,
      usedMargin: usedMargin,
      availableMargin: portfolio.balance,
      currency: portfolio.currency,
    };
  } catch (err) {
    console.error('[UnifiedTradingService] Failed to get paper funds:', err);
    return null;
  }
}

/**
 * Get paper trades (filled orders)
 */
export async function getPaperTrades(): Promise<PaperOrder[]> {
  try {
    const orders = await getOrders('filled');
    return orders;
  } catch (err) {
    console.error('[UnifiedTradingService] Failed to get paper trades:', err);
    return [];
  }
}

/**
 * Reset a paper trading portfolio
 */
export async function resetPaperPortfolio(portfolioId: string): Promise<PaperPortfolio> {
  return invoke<PaperPortfolio>('pt_reset_portfolio', { id: portfolioId });
}

/**
 * Delete a paper trading portfolio
 */
export async function deletePaperPortfolio(portfolioId: string): Promise<void> {
  return invoke<void>('pt_delete_portfolio', { id: portfolioId });
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Check if currently in paper trading mode
 */
export async function isPaperMode(): Promise<boolean> {
  const session = await getTradingSession();
  return session?.mode === 'paper';
}

/**
 * Quick order helpers
 */
export async function buyMarket(
  symbol: string,
  exchange: string,
  quantity: number,
  productType: 'CNC' | 'MIS' | 'NRML' = 'CNC'
): Promise<UnifiedOrderResponse> {
  return placeOrder({
    symbol,
    exchange,
    side: 'buy',
    order_type: 'market',
    quantity,
    product_type: productType,
  });
}

export async function sellMarket(
  symbol: string,
  exchange: string,
  quantity: number,
  productType: 'CNC' | 'MIS' | 'NRML' = 'CNC'
): Promise<UnifiedOrderResponse> {
  return placeOrder({
    symbol,
    exchange,
    side: 'sell',
    order_type: 'market',
    quantity,
    product_type: productType,
  });
}

export async function buyLimit(
  symbol: string,
  exchange: string,
  quantity: number,
  price: number,
  productType: 'CNC' | 'MIS' | 'NRML' = 'CNC'
): Promise<UnifiedOrderResponse> {
  return placeOrder({
    symbol,
    exchange,
    side: 'buy',
    order_type: 'limit',
    quantity,
    price,
    product_type: productType,
  });
}

export async function sellLimit(
  symbol: string,
  exchange: string,
  quantity: number,
  price: number,
  productType: 'CNC' | 'MIS' | 'NRML' = 'CNC'
): Promise<UnifiedOrderResponse> {
  return placeOrder({
    symbol,
    exchange,
    side: 'sell',
    order_type: 'limit',
    quantity,
    price,
    product_type: productType,
  });
}
