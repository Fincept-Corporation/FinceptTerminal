// File: src/services/trading/paperTradingService.ts
// Paper trading engine - simulates trading with virtual money

import Database from '@tauri-apps/plugin-sql';
import type {
  VirtualPortfolio,
  Position,
  Order,
  Trade,
  OrderRequest,
  OrderResult,
  PaperTradingConfig,
  DbPortfolio,
  DbPosition,
  DbOrder,
  DbTrade
} from '../../types/trading';
import { getWebSocketManager } from '../websocket';
import type { NormalizedMessage, Subscription } from '../websocket/types';

/**
 * Paper Trading Service
 * Manages virtual portfolios, positions, orders, and trades
 */
export class PaperTradingService {
  private db: Database | null = null;
  private config: PaperTradingConfig;
  private marketPrices = new Map<string, number>(); // symbol -> current price
  private priceSubscriptions = new Map<string, Subscription>(); // symbol -> subscription
  private orderMonitorInterval: NodeJS.Timeout | null = null;

  constructor(config?: Partial<PaperTradingConfig>) {
    this.config = {
      portfolioId: config?.portfolioId || '',
      feeRate: config?.feeRate || 0.0002,    // 0.02% fee
      slippage: config?.slippage || 0.0001   // 0.01% slippage
    };
  }

  /**
   * Initialize database connection
   */
  async initialize(): Promise<void> {
    if (!this.db) {
      this.db = await Database.load('sqlite:fincept_terminal.db');
    }
  }

  /**
   * Create a new paper trading portfolio
   */
  async createPortfolio(
    name: string,
    provider: string,
    initialBalance: number = 100000
  ): Promise<string> {
    await this.initialize();

    const id = crypto.randomUUID();

    await this.db!.execute(
      `INSERT INTO paper_trading_portfolios (id, name, provider, initial_balance, current_balance)
       VALUES ($1, $2, $3, $4, $5)`,
      [id, name, provider, initialBalance, initialBalance]
    );

    this.config.portfolioId = id;

    return id;
  }

  /**
   * Get portfolio by ID
   */
  async getPortfolio(portfolioId: string): Promise<VirtualPortfolio | null> {
    await this.initialize();

    const results = await this.db!.select<DbPortfolio[]>(
      'SELECT * FROM paper_trading_portfolios WHERE id = $1',
      [portfolioId]
    );

    if (results.length === 0) return null;

    const dbPortfolio = results[0];
    const positions = await this.getPositions(portfolioId);

    // Calculate total P&L
    const totalPnL = positions.reduce((sum, pos) => {
      return sum + (pos.unrealizedPnL || 0) + pos.realizedPnL;
    }, 0);

    return {
      id: dbPortfolio.id,
      name: dbPortfolio.name,
      provider: dbPortfolio.provider,
      initialBalance: dbPortfolio.initial_balance,
      balance: dbPortfolio.current_balance,
      totalPnL,
      positions,
      createdAt: dbPortfolio.created_at,
      updatedAt: dbPortfolio.updated_at
    };
  }

  /**
   * Get all open positions for a portfolio
   */
  async getPositions(portfolioId: string, status: 'open' | 'closed' | 'all' = 'open'): Promise<Position[]> {
    await this.initialize();

    let query = 'SELECT * FROM paper_trading_positions WHERE portfolio_id = $1';
    const params: any[] = [portfolioId];

    if (status !== 'all') {
      query += ' AND status = $2';
      params.push(status);
    }

    query += ' ORDER BY opened_at DESC';

    const results = await this.db!.select<DbPosition[]>(query, params);

    return results.map(dbPos => this.mapDbPosition(dbPos));
  }

  /**
   * Get all orders for a portfolio
   */
  async getOrders(portfolioId: string): Promise<Order[]> {
    await this.initialize();

    const results = await this.db!.select<DbOrder[]>(
      'SELECT * FROM paper_trading_orders WHERE portfolio_id = $1 ORDER BY created_at DESC',
      [portfolioId]
    );

    return results.map(dbOrder => this.mapDbOrder(dbOrder));
  }

