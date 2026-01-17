/**
 * Fyers Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Fyers API v3
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
} from '../../types';
import {
  FYERS_METADATA,
  FYERS_LOGIN_URL,
  FYERS_EXCHANGE_MAP,
  FYERS_WS_URL,
} from './constants';
import {
  toFyersOrderParams,
  toFyersModifyParams,
  toFyersSymbol,
  toFyersInterval,
  fromFyersOrder,
  fromFyersTrade,
  fromFyersPosition,
  fromFyersHolding,
  fromFyersFunds,
  fromFyersQuote,
  fromFyersTickData,
} from './mapper';

/**
 * Fyers broker adapter
 */
export class FyersAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'fyers';
  readonly brokerName = 'Fyers';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = FYERS_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;

  // Tauri event unlisteners
  private eventUnlisteners: UnlistenFn[] = [];

  // REST API polling for quotes (instead of WebSocket)
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange }> = new Map();

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Get OAuth login URL
   */
  getAuthUrl(): string {
    if (!this.apiKey) {
      throw new Error('API key not set. Call setCredentials first.');
    }
    // Fyers requires HTTPS with 127.0.0.1
    const redirectUri = encodeURIComponent('https://127.0.0.1/');
    const responseType = 'code';
    const state = 'fyers_auth';

    return `${FYERS_LOGIN_URL}?client_id=${this.apiKey}&redirect_uri=${redirectUri}&response_type=${responseType}&state=${state}`;
  }

  /**
   * Set API credentials before authentication
   */
  setCredentials(apiKey: string, apiSecret: string): void {
    console.log('[FyersAdapter] setCredentials called with:', {
      apiKey: apiKey ? `${apiKey.substring(0, 10)}...` : 'empty',
      apiSecret: apiSecret ? '***' : 'empty',
      apiKeyLength: apiKey?.length || 0,
      apiSecretLength: apiSecret?.length || 0,
    });
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
    console.log('[FyersAdapter] Credentials stored in memory:', {
      hasApiKey: !!this.apiKey,
      hasApiSecret: !!this.apiSecret,
    });
  }

  /**
   * Authenticate with stored or new credentials
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;

      // Check if this is an auth_code that needs to be exchanged (OAuth callback)
      const authCode = (credentials as any).authCode;
      if (authCode) {
        console.log('[FyersAdapter] Auth code detected, exchanging for access token...');
        return this.handleAuthCallback({ authCode });
      }

      // If access token provided, use it directly (assume already exchanged)
      if (credentials.accessToken) {
        console.log('[FyersAdapter] Access token provided, storing...');
        // For Fyers, if it's a short code, it might be an auth_code
        // Auth codes are typically alphanumeric and ~30 chars
        // Access tokens are longer (typically 60+ chars)
        if (credentials.accessToken.length < 40 && /^[A-Za-z0-9]+$/.test(credentials.accessToken)) {
          console.log('[FyersAdapter] Short token detected, treating as auth_code...');
          return this.handleAuthCallback({ authCode: credentials.accessToken });
        }

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
      }

      // Need to go through OAuth flow
      console.warn('[FyersAdapter] No access token or auth code provided');
      return {
        success: false,
        message: 'Please complete OAuth login',
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
   * Handle OAuth callback
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    try {
      if (!params.authCode && !params.requestToken) {
        return {
          success: false,
          message: 'Authorization code not provided',
          errorCode: 'AUTH_FAILED',
        };
      }

      if (!this.apiKey || !this.apiSecret) {
        return {
          success: false,
          message: 'API credentials not set',
          errorCode: 'CREDENTIALS_MISSING',
        };
      }

      const authCode = params.authCode || params.requestToken || '';

      // Call Rust backend to exchange auth code for access token
      const response = await invoke<{
        success: boolean;
        access_token?: string;
        user_id?: string;
        error?: string;
      }>('fyers_exchange_token', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        authCode,
      });

      if (response.success && response.access_token) {
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
      }

      return {
        success: false,
        message: response.error || 'Token exchange failed',
        errorCode: 'TOKEN_EXCHANGE_FAILED',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Authentication callback failed',
        errorCode: 'AUTH_CALLBACK_FAILED',
      };
    }
  }

  /**
   * Initialize from stored credentials on app startup
   */
  async initFromStorage(): Promise<boolean> {
    try {
      console.log(`[${this.brokerId}] Attempting to load stored credentials...`);
      const credentials = await this.loadCredentials();

      if (!credentials || !credentials.apiKey) {
        console.log(`[${this.brokerId}] No stored credentials found`);
        return false;
      }

      console.log(`[${this.brokerId}] Found stored credentials, restoring session...`);
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;
      this.accessToken = credentials.accessToken || null;

      // If we have an access token, assume valid (Fyers tokens don't expire for 24h)
      // If expired, API calls will fail and user will need to re-auth
      if (this.accessToken) {
        // TODO: Add token validation when Fyers provides an endpoint
        // For now, optimistically restore session
        console.log(`[${this.brokerId}] Access token found, attempting to restore session...`);

        // Test token with a lightweight API call
        try {
          await this.getFundsInternal();
          this._isConnected = true;
          console.log(`[${this.brokerId}] ✓ Session restored successfully`);
          return true;
        } catch (err) {
          console.warn(`[${this.brokerId}] Access token expired or invalid, clearing from storage...`);

          // Clear expired token from database
          await this.storeCredentials({
            apiKey: this.apiKey,
            apiSecret: this.apiSecret || '',
            // No accessToken = clears it
          });

          this.accessToken = null;
          this._isConnected = false;

          console.log(`[${this.brokerId}] Please re-authenticate to continue`);
          return false;
        }
      }

      console.log(`[${this.brokerId}] No access token found, manual authentication required`);
      return false;
    } catch (error) {
      console.error(`[${this.brokerId}] Failed to initialize from storage:`, error);
      return false;
    }
  }

  // ============================================================================
  // Orders - Internal Implementations
  // ============================================================================

  /**
   * Place a new order (internal)
   */
  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    this.ensureAuthenticated();

    try {
      const fyersParams = toFyersOrderParams(params);

      const response = await invoke<{
        success: boolean;
        order_id?: string;
        error?: string;
      }>('fyers_place_order', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
        ...fyersParams,
      });

      if (response.success && response.order_id) {
        return {
          success: true,
          orderId: response.order_id,
          message: 'Order placed successfully',
        };
      }

      return {
        success: false,
        message: response.error || 'Order placement failed',
        errorCode: 'ORDER_PLACEMENT_FAILED',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Failed to place order',
        errorCode: 'ORDER_PLACEMENT_ERROR',
      };
    }
  }

  /**
   * Modify an existing order (internal)
   */
  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    this.ensureAuthenticated();

    if (!params.orderId) {
      return {
        success: false,
        message: 'Order ID is required',
        errorCode: 'MISSING_ORDER_ID',
      };
    }

    try {
      const fyersParams = toFyersModifyParams(params.orderId, params);

      const response = await invoke<{
        success: boolean;
        data?: unknown;
        error?: string;
      }>('fyers_modify_order', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
        ...fyersParams,
      });

      if (response.success) {
        return {
          success: true,
          orderId: params.orderId,
          message: 'Order modified successfully',
        };
      }

      return {
        success: false,
        message: response.error || 'Order modification failed',
        errorCode: 'ORDER_MODIFICATION_FAILED',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Failed to modify order',
        errorCode: 'ORDER_MODIFICATION_ERROR',
      };
    }
  }

  /**
   * Cancel an order (internal)
   */
  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown;
        error?: string;
      }>('fyers_cancel_order', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
        orderId,
      });

      if (response.success) {
        return {
          success: true,
          orderId,
          message: 'Order cancelled successfully',
        };
      }

      return {
        success: false,
        message: response.error || 'Order cancellation failed',
        errorCode: 'ORDER_CANCELLATION_FAILED',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Failed to cancel order',
        errorCode: 'ORDER_CANCELLATION_ERROR',
      };
    }
  }

  /**
   * Get all orders (internal)
   */
  protected async getOrdersInternal(): Promise<Order[]> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('fyers_get_orders', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
      });

      if (response.success && response.data) {
        return response.data.map((order: unknown) =>
          fromFyersOrder(order as Record<string, unknown>)
        );
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch orders:', error);
      return [];
    }
  }

  /**
   * Get all trades (internal)
   */
  protected async getTradeBookInternal(): Promise<Trade[]> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('fyers_get_trade_book', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
      });

      if (response.success && response.data) {
        return response.data.map((trade: unknown) =>
          fromFyersTrade(trade as Record<string, unknown>)
        );
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch trades:', error);
      return [];
    }
  }

  // ============================================================================
  // Portfolio - Internal Implementations
  // ============================================================================

  /**
   * Get all positions (internal)
   */
  protected async getPositionsInternal(): Promise<Position[]> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('fyers_get_positions', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
      });

      if (response.success && response.data) {
        return response.data.map((position: unknown) =>
          fromFyersPosition(position as Record<string, unknown>)
        );
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch positions:', error);
      return [];
    }
  }

  /**
   * Get open position quantity for a specific symbol
   * Used by smart order logic to determine current position
   */
  private async getOpenPositionQuantity(
    symbol: string,
    exchange: StockExchange,
    productType: string
  ): Promise<number> {
    try {
      const positions = await this.getPositionsInternal();
      const position = positions.find(
        p =>
          p.symbol === symbol &&
          p.exchange === exchange &&
          p.productType === productType
      );

      return position ? position.quantity : 0;
    } catch (error) {
      console.error('Failed to get open position:', error);
      return 0;
    }
  }

  /**
   * Get all holdings (internal)
   */
  protected async getHoldingsInternal(): Promise<Holding[]> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('fyers_get_holdings', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
      });

      if (response.success && response.data) {
        return response.data.map((holding: unknown) =>
          fromFyersHolding(holding as Record<string, unknown>)
        );
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch holdings:', error);
      return [];
    }
  }

  // ============================================================================
  // Funds - Internal Implementation
  // ============================================================================

  /**
   * Get account funds/margins (internal)
   * Combines funds data with unrealized P&L from positions (like OpenAlgo)
   */
  protected async getFundsInternal(): Promise<Funds> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown;
        error?: string;
      }>('fyers_get_funds', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
      });

      if (response.success && response.data) {
        const funds = fromFyersFunds(response.data as Record<string, unknown>);

        // Get positions to calculate total unrealized P&L (OpenAlgo pattern)
        try {
          const positions = await this.getPositionsInternal();
          const totalUnrealizedPnl = positions.reduce((sum, pos) => sum + pos.pnl, 0);

          // Update unrealized P&L if we got it from positions
          if (positions.length > 0) {
            funds.unrealizedPnl = totalUnrealizedPnl;
          }
        } catch (err) {
          console.warn('[Fyers] Failed to fetch position P&L for funds:', err);
          // Continue with funds data even if positions fail
        }

        return funds;
      }

      // Return default funds structure
      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        totalBalance: 0,
        currency: 'INR',
      };
    } catch (error) {
      console.error('Failed to fetch funds:', error);
      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        totalBalance: 0,
        currency: 'INR',
      };
    }
  }

  // ============================================================================
  // Market Data - Internal Implementations
  // ============================================================================

  /**
   * Get quote for a single symbol (internal)
   */
  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    this.ensureAuthenticated();

    try {
      // Convert symbol to Fyers format
      const fyersSymbol = toFyersSymbol(symbol, exchange);

      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('fyers_get_quotes', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
        symbols: [fyersSymbol],
      });

      if (response.success && response.data && response.data.length > 0) {
        return fromFyersQuote(response.data[0] as Record<string, unknown>);
      }

      // Return default quote if not found
      return {
        symbol,
        exchange,
        lastPrice: 0,
        open: 0,
        high: 0,
        low: 0,
        close: 0,
        previousClose: 0,
        change: 0,
        changePercent: 0,
        volume: 0,
        bid: 0,
        bidQty: 0,
        ask: 0,
        askQty: 0,
        timestamp: Date.now(),
      };
    } catch (error) {
      console.error('Failed to fetch quote:', error);
      return {
        symbol,
        exchange,
        lastPrice: 0,
        open: 0,
        high: 0,
        low: 0,
        close: 0,
        previousClose: 0,
        change: 0,
        changePercent: 0,
        volume: 0,
        bid: 0,
        bidQty: 0,
        ask: 0,
        askQty: 0,
        timestamp: Date.now(),
      };
    }
  }

  /**
   * Get quotes for multiple symbols (batch processing)
   * Automatically batches into groups of 50 symbols
   */
  protected async getMultipleQuotesInternal(
    symbols: Array<{ symbol: string; exchange: StockExchange }>
  ): Promise<Quote[]> {
    this.ensureAuthenticated();

    const BATCH_SIZE = 50;
    const RATE_LIMIT_DELAY = 100; // 100ms delay between batches
    const allResults: Quote[] = [];

    try {
      // Process in batches of 50
      for (let i = 0; i < symbols.length; i += BATCH_SIZE) {
        const batch = symbols.slice(i, i + BATCH_SIZE);

        // Convert to Fyers format
        const fyersSymbols = batch.map(({ symbol, exchange }) =>
          toFyersSymbol(symbol, exchange)
        );

        const response = await invoke<{
          success: boolean;
          data?: unknown[];
          error?: string;
        }>('fyers_get_quotes', {
          apiKey: this.apiKey!,
          accessToken: this.accessToken!,
          symbols: fyersSymbols,
        });

        if (response.success && response.data) {
          const quotes = response.data.map((item: unknown) =>
            fromFyersQuote(item as Record<string, unknown>)
          );
          allResults.push(...quotes);
        }

        // Rate limit delay between batches
        if (i + BATCH_SIZE < symbols.length) {
          await new Promise(resolve => setTimeout(resolve, RATE_LIMIT_DELAY));
        }
      }

      console.log(`[Fyers] Fetched ${allResults.length} quotes in ${Math.ceil(symbols.length / BATCH_SIZE)} batches`);
      return allResults;
    } catch (error) {
      console.error('Failed to fetch multiple quotes:', error);
      return [];
    }
  }

  /**
   * Get OHLCV data (internal) - Historical data with chunking
   */
  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    this.ensureAuthenticated();

    try {
      const fyersSymbol = toFyersSymbol(symbol, exchange);
      const fyersInterval = toFyersInterval(timeframe);

      const response = await invoke<{
        success: boolean;
        data?: Array<[number, number, number, number, number, number]>; // [timestamp, O, H, L, C, V]
        error?: string;
      }>('fyers_get_history', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
        symbol: fyersSymbol,
        resolution: fyersInterval,
        fromDate: from.toISOString().split('T')[0], // YYYY-MM-DD
        toDate: to.toISOString().split('T')[0],
      });

      console.log('[FyersAdapter] Raw response:', {
        success: response.success,
        dataLength: response.data?.length,
        error: response.error,
        firstCandle: response.data?.[0],
      });

      if (response.success && response.data) {
        const candles = response.data.map(candle => ({
          timestamp: candle[0],
          open: candle[1],
          high: candle[2],
          low: candle[3],
          close: candle[4],
          volume: candle[5],
        }));
        console.log(`[FyersAdapter] ✓ Fetched ${candles.length} candles for ${fyersSymbol}`);
        return candles;
      }

      // API returned error
      const errorMsg = response.error || 'Unknown error from Fyers API';
      console.error(`[FyersAdapter] ❌ Fyers API error: ${errorMsg}`);
      throw new Error(`Fyers API error: ${errorMsg}`);
    } catch (error) {
      console.error('[FyersAdapter] Failed to fetch historical data:', error);
      throw error; // Re-throw so the UI can display the error
    }
  }

  /**
   * Get market depth (internal) - 5-level bid/ask
   */
  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    this.ensureAuthenticated();

    try {
      const fyersSymbol = toFyersSymbol(symbol, exchange);

      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('fyers_get_depth', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
        symbol: fyersSymbol,
      });

      if (response.success && response.data) {
        const depth = response.data;
        const bids = (depth.bids as Array<Record<string, unknown>>) || [];
        const asks = (depth.ask as Array<Record<string, unknown>>) || []; // Fyers uses 'ask' not 'asks'

        return {
          bids: bids.slice(0, 5).map(b => ({
            price: Number(b.price || 0),
            quantity: Number(b.volume || 0),
            orders: Number(b.ord || 0),
          })),
          asks: asks.slice(0, 5).map(a => ({
            price: Number(a.price || 0),
            quantity: Number(a.volume || 0),
            orders: Number(a.ord || 0),
          })),
        };
      }

      return { bids: [], asks: [] };
    } catch (error) {
      console.error('Failed to fetch market depth:', error);
      return { bids: [], asks: [] };
    }
  }

  // ============================================================================
  // WebSocket - Internal Implementations
  // ============================================================================

  /**
   * Connect to WebSocket (internal) - Uses REST API polling instead
   */
  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureAuthenticated();

    try {
      if (!this.apiKey || !this.accessToken) {
        throw new Error('API credentials not set');
      }

      // Start REST API polling (every 2 minutes as requested)
      this.startQuotePolling();

      console.log('Fyers: REST API polling started (2 min interval)');
    } catch (error) {
      console.error('Failed to start Fyers polling:', error);
      throw error;
    }
  }

  /**
   * Start REST API polling for quotes
   */
  private startQuotePolling(): void {
    // Stop existing polling
    this.stopQuotePolling();

    // Poll every 2 minutes (120000ms)
    this.quotePollingInterval = setInterval(async () => {
      await this.pollQuotesSequentially();
    }, 120000);

    // Initial poll
    this.pollQuotesSequentially();
  }

  /**
   * Stop REST API polling
   */
  private stopQuotePolling(): void {
    if (this.quotePollingInterval) {
      clearInterval(this.quotePollingInterval);
      this.quotePollingInterval = null;
    }
  }

  /**
   * Poll quotes sequentially (not parallel) for all subscribed symbols
   */
  private async pollQuotesSequentially(): Promise<void> {
    if (this.pollingSymbols.size === 0) {
      return;
    }

    console.log(`[Fyers] Polling ${this.pollingSymbols.size} symbols sequentially...`);

    // Process each symbol one by one
    for (const [key, { symbol, exchange }] of this.pollingSymbols) {
      try {
        // Fetch quote via REST API
        const quote = await this.getQuoteInternal(symbol, exchange);

        // Convert quote to tick and emit
        const tick: TickData = {
          symbol,
          exchange,
          mode: 'quote',
          lastPrice: quote.lastPrice,
          open: quote.open,
          high: quote.high,
          low: quote.low,
          close: quote.close,
          volume: quote.volume,
          change: quote.change,
          changePercent: quote.changePercent,
          timestamp: quote.timestamp,
        };

        this.emitTick(tick);
      } catch (error) {
        console.error(`[Fyers] Failed to poll ${symbol}:`, error);
      }

      // Small delay between requests to avoid rate limiting
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    console.log(`[Fyers] ✓ Polling complete for ${this.pollingSymbols.size} symbols`);
  }

  /**
   * Subscribe to symbol (internal) - Adds to polling list
   */
  protected async subscribeInternal(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void> {
    try {
      const key = `${exchange}:${symbol}`;
      this.pollingSymbols.set(key, { symbol, exchange });

      console.log(`Fyers: Added ${key} to polling list (${this.pollingSymbols.size} symbols total)`);

      // Trigger immediate poll for this symbol
      this.pollQuotesSequentially();
    } catch (error) {
      console.error('Failed to subscribe:', error);
      throw error;
    }
  }

  /**
   * Unsubscribe from symbol (internal) - Removes from polling list
   */
  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    try {
      const key = `${exchange}:${symbol}`;
      this.pollingSymbols.delete(key);

      console.log(`Fyers: Removed ${key} from polling list (${this.pollingSymbols.size} symbols remaining)`);
    } catch (error) {
      console.error('Failed to unsubscribe:', error);
      throw error;
    }
  }

  /**
   * Cleanup polling on logout
   */
  async logout(): Promise<void> {
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    await super.logout();
  }

  // ============================================================================
  // Bulk Operations - Internal Implementations
  // ============================================================================

  /**
   * Cancel all orders (internal)
   */
  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    const orders = await this.getOrdersInternal();
    const openOrders = orders.filter(o => o.status === 'OPEN' || o.status === 'PENDING');

    const results = await Promise.all(
      openOrders.map(async (order) => {
        try {
          const result = await this.cancelOrderInternal(order.orderId);
          return { success: result.success, orderId: order.orderId };
        } catch (error) {
          return { success: false, orderId: order.orderId, error: String(error) };
        }
      })
    );

    const successful = results.filter(r => r.success).length;
    const failed = results.filter(r => !r.success).length;

    return {
      success: failed === 0,
      totalCount: results.length,
      successCount: successful,
      failedCount: failed,
      results,
    };
  }

  /**
   * Close all positions (internal)
   * Iterates through all positions and places exit orders
   */
  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    this.ensureAuthenticated();

    try {
      const positions = await this.getPositionsInternal();

      // Filter out positions with 0 quantity
      const activePositions = positions.filter(p => p.quantity !== 0);

      if (activePositions.length === 0) {
        return {
          success: true,
          totalCount: 0,
          successCount: 0,
          failedCount: 0,
          results: [],
        };
      }

      const results = await Promise.all(
        activePositions.map(async (position) => {
          try {
            // Determine exit side (reverse of current position)
            const exitSide: 'BUY' | 'SELL' = position.quantity > 0 ? 'SELL' : 'BUY';
            const exitQuantity = Math.abs(position.quantity);

            const orderParams: OrderParams = {
              symbol: position.symbol,
              exchange: position.exchange,
              side: exitSide,
              quantity: exitQuantity,
              orderType: 'MARKET',
              productType: position.productType,
              validity: 'DAY',
            };

            const result = await this.placeOrderInternal(orderParams);
            return {
              success: result.success,
              symbol: position.symbol,
              orderId: result.orderId,
              error: result.message
            };
          } catch (error) {
            return {
              success: false,
              symbol: position.symbol,
              error: String(error)
            };
          }
        })
      );

      const successful = results.filter(r => r.success).length;
      const failed = results.filter(r => !r.success).length;

      console.log(`[Fyers] Close All Positions: ${successful} success, ${failed} failed`);

      return {
        success: failed === 0,
        totalCount: results.length,
        successCount: successful,
        failedCount: failed,
        results,
      };
    } catch (error) {
      console.error('Failed to close all positions:', error);
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }
  }

  /**
   * Place smart order (internal) - Position-aware order placement
   * Automatically calculates required quantity based on target position size
   */
  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    this.ensureAuthenticated();

    try {
      // Get current position quantity
      const currentPosition = await this.getOpenPositionQuantity(
        params.symbol,
        params.exchange,
        params.productType
      );

      const targetPosition = params.positionSize || 0;

      // Calculate what order is needed
      let action: 'BUY' | 'SELL';
      let quantity: number;

      if (targetPosition > currentPosition) {
        // Need to buy more
        action = 'BUY';
        quantity = targetPosition - currentPosition;
      } else if (targetPosition < currentPosition) {
        // Need to sell/reduce
        action = 'SELL';
        quantity = currentPosition - targetPosition;
      } else {
        // Already at target position
        return {
          success: true,
          message: 'Already at target position',
        };
      }

      // Place the calculated order
      const orderParams: OrderParams = {
        symbol: params.symbol,
        exchange: params.exchange,
        side: action,
        quantity,
        orderType: params.orderType || 'MARKET',
        productType: params.productType,
        price: params.price,
        triggerPrice: params.triggerPrice,
        validity: params.validity || 'DAY',
      };

      console.log(`[Fyers Smart Order] Current: ${currentPosition}, Target: ${targetPosition}, Action: ${action} ${quantity}`);

      return this.placeOrderInternal(orderParams);
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Smart order failed',
        errorCode: 'SMART_ORDER_FAILED',
      };
    }
  }

  /**
   * Calculate margin (internal) - Margin calculator
   * POST to /api/v3/multiorder/margin with order list
   */
  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    this.ensureAuthenticated();

    try {
      // Transform orders to Fyers format
      const fyersOrders = orders.map(order => toFyersOrderParams(order));

      const response = await invoke<{
        success: boolean;
        data?: {
          margin_total?: number;
          margin_new_order?: number;
          margin_avail?: number;
        };
        error?: string;
      }>('fyers_calculate_margin', {
        apiKey: this.apiKey!,
        accessToken: this.accessToken!,
        orders: fyersOrders,
      });

      if (response.success && response.data) {
        const marginData = response.data;

        // Fyers returns:
        // - margin_total: Approximate margin required
        // - margin_new_order: Total margin including existing positions
        // - margin_avail: Available margin
        //
        // Note: Fyers doesn't provide SPAN/Exposure breakdown

        return {
          totalMargin: marginData.margin_new_order || marginData.margin_total || 0,
          initialMargin: marginData.margin_total || 0,
        };
      }

      return {
        totalMargin: 0,
        initialMargin: 0,
      };
    } catch (error) {
      console.error('Failed to calculate margin:', error);
      return {
        totalMargin: 0,
        initialMargin: 0,
      };
    }
  }

  // ============================================================================
  // Helper Methods
  // ============================================================================

  /**
   * Ensure user is authenticated before making API calls
   */
  private ensureAuthenticated(): void {
    if (!this.accessToken || !this.apiKey) {
      throw new Error('Not authenticated. Please login first.');
    }
  }

  /**
   * Store credentials securely
   */
  protected async storeCredentials(credentials: BrokerCredentials): Promise<void> {
    try {
      await invoke('store_indian_broker_credentials', {
        brokerId: this.brokerId,
        credentials: {
          api_key: credentials.apiKey,
          api_secret: credentials.apiSecret || '',
          access_token: credentials.accessToken || '',
          user_id: credentials.userId || '',
        },
      });
    } catch (error) {
      console.error('Failed to store credentials:', error);
    }
  }

  /**
   * Clear stored credentials
   */
  private async clearStoredCredentials(): Promise<void> {
    try {
      await invoke('delete_indian_broker_credentials', {
        brokerId: this.brokerId,
      });
    } catch (error) {
      console.error('Failed to clear credentials:', error);
    }
  }
}
