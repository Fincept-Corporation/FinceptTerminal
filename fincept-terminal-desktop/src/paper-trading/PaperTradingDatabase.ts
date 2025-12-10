/**
 * Paper Trading Database Operations
 *
 * Thin wrapper over SQLite service for paper trading data persistence
 * Uses existing paper_trading_* tables from sqliteService
 */

import { sqliteService } from '../services/sqliteService';
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
    const sql = `
      INSERT INTO paper_trading_portfolios
      (id, name, provider, initial_balance, current_balance, currency, margin_mode, leverage)
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
    `;

    await sqliteService.execute(sql, [
      portfolio.id,
      portfolio.name,
      portfolio.provider,
      portfolio.initialBalance,
      portfolio.initialBalance,
      portfolio.currency || 'USD',
      portfolio.marginMode || 'cross',
      portfolio.leverage || 1,
    ]);
  }

  async getPortfolio(portfolioId: string): Promise<PaperTradingPortfolio | null> {
    const sql = `
      SELECT id, name, provider, initial_balance, current_balance, currency,
             margin_mode, leverage, created_at, updated_at
      FROM paper_trading_portfolios
      WHERE id = $1
    `;

    const result = await sqliteService.select<DBPortfolio[]>(sql, [portfolioId]);

    if (!result || result.length === 0) {
      return null;
    }

    const row = result[0];
    return this.mapDBPortfolio(row);
  }

  async updatePortfolioBalance(portfolioId: string, newBalance: number): Promise<void> {
    const sql = `
      UPDATE paper_trading_portfolios
      SET current_balance = $1, updated_at = CURRENT_TIMESTAMP
      WHERE id = $2
    `;

    await sqliteService.execute(sql, [newBalance, portfolioId]);
  }

  async deletePortfolio(portfolioId: string): Promise<void> {
    // Cascading deletes will remove positions, orders, and trades
    const sql = `DELETE FROM paper_trading_portfolios WHERE id = $1`;
    await sqliteService.execute(sql, [portfolioId]);
  }

  async listPortfolios(): Promise<PaperTradingPortfolio[]> {
    const sql = `
      SELECT id, name, provider, initial_balance, current_balance, currency,
             margin_mode, leverage, created_at, updated_at
      FROM paper_trading_portfolios
      ORDER BY created_at DESC
    `;

    const result = await sqliteService.select<DBPortfolio[]>(sql, []);
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
    const positionValue = position.entryPrice * position.quantity;

    const sql = `
      INSERT INTO paper_trading_positions
      (id, portfolio_id, symbol, side, entry_price, quantity, position_value, leverage, margin_mode, status)
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, 'open')
    `;

    await sqliteService.execute(sql, [
      position.id,
      position.portfolioId,
      position.symbol,
      position.side,
      position.entryPrice,
      position.quantity,
      positionValue,
      position.leverage || 1,
      position.marginMode || 'cross',
    ]);
  }

  async getPosition(positionId: string): Promise<PaperTradingPosition | null> {
    const sql = `
      SELECT id, portfolio_id, symbol, side, entry_price, quantity, position_value,
             current_price, unrealized_pnl, realized_pnl, leverage, margin_mode,
             liquidation_price, opened_at, closed_at, status
      FROM paper_trading_positions
      WHERE id = $1
    `;

    const result = await sqliteService.select<DBPosition[]>(sql, [positionId]);

    if (!result || result.length === 0) {
      return null;
    }

    return this.mapDBPosition(result[0]);
  }

  async getPositionBySymbol(portfolioId: string, symbol: string, status: 'open' | 'closed' = 'open'): Promise<PaperTradingPosition | null> {
    const sql = `
      SELECT id, portfolio_id, symbol, side, entry_price, quantity, position_value,
             current_price, unrealized_pnl, realized_pnl, leverage, margin_mode,
             liquidation_price, opened_at, closed_at, status
      FROM paper_trading_positions
      WHERE portfolio_id = $1 AND symbol = $2 AND status = $3
      ORDER BY opened_at DESC
      LIMIT 1
    `;

    const result = await sqliteService.select<DBPosition[]>(sql, [portfolioId, symbol, status]);

    if (!result || result.length === 0) {
      return null;
    }

    return this.mapDBPosition(result[0]);
  }

  async getPositionBySymbolAndSide(
    portfolioId: string,
    symbol: string,
    side: 'long' | 'short',
    status: 'open' | 'closed' = 'open'
  ): Promise<PaperTradingPosition | null> {
    const sql = `
      SELECT id, portfolio_id, symbol, side, entry_price, quantity, position_value,
             current_price, unrealized_pnl, realized_pnl, leverage, margin_mode,
             liquidation_price, opened_at, closed_at, status
      FROM paper_trading_positions
      WHERE portfolio_id = $1 AND symbol = $2 AND side = $3 AND status = $4
      ORDER BY opened_at DESC
      LIMIT 1
    `;

    const result = await sqliteService.select<DBPosition[]>(sql, [portfolioId, symbol, side, status]);

    if (!result || result.length === 0) {
      return null;
    }

    return this.mapDBPosition(result[0]);
  }

  async getPortfolioPositions(portfolioId: string, status?: 'open' | 'closed'): Promise<PaperTradingPosition[]> {
    let sql = `
      SELECT id, portfolio_id, symbol, side, entry_price, quantity, position_value,
             current_price, unrealized_pnl, realized_pnl, leverage, margin_mode,
             liquidation_price, opened_at, closed_at, status
      FROM paper_trading_positions
      WHERE portfolio_id = $1
    `;

    const params: any[] = [portfolioId];

    if (status) {
      sql += ` AND status = $2`;
      params.push(status);
    }

    sql += ` ORDER BY opened_at DESC`;

    const result = await sqliteService.select<DBPosition[]>(sql, params);
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
    const fields: string[] = [];
    const values: any[] = [];
    let paramIndex = 1;

    if (updates.quantity !== undefined) {
      fields.push(`quantity = $${paramIndex++}`);
      values.push(updates.quantity);
    }
    if (updates.entryPrice !== undefined) {
      fields.push(`entry_price = $${paramIndex++}`);
      values.push(updates.entryPrice);
    }
    if (updates.currentPrice !== undefined) {
      fields.push(`current_price = $${paramIndex++}`);
      values.push(updates.currentPrice);
    }
    if (updates.unrealizedPnl !== undefined) {
      fields.push(`unrealized_pnl = $${paramIndex++}`);
      values.push(updates.unrealizedPnl);
    }
    if (updates.realizedPnl !== undefined) {
      fields.push(`realized_pnl = $${paramIndex++}`);
      values.push(updates.realizedPnl);
    }
    if (updates.liquidationPrice !== undefined) {
      fields.push(`liquidation_price = $${paramIndex++}`);
      values.push(updates.liquidationPrice);
    }
    if (updates.status !== undefined) {
      fields.push(`status = $${paramIndex++}`);
      values.push(updates.status);
    }
    if (updates.closedAt !== undefined) {
      fields.push(`closed_at = $${paramIndex++}`);
      values.push(updates.closedAt);
    }

    if (fields.length === 0) return;

    values.push(positionId);
    const sql = `UPDATE paper_trading_positions SET ${fields.join(', ')} WHERE id = $${paramIndex}`;

    await sqliteService.execute(sql, values);
  }

  async deletePosition(positionId: string): Promise<void> {
    const sql = `DELETE FROM paper_trading_positions WHERE id = $1`;
    await sqliteService.execute(sql, [positionId]);
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
    const sql = `
      INSERT INTO paper_trading_orders
      (id, portfolio_id, symbol, side, type, quantity, price, stop_price, status,
       time_in_force, post_only, reduce_only, trailing_percent, trailing_amount,
       iceberg_qty, leverage, margin_mode, filled_quantity, created_at, updated_at)
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8, 'pending', $9, $10, $11, $12, $13, $14, $15, $16, 0, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
    `;

    await sqliteService.execute(sql, [
      order.id,
      order.portfolioId,
      order.symbol,
      order.side,
      order.type,
      order.quantity,
      order.price || null,
      order.stopPrice || null,
      order.timeInForce || 'GTC',
      order.postOnly ? 1 : 0,
      order.reduceOnly ? 1 : 0,
      order.trailingPercent || null,
      order.trailingAmount || null,
      order.icebergQty || null,
      order.leverage || null,
      order.marginMode || null,
    ]);
  }

  async getOrder(orderId: string): Promise<PaperTradingOrder | null> {
    const sql = `
      SELECT id, portfolio_id, symbol, side, type, quantity, price, stop_price,
             filled_quantity, avg_fill_price, status, time_in_force, post_only,
             reduce_only, trailing_percent, trailing_amount, iceberg_qty, leverage,
             margin_mode, created_at, filled_at, updated_at
      FROM paper_trading_orders
      WHERE id = $1
    `;

    const result = await sqliteService.select<DBOrder[]>(sql, [orderId]);

    if (!result || result.length === 0) {
      return null;
    }

    return this.mapDBOrder(result[0]);
  }

  async getPortfolioOrders(portfolioId: string, status?: string): Promise<PaperTradingOrder[]> {
    let sql = `
      SELECT id, portfolio_id, symbol, side, type, quantity, price, stop_price,
             filled_quantity, avg_fill_price, status, time_in_force, post_only,
             reduce_only, trailing_percent, trailing_amount, iceberg_qty, leverage,
             margin_mode, created_at, filled_at, updated_at
      FROM paper_trading_orders
      WHERE portfolio_id = $1
    `;

    const params: any[] = [portfolioId];

    if (status) {
      sql += ` AND status = $2`;
      params.push(status);
    }

    sql += ` ORDER BY created_at DESC`;

    const result = await sqliteService.select<DBOrder[]>(sql, params);
    return result.map(row => this.mapDBOrder(row));
  }

  async getPendingOrders(portfolioId?: string): Promise<PaperTradingOrder[]> {
    let sql = `
      SELECT id, portfolio_id, symbol, side, type, quantity, price, stop_price,
             filled_quantity, avg_fill_price, status, time_in_force, post_only,
             reduce_only, trailing_percent, trailing_amount, iceberg_qty, leverage,
             margin_mode, created_at, filled_at, updated_at
      FROM paper_trading_orders
      WHERE status IN ('pending', 'triggered', 'partial')
    `;

    const params: any[] = [];

    if (portfolioId) {
      sql += ` AND portfolio_id = $1`;
      params.push(portfolioId);
    }

    sql += ` ORDER BY created_at ASC`;

    const result = await sqliteService.select<DBOrder[]>(sql, params);
    return result.map(row => this.mapDBOrder(row));
  }

  async updateOrder(orderId: string, updates: {
    filledQuantity?: number;
    avgFillPrice?: number;
    status?: string;
    filledAt?: string | null;
  }): Promise<void> {
    const fields: string[] = ['updated_at = CURRENT_TIMESTAMP'];
    const values: any[] = [];
    let paramIndex = 1;

    if (updates.filledQuantity !== undefined) {
      fields.push(`filled_quantity = $${paramIndex++}`);
      values.push(updates.filledQuantity);
    }
    if (updates.avgFillPrice !== undefined) {
      fields.push(`avg_fill_price = $${paramIndex++}`);
      values.push(updates.avgFillPrice);
    }
    if (updates.status !== undefined) {
      fields.push(`status = $${paramIndex++}`);
      values.push(updates.status);
    }
    if (updates.filledAt !== undefined) {
      fields.push(`filled_at = $${paramIndex++}`);
      values.push(updates.filledAt);
    }

    values.push(orderId);
    const sql = `UPDATE paper_trading_orders SET ${fields.join(', ')} WHERE id = $${paramIndex}`;

    await sqliteService.execute(sql, values);
  }

  async deleteOrder(orderId: string): Promise<void> {
    const sql = `DELETE FROM paper_trading_orders WHERE id = $1`;
    await sqliteService.execute(sql, [orderId]);
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
    const sql = `
      INSERT INTO paper_trading_trades
      (id, portfolio_id, order_id, symbol, side, price, quantity, fee, fee_rate, is_maker, timestamp)
      VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, CURRENT_TIMESTAMP)
    `;

    await sqliteService.execute(sql, [
      trade.id,
      trade.portfolioId,
      trade.orderId,
      trade.symbol,
      trade.side,
      trade.price,
      trade.quantity,
      trade.fee,
      trade.feeRate,
      trade.isMaker ? 1 : 0,
    ]);
  }

  async getTrade(tradeId: string): Promise<PaperTradingTrade | null> {
    const sql = `
      SELECT id, portfolio_id, order_id, symbol, side, price, quantity,
             fee, fee_rate, is_maker, timestamp
      FROM paper_trading_trades
      WHERE id = $1
    `;

    const result = await sqliteService.select<DBTrade[]>(sql, [tradeId]);

    if (!result || result.length === 0) {
      return null;
    }

    return this.mapDBTrade(result[0]);
  }

  async getPortfolioTrades(portfolioId: string, limit?: number): Promise<PaperTradingTrade[]> {
    let sql = `
      SELECT id, portfolio_id, order_id, symbol, side, price, quantity,
             fee, fee_rate, is_maker, timestamp
      FROM paper_trading_trades
      WHERE portfolio_id = $1
      ORDER BY timestamp DESC
    `;

    const params: any[] = [portfolioId];

    if (limit) {
      sql += ` LIMIT $2`;
      params.push(limit);
    }

    const result = await sqliteService.select<DBTrade[]>(sql, params);
    return result.map(row => this.mapDBTrade(row));
  }

  async getOrderTrades(orderId: string): Promise<PaperTradingTrade[]> {
    const sql = `
      SELECT id, portfolio_id, order_id, symbol, side, price, quantity,
             fee, fee_rate, is_maker, timestamp
      FROM paper_trading_trades
      WHERE order_id = $1
      ORDER BY timestamp ASC
    `;

    const result = await sqliteService.select<DBTrade[]>(sql, [orderId]);
    return result.map(row => this.mapDBTrade(row));
  }

  async deleteTrade(tradeId: string): Promise<void> {
    const sql = `DELETE FROM paper_trading_trades WHERE id = $1`;
    await sqliteService.execute(sql, [tradeId]);
  }

  // ============================================================================
  // MAPPING FUNCTIONS
  // ============================================================================

  private mapDBPortfolio(row: DBPortfolio): PaperTradingPortfolio {
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

  private mapDBPosition(row: DBPosition): PaperTradingPosition {
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

  private mapDBOrder(row: DBOrder): PaperTradingOrder {
    return {
      id: row.id,
      portfolioId: row.portfolio_id,
      symbol: row.symbol,
      side: row.side,
      type: row.type as any,
      amount: row.quantity,
      price: row.price || undefined,
      stopPrice: row.stop_price || undefined,
      filled: row.filled_quantity,
      average: row.avg_fill_price || undefined,
      status: row.status as any,
      timestamp: new Date(row.created_at).getTime(),
      datetime: row.created_at,
      timeInForce: row.time_in_force || undefined,
      postOnly: row.post_only === 1,
      reduceOnly: row.reduce_only === 1,
      trailingPercent: row.trailing_percent || undefined,
      trailingAmount: row.trailing_amount || undefined,
      icebergQty: row.iceberg_qty || undefined,
      leverage: row.leverage || undefined,
      marginMode: row.margin_mode as any,
      lastTradeTimestamp: row.filled_at ? new Date(row.filled_at).getTime() : undefined,
      remaining: row.quantity - row.filled_quantity,
      cost: (row.avg_fill_price || 0) * row.filled_quantity,
      info: {}, // Raw exchange response (not applicable for paper trading)
    } as PaperTradingOrder;
  }

  private mapDBTrade(row: DBTrade): PaperTradingTrade {
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
      isMaker: row.is_maker === 1,
      timestamp: row.timestamp,
    };
  }
}

// Singleton instance
export const paperTradingDatabase = new PaperTradingDatabase();
