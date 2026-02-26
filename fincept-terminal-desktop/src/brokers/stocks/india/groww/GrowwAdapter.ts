/**
 * Groww Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Groww Trading API
 * Based on OpenAlgo broker integration patterns
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
  GROWW_METADATA,
  GROWW_LOGIN_URL,
  GROWW_EXCHANGE_MAP,
  GROWW_SEGMENT_MAP,
} from './constants';
import {
  toGrowwOrderParams,
  toGrowwModifyParams,
  toGrowwInterval,
  fromGrowwOrder,
  fromGrowwTrade,
  fromGrowwPosition,
  fromGrowwHolding,
  fromGrowwFunds,
  fromGrowwQuote,
  fromGrowwOHLCV,
  fromGrowwDepth,
  fromGrowwTick,
} from './mapper';

/**
 * Groww broker adapter
 */
export class GrowwAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'groww';
  readonly brokerName = 'Groww';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = GROWW_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;

  // Tauri event unlisteners for WebSocket events
  private tickerUnlisten: UnlistenFn | null = null;
  private orderbookUnlisten: UnlistenFn | null = null;
  private statusUnlisten: UnlistenFn | null = null;

  // Fallback: REST API polling for quotes
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange }> = new Map();
  private usePollingFallback: boolean = false;

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Set API credentials before authentication
   */
  setCredentials(apiKey: string, apiSecret: string): void {
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
  }

  /**
   * Get login URL - Groww uses TOTP-based auth
   */
  getAuthUrl(): string {
    return GROWW_LOGIN_URL;
  }

  /**
   * Authenticate with Groww using TOTP
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;

      // If access token provided, validate it
      if (credentials.accessToken) {
        console.log('[Groww] Access token provided, validating...');
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
          console.error('[Groww] Access token validation failed');
          return {
            success: false,
            message: 'Invalid or expired access token',
            errorCode: 'AUTH_FAILED',
          };
        }
      }

      // No access token - get one via TOTP
      if (!this.apiKey || !this.apiSecret) {
        return {
          success: false,
          message: 'API key and secret are required',
          errorCode: 'CREDENTIALS_MISSING',
        };
      }

      // Get access token via TOTP
      const response = await invoke<{
        success: boolean;
        access_token?: string;
        user_id?: string;
        error?: string;
      }>('groww_get_access_token', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
      });

      if (!response.success || !response.access_token) {
        return {
          success: false,
          message: response.error || 'Authentication failed',
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
        message: error instanceof Error ? error.message : 'Authentication failed',
        errorCode: 'AUTH_FAILED',
      };
    }
  }

  /**
   * Handle OAuth callback - Groww uses TOTP, not OAuth
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    // Groww doesn't use OAuth callback flow
    // Authentication is done via TOTP in authenticate()
    return {
      success: false,
      message: 'Groww uses TOTP authentication, not OAuth',
      errorCode: 'NOT_SUPPORTED',
    };
  }

  /**
   * Validate access token (uses base class date check + API validation)
   */
  private async validateToken(_token: string): Promise<boolean> {
    return this.validateTokenWithDateCheck();
  }

  /**
   * Groww-specific API token validation
   */
  protected override async validateTokenWithApi(token: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean }>('groww_validate_token', {
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
      this.userId = credentials.userId || null;

      if (this.accessToken) {
        console.log('[Groww] Validating stored access token...');
        const isValid = await this.validateToken(this.accessToken);

        if (!isValid) {
          console.warn('[Groww] Access token expired or invalid, clearing from storage...');

          await this.storeCredentials({
            apiKey: this.apiKey,
            apiSecret: this.apiSecret || '',
          });

          this.accessToken = null;
          this._isConnected = false;

          console.log('[Groww] Please re-authenticate to continue');
          return false;
        }

        console.log('[Groww] Token is valid, session restored');
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
    const segment = GROWW_SEGMENT_MAP[params.exchange] || 'CASH';
    const exchange = GROWW_EXCHANGE_MAP[params.exchange] || params.exchange;

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('groww_place_order', {
      accessToken: this.accessToken,
      tradingSymbol: params.symbol,
      exchange,
      transactionType: params.side,
      orderType: params.orderType,
      quantity: params.quantity,
      price: params.price || null,
      triggerPrice: params.triggerPrice || null,
      product: params.productType,
      validity: params.validity || 'DAY',
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    // Determine segment from existing order or default to CASH
    const segment = 'CASH';

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('groww_modify_order', {
      accessToken: this.accessToken,
      orderId: params.orderId,
      segment,
      quantity: params.quantity,
      price: params.price,
      triggerPrice: params.triggerPrice,
      orderType: params.orderType,
      validity: params.validity,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    // Default segment - ideally we should get this from the order
    const segment = 'CASH';

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('groww_cancel_order', {
      accessToken: this.accessToken,
      orderId,
      segment,
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
    }>('groww_get_orders', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromGrowwOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    // Groww returns trades within order data
    // Fetch orders and extract filled trades
    const orders = await this.getOrdersInternal();
    const trades: Trade[] = [];

    for (const order of orders) {
      if (order.filledQuantity > 0) {
        trades.push({
          tradeId: `${order.orderId}-trade`,
          orderId: order.orderId,
          symbol: order.symbol,
          exchange: order.exchange,
          side: order.side,
          quantity: order.filledQuantity,
          price: order.averagePrice,
          productType: order.productType,
          tradedAt: order.updatedAt || order.placedAt,
        });
      }
    }

    return trades;
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    const orders = await this.getOrdersInternal();
    const pendingOrders = orders.filter(o =>
      o.status === 'PENDING' || o.status === 'OPEN' || o.status === 'TRIGGER_PENDING'
    );

    const results: Array<{ success: boolean; orderId?: string; error?: string }> = [];

    for (const order of pendingOrders) {
      const result = await this.cancelOrderInternal(order.orderId);
      results.push({
        success: result.success,
        orderId: order.orderId,
        error: result.message,
      });
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
    const positions = await this.getPositionsInternal();
    const openPositions = positions.filter(p => p.quantity !== 0);

    const results: Array<{ success: boolean; orderId?: string; error?: string }> = [];

    for (const position of openPositions) {
      const side = position.quantity > 0 ? 'SELL' : 'BUY';
      const quantity = Math.abs(position.quantity);

      const result = await this.placeOrderInternal({
        symbol: position.symbol,
        exchange: position.exchange,
        side: side as 'BUY' | 'SELL',
        quantity,
        orderType: 'MARKET',
        productType: position.productType,
      });

      results.push({
        success: result.success,
        orderId: result.orderId,
        error: result.message,
      });
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

    // Determine actual action based on position
    let actualSide = params.side; // eslint-disable-line prefer-const
    let actualQuantity = params.quantity;

    if (params.side === 'BUY') {
      if (positionSize < 0) {
        // Short position exists, close it first
        actualQuantity = Math.abs(positionSize) + params.quantity;
      }
    } else {
      if (positionSize > 0) {
        // Long position exists, close it first
        actualQuantity = positionSize + params.quantity;
      }
    }

    return this.placeOrderInternal({
      symbol: params.symbol,
      exchange: params.exchange,
      side: actualSide,
      quantity: actualQuantity,
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
    }>('groww_get_positions', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromGrowwPosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('groww_get_holdings', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    // Holdings might be in a nested array
    const holdingsArray = Array.isArray(response.data)
      ? response.data
      : (response.data.holdings as Record<string, unknown>[]) || [];

    return holdingsArray.map(fromGrowwHolding);
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('groww_get_margins', {
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

    return fromGrowwFunds(response.data);
  }

  // ============================================================================
  // Margin Calculator (Not supported by Groww)
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Groww doesn't provide a margin calculation API
    // Return a basic estimation
    return {
      totalMargin: 0,
      initialMargin: 0,
    };
  }

  // ============================================================================
  // Market Data
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    const growwExchange = GROWW_EXCHANGE_MAP[exchange] || exchange;

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('groww_get_quote', {
      accessToken: this.accessToken,
      tradingSymbol: symbol,
      exchange: growwExchange,
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || 'Failed to fetch quote');
    }

    return fromGrowwQuote(response.data, symbol, exchange);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    const growwExchange = GROWW_EXCHANGE_MAP[exchange] || exchange;
    const interval = toGrowwInterval(timeframe);

    const response = await invoke<{
      success: boolean;
      data?: Array<unknown[] | Record<string, unknown>>;
      error?: string;
    }>('groww_get_historical', {
      accessToken: this.accessToken,
      tradingSymbol: symbol,
      exchange: growwExchange,
      interval,
      startDate: from.toISOString().split('T')[0],
      endDate: to.toISOString().split('T')[0],
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || 'Failed to fetch historical data');
    }

    return response.data.map(fromGrowwOHLCV);
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('groww_get_depth', {
      accessToken: this.accessToken,
      tradingSymbol: symbol,
      exchange: GROWW_EXCHANGE_MAP[exchange] || exchange,
    });

    if (!response.success || !response.data) {
      return { bids: [], asks: [] };
    }

    return fromGrowwDepth(response.data);
  }

  // ============================================================================
  // WebSocket - Real WebSocket via Rust Backend (NATS protocol)
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureConnected();

    try {
      await this.connectRealWebSocket();
      console.log('[Groww] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[Groww] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  private async connectRealWebSocket(): Promise<void> {
    if (!this.accessToken) {
      throw new Error('Access token required');
    }

    const result = await invoke<{ success: boolean; data?: boolean; error?: string }>('groww_ws_connect', {
      accessToken: this.accessToken,
    });

    if (!result.success) {
      throw new Error(result.error || 'WebSocket connection failed');
    }

    this.tickerUnlisten = await listen<{
      provider: string; symbol: string; price: number; bid?: number; ask?: number;
      volume?: number; change_percent?: number; timestamp: number;
    }>('groww_ticker', (event) => {
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
    }>('groww_orderbook', (event) => {
      const data = event.payload;
      const [exchangePart, symbolPart] = data.symbol.includes(':') ? data.symbol.split(':') : ['NSE', data.symbol];
      const symbol = symbolPart || data.symbol;
      const exchange = (exchangePart as StockExchange) || 'NSE';

      console.log(`[Groww WebSocket] Depth update for ${symbol}`);

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
    }>('groww_status', (event) => {
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
    try { await invoke('groww_ws_disconnect'); } catch (err) { console.warn('[Groww] WS disconnect error:', err); }
    this.wsConnected = false;
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();
    console.log('[Groww] Starting REST API polling (fallback mode, 2 min interval)');
    this.quotePollingInterval = setInterval(async () => { await this.pollQuotesSequentially(); }, 120000);
    this.pollQuotesSequentially();
  }

  private stopQuotePolling(): void {
    if (this.quotePollingInterval) { clearInterval(this.quotePollingInterval); this.quotePollingInterval = null; }
  }

  private async pollQuotesSequentially(): Promise<void> {
    if (this.pollingSymbols.size === 0) return;

    for (const [key, { symbol, exchange }] of this.pollingSymbols) {
      try {
        const quote = await this.getQuoteInternal(symbol, exchange);
        this.emitTick({
          symbol, exchange, mode: 'full',
          lastPrice: quote.lastPrice, open: quote.open, high: quote.high, low: quote.low,
          close: quote.close, volume: quote.volume, change: quote.change,
          changePercent: quote.changePercent, timestamp: quote.timestamp,
          bid: quote.bid, bidQty: quote.bidQty, ask: quote.ask, askQty: quote.askQty,
        });
      } catch (error) { console.error(`[Groww] Failed to poll ${symbol}:`, error); }
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    console.log(`[Groww] Polling complete for ${this.pollingSymbols.size} symbols`);
  }

  protected async subscribeInternal(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.set(key, { symbol, exchange });
    if (this.wsConnected && !this.usePollingFallback) {
      try {
        await invoke('groww_ws_subscribe', { symbol, mode: mode === 'full' ? 'full' : 'ltp' });
        console.log(`[Groww] ✓ WebSocket subscribed to ${symbol}`);
      } catch (error) {
        console.error(`[Groww] WebSocket subscribe failed:`, error);
        if (!this.usePollingFallback) { this.usePollingFallback = true; this.startQuotePolling(); }
      }
    } else {
      console.log(`[Groww] Added ${key} to polling (${this.pollingSymbols.size} symbols total)`);
      if (this.usePollingFallback) this.pollQuotesSequentially();
    }
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);
    if (this.wsConnected && !this.usePollingFallback) {
      try { await invoke('groww_ws_unsubscribe', { symbol }); } catch (err) { console.warn('[Groww] Unsubscribe failed:', err); }
    }
    console.log(`[Groww] Removed ${key} from polling list`);
  }

  async disconnectWebSocket(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    this.wsConnected = false;
    console.log('[Groww] WebSocket and polling stopped');
  }

  async logout(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    await super.logout();
  }

  async isWebSocketConnected(): Promise<boolean> {
    return this.wsConnected;
  }

  // ============================================================================
  // Symbol Search & Instruments
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    // Groww doesn't have a dedicated search API
    // Return empty array - caller should use master contract download
    console.warn('[Groww] Symbol search not supported. Use master contract for symbol lookup.');
    return [];
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    // Return basic instrument info
    return {
      symbol,
      exchange,
      name: symbol,
      token: '0',
      lotSize: 1,
      tickSize: 0.05,
      instrumentType: 'EQ' as InstrumentType,
      currency: 'INR',
    };
  }

  // ============================================================================
  // Master Contract (Not supported by Groww API)
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    console.warn('[Groww] Master contract download not supported');
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    return null;
  }
}

// Export singleton instance
export const growwAdapter = new GrowwAdapter();
