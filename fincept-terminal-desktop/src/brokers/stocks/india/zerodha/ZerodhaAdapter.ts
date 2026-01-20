/**
 * Zerodha Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Zerodha Kite Connect API
 */

import { invoke } from '@tauri-apps/api/core';
import { listen, UnlistenFn } from '@tauri-apps/api/event';
import { BaseStockBrokerAdapter } from '../../BaseStockBrokerAdapter';
import type {
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
  WebSocketConfig,
  SubscriptionMode,
  BulkOperationResult,
  SmartOrderParams,
  TickData,
  Instrument,
  InstrumentType,
} from '../../types';
import {
  ZERODHA_METADATA,
  ZERODHA_LOGIN_URL,
  ZERODHA_EXCHANGE_MAP,
} from './constants';
import {
  toZerodhaOrderParams,
  toZerodhaModifyParams,
  toZerodhaInterval,
  toZerodhaMarginParams,
  toZerodhaWSMode,
  fromZerodhaOrder,
  fromZerodhaTrade,
  fromZerodhaPosition,
  fromZerodhaHolding,
  fromZerodhaFunds,
  fromZerodhaQuote,
  fromZerodhaOHLCV,
  fromZerodhaDepth,
  fromZerodhaTick,
} from './mapper';

/**
 * Zerodha broker adapter
 */
