/**
 * Base Stock Broker Adapter
 *
 * Abstract base class for all stock brokers globally
 * Provides common functionality - subclasses implement broker-specific logic
 */

import { invoke } from '@tauri-apps/api/core';
import type {
  IStockBrokerAdapter,
  StockBrokerMetadata,
  Region,
  BrokerCredentials,
  AuthResponse,
  AuthCallbackParams,
  Funds,
  MarginRequired,
  OrderParams,
  OrderResponse,
  ModifyOrderParams,
  Order,
  Trade,
  Position,
  Holding,
  Quote,
  OHLCV,
  MarketDepth,
  TimeFrame,
  StockExchange,
  Instrument,
  WebSocketConfig,
  SubscriptionMode,
  TickData,
  BulkOperationResult,
  SmartOrderParams,
  ProductType,
} from './types';
import { ERROR_CODES, ERROR_MESSAGES, DEFAULT_RATE_LIMITS, WEBSOCKET_DEFAULTS } from './constants';

/**
 * Abstract base class for stock broker adapters
 */
export abstract class BaseStockBrokerAdapter implements IStockBrokerAdapter {
  // ============================================================================
  // Abstract Properties (must be implemented by subclass)
  // ============================================================================

  abstract readonly brokerId: string;
  abstract readonly brokerName: string;
  abstract readonly region: Region;
  abstract readonly metadata: StockBrokerMetadata;

  // ============================================================================
  // Protected State
  // ============================================================================

  protected _isConnected: boolean = false;
  protected credentials: BrokerCredentials | null = null;
  protected accessToken: string | null = null;
  protected userId: string | null = null;

  // WebSocket state
  protected wsConnected: boolean = false;
  protected wsConfig: WebSocketConfig | null = null;
  protected tickCallbacks: Set<(tick: TickData) => void> = new Set();
  protected depthCallbacks: Set<(depth: MarketDepth & { symbol: string; exchange: StockExchange }) => void> = new Set();
  protected subscriptions: Map<string, { symbol: string; exchange: StockExchange; mode: SubscriptionMode }> = new Map();

  // Rate limiting
  protected lastApiCall: number = 0;
  protected requestQueue: Array<() => Promise<unknown>> = [];

  // ============================================================================
  // Public Getters
  // ============================================================================

  get isConnected(): boolean {
    return this._isConnected && this.accessToken !== null;
  }

  // ============================================================================
  // Abstract Methods (must be implemented by subclass)
  // ============================================================================

  // Authentication
  abstract authenticate(credentials: BrokerCredentials): Promise<AuthResponse>;

  // Orders - Internal implementations
  protected abstract placeOrderInternal(params: OrderParams): Promise<OrderResponse>;
  protected abstract modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse>;
  protected abstract cancelOrderInternal(orderId: string): Promise<OrderResponse>;
  protected abstract getOrdersInternal(): Promise<Order[]>;
  protected abstract getTradeBookInternal(): Promise<Trade[]>;

  // Positions & Holdings - Internal implementations
  protected abstract getPositionsInternal(): Promise<Position[]>;
  protected abstract getHoldingsInternal(): Promise<Holding[]>;
  protected abstract getFundsInternal(): Promise<Funds>;

  // Market Data - Internal implementations
  protected abstract getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote>;
  protected abstract getOHLCVInternal(symbol: string, exchange: StockExchange, timeframe: TimeFrame, from: Date, to: Date): Promise<OHLCV[]>;
  protected abstract getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth>;

