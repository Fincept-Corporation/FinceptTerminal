/**
 * Universal Paper Trading Adapter
 *
 * Full IExchangeAdapter implementation for realistic paper trading
 * Works with ANY broker: crypto, stocks, forex, commodities, etc.
 * Uses real market data from underlying broker adapter for execution simulation
 */

import type { Market, Ticker, OrderBook, Trade, OHLCV, Balance, Order, Position } from 'ccxt';
import { BaseExchangeAdapter } from '../brokers/crypto/BaseExchangeAdapter';
import type {
  IExchangeAdapter,
  ExchangeCredentials,
  ExchangeConfig,
  ExchangeCapabilities,
} from '../brokers/crypto/types';
import type { PaperTradingConfig, OrderType, OrderSide, OrderParams } from './types';
import { paperTradingDatabase } from './PaperTradingDatabase';
import { PaperTradingBalance } from './PaperTradingBalance';
import { OrderMatchingEngine } from './OrderMatchingEngine';

export class PaperTradingAdapter extends BaseExchangeAdapter {
  private paperConfig: PaperTradingConfig;
  private realAdapter: IExchangeAdapter;
  private balanceManager: PaperTradingBalance;
  private matchingEngine: OrderMatchingEngine;
  private initialized: boolean = false;
  // NO CACHING - Real-time data only

  constructor(config: ExchangeConfig & { paperTradingConfig: PaperTradingConfig; realAdapter: IExchangeAdapter }) {
    // Initialize base with real exchange name (CCXT doesn't support "_paper" suffix)
    super({ ...config, exchange: config.paperTradingConfig.provider });

    this.paperConfig = config.paperTradingConfig;
    this.realAdapter = config.realAdapter;
    this.balanceManager = new PaperTradingBalance(this.paperConfig);
    this.matchingEngine = new OrderMatchingEngine(this.paperConfig, this.realAdapter);

    // Set name (using Object.defineProperty to override readonly)
    Object.defineProperty(this, 'name', {
      value: `${this.realAdapter.name} (Paper Trading)`,
      writable: false,
      configurable: true
    });
  }

  // ============================================================================
  // INITIALIZATION
  // ============================================================================

  async connect(): Promise<void> {
    try {
      console.log('[PaperTradingAdapter] Starting connection...');
      console.log('[PaperTradingAdapter] Real adapter connected:', this.realAdapter.isConnected());

      // Try to connect to real exchange for market data, but don't fail if it doesn't work
      if (!this.realAdapter.isConnected()) {
        console.log('[PaperTradingAdapter] Attempting to connect real adapter for market data...');
        try {
          await this.realAdapter.connect();
          console.log('[PaperTradingAdapter] [OK] Real adapter connected successfully');
        } catch (error) {
          console.warn('[PaperTradingAdapter] [WARN] Real adapter connection failed:', error);
          console.log('[PaperTradingAdapter] Paper trading will work in offline mode (market data unavailable)');
          // Don't throw - continue with paper adapter in offline mode
        }
      } else {
        console.log('[PaperTradingAdapter] Real adapter already connected, skipping');
      }

      // Initialize or load portfolio
      console.log('[PaperTradingAdapter] Initializing portfolio...');
      await this.initializePortfolio();
      console.log('[PaperTradingAdapter] Portfolio initialized');

      // Start order monitoring only if real adapter is connected
      // (Without real adapter, we can't get market prices for matching)
      if (this.realAdapter.isConnected() && this.paperConfig.enableRealtimeUpdates !== false) {
        console.log('[PaperTradingAdapter] Starting order monitoring...');
        // Reduced from 1000ms to 200ms for more responsive limit order fills
        this.matchingEngine.startMonitoring(this.paperConfig.priceUpdateInterval || 200);
      } else {
        console.log('[PaperTradingAdapter] Skipping order monitoring (real adapter not connected)');
      }

      this._isConnected = true;
      this._isAuthenticated = true; // Paper trading doesn't require real auth
      this.initialized = true;

      console.log('[PaperTradingAdapter] [OK] Connection complete!');
      console.log('[PaperTradingAdapter] Mode:', this.realAdapter.isConnected() ? 'ONLINE (with market data)' : 'OFFLINE (no market data)');
      this.emit('connected', { exchange: this.id });
      this.emit('authenticated', { exchange: this.id });
    } catch (error) {
      this._isConnected = false;
      console.error('[PaperTradingAdapter] [FAIL] Connection failed:', error);
      throw this.handleError(error);
    }
  }

