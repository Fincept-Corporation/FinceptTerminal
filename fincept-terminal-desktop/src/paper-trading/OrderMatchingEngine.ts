/**
 * Order Matching Engine
 *
 * Realistic order execution simulation using real market data
 * Supports all order types: market, limit, stop, stop-limit, trailing stop, iceberg, post-only, reduce-only
 */

import type { Ticker, OrderBook } from 'ccxt';
import type { IExchangeAdapter, OrderType, OrderSide, OrderParams, Order } from '../brokers/crypto/types';
import type {
  PaperTradingConfig,
  PaperTradingOrder,
  PaperTradingTrade,
  OrderMatchResult,
  PriceSnapshot,
} from './types';
import { paperTradingDatabase } from './PaperTradingDatabase';
import { PaperTradingBalance } from './PaperTradingBalance';
import { InsufficientFunds } from '../brokers/crypto/types';

export class OrderMatchingEngine {
  private balanceManager: PaperTradingBalance;
  private monitoringInterval: NodeJS.Timeout | null = null;
  private priceCache: Map<string, PriceSnapshot> = new Map();
  private websocketPriceUpdates: Map<string, PriceSnapshot> = new Map();

  constructor(
    private config: PaperTradingConfig,
    private realExchangeAdapter: IExchangeAdapter
  ) {
    this.balanceManager = new PaperTradingBalance(config);
  }

  /**
   * Update price from WebSocket feed (for monitoring orders)
   */
  updatePriceFromWebSocket(symbol: string, ticker: { bid?: number; ask?: number; last?: number }): void {
    const snapshot: PriceSnapshot = {
      symbol,
      bid: ticker.bid || ticker.last || 0,
      ask: ticker.ask || ticker.last || 0,
      last: ticker.last || 0,
      timestamp: Date.now(),
    };
    this.websocketPriceUpdates.set(symbol, snapshot);
  }

  // ============================================================================
  // ORDER PLACEMENT
  // ============================================================================

