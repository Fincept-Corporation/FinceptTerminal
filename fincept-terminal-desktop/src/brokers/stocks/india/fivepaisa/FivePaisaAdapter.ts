/**
 * 5Paisa Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for 5Paisa Trading API
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
  FIVEPAISA_METADATA,
  FIVEPAISA_EXCHANGE_MAP,
} from './constants';
import {
  toFivePaisaOrderParams,
  toFivePaisaModifyParams,
  toFivePaisaInterval,
  fromFivePaisaOrder,
  fromFivePaisaTrade,
  fromFivePaisaPosition,
  fromFivePaisaHolding,
  fromFivePaisaFunds,
  fromFivePaisaQuote,
  fromFivePaisaOHLCV,
  fromFivePaisaDepth,
} from './mapper';

/**
 * 5Paisa broker adapter
 */
export class FivePaisaAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'fivepaisa';
  readonly brokerName = '5Paisa';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = FIVEPAISA_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;
  private clientId: string | null = null;
  // Note: accessToken is inherited from BaseStockBrokerAdapter as protected

  // Symbol to ScripCode mapping (required for 5Paisa API)
  private scripCodeCache: Map<string, number> = new Map();

  // Tauri event unlisteners for WebSocket events
  private tickerUnlisten: UnlistenFn | null = null;
  private orderbookUnlisten: UnlistenFn | null = null;
  private statusUnlisten: UnlistenFn | null = null;

  // REST API polling for quotes (fallback)
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange; scripCode: number }> = new Map();
  private usePollingFallback: boolean = false;

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Get login URL for 5Paisa TOTP auth
   */
  getAuthUrl(): string {
    return 'https://dev-openapi.5paisa.com/WebVendorLogin/VLogin/Index';
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
   * 5Paisa uses two-step TOTP authentication:
   * 1. TOTPLogin - Get request token
   * 2. GetAccessToken - Exchange request token for access token
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey || null;
      this.apiSecret = credentials.apiSecret || null;
      this.userId = credentials.userId || null;
      this.clientId = credentials.userId || null;

      // If access token already provided, validate it
      if (credentials.accessToken) {
        console.log('[FivePaisaAdapter] Access token provided, validating...');
        const isValid = await this.validateToken(credentials.accessToken);
        if (isValid) {
          this.accessToken = credentials.accessToken;
          this._isConnected = true;

          await this.storeCredentials(credentials);

          return {
            success: true,
            accessToken: this.accessToken,
            userId: this.clientId || undefined,
            message: 'Authentication successful',
          };
        } else {
          console.error('[FivePaisaAdapter] Token validation failed');
          return {
            success: false,
            message: 'Invalid or expired token',
            errorCode: 'AUTH_FAILED',
          };
        }
      }

      // Need TOTP for new login
      const totp = (credentials as any).totp;
      const pin = credentials.pin;
      const email = (credentials as any).email;

      if (!this.apiKey || !totp || !pin || !email) {
        return {
          success: false,
          message: 'API Key, Email, PIN, and TOTP are required for authentication',
          errorCode: 'AUTH_REQUIRED',
        };
      }

      // Step 1: TOTP Login - Get request token
      console.log('[FivePaisaAdapter] Step 1: TOTP Login...');
      const totpResponse = await invoke<{
        success: boolean;
        data?: { request_token: string };
        error?: string;
      }>('fivepaisa_totp_login', {
        apiKey: this.apiKey,
        email,
        pin,
        totp,
      });

      if (!totpResponse.success || !totpResponse.data?.request_token) {
        return {
          success: false,
          message: totpResponse.error || 'TOTP login failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      const requestToken = totpResponse.data.request_token;
      console.log('[FivePaisaAdapter] Step 1 complete, got request token');

      // Step 2: Exchange request token for access token
      console.log('[FivePaisaAdapter] Step 2: Getting access token...');
      const accessResponse = await invoke<{
        success: boolean;
        data?: { access_token: string; user_id: string };
        error?: string;
      }>('fivepaisa_get_access_token', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        userId: this.userId || this.clientId,
        requestToken,
      });

      if (!accessResponse.success || !accessResponse.data?.access_token) {
        return {
          success: false,
          message: accessResponse.error || 'Failed to get access token',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.accessToken = accessResponse.data.access_token;
      this.clientId = accessResponse.data.user_id || this.clientId;
      this._isConnected = true;

      console.log('[FivePaisaAdapter] Authentication successful');

      // Store credentials
      await this.storeCredentials({
        apiKey: this.apiKey || '',
        userId: this.clientId || undefined,
        apiSecret: this.apiSecret || undefined,
        accessToken: this.accessToken,
      });

      return {
        success: true,
        accessToken: this.accessToken,
        userId: this.clientId || undefined,
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
   * Handle OAuth callback - 5Paisa doesn't use OAuth, but method required by interface
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    // 5Paisa uses TOTP authentication, not OAuth
    return {
      success: false,
      message: '5Paisa uses TOTP authentication, not OAuth',
      errorCode: 'AUTH_FAILED',
    };
  }

  /**
   * Validate access token (uses base class date check + API validation)
   */
  private async validateToken(_accessToken: string): Promise<boolean> {
    return this.validateTokenWithDateCheck();
  }

  /**
   * 5Paisa-specific API token validation
   */
  protected override async validateTokenWithApi(token: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean }>('fivepaisa_validate_token', {
        apiKey: this.apiKey,
        clientId: this.clientId,
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

      this.apiKey = credentials.apiKey || null;
      this.apiSecret = credentials.apiSecret || null;
      this.userId = credentials.userId || null;
      this.clientId = credentials.userId || null;
      this.accessToken = credentials.accessToken || null;

      if (this.accessToken) {
        console.log('[5Paisa] Validating stored token...');
        const isValid = await this.validateToken(this.accessToken);

        if (!isValid) {
          console.warn('[5Paisa] Token expired or invalid, clearing from storage...');

          await this.storeCredentials({
            apiKey: this.apiKey || '',
            userId: this.clientId || undefined,
            apiSecret: this.apiSecret || undefined,
          });

          this.accessToken = null;
          this._isConnected = false;

          console.log('[5Paisa] Please re-authenticate to continue');
          return false;
        }

        console.log('[5Paisa] Token is valid, session restored');
        this._isConnected = isValid;
        return isValid;
      }

      return false;
    } catch {
      return false;
    }
  }

  // ============================================================================
  // ScripCode Helper (5Paisa uses ScripCode instead of symbol)
  // ============================================================================

  /**
   * Get ScripCode for a symbol
   * In production, this would query a master contract database
   */
  private getScripCode(symbol: string, exchange: StockExchange): number {
    const key = `${exchange}:${symbol}`;
    return this.scripCodeCache.get(key) || 0;
  }

  /**
   * Set ScripCode for a symbol
   */
  setScripCode(symbol: string, exchange: StockExchange, scripCode: number): void {
    const key = `${exchange}:${symbol}`;
    this.scripCodeCache.set(key, scripCode);
  }

  // ============================================================================
  // Orders
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    const scripCode = this.getScripCode(params.symbol, params.exchange);
    if (!scripCode) {
      return {
        success: false,
        message: 'ScripCode not found for symbol. Please set scripCode first.',
      };
    }

    const response = await invoke<{
      success: boolean;
      data?: { order_id: string };
      error?: string;
    }>('fivepaisa_place_order', {
      apiKey: this.apiKey,
      clientId: this.clientId,
      accessToken: this.accessToken,
      exchange: params.exchange,
      symbol: params.symbol,
      scripCode,
      side: params.side,
      quantity: params.quantity,
      price: params.price || 0,
      triggerPrice: params.triggerPrice || 0,
      product: params.productType,
      disclosedQuantity: params.disclosedQuantity,
    });

    return {
      success: response.success,
      orderId: response.data?.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const response = await invoke<{
      success: boolean;
      data?: { order_id: string };
      error?: string;
    }>('fivepaisa_modify_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      exchangeOrderId: params.orderId,
      quantity: params.quantity || 0,
      price: params.price || 0,
      triggerPrice: params.triggerPrice || 0,
      disclosedQuantity: undefined,
    });

    return {
      success: response.success,
      orderId: response.data?.order_id,
      message: response.error,
    };
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    const response = await invoke<{
      success: boolean;
      data?: { order_id: string };
      error?: string;
    }>('fivepaisa_cancel_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      exchangeOrderId: orderId,
    });

    return {
      success: response.success,
      orderId: response.data?.order_id,
      message: response.error,
    };
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('fivepaisa_get_orders', {
      apiKey: this.apiKey,
      clientId: this.clientId,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data || !Array.isArray(response.data)) {
      return [];
    }

    return response.data.map(fromFivePaisaOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('fivepaisa_get_trades', {
      apiKey: this.apiKey,
      clientId: this.clientId,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data || !Array.isArray(response.data)) {
      return [];
    }

    return response.data.map(fromFivePaisaTrade);
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    // Get all open orders and cancel them
    const orders = await this.getOrdersInternal();
    const openOrders = orders.filter(o => o.status === 'OPEN' || o.status === 'PENDING');

    const results: Array<{ success: boolean; orderId?: string; error?: string }> = [];

    for (const order of openOrders) {
      try {
        const result = await this.cancelOrderInternal(order.orderId);
        results.push({
          success: result.success,
          orderId: order.orderId,
          error: result.message,
        });
      } catch (error) {
        results.push({
          success: false,
          orderId: order.orderId,
          error: error instanceof Error ? error.message : 'Cancel failed',
        });
      }
    }

    return {
      success: true,
      totalCount: results.length,
      successCount: results.filter(r => r.success).length,
      failedCount: results.filter(r => !r.success).length,
      results,
    };
  }

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    // Get all positions and close them
    const positions = await this.getPositionsInternal();
    const openPositions = positions.filter(p => p.quantity !== 0);

    const results: Array<{ success: boolean; orderId?: string; error?: string }> = [];

    for (const position of openPositions) {
      try {
        const side = position.quantity > 0 ? 'SELL' : 'BUY';
        const result = await this.placeOrderInternal({
          symbol: position.symbol,
          exchange: position.exchange,
          side,
          quantity: Math.abs(position.quantity),
          orderType: 'MARKET',
          productType: position.productType,
        });
        results.push({
          success: result.success,
          orderId: result.orderId,
          error: result.message,
        });
      } catch (error) {
        results.push({
          success: false,
          error: error instanceof Error ? error.message : 'Close failed',
        });
      }
    }

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
      data?: Record<string, unknown>[];
      error?: string;
    }>('fivepaisa_get_positions', {
      apiKey: this.apiKey,
      clientId: this.clientId,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data || !Array.isArray(response.data)) {
      return [];
    }

    return response.data.map(fromFivePaisaPosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('fivepaisa_get_holdings', {
      apiKey: this.apiKey,
      clientId: this.clientId,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data || !Array.isArray(response.data)) {
      return [];
    }

    return response.data.map(fromFivePaisaHolding);
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('fivepaisa_get_margins', {
      apiKey: this.apiKey,
      clientId: this.clientId,
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

    return fromFivePaisaFunds(response.data);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // 5Paisa doesn't have a dedicated margin calculator API
    // Return basic structure
    return {
      totalMargin: 0,
      initialMargin: 0,
    };
  }

  // ============================================================================
  // Market Data
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    const scripCode = this.getScripCode(symbol, exchange);
    if (!scripCode) {
      throw new Error('ScripCode not found for symbol. Please set scripCode first.');
    }

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('fivepaisa_get_quote', {
      apiKey: this.apiKey,
      clientId: this.clientId,
      accessToken: this.accessToken,
      exchange,
      scripCode,
      scripData: undefined,
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || 'Failed to fetch quote');
    }

    return fromFivePaisaQuote(response.data, symbol, exchange);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    const scripCode = this.getScripCode(symbol, exchange);
    if (!scripCode) {
      return [];
    }

    const interval = toFivePaisaInterval(timeframe);

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('fivepaisa_get_historical', {
      apiKey: this.apiKey,
      clientId: this.clientId,
      accessToken: this.accessToken,
      exchange,
      scripCode,
      resolution: interval,
      fromTimestamp: Math.floor(from.getTime() / 1000),
      toTimestamp: Math.floor(to.getTime() / 1000),
    });

    if (!response.success || !response.data || !Array.isArray(response.data)) {
      return [];
    }

    return response.data.map(fromFivePaisaOHLCV);
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    const scripCode = this.getScripCode(symbol, exchange);
    if (!scripCode) {
      return { bids: [], asks: [] };
    }

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('fivepaisa_get_quote', {
      apiKey: this.apiKey,
      clientId: this.clientId,
      accessToken: this.accessToken,
      exchange,
      scripCode,
      scripData: undefined,
    });

    if (!response.success || !response.data) {
      return { bids: [], asks: [] };
    }

    return fromFivePaisaDepth(response.data);
  }

  // ============================================================================
  // WebSocket - Real WebSocket via Rust Backend with Polling Fallback
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureConnected();

    try {
      await this.connectRealWebSocket();
      console.log('[5Paisa] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[5Paisa] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  private async connectRealWebSocket(): Promise<void> {
    if (!this.accessToken || !this.clientId) {
      throw new Error('Access token and client ID required');
    }

    const result = await invoke<{ success: boolean; data?: boolean; error?: string }>('fivepaisa_ws_connect', {
      clientId: this.clientId,
      accessToken: this.accessToken,
    });

    if (!result.success) throw new Error(result.error || 'WebSocket connection failed');

    this.tickerUnlisten = await listen<{
      provider: string; symbol: string; price: number; bid?: number; ask?: number;
      volume?: number; change_percent?: number; timestamp: number;
    }>('fivepaisa_ticker', (event) => {
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
    }>('fivepaisa_orderbook', (event) => {
      const data = event.payload;
      const [exchangePart, symbolPart] = data.symbol.includes(':') ? data.symbol.split(':') : ['NSE', data.symbol];
      const symbol = symbolPart || data.symbol;
      const exchange = (exchangePart as StockExchange) || 'NSE';

      console.log(`[5Paisa WebSocket] Depth update for ${symbol}`);

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
    }>('fivepaisa_status', (event) => {
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
    try { await invoke('fivepaisa_ws_disconnect'); } catch (err) { console.warn('[5Paisa] WS disconnect error:', err); }
    this.wsConnected = false;
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();
    console.log('[5Paisa] Starting REST API polling (fallback mode, 2 min interval)');
    this.quotePollingInterval = setInterval(async () => { await this.pollQuotesSequentially(); }, 120000);
    this.pollQuotesSequentially();
  }

  private stopQuotePolling(): void {
    if (this.quotePollingInterval) { clearInterval(this.quotePollingInterval); this.quotePollingInterval = null; }
  }

  private async pollQuotesSequentially(): Promise<void> {
    if (this.pollingSymbols.size === 0) return;

    for (const [key, { symbol, exchange, scripCode }] of this.pollingSymbols) {
      try {
        this.setScripCode(symbol, exchange, scripCode);
        const quote = await this.getQuoteInternal(symbol, exchange);
        this.emitTick({
          symbol, exchange, mode: 'full',
          lastPrice: quote.lastPrice, open: quote.open, high: quote.high, low: quote.low,
          close: quote.close, volume: quote.volume, change: quote.change,
          changePercent: quote.changePercent, timestamp: quote.timestamp,
          bid: quote.bid, bidQty: quote.bidQty, ask: quote.ask, askQty: quote.askQty,
        });
      } catch (error) { console.error(`[5Paisa] Failed to poll ${symbol}:`, error); }
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    console.log(`[5Paisa] Polling complete for ${this.pollingSymbols.size} symbols`);
  }

  async disconnectWebSocket(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    console.log('[5Paisa] WebSocket disconnected');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    try {
      const scripCode = this.getScripCode(symbol, exchange);
      if (!scripCode) {
        console.warn(`[5Paisa] ScripCode not found for ${symbol}. Set scripCode first.`);
        return;
      }

      const key = `${exchange}:${symbol}`;
      this.pollingSymbols.set(key, { symbol, exchange, scripCode });

      // If real WebSocket is connected, subscribe via Tauri command
      if (this.wsConnected && !this.usePollingFallback) {
        try {
          await invoke('fivepaisa_ws_subscribe', {
            symbol: `${exchange}:${symbol}`,
            mode: mode === 'full' ? 'depth' : mode === 'quote' ? 'quotes' : 'ltp',
          });
          console.log(`[5Paisa] WebSocket subscribed to ${key}`);
        } catch (err) {
          console.warn(`[5Paisa] WebSocket subscribe failed for ${key}, using polling:`, err);
          this.pollQuotesSequentially();
        }
      } else {
        console.log(`5Paisa: Added ${key} to polling list (${this.pollingSymbols.size} symbols total)`);
        this.pollQuotesSequentially();
      }
    } catch (error) {
      console.error('Failed to subscribe:', error);
      throw error;
    }
  }

  /**
   * Subscribe with scripCode (5Paisa specific)
   */
  async subscribeWithScripCode(
    symbol: string,
    exchange: StockExchange,
    scripCode: number,
    mode: SubscriptionMode = 'full'
  ): Promise<void> {
    this.setScripCode(symbol, exchange, scripCode);
    await this.subscribeInternal(symbol, exchange, mode);
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);

    // If real WebSocket is connected, unsubscribe via Tauri command
    if (this.wsConnected && !this.usePollingFallback) {
      try {
        await invoke('fivepaisa_ws_unsubscribe', { symbol: `${exchange}:${symbol}` });
        console.log(`[5Paisa] WebSocket unsubscribed from ${key}`);
      } catch (err) {
        console.warn(`[5Paisa] WebSocket unsubscribe failed for ${key}:`, err);
      }
    } else {
      console.log(`5Paisa: Removed ${key} from polling list (${this.pollingSymbols.size} symbols remaining)`);
    }
  }

  // ============================================================================
  // Symbol Search & Instruments
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      // 5Paisa doesn't have a dedicated search API
      // Would need to implement from master contract
      console.warn('[5Paisa] Symbol search not implemented');
      return [];
    } catch (error) {
      console.error('[5Paisa] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const scripCode = this.getScripCode(symbol, exchange);
      // Return basic instrument
      return {
        symbol,
        exchange,
        name: symbol,
        token: scripCode ? scripCode.toString() : '0',
        lotSize: 1,
        tickSize: 0.05,
        instrumentType: 'EQUITY' as InstrumentType,
        currency: 'INR',
      };
    } catch (error) {
      console.error('[5Paisa] getInstrument error:', error);
      return null;
    }
  }

  // ============================================================================
  // Master Contract
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    console.log('[5Paisa] Master contract download not implemented');
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    return null;
  }
}

// Export singleton instance
export const fivePaisaAdapter = new FivePaisaAdapter();
