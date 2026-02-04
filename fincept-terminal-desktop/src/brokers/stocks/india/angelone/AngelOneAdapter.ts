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
  private pollDebounceTimer: NodeJS.Timeout | null = null;

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
   * Requires: apiKey, clientCode (userId), password, and TOTP secret
   * The TOTP secret is passed to the Rust backend which generates the 6-digit code
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
          // Token expired - fall through to re-authenticate if we have credentials
          if (!this.clientCode || !this.password || !this.totpSecret) {
            return {
              success: false,
              message: 'Invalid or expired access token',
              errorCode: 'AUTH_FAILED',
            };
          }
          console.log('[AngelOne] Token expired, re-authenticating with stored credentials...');
        }
      }

      // Need client code, password, and TOTP secret for login
      if (!this.clientCode || !this.password) {
        return {
          success: false,
          message: 'Client code and password are required',
          errorCode: 'AUTH_REQUIRED',
        };
      }

      if (!this.totpSecret) {
        return {
          success: false,
          message: 'TOTP secret is required for authentication',
          errorCode: 'AUTH_REQUIRED',
        };
      }

      // Perform login via Rust backend (Rust generates 6-digit TOTP from secret)
      const response = await invoke<{
        success: boolean;
        data?: {
          access_token?: string;
          refresh_token?: string;
          feed_token?: string;
          user_id?: string;
        };
        error?: string;
      }>('angelone_login', {
        apiKey: this.apiKey,
        clientCode: this.clientCode,
        password: this.password,
        totp: this.totpSecret, // Rust backend generates 6-digit code from this secret
      });

      if (!response.success || !response.data?.access_token) {
        return {
          success: false,
          message: response.error || 'Login failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.accessToken = response.data.access_token;
      this.feedToken = response.data.feed_token || null;
      this.userId = response.data.user_id || this.clientCode;
      this._isConnected = true;

      console.log('[AngelOne] authenticate: login response', {
        hasAccessToken: !!this.accessToken,
        hasFeedToken: !!this.feedToken,
        feedTokenValue: this.feedToken ? `${this.feedToken.substring(0, 10)}...` : 'NULL',
        userId: this.userId,
      });

      // Store credentials including password, totpSecret, and feedToken for auto-re-auth + WS
      await this.storeCredentials({
        apiKey: this.apiKey!,
        apiSecret: JSON.stringify({ password: this.password, totpSecret: this.totpSecret, feedToken: this.feedToken }),
        accessToken: this.accessToken,
        refreshToken: response.data.refresh_token,
        userId: this.userId || undefined,
      });

      return {
        success: true,
        accessToken: this.accessToken,
        refreshToken: response.data.refresh_token,
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
   * Validate access token by calling getProfile API
   */
  private async validateToken(token: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean; error?: string }>('angelone_validate_token', {
        apiKey: this.apiKey,
        authToken: token,
      });

      console.log('[AngelOne] Token validation response:', { success: response.success, error: response.error });

      if (!response.success) {
        console.warn('[AngelOne] Token validation failed:', response.error);
        return false;
      }

      return true;
    } catch (err) {
      console.error('[AngelOne] Token validation error:', err);
      return false;
    }
  }

  /**
   * Initialize from stored credentials.
   * If token is expired but password + TOTP secret are stored, auto-re-authenticates.
   */
  async initFromStorage(): Promise<boolean> {
    try {
      console.log('[AngelOne] initFromStorage: loading credentials...');
      const credentials = await this.loadCredentials();
      if (!credentials) {
        console.log('[AngelOne] initFromStorage: no stored credentials found');
        return false;
      }

      console.log('[AngelOne] initFromStorage: credentials loaded', {
        hasApiKey: !!credentials.apiKey,
        hasAccessToken: !!credentials.accessToken,
        hasApiSecret: !!credentials.apiSecret,
        userId: credentials.userId,
        apiSecretLen: credentials.apiSecret?.length,
      });

      this.apiKey = credentials.apiKey;
      this.accessToken = credentials.accessToken || null;
      this.userId = credentials.userId || null;

      // Restore password, totpSecret, and feedToken from apiSecret (stored as JSON)
      if (credentials.apiSecret) {
        try {
          const secretData = JSON.parse(credentials.apiSecret);
          this.password = secretData.password || null;
          this.totpSecret = secretData.totpSecret || null;
          this.feedToken = secretData.feedToken || null;
          this.clientCode = credentials.userId || null;
          console.log('[AngelOne] initFromStorage: parsed apiSecret JSON', {
            hasPassword: !!this.password,
            hasTotpSecret: !!this.totpSecret,
            hasFeedToken: !!this.feedToken,
            clientCode: this.clientCode,
          });
        } catch {
          console.warn('[AngelOne] initFromStorage: apiSecret is not JSON, ignoring');
        }
      }

      if (this.accessToken) {
        console.log('[AngelOne] Validating stored access token...');
        const isValid = await this.validateToken(this.accessToken);

        if (!isValid) {
          console.warn('[AngelOne] Access token expired.');

          // Try auto-re-authentication if we have stored credentials
          if (this.apiKey && this.clientCode && this.password && this.totpSecret) {
            console.log('[AngelOne] Auto-re-authenticating with stored TOTP secret...');
            const authResult = await this.authenticate({
              apiKey: this.apiKey,
              userId: this.clientCode,
              password: this.password,
              totpSecret: this.totpSecret,
            });

            if (authResult.success) {
              console.log('[AngelOne] Auto-re-authentication successful');
              return true;
            }
            console.error('[AngelOne] Auto-re-authentication failed:', authResult.message);
          }

          // Clear expired token from database but keep API key and secrets
          await this.storeCredentials({
            apiKey: this.apiKey,
            apiSecret: this.password && this.totpSecret
              ? JSON.stringify({ password: this.password, totpSecret: this.totpSecret })
              : undefined,
            userId: this.userId || undefined,
          });
          this.accessToken = null;
          this._isConnected = false;
          return false;
        }

        // If feedToken is missing (old credentials format), re-authenticate to get it
        if (!this.feedToken) {
          console.warn('[AngelOne] feedToken is missing — WebSocket will not work without it');
          if (this.apiKey && this.clientCode && this.password && this.totpSecret) {
            console.log('[AngelOne] Attempting re-auth to obtain feedToken...');
            const authResult = await this.authenticate({
              apiKey: this.apiKey,
              userId: this.clientCode,
              password: this.password,
              totpSecret: this.totpSecret,
            });
            if (authResult.success) {
              console.log(`[AngelOne] ✓ Re-authenticated, feedToken=${!!this.feedToken}`);
              return true;
            }
            console.warn('[AngelOne] Re-auth for feedToken failed, WS will be unavailable');
          } else {
            console.warn('[AngelOne] Cannot re-auth: missing apiKey/clientCode/password/totpSecret');
          }
        }

        console.log(`[AngelOne] Token is valid, session restored (feedToken=${!!this.feedToken})`);
        this._isConnected = true;
        return true;
      }

      // No access token but have credentials - try to authenticate
      if (this.apiKey && this.clientCode && this.password && this.totpSecret) {
        console.log('[AngelOne] No token found, authenticating with stored credentials...');
        const authResult = await this.authenticate({
          apiKey: this.apiKey,
          userId: this.clientCode,
          password: this.password,
          totpSecret: this.totpSecret,
        });
        return authResult.success;
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
    // Use token from params if provided, otherwise lookup
    let symbolToken = params.token;
    if (!symbolToken) {
      const instrument = await this.getInstrument(params.symbol, params.exchange);
      symbolToken = instrument?.token || '0';
    }

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

    if (token === '0' || !token) {
      throw new Error(`No instrument token found for ${symbol} on ${exchange}. Download master contract first.`);
    }

    // Normalize exchange for index symbols
    let apiExchange = ANGELONE_EXCHANGE_MAP[exchange] || exchange;
    if (exchange === 'NSE_INDEX') apiExchange = 'NSE';
    if (exchange === 'BSE_INDEX') apiExchange = 'BSE';
    if (exchange === 'MCX_INDEX') apiExchange = 'MCX';

    console.log(`[AngelOne] getQuote: ${symbol} exchange=${apiExchange} token=${token}`);

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

          if (token === '0' || !token) {
            results.push({ symbol, exchange, error: 'No instrument token — download master contract first' });
            continue;
          }

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
    const overallStart = Date.now();
    console.log(`[AngelOne] getOHLCVInternal called:`, { symbol, exchange, timeframe, from: from.toISOString(), to: to.toISOString() });

    // Get instrument token
    const instrumentStart = Date.now();
    const instrument = await this.getInstrument(symbol, exchange);
    const token = instrument?.token || '0';
    console.log(`[AngelOne] getInstrument took ${Date.now() - instrumentStart}ms, token=${token}`);

    const interval = toAngelOneInterval(timeframe);
    const chunkDays = ANGELONE_INTERVAL_LIMITS[timeframe] || 30;

    console.log(`[AngelOne] DEBUG: interval=${interval}, chunkDays=${chunkDays}, apiKey=${this.apiKey ? 'set' : 'MISSING'}, accessToken=${this.accessToken ? 'set' : 'MISSING'}`);

    const allCandles: OHLCV[] = [];
    let currentStart = new Date(from);
    const endDate = new Date(to);

    // Normalize exchange
    let apiExchange = ANGELONE_EXCHANGE_MAP[exchange] || exchange;
    if (exchange === 'NSE_INDEX') apiExchange = 'NSE';
    if (exchange === 'BSE_INDEX') apiExchange = 'BSE';

    const MAX_RETRIES = 3;
    const MAX_CHUNKS = 50; // Safety limit to prevent runaway loops
    let chunkIndex = 0;

    while (currentStart <= endDate && chunkIndex < MAX_CHUNKS) {
      const currentEnd = new Date(currentStart);
      currentEnd.setDate(currentEnd.getDate() + chunkDays - 1);
      if (currentEnd > endDate) currentEnd.setTime(endDate.getTime());

      let chunkSuccess = false;
      let chunkRetries = 0;
      chunkIndex++;

      const fromDateStr = this.formatDateIST(currentStart);
      const toDateStr = this.formatDateIST(currentEnd);
      console.log(`[AngelOne] Chunk ${chunkIndex}: ${fromDateStr} -> ${toDateStr} (exchange=${apiExchange}, token=${token})`);

      while (!chunkSuccess && chunkRetries < MAX_RETRIES) {
        const chunkStart = Date.now();
        try {
          console.log(`[AngelOne] DEBUG: Invoking angelone_get_historical (chunk ${chunkIndex}, attempt ${chunkRetries + 1})...`);
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
            fromDate: fromDateStr,
            toDate: toDateStr,
          });

          const chunkElapsed = Date.now() - chunkStart;
          console.log(`[AngelOne] DEBUG: Invoke returned in ${chunkElapsed}ms:`, {
            success: response.success,
            dataLength: response.data?.length ?? 0,
            error: response.error || null,
          });

          if (response.success && response.data) {
            const candles = response.data.map(fromAngelOneOHLCV);
            allCandles.push(...candles);
            chunkSuccess = true;
          } else {
            // Check if error is retryable (rate limit or timeout)
            const errorMsg = response.error || '';
            const isRetryable = errorMsg.includes('Try After Sometime') ||
                               errorMsg.includes('rate limit') ||
                               errorMsg.includes('exceeding access rate') ||
                               errorMsg.includes('Access denied') ||
                               errorMsg.includes('timeout') ||
                               errorMsg.includes('Request failed');

            if (isRetryable && chunkRetries < MAX_RETRIES - 1) {
              const backoffDelay = Math.pow(2, chunkRetries) * 1000;
              console.warn(`[AngelOne] Historical data chunk failed (${errorMsg}), retry in ${backoffDelay}ms...`);
              await new Promise(resolve => setTimeout(resolve, backoffDelay));
              chunkRetries++;
            } else {
              if (errorMsg) {
                console.warn(`[AngelOne] Skipping chunk after error: ${errorMsg}`);
              }
              chunkSuccess = true; // Skip this chunk and continue
            }
          }
        } catch (error) {
          const chunkElapsed = Date.now() - chunkStart;
          console.error(`[AngelOne] DEBUG: Invoke threw exception after ${chunkElapsed}ms:`, error);
          if (chunkRetries < MAX_RETRIES - 1) {
            const backoffDelay = Math.pow(2, chunkRetries) * 1000;
            console.warn(`[AngelOne] Historical data chunk exception, retry in ${backoffDelay}ms...`, error);
            await new Promise(resolve => setTimeout(resolve, backoffDelay));
            chunkRetries++;
          } else {
            chunkSuccess = true; // Skip this chunk after exhausting retries
          }
        }
      }

      // Advance currentStart to day after currentEnd
      // NOTE: Must create new Date from currentEnd to avoid month-boundary bugs
      // (setDate with day-of-month can wrap backwards across month boundaries)
      currentStart = new Date(currentEnd.getTime());
      currentStart.setDate(currentStart.getDate() + 1);

      // Rate limit delay between chunks (reduced to 300ms for faster loading)
      if (currentStart <= endDate) {
        await new Promise(resolve => setTimeout(resolve, 300));
      }
    }

    if (chunkIndex >= MAX_CHUNKS) {
      console.warn(`[AngelOne] getOHLCVInternal hit MAX_CHUNKS safety limit (${MAX_CHUNKS}). Stopping.`);
    }

    const totalElapsed = Date.now() - overallStart;
    console.log(`[AngelOne] getOHLCVInternal complete: ${allCandles.length} candles in ${totalElapsed}ms (${chunkIndex} chunks)`);

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

    console.log('[AngelOne WS] connectWebSocketInternal called', {
      hasAccessToken: !!this.accessToken,
      hasApiKey: !!this.apiKey,
      hasFeedToken: !!this.feedToken,
      clientCode: this.clientCode,
      userId: this.userId,
    });

    try {
      // Try real WebSocket connection via Rust backend
      await this.connectRealWebSocket();
      console.log('[AngelOne WS] ✓ Real WebSocket connected successfully');
    } catch (error) {
      console.warn('[AngelOne WS] ✗ Real WebSocket FAILED, falling back to polling:', (error as Error).message);
      console.warn('[AngelOne WS] Auth state at failure:', {
        accessToken: this.accessToken ? `${this.accessToken.substring(0, 10)}...` : 'NULL',
        apiKey: this.apiKey ? `${this.apiKey.substring(0, 4)}...` : 'NULL',
        feedToken: this.feedToken ? `${this.feedToken.substring(0, 10)}...` : 'NULL',
      });
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  /**
   * Connect to real Angel One WebSocket via Tauri/Rust backend
   */
  private async connectRealWebSocket(): Promise<void> {
    if (!this.accessToken || !this.apiKey || !this.feedToken) {
      throw new Error(`Missing tokens: accessToken=${!!this.accessToken}, apiKey=${!!this.apiKey}, feedToken=${!!this.feedToken}`);
    }

    // Call Rust backend to connect WebSocket
    console.log('[AngelOne WS] Calling angelone_ws_connect...');
    const result = await invoke<{
      success: boolean;
      data?: boolean;
      error?: string;
    }>('angelone_ws_connect', {
      authToken: this.accessToken,
      apiKey: this.apiKey,
      clientCode: this.clientCode || this.userId || '',
      feedToken: this.feedToken,
    });

    console.log('[AngelOne WS] angelone_ws_connect result:', result);

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

      // Lookup symbol from token map: tick.symbol is "NSE:1333" or "1333"
      const tickToken = tick.symbol.includes(':') ? tick.symbol.split(':')[1] : tick.symbol;
      const symbolInfo = this.tokenSymbolMap.get(tickToken) || this.tokenSymbolMap.get(tick.symbol);

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

      // Lookup symbol from token map: data.symbol is "NSE:1333" or just "1333"
      const tickToken = data.symbol.includes(':') ? data.symbol.split(':')[1] : data.symbol;
      const symbolInfo = this.tokenSymbolMap.get(tickToken) || this.tokenSymbolMap.get(data.symbol);
      const symbol = symbolInfo?.symbol || data.symbol;
      const exchange = symbolInfo?.exchange || ('NSE' as StockExchange);

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
      // Silent error handling
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

    console.log(`[AngelOne] Polling ${this.pollingSymbols.size} symbols via batch quote...`);

    try {
      // Use getMultiQuotes for a single batched API call instead of per-symbol calls
      const symbolList = Array.from(this.pollingSymbols.values()).map(s => ({
        symbol: s.symbol,
        exchange: s.exchange,
      }));

      const results = await this.getMultiQuotes(symbolList);

      for (const result of results) {
        if (result.quote) {
          const tick: TickData = {
            symbol: result.symbol,
            exchange: result.exchange,
            lastPrice: result.quote.lastPrice,
            open: result.quote.open,
            high: result.quote.high,
            low: result.quote.low,
            close: result.quote.close,
            volume: result.quote.volume,
            change: result.quote.change,
            changePercent: result.quote.changePercent,
            timestamp: result.quote.timestamp,
            bid: result.quote.bid,
            bidQty: result.quote.bidQty,
            ask: result.quote.ask,
            askQty: result.quote.askQty,
            mode: 'full',
          };
          this.emitTick(tick);
        } else if (result.error) {
          console.warn(`[AngelOne] Poll: no quote for ${result.symbol}: ${result.error}`);
        }
      }
      console.log(`[AngelOne] Polling complete: ${results.filter(r => r.quote).length}/${results.length} quotes fetched`);
    } catch (error) {
      console.error(`[AngelOne] Batch poll error:`, error);
    }
  }

  async disconnectWebSocket(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    this.wsConnected = false;
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
        const wsExchange = ANGELONE_EXCHANGE_MAP[exchange] || exchange;

        // Rust adapter expects "EXCHANGE:TOKEN" format (e.g. "NSE:2885")
        // Also pass human-readable name so monitoring conditions can match
        await invoke('angelone_ws_subscribe', {
          symbol: `${wsExchange}:${token}`,
          mode: wsMode,
          symbolName: symbol,
        });
      } catch (error) {
        if (!this.usePollingFallback) {
          this.usePollingFallback = true;
          this.startQuotePolling();
        }
      }
    } else {
      console.log(`[AngelOne] Added ${key} to polling (${this.pollingSymbols.size} total)`);
      if (this.usePollingFallback) {
        // Debounce: wait 2 seconds for more symbols to be added before polling
        if (this.pollDebounceTimer) clearTimeout(this.pollDebounceTimer);
        this.pollDebounceTimer = setTimeout(() => {
          this.pollDebounceTimer = null;
          this.pollQuotesSequentially();
        }, 2000);
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
        const wsExchange = ANGELONE_EXCHANGE_MAP[exchange] || exchange;
        await invoke('angelone_ws_unsubscribe', {
          symbol: `${wsExchange}:${token}`,
        });
      } catch (error) {
        // Silent error handling
      }
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
        tradingSymbol: String(item.tradingsymbol || item.symbol || ''), // AngelOne's full trading symbol (e.g., "RELIANCE-EQ")
        exchange: String(item.exchange || 'NSE') as StockExchange,
        name: String(item.name || item.symbol || ''),
        token: String(item.symboltoken || item.token || '0'),
        instrumentToken: String(item.symboltoken || item.token || '0'), // For compatibility
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

  /**
   * Format a Date as "yyyy-MM-dd HH:mm" in IST (UTC+5:30).
   * AngelOne API expects dates in IST, not UTC.
   */
  private formatDateIST(date: Date): string {
    // Convert to IST using toLocaleString with IST timezone
    const formatter = new Intl.DateTimeFormat('en-IN', {
      timeZone: 'Asia/Kolkata',
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      hour12: false
    });

    const parts = formatter.formatToParts(date);
    const values: Record<string, string> = {};
    parts.forEach(part => {
      if (part.type !== 'literal') {
        values[part.type] = part.value;
      }
    });

    // Format as "yyyy-MM-dd HH:mm"
    return `${values.year}-${values.month}-${values.day} ${values.hour}:${values.minute}`;
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
        exchange: String(exchange), // Convert to string
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