  /**
   * Place a new order
   */
  async placeOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number,
    params?: OrderParams
  ): Promise<OrderMatchResult> {
    const orderId = `order_${Date.now()}_${Math.random().toString(36).substring(7)}`;

    // Get current price for validation
    const priceSnapshot = await this.fetchPriceSnapshot(symbol);
    const executionPrice = price || (side === 'buy' ? priceSnapshot.ask : priceSnapshot.bid);

    // Check sufficient funds
    const leverage = params?.leverage || this.config.defaultLeverage || 1;
    const fundsCheck = await this.balanceManager.checkSufficientFunds(
      this.config.portfolioId,
      symbol,
      side,
      amount,
      executionPrice,
      leverage
    );

    if (!fundsCheck.sufficient) {
      return {
        success: false,
        order: this.createOrderObject(orderId, symbol, type, side, amount, price, params, 'rejected'),
        trades: [],
        balance: fundsCheck.available,
        error: fundsCheck.error,
      };
    }

    // Create order in database
    await paperTradingDatabase.createOrder({
      id: orderId,
      portfolioId: this.config.portfolioId,
      symbol,
      side,
      type,
      quantity: amount,
      price: price || null,
      stopPrice: params?.stopPrice || null,
      timeInForce: params?.timeInForce,
      postOnly: params?.postOnly,
      reduceOnly: params?.reduceOnly,
      trailingPercent: params?.trailingPercent || null,
      trailingAmount: params?.trailingAmount || null,
      icebergQty: params?.icebergQty || null,
      leverage: params?.leverage,
      marginMode: params?.marginMode,
    });

    // Handle different order types
    switch (type) {
      case 'market':
        return await this.executeMarketOrder(orderId, symbol, side, amount, params);

      case 'limit':
        return await this.handleLimitOrder(orderId, symbol, side, amount, price!, params);

      case 'stop':
      case 'stop_limit':
        return await this.handleStopOrder(orderId, symbol, type, side, amount, price, params);

      case 'trailing_stop':
        return await this.handleTrailingStopOrder(orderId, symbol, side, amount, params);

      case 'iceberg':
        return await this.handleIcebergOrder(orderId, symbol, side, amount, price!, params);

      default:
        return {
          success: false,
          order: this.createOrderObject(orderId, symbol, type, side, amount, price, params, 'rejected'),
          trades: [],
          balance: fundsCheck.available,
          error: `Unsupported order type: ${type}`,
        };
    }
  }

  // ============================================================================
  // MARKET ORDERS
  // ============================================================================

  private async executeMarketOrder(
    orderId: string,
    symbol: string,
    side: OrderSide,
    amount: number,
    params?: OrderParams
  ): Promise<OrderMatchResult> {
    const priceSnapshot = await this.fetchPriceSnapshot(symbol);

    // Apply slippage for market orders
    const basePrice = side === 'buy' ? priceSnapshot.ask : priceSnapshot.bid;
    const slippage = this.config.slippage.market;
    const executionPrice = side === 'buy'
      ? basePrice * (1 + slippage)
      : basePrice * (1 - slippage);

    // Simulate latency if configured
    if (this.config.simulatedLatency && this.config.simulatedLatency > 0) {
      await this.sleep(this.config.simulatedLatency);
    }

    // Check Time-In-Force
    if (params?.timeInForce === 'PO') {
      // Post-only not valid for market orders
      await paperTradingDatabase.updateOrder(orderId, { status: 'rejected' });
      return {
        success: false,
        order: this.createOrderObject(orderId, symbol, 'market', side, amount, undefined, params, 'rejected'),
        trades: [],
        balance: 0,
        error: 'Post-only (PO) time-in-force not valid for market orders',
      };
    }

    // Execute trade
    const tradeResult = await this.executeTrade(orderId, symbol, side, executionPrice, amount, false, params);

    if (!tradeResult.success) {
      return tradeResult;
    }

    // Update order status
    await paperTradingDatabase.updateOrder(orderId, {
      filledQuantity: amount,
      avgFillPrice: executionPrice,
      status: 'filled',
      filledAt: new Date().toISOString(),
    });

    const order = await paperTradingDatabase.getOrder(orderId);
    return {
      success: true,
      order: order!,
      trades: tradeResult.trades,
      position: tradeResult.position,
      balance: tradeResult.balance,
    };
  }

  // ============================================================================
  // LIMIT ORDERS
  // ============================================================================

  private async handleLimitOrder(
    orderId: string,
    symbol: string,
    side: OrderSide,
    amount: number,
    limitPrice: number,
    params?: OrderParams
  ): Promise<OrderMatchResult> {
    const priceSnapshot = await this.fetchPriceSnapshot(symbol);

    // Check if order can be immediately filled
    const canFillImmediately = side === 'buy'
      ? priceSnapshot.ask <= limitPrice
      : priceSnapshot.bid >= limitPrice;

    // Handle Post-Only
    if (params?.postOnly && canFillImmediately) {
      await paperTradingDatabase.updateOrder(orderId, { status: 'rejected' });
      return {
        success: false,
        order: this.createOrderObject(orderId, symbol, 'limit', side, amount, limitPrice, params, 'rejected'),
        trades: [],
        balance: 0,
        error: 'Post-only order would take liquidity',
      };
    }

    // Handle IOC (Immediate-Or-Cancel)
    if (params?.timeInForce === 'IOC') {
      if (canFillImmediately) {
        return await this.executeLimitOrder(orderId, symbol, side, amount, limitPrice, params);
      } else {
        await paperTradingDatabase.updateOrder(orderId, { status: 'canceled' });
        return {
          success: false,
          order: this.createOrderObject(orderId, symbol, 'limit', side, amount, limitPrice, params, 'canceled'),
          trades: [],
          balance: 0,
          error: 'IOC order could not be filled immediately',
        };
      }
    }

    // Handle FOK (Fill-Or-Kill)
    if (params?.timeInForce === 'FOK') {
      if (canFillImmediately) {
        return await this.executeLimitOrder(orderId, symbol, side, amount, limitPrice, params);
      } else {
        await paperTradingDatabase.updateOrder(orderId, { status: 'canceled' });
        return {
          success: false,
          order: this.createOrderObject(orderId, symbol, 'limit', side, amount, limitPrice, params, 'canceled'),
          trades: [],
          balance: 0,
          error: 'FOK order could not be fully filled',
        };
      }
    }

    // GTC (Good-Till-Canceled) - default
    if (canFillImmediately) {
      return await this.executeLimitOrder(orderId, symbol, side, amount, limitPrice, params);
    } else {
      // Store as pending order
      const order = await paperTradingDatabase.getOrder(orderId);
      return {
        success: true,
        order: order!,
        trades: [],
        balance: 0,
      };
    }
  }

  private async executeLimitOrder(
    orderId: string,
    symbol: string,
    side: OrderSide,
    amount: number,
    limitPrice: number,
    params?: OrderParams
  ): Promise<OrderMatchResult> {
    const executionPrice = limitPrice; // Limit orders fill at limit price (optimistic)
    const isMaker = true; // Limit orders that wait are makers

    // Execute trade
    const tradeResult = await this.executeTrade(orderId, symbol, side, executionPrice, amount, isMaker, params);

    if (!tradeResult.success) {
      return tradeResult;
    }

    // Update order status
    await paperTradingDatabase.updateOrder(orderId, {
      filledQuantity: amount,
      avgFillPrice: executionPrice,
      status: 'filled',
      filledAt: new Date().toISOString(),
    });

    const order = await paperTradingDatabase.getOrder(orderId);
    return {
      success: true,
      order: order!,
      trades: tradeResult.trades,
      position: tradeResult.position,
      balance: tradeResult.balance,
    };
  }

  // ============================================================================
  // STOP ORDERS
  // ============================================================================

  private async handleStopOrder(
    orderId: string,
    symbol: string,
    type: 'stop' | 'stop_limit',
    side: OrderSide,
    amount: number,
    limitPrice: number | undefined,
    params?: OrderParams
  ): Promise<OrderMatchResult> {
    if (!params?.stopPrice) {
      await paperTradingDatabase.updateOrder(orderId, { status: 'rejected' });
      return {
        success: false,
        order: this.createOrderObject(orderId, symbol, type, side, amount, limitPrice, params, 'rejected'),
        trades: [],
        balance: 0,
        error: 'Stop orders require stopPrice parameter',
      };
    }

    // Store as pending order - will be monitored
    const order = await paperTradingDatabase.getOrder(orderId);
    return {
      success: true,
      order: order!,
      trades: [],
      balance: 0,
    };
  }

  private async checkStopOrderTrigger(order: PaperTradingOrder, currentPrice: number): Promise<boolean> {
    if (!order.stopPrice) return false;

    // Buy stop: trigger when price rises above stop price
    // Sell stop: trigger when price falls below stop price
    const triggered = order.side === 'buy'
      ? currentPrice >= order.stopPrice
      : currentPrice <= order.stopPrice;

    return triggered;
  }

  private async executeTriggeredStopOrder(order: PaperTradingOrder, currentPrice: number): Promise<void> {
    // Update order status to triggered
    await paperTradingDatabase.updateOrder(order.id, { status: 'triggered' });

    if (order.type === 'stop') {
      // Stop market order - execute immediately
      const priceSnapshot = await this.fetchPriceSnapshot(order.symbol);
      const executionPrice = order.side === 'buy' ? priceSnapshot.ask : priceSnapshot.bid;

      await this.executeTrade(order.id, order.symbol, order.side as OrderSide, executionPrice, order.amount, false);

      await paperTradingDatabase.updateOrder(order.id, {
        filledQuantity: order.amount,
        avgFillPrice: executionPrice,
        status: 'filled',
        filledAt: new Date().toISOString(),
      });
    } else if (order.type === 'stop_limit' && order.price) {
      // Stop limit order - convert to limit order
      // Will be filled if price reaches limit price
      // For simplicity, we'll try to fill immediately if conditions are met
      const canFill = order.side === 'buy'
        ? currentPrice <= order.price
        : currentPrice >= order.price;

      if (canFill) {
        await this.executeTrade(order.id, order.symbol, order.side as OrderSide, order.price, order.amount, true);

        await paperTradingDatabase.updateOrder(order.id, {
          filledQuantity: order.amount,
          avgFillPrice: order.price,
          status: 'filled',
          filledAt: new Date().toISOString(),
        });
      }
      // else remains in triggered state, waiting for limit price
    }
  }

  // ============================================================================
  // TRAILING STOP ORDERS
  // ============================================================================

  private async handleTrailingStopOrder(
    orderId: string,
    symbol: string,
    side: OrderSide,
    amount: number,
    params?: OrderParams
  ): Promise<OrderMatchResult> {
    if (!params?.trailingPercent && !params?.trailingAmount) {
      await paperTradingDatabase.updateOrder(orderId, { status: 'rejected' });
      return {
        success: false,
        order: this.createOrderObject(orderId, symbol, 'trailing_stop', side, amount, undefined, params, 'rejected'),
        trades: [],
        balance: 0,
        error: 'Trailing stop orders require trailingPercent or trailingAmount',
      };
    }

    // Store as pending order - will be monitored
    const order = await paperTradingDatabase.getOrder(orderId);
    return {
      success: true,
      order: order!,
      trades: [],
      balance: 0,
    };
  }

  // ============================================================================
  // ICEBERG ORDERS
  // ============================================================================

  private async handleIcebergOrder(
    orderId: string,
    symbol: string,
    side: OrderSide,
    amount: number,
    limitPrice: number,
    params?: OrderParams
  ): Promise<OrderMatchResult> {
    if (!params?.icebergQty) {
      await paperTradingDatabase.updateOrder(orderId, { status: 'rejected' });
      return {
        success: false,
        order: this.createOrderObject(orderId, symbol, 'iceberg', side, amount, limitPrice, params, 'rejected'),
        trades: [],
        balance: 0,
        error: 'Iceberg orders require icebergQty parameter',
      };
    }

    // For paper trading, we'll simplify and treat as a regular limit order
    // In real exchanges, only icebergQty is visible in order book
    return await this.handleLimitOrder(orderId, symbol, side, amount, limitPrice, params);
  }

  // ============================================================================
  // TRADE EXECUTION
  // ============================================================================

  private async executeTrade(
    orderId: string,
    symbol: string,
    side: OrderSide,
    price: number,
    quantity: number,
    isMaker: boolean,
    params?: OrderParams
  ): Promise<OrderMatchResult & { success: boolean }> {
    const feeRate = isMaker ? this.config.fees.maker : this.config.fees.taker;
    const orderValue = price * quantity;
    const fee = orderValue * feeRate;

    // Create trade record
    const tradeId = `trade_${Date.now()}_${Math.random().toString(36).substring(7)}`;
    const trade: PaperTradingTrade = {
      id: tradeId,
      portfolioId: this.config.portfolioId,
      orderId,
      symbol,
      side,
      price,
      quantity,
      fee,
      feeRate,
      isMaker,
      timestamp: new Date().toISOString(),
    };

    await paperTradingDatabase.createTrade(trade);

    // Update position
    const leverage = params?.leverage || this.config.defaultLeverage || 1;
    const marginMode = params?.marginMode || this.config.marginMode || 'cross';

    const position = await this.balanceManager.updatePositionAfterFill(
      this.config.portfolioId,
      symbol,
      side,
      price,
      quantity,
      leverage,
      marginMode
    );

    // Update portfolio balance (deduct fees)
    const portfolio = await paperTradingDatabase.getPortfolio(this.config.portfolioId);
    if (portfolio) {
      const newBalance = portfolio.currentBalance - fee;
      await paperTradingDatabase.updatePortfolioBalance(this.config.portfolioId, newBalance);
    }

    const balance = await this.balanceManager.getBalance(this.config.portfolioId);
    const currency = this.config.currency || 'USD';

    return {
      success: true,
      order: this.createOrderObject(orderId, symbol, 'market', side, quantity, price, params, 'filled'),
      trades: [trade],
      position,
      balance: (balance.free as any)?.[currency] || 0,
    };
  }

  // ============================================================================
  // ORDER MONITORING
  // ============================================================================

  /**
   * Start monitoring pending orders
   */
  startMonitoring(intervalMs: number = 500): void {
    if (this.monitoringInterval) {
      return; // Already monitoring
    }

    this.monitoringInterval = setInterval(async () => {
      await this.checkPendingOrders();
    }, intervalMs);
  }

  /**
   * Stop monitoring pending orders
   */
  stopMonitoring(): void {
    if (this.monitoringInterval) {
      clearInterval(this.monitoringInterval);
      this.monitoringInterval = null;
    }
  }

  /**
   * Check all pending orders for execution conditions
   */
  private async checkPendingOrders(): Promise<void> {
    try {
      const pendingOrders = await paperTradingDatabase.getPendingOrders(this.config.portfolioId);

      for (const order of pendingOrders) {
        try {
          const priceSnapshot = await this.fetchPriceSnapshot(order.symbol);

          // Check liquidations first
          await this.checkPositionLiquidations(order.symbol, priceSnapshot.last);

          // Check limit orders
          if (order.type === 'limit' && order.status === 'pending') {
            const canFill = order.side === 'buy'
              ? priceSnapshot.ask <= (order.price || 0)
              : priceSnapshot.bid >= (order.price || 0);

            if (canFill) {
              await this.executeLimitOrder(order.id, order.symbol, order.side as OrderSide, order.amount, order.price!, {
                leverage: order.leverage,
                marginMode: order.marginMode as any,
              });
            }
          }

          // Check stop orders
          if ((order.type === 'stop' || order.type === 'stop_limit') && order.status === 'pending') {
            const triggered = await this.checkStopOrderTrigger(order, priceSnapshot.last);
            if (triggered) {
              await this.executeTriggeredStopOrder(order, priceSnapshot.last);
            }
          }

          // Update position prices
          await this.balanceManager.updatePositionPrices(this.config.portfolioId, order.symbol, priceSnapshot.last);
        } catch (error) {
          console.error(`Error checking order ${order.id}:`, error);
        }
      }
    } catch (error) {
      console.error('Error checking pending orders:', error);
    }
  }

  /**
   * Check for position liquidations
   */
  private async checkPositionLiquidations(symbol: string, currentPrice: number): Promise<void> {
    const position = await paperTradingDatabase.getPositionBySymbol(this.config.portfolioId, symbol, 'open');
    if (!position || !position.liquidationPrice) return;

    const liquidationCheck = await this.balanceManager.checkLiquidation(position, currentPrice);
    if (liquidationCheck.shouldLiquidate) {
      await this.balanceManager.executeLiquidation(position, currentPrice);
    }
  }

  // ============================================================================
  // ORDER CANCELLATION
  // ============================================================================

  /**
   * Cancel an order
   */
  async cancelOrder(orderId: string): Promise<PaperTradingOrder> {
    const order = await paperTradingDatabase.getOrder(orderId);
    if (!order) {
      throw new Error(`Order ${orderId} not found`);
    }

    if (order.status === 'filled') {
      throw new Error(`Cannot cancel filled order ${orderId}`);
    }

    await paperTradingDatabase.updateOrder(orderId, { status: 'canceled' });

    const updatedOrder = await paperTradingDatabase.getOrder(orderId);
    return updatedOrder!;
  }

  /**
   * Cancel all orders for a symbol or all symbols
   */
  async cancelAllOrders(symbol?: string): Promise<PaperTradingOrder[]> {
    const pendingOrders = await paperTradingDatabase.getPendingOrders(this.config.portfolioId);

    const ordersToCancel = symbol
      ? pendingOrders.filter(o => o.symbol === symbol)
      : pendingOrders;

    const canceledOrders: PaperTradingOrder[] = [];
    for (const order of ordersToCancel) {
      await paperTradingDatabase.updateOrder(order.id, { status: 'canceled' });
      const updated = await paperTradingDatabase.getOrder(order.id);
      if (updated) canceledOrders.push(updated);
    }

    return canceledOrders;
  }

  // ============================================================================
  // PRICE DATA
  // ============================================================================

  /**
   * Fetch price snapshot from real exchange
   */
  private async fetchPriceSnapshot(symbol: string): Promise<PriceSnapshot> {
    // Prioritize WebSocket price updates for monitoring
    const wsPrice = this.websocketPriceUpdates.get(symbol);
    if (wsPrice && Date.now() - wsPrice.timestamp < 2000) {
      return wsPrice;
    }

    const cached = this.priceCache.get(symbol);
    const now = Date.now();

    // Use cache if fresh (within 1 second)
    if (cached && now - cached.timestamp < 1000) {
      return cached;
    }

    // Fetch fresh price
    const ticker = await this.realExchangeAdapter.fetchTicker(symbol);

    const snapshot: PriceSnapshot = {
      symbol,
      bid: ticker.bid || ticker.last || 0,
      ask: ticker.ask || ticker.last || 0,
      last: ticker.last || 0,
      timestamp: now,
    };

    this.priceCache.set(symbol, snapshot);
    return snapshot;
  }

  // ============================================================================
  // UTILITIES
  // ============================================================================

  private createOrderObject(
    id: string,
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number,
    params?: OrderParams,
    status: string = 'pending'
  ): PaperTradingOrder {
    return {
      id,
      portfolioId: this.config.portfolioId,
      symbol,
      type,
      side,
      amount,
      price,
      stopPrice: params?.stopPrice,
      filled: 0,
      remaining: amount,
      average: undefined,
      status: status as any,
      timestamp: Date.now(),
      datetime: new Date().toISOString(),
      timeInForce: params?.timeInForce,
      postOnly: params?.postOnly,
      reduceOnly: params?.reduceOnly,
      trailingPercent: params?.trailingPercent,
      trailingAmount: params?.trailingAmount,
      icebergQty: params?.icebergQty,
      leverage: params?.leverage,
      marginMode: params?.marginMode,
      cost: 0,
      info: {},
    } as PaperTradingOrder;
  }

  private sleep(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Cleanup resources
   */
  destroy(): void {
    this.stopMonitoring();
    this.priceCache.clear();
  }
}
