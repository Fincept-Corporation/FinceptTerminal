/**
 * IIFL Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for IIFL XTS Trading API
 * Reference: https://symphonyfintech.com/xts-trading-front-end-api/
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
  TickData,
  Instrument,
  InstrumentType,
  BulkOperationResult,
  SmartOrderParams,
} from '../../types';
import {
  IIFL_METADATA,
  IIFL_EXCHANGE_SEGMENT_ID,
} from './constants';
import {
  toIIFLOrderParams,
  toIIFLModifyParams,
  toIIFLInterval,
  fromIIFLOrder,
  fromIIFLTrade,
  fromIIFLPosition,
  fromIIFLHolding,
  fromIIFLFunds,
  fromIIFLQuote,
  fromIIFLOHLCV,
  fromIIFLDepth,
  getExchangeSegmentId,
} from './mapper';

/**
 * IIFL broker adapter
 */
export class IIFLAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'iifl';
  readonly brokerName = 'IIFL Securities';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = IIFL_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;
  private marketApiKey: string | null = null;
  private marketApiSecret: string | null = null;
  private feedToken: string | null = null;
  // Note: accessToken is inherited from BaseStockBrokerAdapter as protected

  // Symbol to Token mapping (instrument ID)
  private tokenCache: Map<string, number> = new Map();

  // Tauri event unlisteners for WebSocket events
  private tickerUnlisten: UnlistenFn | null = null;
  private orderbookUnlisten: UnlistenFn | null = null;
  private statusUnlisten: UnlistenFn | null = null;

  // REST API polling for quotes (fallback)
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange; token: number }> = new Map();
  private usePollingFallback: boolean = false;

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Get login URL for IIFL - API key auth doesn't need browser login
   */
  getAuthUrl(): string {
    return 'https://ttblaze.iifl.com';
  }

  /**
   * Set API credentials before authentication
   */
  setCredentials(apiKey: string, apiSecret: string): void {
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
  }

  /**
   * Set market data credentials (separate from trading credentials)
   */
  setMarketDataCredentials(apiKey: string, apiSecret: string): void {
    this.marketApiKey = apiKey;
    this.marketApiSecret = apiSecret;
  }

  /**
   * Authenticate with IIFL XTS API
   * IIFL uses API Key + Secret authentication
   * 1. Get session token for trading
   * 2. Get feed token for market data (optional)
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey || null;
      this.apiSecret = credentials.apiSecret || null;
      this.userId = credentials.userId || null;

      // Optional: Market data credentials (can be same or different)
      const marketCreds = credentials as any;
      this.marketApiKey = marketCreds.marketApiKey || this.apiKey;
      this.marketApiSecret = marketCreds.marketApiSecret || this.apiSecret;

      // If access token already provided, validate it
      if (credentials.accessToken) {
        console.log('[IIFLAdapter] Access token provided, validating...');
        const isValid = await this.validateToken(credentials.accessToken);
        if (isValid) {
          this.accessToken = credentials.accessToken;
          this.feedToken = marketCreds.feedToken || null;
          this._isConnected = true;

          await this.storeCredentials(credentials);

          return {
            success: true,
            accessToken: this.accessToken,
            userId: this.userId || undefined,
            message: 'Authentication successful',
          };
        } else {
          console.error('[IIFLAdapter] Token validation failed');
          return {
            success: false,
            message: 'Invalid or expired token',
            errorCode: 'AUTH_FAILED',
          };
        }
      }

      // Need API credentials for new login
      if (!this.apiKey || !this.apiSecret) {
        return {
          success: false,
          message: 'API Key and Secret are required for authentication',
          errorCode: 'AUTH_REQUIRED',
        };
      }

      // Step 1: Get session token for trading
      console.log('[IIFLAdapter] Getting session token...');
      const sessionResponse = await invoke<{
        success: boolean;
        access_token?: string;
        user_id?: string;
        error?: string;
      }>('iifl_exchange_token', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        requestToken: '', // Not used for IIFL
      });

      if (!sessionResponse.success || !sessionResponse.access_token) {
        return {
          success: false,
          message: sessionResponse.error || 'Failed to get session token',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.accessToken = sessionResponse.access_token;
      this.userId = sessionResponse.user_id || this.userId;

      console.log('[IIFLAdapter] Session token obtained');

      // Step 2: Get feed token for market data (optional)
      if (this.marketApiKey && this.marketApiSecret) {
        console.log('[IIFLAdapter] Getting feed token...');
        try {
          const feedResponse = await invoke<{
            success: boolean;
            access_token?: string;
            user_id?: string;
            error?: string;
          }>('iifl_get_feed_token', {
            apiKey: this.marketApiKey,
            apiSecret: this.marketApiSecret,
          });

          if (feedResponse.success && feedResponse.access_token) {
            this.feedToken = feedResponse.access_token;
            console.log('[IIFLAdapter] Feed token obtained');
          } else {
            console.warn('[IIFLAdapter] Could not get feed token:', feedResponse.error);
          }
        } catch (feedError) {
          console.warn('[IIFLAdapter] Feed token error:', feedError);
        }
      }

      this._isConnected = true;

      console.log('[IIFLAdapter] Authentication successful');

      // Store credentials
      await this.storeCredentials({
        apiKey: this.apiKey || '',
        userId: this.userId || undefined,
        apiSecret: this.apiSecret || undefined,
        accessToken: this.accessToken,
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
        message: error instanceof Error ? error.message : 'Authentication failed',
        errorCode: 'AUTH_FAILED',
      };
    }
  }

  /**
   * Handle OAuth callback - IIFL uses API key auth, not OAuth
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    return {
      success: false,
      message: 'IIFL uses API Key authentication, not OAuth',
      errorCode: 'AUTH_FAILED',
    };
  }

  /**
   * Validate access token
   */
  private async validateToken(accessToken: string): Promise<boolean> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: boolean;
        error?: string;
      }>('iifl_validate_token', {
        accessToken,
      });
      return response.success && response.data === true;
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

      this.apiKey = credentials.apiKey || null;
      this.apiSecret = credentials.apiSecret || null;
      this.userId = credentials.userId || null;
      this.accessToken = credentials.accessToken || null;

      if (this.accessToken) {
        console.log('[IIFL] Validating stored token...');
        const isValid = await this.validateToken(this.accessToken);

        if (!isValid) {
          console.warn('[IIFL] Token expired or invalid, clearing from storage...');

          await this.storeCredentials({
            apiKey: this.apiKey || '',
            userId: this.userId || undefined,
            apiSecret: this.apiSecret || undefined,
          });

          this.accessToken = null;
          this._isConnected = false;

          console.log('[IIFL] Please re-authenticate to continue');
          return false;
        }

        console.log('[IIFL] Token is valid, session restored');
        this._isConnected = isValid;
        return isValid;
      }

      return false;
    } catch {
      return false;
    }
  }

  // ============================================================================
  // Token Helper (IIFL uses instrument ID)
  // ============================================================================

  /**
   * Get token (instrument ID) for a symbol
   */
  private getToken(symbol: string, exchange: StockExchange): number {
    const key = `${exchange}:${symbol}`;
    return this.tokenCache.get(key) || 0;
  }

  /**
   * Set token (instrument ID) for a symbol
   */
  setToken(symbol: string, exchange: StockExchange, token: number): void {
    const key = `${exchange}:${symbol}`;
    this.tokenCache.set(key, token);
  }

  // ============================================================================
  // Orders
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    const token = this.getToken(params.symbol, params.exchange);
    if (!token) {
      return {
        success: false,
        message: 'Token not found for symbol. Please set token first.',
      };
    }

    const orderParams = toIIFLOrderParams(params, token);

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('iifl_place_order', {
      accessToken: this.accessToken,
      params: orderParams,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const modifyParams = toIIFLModifyParams(params);

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('iifl_modify_order', {
      accessToken: this.accessToken,
      orderId: params.orderId,
      params: modifyParams,
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
    }>('iifl_cancel_order', {
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
    }>('iifl_get_orders', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data || !Array.isArray(response.data)) {
      return [];
    }

    return response.data.map(fromIIFLOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('iifl_get_trade_book', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data || !Array.isArray(response.data)) {
      return [];
    }

    return response.data.map(fromIIFLTrade);
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    const response = await invoke<{
      success: boolean;
      data?: Array<{ success: boolean; order_id?: string; error?: string }>;
      error?: string;
    }>('iifl_cancel_all_orders', {
      accessToken: this.accessToken,
    });

    const results = response.data || [];

    return {
      success: response.success,
      totalCount: results.length,
      successCount: results.filter(r => r.success).length,
      failedCount: results.filter(r => !r.success).length,
      results: results.map(r => ({
        success: r.success,
        orderId: r.order_id,
        error: r.error,
      })),
    };
  }

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    const response = await invoke<{
      success: boolean;
      data?: Array<{ success: boolean; order_id?: string; error?: string }>;
      error?: string;
    }>('iifl_close_all_positions', {
      accessToken: this.accessToken,
    });

    const results = response.data || [];

    return {
      success: response.success,
      totalCount: results.length,
      successCount: results.filter(r => r.success).length,
      failedCount: results.filter(r => !r.success).length,
      results: results.map(r => ({
        success: r.success,
        orderId: r.order_id,
        error: r.error,
      })),
    };
  }

  // ============================================================================
  // Smart Order
  // ============================================================================

  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    // Get current position size if not provided
    let positionSize = params.positionSize ?? 0;
    if (params.positionSize === undefined) {
      const position = await this.getOpenPosition(params.symbol, params.exchange, params.productType);
      positionSize = position?.quantity ?? 0;
    }

    // Determine the action based on position size and desired side
    const side = params.side;
    const quantity = params.quantity;

    return this.placeOrderInternal({
      symbol: params.symbol,
      exchange: params.exchange,
      side,
      quantity,
      orderType: params.orderType,
      productType: params.productType,
      price: params.price,
      triggerPrice: params.triggerPrice,
    });
  }

  // ============================================================================
  // Positions & Holdings
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('iifl_get_positions', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    // IIFL returns positions in positionList array
    const positionList = (response.data as any).positionList || [];
    if (!Array.isArray(positionList)) {
      return [];
    }

    return positionList.map(fromIIFLPosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('iifl_get_holdings', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    // Extract holdings from RMSHoldings structure
    const rmsHoldings = (response.data as any).RMSHoldings?.Holdings || {};
    const holdingsArray = Object.values(rmsHoldings);

    if (!Array.isArray(holdingsArray) || holdingsArray.length === 0) {
      return [];
    }

    return holdingsArray.map(h => fromIIFLHolding(h as Record<string, unknown>));
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('iifl_get_funds', {
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

    return fromIIFLFunds(response.data);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // IIFL doesn't have a dedicated margin calculator API
    return {
      totalMargin: 0,
      initialMargin: 0,
    };
  }

  // ============================================================================
  // Market Data
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    const token = this.getToken(symbol, exchange);
    if (!token) {
      throw new Error('Token not found for symbol. Please set token first.');
    }

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('iifl_get_quotes', {
      feedToken: this.feedToken || this.accessToken,
      exchange,
      token,
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || 'Failed to fetch quote');
    }

    return fromIIFLQuote(response.data, symbol, exchange);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    const token = this.getToken(symbol, exchange);
    if (!token) {
      return [];
    }

    const interval = toIIFLInterval(timeframe);

    // Format dates as "MMM DD YYYY HHMMSS" (IIFL expects IST)
    const formatDate = (date: Date): string => {
      const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
      const month = months[date.getMonth()];
      const day = String(date.getDate()).padStart(2, '0');
      const year = date.getFullYear();
      const hours = String(date.getHours()).padStart(2, '0');
      const minutes = String(date.getMinutes()).padStart(2, '0');
      const seconds = String(date.getSeconds()).padStart(2, '0');
      return `${month} ${day} ${year} ${hours}${minutes}${seconds}`;
    };

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('iifl_get_history', {
      feedToken: this.feedToken || this.accessToken,
      exchange,
      token,
      timeframe: interval,
      fromDate: formatDate(from),
      toDate: formatDate(to),
    });

    if (!response.success || !response.data || !Array.isArray(response.data)) {
      return [];
    }

    return response.data.map(fromIIFLOHLCV);
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    const token = this.getToken(symbol, exchange);
    if (!token) {
      return { bids: [], asks: [] };
    }

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('iifl_get_depth', {
      feedToken: this.feedToken || this.accessToken,
      exchange,
      token,
    });

    if (!response.success || !response.data) {
      return { bids: [], asks: [] };
    }

    return fromIIFLDepth(response.data);
  }

  // ============================================================================
  // WebSocket - Real WebSocket via Rust Backend with Polling Fallback
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureConnected();

    try {
      await this.connectRealWebSocket();
      console.log('[IIFL] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[IIFL] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  private async connectRealWebSocket(): Promise<void> {
    if (!this.accessToken) {
      throw new Error('Access token required');
    }

    const result = await invoke<{ success: boolean; data?: boolean; error?: string }>('iifl_ws_connect', {
      accessToken: this.accessToken,
      feedToken: this.feedToken || this.accessToken,
    });

    if (!result.success) throw new Error(result.error || 'WebSocket connection failed');

    this.tickerUnlisten = await listen<{
      provider: string; symbol: string; price: number; bid?: number; ask?: number;
      volume?: number; change_percent?: number; timestamp: number;
    }>('iifl_ticker', (event) => {
      const tick = event.payload;
      this.emitTick({
        symbol: tick.symbol, exchange: 'NSE' as StockExchange, mode: 'quote',
        lastPrice: tick.price, bid: tick.bid, ask: tick.ask,
        volume: tick.volume || 0, changePercent: tick.change_percent || 0, timestamp: tick.timestamp,
      });
    });

    this.orderbookUnlisten = await listen<{
      provider: string; symbol: string;
      bids: Array<{ price: number; quantity: number; orders?: number }>; asks: Array<{ price: number; quantity: number; orders?: number }>;
      timestamp: number;
    }>('iifl_orderbook', (event) => {
      const data = event.payload;
      const [exchangePart, symbolPart] = data.symbol.includes(':') ? data.symbol.split(':') : ['NSE', data.symbol];
      const symbol = symbolPart || data.symbol;
      const exchange = (exchangePart as StockExchange) || 'NSE';

      console.log(`[IIFL WebSocket] Depth update for ${symbol}`);

      const bestBid = data.bids?.[0];
      const bestAsk = data.asks?.[0];

      const tick: TickData = {
        symbol, exchange, mode: 'full',
        lastPrice: bestBid?.price || 0,
        bid: bestBid?.price, ask: bestAsk?.price,
        bidQty: bestBid?.quantity, askQty: bestAsk?.quantity,
        timestamp: data.timestamp,
        depth: {
          bids: data.bids?.map((b) => ({ price: b.price, quantity: b.quantity, orders: b.orders || 0 })) || [],
          asks: data.asks?.map((a) => ({ price: a.price, quantity: a.quantity, orders: a.orders || 0 })) || [],
        },
      };

      this.emitTick(tick);
      if (tick.depth) {
        this.emitDepth({ symbol, exchange, bids: tick.depth.bids, asks: tick.depth.asks });
      }
    });

    this.statusUnlisten = await listen<{
      provider: string; status: string; message?: string; timestamp: number;
    }>('iifl_status', (event) => {
      const status = event.payload;
      if (status.status === 'disconnected' || status.status === 'error') {
        this.wsConnected = false;
        if (!this.usePollingFallback) { this.usePollingFallback = true; this.startQuotePolling(); }
      } else if (status.status === 'connected') {
        this.wsConnected = true;
        if (this.usePollingFallback) { this.usePollingFallback = false; this.stopQuotePolling(); }
      }
    });

    this.wsConnected = true;
  }

  private async disconnectRealWebSocket(): Promise<void> {
    if (this.tickerUnlisten) { this.tickerUnlisten(); this.tickerUnlisten = null; }
    if (this.orderbookUnlisten) { this.orderbookUnlisten(); this.orderbookUnlisten = null; }
    if (this.statusUnlisten) { this.statusUnlisten(); this.statusUnlisten = null; }
    try { await invoke('iifl_ws_disconnect'); } catch (err) { console.warn('[IIFL] WS disconnect error:', err); }
    this.wsConnected = false;
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();
    console.log('[IIFL] Starting REST API polling (fallback mode, 2 min interval)');
    this.quotePollingInterval = setInterval(async () => { await this.pollQuotesSequentially(); }, 120000);
    this.pollQuotesSequentially();
  }

  private stopQuotePolling(): void {
    if (this.quotePollingInterval) { clearInterval(this.quotePollingInterval); this.quotePollingInterval = null; }
  }

  private async pollQuotesSequentially(): Promise<void> {
    if (this.pollingSymbols.size === 0) return;

    for (const [key, { symbol, exchange, token }] of this.pollingSymbols) {
      try {
        this.setToken(symbol, exchange, token);
        const quote = await this.getQuoteInternal(symbol, exchange);
        this.emitTick({
          symbol, exchange, mode: 'full',
          lastPrice: quote.lastPrice, open: quote.open, high: quote.high, low: quote.low,
          close: quote.close, volume: quote.volume, change: quote.change,
          changePercent: quote.changePercent, timestamp: quote.timestamp,
          bid: quote.bid, bidQty: quote.bidQty, ask: quote.ask, askQty: quote.askQty,
        });
      } catch (error) { console.error(`[IIFL] Failed to poll ${symbol}:`, error); }
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    console.log(`[IIFL] Polling complete for ${this.pollingSymbols.size} symbols`);
  }

  async disconnectWebSocket(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    console.log('[IIFL] WebSocket disconnected');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    try {
      const token = this.getToken(symbol, exchange);
      if (!token) {
        console.warn(`[IIFL] Token not found for ${symbol}. Set token first.`);
        return;
      }

      const key = `${exchange}:${symbol}`;
      this.pollingSymbols.set(key, { symbol, exchange, token });

      // If real WebSocket is connected, subscribe via Tauri command
      if (this.wsConnected && !this.usePollingFallback) {
        try {
          await invoke('iifl_ws_subscribe', {
            symbol: `${exchange}:${symbol}`,
            mode: mode === 'full' ? 'depth' : mode === 'quote' ? 'quotes' : 'ltp',
          });
          console.log(`[IIFL] WebSocket subscribed to ${key}`);
        } catch (err) {
          console.warn(`[IIFL] WebSocket subscribe failed for ${key}, using polling:`, err);
          this.pollQuotesSequentially();
        }
      } else {
        console.log(`IIFL: Added ${key} to polling list (${this.pollingSymbols.size} symbols total)`);
        this.pollQuotesSequentially();
      }
    } catch (error) {
      console.error('Failed to subscribe:', error);
      throw error;
    }
  }

  /**
   * Subscribe with token (IIFL specific)
   */
  async subscribeWithToken(
    symbol: string,
    exchange: StockExchange,
    token: number,
    mode: SubscriptionMode = 'full'
  ): Promise<void> {
    this.setToken(symbol, exchange, token);
    await this.subscribeInternal(symbol, exchange, mode);
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);

    // If real WebSocket is connected, unsubscribe via Tauri command
    if (this.wsConnected && !this.usePollingFallback) {
      try {
        await invoke('iifl_ws_unsubscribe', { symbol: `${exchange}:${symbol}` });
        console.log(`[IIFL] WebSocket unsubscribed from ${key}`);
      } catch (err) {
        console.warn(`[IIFL] WebSocket unsubscribe failed for ${key}:`, err);
      }
    } else {
      console.log(`IIFL: Removed ${key} from polling list (${this.pollingSymbols.size} symbols remaining)`);
    }
  }

  // ============================================================================
  // Symbol Search & Instruments
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>[];
        error?: string;
      }>('iifl_search_symbol', {
        query,
        exchange: exchange || null,
      });

      if (!response.success || !response.data) {
        return [];
      }

      return response.data.map(item => ({
        symbol: String(item.symbol || item.name || ''),
        exchange: (item.exchange || 'NSE') as StockExchange,
        name: String(item.name || item.description || ''),
        token: String(item.token || item.exchangeInstrumentID || '0'),
        lotSize: Number(item.lotSize || 1),
        tickSize: Number(item.tickSize || 0.05),
        instrumentType: (item.instrumentType || 'EQUITY') as InstrumentType,
        currency: 'INR',
      }));
    } catch (error) {
      console.error('[IIFL] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const token = this.getToken(symbol, exchange);
      // Return basic instrument
      return {
        symbol,
        exchange,
        name: symbol,
        token: token ? token.toString() : '0',
        lotSize: 1,
        tickSize: 0.05,
        instrumentType: 'EQUITY' as InstrumentType,
        currency: 'INR',
      };
    } catch (error) {
      console.error('[IIFL] getInstrument error:', error);
      return null;
    }
  }

  // ============================================================================
  // Master Contract
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    console.log('[IIFL] Downloading master contract...');

    const exchanges = ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'];

    for (const exchange of exchanges) {
      try {
        const response = await invoke<{
          success: boolean;
          data?: Record<string, unknown>[];
          error?: string;
        }>('iifl_download_master_contract', {
          exchange,
        });

        if (response.success && response.data) {
          console.log(`[IIFL] Downloaded ${response.data.length} instruments for ${exchange}`);

          // Cache tokens
          for (const inst of response.data) {
            const symbol = String(inst.symbol || inst.name || '');
            const token = Number(inst.token || inst.exchangeInstrumentID || 0);
            if (symbol && token) {
              this.setToken(symbol, exchange as StockExchange, token);
            }
          }
        }
      } catch (error) {
        console.error(`[IIFL] Failed to download ${exchange} master contract:`, error);
      }
    }

    console.log('[IIFL] Master contract download complete');
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    return null;
  }
}

// Export singleton instance
export const iiflAdapter = new IIFLAdapter();
