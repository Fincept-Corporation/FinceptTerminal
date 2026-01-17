/**
 * Base adapter class for Indian stock brokers
 * Provides common functionality and abstract methods for broker implementations
 */

import { invoke } from '@tauri-apps/api/core';
import type {
  IIndianBrokerAdapter,
  IndianBrokerMetadata,
  BrokerCredentials,
  AuthResponse,
  AuthCallbackParams,
  FundsData,
  MarginRequired,
  OrderParams,
  OrderResponse,
  ModifyOrderParams,
  Order,
  Position,
  Holding,
  Quote,
  OHLCV,
  MarketDepth,
  TimeFrame,
  IndianExchange,
  SymbolSearchResult,
  SymbolToken,
  WebSocketConfig,
  SubscriptionMode,
  TickData,
  ApiResponse,
  Trade,
  SmartOrderParams,
  OpenPositionResult,
  BulkOperationResult,
  MarginOrderParams,
  MarginCalculation,
  ProductType,
} from './types';
import { ERROR_CODES, ERROR_MESSAGES } from './constants';

/**
 * Abstract base class for Indian broker adapters
 * Implements common functionality and defines abstract methods for broker-specific logic
 */
export abstract class BaseIndianBrokerAdapter implements IIndianBrokerAdapter {
  // ============================================================================
  // Abstract Properties (must be implemented by subclass)
  // ============================================================================

  abstract readonly brokerId: string;
  abstract readonly brokerName: string;
  abstract readonly metadata: IndianBrokerMetadata;

  // ============================================================================
  // Protected Properties
  // ============================================================================

  protected _isConnected: boolean = false;
  protected credentials: BrokerCredentials | null = null;
  protected accessToken: string | null = null;
  protected userId: string | null = null;

  // WebSocket state
  protected wsConnected: boolean = false;
  protected wsConfig: WebSocketConfig | null = null;
  protected subscriptions: Map<string, { symbol: string; exchange: IndianExchange; mode: SubscriptionMode }> = new Map();

  // Rate limiting
  protected lastApiCall: number = 0;
  protected apiCallQueue: Array<() => Promise<unknown>> = [];

  // ============================================================================
  // Public Getters
  // ============================================================================

  get isConnected(): boolean {
    return this._isConnected && this.accessToken !== null;
  }

  // ============================================================================
  // Abstract Methods (must be implemented by subclass)
  // ============================================================================

  /**
   * Get the authentication URL for OAuth flow
   */
  abstract getAuthUrl(): string;

  /**
   * Process the authentication callback and exchange tokens
   */
  abstract handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse>;

  /**
   * Broker-specific order placement
   */
  protected abstract placeOrderInternal(params: OrderParams): Promise<OrderResponse>;

  /**
   * Broker-specific order modification
   */
  protected abstract modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse>;

  /**
   * Broker-specific order cancellation
   */
  protected abstract cancelOrderInternal(orderId: string): Promise<OrderResponse>;

  /**
   * Broker-specific order fetch
   */
  protected abstract getOrdersInternal(): Promise<Order[]>;

  /**
   * Broker-specific position fetch
   */
  protected abstract getPositionsInternal(): Promise<Position[]>;

  /**
   * Broker-specific holdings fetch
   */
  protected abstract getHoldingsInternal(): Promise<Holding[]>;

  /**
   * Broker-specific funds fetch
   */
  protected abstract getFundsInternal(): Promise<FundsData>;

  /**
   * Broker-specific quote fetch
   */
  protected abstract getQuoteInternal(symbol: string, exchange: IndianExchange): Promise<Quote>;

  /**
   * Broker-specific OHLCV fetch
   */
  protected abstract getOHLCVInternal(
    symbol: string,
    exchange: IndianExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]>;

  /**
   * Broker-specific market depth fetch
   */
  protected abstract getMarketDepthInternal(
    symbol: string,
    exchange: IndianExchange
  ): Promise<MarketDepth>;

  /**
   * Broker-specific WebSocket connection
   */
  protected abstract connectWebSocketInternal(config: WebSocketConfig): Promise<void>;

  /**
   * Broker-specific WebSocket subscription
   */
  protected abstract subscribeInternal(
    symbol: string,
    exchange: IndianExchange,
    mode: SubscriptionMode
  ): Promise<void>;

