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

  private apiKey: string | null = null; // neo-fin-key (consumer key)
  private mobileNumber: string | null = null;
  private ucc: string | null = null; // User Client Code
  private mpin: string | null = null;

  // Intermediate auth state (between TOTP and MPIN)
  private viewToken: string | null = null;
  private viewSid: string | null = null;

  // REST API polling for quotes
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange }> = new Map();

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Set API credentials before authentication
   *
   * For Kotak:
   * - apiKey = neo-fin-key (consumer key)
   * - apiSecret = mpin
   * - userId = UCC (User Client Code)
   * - Additional: mobileNumber required
   */
  setCredentials(apiKey: string, apiSecret: string, userId?: string): void {
    this.apiKey = apiKey;
    this.mpin = apiSecret;
    this.ucc = userId || null;
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
    if (!this.apiKey || !this.mobileNumber || !this.ucc) {
      return {
        success: false,
        message: 'Missing credentials. Call setCredentials() and setMobileNumber() first.',
      };
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
        consumerKey: this.apiKey,
        mobileNumber: this.mobileNumber,
        ucc: this.ucc,
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
      this.apiKey = credentials.apiKey;
      this.mpin = credentials.apiSecret || null;
      this.ucc = credentials.userId || null;

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

      this.apiKey = credentials.apiKey;
      this.mpin = credentials.apiSecret || null;
      this.accessToken = credentials.accessToken || null;
      this.ucc = credentials.userId || null;
      this.userId = credentials.userId || null;

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
  // WebSocket / Polling
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureConnected();

    try {
      this.startQuotePolling();
      this.wsConnected = true;
      console.log('[Kotak] REST API polling started');
    } catch (error) {
      console.error('[Kotak] Failed to start polling:', error);
      throw error;
    }
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();

    // Poll every 30 seconds
    this.quotePollingInterval = setInterval(async () => {
      await this.pollQuotes();
    }, 30000);

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

    console.log(`[Kotak] Polling ${this.pollingSymbols.size} symbols...`);

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
        console.error(`[Kotak] Poll error for ${symbol}:`, error);
      }

      // Small delay between requests
      await new Promise(resolve => setTimeout(resolve, 200));
    }

    console.log(`[Kotak] Polling complete`);
  }

  async disconnectWebSocket(): Promise<void> {
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.wsConnected = false;
    console.log('[Kotak] Polling stopped');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.set(key, { symbol, exchange });
    console.log(`[Kotak] Added ${key} to polling (${this.pollingSymbols.size} total)`);

    // Trigger immediate poll
    this.pollQuotes();
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);
    console.log(`[Kotak] Removed ${key} from polling`);
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