  // WebSocket - Internal implementations
  protected abstract connectWebSocketInternal(config: WebSocketConfig): Promise<void>;
  protected abstract subscribeInternal(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void>;
  protected abstract unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void>;

  // Bulk Operations - Internal implementations
  protected abstract cancelAllOrdersInternal(): Promise<BulkOperationResult>;
  protected abstract closeAllPositionsInternal(): Promise<BulkOperationResult>;

  // Smart Order - Internal implementation
  protected abstract placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse>;

  // Margin Calculator - Internal implementation
  protected abstract calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired>;

  // ============================================================================
  // Common Authentication Methods
  // ============================================================================

  async logout(): Promise<void> {
    this._isConnected = false;
    this.accessToken = null;
    this.userId = null;
    this.credentials = null;

    // Disconnect WebSocket
    if (this.wsConnected) {
      await this.disconnectWebSocket();
    }

    // Clear stored credentials
    await this.clearCredentials();
  }

  async refreshSession?(): Promise<AuthResponse> {
    // Default implementation - subclasses can override
    if (!this.credentials) {
      return {
        success: false,
        message: 'No credentials to refresh',
        errorCode: ERROR_CODES.AUTH_FAILED,
      };
    }
    return this.authenticate(this.credentials);
  }

  // ============================================================================
  // Credential Storage (via Rust backend)
  // ============================================================================

  protected async storeCredentials(credentials: BrokerCredentials): Promise<void> {
    try {
      console.log(`[${this.brokerId}] storeCredentials called with:`, {
        hasApiKey: !!credentials.apiKey,
        hasApiSecret: !!credentials.apiSecret,
        apiKeyLength: credentials.apiKey?.length || 0,
        apiSecretLength: credentials.apiSecret?.length || 0,
        apiKeyValue: credentials.apiKey ? '***' : 'undefined',
        apiSecretValue: credentials.apiSecret ? '***' : 'undefined',
      });

      const payload = {
        apiKey: credentials.apiKey,
        apiSecret: credentials.apiSecret,
        accessToken: credentials.accessToken,
        userId: credentials.userId,
      };

      console.log(`[${this.brokerId}] Sending to Rust:`, {
        brokerId: this.brokerId,
        credentials: {
          apiKey: payload.apiKey ? '***' : payload.apiKey,
          apiSecret: payload.apiSecret ? '***' : payload.apiSecret,
          accessToken: payload.accessToken,
          userId: payload.userId,
        },
      });

      await invoke('store_indian_broker_credentials', {
        brokerId: this.brokerId,
        credentials: payload,
      });

      console.log(`[${this.brokerId}] ✓ Credentials stored successfully`);
    } catch (error) {
      console.error(`[${this.brokerId}] Failed to store credentials:`, error);
    }
  }

  protected async loadCredentials(): Promise<(BrokerCredentials & { tokenTimestamp?: number; userId?: string }) | null> {
    try {
      console.log(`[${this.brokerId}] Loading credentials from Rust backend...`);
      const response = await invoke<{
        success: boolean;
        data?: {
          apiKey?: string;
          apiSecret?: string;
          accessToken?: string;
          userId?: string;
          tokenTimestamp?: number;
          additionalData?: string;
        };
      }>('get_indian_broker_credentials', {
        brokerId: this.brokerId,
      });

      console.log(`[${this.brokerId}] Rust response:`, {
        success: response.success,
        hasData: !!response.data,
        hasApiKey: !!(response.data?.apiKey),
        hasAccessToken: !!(response.data?.accessToken),
        tokenTimestamp: response.data?.tokenTimestamp,
      });

      if (!response.success || !response.data) {
        console.warn(`[${this.brokerId}] No credentials found in response`);
        return null;
      }

      const creds = {
        apiKey: response.data.apiKey || '',
        apiSecret: response.data.apiSecret,
        accessToken: response.data.accessToken,
        userId: response.data.userId,
        tokenTimestamp: response.data.tokenTimestamp,
      };

      console.log(`[${this.brokerId}] ✓ Credentials loaded successfully (token from: ${creds.tokenTimestamp ? new Date(creds.tokenTimestamp).toISOString() : 'N/A'})`);
      return creds;
    } catch (error) {
      console.error(`[${this.brokerId}] Failed to load credentials:`, error);
      return null;
    }
  }

  protected async clearCredentials(): Promise<void> {
    try {
      await invoke('delete_indian_broker_credentials', {
        brokerId: this.brokerId,
      });
    } catch (error) {
      console.error(`[${this.brokerId}] Failed to clear credentials:`, error);
    }
  }

  /**
   * Check if token was generated today (IST timezone)
   * Indian broker tokens expire at midnight IST
   * This is a static utility that can be used for session validation
   */
  public static isTokenValidForTodayIST(tokenTimestamp?: number): boolean {
    if (!tokenTimestamp) return false;

    // Get current time in IST
    const now = new Date();
    const istOffset = 5.5 * 60 * 60 * 1000; // IST is UTC+5:30
    const nowIST = new Date(now.getTime() + istOffset + now.getTimezoneOffset() * 60 * 1000);

    // Get token time in IST
    const tokenDate = new Date(tokenTimestamp);
    const tokenIST = new Date(tokenDate.getTime() + istOffset + tokenDate.getTimezoneOffset() * 60 * 1000);

    // Check if same day in IST
    return (
      nowIST.getFullYear() === tokenIST.getFullYear() &&
      nowIST.getMonth() === tokenIST.getMonth() &&
      nowIST.getDate() === tokenIST.getDate()
    );
  }

  /**
   * Instance method that calls the static version
   */
  public isTokenValidForToday(tokenTimestamp?: number): boolean {
    return BaseStockBrokerAdapter.isTokenValidForTodayIST(tokenTimestamp);
  }

  // ============================================================================
  // Rate Limiting
  // ============================================================================

  protected async rateLimit(): Promise<void> {
    const now = Date.now();
    const elapsed = now - this.lastApiCall;
    const minInterval = DEFAULT_RATE_LIMITS.minRequestInterval;

    if (elapsed < minInterval) {
      await new Promise(resolve => setTimeout(resolve, minInterval - elapsed));
    }

    this.lastApiCall = Date.now();
  }

  // ============================================================================
  // Connection Check
  // ============================================================================

  protected ensureConnected(): void {
    if (!this.isConnected) {
      throw new Error(`Not connected to ${this.brokerName}. Please authenticate first.`);
    }
  }

  // ============================================================================
  // Public Order Methods (with rate limiting and connection checks)
  // ============================================================================

  async placeOrder(params: OrderParams): Promise<OrderResponse> {
    this.ensureConnected();
    await this.rateLimit();
    return this.placeOrderInternal(params);
  }

  async modifyOrder(params: ModifyOrderParams): Promise<OrderResponse> {
    this.ensureConnected();
    await this.rateLimit();
    return this.modifyOrderInternal(params);
  }

  async cancelOrder(orderId: string): Promise<OrderResponse> {
    this.ensureConnected();
    await this.rateLimit();
    return this.cancelOrderInternal(orderId);
  }

  async getOrders(): Promise<Order[]> {
    this.ensureConnected();
    await this.rateLimit();
    return this.getOrdersInternal();
  }

  async getTradeBook(): Promise<Trade[]> {
    this.ensureConnected();
    await this.rateLimit();
    return this.getTradeBookInternal();
  }

  async cancelAllOrders(): Promise<BulkOperationResult> {
    this.ensureConnected();
    await this.rateLimit();
    return this.cancelAllOrdersInternal();
  }

  async placeSmartOrder(params: SmartOrderParams): Promise<OrderResponse> {
    this.ensureConnected();
    await this.rateLimit();
    return this.placeSmartOrderInternal(params);
  }

  // ============================================================================
  // Public Position & Holdings Methods
  // ============================================================================

  async getPositions(): Promise<Position[]> {
    this.ensureConnected();
    await this.rateLimit();
    return this.getPositionsInternal();
  }

  async getHoldings(): Promise<Holding[]> {
    this.ensureConnected();
    await this.rateLimit();
    return this.getHoldingsInternal();
  }

  async getFunds(): Promise<Funds> {
    this.ensureConnected();
    await this.rateLimit();
    return this.getFundsInternal();
  }

  async getOpenPosition(
    symbol: string,
    exchange: StockExchange,
    productType: ProductType
  ): Promise<Position | null> {
    const positions = await this.getPositions();
    return positions.find(
      p => p.symbol === symbol && p.exchange === exchange && p.productType === productType
    ) || null;
  }

  async closeAllPositions(): Promise<BulkOperationResult> {
    this.ensureConnected();
    await this.rateLimit();
    return this.closeAllPositionsInternal();
  }

  // ============================================================================
  // Public Account Methods
  // ============================================================================

  async getMarginRequired(params: OrderParams): Promise<MarginRequired> {
    this.ensureConnected();
    await this.rateLimit();
    return this.calculateMarginInternal([params]);
  }

  async calculateMargin(orders: OrderParams[]): Promise<MarginRequired> {
    this.ensureConnected();
    await this.rateLimit();
    return this.calculateMarginInternal(orders);
  }

  // ============================================================================
  // Public Market Data Methods
  // ============================================================================

  async getQuote(symbol: string, exchange: StockExchange): Promise<Quote> {
    this.ensureConnected();
    await this.rateLimit();
    return this.getQuoteInternal(symbol, exchange);
  }

  async getQuotes(
    instruments: Array<{ symbol: string; exchange: StockExchange }>
  ): Promise<Quote[]> {
    this.ensureConnected();
    await this.rateLimit();

    // Default: fetch one by one (subclasses should override for batch support)
    const quotes: Quote[] = [];
    for (const { symbol, exchange } of instruments) {
      try {
        const quote = await this.getQuoteInternal(symbol, exchange);
        quotes.push(quote);
      } catch (error) {
        console.error(`[${this.brokerId}] Error fetching quote for ${symbol}:`, error);
      }
    }
    return quotes;
  }

  async getOHLCV(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    this.ensureConnected();
    await this.rateLimit();
    return this.getOHLCVInternal(symbol, exchange, timeframe, from, to);
  }

  async getMarketDepth(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    this.ensureConnected();
    await this.rateLimit();
    return this.getMarketDepthInternal(symbol, exchange);
  }

  // ============================================================================
  // Symbol Search (via Rust backend)
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const results = await invoke<Instrument[]>('search_indian_symbols', {
        brokerId: this.brokerId,
        query,
        exchange,
      });
      return results;
    } catch (error) {
      console.error(`[${this.brokerId}] Symbol search error:`, error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const result = await invoke<Instrument | null>('get_indian_symbol_token', {
        brokerId: this.brokerId,
        symbol,
        exchange,
      });
      return result;
    } catch (error) {
      console.error(`[${this.brokerId}] Get instrument error:`, error);
      return null;
    }
  }

