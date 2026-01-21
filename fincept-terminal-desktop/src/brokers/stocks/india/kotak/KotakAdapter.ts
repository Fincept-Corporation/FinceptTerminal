/**
 * Kotak Neo Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Kotak Neo API
 * Based on OpenAlgo broker integration patterns
 *
 * Auth Flow: TOTP + MPIN (2-step)
 * 1. Login with TOTP → get view_token, view_sid
 * 2. Validate with MPIN → get trading_token, trading_sid, base_url
 *
 * Auth token format: trading_token:::trading_sid:::base_url:::access_token
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
import { KOTAK_METADATA } from './constants';
import {
  toKotakExchange,
  toKotakOrderParams,
  toKotakModifyParams,
  fromKotakOrder,
  fromKotakTrade,
  fromKotakPosition,
  fromKotakHolding,
  fromKotakFunds,
  fromKotakQuote,
} from './mapper';

/**
 * Kotak Neo broker adapter
 *
 * IMPORTANT: Kotak uses a unique 2-step authentication:
 * Step 1: TOTP login with mobile, UCC, TOTP
 * Step 2: MPIN validation
 */
export class KotakAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'kotak';
  readonly brokerName = 'Kotak Neo';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = KOTAK_METADATA;

  private ucc: string | null = null; // User Client Code (stored as apiKey)
  protected accessToken: string | null = null; // neo-fin-key access token (stored as apiSecret)
  private mobileNumber: string | null = null;
  private mpin: string | null = null; // 6-digit MPIN (stored as userId)
  protected apiKey: string | null = null; // Alias for UCC (for base class compatibility)

  // Intermediate auth state (between TOTP and MPIN)
  private viewToken: string | null = null;
  private viewSid: string | null = null;

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
   *
   * For Kotak (OpenAlgo mapping):
   * - apiKey = UCC (User Client Code)
   * - apiSecret = Access Token (neo-fin-key consumer token)
   */
  setCredentials(apiKey: string, apiSecret: string): void {
    this.ucc = apiKey; // UCC is primary identifier
    this.apiKey = apiKey; // Set apiKey for base class compatibility
    this.accessToken = apiSecret; // Access token for authorization
  }

  /**
   * Set MPIN separately (Kotak-specific)
   */
  setMpin(mpin: string): void {
    this.mpin = mpin;
  }

  /**
   * Set mobile number (required for Kotak auth)
   */
  setMobileNumber(mobile: string): void {
    this.mobileNumber = mobile;
  }

  /**
   * Get auth URL - Kotak doesn't use traditional OAuth
   * Returns info about the TOTP auth flow instead
   */
  getAuthUrl(): string {
    // Kotak uses TOTP + MPIN, not a web URL
    return 'totp://kotak-neo';
  }

  /**
   * Step 1: TOTP Login
   *
   * Call this with the TOTP from Google Authenticator
   * Returns view_token and view_sid for step 2
   */
  async loginWithTotp(totp: string): Promise<{ success: boolean; message: string }> {
    if (!this.ucc || !this.accessToken || !this.mobileNumber) {
      return {
        success: false,
        message: 'Missing credentials. UCC, Access Token, and Mobile Number required.',
      };
    }

    // Ensure mobile number has +91 prefix
    let mobile = this.mobileNumber.trim().replace(/\s/g, '');
    if (mobile.startsWith('+91')) {
      // Already has prefix
    } else if (mobile.startsWith('91') && mobile.length === 12) {
      mobile = `+${mobile}`;
    } else {
      mobile = `+91${mobile}`;
    }

    try {
      const response = await invoke<{
        success: boolean;
        data?: {
          view_token: string;
          view_sid: string;
        };
        error?: string;
      }>('kotak_totp_login', {
        ucc: this.ucc,
        accessToken: this.accessToken,
        mobileNumber: mobile,
        totp,
      });

      if (!response.success || !response.data) {
        return {
          success: false,
          message: response.error || 'TOTP login failed',
        };
      }

      // Store intermediate tokens
      this.viewToken = response.data.view_token;
      this.viewSid = response.data.view_sid;

      return {
        success: true,
        message: 'TOTP verified. Call validateMpin() to complete authentication.',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'TOTP login failed',
      };
    }
  }

  /**
   * Step 2: MPIN Validation
   *
   * Call this after successful TOTP login
   * Completes authentication and returns access token
   */
  async validateMpin(): Promise<AuthResponse> {
    if (!this.viewToken || !this.viewSid) {
      return {
        success: false,
        message: 'Call loginWithTotp() first',
        errorCode: 'AUTH_REQUIRED',
      };
    }

    if (!this.mpin || !this.ucc) {
      return {
        success: false,
        message: 'MPIN and UCC are required',
        errorCode: 'CREDENTIALS_MISSING',
      };
    }

    try {
      const response = await invoke<{
        success: boolean;
        data?: {
          auth_token: string; // Combined: trading_token:::trading_sid:::base_url:::access_token
          user_id: string;
        };
        error?: string;
      }>('kotak_mpin_validate', {
        viewToken: this.viewToken,
        viewSid: this.viewSid,
        ucc: this.ucc,
        mpin: this.mpin,
      });

      if (!response.success || !response.data) {
        return {
          success: false,
          message: response.error || 'MPIN validation failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.accessToken = response.data.auth_token;
      this.userId = response.data.user_id || this.ucc;
      this._isConnected = true;

      // Clear intermediate tokens
      this.viewToken = null;
      this.viewSid = null;

      // Store credentials
      await this.storeCredentials({
        apiKey: this.apiKey!,
        apiSecret: this.mpin,
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
        message: error instanceof Error ? error.message : 'MPIN validation failed',
        errorCode: 'AUTH_FAILED',
      };
    }
  }

  /**
   * Authenticate with Kotak
   *
   * For Kotak, this requires TOTP + MPIN flow:
   * 1. Call setCredentials(apiKey, mpin, ucc) and setMobileNumber(mobile)
   * 2. Call loginWithTotp(totp)
   * 3. Call validateMpin()
   *
   * If accessToken is provided in credentials, validates it instead
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.ucc = credentials.apiKey; // UCC stored as apiKey
      this.apiKey = credentials.apiKey; // Set for base class compatibility
      this.accessToken = credentials.apiSecret || null; // Access token as apiSecret
      this.mpin = credentials.userId || null; // MPIN as userId

      // If access token provided, validate it
      if (credentials.accessToken) {
        console.log('[Kotak] Access token provided, validating...');
        const isValid = await this.validateToken(credentials.accessToken);
        if (isValid) {
          this.accessToken = credentials.accessToken;
          this.userId = this.ucc;
          this._isConnected = true;

          await this.storeCredentials(credentials);

          return {
            success: true,
            accessToken: this.accessToken,
            userId: this.userId || undefined,
            message: 'Authentication successful',
          };
        } else {
          console.error('[Kotak] Access token validation failed');
          return {
            success: false,
            message: 'Invalid or expired access token',
            errorCode: 'AUTH_FAILED',
          };
        }
      }

      // No access token - need TOTP + MPIN flow
      return {
        success: false,
        message: 'Kotak requires TOTP + MPIN authentication. Use loginWithTotp() and validateMpin().',
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
   * Handle OAuth callback - not used for Kotak (uses TOTP + MPIN)
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    return {
      success: false,
      message: 'Kotak uses TOTP + MPIN authentication, not OAuth callback.',
      errorCode: 'NOT_SUPPORTED',
    };
  }

  /**
   * Validate access token
   */
  private async validateToken(token: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean }>('kotak_validate_token', {
        authToken: token,
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

      this.ucc = credentials.apiKey; // UCC stored as apiKey
      this.apiKey = credentials.apiKey; // Set for base class compatibility
      this.accessToken = credentials.apiSecret || credentials.accessToken || null; // Access token
      this.mpin = credentials.userId || null; // MPIN stored as userId
      this.userId = this.ucc; // Set userId to UCC for identification

      // Load mobile number from additionalData
      const additionalData = (credentials as any).additionalData;
      if (additionalData) {
        try {
          const parsed = JSON.parse(additionalData);
          this.mobileNumber = parsed.mobileNumber || null;
        } catch (e) {
          console.warn('[Kotak] Failed to parse additionalData:', e);
        }
      }

      if (this.accessToken) {
        console.log('[Kotak] Validating stored access token...');
        const isValid = await this.validateToken(this.accessToken);

        if (!isValid) {
          console.warn('[Kotak] Access token expired, clearing from storage...');
          await this.storeCredentials({
            apiKey: this.apiKey,
            apiSecret: this.mpin || undefined,
            userId: this.ucc || undefined,
          });
          this.accessToken = null;
          this._isConnected = false;
          return false;
        }

        console.log('[Kotak] Token is valid, session restored');
        this._isConnected = true;
        return true;
      }

      return false;
    } catch {
      return false;
    }
  }

  /**
   * Refresh session - Kotak requires re-authentication
   */
  async refreshSession(): Promise<AuthResponse> {
    return {
      success: false,
      message: 'Kotak requires re-authentication with TOTP + MPIN.',
      errorCode: 'REFRESH_NOT_SUPPORTED',
    };
  }

  // ============================================================================
  // Orders
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    const token = await this.getKotakToken(params.symbol, params.exchange);
    const kotakParams = toKotakOrderParams(token, params);

    const response = await invoke<{
      success: boolean;
      data?: { order_id: string };
      error?: string;
    }>('kotak_place_order', {
      authToken: this.accessToken,
      params: kotakParams,
    });

    return {
      success: response.success,
      orderId: response.data?.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const kotakParams = toKotakModifyParams(params.orderId, params);

    const response = await invoke<{
      success: boolean;
      data?: { order_id: string };
      error?: string;
    }>('kotak_modify_order', {
      authToken: this.accessToken,
      orderId: params.orderId,
      params: kotakParams,
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
    }>('kotak_cancel_order', {
      authToken: this.accessToken,
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
    }>('kotak_get_orders', {
      authToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromKotakOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('kotak_get_trade_book', {
      authToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromKotakTrade);
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
    }>('kotak_get_positions', {
      authToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromKotakPosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('kotak_get_holdings', {
      authToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromKotakHolding);
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('kotak_get_funds', {
      authToken: this.accessToken,
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

    return fromKotakFunds(response.data);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Kotak doesn't have a batch margin API, estimate based on individual orders
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
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('kotak_get_quotes', {
      authToken: this.accessToken,
      symbol,
      exchange,
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || 'Failed to fetch quote');
    }

    return fromKotakQuote(response.data, symbol, exchange);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    // Kotak Neo doesn't support historical data API
    console.warn('[Kotak] Historical data is not supported by Kotak Neo API');
    return [];
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('kotak_get_depth', {
        authToken: this.accessToken,
        symbol,
        exchange,
      });

      if (!response.success || !response.data) {
        return { bids: [], asks: [] };
      }

      const buyDepth = (response.data.bd as Array<{ bp: number; bq: number }>) || [];
      const sellDepth = (response.data.sd as Array<{ sp: number; sq: number }>) || [];

      return {
        bids: buyDepth.map(l => ({ price: l.bp, quantity: l.bq })),
        asks: sellDepth.map(l => ({ price: l.sp, quantity: l.sq })),
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
      await this.connectRealWebSocket();
      console.log('[Kotak] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[Kotak] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  private async connectRealWebSocket(): Promise<void> {
    if (!this.accessToken) {
      throw new Error('Access token required');
    }

    const result = await invoke<{
      success: boolean;
      data?: boolean;
      error?: string;
    }>('kotak_ws_connect', {
      accessToken: this.accessToken,
    });

    if (!result.success) {
      throw new Error(result.error || 'WebSocket connection failed');
    }

    this.tickerUnlisten = await listen<{
      provider: string;
      symbol: string;
      price: number;
      bid?: number;
      ask?: number;
      volume?: number;
      change_percent?: number;
      timestamp: number;
    }>('kotak_ticker', (event) => {
      const tick = event.payload;
      this.emitTick({
        symbol: tick.symbol,
        exchange: 'NSE' as StockExchange,
        mode: 'quote',
        lastPrice: tick.price,
        bid: tick.bid,
        ask: tick.ask,
        volume: tick.volume || 0,
        changePercent: tick.change_percent || 0,
        timestamp: tick.timestamp,
      });
    });

    this.orderbookUnlisten = await listen<{
      provider: string;
      symbol: string;
      bids: Array<{ price: number; quantity: number; orders?: number }>;
      asks: Array<{ price: number; quantity: number; orders?: number }>;
      timestamp: number;
    }>('kotak_orderbook', (event) => {
      const data = event.payload;
      const [exchangePart, symbolPart] = data.symbol.includes(':') ? data.symbol.split(':') : ['NSE', data.symbol];
      const symbol = symbolPart || data.symbol;
      const exchange = (exchangePart as StockExchange) || 'NSE';

      console.log(`[Kotak WebSocket] Depth update for ${symbol}`);

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
      provider: string;
      status: string;
      message?: string;
      timestamp: number;
    }>('kotak_status', (event) => {
      const status = event.payload;
      console.log(`[Kotak WebSocket] Status: ${status.status}`);
      if (status.status === 'disconnected' || status.status === 'error') {
        this.wsConnected = false;
        if (!this.usePollingFallback) {
          this.usePollingFallback = true;
          this.startQuotePolling();
        }
      } else if (status.status === 'connected') {
        this.wsConnected = true;
        if (this.usePollingFallback) {
          this.usePollingFallback = false;
          this.stopQuotePolling();
        }
      }
    });

    this.wsConnected = true;
  }

  private async disconnectRealWebSocket(): Promise<void> {
    if (this.tickerUnlisten) { this.tickerUnlisten(); this.tickerUnlisten = null; }
    if (this.orderbookUnlisten) { this.orderbookUnlisten(); this.orderbookUnlisten = null; }
    if (this.statusUnlisten) { this.statusUnlisten(); this.statusUnlisten = null; }
    try { await invoke('kotak_ws_disconnect'); } catch (err) { console.warn('[Kotak] WS disconnect error:', err); }
    this.wsConnected = false;
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();
    console.log('[Kotak] Starting REST API polling (fallback mode)');
    this.quotePollingInterval = setInterval(async () => { await this.pollQuotes(); }, 30000);
    this.pollQuotes();
  }

  private stopQuotePolling(): void {
    if (this.quotePollingInterval) { clearInterval(this.quotePollingInterval); this.quotePollingInterval = null; }
  }

  private async pollQuotes(): Promise<void> {
    if (this.pollingSymbols.size === 0) return;
    for (const [key, { symbol, exchange }] of this.pollingSymbols) {
      try {
        const quote = await this.getQuoteInternal(symbol, exchange);
        this.emitTick({
          symbol, exchange,
          lastPrice: quote.lastPrice, open: quote.open, high: quote.high, low: quote.low,
          close: quote.close, volume: quote.volume, change: quote.change,
          changePercent: quote.changePercent, timestamp: quote.timestamp,
          bid: quote.bid, bidQty: quote.bidQty, ask: quote.ask, askQty: quote.askQty, mode: 'full',
        });
      } catch (error) { console.error(`[Kotak] Poll error for ${symbol}:`, error); }
      await new Promise(resolve => setTimeout(resolve, 200));
    }
  }

  async disconnectWebSocket(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    this.wsConnected = false;
  }

  protected async subscribeInternal(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.set(key, { symbol, exchange });
    if (this.wsConnected && !this.usePollingFallback) {
      try {
        await invoke('kotak_ws_subscribe', { symbol, mode: mode === 'full' ? 'full' : 'ltp' });
        console.log(`[Kotak] ✓ WebSocket subscribed to ${symbol}`);
      } catch (error) {
        console.error(`[Kotak] WebSocket subscribe failed:`, error);
        if (!this.usePollingFallback) { this.usePollingFallback = true; this.startQuotePolling(); }
      }
    } else {
      console.log(`[Kotak] Added ${key} to polling`);
      if (this.usePollingFallback) this.pollQuotes();
    }
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);
    if (this.wsConnected && !this.usePollingFallback) {
      try { await invoke('kotak_ws_unsubscribe', { symbol }); } catch (err) { console.warn('[Kotak] Unsubscribe failed:', err); }
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
      }>('kotak_search_symbol', {
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
        token: String(item.token || item.brsymbol || ''),
        lotSize: Number(item.lotsize || 1),
        tickSize: 0.05,
        instrumentType: mapInstrumentType(String(item.instrumenttype || 'EQ')),
        currency: 'INR',
      }));
    } catch (error) {
      console.error('[Kotak] searchSymbols error:', error);
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
      const token = await this.getKotakToken(symbol, exchange);
      return {
        symbol,
        exchange,
        name: symbol,
        token,
        lotSize: 1,
        tickSize: 0.05,
        instrumentType: 'EQUITY',
        currency: 'INR',
      };
    } catch (error) {
      console.error('[Kotak] getInstrument error:', error);
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
   * Get Kotak token (pSymbol) for a symbol
   */
  private async getKotakToken(symbol: string, exchange: StockExchange): Promise<string> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: string;
        error?: string;
      }>('kotak_get_token_for_symbol', {
        symbol,
        exchange,
      });

      if (response.success && response.data) {
        return response.data;
      }

      // Fallback: use symbol as token
      return symbol;
    } catch (error) {
      console.warn(`[Kotak] Failed to get token for ${symbol}, using symbol as fallback`);
      return symbol;
    }
  }

  // ============================================================================
  // Master Contract
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    await invoke('kotak_download_master_contract');
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: { last_updated: number };
        error?: string;
      }>('kotak_get_master_contract_metadata');

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
export const kotakAdapter = new KotakAdapter();
