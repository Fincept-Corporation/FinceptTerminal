/**
 * Paper Trading Database Operations
 *
 * Uses Rust SQLite backend via Tauri invoke() commands
 * NO raw SQL - all operations go through Rust backend
 */

import { invoke } from '@tauri-apps/api/core';
import type {
  PaperTradingPortfolio,
  PaperTradingPosition,
  PaperTradingOrder,
  PaperTradingTrade,
  DBPortfolio,
  DBPosition,
  DBOrder,
  DBTrade,
} from './types';
import type { Order, OrderSide } from '../brokers/crypto/types';

export class PaperTradingDatabase {
  // ============================================================================
  // DATABASE HEALTH CHECK
  // ============================================================================

  /**
   * Check if database is accessible and initialized
   */
  async checkHealth(): Promise<boolean> {
    try {
      // Use dedicated health check command
      const result = await invoke<boolean>('db_check_health');

      if (result) {
        return true;
      } else {
        console.error('[PaperTradingDatabase] ✗ Database health check returned false');
        return false;
      }
    } catch (error) {
      console.error('[PaperTradingDatabase] ========================================');
      console.error('[PaperTradingDatabase] DATABASE HEALTH CHECK FAILED');
      console.error('[PaperTradingDatabase] ========================================');
      console.error('[PaperTradingDatabase] Error details:', error);
      console.error('[PaperTradingDatabase] This usually means:');
      console.error('[PaperTradingDatabase] 1. Database failed to initialize on app startup');
      console.error('[PaperTradingDatabase] 2. Missing write permissions to app data directory');
      console.error('[PaperTradingDatabase] 3. Rust backend is not running or commands not registered');
      console.error('[PaperTradingDatabase] ========================================');
      return false;
    }
  }

  // ============================================================================
  // PORTFOLIO OPERATIONS
  // ============================================================================

  async createPortfolio(portfolio: {
    id: string;
    name: string;
    provider: string;
    initialBalance: number;
    currency?: string;
    marginMode?: 'cross' | 'isolated';
    leverage?: number;
  }): Promise<void> {
    await invoke('db_create_portfolio', {
      id: portfolio.id,
      name: portfolio.name,
      provider: portfolio.provider,
      initialBalance: portfolio.initialBalance,
      currency: portfolio.currency || 'USD',
      marginMode: portfolio.marginMode || 'cross',
      leverage: portfolio.leverage || 1,
    });
  }

  async getPortfolio(portfolioId: string): Promise<PaperTradingPortfolio | null> {
    try {
      const result = await invoke<any>('db_get_portfolio', { id: portfolioId });
      return this.mapDBPortfolio(result);
    } catch (error) {
      console.error('[PaperTradingDatabase] getPortfolio error:', error);
      return null;
    }
  }

  async updatePortfolioBalance(portfolioId: string, newBalance: number): Promise<void> {
    await invoke('db_update_portfolio_balance', {
      id: portfolioId,
      newBalance
    });
  }

  async deletePortfolio(portfolioId: string): Promise<void> {
    await invoke('db_delete_portfolio', { id: portfolioId });
  }

  async listPortfolios(): Promise<PaperTradingPortfolio[]> {
    const result = await invoke<any[]>('db_list_portfolios');
    return result.map((row: any) => this.mapDBPortfolio(row));
  }

  // ============================================================================
  // POSITION OPERATIONS
  // ============================================================================

  async createPosition(position: {
    id: string;
    portfolioId: string;
    symbol: string;
    side: 'long' | 'short';
    entryPrice: number;
    quantity: number;
    leverage?: number;
    marginMode?: 'cross' | 'isolated';
  }): Promise<void> {
    await invoke('db_create_position', {
      id: position.id,
      portfolioId: position.portfolioId,
      symbol: position.symbol,
      side: position.side,
      entryPrice: position.entryPrice,
      quantity: position.quantity,
      leverage: position.leverage || 1,
      marginMode: position.marginMode || 'cross',
    });
  }

  async getPosition(positionId: string): Promise<PaperTradingPosition | null> {
    try {
      const result = await invoke<any>('db_get_position', { id: positionId });
      return this.mapDBPosition(result);
    } catch (error) {
      console.error('[PaperTradingDatabase] getPosition error:', error);
      return null;
    }
  }

