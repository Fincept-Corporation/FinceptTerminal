/**
 * Upstox Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Upstox API v2/v3
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
} from '../../types';
import {
  UPSTOX_METADATA,
  UPSTOX_LOGIN_URL,
  UPSTOX_INTERVAL_MAP,
} from './constants';
import {
  toUpstoxExchange,
  toUpstoxOrderParams,
  toUpstoxModifyParams,
  fromUpstoxOrder,
  fromUpstoxTrade,
  fromUpstoxPosition,
  fromUpstoxHolding,
  fromUpstoxFunds,
  fromUpstoxQuote,
} from './mapper';

/**
 * Upstox broker adapter
 */
export class UpstoxAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'upstox';
  readonly brokerName = 'Upstox';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = UPSTOX_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;
  private redirectUri: string = 'http://localhost:5000/callback';

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
  setCredentials(apiKey: string, apiSecret: string): void {
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
  }

  /**
   * Get OAuth login URL
   */
  getAuthUrl(): string {
    if (!this.apiKey) {
      throw new Error('API key not set. Call setCredentials first.');
    }
    const params = new URLSearchParams({
      client_id: this.apiKey,
      redirect_uri: this.redirectUri,
      response_type: 'code',
    });
    return `${UPSTOX_LOGIN_URL}?${params.toString()}`;
  }

  /**
   * Authenticate with Upstox
   * Supports both direct access token and OAuth code exchange
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;

      // If access token provided, validate it
      if (credentials.accessToken) {
        console.log('[Upstox] Access token provided, validating...');
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
          console.error('[Upstox] Access token validation failed');
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
   * Handle OAuth callback - exchange auth code for access token
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    try {
      if (!params.authCode) {
        return {
          success: false,
          message: 'Authorization code is required',
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

      // Exchange auth code for access token via Rust backend
      const response = await invoke<{
        success: boolean;
        data?: {
          access_token: string;
          user_id?: string;
        };
        error?: string;
      }>('upstox_exchange_token', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        redirectUri: this.redirectUri,
        authCode: params.authCode,
      });

      if (!response.success || !response.data?.access_token) {
        return {
          success: false,
          message: response.error || 'Token exchange failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.accessToken = response.data.access_token;
      this.userId = response.data.user_id || null;
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
   * Validate access token (uses base class date check + API validation)
   */
  private async validateToken(_token: string): Promise<boolean> {
    return this.validateTokenWithDateCheck();
  }

  /**
   * Upstox-specific API token validation
   */
  protected override async validateTokenWithApi(token: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean }>('upstox_validate_token', {
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
        console.log('[Upstox] Validating stored access token...');
        const isValid = await this.validateToken(this.accessToken);

        if (!isValid) {
          console.warn('[Upstox] Access token expired, clearing from storage...');

          // Clear expired token from database but keep API credentials
          await this.storeCredentials({
            apiKey: this.apiKey,
            apiSecret: this.apiSecret || undefined,
          });
          this.accessToken = null;
          this._isConnected = false;
          return false;
        }

        console.log('[Upstox] Token is valid, session restored');
        this._isConnected = true;
        return true;
      }

      return false;
    } catch {
      return false;
    }
  }

  /**
   * Refresh session - Upstox requires re-authentication
   */
  async refreshSession(): Promise<AuthResponse> {
    return {
      success: false,
      message: 'Upstox requires re-authentication. Use OAuth flow.',
      errorCode: 'REFRESH_NOT_SUPPORTED',
    };
  }

  // ============================================================================
  // Orders
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    // Get instrument key first
    const instrumentKey = await this.getInstrumentKey(params.symbol, params.exchange);

    const upstoxParams = toUpstoxOrderParams(params);
    upstoxParams.instrument_token = instrumentKey;

    const response = await invoke<{
      success: boolean;
      data?: { order_id: string };
      error?: string;
    }>('upstox_place_order', {
      accessToken: this.accessToken,
      params: upstoxParams,
    });

    return {
      success: response.success,
      orderId: response.data?.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const upstoxParams = toUpstoxModifyParams(params.orderId, params);

    const response = await invoke<{
      success: boolean;
      data?: { order_id: string };
      error?: string;
    }>('upstox_modify_order', {
      accessToken: this.accessToken,
      orderId: params.orderId,
      params: upstoxParams,
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
    }>('upstox_cancel_order', {
      accessToken: this.accessToken,
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
    }>('upstox_get_orders', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromUpstoxOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('upstox_get_trade_book', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromUpstoxTrade);
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
    }>('upstox_get_positions', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromUpstoxPosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('upstox_get_holdings', {
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromUpstoxHolding);
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('upstox_get_funds', {
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

    return fromUpstoxFunds(response.data);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Upstox doesn't have a batch margin API, estimate based on individual orders
    let totalMargin = 0;

    for (const order of orders) {
      try {
        const quote = await this.getQuote(order.symbol, order.exchange);
        const price = order.price || quote.lastPrice;
        const value = price * order.quantity;

        // Rough margin estimation
        if (order.productType === 'INTRADAY') {
          totalMargin += value * 0.20; // ~20% for intraday
        } else {
          totalMargin += value; // Full value for delivery
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
    const instrumentKey = await this.getInstrumentKey(symbol, exchange);

    const response = await invoke<{
      success: boolean;
      data?: Record<string, Record<string, unknown>>;
      error?: string;
    }>('upstox_get_quotes', {
      accessToken: this.accessToken,
      instrumentKeys: [instrumentKey],
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || 'Failed to fetch quote');
    }

    const quoteData = response.data[instrumentKey];
    if (!quoteData) {
      throw new Error('Quote not found');
    }

    return fromUpstoxQuote(quoteData, symbol, exchange);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    const instrumentKey = await this.getInstrumentKey(symbol, exchange);
    const intervalConfig = UPSTOX_INTERVAL_MAP[timeframe];

    if (!intervalConfig) {
      throw new Error(`Unsupported timeframe: ${timeframe}`);
    }

    const response = await invoke<{
      success: boolean;
      data?: {
        candles?: Array<[string, number, number, number, number, number, number]>;
      };
      error?: string;
    }>('upstox_get_history', {
      accessToken: this.accessToken,
      instrumentKey,
      interval: intervalConfig.interval,
      toDate: to.toISOString().split('T')[0],
      fromDate: from.toISOString().split('T')[0],
    });

    if (!response.success || !response.data?.candles) {
      return [];
    }

    return response.data.candles.map(candle => ({
      timestamp: new Date(candle[0]).getTime(),
      open: candle[1],
      high: candle[2],
      low: candle[3],
      close: candle[4],
      volume: candle[5],
    }));
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    try {
      const instrumentKey = await this.getInstrumentKey(symbol, exchange);

      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('upstox_get_depth', {
        accessToken: this.accessToken,
        instrumentKey,
      });

      if (!response.success || !response.data) {
        return { bids: [], asks: [] };
      }

      const depth = response.data.depth as Record<string, unknown> | undefined;
      if (!depth) {
        return { bids: [], asks: [] };
      }

      const buyLevels = depth.buy as Array<{ price: number; quantity: number }> || [];
      const sellLevels = depth.sell as Array<{ price: number; quantity: number }> || [];

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
      console.log('[Upstox] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[Upstox] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  /**
   * Connect to real Upstox WebSocket via Tauri/Rust backend
   */
  private async connectRealWebSocket(): Promise<void> {
    if (!this.accessToken) {
      throw new Error('Access token not available');
    }

    // Call Rust backend to connect WebSocket
    const result = await invoke<{
      success: boolean;
      data?: boolean;
      error?: string;
    }>('upstox_ws_connect', {
      accessToken: this.accessToken,
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
    }>('upstox_ticker', (event) => {
      const tick = event.payload;

      // Parse exchange from instrument key (format: "NSE_EQ|RELIANCE")
      const parts = tick.symbol.split('|');
      const exchangePart = parts[0] || 'NSE_EQ';
      const exchange = exchangePart.startsWith('BSE') ? 'BSE' : 'NSE';
      const symbolName = parts[1] || tick.symbol;

      this.emitTick({
        symbol: symbolName,
        exchange: exchange as StockExchange,
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
    }>('upstox_orderbook', (event) => {
      const data = event.payload;

      // Parse symbol from Upstox format (e.g., "NSE_EQ|RELIANCE" -> "RELIANCE")
      const [exchangePart, symbolPart] = data.symbol.includes('|')
        ? data.symbol.split('|')
        : [data.symbol.split(':')[0], data.symbol.split(':')[1] || data.symbol];
      const symbol = symbolPart?.replace('-EQ', '') || data.symbol;
      const exchange = exchangePart?.replace('_EQ', '').replace('_FO', '') as StockExchange || 'NSE';

      console.log(`[Upstox WebSocket] Depth update for ${symbol}:`, {
        bids: data.bids?.length || 0,
        asks: data.asks?.length || 0,
      });

      // Get best bid/ask from depth
      const bestBid = data.bids?.[0];
      const bestAsk = data.asks?.[0];

      // Create tick data with depth information
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
          bids: data.bids?.map((b) => ({
            price: b.price,
            quantity: b.quantity,
            orders: b.orders || 0,
          })) || [],
          asks: data.asks?.map((a) => ({
            price: a.price,
            quantity: a.quantity,
            orders: a.orders || 0,
          })) || [],
        },
      };

      this.emitTick(tick);

      // Also emit depth data separately for UI components
      if (tick.depth) {
        this.emitDepth({
          symbol,
          exchange,
          bids: tick.depth.bids,
          asks: tick.depth.asks,
        });
      }
    });

    // Listen for status/connection events
    this.statusUnlisten = await listen<{
      provider: string;
      status: string;
      message?: string;
      timestamp: number;
    }>('upstox_status', (event) => {
      const status = event.payload;
      console.log(`[Upstox WebSocket] Status: ${status.status} - ${status.message || ''}`);

      // Handle disconnection
      if (status.status === 'disconnected' || status.status === 'error') {
        this.wsConnected = false;

        // Auto-fallback to polling on disconnect
        if (!this.usePollingFallback) {
          console.warn('[Upstox] WebSocket disconnected, enabling polling fallback');
          this.usePollingFallback = true;
          this.startQuotePolling();
        }
      } else if (status.status === 'connected') {
        this.wsConnected = true;
        // Disable polling fallback when connected
        if (this.usePollingFallback) {
          console.log('[Upstox] WebSocket reconnected, disabling polling fallback');
          this.usePollingFallback = false;
          this.stopQuotePolling();
        }
      }
    });

    this.wsConnected = true;
  }

  /**
   * Disconnect from WebSocket
   */
  private async disconnectRealWebSocket(): Promise<void> {
    // Cleanup Tauri event listeners
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

    // Call Rust backend to disconnect
    try {
      await invoke('upstox_ws_disconnect');
    } catch (err) {
      console.warn('[Upstox] WebSocket disconnect error:', err);
    }

    this.wsConnected = false;
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();

    console.log('[Upstox] Starting REST API polling (fallback mode, 2 min interval)');

    // Poll every 2 minutes
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

    console.log(`[Upstox] Polling ${this.pollingSymbols.size} symbols...`);

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
        console.error(`[Upstox] Poll error for ${symbol}:`, error);
      }

      // Rate limit delay
      await new Promise(resolve => setTimeout(resolve, 1000));
    }

    console.log(`[Upstox] Polling complete`);
  }

  async disconnectWebSocket(): Promise<void> {
    // Disconnect real WebSocket
    await this.disconnectRealWebSocket();

    // Stop polling fallback
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;

    this.wsConnected = false;
    console.log('[Upstox] WebSocket disconnected');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    const instrumentKey = await this.getInstrumentKey(symbol, exchange);
    const key = `${exchange}:${symbol}`;

    // Always track in pollingSymbols for fallback
    this.pollingSymbols.set(key, { symbol, exchange });

    if (this.wsConnected && !this.usePollingFallback) {
      // Use real WebSocket subscription
      try {
        const wsMode = mode === 'full' ? 'full' : 'ltpc';

        await invoke('upstox_ws_subscribe', {
          symbol: instrumentKey,
          mode: wsMode,
        });

        console.log(`[Upstox] ✓ WebSocket subscribed to ${instrumentKey} (mode: ${wsMode})`);
      } catch (error) {
        console.error(`[Upstox] WebSocket subscribe failed for ${instrumentKey}:`, error);
        // Fallback to polling for this symbol
        if (!this.usePollingFallback) {
          this.usePollingFallback = true;
          this.startQuotePolling();
        }
      }
    } else {
      // Using polling fallback
      console.log(`[Upstox] Added ${key} to polling list (${this.pollingSymbols.size} symbols)`);

      // Trigger immediate poll
      if (this.usePollingFallback) {
        this.pollQuotes();
      }
    }
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const instrumentKey = await this.getInstrumentKey(symbol, exchange);
    const key = `${exchange}:${symbol}`;

    // Remove from polling symbols
    this.pollingSymbols.delete(key);

    if (this.wsConnected && !this.usePollingFallback) {
      // Unsubscribe via real WebSocket
      try {
        await invoke('upstox_ws_unsubscribe', {
          symbol: instrumentKey,
        });
        console.log(`[Upstox] ✓ WebSocket unsubscribed from ${instrumentKey}`);
      } catch (error) {
        console.warn(`[Upstox] WebSocket unsubscribe failed for ${instrumentKey}:`, error);
      }
    } else {
      console.log(`[Upstox] Removed ${key} from polling list (${this.pollingSymbols.size} remaining)`);
    }
  }

  /**
   * Cleanup WebSocket and polling on logout
   */
  async logout(): Promise<void> {
    // Disconnect real WebSocket
    await this.disconnectRealWebSocket();

    // Stop polling fallback
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
      }>('upstox_search_symbol', {
        keyword: query,
        exchange: exchange ? toUpstoxExchange(exchange) : null,
        limit: 20,
      });

      if (!response.success || !response.data) {
        return [];
      }

      return response.data.map(item => ({
        symbol: String(item.trading_symbol || item.tradingsymbol || ''),
        exchange: String(item.exchange || 'NSE') as StockExchange,
        name: String(item.name || item.trading_symbol || ''),
        token: String(item.instrument_key || ''),
        lotSize: Number(item.lot_size || 1),
        tickSize: Number(item.tick_size || 0.05),
        instrumentType: mapInstrumentType(String(item.instrument_type || 'EQ')),
        currency: 'INR',
        expiry: item.expiry ? String(item.expiry) : undefined,
        strike: item.strike_price ? Number(item.strike_price) : undefined,
      }));
    } catch (error) {
      console.error('[Upstox] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const instrumentKey = await this.getInstrumentKey(symbol, exchange);

      // Search for the symbol to get full details
      const results = await this.searchSymbols(symbol, exchange);
      const instrument = results.find(i => i.symbol === symbol && i.exchange === exchange);

      if (instrument) {
        return instrument;
      }

      // Return minimal instrument with key
      return {
        symbol,
        exchange,
        name: symbol,
        token: instrumentKey,
        lotSize: 1,
        tickSize: 0.05,
        instrumentType: 'EQUITY',
        currency: 'INR',
      };
    } catch (error) {
      console.error('[Upstox] getInstrument error:', error);
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
   * Get Upstox instrument key for a symbol
   */
  private async getInstrumentKey(symbol: string, exchange: StockExchange): Promise<string> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: string;
        error?: string;
      }>('upstox_get_instrument_key', {
        symbol,
        exchange: toUpstoxExchange(exchange),
      });

      if (response.success && response.data) {
        return response.data;
      }

      // Fallback: construct instrument key
      const upstoxExchange = toUpstoxExchange(exchange);
      return `${upstoxExchange}|${symbol}`;
    } catch {
      const upstoxExchange = toUpstoxExchange(exchange);
      return `${upstoxExchange}|${symbol}`;
    }
  }

  // ============================================================================
  // Master Contract
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    await invoke('upstox_download_master_contract');
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: { last_updated: number };
        error?: string;
      }>('upstox_get_master_contract_metadata');

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
    FUTIDX: 'FUTURE',
    FUTSTK: 'FUTURE',
    OPTIDX: 'OPTION',
    OPTSTK: 'OPTION',
    FUT: 'FUTURE',
    OPT: 'OPTION',
  };
  return typeMap[type.toUpperCase()] || 'EQUITY';
}

// Export singleton instance
export const upstoxAdapter = new UpstoxAdapter();