  /**
   * Place an order
   */
  async placeOrder(portfolioId: string, order: OrderRequest): Promise<OrderResult> {
    await this.initialize();

    const portfolio = await this.getPortfolio(portfolioId);

    if (!portfolio) {
      return { success: false, error: 'Portfolio not found' };
    }

    // Validate balance for buy orders
    if (order.side === 'buy') {
      const estimatedPrice = order.price || await this.getCurrentPrice(order.symbol);
      const requiredBalance = order.quantity * estimatedPrice;

      if (portfolio.balance < requiredBalance) {
        return { success: false, error: 'Insufficient balance' };
      }
    }

    // Validate order parameters
    if ((order.type === 'limit' || order.type === 'stop_limit') && !order.price) {
      return { success: false, error: 'Limit orders require a price' };
    }

    if ((order.type === 'stop_market' || order.type === 'stop_limit') && !order.stopPrice) {
      return { success: false, error: 'Stop orders require a stop price' };
    }

    // Create order record
    const orderId = crypto.randomUUID();

    await this.db!.execute(
      `INSERT INTO paper_trading_orders
       (id, portfolio_id, symbol, side, type, quantity, price, stop_price, status)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8, 'pending')`,
      [
        orderId,
        portfolioId,
        order.symbol,
        order.side,
        order.type,
        order.quantity,
        order.price || null,
        order.stopPrice || null
      ]
    );

    // Execute market orders immediately
    if (order.type === 'market') {
      await this.executeMarketOrder(portfolioId, orderId, order);
    }

    // Subscribe to price updates for this symbol
    await this.subscribeToPriceUpdates(order.symbol, portfolio.provider);

    return { success: true, orderId };
  }

  /**
   * Execute a market order
   */
  private async executeMarketOrder(
    portfolioId: string,
    orderId: string,
    order: OrderRequest
  ): Promise<void> {
    const currentPrice = await this.getCurrentPrice(order.symbol);

    // Apply slippage
    const executionPrice = order.side === 'buy'
      ? currentPrice * (1 + this.config.slippage)
      : currentPrice * (1 - this.config.slippage);

    // Calculate fee
    const fee = executionPrice * order.quantity * this.config.feeRate;

    // Total cost/proceeds
    const totalCost = order.side === 'buy'
      ? (executionPrice * order.quantity) + fee
      : (executionPrice * order.quantity) - fee;

    // Update order status
    await this.db!.execute(
      `UPDATE paper_trading_orders
       SET status = 'filled', filled_quantity = $1, avg_fill_price = $2, filled_at = CURRENT_TIMESTAMP
       WHERE id = $3`,
      [order.quantity, executionPrice, orderId]
    );

    // Create trade record
    await this.createTrade(portfolioId, orderId, order, executionPrice, fee);

    // Update position
    await this.updatePosition(portfolioId, order, executionPrice);

    // Update balance
    const balanceChange = order.side === 'buy' ? -totalCost : totalCost;
    await this.updateBalance(portfolioId, balanceChange);
  }

  /**
   * Execute a limit order (called by order monitor)
   */
  private async executeLimitOrder(
    portfolioId: string,
    orderId: string,
    order: DbOrder,
    currentPrice: number
  ): Promise<void> {
    const fee = currentPrice * order.quantity * this.config.feeRate;

    // Update order status
    await this.db!.execute(
      `UPDATE paper_trading_orders
       SET status = 'filled', filled_quantity = $1, avg_fill_price = $2, filled_at = CURRENT_TIMESTAMP
       WHERE id = $3`,
      [order.quantity, currentPrice, order.id]
    );

    // Create trade record
    const tradeRequest: OrderRequest = {
      symbol: order.symbol,
      side: order.side,
      type: order.type,
      quantity: order.quantity
    };

    await this.createTrade(portfolioId, orderId, tradeRequest, currentPrice, fee);

    // Update position
    await this.updatePosition(portfolioId, tradeRequest, currentPrice);

    // Update balance
    const totalCost = order.side === 'buy'
      ? (currentPrice * order.quantity) + fee
      : (currentPrice * order.quantity) - fee;

    const balanceChange = order.side === 'buy' ? -totalCost : totalCost;
    await this.updateBalance(portfolioId, balanceChange);
  }

