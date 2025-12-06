/**
 * Base Exchange Adapter
 *
 * Abstract base class for all exchange adapters
 * Provides common functionality and enforces consistent interface
 */

import ccxt, { Exchange, Market, Ticker, OrderBook, Trade, OHLCV, Balance, Order, Position } from 'ccxt';
import {
  IExchangeAdapter,
  ExchangeCredentials,
  ExchangeConfig,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCapabilities,
  ExchangeError,
  AuthenticationError,
  RateLimitError,
  ExchangeEvent,
  ExchangeEventData,
} from './types';

export abstract class BaseExchangeAdapter implements IExchangeAdapter {
  protected exchange: Exchange;
  protected _isConnected: boolean = false;
  protected _isAuthenticated: boolean = false;
  protected eventListeners: Map<ExchangeEvent, Set<(data: ExchangeEventData) => void>> = new Map();

  public readonly id: string;
  public readonly name: string;

  constructor(config: ExchangeConfig) {
    this.id = config.exchange;

    // Initialize CCXT exchange
    const ExchangeClass = ccxt[config.exchange as keyof typeof ccxt] as typeof Exchange;
    if (!ExchangeClass) {
      throw new Error(`Exchange "${config.exchange}" not supported by CCXT`);
    }

    this.exchange = new ExchangeClass({
      apiKey: config.credentials?.apiKey,
      secret: config.credentials?.secret,
      password: config.credentials?.password,
      uid: config.credentials?.uid,
      enableRateLimit: config.enableRateLimit ?? true,
      timeout: config.timeout ?? 30000,
      ...(config.sandbox && { sandbox: true }),
      ...config.options,
    });

    this.name = this.exchange.name as string;
  }

  // ============================================================================
  // CONNECTION MANAGEMENT
  // ============================================================================

  async connect(): Promise<void> {
    try {
      await this.exchange.loadMarkets();
      this._isConnected = true;
      this.emit('connected', { exchange: this.id });
    } catch (error) {
      this._isConnected = false;
      throw this.handleError(error);
    }
  }

  async disconnect(): Promise<void> {
    this._isConnected = false;
    this._isAuthenticated = false;
    this.emit('disconnected', { exchange: this.id });
  }

  isConnected(): boolean {
    return this._isConnected;
  }

  // ============================================================================
  // AUTHENTICATION
  // ============================================================================

  async authenticate(credentials: ExchangeCredentials): Promise<void> {
    try {
      this.exchange.apiKey = credentials.apiKey;
      this.exchange.secret = credentials.secret;
      if (credentials.password) this.exchange.password = credentials.password;
      if (credentials.uid) this.exchange.uid = credentials.uid;

      // Test authentication by fetching balance
      await this.exchange.fetchBalance();
      this._isAuthenticated = true;
      this.emit('authenticated', { exchange: this.id });
    } catch (error) {
      this._isAuthenticated = false;
      throw new AuthenticationError(
        'Authentication failed',
        this.id,
        error
      );
    }
  }

  isAuthenticated(): boolean {
    return this._isAuthenticated;
  }

  // ============================================================================
  // MARKET DATA
  // ============================================================================

