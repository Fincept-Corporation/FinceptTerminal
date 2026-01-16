/**
 * Paper Trading Adapter
 *
 * Thin wrapper that uses Rust backend for all business logic.
 * Implements IExchangeAdapter interface for compatibility with BrokerContext.
 */

import type {
  IExchangeAdapter,
  Order,
  Position,
  Balance,
  Ticker,
  OrderBook,
  Trade,
  OHLCV,
  Market,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCredentials,
  ExchangeCapabilities,
  ExchangeEvent,
  ExchangeEventData,
} from '../brokers/crypto/types';
import type { PaperTradingConfig, PaperTradingStats } from './types';
import { paperTradingDatabase } from './PaperTradingDatabase';

export class PaperTradingAdapter implements IExchangeAdapter {
  // Required interface properties
  public id: string;
  public name: string;

  private config: PaperTradingConfig;
  private realAdapter: IExchangeAdapter;
  private connected: boolean = false;
  private authenticated: boolean = false;
  private priceUpdateInterval: NodeJS.Timeout | null = null;
  private eventListeners: Map<ExchangeEvent, Set<(data: ExchangeEventData) => void>> = new Map();

  constructor(config: PaperTradingConfig, realAdapter: IExchangeAdapter) {
    this.config = config;
    this.realAdapter = realAdapter;
    this.id = `paper_${config.provider}`;
    this.name = `Paper Trading (${config.provider})`;
  }

  // ============================================================================
  // CONNECTION & AUTHENTICATION
  // ============================================================================

  async connect(): Promise<void> {
    // Ensure portfolio exists in Rust backend
    try {
      const existing = await paperTradingDatabase.getPortfolio(this.config.portfolioId);
      if (!existing) {
        await paperTradingDatabase.createPortfolio({
          id: this.config.portfolioId,
          name: this.config.portfolioName,
          provider: this.config.provider,
          initialBalance: this.config.initialBalance,
          currency: this.config.currency || 'USD',
          marginMode: this.config.marginMode || 'cross',
          leverage: this.config.defaultLeverage || 1,
        });
      }
    } catch (error) {
      console.error('[PaperTradingAdapter] Failed to create portfolio:', error);
    }

    // Start price update loop if enabled
    if (this.config.enableRealtimeUpdates !== false) {
      this.startPriceUpdates();
    }

    this.connected = true;
    this.authenticated = true; // Paper trading is always "authenticated"
  }

  async disconnect(): Promise<void> {
    this.stopPriceUpdates();
    this.connected = false;
  }

  isConnected(): boolean {
    return this.connected;
  }

  async authenticate(_credentials: ExchangeCredentials): Promise<void> {
    // Paper trading doesn't need real authentication
    this.authenticated = true;
  }

  isAuthenticated(): boolean {
    return this.authenticated;
  }

  // ============================================================================
  // PRICE UPDATES
  // ============================================================================

  private startPriceUpdates(): void {
    const interval = this.config.priceUpdateInterval || 1000;
    this.priceUpdateInterval = setInterval(async () => {
      try {
        // Get all open positions
        const positions = await paperTradingDatabase.getPortfolioPositions(
          this.config.portfolioId,
          'open'
        );

        // Update each position with current price
        for (const pos of positions) {
          try {
            const ticker = await this.realAdapter.fetchTicker(pos.symbol);
            if (ticker?.last) {
              const pnl = pos.side === 'long'
                ? (ticker.last - pos.entryPrice) * pos.quantity
                : (pos.entryPrice - ticker.last) * pos.quantity;

              await paperTradingDatabase.updatePosition(pos.id, {
                currentPrice: ticker.last,
                unrealizedPnl: pnl,
              });
            }
          } catch {
            // Ignore individual price fetch errors
          }
        }
      } catch (error) {
        console.error('[PaperTradingAdapter] Price update error:', error);
      }
    }, interval);
  }

  private stopPriceUpdates(): void {
    if (this.priceUpdateInterval) {
      clearInterval(this.priceUpdateInterval);
      this.priceUpdateInterval = null;
    }
  }

  // ============================================================================
  // MARKET DATA (delegated to real adapter)
  // ============================================================================

  async fetchMarkets(): Promise<Market[]> {
    return this.realAdapter.fetchMarkets();
  }

