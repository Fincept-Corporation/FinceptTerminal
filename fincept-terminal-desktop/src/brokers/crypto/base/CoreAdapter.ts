/**
 * Core Exchange Adapter
 *
 * Base class with connection, authentication, and core market data methods
 * Extended by feature-specific adapters
 */

import ccxt, { Exchange, Market, Ticker, OrderBook, Trade, OHLCV, Balance, Order, Position } from 'ccxt';
import {
  ExchangeCredentials,
  ExchangeConfig,
  OrderType,
  ExchangeCapabilities,
  ExchangeError,
  AuthenticationError,
  RateLimitError,
  ExchangeEvent,
  ExchangeEventData,
} from '../types';

export abstract class CoreExchangeAdapter {
  protected exchange: Exchange;
  protected _isConnected: boolean = false;
  protected _isAuthenticated: boolean = false;
  protected eventListeners: Map<ExchangeEvent, Set<(data: ExchangeEventData) => void>> = new Map();

  public readonly id: string;
  public readonly name: string;

  constructor(config: ExchangeConfig) {
    this.id = config.exchange;

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
      timeout: config.timeout ?? 10000,
      ...(config.sandbox && { sandbox: true }),
      ...config.options,
    });

    this.name = this.exchange.name as string;
  }

  // ============================================================================
  // CONNECTION MANAGEMENT
  // ============================================================================

  async connect(): Promise<void> {
    const maxRetries = 2;
    const retryDelay = 2000;
    let lastError: any = null;

    for (let attempt = 0; attempt <= maxRetries; attempt++) {
      try {
        const originalTimeout = this.exchange.timeout;
        this.exchange.timeout = 10000;
        await this.exchange.loadMarkets();
        this.exchange.timeout = originalTimeout;
        this._isConnected = true;
        this.emit('connected', { exchange: this.id });
        return;
      } catch (error: any) {
        lastError = error;
        const errorMessage = error?.message || String(error);
        const isNetworkError =
          errorMessage.includes('CORS') ||
          errorMessage.includes('Network') ||
          errorMessage.includes('fetch') ||
          errorMessage.includes('timed out') ||
          errorMessage.includes('Failed to fetch') ||
          error?.name === 'TypeError';

        if (isNetworkError && errorMessage.includes('CORS')) {
          console.error(`[${this.id}] [FAIL] CORS error detected`);
          this._isConnected = false;
          break;
        }

        if (attempt < maxRetries) {
          await new Promise(resolve => setTimeout(resolve, retryDelay));
        }
      }
    }

    this._isConnected = false;
    const errorMessage = lastError?.message || String(lastError);
    if (errorMessage.includes('CORS')) {
      throw new Error(`Cannot connect to ${this.id} due to browser security (CORS).`);
    } else if (errorMessage.includes('Network') || errorMessage.includes('fetch')) {
      throw new Error(`Network error connecting to ${this.id}.`);
    } else {
      throw this.handleError(lastError);
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
      await this.exchange.fetchBalance();
      this._isAuthenticated = true;
      this.emit('authenticated', { exchange: this.id });
    } catch (error) {
      this._isAuthenticated = false;
      throw new AuthenticationError('Authentication failed', this.id, error);
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

  async fetchOHLCV(symbol: string, timeframe: string = '1m', limit?: number): Promise<OHLCV[]> {
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
      rateLimits: { public: 1000, private: 3000, trading: 1000 },
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
    if (error instanceof ExchangeError) return error;
    const message = error.message || 'Unknown error';
    if (message.includes('rate limit')) return new RateLimitError(message, this.id, error);
    if (message.includes('authentication') || message.includes('Invalid API')) {
      return new AuthenticationError(message, this.id, error);
    }
    return new ExchangeError(message, 'UNKNOWN', this.id, error);
  }

  protected async ensureConnected(): Promise<void> {
    if (!this._isConnected) await this.connect();
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
    if (parts.length !== 2) throw new Error(`Invalid symbol format: ${symbol}`);
    return { base: parts[0], quote: parts[1] };
  }

  protected formatSymbol(base: string, quote: string): string {
    return `${base}/${quote}`;
  }
}
