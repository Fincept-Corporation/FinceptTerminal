/**
 * Alice Blue Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Alice Blue Trading API
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
  ALICEBLUE_METADATA,
  ALICEBLUE_EXCHANGE_MAP,
} from './constants';
import {
  toAliceBlueOrderParams,
  toAliceBlueModifyParams,
  toAliceBlueInterval,
  fromAliceBlueOrder,
  fromAliceBlueTrade,
  fromAliceBluePosition,
  fromAliceBlueHolding,
  fromAliceBlueFunds,
  fromAliceBlueQuote,
  fromAliceBlueOHLCV,
  fromAliceBlueDepth,
} from './mapper';

/**
 * Alice Blue broker adapter
 */
export class AliceBlueAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'aliceblue';
  readonly brokerName = 'Alice Blue';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = ALICEBLUE_METADATA;

  private apiSecret: string | null = null;
  private encKey: string | null = null;
  private sessionId: string | null = null;

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
   * Get login URL - Alice Blue uses direct session auth, no OAuth redirect
   */
  getAuthUrl(): string {
    return 'https://ant.aliceblueonline.com';
  }

  /**
   * Set API credentials before authentication
   */
  setCredentials(userId: string, apiSecret: string, encKey?: string): void {
    this.userId = userId;
    this.apiSecret = apiSecret;
    this.encKey = encKey || null;
  }

  /**
   * Authenticate with stored or new credentials
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.userId = credentials.userId || null;
      this.apiSecret = credentials.apiSecret || null;
      this.encKey = (credentials as any).encKey || null;

      // If session ID already provided, validate it
      if (credentials.accessToken) {
        console.log('[AliceBlueAdapter] Session ID provided, validating...');
        const isValid = await this.validateSession(credentials.accessToken);
        if (isValid) {
          this.sessionId = credentials.accessToken;
          this._isConnected = true;

          await this.storeCredentials(credentials);

          return {
            success: true,
            accessToken: this.sessionId,
            userId: this.userId || undefined,
            message: 'Authentication successful',
          };
        } else {
          console.error('[AliceBlueAdapter] Session validation failed');
          return {
            success: false,
            message: 'Invalid or expired session',
            errorCode: 'AUTH_FAILED',
          };
        }
      }

      // Need to get new session via encryption key auth
      if (!this.userId || !this.apiSecret || !this.encKey) {
        return {
          success: false,
          message: 'User ID, API Secret, and Encryption Key are required',
          errorCode: 'AUTH_REQUIRED',
        };
      }

      // Get session using SHA256 checksum auth
      const response = await invoke<{
        success: boolean;
        session_id?: string;
        error?: string;
      }>('aliceblue_get_session', {
        userId: this.userId,
        apiSecret: this.apiSecret,
        encKey: this.encKey,
      });

      if (!response.success || !response.session_id) {
        return {
          success: false,
          message: response.error || 'Session creation failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.sessionId = response.session_id;
      this._isConnected = true;

      // Store credentials
      await this.storeCredentials({
        apiKey: this.userId || '',
        userId: this.userId || undefined,
        apiSecret: this.apiSecret || undefined,
        accessToken: this.sessionId,
      });

      return {
        success: true,
        accessToken: this.sessionId,
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
   * Handle OAuth callback - Alice Blue doesn't use OAuth, but method required by interface
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    // Alice Blue uses direct session auth, not OAuth
    return {
      success: false,
      message: 'Alice Blue uses direct session authentication, not OAuth',
      errorCode: 'AUTH_FAILED',
    };
  }

  /**
   * Validate session
   */
  private async validateSession(sessionId: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean }>('aliceblue_validate_session', {
        userId: this.userId,
        apiSecret: this.apiSecret,
        sessionId: sessionId,
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

      this.userId = credentials.userId || null;
      this.apiSecret = credentials.apiSecret || null;
      this.sessionId = credentials.accessToken || null;

      if (this.sessionId) {
        console.log('[AliceBlue] Validating stored session...');
        const isValid = await this.validateSession(this.sessionId);

        if (!isValid) {
          console.warn('[AliceBlue] Session expired or invalid, clearing from storage...');

          await this.storeCredentials({
            apiKey: this.userId || '',
            userId: this.userId || undefined,
            apiSecret: this.apiSecret || undefined,
          });

          this.sessionId = null;
          this._isConnected = false;

          console.log('[AliceBlue] Please re-authenticate to continue');
          return false;
        }

        console.log('[AliceBlue] Session is valid, session restored');
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
    const aliceBlueParams = toAliceBlueOrderParams(params);

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('aliceblue_place_order', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
      params: aliceBlueParams,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const aliceBlueParams = toAliceBlueModifyParams(params);

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('aliceblue_modify_order', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
      orderId: params.orderId,
      params: aliceBlueParams,
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
    }>('aliceblue_cancel_order', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
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
    }>('aliceblue_get_orders', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAliceBlueOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('aliceblue_get_trades', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAliceBlueTrade);
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
    }>('aliceblue_get_positions', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAliceBluePosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('aliceblue_get_holdings', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAliceBlueHolding);
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('aliceblue_get_margins', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
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

    return fromAliceBlueFunds(response.data);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Alice Blue doesn't have a dedicated margin calculator API
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
    const aliceBlueExchange = ALICEBLUE_EXCHANGE_MAP[exchange] || exchange;

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('aliceblue_get_quote', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
      exchange: aliceBlueExchange,
      symbol,
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || 'Failed to fetch quote');
    }

    return fromAliceBlueQuote(response.data, symbol, exchange);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    const interval = toAliceBlueInterval(timeframe);
    const aliceBlueExchange = ALICEBLUE_EXCHANGE_MAP[exchange] || exchange;

    const response = await invoke<{
      success: boolean;
      data?: unknown[][];
      error?: string;
    }>('aliceblue_get_historical', {
      userId: this.userId,
      sessionId: this.sessionId,
      exchange: aliceBlueExchange,
      symbol,
      resolution: interval,
      from: Math.floor(from.getTime() / 1000),
      to: Math.floor(to.getTime() / 1000),
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAliceBlueOHLCV);
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    const aliceBlueExchange = ALICEBLUE_EXCHANGE_MAP[exchange] || exchange;

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('aliceblue_get_quote', {
      userId: this.userId,
      apiSecret: this.apiSecret,
      sessionId: this.sessionId,
      exchange: aliceBlueExchange,
      symbol,
    });

    if (!response.success || !response.data) {
      return { bids: [], asks: [] };
    }

    return fromAliceBlueDepth(response.data);
  }

  // ============================================================================
  // WebSocket - Real WebSocket via Rust Backend
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureConnected();

    try {
      await this.connectRealWebSocket();
      console.log('[AliceBlue] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[AliceBlue] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  private async connectRealWebSocket(): Promise<void> {
    if (!this.accessToken || !this.userId) {
      throw new Error('Access token and user ID required');
    }

    const result = await invoke<{ success: boolean; data?: boolean; error?: string }>('aliceblue_ws_connect', {
      userId: this.userId,
      accessToken: this.accessToken,
    });

    if (!result.success) throw new Error(result.error || 'WebSocket connection failed');

    this.tickerUnlisten = await listen<{
      provider: string; symbol: string; price: number; bid?: number; ask?: number;
      volume?: number; change_percent?: number; timestamp: number;
    }>('aliceblue_ticker', (event) => {
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
    }>('aliceblue_orderbook', (event) => {
      const data = event.payload;
      const [exchangePart, symbolPart] = data.symbol.includes(':') ? data.symbol.split(':') : ['NSE', data.symbol];
      const symbol = symbolPart || data.symbol;
      const exchange = (exchangePart as StockExchange) || 'NSE';

      console.log(`[AliceBlue WebSocket] Depth update for ${symbol}`);

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
    }>('aliceblue_status', (event) => {
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
    try { await invoke('aliceblue_ws_disconnect'); } catch (err) { console.warn('[AliceBlue] WS disconnect error:', err); }
    this.wsConnected = false;
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();
    console.log('[AliceBlue] Starting REST API polling (fallback mode, 2 min interval)');
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
      } catch (error) { console.error(`[AliceBlue] Failed to poll ${symbol}:`, error); }
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    console.log(`[AliceBlue] Polling complete for ${this.pollingSymbols.size} symbols`);
  }

  /**
   * Disconnect from WebSocket (stop polling)
   */
  async disconnectWebSocket(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    console.log('[AliceBlue] WebSocket disconnected');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    try {
      const key = `${exchange}:${symbol}`;
      this.pollingSymbols.set(key, { symbol, exchange });

      // If real WebSocket is connected, subscribe via Tauri command
      if (this.wsConnected && !this.usePollingFallback) {
        try {
          await invoke('aliceblue_ws_subscribe', {
            symbol: `${exchange}:${symbol}`,
            mode: mode === 'full' ? 'depth' : mode === 'quote' ? 'quotes' : 'ltp',
          });
          console.log(`[AliceBlue] WebSocket subscribed to ${key}`);
        } catch (err) {
          console.warn(`[AliceBlue] WebSocket subscribe failed for ${key}, using polling:`, err);
        }
      } else {
        console.log(`AliceBlue: Added ${key} to polling list (${this.pollingSymbols.size} symbols total)`);
        // Trigger immediate poll for this symbol
        this.pollQuotesSequentially();
      }
    } catch (error) {
      console.error('Failed to subscribe:', error);
      throw error;
    }
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);

    // If real WebSocket is connected, unsubscribe via Tauri command
    if (this.wsConnected && !this.usePollingFallback) {
      try {
        await invoke('aliceblue_ws_unsubscribe', { symbol: `${exchange}:${symbol}` });
        console.log(`[AliceBlue] WebSocket unsubscribed from ${key}`);
      } catch (err) {
        console.warn(`[AliceBlue] WebSocket unsubscribe failed for ${key}:`, err);
      }
    } else {
      console.log(`AliceBlue: Removed ${key} from polling list (${this.pollingSymbols.size} symbols remaining)`);
    }
  }

  // ============================================================================
  // Symbol Search & Instruments
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      // Alice Blue doesn't have a dedicated search API
      // Would need to implement from master contract
      console.warn('[AliceBlue] Symbol search not implemented');
      return [];
    } catch (error) {
      console.error('[AliceBlue] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      // Return basic instrument
      return {
        symbol,
        exchange,
        name: symbol,
        token: '0',
        lotSize: 1,
        tickSize: 0.05,
        instrumentType: 'EQUITY' as InstrumentType,
        currency: 'INR',
      };
    } catch (error) {
      console.error('[AliceBlue] getInstrument error:', error);
      return null;
    }
  }

  // ============================================================================
  // Master Contract
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    console.log('[AliceBlue] Master contract download not implemented');
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    return null;
  }
}

// Export singleton instance
export const aliceBlueAdapter = new AliceBlueAdapter();