  // ============================================================================
  // WebSocket Methods
  // ============================================================================

  async connectWebSocket(config?: WebSocketConfig): Promise<void> {
    this.ensureConnected();

    this.wsConfig = {
      ...WEBSOCKET_DEFAULTS,
      ...config,
    };

    await this.connectWebSocketInternal(this.wsConfig);
    this.wsConnected = true;
  }

  async disconnectWebSocket(): Promise<void> {
    // Unsubscribe all
    for (const [key, sub] of this.subscriptions) {
      try {
        await this.unsubscribeInternal(sub.symbol, sub.exchange);
      } catch {
        // Ignore errors during disconnect
      }
    }

    this.subscriptions.clear();
    this.wsConnected = false;
  }

  async subscribe(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    if (!this.wsConnected) {
      await this.connectWebSocket();
    }

    const key = `${exchange}:${symbol}`;
    this.subscriptions.set(key, { symbol, exchange, mode });

    await this.subscribeInternal(symbol, exchange, mode);
  }

  async unsubscribe(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.subscriptions.delete(key);

    if (this.wsConnected) {
      await this.unsubscribeInternal(symbol, exchange);
    }
  }

  onTick(callback: (tick: TickData) => void): void {
    this.tickCallbacks.add(callback);
  }

  offTick(callback: (tick: TickData) => void): void {
    this.tickCallbacks.delete(callback);
  }