  async getPositionBySymbol(portfolioId: string, symbol: string, status: 'open' | 'closed' = 'open'): Promise<PaperTradingPosition | null> {
    try {
      const result = await invoke<any | null>('db_get_position_by_symbol', {
        portfolioId,
        symbol,
        status,
      });
      return result ? this.mapDBPosition(result) : null;
    } catch (error) {
      console.error('[PaperTradingDatabase] getPositionBySymbol error:', error);
      return null;
    }
  }

  async getPositionBySymbolAndSide(
    portfolioId: string,
    symbol: string,
    side: 'long' | 'short',
    status: 'open' | 'closed' = 'open'
  ): Promise<PaperTradingPosition | null> {
    try {
      const result = await invoke<any | null>('db_get_position_by_symbol_and_side', {
        portfolioId,
        symbol,
        side,
        status,
      });
      return result ? this.mapDBPosition(result) : null;
    } catch (error) {
      console.error('[PaperTradingDatabase] getPositionBySymbolAndSide error:', error);
      return null;
    }
  }

  async getPortfolioPositions(portfolioId: string, status?: 'open' | 'closed'): Promise<PaperTradingPosition[]> {
    const result = await invoke<any[]>('db_get_portfolio_positions', {
      portfolioId,
      status: status || null,
    });
    return result.map((row: any) => this.mapDBPosition(row));
  }

  async updatePosition(positionId: string, updates: {
    quantity?: number;
    entryPrice?: number;
    currentPrice?: number;
    unrealizedPnl?: number;
    realizedPnl?: number;
    liquidationPrice?: number | null;
    status?: 'open' | 'closed';
    closedAt?: string | null;
  }): Promise<void> {
    await invoke('db_update_position', {
      id: positionId,
      quantity: updates.quantity || null,
      entryPrice: updates.entryPrice || null,
      currentPrice: updates.currentPrice || null,
      unrealizedPnl: updates.unrealizedPnl || null,
      realizedPnl: updates.realizedPnl || null,
      liquidationPrice: updates.liquidationPrice !== undefined ? updates.liquidationPrice : null,
      status: updates.status || null,
      closedAt: updates.closedAt !== undefined ? updates.closedAt : null,
    });
  }

  async deletePosition(positionId: string): Promise<void> {
    await invoke('db_delete_position', { id: positionId });
  }

  // ============================================================================
  // ORDER OPERATIONS
  // ============================================================================

  async createOrder(order: {
    id: string;
    portfolioId: string;
    symbol: string;
    side: OrderSide;
    type: string;
    quantity: number;
    price?: number | null;
    stopPrice?: number | null;
    timeInForce?: string;
    postOnly?: boolean;
    reduceOnly?: boolean;
    trailingPercent?: number | null;
    trailingAmount?: number | null;
    icebergQty?: number | null;
    leverage?: number;
    marginMode?: string;
  }): Promise<void> {
    await invoke('db_create_order', {
      id: order.id,
      portfolioId: order.portfolioId,
      symbol: order.symbol,
      side: order.side,
      orderType: order.type,
      quantity: order.quantity,
      price: order.price || null,
      timeInForce: order.timeInForce || 'GTC',
    });
  }

  async getOrder(orderId: string): Promise<PaperTradingOrder | null> {
    try {
      const result = await invoke<any>('db_get_order', { id: orderId });
      return this.mapDBOrder(result);
    } catch (error) {
      console.error('[PaperTradingDatabase] getOrder error:', error);
      return null;
    }
  }

  async getPortfolioOrders(portfolioId: string, status?: string): Promise<PaperTradingOrder[]> {
    const result = await invoke<any[]>('db_get_portfolio_orders', {
      portfolioId,
      status: status || null,
    });
    return result.map(row => this.mapDBOrder(row));
  }

  async getPendingOrders(portfolioId?: string): Promise<PaperTradingOrder[]> {
    const result = await invoke<any[]>('db_get_pending_orders', {
      portfolioId: portfolioId || null,
    });
    return result.map(row => this.mapDBOrder(row));
  }

  async updateOrder(orderId: string, updates: {
    filledQuantity?: number;
    avgFillPrice?: number;
    status?: string;
    filledAt?: string | null;
    stopPrice?: number;
  }): Promise<void> {
    await invoke('db_update_order', {
      id: orderId,
      filledQuantity: updates.filledQuantity || null,
      avgFillPrice: updates.avgFillPrice || null,
      status: updates.status || null,
      filledAt: updates.filledAt !== undefined ? updates.filledAt : null,
      stopPrice: updates.stopPrice || null,
    });
  }