export class ZerodhaAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'zerodha';
  readonly brokerName = 'Zerodha';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = ZERODHA_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;

  // WebSocket polling fallback flag
  private usePollingFallback: boolean = false;

  // Fallback: REST API polling (used when WebSocket fails)
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange }> = new Map();

  // Token to symbol mapping for WebSocket subscriptions
  private tokenSymbolMap: Map<number, { symbol: string; exchange: StockExchange }> = new Map();

  // Tauri event unlisteners for WebSocket events
  private eventUnlisteners: UnlistenFn[] = [];

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Get OAuth login URL
   */
  getAuthUrl(): string {
    if (!this.apiKey) {
      throw new Error('API key not set. Call setCredentials first.');
    }
    return `${ZERODHA_LOGIN_URL}?api_key=${this.apiKey}&v=3`;
  }

  /**
   * Set API credentials before authentication
   */
  setCredentials(apiKey: string, apiSecret: string): void {
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
  }

  /**
   * Authenticate with stored or new credentials
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;

      // Check if this is a request_token that needs to be exchanged (OAuth callback)
      const requestToken = (credentials as any).requestToken;
      if (requestToken) {
        console.log('[ZerodhaAdapter] Request token detected, exchanging for access token...');
        return this.handleAuthCallback({ requestToken });
      }

      // If access token provided, validate it
      if (credentials.accessToken) {
        console.log('[ZerodhaAdapter] Access token provided, validating...');
        const isValid = await this.validateToken(credentials.accessToken);
        if (isValid) {
          this.accessToken = credentials.accessToken;
          this.userId = credentials.userId || null;
          this._isConnected = true;

          await this.storeCredentials(credentials);

          return {
            success: true,
            accessToken: this.accessToken,
            userId: this.userId || undefined,
            message: 'Authentication successful',
          };
        } else {
          console.error('[ZerodhaAdapter] Access token validation failed');
          return {
            success: false,
            message: 'Invalid or expired access token',
            errorCode: 'AUTH_FAILED',
          };
        }
      }

      // Need to go through OAuth flow
      console.warn('[ZerodhaAdapter] No access token or request token provided');
      return {
        success: false,
        message: 'Please complete OAuth login',
        errorCode: 'AUTH_REQUIRED',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Authentication failed',
        errorCode: 'AUTH_FAILED',
      };
    }
  }

  /**
   * Handle OAuth callback
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    try {
      if (!params.requestToken) {
        return {
          success: false,
          message: 'Request token not provided',
          errorCode: 'AUTH_FAILED',
        };
      }

      if (!this.apiKey || !this.apiSecret) {
        return {
          success: false,
          message: 'API credentials not set',
          errorCode: 'AUTH_FAILED',
        };
      }

      // Exchange request token for access token via Rust backend
      const response = await invoke<{
        success: boolean;
        access_token?: string;
        user_id?: string;
        error?: string;
      }>('zerodha_exchange_token', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        requestToken: params.requestToken,
      });

      if (!response.success || !response.access_token) {
        return {
          success: false,
          message: response.error || 'Token exchange failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.accessToken = response.access_token;
      this.userId = response.user_id || null;
      this._isConnected = true;

      // Store credentials
      await this.storeCredentials({
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        accessToken: this.accessToken,
        userId: this.userId || undefined,
      });

      return {
        success: true,
        accessToken: this.accessToken,
        userId: this.userId || undefined,
        message: 'Authentication successful',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Auth callback failed',
        errorCode: 'AUTH_FAILED',
      };
    }
  }

  /**
   * Validate access token
   */
  private async validateToken(token: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean }>('zerodha_validate_token', {
        apiKey: this.apiKey,
        accessToken: token,
      });
      return response.success;
    } catch {
      return false;
    }
  }

  /**
   * Initialize from stored credentials
   */
  async initFromStorage(): Promise<boolean> {
    try {
      const credentials = await this.loadCredentials();
      if (!credentials) return false;

      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;
      this.accessToken = credentials.accessToken || null;

      if (this.accessToken) {
        console.log('[Zerodha] Validating stored access token...');
        const isValid = await this.validateToken(this.accessToken);

        if (!isValid) {
          console.warn('[Zerodha] Access token expired or invalid, clearing from storage...');

          // Clear expired token from database
          await this.storeCredentials({
            apiKey: this.apiKey,
            apiSecret: this.apiSecret || '',
            // No accessToken = clears it
          });

          this.accessToken = null;
          this._isConnected = false;

          console.log('[Zerodha] Please re-authenticate to continue');
          return false;
        }

        console.log('[Zerodha] ✓ Token is valid, session restored');
        this._isConnected = isValid;
        return isValid;
      }

      return false;
    } catch {
      return false;
    }
  }

  // ============================================================================
  // Orders
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    const zerodhaParams = toZerodhaOrderParams(params);
    const variety = params.amo ? 'amo' : 'regular';

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('zerodha_place_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      params: zerodhaParams,
      variety,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const zerodhaParams = toZerodhaModifyParams(params);

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('zerodha_modify_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      orderId: params.orderId,
      params: zerodhaParams,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('zerodha_cancel_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      orderId,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('zerodha_get_orders', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromZerodhaOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('zerodha_get_trade_book', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromZerodhaTrade);
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    const response = await invoke<{
      success: boolean;
      data?: Array<{
        success: boolean;
        order_id?: string;
        error?: string;
      }>;
      error?: string;
    }>('zerodha_cancel_all_orders', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }

    const results = response.data.map(r => ({
      success: r.success,
      orderId: r.order_id,
      error: r.error,
    }));

    return {
      success: true,
      totalCount: results.length,
      successCount: results.filter(r => r.success).length,
      failedCount: results.filter(r => !r.success).length,
      results,
    };
  }

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    const response = await invoke<{
      success: boolean;
      data?: Array<{
        success: boolean;
        order_id?: string;
        error?: string;
      }>;
      error?: string;
    }>('zerodha_close_all_positions', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }

    const results = response.data.map(r => ({
      success: r.success,
      orderId: r.order_id,
      error: r.error,
    }));

    return {
      success: true,
      totalCount: results.length,
      successCount: results.filter(r => r.success).length,
      failedCount: results.filter(r => !r.success).length,
      results,
    };
  }

  // ============================================================================
  // Smart Order
  // ============================================================================

  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    const zerodhaExchange = ZERODHA_EXCHANGE_MAP[params.exchange] || params.exchange;

    // Get current position size if not provided
    let positionSize = params.positionSize ?? 0;
    if (params.positionSize === undefined) {
      const position = await this.getOpenPosition(params.symbol, params.exchange, params.productType);
      positionSize = position?.quantity ?? 0;
    }

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('zerodha_place_smart_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      tradingsymbol: params.symbol,
      exchange: zerodhaExchange,
      product: params.productType,
      action: params.side,
      quantity: params.quantity,
      orderType: params.orderType,
      price: params.price,
      triggerPrice: params.triggerPrice,
      positionSize,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  // ============================================================================
  // Positions & Holdings
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    const response = await invoke<{
      success: boolean;
      data?: {
        net?: Record<string, unknown>[];
        day?: Record<string, unknown>[];
      };
      error?: string;
    }>('zerodha_get_positions', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    const positions = response.data.net || [];
    return positions.map(fromZerodhaPosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('zerodha_get_holdings', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromZerodhaHolding);
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('zerodha_get_margins', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        totalBalance: 0,
        currency: 'INR',
      };
    }

    return fromZerodhaFunds(response.data);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    const zerodhaOrders = orders.map(toZerodhaMarginParams);

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('zerodha_calculate_margin', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      orders: zerodhaOrders,
    });

    if (!response.success || !response.data) {
      return {
        totalMargin: 0,
        initialMargin: 0,
      };
    }

    const data = response.data;
    const initial = Number(data.initial || data.total || 0);
    const exposure = Number(data.exposure || 0);
    const total = Number(data.total || initial + exposure);

    return {
      totalMargin: total,
      initialMargin: initial,
      exposureMargin: exposure,
      spanMargin: data.span ? Number(data.span) : undefined,
      optionPremium: data.option_premium ? Number(data.option_premium) : undefined,
      orders: orders.map((o, i) => ({
        symbol: o.symbol,
        margin: Array.isArray(data.orders) ? Number((data.orders as Record<string, unknown>[])[i]?.margin || 0) : 0,
      })),
    };
  }

  // ============================================================================
  // Market Data
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    const instrumentKey = `${ZERODHA_EXCHANGE_MAP[exchange] || exchange}:${symbol}`;

    const response = await invoke<{
      success: boolean;
      data?: Record<string, Record<string, unknown>>;
      error?: string;
    }>('zerodha_get_quote', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      instruments: [instrumentKey],
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || 'Failed to fetch quote');
    }

    const quoteData = response.data[instrumentKey];
    if (!quoteData) {
      throw new Error(`Quote not found for ${symbol}`);
    }

    return fromZerodhaQuote(quoteData, symbol, exchange);
  }

  /**
   * Get multiple quotes in batch (up to 500 symbols per request)
   * Automatically batches if more than 500 symbols
   */
  async getMultiQuotes(
    symbols: Array<{ symbol: string; exchange: StockExchange }>
  ): Promise<Array<{ symbol: string; exchange: StockExchange; quote?: Quote; error?: string }>> {
    this.ensureConnected();

    const BATCH_SIZE = 500;
    const results: Array<{ symbol: string; exchange: StockExchange; quote?: Quote; error?: string }> = [];

    // Process in batches of 500
    for (let i = 0; i < symbols.length; i += BATCH_SIZE) {
      const batch = symbols.slice(i, i + BATCH_SIZE);
      const instrumentKeys = batch.map(
        ({ symbol, exchange }) => `${ZERODHA_EXCHANGE_MAP[exchange] || exchange}:${symbol}`
      );

      try {
        const response = await invoke<{
          success: boolean;
          data?: Record<string, Record<string, unknown>>;
          error?: string;
        }>('zerodha_get_quote', {
          apiKey: this.apiKey,
          accessToken: this.accessToken,
          instruments: instrumentKeys,
        });

        if (!response.success || !response.data) {
          // Mark all in batch as failed
          batch.forEach(({ symbol, exchange }) => {
            results.push({
              symbol,
              exchange,
              error: response.error || 'Failed to fetch quotes',
            });
          });
          continue;
        }

        // Parse each symbol's quote
        batch.forEach(({ symbol, exchange }) => {
          const instrumentKey = `${ZERODHA_EXCHANGE_MAP[exchange] || exchange}:${symbol}`;
          const quoteData = response.data![instrumentKey];

          if (quoteData) {
            try {
              const quote = fromZerodhaQuote(quoteData, symbol, exchange);
              results.push({ symbol, exchange, quote });
            } catch (error) {
              results.push({
                symbol,
                exchange,
                error: error instanceof Error ? error.message : 'Failed to parse quote',
              });
            }
          } else {
            results.push({
              symbol,
              exchange,
              error: 'Quote not found',
            });
          }
        });

        // Rate limit delay between batches
        if (i + BATCH_SIZE < symbols.length) {
          await new Promise(resolve => setTimeout(resolve, 1000));
        }
      } catch (error) {
        // Mark all in batch as failed
        batch.forEach(({ symbol, exchange }) => {
          results.push({
            symbol,
            exchange,
            error: error instanceof Error ? error.message : 'Request failed',
          });
        });
      }
    }

    return results;
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    // Get instrument token
    const instrument = await this.getInstrument(symbol, exchange);
    if (!instrument?.token) {
      throw new Error(`Instrument token not found for ${symbol}`);
    }

    const interval = toZerodhaInterval(timeframe);
    const allCandles: OHLCV[] = [];

    // Zerodha API has 60-day limit for historical data, so we chunk the request
    const CHUNK_DAYS = 60;
    let currentStart = new Date(from);
    const endDate = new Date(to);

    while (currentStart <= endDate) {
      // Calculate chunk end date (60 days or remaining period)
      const currentEnd = new Date(currentStart);
      currentEnd.setDate(currentEnd.getDate() + CHUNK_DAYS - 1);

      if (currentEnd > endDate) {
        currentEnd.setTime(endDate.getTime());
      }

      try {
        const response = await invoke<{
          success: boolean;
          data?: {
            candles?: unknown[][];
          };
          error?: string;
        }>('zerodha_get_historical', {
          apiKey: this.apiKey,
          accessToken: this.accessToken,
          instrumentToken: instrument.token,
          interval,
          from: currentStart.toISOString().split('T')[0],
          to: currentEnd.toISOString().split('T')[0],
        });

        if (response.success && response.data?.candles) {
          const candles = response.data.candles.map(fromZerodhaOHLCV);
          allCandles.push(...candles);
        }

        // Move to next chunk
        currentStart.setDate(currentEnd.getDate() + 1);

        // Rate limit delay between chunks
        if (currentStart <= endDate) {
          await new Promise(resolve => setTimeout(resolve, 500));
        }
      } catch (error) {
        console.error(`Failed to fetch OHLCV chunk from ${currentStart} to ${currentEnd}:`, error);
        // Continue with next chunk instead of failing completely
        currentStart.setDate(currentEnd.getDate() + 1);
      }
    }

    // Sort by timestamp and remove duplicates
    const uniqueCandles = allCandles
      .sort((a, b) => a.timestamp - b.timestamp)
      .filter((candle, index, arr) =>
        index === 0 || candle.timestamp !== arr[index - 1].timestamp
      );

    return uniqueCandles;
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    const instrumentKey = `${ZERODHA_EXCHANGE_MAP[exchange] || exchange}:${symbol}`;

    const response = await invoke<{
      success: boolean;
      data?: Record<string, Record<string, unknown>>;
      error?: string;
    }>('zerodha_get_quote', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      instruments: [instrumentKey],
    });

    if (!response.success || !response.data) {
      return { bids: [], asks: [] };
    }

    const quoteData = response.data[instrumentKey];
    if (!quoteData?.depth) {
      return { bids: [], asks: [] };
    }

    return fromZerodhaDepth(quoteData.depth as Record<string, unknown>);
  }

  // ============================================================================
  // WebSocket - Real WebSocket via Rust Backend with Polling Fallback
  // ============================================================================

  /**
   * Connect to WebSocket (internal) - Uses real Zerodha WebSocket via Rust backend
   * Falls back to REST polling if WebSocket connection fails
   */
  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureConnected();

    try {
      if (!this.apiKey || !this.accessToken) {
        throw new Error('API credentials not set');
      }

      // Try real WebSocket connection via Rust backend
      await this.connectRealWebSocket();
      console.log('[Zerodha] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[Zerodha] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  /**
   * Connect to real Zerodha WebSocket via Tauri/Rust backend
   */
  private async connectRealWebSocket(): Promise<void> {
    // Call Rust backend to connect WebSocket
    const result = await invoke<{
      success: boolean;
      data?: boolean;
      error?: string;
    }>('zerodha_ws_connect', {
      apiKey: this.apiKey!,
      accessToken: this.accessToken!,
    });

    if (!result.success) {
      throw new Error(result.error || 'WebSocket connection failed');
    }

    // Set up Tauri event listeners
    await this.setupWebSocketEventListeners();

    this.wsConnected = true;
  }

  /**
   * Disconnect from real WebSocket
   */
  private async disconnectRealWebSocket(): Promise<void> {
    // Cleanup Tauri event listeners
    await this.cleanupWebSocketEventListeners();

    // Call Rust backend to disconnect
    try {
      await invoke('zerodha_ws_disconnect');
    } catch (err) {
      console.warn('[Zerodha] WebSocket disconnect error:', err);
    }

    this.wsConnected = false;
  }

  /**
   * Start REST API polling for quotes
   */
  private startQuotePolling(): void {
    // Stop existing polling
    this.stopQuotePolling();

    // Poll every 2 minutes (120000ms)
    this.quotePollingInterval = setInterval(async () => {
      await this.pollQuotesSequentially();
    }, 120000);

    // Initial poll
    this.pollQuotesSequentially();
  }

  /**
   * Stop REST API polling
   */
  private stopQuotePolling(): void {
    if (this.quotePollingInterval) {
      clearInterval(this.quotePollingInterval);
      this.quotePollingInterval = null;
    }
  }

  /**
   * Poll quotes sequentially (not parallel) for all subscribed symbols
   */
  private async pollQuotesSequentially(): Promise<void> {
    if (this.pollingSymbols.size === 0) {
      return;
    }

    console.log(`[Zerodha] Polling ${this.pollingSymbols.size} symbols sequentially...`);

    // Process each symbol one by one
    for (const [key, { symbol, exchange }] of this.pollingSymbols) {
      try {
        // Fetch quote via REST API
        const quote = await this.getQuoteInternal(symbol, exchange);

        // Convert quote to tick and emit
        const tick: TickData = {
          symbol,
          exchange,
          mode: 'full',
          lastPrice: quote.lastPrice,
          open: quote.open,
          high: quote.high,
          low: quote.low,
          close: quote.close,
          volume: quote.volume,
          change: quote.change,
          changePercent: quote.changePercent,
          timestamp: quote.timestamp,
          bid: quote.bid,
          bidQty: quote.bidQty,
          ask: quote.ask,
          askQty: quote.askQty,
        };

        this.emitTick(tick);
      } catch (error) {
        console.error(`[Zerodha] Failed to poll ${symbol}:`, error);
      }

      // Small delay between requests to avoid rate limiting
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    console.log(`[Zerodha] ✓ Polling complete for ${this.pollingSymbols.size} symbols`);
  }

  /**
   * Set up Tauri event listeners for WebSocket data
   */
  private async setupWebSocketEventListeners(): Promise<void> {
    // Clean up any existing listeners
    await this.cleanupWebSocketEventListeners();

    // Listen for ticker (price) updates
    const tickerUnlisten = await listen<{
      provider: string;
      symbol: string;
      price: number;
      bid?: number;
      ask?: number;
      bid_size?: number;
      ask_size?: number;
      volume?: number;
      high?: number;
      low?: number;
      open?: number;
      close?: number;
      change?: number;
      change_percent?: number;
      timestamp: number;
    }>('zerodha_ticker', (event) => {
      this.handleTickerEvent(event.payload);
    });
    this.eventUnlisteners.push(tickerUnlisten);

    // Listen for status updates
    const statusUnlisten = await listen<{
      provider: string;
      status: string;
      message?: string;
      timestamp: number;
    }>('zerodha_status', (event) => {
      this.handleStatusEvent(event.payload);
    });
    this.eventUnlisteners.push(statusUnlisten);

    // Listen for orderbook updates
    const orderbookUnlisten = await listen<{
      provider: string;
      symbol: string;
      bids: Array<{ price: number; quantity: number }>;
      asks: Array<{ price: number; quantity: number }>;
      timestamp: number;
    }>('zerodha_orderbook', (event) => {
      this.handleOrderBookEvent(event.payload);
    });
    this.eventUnlisteners.push(orderbookUnlisten);

    console.log('[Zerodha] WebSocket event listeners set up');
  }

  /**
   * Handle ticker event from Rust backend
   */
  private handleTickerEvent(data: {
    symbol: string;
    price: number;
    bid?: number;
    ask?: number;
    bid_size?: number;
    ask_size?: number;
    volume?: number;
    high?: number;
    low?: number;
    open?: number;
    close?: number;
    change?: number;
    change_percent?: number;
    timestamp: number;
  }): void {
    // Parse symbol: format is "EXCHANGE:SYMBOL"
    const parts = data.symbol.split(':');
    const exchange = (parts[0] || 'NSE') as StockExchange;
    const symbol = parts[1] || data.symbol;

    // Determine mode from available data
    let mode: SubscriptionMode = 'ltp';
    if (data.bid && data.ask) {
      mode = 'full';
    } else if (data.open || data.volume) {
      mode = 'quote';
    }

    const tickData: TickData = {
      symbol,
      exchange,
      lastPrice: data.price,
      timestamp: data.timestamp,
      mode,
      bid: data.bid,
      ask: data.ask,
      bidQty: data.bid_size,
      askQty: data.ask_size,
      volume: data.volume,
      high: data.high,
      low: data.low,
      open: data.open,
      close: data.close,
      change: data.change,
      changePercent: data.change_percent,
    };

    this.emitTick(tickData);

    // Also send to paper trading engine for order fills
    this.processPaperTradingTick(data.symbol, data.price);
  }

  /**
   * Process tick for paper trading (auto-fill orders, update positions)
   */
  private async processPaperTradingTick(symbol: string, price: number): Promise<void> {
    try {
      await invoke('process_tick_for_paper_trading', { symbol, price });
    } catch (error) {
      // Silently ignore - paper trading may not be active
    }
  }

  /**
   * Handle status event from Rust backend
   */
  private handleStatusEvent(data: {
    status: string;
    message?: string;
    timestamp: number;
  }): void {
    console.log(`[Zerodha] WebSocket status: ${data.status}`, data.message || '');

    if (data.status === 'error' || data.status === 'disconnected') {
      this.wsConnected = false;

      // Auto-fallback to polling on disconnect
      if (!this.usePollingFallback && this.pollingSymbols.size > 0) {
        console.warn('[Zerodha] WebSocket disconnected, enabling polling fallback');
        this.usePollingFallback = true;
        this.startQuotePolling();
      }
    } else if (data.status === 'connected') {
      this.wsConnected = true;

      // Disable polling fallback when connected
      if (this.usePollingFallback) {
        console.log('[Zerodha] WebSocket reconnected, disabling polling fallback');
        this.usePollingFallback = false;
        this.stopQuotePolling();
      }
    }
  }

  /**
   * Handle orderbook event from Rust backend
   */
  private handleOrderBookEvent(data: {
    symbol: string;
    bids: Array<{ price: number; quantity: number }>;
    asks: Array<{ price: number; quantity: number }>;
    timestamp: number;
  }): void {
    // Parse symbol: format is "EXCHANGE:SYMBOL"
    const parts = data.symbol.split(':');
    const exchange = (parts[0] || 'NSE') as StockExchange;
    const symbol = parts[1] || data.symbol;

    // Get best bid/ask
    const bestBid = data.bids?.[0];
    const bestAsk = data.asks?.[0];

    // Emit orderbook update
    const tickData: TickData = {
      symbol,
      exchange,
      lastPrice: bestBid?.price || 0,
      bid: bestBid?.price,
      ask: bestAsk?.price,
      bidQty: bestBid?.quantity,
      askQty: bestAsk?.quantity,
      timestamp: data.timestamp,
      mode: 'full',
      depth: {
        bids: data.bids.map((b) => ({
          price: b.price,
          quantity: b.quantity,
          orders: 1,
        })),
        asks: data.asks.map((a) => ({
          price: a.price,
          quantity: a.quantity,
          orders: 1,
        })),
      },
    };

    this.emitTick(tickData);

    // Also emit depth data separately for UI components
    if (tickData.depth) {
      this.emitDepth({
        symbol,
        exchange,
        bids: tickData.depth.bids,
        asks: tickData.depth.asks,
      });
    }
  }

  /**
   * Clean up WebSocket event listeners
   */
  private async cleanupWebSocketEventListeners(): Promise<void> {
    for (const unlisten of this.eventUnlisteners) {
      unlisten();
    }
    this.eventUnlisteners = [];
  }

  /**
   * Disconnect from WebSocket
   */
  async disconnectWebSocket(): Promise<void> {
    // Disconnect real WebSocket
    await this.disconnectRealWebSocket();

    // Stop REST API polling
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.tokenSymbolMap.clear();
    this.usePollingFallback = false;

    this.wsConnected = false;
    console.log('[Zerodha] WebSocket disconnected');
  }

  /**
   * Subscribe to symbol (internal) - Uses real WebSocket or polling fallback
   */
  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    const key = `${exchange}:${symbol}`;

    // Always track in pollingSymbols for fallback
    this.pollingSymbols.set(key, { symbol, exchange });

    if (this.wsConnected && !this.usePollingFallback) {
      // Use real WebSocket subscription
      try {
        // Get instrument token for the symbol
        const tokenResponse = await invoke<{
          success: boolean;
          data?: number;
          error?: string;
        }>('zerodha_get_instrument_token', {
          symbol,
          exchange,
        });

        if (tokenResponse.success && tokenResponse.data) {
          const token = tokenResponse.data;

          // Store token-symbol mapping
          this.tokenSymbolMap.set(token, { symbol, exchange });

          // Update token map in Rust backend
          await invoke('zerodha_ws_set_token_map', {
            tokenMap: Object.fromEntries(
              Array.from(this.tokenSymbolMap.entries()).map(([t, s]) => [
                t,
                `${s.exchange}:${s.symbol}`,
              ])
            ),
          });

          // Subscribe via WebSocket
          const wsMode = toZerodhaWSMode(mode);
          await invoke('zerodha_ws_subscribe', {
            tokens: [token],
            mode: wsMode,
          });

          console.log(`[Zerodha] ✓ WebSocket subscribed to ${key} (token: ${token}, mode: ${wsMode})`);
        } else {
          console.warn(`[Zerodha] Could not get token for ${key}, using polling`);
          // Fallback to polling for this symbol
          if (!this.usePollingFallback) {
            this.pollQuotesSequentially();
          }
        }
      } catch (error) {
        console.error(`[Zerodha] WebSocket subscribe failed for ${key}:`, error);
        // Fallback to polling
        if (!this.usePollingFallback) {
          this.usePollingFallback = true;
          this.startQuotePolling();
        }
      }
    } else {
      // Using polling fallback
      console.log(`[Zerodha] Added ${key} to polling list (${this.pollingSymbols.size} symbols)`);

      // Trigger immediate poll for this symbol
      if (this.usePollingFallback) {
        this.pollQuotesSequentially();
      }
    }
  }

  /**
   * Unsubscribe from symbol (internal)
   */
  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;

    // Remove from polling symbols
    this.pollingSymbols.delete(key);

    if (this.wsConnected && !this.usePollingFallback) {
      // Find and remove token from map
      for (const [token, info] of this.tokenSymbolMap.entries()) {
        if (info.symbol === symbol && info.exchange === exchange) {
          try {
            await invoke('zerodha_ws_unsubscribe', {
              tokens: [token],
            });
            this.tokenSymbolMap.delete(token);
            console.log(`[Zerodha] ✓ WebSocket unsubscribed from ${key} (token: ${token})`);
          } catch (error) {
            console.warn(`[Zerodha] WebSocket unsubscribe failed for ${key}:`, error);
          }
          break;
        }
      }
    } else {
      console.log(`[Zerodha] Removed ${key} from polling list (${this.pollingSymbols.size} remaining)`);
    }
  }

  /**
   * Check if WebSocket is connected
   */
  async isWebSocketConnected(): Promise<boolean> {
    try {
      return await invoke<boolean>('zerodha_ws_is_connected');
    } catch {
      return false;
    }
  }

  /**
   * Handle incoming tick from WebSocket (legacy method for compatibility)
   */
  handleTick(tick: Record<string, unknown>): void {
    const instrumentToken = Number(tick.instrument_token);
    const symbolInfo = this.tokenSymbolMap.get(instrumentToken);

    if (!symbolInfo) {
      console.warn(`[Zerodha] Unknown instrument token: ${instrumentToken}`);
      return;
    }

    // Determine mode from tick data
    let mode: SubscriptionMode = 'ltp';
    if (tick.depth) {
      mode = 'full';
    } else if (tick.ohlc || tick.volume) {
      mode = 'quote';
    }

    const tickData = fromZerodhaTick(tick, symbolInfo, mode);
    this.emitTick(tickData);
  }

  // ============================================================================
  // Symbol Search & Instruments
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: Array<Record<string, unknown>>;
        error?: string;
      }>('zerodha_search_symbols', {
        query,
        exchange: exchange || null,
      });

      if (!response.success || !response.data) {
        console.warn('[Zerodha] Symbol search failed:', response.error);
        return [];
      }

      return response.data.map(item => ({
        symbol: String(item.symbol || ''),
        exchange: String(item.exchange || 'NSE') as StockExchange,
        name: String(item.name || item.symbol || ''),
        token: String(item.token || item.instrument_token || '0'),
        lotSize: Number(item.lot_size || 1),
        tickSize: Number(item.tick_size || 0.05),
        instrumentType: String(item.instrument_type || 'EQ') as InstrumentType,
        currency: 'INR' as const,
        expiry: item.expiry ? String(item.expiry) : undefined,
        strike: item.strike ? Number(item.strike) : undefined,
      }));
    } catch (error) {
      console.error('[Zerodha] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('zerodha_get_instrument', {
        symbol,
        exchange,
      });

      if (!response.success || !response.data) {
        console.warn('[Zerodha] Instrument lookup failed:', response.error);
        // Fallback to basic instrument
        return {
          symbol,
          exchange,
          name: symbol,
          token: '0',
          lotSize: 1,
          tickSize: 0.05,
          instrumentType: 'EQ' as any,
          currency: 'INR',
        };
      }

      const data = response.data;
      return {
        symbol: String(data.symbol || symbol),
        exchange: String(data.exchange || exchange) as StockExchange,
        name: String(data.name || symbol),
        token: String(data.token || data.instrument_token || '0'),
        lotSize: Number(data.lot_size || 1),
        tickSize: Number(data.tick_size || 0.05),
        instrumentType: String(data.instrument_type || 'EQ') as InstrumentType,
        currency: 'INR' as const,
        expiry: data.expiry ? String(data.expiry) : undefined,
        strike: data.strike ? Number(data.strike) : undefined,
      };
    } catch (error) {
      console.error('[Zerodha] getInstrument error:', error);
      // Return fallback
      return {
        symbol,
        exchange,
        name: symbol,
        token: '0',
        lotSize: 1,
        tickSize: 0.05,
        instrumentType: 'EQ' as any,
        currency: 'INR',
      };
    }
  }

  // ============================================================================
  // Master Contract
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    await invoke('zerodha_download_master_contract', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });
  }

  /**
   * Get last updated timestamp of master contract
   */
  async getMasterContractLastUpdated(): Promise<Date | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: {
          last_updated: number;
          symbol_count: number;
          age_seconds: number;
        };
        error?: string;
      }>('zerodha_get_master_contract_metadata');

      if (!response.success || !response.data) {
        return null;
      }

      // last_updated is Unix timestamp in seconds
      return new Date(response.data.last_updated * 1000);
    } catch (error) {
      console.error('[Zerodha] getMasterContractLastUpdated error:', error);
      return null;
    }
  }
}

// Export singleton instance
export const zerodhaAdapter = new ZerodhaAdapter();