  async disconnect(): Promise<void> {
    // Clean up all resources (stop monitoring + clear caches)
    this.matchingEngine.cleanup();
    this._isConnected = false;
    this._isAuthenticated = false;
    this.initialized = false;
    this.emit('disconnected', { exchange: this.id });
  }

  /**
   * Reset paper trading account to initial state
   * - Closes all positions
   * - Cancels all orders
   * - Resets balance to initial capital
   * - Clears trade history
   */
  async resetAccount(): Promise<void> {
    console.log('[PaperTradingAdapter] Resetting account...');
    const portfolioId = this.paperConfig.portfolioId;

    try {
      // 1. Close all open positions
      const openPositions = await paperTradingDatabase.getPortfolioPositions(portfolioId, 'open');
      console.log(`[PaperTradingAdapter] Closing ${openPositions.length} open positions...`);
      for (const position of openPositions) {
        await paperTradingDatabase.updatePosition(position.id, {
          status: 'closed',
          closedAt: new Date().toISOString(),
        });
      }

      // 2. Cancel all pending orders
      const pendingOrders = await paperTradingDatabase.getPendingOrders(portfolioId);
      console.log(`[PaperTradingAdapter] Cancelling ${pendingOrders.length} pending orders...`);
      for (const order of pendingOrders) {
        await paperTradingDatabase.updateOrder(order.id, { status: 'canceled' });
      }

      // 3. Delete all positions (to clean up)
      console.log('[PaperTradingAdapter] Deleting all positions...');
      const allPositions = await paperTradingDatabase.getPortfolioPositions(portfolioId);
      for (const position of allPositions) {
        await paperTradingDatabase.deletePosition(position.id);
      }

      // 4. Delete all orders (to clean up)
      console.log('[PaperTradingAdapter] Deleting all orders...');
      const allOrders = await paperTradingDatabase.getPortfolioOrders(portfolioId);
      for (const order of allOrders) {
        await paperTradingDatabase.deleteOrder(order.id);
      }

      // 5. Delete all trades (to clean up)
      console.log('[PaperTradingAdapter] Deleting all trades...');
      const allTrades = await paperTradingDatabase.getPortfolioTrades(portfolioId);
      for (const trade of allTrades) {
        await paperTradingDatabase.deleteTrade(trade.id);
      }

      // 6. Reset balance to initial capital
      const initialBalance = this.paperConfig.initialBalance;
      console.log(`[PaperTradingAdapter] Resetting balance to ${initialBalance}...`);
      await paperTradingDatabase.updatePortfolioBalance(portfolioId, initialBalance);

      // 7. Reinitialize balance manager
      this.balanceManager = new PaperTradingBalance(this.paperConfig);

      console.log('[PaperTradingAdapter] [OK] Account reset complete!');
      this.emit('reset', { exchange: this.id, portfolioId });
    } catch (error) {
      console.error('[PaperTradingAdapter] [FAIL] Reset failed:', error);
      throw error;
    }
  }

  async authenticate(credentials: ExchangeCredentials): Promise<void> {
    // Paper trading doesn't require authentication
    // but we'll mark as authenticated for consistency
    this._isAuthenticated = true;
    this.emit('authenticated', { exchange: this.id });
  }