  /**
   * Create trade record
   */
  private async createTrade(
    portfolioId: string,
    orderId: string,
    order: OrderRequest,
    price: number,
    fee: number
  ): Promise<void> {
    const tradeId = crypto.randomUUID();

    await this.db!.execute(
      `INSERT INTO paper_trading_trades
       (id, portfolio_id, order_id, symbol, side, price, quantity, fee)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`,
      [tradeId, portfolioId, orderId, order.symbol, order.side, price, order.quantity, fee]
    );
  }

  /**
   * Update or create position
   */
  private async updatePosition(
    portfolioId: string,
    order: OrderRequest,
    price: number
  ): Promise<void> {
    // Check if position exists
    const existing = await this.db!.select<DbPosition[]>(
      `SELECT * FROM paper_trading_positions
       WHERE portfolio_id = $1 AND symbol = $2 AND status = 'open'`,
      [portfolioId, order.symbol]
    );

    if (existing.length > 0) {
      const position = existing[0];

      // Determine if we're adding to or closing position
      const isSameSide = (order.side === 'buy' && position.side === 'long') ||
                         (order.side === 'sell' && position.side === 'short');

      if (isSameSide) {
        // Add to position
        const newQuantity = position.quantity + order.quantity;
        const newAvgPrice = ((position.entry_price * position.quantity) + (price * order.quantity)) / newQuantity;

        await this.db!.execute(
          `UPDATE paper_trading_positions
           SET quantity = $1, entry_price = $2, updated_at = CURRENT_TIMESTAMP
           WHERE id = $3`,
          [newQuantity, newAvgPrice, position.id]
        );
      } else {
        // Opposite side - reduce or close position
        if (order.quantity >= position.quantity) {
          // Close entire position
          const pnl = (price - position.entry_price) * position.quantity *
                     (position.side === 'long' ? 1 : -1);

          await this.db!.execute(
            `UPDATE paper_trading_positions
             SET status = 'closed', realized_pnl = $1, closed_at = CURRENT_TIMESTAMP
             WHERE id = $2`,
            [pnl, position.id]
          );

          // If order quantity exceeds position, create new opposite position
          if (order.quantity > position.quantity) {
            await this.createPosition(
              portfolioId,
              order.symbol,
              order.side === 'buy' ? 'long' : 'short',
              price,
              order.quantity - position.quantity
            );
          }
        } else {
          // Reduce position
          const newQuantity = position.quantity - order.quantity;
          const partialPnl = (price - position.entry_price) * order.quantity *
                            (position.side === 'long' ? 1 : -1);

          await this.db!.execute(
            `UPDATE paper_trading_positions
             SET quantity = $1, realized_pnl = realized_pnl + $2, updated_at = CURRENT_TIMESTAMP
             WHERE id = $3`,
            [newQuantity, partialPnl, position.id]
          );
        }
      }
    } else {
      // Create new position
      await this.createPosition(
        portfolioId,
        order.symbol,
        order.side === 'buy' ? 'long' : 'short',
        price,
        order.quantity
      );
    }
  }

  /**
   * Create new position
   */
  private async createPosition(
    portfolioId: string,
    symbol: string,
    side: 'long' | 'short',
    entryPrice: number,
    quantity: number
  ): Promise<void> {
    const positionId = crypto.randomUUID();

    await this.db!.execute(
      `INSERT INTO paper_trading_positions
       (id, portfolio_id, symbol, side, entry_price, quantity, status)
       VALUES ($1, $2, $3, $4, $5, $6, 'open')`,
      [positionId, portfolioId, symbol, side, entryPrice, quantity]
    );
  }

  /**
   * Update portfolio balance
   */
  private async updateBalance(portfolioId: string, amount: number): Promise<void> {
    await this.db!.execute(
      `UPDATE paper_trading_portfolios
       SET current_balance = current_balance + $1, updated_at = CURRENT_TIMESTAMP
       WHERE id = $2`,
      [amount, portfolioId]
    );
  }

  /**
   * Subscribe to market price updates for a symbol
   */
  private async subscribeToPriceUpdates(symbol: string, provider: string): Promise<void> {
    // Avoid duplicate subscriptions
    if (this.priceSubscriptions.has(symbol)) {
      return;
    }

    const manager = getWebSocketManager();
    const topic = `${provider}.ticker.${symbol}`;

    const subscription = await manager.subscribe(topic, (message: NormalizedMessage) => {
      this.handlePriceUpdate(message);
    });

    this.priceSubscriptions.set(symbol, subscription);

    // Start order monitor if not already running
    if (!this.orderMonitorInterval) {
      this.startOrderMonitor();
    }
  }