  protected emitTick(tick: TickData): void {
    for (const callback of this.tickCallbacks) {
      try {
        callback(tick);
      } catch (error) {
        console.error(`[${this.brokerId}] Tick callback error:`, error);
      }
    }
  }

  // Depth event handlers
  onDepth(callback: (depth: MarketDepth & { symbol: string; exchange: StockExchange }) => void): void {
    this.depthCallbacks.add(callback);
  }

  offDepth(callback: (depth: MarketDepth & { symbol: string; exchange: StockExchange }) => void): void {
    this.depthCallbacks.delete(callback);
  }

  protected emitDepth(depth: MarketDepth & { symbol: string; exchange: StockExchange }): void {
    for (const callback of this.depthCallbacks) {
      try {
        callback(depth);
      } catch (error) {
        console.error(`[${this.brokerId}] Depth callback error:`, error);
      }
    }
  }

  // ============================================================================
  // Master Contract (Optional - subclasses can override)
  // ============================================================================

  async downloadMasterContract?(): Promise<void> {
    // Default: no-op, subclasses implement if needed
  }

  async getMasterContractLastUpdated?(): Promise<Date | null> {
    try {
      const timestamp = await invoke<number | null>('get_master_contract_timestamp', {
        brokerId: this.brokerId,
      });
      return timestamp ? new Date(timestamp) : null;
    } catch {
      return null;
    }
  }

  // ============================================================================
  // Error Handling Helpers
  // ============================================================================

  protected handleError(error: unknown, operation: string): never {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`[${this.brokerId}] ${operation} failed:`, message);

    throw new Error(`${operation} failed: ${message}`);
  }

  protected createErrorResponse(message: string, errorCode?: string): OrderResponse {
    return {
      success: false,
      message,
      errorCode: errorCode || ERROR_CODES.UNKNOWN_ERROR,
    };
  }
}