  async deleteOrder(orderId: string): Promise<void> {
    await invoke('db_delete_order', { id: orderId });
  }

  // ============================================================================
  // TRADE OPERATIONS
  // ============================================================================

  async createTrade(trade: {
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
  }): Promise<void> {
    await invoke('db_create_trade', {
      id: trade.id,
      portfolioId: trade.portfolioId,
      orderId: trade.orderId,
      symbol: trade.symbol,
      side: trade.side,
      price: trade.price,
      quantity: trade.quantity,
      fee: trade.fee,
      feeRate: trade.feeRate,
      isMaker: trade.isMaker,
    });
  }

  async getTrade(tradeId: string): Promise<PaperTradingTrade | null> {
    try {
      const result = await invoke<any>('db_get_trade', { id: tradeId });
      return this.mapDBTrade(result);
    } catch (error) {
      console.error('[PaperTradingDatabase] getTrade error:', error);
      return null;
    }
  }

  async getPortfolioTrades(portfolioId: string, limit?: number): Promise<PaperTradingTrade[]> {
    const result = await invoke<any[]>('db_get_portfolio_trades', {
      portfolioId,
      limit: limit || null,
    });
    return result.map(row => this.mapDBTrade(row));
  }

  async getOrderTrades(orderId: string): Promise<PaperTradingTrade[]> {
    const result = await invoke<any[]>('db_get_order_trades', { orderId });
    return result.map(row => this.mapDBTrade(row));
  }

  async deleteTrade(tradeId: string): Promise<void> {
    await invoke('db_delete_trade', { id: tradeId });
  }

  // ============================================================================
  // MAPPING FUNCTIONS
  // ============================================================================

  private mapDBPortfolio(row: any): PaperTradingPortfolio {
    return {
      id: row.id,
      name: row.name,
      provider: row.provider,
      initialBalance: row.initial_balance,
      currentBalance: row.current_balance,
      currency: row.currency || 'USD',
      marginMode: row.margin_mode || 'cross',
      leverage: row.leverage || 1,
      createdAt: row.created_at,
      updatedAt: row.updated_at,
    };
  }

  private mapDBPosition(row: any): PaperTradingPosition {
    return {
      id: row.id,
      portfolioId: row.portfolio_id,
      symbol: row.symbol,
      side: row.side,
      entryPrice: row.entry_price,
      quantity: row.quantity,
      positionValue: row.position_value,
      currentPrice: row.current_price,
      unrealizedPnl: row.unrealized_pnl,
      realizedPnl: row.realized_pnl,
      leverage: row.leverage,
      marginMode: row.margin_mode,
      liquidationPrice: row.liquidation_price,
      openedAt: row.opened_at,
      closedAt: row.closed_at,
      status: row.status,
    };
  }

  private mapDBOrder(row: any): PaperTradingOrder {
    return {
      id: row.id,
      portfolioId: row.portfolio_id,
      symbol: row.symbol,
      side: row.side,
      type: row.order_type as any,
      amount: row.quantity,
      price: row.price || undefined,
      stopPrice: row.stop_price || undefined,
      filled: row.filled_quantity,
      average: row.avg_fill_price || undefined,
      status: row.status as any,
      timestamp: new Date(row.created_at).getTime(),
      datetime: row.created_at,
      timeInForce: row.time_in_force || undefined,
      postOnly: row.post_only === true || row.post_only === 1,
      reduceOnly: row.reduce_only === true || row.reduce_only === 1,
      trailingPercent: undefined,
      trailingAmount: undefined,
      icebergQty: undefined,
      leverage: undefined,
      marginMode: undefined,
      lastTradeTimestamp: row.filled_at ? new Date(row.filled_at).getTime() : undefined,
      remaining: row.quantity - row.filled_quantity,
      cost: (row.avg_fill_price || 0) * row.filled_quantity,
      info: {}, // Raw exchange response (not applicable for paper trading)
    } as PaperTradingOrder;
  }

  private mapDBTrade(row: any): PaperTradingTrade {
    return {
      id: row.id,
      portfolioId: row.portfolio_id,
      orderId: row.order_id,
      symbol: row.symbol,
      side: row.side,
      price: row.price,
      quantity: row.quantity,
      fee: row.fee,
      feeRate: row.fee_rate,
      isMaker: row.is_maker === true || row.is_maker === 1,
      timestamp: row.timestamp,
    };
  }
}

// Singleton instance
export const paperTradingDatabase = new PaperTradingDatabase();