  async fetchTicker(symbol: string): Promise<Ticker> {
    return this.realAdapter.fetchTicker(symbol);
  }

  async fetchTickers(symbols?: string[]): Promise<Record<string, Ticker>> {
    return this.realAdapter.fetchTickers(symbols);
  }

  async fetchOrderBook(symbol: string, limit?: number): Promise<OrderBook> {
    return this.realAdapter.fetchOrderBook(symbol, limit);
  }

  async fetchTrades(symbol: string, limit?: number): Promise<Trade[]> {
    return this.realAdapter.fetchTrades(symbol, limit);
  }

  async fetchOHLCV(symbol: string, timeframe: string, limit?: number): Promise<OHLCV[]> {
    return this.realAdapter.fetchOHLCV(symbol, timeframe, limit);
  }

  // ============================================================================
  // BALANCE
  // ============================================================================

  async fetchBalance(): Promise<Balance> {
    const portfolio = await paperTradingDatabase.getPortfolio(this.config.portfolioId);
    const currency = this.config.currency || 'USD';

    if (!portfolio) {
      return {
        free: { [currency]: this.config.initialBalance } as any,
        used: { [currency]: 0 } as any,
        total: { [currency]: this.config.initialBalance } as any,
        info: {},
      } as unknown as Balance;
    }

    const stats = await paperTradingDatabase.getPortfolioStats(this.config.portfolioId);

    return {
      free: { [currency]: stats.available_margin } as any,
      used: { [currency]: stats.blocked_margin } as any,
      total: { [currency]: stats.total_value } as any,
      info: {
        realizedPnl: stats.realized_pnl,
        unrealizedPnl: stats.unrealized_pnl,
      },
    } as unknown as Balance;
  }

  // ============================================================================
  // POSITIONS
  // ============================================================================

  async fetchPositions(symbols?: string[]): Promise<Position[]> {
    const positions = await paperTradingDatabase.getPortfolioPositions(
      this.config.portfolioId,
      'open'
    );

    return positions.map(pos => ({
      id: pos.id,
      symbol: pos.symbol,
      timestamp: new Date(pos.openedAt).getTime(),
      datetime: pos.openedAt,
      isolated: pos.marginMode === 'isolated',
      hedged: false,
      side: pos.side,
      contracts: pos.quantity,
      contractSize: 1,
      entryPrice: pos.entryPrice,
      markPrice: pos.currentPrice || pos.entryPrice,
      notional: pos.positionValue,
      leverage: pos.leverage,
      collateral: pos.positionValue / pos.leverage,
      initialMargin: pos.positionValue / pos.leverage,
      maintenanceMargin: (pos.positionValue / pos.leverage) * 0.5,
      initialMarginPercentage: 1 / pos.leverage,
      maintenanceMarginPercentage: 0.5 / pos.leverage,
      unrealizedPnl: pos.unrealizedPnl || 0,
      liquidationPrice: pos.liquidationPrice || undefined,
      marginMode: pos.marginMode,
      marginRatio: undefined,
      percentage: pos.unrealizedPnl ? (pos.unrealizedPnl / pos.entryPrice / pos.quantity) * 100 : 0,
      info: pos,
    })) as Position[];
  }

  // ============================================================================
  // ORDERS
  // ============================================================================