  async fetchMarkets(): Promise<Market[]> {
    try {
      await this.ensureConnected();
      return await this.exchange.fetchMarkets();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchStatus(): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchStatus']) {
        return { status: 'ok', updated: Date.now() };
      }
      return await this.exchange.fetchStatus();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchCurrencies(): Promise<any> {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchCurrencies']) {
        return {};
      }
      return await this.exchange.fetchCurrencies();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchTime(): Promise<number> {
    try {
      await this.ensureConnected();
      return await this.exchange.fetchTime() as number;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchTicker(symbol: string): Promise<Ticker> {
    try {
      await this.ensureConnected();
      return await this.exchange.fetchTicker(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchTickers(symbols?: string[]): Promise<Record<string, Ticker>> {
    try {
      await this.ensureConnected();
      return await this.exchange.fetchTickers(symbols);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOrderBook(symbol: string, limit?: number): Promise<OrderBook> {
    try {
      await this.ensureConnected();
      return await this.exchange.fetchOrderBook(symbol, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchTrades(symbol: string, limit?: number): Promise<Trade[]> {
    try {
      await this.ensureConnected();
      return await this.exchange.fetchTrades(symbol, undefined, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOHLCV(
    symbol: string,
    timeframe: string = '1m',
    limit?: number
  ): Promise<OHLCV[]> {
    try {
      await this.ensureConnected();
      return await this.exchange.fetchOHLCV(symbol, timeframe, undefined, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ACCOUNT & BALANCE
  // ============================================================================

  async fetchBalance(): Promise<Balance> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchBalance() as unknown as Balance;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchLedger(code?: string, since?: number, limit?: number): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchLedger']) {
        throw new Error('fetchLedger not supported by this exchange');
      }
      return await this.exchange.fetchLedger(code, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchTradingFee(symbol: string): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchTradingFee']) {
        const fees = await this.exchange.fetchTradingFees();
        return fees[symbol] || null;
      }
      return await this.exchange.fetchTradingFee(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchPositions(symbols?: string[]): Promise<Position[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchPositions']) {
        return [];
      }
      return await this.exchange.fetchPositions(symbols);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // TRADING
  // ============================================================================

  async createOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number,
    params?: OrderParams
  ): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.createOrder(symbol, type, side, amount, price, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async createOrders(orders: Array<{
    symbol: string;
    type: OrderType;
    side: OrderSide;
    amount: number;
    price?: number;
    params?: OrderParams;
  }>): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createOrders']) {
        // Fallback: create orders one by one
        const results: Order[] = [];
        for (const order of orders) {
          const result = await this.createOrder(
            order.symbol,
            order.type,
            order.side,
            order.amount,
            order.price,
            order.params
          );
          results.push(result);
        }
        return results;
      }
      return await this.exchange.createOrders(orders);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async createMarketOrderWithCost(
    symbol: string,
    side: OrderSide,
    cost: number,
    params?: OrderParams
  ): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createMarketOrderWithCost']) {
        throw new Error('createMarketOrderWithCost not supported');
      }
      return await (this.exchange as any).createMarketOrderWithCost(symbol, side, cost, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async createMarketBuyOrderWithCost(
    symbol: string,
    cost: number,
    params?: OrderParams
  ): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createMarketBuyOrderWithCost']) {
        return await this.createMarketOrderWithCost(symbol, 'buy', cost, params);
      }
      return await (this.exchange as any).createMarketBuyOrderWithCost(symbol, cost, params);
    } catch (error) {
      throw this.handleError(error);
    }
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
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['editOrder']) {
        throw new Error('editOrder not supported by this exchange');
      }
      return await this.exchange.editOrder(orderId, symbol, type as any, side as any, amount, price, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async cancelOrder(orderId: string, symbol: string): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.cancelOrder(orderId, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async cancelOrders(orderIds: string[], symbol?: string): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['cancelOrders']) {
        // Fallback: cancel orders one by one
        const results: Order[] = [];
        for (const orderId of orderIds) {
          if (!symbol) throw new Error('Symbol required for bulk cancel fallback');
          const result = await this.cancelOrder(orderId, symbol);
          results.push(result);
        }
        return results;
      }
      return await this.exchange.cancelOrders(orderIds, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async cancelAllOrders(symbol?: string): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['cancelAllOrders']) {
        throw new Error('Exchange does not support cancelAllOrders');
      }
      return await this.exchange.cancelAllOrders(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async cancelAllOrdersAfter(timeout: number): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['cancelAllOrdersAfter']) {
        throw new Error('cancelAllOrdersAfter not supported by this exchange');
      }
      return await (this.exchange as any).cancelAllOrdersAfter(timeout);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOrder(orderId: string, symbol: string): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchOrder(orderId, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOrdersByIds(orderIds: string[], symbol?: string): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchOrdersByIds']) {
        // Fallback: fetch orders one by one
        const results: Order[] = [];
        for (const orderId of orderIds) {
          if (!symbol) throw new Error('Symbol required for fetchOrdersByIds fallback');
          const order = await this.fetchOrder(orderId, symbol);
          results.push(order);
        }
        return results;
      }
      return await (this.exchange as any).fetchOrdersByIds(orderIds, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOrderTrades(orderId: string, symbol: string): Promise<Trade[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchOrderTrades']) {
        throw new Error('fetchOrderTrades not supported by this exchange');
      }
      return await this.exchange.fetchOrderTrades(orderId, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchMyTrades(symbol?: string, since?: number, limit?: number): Promise<Trade[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchMyTrades']) {
        throw new Error('fetchMyTrades not supported by this exchange');
      }
      return await this.exchange.fetchMyTrades(symbol, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOpenOrders(symbol?: string): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchOpenOrders(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchClosedOrders(symbol?: string, limit?: number): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchClosedOrders(symbol, undefined, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // DEPOSITS & WITHDRAWALS
  // ============================================================================

  async fetchDeposits(code?: string, since?: number, limit?: number): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchDeposits']) {
        throw new Error('fetchDeposits not supported by this exchange');
      }
      return await this.exchange.fetchDeposits(code, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchWithdrawals(code?: string, since?: number, limit?: number): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchWithdrawals']) {
        throw new Error('fetchWithdrawals not supported by this exchange');
      }
      return await this.exchange.fetchWithdrawals(code, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchDepositAddress(code: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchDepositAddress']) {
        throw new Error('fetchDepositAddress not supported by this exchange');
      }
      return await this.exchange.fetchDepositAddress(code, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async createDepositAddress(code: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createDepositAddress']) {
        throw new Error('createDepositAddress not supported by this exchange');
      }
      return await this.exchange.createDepositAddress(code, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchDepositMethods(code?: string): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchDepositMethods']) {
        throw new Error('fetchDepositMethods not supported by this exchange');
      }
      return await (this.exchange as any).fetchDepositMethods(code);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async withdraw(code: string, amount: number, address: string, tag?: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['withdraw']) {
        throw new Error('withdraw not supported by this exchange');
      }
      return await this.exchange.withdraw(code, amount, address, tag, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // TRANSFERS
  // ============================================================================

  async transfer(code: string, amount: number, fromAccount: string, toAccount: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['transfer']) {
        throw new Error('transfer not supported by this exchange');
      }
      return await this.exchange.transfer(code, amount, fromAccount, toAccount, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async transferOut(code: string, amount: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      // transferOut is typically just a transfer to an external account
      if (!this.exchange.has['transfer']) {
        throw new Error('transferOut not supported by this exchange');
      }
      return await this.exchange.transfer(code, amount, 'spot', 'external', params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // EXCHANGE INFORMATION
  // ============================================================================

  getCapabilities(): ExchangeCapabilities {
    const has = this.exchange.has;
    return {
      spot: !!has['spot'],
      margin: !!has['margin'],
      futures: !!has['future'],
      options: !!has['option'],
      swap: !!has['swap'],

      supportedOrderTypes: this.getSupportedOrderTypes(),
      supportedTimeInForce: ['GTC', 'IOC', 'FOK', 'PO'],

      leverageTrading: !!has['setLeverage'],
      stopOrders: !!has['createStopOrder'],
      conditionalOrders: !!has['createOrder'],
      algoOrders: false,

      realtimeData: !!has['watchTicker'],
      historicalData: !!has['fetchOHLCV'],
      websocketSupport: !!has['watchTicker'],

      multiAccount: false,
      subAccounts: false,
      portfolioMargin: false,

      restApi: true,
      websocketApi: !!has['watchTicker'],
      fixApi: false,

      rateLimits: {
        public: 1000,
        private: 3000,
        trading: 1000,
      },
    };
  }

  protected getSupportedOrderTypes(): OrderType[] {
    const types: OrderType[] = ['market', 'limit'];
    if (this.exchange.has['createStopOrder']) types.push('stop', 'stop_limit');
    if (this.exchange.has['createTrailingAmountOrder']) types.push('trailing_stop');
    return types;
  }

  getMarkets(): Record<string, Market> {
    return this.exchange.markets;
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
    this.eventListeners.get(event)?.delete(callback);
  }

  public emit(event: ExchangeEvent, data: any): void {
    const eventData: ExchangeEventData = {
      event,
      exchange: this.id,
      timestamp: Date.now(),
      data,
    };

    this.eventListeners.get(event)?.forEach(callback => {
      try {
        callback(eventData);
      } catch (error) {
        console.error(`Error in event listener for ${event}:`, error);
      }
    });
  }

  // ============================================================================
  // ERROR HANDLING
  // ============================================================================

  protected handleError(error: any): ExchangeError {
    if (error instanceof ExchangeError) {
      return error;
    }

    const message = error.message || 'Unknown error';

    if (message.includes('rate limit')) {
      return new RateLimitError(message, this.id, error);
    }

    if (message.includes('authentication') || message.includes('Invalid API')) {
      return new AuthenticationError(message, this.id, error);
    }

    return new ExchangeError(message, 'UNKNOWN', this.id, error);
  }

  protected async ensureConnected(): Promise<void> {
    if (!this._isConnected) {
      await this.connect();
    }
  }

  protected async ensureAuthenticated(): Promise<void> {
    await this.ensureConnected();
    if (!this._isAuthenticated && (this.exchange.apiKey && this.exchange.secret)) {
      throw new AuthenticationError('Not authenticated', this.id);
    }
  }

  // ============================================================================
  // UTILITIES
  // ============================================================================

  protected parseSymbol(symbol: string): { base: string; quote: string } {
    const parts = symbol.split('/');
    if (parts.length !== 2) {
      throw new Error(`Invalid symbol format: ${symbol}`);
    }
    return { base: parts[0], quote: parts[1] };
  }

  protected formatSymbol(base: string, quote: string): string {
    return `${base}/${quote}`;
  }
}