  private async initializePortfolio(): Promise<void> {
    // Check database health first
    console.log('[PaperTradingAdapter] Checking database health...');
    const isHealthy = await paperTradingDatabase.checkHealth();

    if (!isHealthy) {
      throw new Error(
        'Paper trading database is not accessible. The Rust database backend may have failed to initialize. ' +
        'Please check the terminal/console for database initialization errors and restart the application.'
      );
    }

    console.log('[PaperTradingAdapter] Database health check passed');

    const existing = await paperTradingDatabase.getPortfolio(this.paperConfig.portfolioId);

    if (!existing) {
      // Create new portfolio
      console.log('[PaperTradingAdapter] Creating new portfolio:', this.paperConfig.portfolioId);
      await paperTradingDatabase.createPortfolio({
        id: this.paperConfig.portfolioId,
        name: this.paperConfig.portfolioName,
        provider: this.paperConfig.provider,
        initialBalance: this.paperConfig.initialBalance,
        currency: this.paperConfig.currency,
        marginMode: this.paperConfig.marginMode,
        leverage: this.paperConfig.defaultLeverage,
      });
      console.log('[PaperTradingAdapter] Portfolio created successfully');
    } else {
      console.log('[PaperTradingAdapter] Using existing portfolio:', this.paperConfig.portfolioId);
    }
  }

  // ============================================================================
  // MARKET DATA (Delegate to real exchange)
  // ============================================================================

  async fetchMarkets(): Promise<Market[]> {
    return await this.realAdapter.fetchMarkets();
  }

  async fetchTicker(symbol: string): Promise<Ticker> {
    return await this.realAdapter.fetchTicker(symbol);
  }

  async fetchTickers(symbols?: string[]): Promise<Record<string, Ticker>> {
    return await this.realAdapter.fetchTickers(symbols);
  }

  async fetchOrderBook(symbol: string, limit?: number): Promise<OrderBook> {
    return await this.realAdapter.fetchOrderBook(symbol, limit);
  }

  async fetchTrades(symbol: string, limit?: number): Promise<Trade[]> {
    return await this.realAdapter.fetchTrades(symbol, limit);
  }

  async fetchOHLCV(symbol: string, timeframe: string, limit?: number): Promise<OHLCV[]> {
    return await this.realAdapter.fetchOHLCV(symbol, timeframe, limit);
  }

  getMarkets(): Record<string, Market> {
    return this.exchange.markets || {};
  }

  async fetchStatus(): Promise<any> {
    if ('fetchStatus' in this.realAdapter && typeof this.realAdapter.fetchStatus === 'function') {
      return await this.realAdapter.fetchStatus();
    }
    return { status: 'ok', updated: Date.now() };
  }

  async fetchCurrencies(): Promise<any> {
    if ('fetchCurrencies' in this.realAdapter && typeof this.realAdapter.fetchCurrencies === 'function') {
      return await this.realAdapter.fetchCurrencies();
    }
    return {};
  }

  async fetchTime(): Promise<number> {
    if ('fetchTime' in this.realAdapter && typeof this.realAdapter.fetchTime === 'function') {
      return await this.realAdapter.fetchTime();
    }
    return Date.now();
  }

  // ============================================================================
  // ACCOUNT & BALANCE (Paper trading specific)
  // ============================================================================

  async fetchBalance(): Promise<Balance> {
    if (!this.initialized) {
      throw new Error('Paper trading adapter not initialized. Call connect() first.');
    }

    const balance = await this.balanceManager.getBalance(this.paperConfig.portfolioId);
    this.emit('balance', { exchange: this.id, data: balance });
    return balance;
  }