  async createOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number,
    params?: OrderParams
  ): Promise<Order> {
    const orderId = crypto.randomUUID();

    // Normalize order type for paper trading
    const normalizedType = this.normalizeOrderType(type);

    // Get current market price for market orders
    let executionPrice = price;
    if (normalizedType === 'market') {
      const ticker = await this.realAdapter.fetchTicker(symbol);
      executionPrice = side === 'buy' ? ticker.ask || ticker.last : ticker.bid || ticker.last;

      // Apply slippage
      const slippage = this.config.slippage?.market || 0.001;
      if (executionPrice) {
        executionPrice = side === 'buy'
          ? executionPrice * (1 + slippage)
          : executionPrice * (1 - slippage);
      }
    }

    // Calculate order value and check margin
    const orderValue = amount * (executionPrice || 0);
    const availableMargin = await paperTradingDatabase.getAvailableMargin(this.config.portfolioId);

    if (orderValue > availableMargin) {
      throw new Error(`Insufficient margin. Required: ${orderValue}, Available: ${availableMargin}`);
    }

    // Create order in database
    await paperTradingDatabase.createOrder({
      id: orderId,
      portfolioId: this.config.portfolioId,
      symbol,
      side,
      type: normalizedType,
      quantity: amount,
      price: executionPrice || null,
      timeInForce: params?.timeInForce || 'GTC',
    });

    // For market orders, execute immediately
    if (normalizedType === 'market' && executionPrice) {
      const fee = orderValue * this.config.fees.taker;

      // Fill order via Rust backend (creates trade, updates position, deducts fees)
      await paperTradingDatabase.fillOrder(
        orderId,
        executionPrice,
        amount,
        fee,
        this.config.fees.taker
      );
    } else if (normalizedType === 'limit' && executionPrice) {
      // Block margin for limit orders
      await paperTradingDatabase.createMarginBlock({
        id: crypto.randomUUID(),
        portfolioId: this.config.portfolioId,
        orderId,
        symbol,
        blockedAmount: orderValue,
      });
    }

    // Return order
    const order = await paperTradingDatabase.getOrder(orderId);
    return this.mapOrder(order!);
  }

  async cancelOrder(orderId: string, symbol: string): Promise<Order> {
    const order = await paperTradingDatabase.getOrder(orderId);
    if (!order) {
      throw new Error(`Order ${orderId} not found`);
    }

    // Release margin block
    await paperTradingDatabase.deleteMarginBlock(orderId);

    // Update order status
    await paperTradingDatabase.updateOrder(orderId, { status: 'canceled' });

    const updatedOrder = await paperTradingDatabase.getOrder(orderId);
    return this.mapOrder(updatedOrder!);
  }

  async cancelAllOrders(symbol?: string): Promise<Order[]> {
    const pendingOrders = await paperTradingDatabase.getPendingOrders(this.config.portfolioId);
    const cancelledOrders: Order[] = [];

    for (const order of pendingOrders) {
      if (!symbol || order.symbol === symbol) {
        const cancelled = await this.cancelOrder(order.id, order.symbol);
        cancelledOrders.push(cancelled);
      }
    }

    return cancelledOrders;
  }

  async fetchOrder(orderId: string, symbol: string): Promise<Order> {
    const order = await paperTradingDatabase.getOrder(orderId);
    if (!order) {
      throw new Error(`Order ${orderId} not found`);
    }
    return this.mapOrder(order);
  }

  async fetchOpenOrders(symbol?: string): Promise<Order[]> {
    const orders = await paperTradingDatabase.getPendingOrders(this.config.portfolioId);
    return orders.map(o => this.mapOrder(o));
  }

  async fetchClosedOrders(symbol?: string, limit?: number): Promise<Order[]> {
    const orders = await paperTradingDatabase.getPortfolioOrders(this.config.portfolioId, 'filled');
    return orders.slice(0, limit).map(o => this.mapOrder(o));
  }

  // ============================================================================
  // TRADES
  // ============================================================================

  async fetchMyTrades(symbol?: string, since?: number, limit?: number): Promise<Trade[]> {
    const trades = await paperTradingDatabase.getPortfolioTrades(this.config.portfolioId, limit);
    return trades.map(t => ({
      id: t.id,
      order: t.orderId,
      symbol: t.symbol,
      side: t.side as OrderSide,
      type: t.isMaker ? 'maker' : 'taker',
      takerOrMaker: t.isMaker ? 'maker' : 'taker',
      price: t.price,
      amount: t.quantity,
      cost: t.price * t.quantity,
      fee: {
        cost: t.fee,
        currency: this.config.currency || 'USD',
        rate: t.feeRate,
      },
      timestamp: new Date(t.timestamp).getTime(),
      datetime: t.timestamp,
      info: t,
    })) as Trade[];
  }

  // ============================================================================
  // EVENT SYSTEM
  // ============================================================================

  on(event: ExchangeEvent, callback: (data: ExchangeEventData) => void): void {
    if (!this.eventListeners.has(event)) {
      this.eventListeners.set(event, new Set());
    }
    this.eventListeners.get(event)!.add(callback);
  }

  off(event: ExchangeEvent, callback: (data: ExchangeEventData) => void): void {
    const listeners = this.eventListeners.get(event);
    if (listeners) {
      listeners.delete(callback);
    }
  }

  emit(event: ExchangeEvent, data: ExchangeEventData): void {
    const listeners = this.eventListeners.get(event);
    if (listeners) {
      listeners.forEach(callback => callback(data));
    }
  }

  // ============================================================================
  // CAPABILITIES
  // ============================================================================

  getCapabilities(): ExchangeCapabilities {
    return {
      spot: true,
      margin: this.config.enableMarginTrading || false,
      futures: false,
      options: false,
      swap: false,
      supportedOrderTypes: ['market', 'limit', 'stop', 'stop_limit'],
      supportedTimeInForce: ['GTC', 'IOC', 'FOK'],
      leverageTrading: this.config.enableMarginTrading || false,
      stopOrders: true,
      conditionalOrders: false,
      algoOrders: false,
      realtimeData: true,
      historicalData: true,
      websocketSupport: false,
      multiAccount: false,
      subAccounts: false,
      portfolioMargin: false,
      restApi: true,
      websocketApi: false,
      fixApi: false,
      rateLimits: {
        public: 1000,
        private: 100,
        trading: 50,
      },
    };
  }

  // ============================================================================
  // STATISTICS
  // ============================================================================

  async getStatistics(): Promise<PaperTradingStats> {
    const stats = await paperTradingDatabase.getPortfolioStats(this.config.portfolioId);
    const trades = await paperTradingDatabase.getPortfolioTrades(this.config.portfolioId);

    // Calculate win/loss stats
    let totalFees = 0;

    for (const trade of trades) {
      totalFees += trade.fee;
    }

    return {
      portfolioId: this.config.portfolioId,
      totalTrades: stats.total_trades,
      winningTrades: 0,
      losingTrades: 0,
      winRate: 0,
      totalPnL: stats.total_pnl,
      realizedPnL: stats.realized_pnl,
      unrealizedPnL: stats.unrealized_pnl,
      totalFees,
      averageWin: 0,
      averageLoss: 0,
      largestWin: 0,
      largestLoss: 0,
      profitFactor: 0,
      sharpeRatio: null,
      maxDrawdown: null,
      avgHoldingPeriod: null,
    };
  }

  // ============================================================================
  // ACCOUNT MANAGEMENT
  // ============================================================================

  async resetAccount(): Promise<void> {
    await paperTradingDatabase.resetPortfolio(
      this.config.portfolioId,
      this.config.initialBalance
    );
  }

  // ============================================================================
  // HELPERS
  // ============================================================================

  private normalizeOrderType(type: OrderType): 'market' | 'limit' | 'stop' | 'stop_limit' {
    switch (type) {
      case 'market':
        return 'market';
      case 'limit':
        return 'limit';
      case 'stop':
      case 'stop_market':
        return 'stop';
      case 'stop_limit':
        return 'stop_limit';
      default:
        return 'market';
    }
  }

  private mapOrder(order: any): Order {
    return {
      id: order.id,
      clientOrderId: order.clientOrderId,
      timestamp: new Date(order.createdAt || order.created_at).getTime(),
      datetime: order.createdAt || order.created_at,
      lastTradeTimestamp: order.filledAt ? new Date(order.filledAt).getTime() : undefined,
      symbol: order.symbol,
      type: order.type || order.order_type,
      side: order.side,
      price: order.price || 0,
      amount: order.amount || order.quantity,
      cost: (order.average || order.avg_fill_price || 0) * (order.filled || order.filled_quantity || 0),
      average: order.average || order.avg_fill_price,
      filled: order.filled || order.filled_quantity || 0,
      remaining: order.remaining || (order.quantity - (order.filled_quantity || 0)),
      status: order.status,
      fee: undefined,
      trades: [],
      reduceOnly: order.reduce_only || false,
      postOnly: order.post_only || false,
      info: order,
    } as unknown as Order;
  }

  // Get provider name
  getProvider(): string {
    return this.config.provider;
  }

  // Get portfolio ID
  getPortfolioId(): string {
    return this.config.portfolioId;
  }
}

/**
 * Create a paper trading adapter
 */
export function createPaperTradingAdapter(
  config: PaperTradingConfig,
  realAdapter: IExchangeAdapter
): PaperTradingAdapter {
  return new PaperTradingAdapter(config, realAdapter);
}
