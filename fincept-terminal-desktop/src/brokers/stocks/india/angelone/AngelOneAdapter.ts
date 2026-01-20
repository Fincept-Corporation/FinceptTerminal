/**
 * Angel One Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Angel One Smart API
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
import {
  ANGELONE_METADATA,
  ANGELONE_API_BASE_URL,
  ANGELONE_ENDPOINTS,
  ANGELONE_EXCHANGE_MAP,
  ANGELONE_INTERVAL_LIMITS,
  getAngelOneHeaders,
} from './constants';
import {
  toAngelOneOrderParams,
  toAngelOneModifyParams,
  toAngelOneInterval,
  toAngelOneWSMode,
  fromAngelOneOrder,
  fromAngelOneTrade,
  fromAngelOnePosition,
  fromAngelOneHolding,
  fromAngelOneFunds,
  fromAngelOneQuote,
  fromAngelOneOHLCV,
  fromAngelOneDepth,
  fromAngelOneTick,
} from './mapper';

/**
 * Angel One broker adapter
 */
export class AngelOneAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'angelone';
  readonly brokerName = 'Angel One';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = ANGELONE_METADATA;

  private apiKey: string | null = null;
  private clientCode: string | null = null;
  private totpSecret: string | null = null;
  private password: string | null = null;
  private feedToken: string | null = null;

  // Tauri event unlisteners for WebSocket events
  private tickerUnlisten: UnlistenFn | null = null;
  private orderbookUnlisten: UnlistenFn | null = null;
  private statusUnlisten: UnlistenFn | null = null;

  // Fallback: REST API polling for quotes (when WebSocket fails)
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange }> = new Map();
  private usePollingFallback: boolean = false;

  // Token to symbol mapping
  private tokenSymbolMap: Map<string, { symbol: string; exchange: StockExchange }> = new Map();

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Set API credentials before authentication
   */
  setCredentials(apiKey: string, apiSecret: string): void {
    this.apiKey = apiKey;
    // apiSecret can be used for TOTP or other purposes
  }

  /**
   * Authenticate with Angel One
   * Requires: apiKey, clientCode (userId), password, and TOTP
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey;
      this.clientCode = credentials.userId || null;
      this.password = credentials.password || null;
      this.totpSecret = credentials.totpSecret || null;

      // If access token provided, validate it
      if (credentials.accessToken) {
        console.log('[AngelOne] Access token provided, validating...');
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
          console.error('[AngelOne] Access token validation failed');
          return {
            success: false,
            message: 'Invalid or expired access token',
            errorCode: 'AUTH_FAILED',
          };
        }
      }

      // Need client code, password, and TOTP for login
      if (!this.clientCode || !this.password) {
        return {
          success: false,
          message: 'Client code and password are required',
          errorCode: 'AUTH_REQUIRED',
        };
      }

      // Perform login via Rust backend
      const response = await invoke<{
        success: boolean;
        access_token?: string;
        refresh_token?: string;
        feed_token?: string;
        user_id?: string;
        error?: string;
      }>('angelone_login', {
        apiKey: this.apiKey,
        clientCode: this.clientCode,
        password: this.password,
        totp: credentials.pin || '', // TOTP can be passed as pin
      });

      if (!response.success || !response.access_token) {
        return {
          success: false,
          message: response.error || 'Login failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.accessToken = response.access_token;
      this.feedToken = response.feed_token || null;
      this.userId = response.user_id || this.clientCode;
      this._isConnected = true;

      // Store credentials
      await this.storeCredentials({
        apiKey: this.apiKey!,
        accessToken: this.accessToken,
        refreshToken: response.refresh_token,
        userId: this.userId || undefined,
      });

      return {
        success: true,
        accessToken: this.accessToken,
        refreshToken: response.refresh_token,
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
   * Handle auth callback (not typically used for Angel One as it uses TOTP)
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    // Angel One doesn't use OAuth callback, but we implement for interface compliance
    return {
      success: false,
      message: 'Angel One uses TOTP authentication, not OAuth callback',
      errorCode: 'NOT_SUPPORTED',
    };
  }

  /**
   * Validate access token
   */
  private async validateToken(token: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean }>('angelone_validate_token', {
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
      this.accessToken = credentials.accessToken || null;
      this.userId = credentials.userId || null;

      if (this.accessToken) {
        console.log('[AngelOne] Validating stored access token...');
        const isValid = await this.validateToken(this.accessToken);

        if (!isValid) {
          console.warn('[AngelOne] Access token expired, clearing from storage...');

          // Clear expired token from database but keep API key
          await this.storeCredentials({
            apiKey: this.apiKey,
          });
          this.accessToken = null;
          this._isConnected = false;
          return false;
        }

        console.log('[AngelOne] Token is valid, session restored');
        this._isConnected = true;
        return true;
      }

      return false;
    } catch {
      return false;
    }
  }

  /**
   * Refresh session (generate new token using refresh token)
   */
  async refreshSession(): Promise<AuthResponse> {
    try {
      const response = await invoke<{
        success: boolean;
        access_token?: string;
        error?: string;
      }>('angelone_refresh_token', {
        apiKey: this.apiKey,
        refreshToken: this.accessToken, // Using stored token
      });

      if (!response.success || !response.access_token) {
        return {
          success: false,
          message: response.error || 'Token refresh failed',
          errorCode: 'REFRESH_FAILED',
        };
      }

      this.accessToken = response.access_token;
      this._isConnected = true;

      return {
        success: true,
        accessToken: this.accessToken,
        message: 'Session refreshed',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Refresh failed',
        errorCode: 'REFRESH_FAILED',
      };
    }
  }

  // ============================================================================
  // Orders
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    // Get symbol token first
    const instrument = await this.getInstrument(params.symbol, params.exchange);
    const symbolToken = instrument?.token || '0';

    const angelParams = toAngelOneOrderParams(params, symbolToken);

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('angelone_place_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      params: angelParams,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    // Get original order info for modify
    const orders = await this.getOrders();
    const originalOrder = orders.find(o => o.orderId === params.orderId);

    if (!originalOrder) {
      return {
        success: false,
        message: 'Original order not found',
        errorCode: 'ORDER_NOT_FOUND',
      };
    }

    const instrument = await this.getInstrument(originalOrder.symbol, originalOrder.exchange);
    const symbolToken = instrument?.token || '0';

    const angelParams = toAngelOneModifyParams(params.orderId, params, {
      symbol: originalOrder.symbol,
      exchange: originalOrder.exchange,
      symbolToken,
    });

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('angelone_modify_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      params: angelParams,
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
    }>('angelone_cancel_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      orderId,
      variety: 'NORMAL',
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
    }>('angelone_get_order_book', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAngelOneOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('angelone_get_trade_book', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAngelOneTrade);
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    // Get all open orders
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
    // Get all open positions
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
        // Determine action based on position
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
    // Get current position size if not provided
    let positionSize = params.positionSize ?? 0;
    if (params.positionSize === undefined) {
      const position = await this.getOpenPosition(params.symbol, params.exchange, params.productType);
      positionSize = position?.quantity ?? 0;
    }

    // If both position_size and current_position are 0, place regular order
    if (positionSize === 0 && params.quantity !== 0) {
      return this.placeOrder(params);
    }

    // If position matches desired size, no action needed
    if (positionSize === params.positionSize) {
      return {
        success: true,
        message: params.quantity === 0
          ? 'No open position found. Not placing exit order.'
          : 'No action needed. Position size matches current position',
      };
    }

    // Calculate action and quantity needed
    let action: 'BUY' | 'SELL';
    let quantity: number;

    const targetPosition = params.positionSize ?? (params.side === 'BUY' ? params.quantity : -params.quantity);

    if (targetPosition === 0) {
      // Close position
      action = positionSize > 0 ? 'SELL' : 'BUY';
      quantity = Math.abs(positionSize);
    } else if (positionSize === 0) {
      // Open new position
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
    }>('angelone_get_positions', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAngelOnePosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: {
        holdings?: Record<string, unknown>[];
      };
      error?: string;
    }>('angelone_get_holdings', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data?.holdings) {
      return [];
    }

    return response.data.holdings.map(fromAngelOneHolding);
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('angelone_get_rms', {
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

    return fromAngelOneFunds(response.data);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Angel One margin calculation via batch endpoint
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('angelone_calculate_margin', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      orders: orders.map(o => ({
        symbol: o.symbol,
        exchange: o.exchange,
        quantity: o.quantity,
        price: o.price || 0,
        productType: o.productType,
        transactionType: o.side,
      })),
    });

    if (!response.success || !response.data) {
      return {
        totalMargin: 0,
        initialMargin: 0,
      };
    }

    const data = response.data;
    return {
      totalMargin: Number(data.totalMarginRequired || 0),
      initialMargin: Number(data.spanMargin || data.totalMarginRequired || 0),
      exposureMargin: Number(data.exposureMargin || 0),
      orders: orders.map(o => ({
        symbol: o.symbol,
        margin: Number(data.totalMarginRequired || 0) / orders.length,
      })),
    };
  }

  // ============================================================================
  // Market Data
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    // Get token for the symbol
    const instrument = await this.getInstrument(symbol, exchange);
    const token = instrument?.token || '0';

    // Normalize exchange for index symbols
    let apiExchange = ANGELONE_EXCHANGE_MAP[exchange] || exchange;
    if (exchange === 'NSE_INDEX') apiExchange = 'NSE';
    if (exchange === 'BSE_INDEX') apiExchange = 'BSE';
    if (exchange === 'MCX_INDEX') apiExchange = 'MCX';

    const response = await invoke<{
      success: boolean;
      data?: {
        fetched?: Record<string, unknown>[];
      };
      error?: string;
    }>('angelone_get_quote', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      exchange: apiExchange,
      tokens: [token],
    });

    if (!response.success || !response.data?.fetched?.[0]) {
      throw new Error(response.error || 'Failed to fetch quote');
    }

    return fromAngelOneQuote(response.data.fetched[0], symbol, exchange);
  }

  /**
   * Get multiple quotes (with automatic batching - max 50 per request)
   */
  async getMultiQuotes(
    symbols: Array<{ symbol: string; exchange: StockExchange }>
  ): Promise<Array<{ symbol: string; exchange: StockExchange; quote?: Quote; error?: string }>> {
    this.ensureConnected();

    const BATCH_SIZE = 50;
    const RATE_LIMIT_DELAY = 1000; // 1 second between batches
    const results: Array<{ symbol: string; exchange: StockExchange; quote?: Quote; error?: string }> = [];

    // Build token map
    const symbolMap: Map<string, { symbol: string; exchange: StockExchange }> = new Map();

    for (let i = 0; i < symbols.length; i += BATCH_SIZE) {
      const batch = symbols.slice(i, i + BATCH_SIZE);

      // Group by exchange
      const exchangeGroups: Map<string, string[]> = new Map();

      for (const { symbol, exchange } of batch) {
        try {
          const instrument = await this.getInstrument(symbol, exchange);
          const token = instrument?.token || '0';

          let apiExchange = ANGELONE_EXCHANGE_MAP[exchange] || exchange;
          if (exchange === 'NSE_INDEX') apiExchange = 'NSE';
          if (exchange === 'BSE_INDEX') apiExchange = 'BSE';

          if (!exchangeGroups.has(apiExchange)) {
            exchangeGroups.set(apiExchange, []);
          }
          exchangeGroups.get(apiExchange)!.push(token);
          symbolMap.set(`${apiExchange}:${token}`, { symbol, exchange });
        } catch (error) {
          results.push({
            symbol,
            exchange,
            error: 'Failed to resolve token',
          });
        }
      }

      // Fetch quotes for each exchange group
      for (const [apiExchange, tokens] of exchangeGroups) {
        try {
          const response = await invoke<{
            success: boolean;
            data?: {
              fetched?: Record<string, unknown>[];
            };
            error?: string;
          }>('angelone_get_quote', {
            apiKey: this.apiKey,
            accessToken: this.accessToken,
            exchange: apiExchange,
            tokens,
          });

          if (response.success && response.data?.fetched) {
            for (const quoteData of response.data.fetched) {
              const token = String(quoteData.symbolToken || '');
              const key = `${apiExchange}:${token}`;
              const symbolInfo = symbolMap.get(key);

              if (symbolInfo) {
                try {
                  const quote = fromAngelOneQuote(quoteData, symbolInfo.symbol, symbolInfo.exchange);
                  results.push({ ...symbolInfo, quote });
                } catch (err) {
                  results.push({ ...symbolInfo, error: 'Parse error' });
                }
              }
            }
          }
        } catch (error) {
          console.error(`[AngelOne] Quote batch error for ${apiExchange}:`, error);
        }
      }

      // Rate limit delay between batches
      if (i + BATCH_SIZE < symbols.length) {
        await new Promise(resolve => setTimeout(resolve, RATE_LIMIT_DELAY));
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
    const token = instrument?.token || '0';

    const interval = toAngelOneInterval(timeframe);
    const chunkDays = ANGELONE_INTERVAL_LIMITS[timeframe] || 30;

    const allCandles: OHLCV[] = [];
    let currentStart = new Date(from);
    const endDate = new Date(to);

    // Normalize exchange
    let apiExchange = ANGELONE_EXCHANGE_MAP[exchange] || exchange;
    if (exchange === 'NSE_INDEX') apiExchange = 'NSE';
    if (exchange === 'BSE_INDEX') apiExchange = 'BSE';

    while (currentStart <= endDate) {
      const currentEnd = new Date(currentStart);
      currentEnd.setDate(currentEnd.getDate() + chunkDays - 1);
      if (currentEnd > endDate) currentEnd.setTime(endDate.getTime());

      try {
        const response = await invoke<{
          success: boolean;
          data?: unknown[][];
          error?: string;
        }>('angelone_get_historical', {
          apiKey: this.apiKey,
          accessToken: this.accessToken,
          exchange: apiExchange,
          symbolToken: token,
          interval,
          fromDate: currentStart.toISOString().replace('T', ' ').slice(0, 16),
          toDate: currentEnd.toISOString().replace('T', ' ').slice(0, 16),
        });

        if (response.success && response.data) {
          const candles = response.data.map(fromAngelOneOHLCV);
          allCandles.push(...candles);
        }

        currentStart.setDate(currentEnd.getDate() + 1);

        // Rate limit delay
        if (currentStart <= endDate) {
          await new Promise(resolve => setTimeout(resolve, 500));
        }
      } catch (error) {
        console.error(`[AngelOne] OHLCV chunk error:`, error);
        currentStart.setDate(currentEnd.getDate() + 1);
      }
    }

    // Sort and dedupe
    return allCandles
      .sort((a, b) => a.timestamp - b.timestamp)
      .filter((c, i, arr) => i === 0 || c.timestamp !== arr[i - 1].timestamp);
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    try {
      const quote = await this.getQuote(symbol, exchange);
      // Quote includes depth data from the full quote mode
      // We need to fetch with depth mode
      const instrument = await this.getInstrument(symbol, exchange);
      const token = instrument?.token || '0';

      let apiExchange = ANGELONE_EXCHANGE_MAP[exchange] || exchange;
      if (exchange === 'NSE_INDEX') apiExchange = 'NSE';
      if (exchange === 'BSE_INDEX') apiExchange = 'BSE';

      const response = await invoke<{
        success: boolean;
        data?: {
          fetched?: Record<string, unknown>[];
        };
        error?: string;
      }>('angelone_get_quote', {
        apiKey: this.apiKey,
        accessToken: this.accessToken,
        exchange: apiExchange,
        tokens: [token],
        mode: 'FULL',
      });

      if (!response.success || !response.data?.fetched?.[0]) {
        return { bids: [], asks: [] };
      }

      const quoteData = response.data.fetched[0];
      if (quoteData.depth) {
        return fromAngelOneDepth(quoteData.depth as Record<string, unknown>);
      }

      return { bids: [], asks: [] };
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
      console.log('[AngelOne] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[AngelOne] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  /**
   * Connect to real Angel One WebSocket via Tauri/Rust backend
   */
  private async connectRealWebSocket(): Promise<void> {
    if (!this.accessToken || !this.apiKey || !this.feedToken) {
      throw new Error('Access token, API key, and feed token required');
    }

    // Call Rust backend to connect WebSocket
    const result = await invoke<{
      success: boolean;
      data?: boolean;
      error?: string;
    }>('angelone_ws_connect', {
      apiKey: this.apiKey,
      clientCode: this.clientCode || this.userId || '',
      feedToken: this.feedToken,
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
    }>('angelone_ticker', (event) => {
      const tick = event.payload;

      // Lookup symbol from token map
      const symbolInfo = this.tokenSymbolMap.get(tick.symbol);

      this.emitTick({
        symbol: symbolInfo?.symbol || tick.symbol,
        exchange: symbolInfo?.exchange || ('NSE' as StockExchange),
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
    }>('angelone_orderbook', (event) => {
      const data = event.payload;

      // Parse symbol from AngelOne format (e.g., "NSE:RELIANCE-EQ" -> "RELIANCE")
      const [exchangePart, symbolPart] = data.symbol.includes(':')
        ? data.symbol.split(':')
        : ['NSE', data.symbol];
      const symbol = symbolPart?.replace('-EQ', '') || data.symbol;
      const exchange = (exchangePart as StockExchange) || 'NSE';

      console.log(`[AngelOne WebSocket] Depth update for ${symbol}:`, {
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
    }>('angelone_status', (event) => {
      const status = event.payload;
      console.log(`[AngelOne WebSocket] Status: ${status.status} - ${status.message || ''}`);

      if (status.status === 'disconnected' || status.status === 'error') {
        this.wsConnected = false;
        if (!this.usePollingFallback) {
          console.warn('[AngelOne] WebSocket disconnected, enabling polling fallback');
          this.usePollingFallback = true;
          this.startQuotePolling();
        }
      } else if (status.status === 'connected') {
        this.wsConnected = true;
        if (this.usePollingFallback) {
          console.log('[AngelOne] WebSocket reconnected, disabling polling fallback');
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
      await invoke('angelone_ws_disconnect');
    } catch (err) {
      console.warn('[AngelOne] WebSocket disconnect error:', err);
    }

    this.wsConnected = false;
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();

    console.log('[AngelOne] Starting REST API polling (fallback mode, 2 min interval)');

    // Poll every 2 minutes (to stay within rate limits)
    this.quotePollingInterval = setInterval(async () => {
      await this.pollQuotesSequentially();
    }, 120000);

    // Initial poll
    this.pollQuotesSequentially();
  }

  private stopQuotePolling(): void {
    if (this.quotePollingInterval) {
      clearInterval(this.quotePollingInterval);
      this.quotePollingInterval = null;
    }
  }

  private async pollQuotesSequentially(): Promise<void> {
    if (this.pollingSymbols.size === 0) return;

    console.log(`[AngelOne] Polling ${this.pollingSymbols.size} symbols...`);

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
        console.error(`[AngelOne] Poll error for ${symbol}:`, error);
      }

      // Rate limit: 1 second between requests
      await new Promise(resolve => setTimeout(resolve, 1000));
    }

    console.log(`[AngelOne] Polling complete`);
  }

  async disconnectWebSocket(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    this.wsConnected = false;
    console.log('[AngelOne] WebSocket disconnected');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.set(key, { symbol, exchange });

    // Get token and store in map for lookup
    const instrument = await this.getInstrument(symbol, exchange);
    const token = instrument?.token || '0';
    this.tokenSymbolMap.set(token, { symbol, exchange });

    if (this.wsConnected && !this.usePollingFallback) {
      try {
        const wsMode = toAngelOneWSMode(mode);

        await invoke('angelone_ws_subscribe', {
          symbol: token,
          exchange: ANGELONE_EXCHANGE_MAP[exchange] || exchange,
          mode: wsMode,
        });

        console.log(`[AngelOne] ✓ WebSocket subscribed to ${symbol} (token: ${token}, mode: ${wsMode})`);
      } catch (error) {
        console.error(`[AngelOne] WebSocket subscribe failed:`, error);
        if (!this.usePollingFallback) {
          this.usePollingFallback = true;
          this.startQuotePolling();
        }
      }
    } else {
      console.log(`[AngelOne] Added ${key} to polling (${this.pollingSymbols.size} total)`);
      if (this.usePollingFallback) {
        this.pollQuotesSequentially();
      }
    }
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);

    // Get token
    const instrument = await this.getInstrument(symbol, exchange);
    const token = instrument?.token || '0';
    this.tokenSymbolMap.delete(token);

    if (this.wsConnected && !this.usePollingFallback) {
      try {
        await invoke('angelone_ws_unsubscribe', {
          symbol: token,
          exchange: ANGELONE_EXCHANGE_MAP[exchange] || exchange,
        });
        console.log(`[AngelOne] ✓ WebSocket unsubscribed from ${symbol}`);
      } catch (error) {
        console.warn(`[AngelOne] WebSocket unsubscribe failed:`, error);
      }
    } else {
      console.log(`[AngelOne] Removed ${key} from polling`);
    }
  }

  async logout(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.tokenSymbolMap.clear();
    this.usePollingFallback = false;
    await super.logout();
  }

  // ============================================================================
  // Symbol Search
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: Array<Record<string, unknown>>;
        error?: string;
      }>('angelone_search_symbols', {
        apiKey: this.apiKey,
        accessToken: this.accessToken,
        query,
        exchange: exchange || null,
      });

      if (!response.success || !response.data) {
        return [];
      }

      return response.data.map(item => ({
        symbol: String(item.symbol || item.tradingsymbol || ''),
        exchange: String(item.exchange || 'NSE') as StockExchange,
        name: String(item.name || item.symbol || ''),
        token: String(item.symboltoken || item.token || '0'),
        lotSize: Number(item.lotsize || 1),
        tickSize: Number(item.ticksize || 0.05),
        instrumentType: String(item.instrumenttype || 'EQ') as any,
        currency: 'INR',
        expiry: item.expiry ? String(item.expiry) : undefined,
        strike: item.strike ? Number(item.strike) : undefined,
      }));
    } catch (error) {
      console.error('[AngelOne] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('angelone_get_instrument', {
        apiKey: this.apiKey,
        symbol,
        exchange,
      });

      if (!response.success || !response.data) {
        // Return fallback
        return {
          symbol,
          exchange,
          name: symbol,
          token: '0',
          lotSize: 1,
          tickSize: 0.05,
          instrumentType: 'EQUITY',
          currency: 'INR',
        };
      }

      const data = response.data;
      return {
        symbol: String(data.symbol || data.tradingsymbol || symbol),
        exchange: String(data.exchange || exchange) as StockExchange,
        name: String(data.name || symbol),
        token: String(data.symboltoken || data.token || '0'),
        lotSize: Number(data.lotsize || 1),
        tickSize: Number(data.ticksize || 0.05),
        instrumentType: String(data.instrumenttype || 'EQ') as any,
        currency: 'INR',
        expiry: data.expiry ? String(data.expiry) : undefined,
        strike: data.strike ? Number(data.strike) : undefined,
      };
    } catch (error) {
      console.error('[AngelOne] getInstrument error:', error);
      return {
        symbol,
        exchange,
        name: symbol,
        token: '0',
        lotSize: 1,
        tickSize: 0.05,
        instrumentType: 'EQUITY',
        currency: 'INR',
      };
    }
  }

  // ============================================================================
  // Master Contract
  // ============================================================================

  async downloadMasterContract(): Promise<void> {
    await invoke('angelone_download_master_contract', {
      apiKey: this.apiKey,
    });
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    try {
      const response = await invoke<{ timestamp?: number }>('angelone_master_contract_info');
      return response.timestamp ? new Date(response.timestamp) : null;
    } catch {
      return null;
    }
  }
}

// Export singleton instance
export const angelOneAdapter = new AngelOneAdapter();