  /**
   * Broker-specific WebSocket unsubscription
   */
  protected abstract unsubscribeInternal(
    symbol: string,
    exchange: IndianExchange
  ): Promise<void>;

  /**
   * Broker-specific master contract download
   */
  protected abstract downloadMasterContractInternal(): Promise<void>;

  // ============================================================================
  // Abstract Methods for New Features (Trade Book, Smart Orders, Bulk Operations)
  // Subclasses must implement these methods
  // ============================================================================

  /**
   * Get trade book (executed trades)
   */
  abstract getTradeBook(): Promise<Trade[]>;

  /**
   * Get open position for a specific symbol
   */
  abstract getOpenPosition(
    symbol: string,
    exchange: IndianExchange,
    product: ProductType
  ): Promise<OpenPositionResult>;

  /**
   * Place smart order (checks position and adjusts)
   */
  abstract placeSmartOrder(params: SmartOrderParams): Promise<OrderResponse>;

  /**
   * Close all open positions
   */
  abstract closeAllPositions(): Promise<BulkOperationResult>;

  /**
   * Cancel all open orders
   */
  abstract cancelAllOrders(): Promise<BulkOperationResult>;

  /**
   * Calculate margin for orders
   */
  abstract calculateMargin(orders: MarginOrderParams[]): Promise<MarginCalculation>;

  // ============================================================================
  // Common Authentication Methods
  // ============================================================================

  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.credentials = credentials;

      // If access token is already provided, validate it
      if (credentials.accessToken) {
        const isValid = await this.validateToken(credentials.accessToken);
        if (isValid) {
          this.accessToken = credentials.accessToken;
          this.userId = credentials.userId || null;
          this._isConnected = true;

          // Store credentials in SQLite
          await this.storeCredentials(credentials);

          return {
            success: true,
            accessToken: credentials.accessToken,
            userId: this.userId || undefined,
            message: 'Authentication successful',
          };
        }
      }