  async fetchPositions(symbols?: string[]): Promise<Position[]> {
    if (!this.initialized) {
      throw new Error('Paper trading adapter not initialized. Call connect() first.');
    }

    const positions = await paperTradingDatabase.getPortfolioPositions(this.paperConfig.portfolioId, 'open');

    // Update PnL with current market prices
    for (const p of positions) {
      // Fetch live market price for each position
      let currentPrice = p.currentPrice || p.entryPrice;
      try {
        const ticker = await this.realAdapter.fetchTicker(p.symbol);
        if (ticker && ticker.last) {
          currentPrice = ticker.last;
          // Update the database with the current price
          await paperTradingDatabase.updatePosition(p.id, {
            currentPrice: currentPrice
          });
        }
      } catch (error) {
        console.warn(`[PaperTrading] Failed to fetch current price for ${p.symbol}, using cached price:`, error);
      }

      const unrealizedPnl = p.side === 'long'
        ? (currentPrice - p.entryPrice) * p.quantity
        : (p.entryPrice - currentPrice) * p.quantity;

      p.currentPrice = currentPrice;
      p.unrealizedPnl = unrealizedPnl;
    }

    // Convert to CCXT Position format
    const ccxtPositions: Position[] = positions
      .filter(p => !symbols || symbols.includes(p.symbol))
      .map(p => ({
        info: p,
        id: p.id,
        symbol: p.symbol,
        timestamp: new Date(p.openedAt).getTime(),
        datetime: p.openedAt,
        initialMargin: (p.quantity * p.entryPrice) / p.leverage,
        initialMarginPercentage: 1 / p.leverage,
        maintenanceMargin: ((p.quantity * p.entryPrice) / p.leverage) * 0.5,
        maintenanceMarginPercentage: 0.5 / p.leverage,
        entryPrice: p.entryPrice,
        notional: p.quantity * (p.currentPrice || p.entryPrice),
        leverage: p.leverage,
        unrealizedPnl: p.unrealizedPnl || 0,
        contracts: p.quantity,
        contractSize: 1,
        marginRatio: undefined,
        liquidationPrice: p.liquidationPrice || 0,
        markPrice: p.currentPrice || p.entryPrice,
        collateral: (p.quantity * p.entryPrice) / p.leverage,
        marginMode: p.marginMode,
        side: p.side === 'long' ? 'long' : 'short',
        percentage: p.unrealizedPnl ? (p.unrealizedPnl / ((p.quantity * p.entryPrice) / p.leverage)) * 100 : 0,
      } as Position));

    this.emit('position', { exchange: this.id, data: ccxtPositions });
    return ccxtPositions;
  }

  async fetchLedger(code?: string, since?: number, limit?: number): Promise<any> {
    // Return trades as ledger entries
    const trades = await paperTradingDatabase.getPortfolioTrades(this.paperConfig.portfolioId, limit);
    return trades.map(t => ({
      id: t.id,
      timestamp: new Date(t.timestamp).getTime(),
      datetime: t.timestamp,
      direction: t.side === 'buy' ? 'in' : 'out',
      account: this.paperConfig.portfolioId,
      referenceId: t.orderId,
      referenceAccount: t.symbol,
      type: 'trade',
      currency: code || this.paperConfig.currency || 'USD',
      amount: t.quantity,
      before: null,
      after: null,
      status: 'ok',
      fee: { cost: t.fee, currency: this.paperConfig.currency || 'USD' },
      info: t,
    }));
  }

  async fetchTradingFee(symbol: string): Promise<any> {
    return {
      info: {},
      symbol,
      maker: this.paperConfig.fees.maker,
      taker: this.paperConfig.fees.taker,
      percentage: true,
      tierBased: false,
    };
  }

  // ============================================================================
  // TRADING OPERATIONS (Paper trading specific)
  // ============================================================================

