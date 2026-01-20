/**
 * Dhan Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Dhan API
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
  ProductType,
} from '../../types';
import { DHAN_METADATA, DHAN_AUTH_BASE, DHAN_INTERVAL_MAP } from './constants';
import {
  toDhanExchange,
  toDhanOrderParams,
  toDhanModifyParams,
  fromDhanOrder,
  fromDhanTrade,
  fromDhanPosition,
  fromDhanHolding,
  fromDhanFunds,
  fromDhanQuote,
  fromDhanProduct,
} from './mapper';

/**
 * Dhan broker adapter
 */
export class DhanAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'dhan';
  readonly brokerName = 'Dhan';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = DHAN_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;
  private clientId: string | null = null;

  // Tauri event unlisteners for WebSocket events
  private tickerUnlisten: UnlistenFn | null = null;
  private orderbookUnlisten: UnlistenFn | null = null;
  private statusUnlisten: UnlistenFn | null = null;

  // Fallback: REST API polling for quotes (when WebSocket fails)
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange }> = new Map();
  private usePollingFallback: boolean = false;

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Set API credentials before authentication
   */
  setCredentials(apiKey: string, apiSecret: string, clientId?: string): void {
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
    this.clientId = clientId || null;
  }

  /**
   * Get OAuth login URL - Dhan uses consent-based auth
   * Note: For Dhan, you should call generateConsentUrl() instead for the full flow
   */
  getAuthUrl(): string {
    if (!this.apiKey) {
      throw new Error('API key not set. Call setCredentials first.');
    }
    // Return base auth URL - actual consent generation happens in authenticate
    return `${DHAN_AUTH_BASE}/authorize`;
  }

  /**
   * Generate consent URL (async) - Dhan's actual OAuth flow
   */
  async generateConsentUrl(): Promise<string> {
    if (!this.apiKey || !this.apiSecret || !this.clientId) {
      throw new Error('API key, secret, and client ID are required. Call setCredentials first.');
    }

    // Generate consent
    const response = await invoke<{
      success: boolean;
      data?: { consent_app_id: string; login_url: string };
      error?: string;
    }>('dhan_generate_consent', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      clientId: this.clientId,
    });

    if (!response.success || !response.data?.login_url) {
      throw new Error(response.error || 'Failed to generate consent');
    }

    return response.data.login_url;
  }

  /**
   * Authenticate with Dhan
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;
      this.clientId = credentials.userId || null;

      // If access token provided, validate it
      if (credentials.accessToken && this.clientId) {
        console.log('[Dhan] Access token provided, validating...');
        const isValid = await this.validateToken(credentials.accessToken, this.clientId);
        if (isValid) {
          this.accessToken = credentials.accessToken;
          this.userId = this.clientId;
          this._isConnected = true;

          await this.storeCredentials(credentials);

          return {
            success: true,
            accessToken: this.accessToken,
            userId: this.userId || undefined,
            message: 'Authentication successful',
          };
        } else {
          console.error('[Dhan] Access token validation failed');
          return {
            success: false,
            message: 'Invalid or expired access token',
            errorCode: 'AUTH_FAILED',
          };
        }
      }

      // No access token - need OAuth flow
      return {
        success: false,
        message: 'OAuth authentication required. Use getAuthUrl() and handleAuthCallback().',
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
   * Handle OAuth callback - exchange token ID for access token
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    try {
      if (!params.authCode) {
        return {
          success: false,
          message: 'Token ID is required',
          errorCode: 'INVALID_CALLBACK',
        };
      }

      if (!this.apiKey || !this.apiSecret) {
        return {
          success: false,
          message: 'API credentials not set. Call setCredentials first.',
          errorCode: 'CREDENTIALS_MISSING',
        };
      }

      // Exchange token ID for access token
      const response = await invoke<{
        success: boolean;
        data?: {
          access_token: string;
          client_id?: string;
          client_name?: string;
        };
        error?: string;
      }>('dhan_exchange_token', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        tokenId: params.authCode,
      });

      if (!response.success || !response.data?.access_token) {
        return {
          success: false,
          message: response.error || 'Token exchange failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.accessToken = response.data.access_token;
      this.clientId = response.data.client_id || this.clientId;
      this.userId = this.clientId;
      this._isConnected = true;

      // Store credentials
      await this.storeCredentials({
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        accessToken: this.accessToken,
        userId: this.clientId || undefined,
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
   * Validate access token
   */
  private async validateToken(token: string, clientId: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean }>('dhan_validate_token', {
        accessToken: token,
        clientId,
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
      this.clientId = credentials.userId || null;
      this.userId = credentials.userId || null;

      if (this.accessToken && this.clientId) {
        console.log('[Dhan] Validating stored access token...');
        const isValid = await this.validateToken(this.accessToken, this.clientId);

        if (!isValid) {
          console.warn('[Dhan] Access token expired, clearing from storage...');
          await this.storeCredentials({
            apiKey: this.apiKey,
            apiSecret: this.apiSecret || undefined,
            userId: this.clientId,
          });
          this.accessToken = null;
          this._isConnected = false;
          return false;
        }

        console.log('[Dhan] Token is valid, session restored');
        this._isConnected = true;
        return true;
      }

      return false;
    } catch {
      return false;
    }
  }

  /**
   * Refresh session - Dhan requires re-authentication
   */
  async refreshSession(): Promise<AuthResponse> {
    return {
      success: false,
      message: 'Dhan requires re-authentication. Use OAuth flow.',
      errorCode: 'REFRESH_NOT_SUPPORTED',
    };
  }

  // ============================================================================
  // Orders
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    const securityId = await this.getSecurityId(params.symbol, params.exchange);
    const dhanParams = toDhanOrderParams(this.clientId!, securityId, params);

    const response = await invoke<{
      success: boolean;
      data?: { order_id: string };
      error?: string;
    }>('dhan_place_order', {
      accessToken: this.accessToken,
      clientId: this.clientId,
      params: dhanParams,
    });

    return {
      success: response.success,
      orderId: response.data?.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const dhanParams = toDhanModifyParams(this.clientId!, params.orderId, params);

    const response = await invoke<{
      success: boolean;
      data?: { order_id: string };
      error?: string;
    }>('dhan_modify_order', {
      accessToken: this.accessToken,
      clientId: this.clientId,
      orderId: params.orderId,
      params: dhanParams,
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
    }>('dhan_cancel_order', {
      accessToken: this.accessToken,
      clientId: this.clientId,
      orderId,
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
    }>('dhan_get_orders', {
      accessToken: this.accessToken,
      clientId: this.clientId,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromDhanOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('dhan_get_trade_book', {
      accessToken: this.accessToken,
      clientId: this.clientId,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromDhanTrade);
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    const orders = await this.getOrders();
    const openOrders = orders.filter(o =>
      o.status === 'OPEN' || o.status === 'PENDING' || o.status === 'TRIGGER_PENDING'
    );

    if (openOrders.length === 0) {
      return {
        success: true,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }

    const results: Array<{ success: boolean; orderId?: string; error?: string }> = [];

    for (const order of openOrders) {
      try {
        const response = await this.cancelOrder(order.orderId);
        results.push({
          success: response.success,
          orderId: order.orderId,
          error: response.message,
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
    const positions = await this.getPositions();
    const openPositions = positions.filter(p => p.quantity !== 0);

    if (openPositions.length === 0) {
      return {
        success: true,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }

    const results: Array<{ success: boolean; orderId?: string; symbol?: string; error?: string }> = [];

    for (const position of openPositions) {
      try {
        const action = position.quantity > 0 ? 'SELL' : 'BUY';
        const quantity = Math.abs(position.quantity);

        const response = await this.placeOrder({
          symbol: position.symbol,
          exchange: position.exchange,
          side: action,
          quantity,
          orderType: 'MARKET',
          productType: position.productType,
        });

        results.push({
          success: response.success,
          orderId: response.orderId,
          symbol: position.symbol,
          error: response.message,
        });
      } catch (error) {
        results.push({
          success: false,
          symbol: position.symbol,
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
    let positionSize = params.positionSize ?? 0;
    if (params.positionSize === undefined) {
      const position = await this.getOpenPosition(params.symbol, params.exchange, params.productType);
      positionSize = position?.quantity ?? 0;
    }

    if (positionSize === 0 && params.quantity !== 0) {
      return this.placeOrder(params);
    }

    if (positionSize === params.positionSize) {
      return {
        success: true,
        message: params.quantity === 0
          ? 'No open position found. Not placing exit order.'
          : 'No action needed. Position size matches current position',
      };
    }

    let action: 'BUY' | 'SELL';
    let quantity: number;

    const targetPosition = params.positionSize ?? (params.side === 'BUY' ? params.quantity : -params.quantity);

    if (targetPosition === 0) {
      action = positionSize > 0 ? 'SELL' : 'BUY';
      quantity = Math.abs(positionSize);
    } else if (positionSize === 0) {
      action = targetPosition > 0 ? 'BUY' : 'SELL';
      quantity = Math.abs(targetPosition);
    } else if (targetPosition > positionSize) {
      action = 'BUY';
      quantity = targetPosition - positionSize;
    } else {
      action = 'SELL';
      quantity = positionSize - targetPosition;
    }

    return this.placeOrder({
      ...params,
      side: action,
      quantity,
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
    }>('dhan_get_positions', {
      accessToken: this.accessToken,
      clientId: this.clientId,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromDhanPosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('dhan_get_holdings', {
      accessToken: this.accessToken,
      clientId: this.clientId,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromDhanHolding);
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('dhan_get_funds', {
      accessToken: this.accessToken,
      clientId: this.clientId,
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

    return fromDhanFunds(response.data);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Dhan doesn't have a batch margin API, estimate based on individual orders
    let totalMargin = 0;

    for (const order of orders) {
      try {
        const quote = await this.getQuote(order.symbol, order.exchange);
        const price = order.price || quote.lastPrice;
        const value = price * order.quantity;

        if (order.productType === 'INTRADAY') {
          totalMargin += value * 0.20;
        } else {
          totalMargin += value;
        }
      } catch {
        // Skip if quote fails
      }
    }

    return {
      totalMargin,
      initialMargin: totalMargin,
    };
  }

  // ============================================================================
  // Market Data
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    const securityId = await this.getSecurityId(symbol, exchange);
    const dhanExchange = toDhanExchange(exchange);
    const instrumentKey = `${dhanExchange}|${securityId}`;

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('dhan_get_quotes', {
      accessToken: this.accessToken,
      clientId: this.clientId,
      instrumentKeys: [instrumentKey],
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || 'Failed to fetch quote');
    }

    // Parse nested response
    const quoteData = response.data?.data as Record<string, Record<string, Record<string, unknown>>> | undefined;
    const exchangeData = quoteData?.[dhanExchange];
    const symbolData = exchangeData?.[securityId] as Record<string, unknown> | undefined;

    if (!symbolData) {
      throw new Error('Quote not found');
    }

    return fromDhanQuote(symbolData, symbol, exchange);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    const securityId = await this.getSecurityId(symbol, exchange);
    const dhanExchange = toDhanExchange(exchange);
    const interval = DHAN_INTERVAL_MAP[timeframe];

    if (!interval) {
      throw new Error(`Unsupported timeframe: ${timeframe}`);
    }

    // Determine instrument type
    let instrument = 'EQUITY';
    if (exchange === 'NFO' || exchange === 'BFO') {
      instrument = 'FUTIDX'; // Simplified - should be determined from symbol
    } else if (exchange === 'MCX') {
      instrument = 'FUTCOM';
    } else if (exchange === 'NSE_INDEX' || exchange === 'BSE_INDEX') {
      instrument = 'INDEX';
    }

    const response = await invoke<{
      success: boolean;
      data?: {
        timestamp?: number[];
        open?: number[];
        high?: number[];
        low?: number[];
        close?: number[];
        volume?: number[];
      };
      error?: string;
    }>('dhan_get_history', {
      accessToken: this.accessToken,
      clientId: this.clientId,
      securityId,
      exchangeSegment: dhanExchange,
      instrument,
      interval,
      fromDate: from.toISOString().split('T')[0],
      toDate: to.toISOString().split('T')[0],
    });

    if (!response.success || !response.data) {
      return [];
    }

    const { timestamp = [], open = [], high = [], low = [], close = [], volume = [] } = response.data;

    return timestamp.map((ts, i) => ({
      timestamp: ts * 1000,
      open: open[i] || 0,
      high: high[i] || 0,
      low: low[i] || 0,
      close: close[i] || 0,
      volume: volume[i] || 0,
    }));
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    try {
      const securityId = await this.getSecurityId(symbol, exchange);
      const dhanExchange = toDhanExchange(exchange);

      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('dhan_get_depth', {
        accessToken: this.accessToken,
        clientId: this.clientId,
        securityId,
        exchangeSegment: dhanExchange,
      });

      if (!response.success || !response.data) {
        return { bids: [], asks: [] };
      }

      const depth = response.data.depth as Record<string, unknown> | undefined;
      if (!depth) {
        return { bids: [], asks: [] };
      }

      const buyLevels = (depth.buy as Array<{ price: number; quantity: number }>) || [];
      const sellLevels = (depth.sell as Array<{ price: number; quantity: number }>) || [];

      return {
        bids: buyLevels.map(l => ({ price: l.price, quantity: l.quantity })),
        asks: sellLevels.map(l => ({ price: l.price, quantity: l.quantity })),
      };
    } catch {
      return { bids: [], asks: [] };
    }
  }

  // ============================================================================
  // WebSocket - Real WebSocket via Rust Backend
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureConnected();

    try {
      // Try real WebSocket connection via Rust backend
      await this.connectRealWebSocket();
      console.log('[Dhan] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[Dhan] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  /**
   * Connect to real Dhan WebSocket via Tauri/Rust backend
   */
  private async connectRealWebSocket(): Promise<void> {
    if (!this.accessToken || !this.clientId) {
      throw new Error('Access token and client ID not available');
    }

    // Call Rust backend to connect WebSocket
    const result = await invoke<{
      success: boolean;
      data?: boolean;
      error?: string;
    }>('dhan_ws_connect', {
      clientId: this.clientId,
      accessToken: this.accessToken,
      is20Depth: false, // Use 5-level depth by default
    });

    if (!result.success) {
      throw new Error(result.error || 'WebSocket connection failed');
    }

    // Listen for ticker events from Rust backend
    this.tickerUnlisten = await listen<{
      provider: string;
      symbol: string;
      price: number;
      bid?: number;
      ask?: number;
      volume?: number;
      change_percent?: number;
      timestamp: number;
    }>('dhan_ticker', (event) => {
      const tick = event.payload;

      // Parse symbol (format depends on Dhan - usually security_id)
      this.emitTick({
        symbol: tick.symbol,
        exchange: 'NSE' as StockExchange, // Will be parsed from actual data
        mode: 'quote',
        lastPrice: tick.price,
        bid: tick.bid,
        ask: tick.ask,
        volume: tick.volume || 0,
        changePercent: tick.change_percent || 0,
        timestamp: tick.timestamp,
      });
    });

    // Listen for orderbook/depth events
    this.orderbookUnlisten = await listen<{
      provider: string;
      symbol: string;
      bids: Array<{ price: number; quantity: number; orders?: number }>;
      asks: Array<{ price: number; quantity: number; orders?: number }>;
      timestamp: number;
    }>('dhan_orderbook', (event) => {
      const data = event.payload;

      // Parse symbol from Dhan format
      const [exchangePart, symbolPart] = data.symbol.includes(':')
        ? data.symbol.split(':')
        : ['NSE', data.symbol];
      const symbol = symbolPart || data.symbol;
      const exchange = (exchangePart as StockExchange) || 'NSE';

      console.log(`[Dhan WebSocket] Depth update for ${symbol}:`, {
        bids: data.bids?.length || 0,
        asks: data.asks?.length || 0,
      });

      const bestBid = data.bids?.[0];
      const bestAsk = data.asks?.[0];

      const tick: TickData = {
        symbol,
        exchange,
        mode: 'full',
        lastPrice: bestBid?.price || 0,
        bid: bestBid?.price,
        ask: bestAsk?.price,
        bidQty: bestBid?.quantity,
        askQty: bestAsk?.quantity,
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

    // Listen for status/connection events
    this.statusUnlisten = await listen<{
      provider: string;
      status: string;
      message?: string;
      timestamp: number;
    }>('dhan_status', (event) => {
      const status = event.payload;
      console.log(`[Dhan WebSocket] Status: ${status.status} - ${status.message || ''}`);

      if (status.status === 'disconnected' || status.status === 'error') {
        this.wsConnected = false;
        if (!this.usePollingFallback) {
          console.warn('[Dhan] WebSocket disconnected, enabling polling fallback');
          this.usePollingFallback = true;
          this.startQuotePolling();
        }
      } else if (status.status === 'connected') {
        this.wsConnected = true;
        if (this.usePollingFallback) {
          console.log('[Dhan] WebSocket reconnected, disabling polling fallback');
          this.usePollingFallback = false;
          this.stopQuotePolling();
        }
      }
    });

    this.wsConnected = true;
  }

  private async disconnectRealWebSocket(): Promise<void> {
    if (this.tickerUnlisten) {
      this.tickerUnlisten();
      this.tickerUnlisten = null;
    }
    if (this.orderbookUnlisten) {
      this.orderbookUnlisten();
      this.orderbookUnlisten = null;
    }
    if (this.statusUnlisten) {
      this.statusUnlisten();
      this.statusUnlisten = null;
    }

    try {
      await invoke('dhan_ws_disconnect');
    } catch (err) {
      console.warn('[Dhan] WebSocket disconnect error:', err);
    }

    this.wsConnected = false;
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();

    console.log('[Dhan] Starting REST API polling (fallback mode, 2 min interval)');

    // Poll every 2 minutes (Dhan has strict rate limits)
    this.quotePollingInterval = setInterval(async () => {
      await this.pollQuotes();
    }, 120000);

    // Initial poll
    this.pollQuotes();
  }

  private stopQuotePolling(): void {
    if (this.quotePollingInterval) {
      clearInterval(this.quotePollingInterval);
      this.quotePollingInterval = null;
    }
  }

  private async pollQuotes(): Promise<void> {
    if (this.pollingSymbols.size === 0) return;

    console.log(`[Dhan] Polling ${this.pollingSymbols.size} symbols...`);

    for (const [key, { symbol, exchange }] of this.pollingSymbols) {
      try {
        const quote = await this.getQuoteInternal(symbol, exchange);

        const tick: TickData = {
          symbol,
          exchange,
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
          mode: 'full',
        };

        this.emitTick(tick);
      } catch (error) {
        console.error(`[Dhan] Poll error for ${symbol}:`, error);
      }

      // Rate limit delay (Dhan requires 1 req/sec)
      await new Promise(resolve => setTimeout(resolve, 1100));
    }

    console.log(`[Dhan] Polling complete`);
  }

  async disconnectWebSocket(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    this.wsConnected = false;
    console.log('[Dhan] WebSocket disconnected');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.set(key, { symbol, exchange });

    if (this.wsConnected && !this.usePollingFallback) {
      try {
        const securityId = await this.getSecurityId(symbol, exchange);
        const wsMode = mode === 'full' ? 'full' : mode === 'quote' ? 'quote' : 'ticker';

        await invoke('dhan_ws_subscribe', {
          symbol: securityId,
          mode: wsMode,
        });

        console.log(`[Dhan] ✓ WebSocket subscribed to ${symbol} (mode: ${wsMode})`);
      } catch (error) {
        console.error(`[Dhan] WebSocket subscribe failed:`, error);
        if (!this.usePollingFallback) {
          this.usePollingFallback = true;
          this.startQuotePolling();
        }
      }
    } else {
      console.log(`[Dhan] Added ${key} to polling (${this.pollingSymbols.size} total)`);
      if (this.usePollingFallback) {
        this.pollQuotes();
      }
    }
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);

    if (this.wsConnected && !this.usePollingFallback) {
      try {
        const securityId = await this.getSecurityId(symbol, exchange);
        await invoke('dhan_ws_unsubscribe', { symbol: securityId });
        console.log(`[Dhan] ✓ WebSocket unsubscribed from ${symbol}`);
      } catch (error) {
        console.warn(`[Dhan] WebSocket unsubscribe failed:`, error);
      }
    } else {
      console.log(`[Dhan] Removed ${key} from polling`);
    }
  }

  async logout(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    await super.logout();
  }

  // ============================================================================
  // Symbol Search & Instrument
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: Array<Record<string, unknown>>;
        error?: string;
      }>('dhan_search_symbol', {
        keyword: query,
        exchange: exchange || null,
        limit: 20,
      });

      if (!response.success || !response.data) {
        return [];
      }

      return response.data.map(item => ({
        symbol: String(item.symbol || ''),
        exchange: String(item.exchange || 'NSE') as StockExchange,
        name: String(item.name || item.symbol || ''),
        token: String(item.security_id || ''),
        lotSize: Number(item.lot_size || 1),
        tickSize: Number(item.tick_size || 0.05),
        instrumentType: mapInstrumentType(String(item.instrument_type || 'EQ')),
        currency: 'INR',
        expiry: item.expiry ? String(item.expiry) : undefined,
        strike: item.strike ? Number(item.strike) : undefined,
      }));
    } catch (error) {
      console.error('[Dhan] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const results = await this.searchSymbols(symbol, exchange);
      const instrument = results.find(i => i.symbol === symbol && i.exchange === exchange);

      if (instrument) {
        return instrument;
      }

      // Return minimal instrument
      const securityId = await this.getSecurityId(symbol, exchange);
      return {
        symbol,
        exchange,
        name: symbol,
        token: securityId,
        lotSize: 1,
        tickSize: 0.05,
        instrumentType: 'EQUITY',
        currency: 'INR',
      };
    } catch (error) {
      console.error('[Dhan] getInstrument error:', error);
      return {
        symbol,
        exchange,
        name: symbol,
        token: '',
        lotSize: 1,
        tickSize: 0.05,
        instrumentType: 'EQUITY',
        currency: 'INR',
      };
    }
  }

  /**
   * Get Dhan security ID for a symbol
   */
  private async getSecurityId(symbol: string, exchange: StockExchange): Promise<string> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: string;
        error?: string;
      }>('dhan_get_security_id', {
        symbol,
        exchange,
      });

      if (response.success && response.data) {
        return response.data;
      }

      throw new Error(response.error || 'Security ID not found');
    } catch (error) {
      throw new Error(`Failed to get security ID for ${symbol}: ${error}`);
    }
  }

  // ============================================================================
  // Master Contract
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    await invoke('dhan_download_master_contract');
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: { last_updated: number };
        error?: string;
      }>('dhan_get_master_contract_metadata');

      if (response.success && response.data?.last_updated) {
        return new Date(response.data.last_updated * 1000);
      }
      return null;
    } catch {
      return null;
    }
  }
}

// ============================================================================
// Helper Functions
// ============================================================================

function mapInstrumentType(type: string): 'EQUITY' | 'ETF' | 'INDEX' | 'FUTURE' | 'OPTION' | 'BOND' | 'MUTUAL_FUND' {
  const typeMap: Record<string, 'EQUITY' | 'ETF' | 'INDEX' | 'FUTURE' | 'OPTION'> = {
    EQ: 'EQUITY',
    EQUITY: 'EQUITY',
    ETF: 'ETF',
    INDEX: 'INDEX',
    FUT: 'FUTURE',
    FUTIDX: 'FUTURE',
    FUTSTK: 'FUTURE',
    FUTCOM: 'FUTURE',
    FUTCUR: 'FUTURE',
    CE: 'OPTION',
    PE: 'OPTION',
    OPTIDX: 'OPTION',
    OPTSTK: 'OPTION',
    OPTFUT: 'OPTION',
    OPTCUR: 'OPTION',
  };
  return typeMap[type.toUpperCase()] || 'EQUITY';
}

// Export singleton instance
export const dhanAdapter = new DhanAdapter();