      // If no access token, return auth URL for OAuth flow
      return {
        success: false,
        message: 'Please complete authentication via browser',
        errorCode: ERROR_CODES.AUTH_TOKEN_EXPIRED,
      };
    } catch (error) {
      return this.handleAuthError(error);
    }
  }

  async logout(): Promise<void> {
    try {
      // Disconnect WebSocket if connected
      if (this.wsConnected) {
        await this.disconnectWebSocket();
      }

      // Clear credentials from storage
      await this.clearCredentials();

      // Reset state
      this._isConnected = false;
      this.accessToken = null;
      this.userId = null;
      this.credentials = null;
    } catch (error) {
      console.error(`[${this.brokerId}] Logout error:`, error);
      throw error;
    }
  }

  async refreshSession(): Promise<AuthResponse> {
    try {
      if (!this.credentials?.refreshToken) {
        return {
          success: false,
          message: 'No refresh token available',
          errorCode: ERROR_CODES.AUTH_SESSION_EXPIRED,
        };
      }

      // This should be overridden by brokers that support refresh tokens
      return {
        success: false,
        message: 'Session refresh not supported. Please login again.',
        errorCode: ERROR_CODES.AUTH_SESSION_EXPIRED,
      };
    } catch (error) {
      return this.handleAuthError(error);
    }
  }

  /**
   * Validate if a token is still valid
   */
  protected async validateToken(token: string): Promise<boolean> {
    try {
      // Try to fetch user profile or funds as a validation check
      // This should be overridden by subclasses with broker-specific validation
      return true;
    } catch {
      return false;
    }
  }

  // ============================================================================
  // Order Methods (with common wrapper logic)
  // ============================================================================

  async placeOrder(params: OrderParams): Promise<OrderResponse> {
    this.ensureConnected();
    await this.rateLimit();

    try {
      return await this.placeOrderInternal(params);
    } catch (error) {
      return this.handleOrderError(error);
    }
  }

  async modifyOrder(params: ModifyOrderParams): Promise<OrderResponse> {
    this.ensureConnected();
    await this.rateLimit();

    try {
      return await this.modifyOrderInternal(params);
    } catch (error) {
      return this.handleOrderError(error);
    }
  }

  async cancelOrder(orderId: string): Promise<OrderResponse> {
    this.ensureConnected();
    await this.rateLimit();

    try {
      return await this.cancelOrderInternal(orderId);
    } catch (error) {
      return this.handleOrderError(error);
    }
  }

  async getOrders(): Promise<Order[]> {
    this.ensureConnected();
    await this.rateLimit();

    return this.getOrdersInternal();
  }

  async getOrderHistory(orderId: string): Promise<Order[]> {
    this.ensureConnected();
    await this.rateLimit();

    // Default implementation returns the order
    const orders = await this.getOrders();
    return orders.filter((o) => o.orderId === orderId);
  }

  // ============================================================================
  // Position & Holdings Methods
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

  async getFunds(): Promise<FundsData> {
    this.ensureConnected();
    await this.rateLimit();

    return this.getFundsInternal();
  }

  async getMarginRequired(params: OrderParams): Promise<MarginRequired> {
    this.ensureConnected();
    await this.rateLimit();

    // Default implementation - should be overridden by brokers that support margin calculator
    return {
      total: 0,
      var: 0,
      span: 0,
      exposure: 0,
      premium: 0,
    };
  }

  // ============================================================================
  // Market Data Methods
  // ============================================================================

  async getQuote(symbol: string, exchange: IndianExchange): Promise<Quote> {
    this.ensureConnected();
    await this.rateLimit();

    return this.getQuoteInternal(symbol, exchange);
  }

  async getQuotes(
    symbols: Array<{ symbol: string; exchange: IndianExchange }>
  ): Promise<Quote[]> {
    this.ensureConnected();
    await this.rateLimit();

    // Default implementation - fetch one by one
    // Subclasses should override for batch API support
    const quotes: Quote[] = [];
    for (const { symbol, exchange } of symbols) {
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
    exchange: IndianExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    this.ensureConnected();
    await this.rateLimit();

    return this.getOHLCVInternal(symbol, exchange, timeframe, from, to);
  }

  async getMarketDepth(symbol: string, exchange: IndianExchange): Promise<MarketDepth> {
    this.ensureConnected();
    await this.rateLimit();

    return this.getMarketDepthInternal(symbol, exchange);
  }

  // ============================================================================
  // Symbol Management Methods
  // ============================================================================

  async searchSymbols(
    query: string,
    exchange?: IndianExchange
  ): Promise<SymbolSearchResult[]> {
    try {
      const results = await invoke<SymbolSearchResult[]>('search_indian_symbols', {
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

  async getSymbolToken(
    symbol: string,
    exchange: IndianExchange
  ): Promise<SymbolToken | null> {
    try {
      const result = await invoke<SymbolToken | null>('get_indian_symbol_token', {
        brokerId: this.brokerId,
        symbol,
        exchange,
      });
      return result;
    } catch (error) {
      console.error(`[${this.brokerId}] Get symbol token error:`, error);
      return null;
    }
  }

  // ============================================================================
  // WebSocket Methods
  // ============================================================================

  async connectWebSocket(config: WebSocketConfig): Promise<void> {
    if (this.wsConnected) {
      console.warn(`[${this.brokerId}] WebSocket already connected`);
      return;
    }

    this.wsConfig = config;
    await this.connectWebSocketInternal(config);
    this.wsConnected = true;
  }

  async disconnectWebSocket(): Promise<void> {
    if (!this.wsConnected) {
      return;
    }

    try {
      // Unsubscribe from all symbols
      for (const [key, sub] of this.subscriptions) {
        await this.unsubscribeInternal(sub.symbol, sub.exchange);
      }
      this.subscriptions.clear();

      // Close WebSocket connection
      await invoke('disconnect_indian_broker_websocket', {
        brokerId: this.brokerId,
      });

      this.wsConnected = false;
      this.wsConfig = null;
    } catch (error) {
      console.error(`[${this.brokerId}] WebSocket disconnect error:`, error);
      throw error;
    }
  }

  async subscribe(
    symbol: string,
    exchange: IndianExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    if (!this.wsConnected) {
      throw new Error('WebSocket not connected');
    }

    const key = `${exchange}:${symbol}`;
    if (this.subscriptions.has(key)) {
      // Update mode if different
      const existing = this.subscriptions.get(key)!;
      if (existing.mode !== mode) {
        await this.unsubscribeInternal(symbol, exchange);
        await this.subscribeInternal(symbol, exchange, mode);
        this.subscriptions.set(key, { symbol, exchange, mode });
      }
      return;
    }

    await this.subscribeInternal(symbol, exchange, mode);
    this.subscriptions.set(key, { symbol, exchange, mode });
  }

  async unsubscribe(symbol: string, exchange: IndianExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    if (!this.subscriptions.has(key)) {
      return;
    }

    await this.unsubscribeInternal(symbol, exchange);
    this.subscriptions.delete(key);
  }

  // ============================================================================
  // Master Contract Methods
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    this.ensureConnected();

    await this.downloadMasterContractInternal();
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
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
  // Protected Helper Methods
  // ============================================================================

  /**
   * Ensure adapter is connected before making API calls
   */
  protected ensureConnected(): void {
    if (!this.isConnected) {
      throw new Error(ERROR_MESSAGES[ERROR_CODES.AUTH_SESSION_EXPIRED]);
    }
  }

  /**
   * Simple rate limiting
   */
  protected async rateLimit(): Promise<void> {
    const now = Date.now();
    const minInterval = 100; // 100ms between calls

    if (now - this.lastApiCall < minInterval) {
      await new Promise((resolve) => setTimeout(resolve, minInterval - (now - this.lastApiCall)));
    }

    this.lastApiCall = Date.now();
  }

  /**
   * Make HTTP request through Tauri backend
   */
  protected async makeRequest<T>(
    method: 'GET' | 'POST' | 'PUT' | 'DELETE',
    endpoint: string,
    data?: Record<string, unknown>,
    headers?: Record<string, string>
  ): Promise<ApiResponse<T>> {
    try {
      const response = await invoke<ApiResponse<T>>('indian_broker_request', {
        brokerId: this.brokerId,
        method,
        endpoint,
        data,
        headers: {
          ...headers,
          Authorization: this.accessToken ? `token ${this.accessToken}` : undefined,
        },
      });
      return response;
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Request failed',
        timestamp: Date.now(),
      };
    }
  }

  /**
   * Store credentials in SQLite database
   */
  protected async storeCredentials(credentials: BrokerCredentials): Promise<void> {
    try {
      await invoke('store_indian_broker_credentials', {
        brokerId: this.brokerId,
        credentials: {
          ...credentials,
          // Don't store password
          password: undefined,
        },
      });
    } catch (error) {
      console.error(`[${this.brokerId}] Failed to store credentials:`, error);
    }
  }

  /**
   * Load credentials from SQLite database
   */
  protected async loadCredentials(): Promise<BrokerCredentials | null> {
    try {
      const credentials = await invoke<BrokerCredentials | null>(
        'get_indian_broker_credentials',
        {
          brokerId: this.brokerId,
        }
      );
      return credentials;
    } catch {
      return null;
    }
  }

  /**
   * Clear stored credentials
   */
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
   * Handle authentication errors
   */
  protected handleAuthError(error: unknown): AuthResponse {
    const message = error instanceof Error ? error.message : 'Authentication failed';
    console.error(`[${this.brokerId}] Auth error:`, message);

    return {
      success: false,
      message,
      errorCode: ERROR_CODES.AUTH_INVALID_CREDENTIALS,
    };
  }

  /**
   * Handle order errors
   */
  protected handleOrderError(error: unknown): OrderResponse {
    const message = error instanceof Error ? error.message : 'Order operation failed';
    console.error(`[${this.brokerId}] Order error:`, message);

    return {
      success: false,
      message,
      errorCode: ERROR_CODES.UNKNOWN_ERROR,
    };
  }

  /**
   * Emit tick data to registered callback
   */
  protected emitTick(tick: TickData): void {
    if (this.wsConfig?.onTick) {
      this.wsConfig.onTick(tick);
    }
  }

  /**
   * Emit connection event
   */
  protected emitConnect(): void {
    if (this.wsConfig?.onConnect) {
      this.wsConfig.onConnect();
    }
  }

  /**
   * Emit disconnection event
   */
  protected emitDisconnect(): void {
    if (this.wsConfig?.onDisconnect) {
      this.wsConfig.onDisconnect();
    }
  }

  /**
   * Emit error event
   */
  protected emitError(error: Error): void {
    if (this.wsConfig?.onError) {
      this.wsConfig.onError(error);
    }
  }
}