  /**
   * Handle incoming price updates
   */
  private handlePriceUpdate(message: NormalizedMessage): void {
    const symbol = message.symbol;
    if (!symbol) return;

    let price: number | undefined;

    // Extract price from different message types
    if (message.type === 'ticker') {
      price = message.data.last || message.data.close || message.data.mid;
    } else if (message.type === 'trade') {
      price = message.data.price || message.data.last;
    }

    if (price) {
      this.marketPrices.set(symbol, price);

      // Update unrealized P&L for open positions
      this.updateUnrealizedPnL(symbol, price);
    }
  }

  /**
   * Get current market price for a symbol
   */
  private async getCurrentPrice(symbol: string): Promise<number> {
    const price = this.marketPrices.get(symbol);

    if (!price) {
      throw new Error(`No market price available for ${symbol}`);
    }

    return price;
  }

  /**
   * Update unrealized P&L for all positions of a symbol
   */
  private async updateUnrealizedPnL(symbol: string, currentPrice: number): Promise<void> {
    await this.initialize();

    const positions = await this.db!.select<DbPosition[]>(
      `SELECT * FROM paper_trading_positions WHERE symbol = $1 AND status = 'open'`,
      [symbol]
    );

    for (const position of positions) {
      const pnl = (currentPrice - position.entry_price) * position.quantity *
                 (position.side === 'long' ? 1 : -1);

      await this.db!.execute(
        `UPDATE paper_trading_positions
         SET current_price = $1, unrealized_pnl = $2, updated_at = CURRENT_TIMESTAMP
         WHERE id = $3`,
        [currentPrice, pnl, position.id]
      );
    }
  }

  /**
   * Monitor pending orders and execute when conditions are met
   */
  private startOrderMonitor(): void {
    // Check every 500ms
    this.orderMonitorInterval = setInterval(async () => {
      await this.checkPendingOrders();
    }, 500);
  }

  /**
   * Check all pending orders and execute if conditions are met
   */
  private async checkPendingOrders(): Promise<void> {
    await this.initialize();

    const pendingOrders = await this.db!.select<DbOrder[]>(
      `SELECT * FROM paper_trading_orders WHERE status IN ('pending', 'triggered')`
    );

    for (const order of pendingOrders) {
      const currentPrice = this.marketPrices.get(order.symbol);
      if (!currentPrice) continue;

      if (order.type === 'limit' && order.price) {
        // Buy limit: execute if market <= limit
        // Sell limit: execute if market >= limit
        const shouldExecute = order.side === 'buy'
          ? currentPrice <= order.price
          : currentPrice >= order.price;

        if (shouldExecute) {
          await this.executeLimitOrder(order.portfolio_id, order.id, order, currentPrice);
        }
      } else if (order.type === 'stop_market' && order.stop_price) {
        // Buy stop: trigger if market >= stop
        // Sell stop: trigger if market <= stop
        const triggered = order.side === 'buy'
          ? currentPrice >= order.stop_price
          : currentPrice <= order.stop_price;

        if (triggered) {
          // Execute as market order
          const orderRequest: OrderRequest = {
            symbol: order.symbol,
            side: order.side,
            type: 'market',
            quantity: order.quantity
          };

          await this.executeMarketOrder(order.portfolio_id, order.id, orderRequest);
        }
      } else if (order.type === 'stop_limit' && order.stop_price && order.price) {
        // Two-step: first trigger, then wait for limit
        if (order.status === 'pending') {
          // Check if stop triggered
          const triggered = order.side === 'buy'
            ? currentPrice >= order.stop_price
            : currentPrice <= order.stop_price;

          if (triggered) {
            await this.db!.execute(
              `UPDATE paper_trading_orders SET status = 'triggered' WHERE id = $1`,
              [order.id]
            );
          }
        } else if (order.status === 'triggered') {
          // Now check limit condition
          const shouldExecute = order.side === 'buy'
            ? currentPrice <= order.price
            : currentPrice >= order.price;

          if (shouldExecute) {
            await this.executeLimitOrder(order.portfolio_id, order.id, order, currentPrice);
          }
        }
      }
    }
  }

