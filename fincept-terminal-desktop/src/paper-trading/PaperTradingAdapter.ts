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

      // Connect to real exchange for market data
      if (!this.realAdapter.isConnected()) {
        console.log('[PaperTradingAdapter] Connecting real adapter...');
        await this.realAdapter.connect();
        console.log('[PaperTradingAdapter] Real adapter connected successfully');
      } else {
        console.log('[PaperTradingAdapter] Real adapter already connected, skipping');
      }

      // Initialize or load portfolio
      console.log('[PaperTradingAdapter] Initializing portfolio...');
      await this.initializePortfolio();
      console.log('[PaperTradingAdapter] Portfolio initialized');

      // Start order monitoring
      if (this.paperConfig.enableRealtimeUpdates !== false) {
        console.log('[PaperTradingAdapter] Starting order monitoring...');
        this.matchingEngine.startMonitoring(this.paperConfig.priceUpdateInterval || 1000);
      }

      this._isConnected = true;
      this._isAuthenticated = true; // Paper trading doesn't require real auth
      this.initialized = true;

      console.log('[PaperTradingAdapter] ✓ Connection complete!');
      this.emit('connected', { exchange: this.id });
      this.emit('authenticated', { exchange: this.id });
    } catch (error) {
      this._isConnected = false;
      console.error('[PaperTradingAdapter] ✗ Connection failed:', error);
      throw this.handleError(error);
    }
  }

  async disconnect(): Promise<void> {
    this.matchingEngine.stopMonitoring();
    this._isConnected = false;
    this._isAuthenticated = false;
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

      console.log('[PaperTradingAdapter] ✓ Account reset complete!');
      this.emit('reset', { exchange: this.id, portfolioId });
    } catch (error) {
      console.error('[PaperTradingAdapter] ✗ Reset failed:', error);
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
    const existing = await paperTradingDatabase.getPortfolio(this.paperConfig.portfolioId);

    if (!existing) {
      // Create new portfolio
      await paperTradingDatabase.createPortfolio({
        id: this.paperConfig.portfolioId,
        name: this.paperConfig.portfolioName,
        provider: this.paperConfig.provider,
        initialBalance: this.paperConfig.initialBalance,
        currency: this.paperConfig.currency,
        marginMode: this.paperConfig.marginMode,
        leverage: this.paperConfig.defaultLeverage,
      });
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

    console.log('[PaperTradingAdapter] fetchPositions called');
    console.log('[PaperTradingAdapter] Portfolio ID:', this.paperConfig.portfolioId);

    const positions = await paperTradingDatabase.getPortfolioPositions(this.paperConfig.portfolioId, 'open');
    console.log('[PaperTradingAdapter] Raw open positions from DB:', positions.length);

    // Also check ALL positions to see if there are closed ones
    const allPositions = await paperTradingDatabase.getPortfolioPositions(this.paperConfig.portfolioId);
    console.log('[PaperTradingAdapter] Total positions (open + closed):', allPositions.length);
    allPositions.forEach(p => {
      console.log(`  Position: ${p.symbol} ${p.side} ${p.quantity} @ ${p.entryPrice} - Status: ${p.status}`);
    });

    // Update positions with current prices
    for (const p of positions) {
      try {
        const ticker = await this.realAdapter.fetchTicker(p.symbol);
        const currentPrice = ticker.last || ticker.close || p.currentPrice || p.entryPrice;

        const unrealizedPnl = p.side === 'long'
          ? (currentPrice - p.entryPrice) * p.quantity
          : (p.entryPrice - currentPrice) * p.quantity;

        await paperTradingDatabase.updatePosition(p.id, {
          currentPrice,
          unrealizedPnl,
        });

        p.currentPrice = currentPrice;
        p.unrealizedPnl = unrealizedPnl;
      } catch (err) {
        console.warn(`[PaperTradingAdapter] Failed to update price for ${p.symbol}`);
      }
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
    // For paper trading, we'll cancel and recreate the order
    await this.cancelOrder(orderId, symbol);

    if (!type || !side || !amount) {
      throw new Error('Edit order requires type, side, and amount');
    }

    return await this.createOrder(symbol, type, side, amount, price, params);
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

    const openOrders = orders.filter(o => ['pending', 'triggered', 'partial'].includes(o.status));

    if (symbol) {
      return openOrders.filter(o => o.symbol === symbol) as Order[];
    }

    return openOrders as Order[];
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
   * Get paper trading statistics
   */
  async getStatistics(): Promise<any> {
    const portfolio = await paperTradingDatabase.getPortfolio(this.paperConfig.portfolioId);
    const positions = await paperTradingDatabase.getPortfolioPositions(this.paperConfig.portfolioId);
    const trades = await paperTradingDatabase.getPortfolioTrades(this.paperConfig.portfolioId);
    const pnl = await this.balanceManager.calculatePortfolioPnL(this.paperConfig.portfolioId);
    const totalValue = await this.balanceManager.calculateTotalValue(this.paperConfig.portfolioId);

    const winningTrades = trades.filter(t => {
      // Calculate P&L for each trade (simplified)
      return t.side === 'sell'; // Assume sells close positions
    });

    return {
      portfolioId: this.paperConfig.portfolioId,
      portfolioName: this.paperConfig.portfolioName,
      initialBalance: this.paperConfig.initialBalance,
      currentBalance: portfolio?.currentBalance || 0,
      totalValue,
      totalPnL: pnl.totalPnL,
      realizedPnL: pnl.realizedPnL,
      unrealizedPnL: pnl.unrealizedPnL,
      totalTrades: trades.length,
      openPositions: positions.filter(p => p.status === 'open').length,
      closedPositions: positions.filter(p => p.status === 'closed').length,
      returnPercent: ((pnl.totalPnL / this.paperConfig.initialBalance) * 100).toFixed(2),
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