  async createOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number,
    params?: OrderParams
  ): Promise<Order> {
    if (!this.initialized) {
      throw new Error('Paper trading adapter not initialized. Call connect() first.');
    }

    const result = await this.matchingEngine.placeOrder(symbol, type, side, amount, price, params);

    if (!result.success) {
      throw new Error(result.error || 'Order placement failed');
    }

    this.emit('order', { exchange: this.id, data: result.order });
    if (result.position) {
      this.emit('position', { exchange: this.id, data: result.position });
    }

    return result.order as Order;
  }

  async editOrder(
    orderId: string,
    symbol: string,
    type?: OrderType,
    side?: OrderSide,
    amount?: number,
    price?: number,
    params?: OrderParams
  ): Promise<Order> {
    if (!this.initialized) {
      throw new Error('Paper trading adapter not initialized. Call connect() first.');
    }

    // Import the lock manager locally to avoid circular dependency
    const { transactionLockManager } = await import('./TransactionLockManager');

    // Execute atomically with locks
    return await transactionLockManager.withMultipleLocks(
      [
        { type: 'portfolio', id: this.paperConfig.portfolioId },
        { type: 'order', id: orderId },
        { type: 'symbol', id: this.paperConfig.portfolioId, symbol },
      ],
      async () => {
        // Fetch the original order first
        const originalOrder = await paperTradingDatabase.getOrder(orderId);
        if (!originalOrder) {
          throw new Error(`Order ${orderId} not found`);
        }

        if (originalOrder.status === 'filled') {
          throw new Error(`Cannot edit filled order ${orderId}`);
        }

        if (originalOrder.status === 'cancelled') {
          throw new Error(`Cannot edit cancelled order ${orderId}`);
        }

        // Validate new parameters
        if (!type || !side || !amount) {
          throw new Error('Edit order requires type, side, and amount');
        }

        // Try to create new order FIRST (validates sufficient funds, etc.)
        let newOrder: Order;
        try {
          newOrder = await this.createOrder(symbol, type, side, amount, price, params);
        } catch (error) {
          // If new order fails, don't cancel the original
          throw new Error(`Failed to create replacement order: ${error instanceof Error ? error.message : String(error)}`);
        }

        // Only after new order succeeds, cancel the original
        try {
          await this.matchingEngine.cancelOrder(orderId);
        } catch (cancelError) {
          // If cancel fails after new order created, we have a problem
          // Try to cancel the new order to maintain atomicity
          console.error('[PaperTradingAdapter] Failed to cancel original order after creating new one:', cancelError);
          try {
            await this.matchingEngine.cancelOrder(newOrder.id);
          } catch (cleanupError) {
            console.error('[PaperTradingAdapter] Failed to cleanup new order:', cleanupError);
          }
          throw new Error(`Edit order failed: ${cancelError instanceof Error ? cancelError.message : String(cancelError)}`);
        }

        this.emit('order', { exchange: this.id, data: newOrder });
        return newOrder;
      }
    );
  }

  async cancelOrder(orderId: string, symbol: string): Promise<Order> {
    if (!this.initialized) {
      throw new Error('Paper trading adapter not initialized. Call connect() first.');
    }

    const canceledOrder = await this.matchingEngine.cancelOrder(orderId);
    this.emit('order', { exchange: this.id, data: canceledOrder });
    return canceledOrder as Order;
  }

  async cancelOrders(orderIds: string[], symbol?: string): Promise<Order[]> {
    const canceled: Order[] = [];
    for (const orderId of orderIds) {
      const order = await this.cancelOrder(orderId, symbol || '');
      canceled.push(order);
    }
    return canceled;
  }

  async cancelAllOrders(symbol?: string): Promise<Order[]> {
    if (!this.initialized) {
      throw new Error('Paper trading adapter not initialized. Call connect() first.');
    }

    const canceledOrders = await this.matchingEngine.cancelAllOrders(symbol);
    for (const order of canceledOrders) {
      this.emit('order', { exchange: this.id, data: order });
    }
    return canceledOrders as Order[];
  }

  async cancelAllOrdersAfter(timeout: number): Promise<any> {
    // Implement dead man's switch
    setTimeout(async () => {
      await this.cancelAllOrders();
    }, timeout);

    return { timeout, status: 'scheduled' };
  }

  async fetchOrder(orderId: string, symbol: string): Promise<Order> {
    const order = await paperTradingDatabase.getOrder(orderId);
    if (!order) {
      throw new Error(`Order ${orderId} not found`);
    }
    return order as Order;
  }

  async fetchOrdersByIds(orderIds: string[], symbol?: string): Promise<Order[]> {
    const orders: Order[] = [];
    for (const orderId of orderIds) {
      const order = await paperTradingDatabase.getOrder(orderId);
      if (order) orders.push(order as Order);
    }
    return orders;
  }

  async fetchOrderTrades(orderId: string, symbol: string): Promise<Trade[]> {
    const trades = await paperTradingDatabase.getOrderTrades(orderId);

    return trades.map(t => ({
      id: t.id,
      order: orderId,
      info: t,
      timestamp: new Date(t.timestamp).getTime(),
      datetime: t.timestamp,
      symbol: t.symbol,
      type: 'limit',
      side: t.side,
      takerOrMaker: t.isMaker ? 'maker' : 'taker',
      price: t.price,
      amount: t.quantity,
      cost: t.price * t.quantity,
      fee: {
        cost: t.fee,
        currency: (this.paperConfig.currency || 'USD') as string,
        rate: t.feeRate,
      },
    } as Trade));
  }

  async fetchOpenOrders(symbol?: string): Promise<Order[]> {
    const orders = await paperTradingDatabase.getPortfolioOrders(this.paperConfig.portfolioId);
    const openOrders = orders.filter(o => ['pending', 'triggered', 'partial'].includes(o.status)) as Order[];

    if (symbol) {
      return openOrders.filter(o => o.symbol === symbol);
    }

    return openOrders;
  }

  async fetchClosedOrders(symbol?: string, limit?: number): Promise<Order[]> {
    const orders = await paperTradingDatabase.getPortfolioOrders(this.paperConfig.portfolioId);

    let closedOrders = orders.filter(o => ['filled', 'canceled', 'rejected'].includes(o.status));

    if (symbol) {
      closedOrders = closedOrders.filter(o => o.symbol === symbol);
    }

    if (limit) {
      closedOrders = closedOrders.slice(0, limit);
    }

    return closedOrders as Order[];
  }

  async fetchMyTrades(symbol?: string, since?: number, limit?: number): Promise<Trade[]> {
    const trades = await paperTradingDatabase.getPortfolioTrades(this.paperConfig.portfolioId, limit);

    let filteredTrades = trades;

    if (symbol) {
      filteredTrades = filteredTrades.filter(t => t.symbol === symbol);
    }

    if (since) {
      filteredTrades = filteredTrades.filter(t => new Date(t.timestamp).getTime() >= since);
    }

    return filteredTrades.map(t => ({
      id: t.id,
      order: t.orderId,
      info: t,
      timestamp: new Date(t.timestamp).getTime(),
      datetime: t.timestamp,
      symbol: t.symbol,
      type: 'limit',
      side: t.side,
      takerOrMaker: t.isMaker ? 'maker' : 'taker',
      price: t.price,
      amount: t.quantity,
      cost: t.price * t.quantity,
      fee: {
        cost: t.fee,
        currency: (this.paperConfig.currency || 'USD') as string,
        rate: t.feeRate,
      },
    } as Trade));
  }

  // ============================================================================
  // ADVANCED ORDER TYPES
  // ============================================================================

  async createStopLossOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    stopPrice: number,
    limitPrice?: number
  ): Promise<Order> {
    return await this.createOrder(
      symbol,
      limitPrice ? 'stop_limit' : 'stop',
      side,
      amount,
      limitPrice,
      { stopPrice }
    );
  }

  async createTakeProfitOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    stopPrice: number,
    limitPrice?: number
  ): Promise<Order> {
    return await this.createOrder(
      symbol,
      limitPrice ? 'stop_limit' : 'stop',
      side,
      amount,
      limitPrice,
      { stopPrice }
    );
  }

  async createTrailingStopOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    trailingPercent: number
  ): Promise<Order> {
    return await this.createOrder(symbol, 'trailing_stop', side, amount, undefined, { trailingPercent });
  }

  async createIcebergOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    price: number,
    visibleAmount: number
  ): Promise<Order> {
    return await this.createOrder(symbol, 'iceberg', side, amount, price, { icebergQty: visibleAmount });
  }

  async createMarketOrderWithCost(symbol: string, side: OrderSide, cost: number): Promise<Order> {
    const ticker = await this.fetchTicker(symbol);
    const price = side === 'buy' ? ticker.ask || ticker.last || 0 : ticker.bid || ticker.last || 0;
    const amount = cost / price;

    return await this.createOrder(symbol, 'market', side, amount);
  }

  async createPostOnlyOrder(symbol: string, side: OrderSide, amount: number, price: number): Promise<Order> {
    return await this.createOrder(symbol, 'limit', side, amount, price, { postOnly: true });
  }

  async createReduceOnlyOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number
  ): Promise<Order> {
    return await this.createOrder(symbol, type, side, amount, price, { reduceOnly: true });
  }

  // ============================================================================
  // MARGIN & LEVERAGE (Paper trading specific)
  // ============================================================================

  async setLeverage(symbol: string, leverage: number): Promise<any> {
    // Update portfolio default leverage
    const portfolio = await paperTradingDatabase.getPortfolio(this.paperConfig.portfolioId);
    if (portfolio) {
      this.paperConfig.defaultLeverage = leverage;
    }

    return { symbol, leverage };
  }

  async setMarginMode(symbol: string, marginMode: 'cross' | 'isolated'): Promise<any> {
    this.paperConfig.marginMode = marginMode;
    return { symbol, marginMode };
  }

  // ============================================================================
  // DEPOSITS & WITHDRAWALS (Not applicable for paper trading)
  // ============================================================================

  async fetchDeposits(code?: string, since?: number, limit?: number): Promise<any> {
    return []; // No deposits in paper trading
  }

  async fetchWithdrawals(code?: string, since?: number, limit?: number): Promise<any> {
    return []; // No withdrawals in paper trading
  }

  async fetchDepositAddress(code: string, params?: any): Promise<any> {
    throw new Error('Deposits not supported in paper trading');
  }

  async createDepositAddress(code: string, params?: any): Promise<any> {
    throw new Error('Deposits not supported in paper trading');
  }

  async fetchDepositMethods(code?: string): Promise<any> {
    return [];
  }

  async withdraw(code: string, amount: number, address: string, tag?: string, params?: any): Promise<any> {
    throw new Error('Withdrawals not supported in paper trading');
  }

  // ============================================================================
  // TRANSFERS
  // ============================================================================

  async transfer(code: string, amount: number, fromAccount: string, toAccount: string): Promise<any> {
    // Paper trading doesn't have sub-accounts, but we'll return success for compatibility
    return {
      info: {},
      id: `transfer_${Date.now()}`,
      timestamp: Date.now(),
      datetime: new Date().toISOString(),
      currency: code,
      amount,
      fromAccount,
      toAccount,
      status: 'ok',
    };
  }

  async transferOut(code: string, amount: number, params?: any): Promise<any> {
    return await this.transfer(code, amount, 'main', 'external');
  }

  // ============================================================================
  // CAPABILITIES
  // ============================================================================

  getCapabilities(): ExchangeCapabilities {
    const realCapabilities = ('getCapabilities' in this.realAdapter && typeof this.realAdapter.getCapabilities === 'function')
      ? this.realAdapter.getCapabilities()
      : {} as ExchangeCapabilities;

    return {
      ...realCapabilities,
      // Paper trading specific overrides
      spot: this.paperConfig.enableMarginTrading ? true : realCapabilities.spot,
      margin: this.paperConfig.enableMarginTrading || false,
      futures: this.paperConfig.enableMarginTrading || false,
      leverageTrading: this.paperConfig.enableMarginTrading || false,
      multiAccount: false,
      subAccounts: false,
    };
  }

  // ============================================================================
  // WEBSOCKET (Optional - delegate to real adapter if available)
  // ============================================================================

  async *watchTicker(symbol: string): AsyncGenerator<Ticker> {
    if (this.realAdapter.watchTicker) {
      yield* this.realAdapter.watchTicker(symbol);
    } else {
      throw new Error('WebSocket not supported by underlying adapter');
    }
  }

  async *watchOrderBook(symbol: string, limit?: number): AsyncGenerator<OrderBook> {
    if (this.realAdapter.watchOrderBook) {
      yield* this.realAdapter.watchOrderBook(symbol, limit);
    } else {
      throw new Error('WebSocket not supported by underlying adapter');
    }
  }

  async *watchTrades(symbol: string): AsyncGenerator<Trade[]> {
    if (this.realAdapter.watchTrades) {
      yield* this.realAdapter.watchTrades(symbol);
    } else {
      throw new Error('WebSocket not supported by underlying adapter');
    }
  }

  async *watchBalance(): AsyncGenerator<Balance> {
    // Emit paper trading balance periodically
    while (true) {
      const balance = await this.fetchBalance();
      yield balance;
      await new Promise(resolve => setTimeout(resolve, 1000));
    }
  }

  async *watchOrders(symbol?: string): AsyncGenerator<Order[]> {
    // Emit paper trading orders periodically
    while (true) {
      const orders = await this.fetchOpenOrders(symbol);
      yield orders;
      await new Promise(resolve => setTimeout(resolve, 1000));
    }
  }

  // ============================================================================
  // UTILITIES
  // ============================================================================

  /**
   * Reset portfolio to initial state
   */
  async resetPortfolio(): Promise<void> {
    // Cancel all orders
    await this.cancelAllOrders();

    // Close all positions
    const positions = await paperTradingDatabase.getPortfolioPositions(this.paperConfig.portfolioId, 'open');
    for (const position of positions) {
      const closeSide = position.side === 'long' ? 'sell' : 'buy';
      await this.createOrder(position.symbol, 'market', closeSide, position.quantity, undefined, { reduceOnly: true });
    }

    // Reset balance
    await paperTradingDatabase.updatePortfolioBalance(
      this.paperConfig.portfolioId,
      this.paperConfig.initialBalance
    );
  }

  /**
   * Get paper trading statistics (improved with comprehensive metrics)
   */
  async getStatistics(): Promise<any> {
    const { StatisticsCalculator } = await import('./StatisticsCalculator');
    const statsCalculator = new StatisticsCalculator();

    const portfolio = await paperTradingDatabase.getPortfolio(this.paperConfig.portfolioId);
    const positions = await paperTradingDatabase.getPortfolioPositions(this.paperConfig.portfolioId);
    const totalValue = await this.balanceManager.calculateTotalValue(this.paperConfig.portfolioId);

    // Get comprehensive statistics
    const performanceSummary = await statsCalculator.getPerformanceSummary(
      this.paperConfig.portfolioId,
      this.paperConfig.initialBalance
    );

    return {
      // Portfolio info
      portfolioId: this.paperConfig.portfolioId,
      portfolioName: this.paperConfig.portfolioName,
      initialBalance: this.paperConfig.initialBalance,
      currentBalance: portfolio?.currentBalance || 0,
      totalValue,

      // P&L metrics
      totalPnL: performanceSummary.stats.totalPnL,
      realizedPnL: performanceSummary.stats.realizedPnL,
      unrealizedPnL: performanceSummary.stats.unrealizedPnL,
      returnPercent: performanceSummary.returnPercent.toFixed(2),

      // Trade metrics
      totalTrades: performanceSummary.stats.totalTrades,
      winningTrades: performanceSummary.stats.winningTrades,
      losingTrades: performanceSummary.stats.losingTrades,
      winRate: performanceSummary.stats.winRate.toFixed(2),

      // Position metrics
      openPositions: positions.filter(p => p.status === 'open').length,
      closedPositions: positions.filter(p => p.status === 'closed').length,

      // Performance metrics
      averageWin: performanceSummary.stats.averageWin.toFixed(2),
      averageLoss: performanceSummary.stats.averageLoss.toFixed(2),
      largestWin: performanceSummary.stats.largestWin.toFixed(2),
      largestLoss: performanceSummary.stats.largestLoss.toFixed(2),
      profitFactor: performanceSummary.stats.profitFactor.toFixed(2),
      riskRewardRatio: performanceSummary.riskRewardRatio.toFixed(2),

      // Advanced metrics
      sharpeRatio: performanceSummary.stats.sharpeRatio?.toFixed(2) || null,
      maxDrawdown: performanceSummary.stats.maxDrawdown?.toFixed(2) || null,
      expectancy: performanceSummary.expectancy.toFixed(2),
      kellyCriterion: (performanceSummary.kellyCriterion * 100).toFixed(2), // as percentage

      // Timing metrics
      avgHoldingPeriod: performanceSummary.stats.avgHoldingPeriod?.toFixed(0) || null, // in minutes

      // Fees
      totalFees: performanceSummary.stats.totalFees.toFixed(2),
    };
  }

  /**
   * Cleanup resources
   */
  async destroy(): Promise<void> {
    this.matchingEngine.destroy();
    await this.disconnect();
  }
}