  /**
   * Cancel an order
   */
  async cancelOrder(orderId: string): Promise<boolean> {
    await this.initialize();

    await this.db!.execute(
      `UPDATE paper_trading_orders SET status = 'cancelled' WHERE id = $1 AND status IN ('pending', 'triggered')`,
      [orderId]
    );

    return true;
  }

  /**
   * Close a position
   */
  async closePosition(positionId: string): Promise<boolean> {
    await this.initialize();

    const results = await this.db!.select<DbPosition[]>(
      `SELECT * FROM paper_trading_positions WHERE id = $1 AND status = 'open'`,
      [positionId]
    );

    if (results.length === 0) return false;

    const position = results[0];
    const currentPrice = this.marketPrices.get(position.symbol);

    if (!currentPrice) {
      throw new Error(`No current price for ${position.symbol}`);
    }

    // Create closing order
    const orderRequest: OrderRequest = {
      symbol: position.symbol,
      side: position.side === 'long' ? 'sell' : 'buy',
      type: 'market',
      quantity: position.quantity
    };

    await this.placeOrder(position.portfolio_id, orderRequest);

    return true;
  }

  /**
   * Reset portfolio to initial state
   */
  async resetPortfolio(portfolioId: string): Promise<boolean> {
    await this.initialize();

    const portfolio = await this.getPortfolio(portfolioId);
    if (!portfolio) return false;

    // Delete all positions, orders, and trades
    await this.db!.execute(`DELETE FROM paper_trading_trades WHERE portfolio_id = $1`, [portfolioId]);
    await this.db!.execute(`DELETE FROM paper_trading_orders WHERE portfolio_id = $1`, [portfolioId]);
    await this.db!.execute(`DELETE FROM paper_trading_positions WHERE portfolio_id = $1`, [portfolioId]);

    // Reset balance
    await this.db!.execute(
      `UPDATE paper_trading_portfolios SET current_balance = initial_balance, updated_at = CURRENT_TIMESTAMP WHERE id = $1`,
      [portfolioId]
    );

    return true;
  }

  /**
   * Cleanup - unsubscribe from price updates and stop monitoring
   */
  cleanup(): void {
    // Unsubscribe from all price updates
    this.priceSubscriptions.forEach(sub => sub.unsubscribe());
    this.priceSubscriptions.clear();

    // Stop order monitor
    if (this.orderMonitorInterval) {
      clearInterval(this.orderMonitorInterval);
      this.orderMonitorInterval = null;
    }
  }

  // Helper mappers
  private mapDbPosition(dbPos: DbPosition): Position {
    return {
      id: dbPos.id,
      portfolioId: dbPos.portfolio_id,
      symbol: dbPos.symbol,
      side: dbPos.side,
      entryPrice: dbPos.entry_price,
      quantity: dbPos.quantity,
      currentPrice: dbPos.current_price || undefined,
      unrealizedPnL: dbPos.unrealized_pnl || undefined,
      realizedPnL: dbPos.realized_pnl,
      openedAt: dbPos.opened_at,
      closedAt: dbPos.closed_at || undefined,
      status: dbPos.status
    };
  }

  private mapDbOrder(dbOrder: DbOrder): Order {
    return {
      id: dbOrder.id,
      portfolioId: dbOrder.portfolio_id,
      symbol: dbOrder.symbol,
      side: dbOrder.side,
      type: dbOrder.type,
      quantity: dbOrder.quantity,
      price: dbOrder.price || undefined,
      stopPrice: dbOrder.stop_price || undefined,
      filledQuantity: dbOrder.filled_quantity,
      avgFillPrice: dbOrder.avg_fill_price || undefined,
      status: dbOrder.status,
      createdAt: dbOrder.created_at,
      filledAt: dbOrder.filled_at || undefined
    };
  }
}

/**
 * Create a new paper trading service instance
 */
export function createPaperTradingService(config?: Partial<PaperTradingConfig>): PaperTradingService {
  return new PaperTradingService(config);
}
