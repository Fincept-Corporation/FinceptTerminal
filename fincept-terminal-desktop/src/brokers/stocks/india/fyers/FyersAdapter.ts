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
  Instrument,
} from '../../types';
import {
  FYERS_METADATA,
  FYERS_LOGIN_URL,
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

  // WebSocket state (protected to match base class)
  protected override wsConnected: boolean = false;
  protected wsSubscriptions: Set<string> = new Set();

  // Tauri event unlisteners for WebSocket
  protected wsUnlisteners: Array<() => void> = [];

  // Hybrid data strategy: WebSocket during market hours, polling off-hours
  private pollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange; mode: SubscriptionMode }> = new Map();
  // Off-market polling interval: 30 minutes (data won't change when market is closed)
  private readonly POLLING_INTERVAL_MS = 30 * 60 * 1000; // 30 minutes for off-market polling
  private usePollingMode: boolean = false;
  private marketHoursCheckInterval: NodeJS.Timeout | null = null;

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

          // Download master contract for WebSocket symbol lookup (uses correct public Fyers URL)
          try {
            console.log(`[${this.brokerId}] Downloading master contract for WebSocket...`);
            await invoke('download_fyers_master_contract');
            console.log(`[${this.brokerId}] ✓ Master contract ready`);
          } catch (mcErr) {
            console.warn(`[${this.brokerId}] Master contract download failed:`, mcErr);
          }

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
   * Fyers API rate limit: ~10 requests per second
   */
  protected async getMultipleQuotesInternal(
    symbols: Array<{ symbol: string; exchange: StockExchange }>
  ): Promise<Quote[]> {
    this.ensureAuthenticated();

    const BATCH_SIZE = 50; // Fyers supports up to 50 symbols per request
    const RATE_LIMIT_DELAY = 1000; // 1 second delay between batches (safe for Fyers rate limits)
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
  // WebSocket - Internal Implementations (Hybrid: WebSocket + Polling)
  // ============================================================================

  /**
   * Check if Indian stock market is currently open
   * NSE/BSE: Monday-Friday, 9:15 AM - 3:30 PM IST
   */
  private isMarketOpen(): boolean {
    // Get current time in IST using Intl.DateTimeFormat
    const now = new Date();
    const istFormatter = new Intl.DateTimeFormat('en-US', {
      timeZone: 'Asia/Kolkata',
      hour: 'numeric',
      minute: 'numeric',
      hour12: false,
      weekday: 'short',
    });

    const parts = istFormatter.formatToParts(now);
    const weekday = parts.find(p => p.type === 'weekday')?.value || '';
    const hour = parseInt(parts.find(p => p.type === 'hour')?.value || '0', 10);
    const minute = parseInt(parts.find(p => p.type === 'minute')?.value || '0', 10);

    const timeInMinutes = hour * 60 + minute;

    // Market hours: 9:15 AM (555 min) to 3:30 PM (930 min), Mon-Fri
    const marketOpen = 9 * 60 + 15;  // 9:15 AM = 555 minutes
    const marketClose = 15 * 60 + 30; // 3:30 PM = 930 minutes

    const weekdays = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri'];
    const isWeekday = weekdays.includes(weekday);
    const isWithinHours = timeInMinutes >= marketOpen && timeInMinutes <= marketClose;

    console.log(`[Fyers] Market check: ${weekday} ${hour}:${minute.toString().padStart(2, '0')} IST | open=${isWeekday && isWithinHours}`);

    return isWeekday && isWithinHours;
  }

  /**
   * Connect to Fyers WebSocket for real-time market data
   * Uses hybrid approach: WebSocket during market hours, polling off-hours
   */
  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureAuthenticated();

    try {
      if (!this.apiKey || !this.accessToken) {
        throw new Error('API credentials not set');
      }

      // Check if market is open to decide connection strategy
      const marketOpen = this.isMarketOpen();

      if (marketOpen) {
        // Market is open - try WebSocket first
        console.log('[Fyers] Market is OPEN - attempting WebSocket connection...');
        await this.connectWebSocketDirect();
      } else {
        // Market is closed - use polling mode
        console.log('[Fyers] Market is CLOSED - using polling mode for data');
        this.usePollingMode = true;
        this.startPolling();
      }

      // Set up periodic market hours check to switch modes
      this.startMarketHoursMonitor();

    } catch (error) {
      console.error('[Fyers] Connection failed, falling back to polling:', error);
      // Fallback to polling on any error
      this.usePollingMode = true;
      this.startPolling();
    }
  }

  /**
   * Direct WebSocket connection via Tauri backend
   */
  private async connectWebSocketDirect(): Promise<void> {
    const wsAccessToken = `${this.apiKey}:${this.accessToken}`;

    console.log('[Fyers WS] Connecting...', {
      hasApiKey: !!this.apiKey,
      apiKeyLength: this.apiKey?.length || 0,
      accessTokenLength: this.accessToken?.length || 0,
      wsAccessTokenLength: wsAccessToken.length,
    });

    try {
      const response = await invoke<{
        success: boolean;
        data?: boolean;
        error?: string;
      }>('fyers_ws_connect', {
        app: undefined,
        apiKey: this.apiKey,
        accessToken: wsAccessToken,
      });

      console.log('[Fyers WS] Response:', response);

      if (!response.success) {
        const errorMsg = response.error || 'WebSocket connection failed (no error details)';
        console.error('[Fyers WS] ✗ Connection failed:', errorMsg);
        throw new Error(errorMsg);
      }

      this.wsConnected = true;
      this.usePollingMode = false;
      console.log('[Fyers WS] ✓ Connected successfully');

      await this.setupWebSocketListeners();
      await this.resubscribeViaWebSocket();
    } catch (invokeError) {
      console.error('[Fyers WS] ✗ Tauri invoke error:', invokeError);
      throw invokeError;
    }
  }

  /**
   * Start periodic market hours monitoring
   * Switches between WebSocket and polling based on market status
   */
  private startMarketHoursMonitor(): void {
    // Clear existing monitor
    if (this.marketHoursCheckInterval) {
      clearInterval(this.marketHoursCheckInterval);
    }

    // Check every 5 minutes
    this.marketHoursCheckInterval = setInterval(async () => {
      const marketOpen = this.isMarketOpen();

      if (marketOpen && this.usePollingMode) {
        // Market just opened - switch to WebSocket
        console.log('[Fyers] Market opened - switching to WebSocket');
        this.stopPolling();
        try {
          await this.connectWebSocketDirect();
        } catch (err) {
          console.warn('[Fyers] WebSocket failed, continuing with polling:', err);
          this.startPolling();
        }
      } else if (!marketOpen && !this.usePollingMode && this.wsConnected) {
        // Market just closed - switch to polling
        console.log('[Fyers] Market closed - switching to polling');
        await this.disconnectWebSocket();
        this.usePollingMode = true;
        this.startPolling();
      }
    }, 5 * 60 * 1000); // Check every 5 minutes
  }

  /**
   * Resubscribe all polling symbols via WebSocket (BATCH mode for speed!)
   */
  private async resubscribeViaWebSocket(): Promise<void> {
    if (this.pollingSymbols.size === 0) {
      console.log('[Fyers] No symbols to resubscribe');
      return;
    }

    // Group symbols by mode
    const depthSymbols: string[] = [];
    const quoteSymbols: string[] = [];

    for (const [fyersSymbol, data] of this.pollingSymbols) {
      if (data.mode === 'full') {
        depthSymbols.push(fyersSymbol);
      } else {
        quoteSymbols.push(fyersSymbol);
      }
    }

    // Batch subscribe depth symbols
    if (depthSymbols.length > 0) {
      console.log(`[Fyers] Batch subscribing ${depthSymbols.length} depth symbols...`);
      try {
        const response = await invoke<{ success: boolean; error?: string }>('fyers_ws_subscribe_batch', {
          symbols: depthSymbols,
          mode: 'depth',
        });
        if (response.success) {
          depthSymbols.forEach(s => this.wsSubscriptions.add(s));
          console.log(`[Fyers] ✓ Batch subscribed ${depthSymbols.length} depth symbols`);
        } else {
          console.warn(`[Fyers] Batch depth subscription failed: ${response.error}`);
        }
      } catch (err) {
        console.error('[Fyers] Batch depth subscription error:', err);
      }
    }

    // Batch subscribe quote symbols
    if (quoteSymbols.length > 0) {
      console.log(`[Fyers] Batch subscribing ${quoteSymbols.length} quote symbols...`);
      try {
        const response = await invoke<{ success: boolean; error?: string }>('fyers_ws_subscribe_batch', {
          symbols: quoteSymbols,
          mode: 'quote',
        });
        if (response.success) {
          quoteSymbols.forEach(s => this.wsSubscriptions.add(s));
          console.log(`[Fyers] ✓ Batch subscribed ${quoteSymbols.length} quote symbols`);
        } else {
          console.warn(`[Fyers] Batch quote subscription failed: ${response.error}`);
        }
      } catch (err) {
        console.error('[Fyers] Batch quote subscription error:', err);
      }
    }
  }

  /**
   * Set up Tauri event listeners for WebSocket messages
   */
  private async setupWebSocketListeners(): Promise<void> {
    // Clean up existing listeners
    this.cleanupWebSocketListeners();

    // Listen for ticker data
    const tickerUnlisten = await listen<{
      provider: string;
      symbol: string;
      price: number;
      bid?: number;
      ask?: number;
      bid_size?: number;
      ask_size?: number;
      volume?: number;
      high?: number;
      low?: number;
      open?: number;
      close?: number;
      change?: number;
      change_percent?: number;
      timestamp: number;
    }>('fyers_ticker', (event) => {
      const data = event.payload;

      // Extract symbol and exchange from Fyers format (e.g., "NSE:RELIANCE-EQ")
      const [exchange, symbolPart] = data.symbol.split(':');
      const symbol = symbolPart?.replace('-EQ', '').replace('-INDEX', '') || data.symbol;

      console.log('[Fyers] Ticker update:', symbol, 'LTP:', data.price, 'change:', data.change_percent?.toFixed(2) + '%');

      const tick: TickData = {
        symbol,
        exchange: (exchange as StockExchange) || 'NSE',
        mode: 'quote',
        lastPrice: data.price,
        open: data.open,
        high: data.high,
        low: data.low,
        close: data.close,
        volume: data.volume,
        change: data.change,
        changePercent: data.change_percent,
        bid: data.bid,
        ask: data.ask,
        bidQty: data.bid_size,
        askQty: data.ask_size,
        timestamp: data.timestamp,
      };

      this.emitTick(tick);
    });

    this.wsUnlisteners.push(tickerUnlisten);

    // Listen for orderbook/depth data
    const orderbookUnlisten = await listen<{
      provider: string;
      symbol: string;
      bids: Array<{ price: number; quantity: number; count?: number }>;
      asks: Array<{ price: number; quantity: number; count?: number }>;
      timestamp: number;
      is_snapshot?: boolean;
    }>('fyers_orderbook', (event) => {
      const data = event.payload;

      // Parse symbol from Fyers format (e.g., "NSE:RELIANCE-EQ" -> "RELIANCE")
      const [exchange, symbolPart] = data.symbol.split(':');
      const symbol = symbolPart?.replace('-EQ', '').replace('-INDEX', '') || data.symbol;

      console.log('[Fyers] Depth update:', symbol, 'bids:', data.bids?.length, 'asks:', data.asks?.length);

      // Get best bid/ask from depth
      const bestBid = data.bids?.[0];
      const bestAsk = data.asks?.[0];

      // Create tick data with depth information
      const tick: TickData = {
        symbol,
        exchange: (exchange as StockExchange) || 'NSE',
        mode: 'full',
        lastPrice: bestBid?.price || 0, // Use bid as approximate LTP
        bid: bestBid?.price,
        ask: bestAsk?.price,
        bidQty: bestBid?.quantity,
        askQty: bestAsk?.quantity,
        timestamp: data.timestamp,
        // Include full depth data
        depth: {
          bids: data.bids?.map((b) => ({
            price: b.price,
            quantity: b.quantity,
            orders: b.count || 0,
          })) || [],
          asks: data.asks?.map((a) => ({
            price: a.price,
            quantity: a.quantity,
            orders: a.count || 0,
          })) || [],
        },
      };

      this.emitTick(tick);

      // Also emit depth data separately for UI components that listen for depth updates
      if (tick.depth) {
        this.emitDepth({
          symbol,
          exchange: (exchange as StockExchange) || 'NSE',
          bids: tick.depth.bids,
          asks: tick.depth.asks,
        });
      }
    });

    this.wsUnlisteners.push(orderbookUnlisten);

    // Listen for status updates
    const statusUnlisten = await listen<{
      provider: string;
      status: string;
      message: string;
      timestamp: number;
    }>('fyers_status', (event) => {
      const data = event.payload;
      console.log(`[Fyers] Status: ${data.status} - ${data.message}`);

      if (data.status === 'disconnected' || data.status === 'error') {
        this.wsConnected = false;
      }
    });

    this.wsUnlisteners.push(statusUnlisten);

    console.log('[Fyers] WebSocket event listeners set up');
  }

  /**
   * Clean up WebSocket event listeners
   */
  private cleanupWebSocketListeners(): void {
    for (const unlisten of this.wsUnlisteners) {
      unlisten();
    }
    this.wsUnlisteners = [];
  }

  // ============================================================================
  // Polling Mode - For Off-Market Hours
  // ============================================================================

  /**
   * Start REST API polling for subscribed symbols
   */
  private startPolling(): void {
    if (this.pollingInterval) {
      return; // Already polling
    }

    console.log(`[Fyers] Polling mode started (${this.POLLING_INTERVAL_MS / 1000}s interval)`);

    this.pollingInterval = setInterval(async () => {
      await this.pollSubscribedSymbols();
    }, this.POLLING_INTERVAL_MS);

    // Do an immediate poll
    this.pollSubscribedSymbols();
  }

  /**
   * Stop REST API polling
   */
  private stopPolling(): void {
    if (this.pollingInterval) {
      clearInterval(this.pollingInterval);
      this.pollingInterval = null;
      console.log('[Fyers] Polling stopped');
    }
  }

  /**
   * Poll all subscribed symbols via REST API
   */
  private async pollSubscribedSymbols(): Promise<void> {
    if (this.pollingSymbols.size === 0) {
      return;
    }

    try {
      // Get all symbols to poll
      const fyersSymbols: string[] = [];

      for (const [fyersSymbol] of this.pollingSymbols) {
        fyersSymbols.push(fyersSymbol);
      }

      // Fetch quotes in batch
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
        // Emit tick data for each quote
        for (const quoteData of response.data) {
          const quote = fromFyersQuote(quoteData as Record<string, unknown>);

          // Skip quotes with errors silently
          if (quote.error) continue;

          const tick: TickData = {
            symbol: quote.symbol,
            exchange: quote.exchange,
            mode: 'quote',
            lastPrice: quote.lastPrice,
            open: quote.open,
            high: quote.high,
            low: quote.low,
            close: quote.close,
            volume: quote.volume,
            change: quote.change,
            changePercent: quote.changePercent,
            bid: quote.bid,
            ask: quote.ask,
            bidQty: quote.bidQty,
            askQty: quote.askQty,
            timestamp: quote.timestamp,
          };

          this.emitTick(tick);
        }
      } else if (!response.success) {
        console.error('[Fyers] Poll failed:', response.error);
      }
    } catch (error) {
      console.error('[Fyers] Polling error:', error);
    }
  }

  // ============================================================================
  // Subscribe/Unsubscribe - Hybrid Mode (Override base class)
  // ============================================================================

  /**
   * Override base class subscribe to implement hybrid WebSocket/Polling
   */
  async subscribe(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void> {
    console.log(`[Fyers] >>> SUBSCRIBE: ${symbol}@${exchange} mode=${mode} | ws=${this.wsConnected} poll=${this.usePollingMode}`);
    await this.subscribeInternal(symbol, exchange, mode);
  }

  /**
   * FAST batch subscribe - subscribes to multiple symbols at once!
   * Use this for watchlists or any batch of symbols for much faster subscription.
   */
  async subscribeBatch(symbols: Array<{ symbol: string; exchange: StockExchange }>, mode: SubscriptionMode = 'quote'): Promise<void> {
    if (symbols.length === 0) return;

    console.log(`[Fyers] >>> BATCH SUBSCRIBE: ${symbols.length} symbols, mode=${mode}`);
    const startTime = Date.now();

    // Convert to Fyers format
    const fyersSymbols = symbols.map(({ symbol, exchange }) => {
      const fyersSymbol = toFyersSymbol(symbol, exchange);
      this.pollingSymbols.set(fyersSymbol, { symbol, exchange, mode });
      return fyersSymbol;
    });

    // Initialize connection if needed
    if (!this.wsConnected && !this.usePollingMode && !this.pollingInterval) {
      const marketOpen = this.isMarketOpen();
      console.log(`[Fyers] Init strategy: market=${marketOpen ? 'OPEN' : 'CLOSED'}`);

      if (marketOpen) {
        try {
          await this.connectWebSocketDirect();
          this.startMarketHoursMonitor();
        } catch (wsError) {
          console.error('[Fyers] WS failed, fallback to polling:', wsError);
          this.usePollingMode = true;
          this.startPolling();
        }
      } else {
        this.usePollingMode = true;
        this.startPolling();
        this.startMarketHoursMonitor();
      }
    }

    // Use batch subscribe if WebSocket is connected
    if (this.wsConnected && !this.usePollingMode) {
      try {
        const wsMode = mode === 'full' ? 'depth' : 'quote';
        const response = await invoke<{ success: boolean; error?: string }>('fyers_ws_subscribe_batch', {
          symbols: fyersSymbols,
          mode: wsMode,
        });

        if (response.success) {
          fyersSymbols.forEach(s => this.wsSubscriptions.add(s));
          console.log(`[Fyers] ✓ Batch WS subscribed to ${fyersSymbols.length} symbols in ${Date.now() - startTime}ms`);
          return;
        } else {
          console.warn(`[Fyers] Batch WS subscribe failed: ${response.error}, falling back to polling`);
        }
      } catch (err) {
        console.error('[Fyers] Batch WS subscribe error:', err);
      }
    }

    // Fallback: fetch initial quotes via REST
    console.log(`[Fyers] Fetching ${fyersSymbols.length} quotes via REST (polling mode)`);
    await this.fetchBatchQuotes(fyersSymbols);

    if (!this.pollingInterval) {
      this.usePollingMode = true;
      this.startPolling();
    }

    console.log(`[Fyers] ✓ Batch subscribe complete in ${Date.now() - startTime}ms`);
  }

  // Debounce timer for batched initial fetch
  private pendingInitialFetch: NodeJS.Timeout | null = null;
  private pendingSymbols: Set<string> = new Set();

  // Track symbols with errors (to avoid repeated error logging)
  private errorSymbols: Set<string> = new Set();

  /**
   * Subscribe to symbol - uses WebSocket or polling based on market hours
   */
  protected async subscribeInternal(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void> {
    const fyersSymbol = toFyersSymbol(symbol, exchange);
    this.pollingSymbols.set(fyersSymbol, { symbol, exchange, mode });

    try {
      // Initialize connection strategy on first subscription
      if (!this.wsConnected && !this.usePollingMode && !this.pollingInterval) {
        const marketOpen = this.isMarketOpen();
        console.log(`[Fyers] Init strategy: market=${marketOpen ? 'OPEN' : 'CLOSED'}`);

        if (marketOpen) {
          try {
            await this.connectWebSocketDirect();
            this.startMarketHoursMonitor();
          } catch (wsError) {
            console.error('[Fyers] WS failed, fallback to polling:', wsError);
            this.usePollingMode = true;
            this.startPolling();
          }
        } else {
          this.usePollingMode = true;
          this.startPolling();
          this.startMarketHoursMonitor();
        }
      }

      // Handle subscription based on current mode
      if (this.usePollingMode) {
        if (!this.pollingInterval) this.startPolling();
        this.queueInitialFetch(fyersSymbol);
      } else if (this.wsConnected) {
        const channel = mode === 'full' ? 'depth' : 'SymbolUpdate';
        const response = await invoke<{ success: boolean; error?: string }>('fyers_ws_subscribe', {
          symbol: fyersSymbol,
          mode: channel,
        });
        if (!response.success) {
          console.warn(`[Fyers] WS subscribe failed for ${fyersSymbol}: ${response.error}`);
          this.queueInitialFetch(fyersSymbol);
        }
        this.wsSubscriptions.add(fyersSymbol);
      } else {
        this.queueInitialFetch(fyersSymbol);
        if (!this.pollingInterval) {
          this.usePollingMode = true;
          this.startPolling();
        }
      }
    } catch (error) {
      console.error(`[Fyers] Subscribe error for ${fyersSymbol}:`, error);
      this.queueInitialFetch(fyersSymbol);
    }
  }

  /**
   * Queue symbol for batched initial fetch
   * Uses debouncing to collect multiple symbols and fetch them in one batch
   */
  private queueInitialFetch(fyersSymbol: string): void {
    this.pendingSymbols.add(fyersSymbol);

    // Clear existing timer
    if (this.pendingInitialFetch) {
      clearTimeout(this.pendingInitialFetch);
    }

    // Set new timer - wait 200ms for more symbols to be added
    this.pendingInitialFetch = setTimeout(async () => {
      const symbols = Array.from(this.pendingSymbols);
      this.pendingSymbols.clear();
      this.pendingInitialFetch = null;

      if (symbols.length > 0) {
        console.log(`[Fyers] Batch fetching ${symbols.length} symbols for initial data`);
        await this.fetchBatchQuotes(symbols);
      }
    }, 200);
  }

  /**
   * Fetch quotes for multiple symbols in one batch request
   */
  private async fetchBatchQuotes(fyersSymbols: string[]): Promise<void> {
    try {
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
        let successCount = 0;
        for (const quoteData of response.data) {
          const quote = fromFyersQuote(quoteData as Record<string, unknown>);

          // Skip quotes with errors silently
          if (quote.error) continue;

          const tick: TickData = {
            symbol: quote.symbol,
            exchange: quote.exchange,
            mode: 'quote',
            lastPrice: quote.lastPrice,
            open: quote.open,
            high: quote.high,
            low: quote.low,
            close: quote.close,
            volume: quote.volume,
            change: quote.change,
            changePercent: quote.changePercent,
            bid: quote.bid,
            ask: quote.ask,
            bidQty: quote.bidQty,
            askQty: quote.askQty,
            timestamp: quote.timestamp,
          };

          this.emitTick(tick);
          successCount++;
        }
        console.log(`[Fyers] Batch fetch: ${successCount}/${fyersSymbols.length} quotes`);
      } else {
        console.error('[Fyers] Batch fetch failed:', response.error);
      }
    } catch (error) {
      console.error('[Fyers] Batch fetch error:', error);
    }
  }

  /**
   * Fetch a single quote and emit as tick data
   * Used for immediate data when subscribing
   */
  private async fetchAndEmitQuote(fyersSymbol: string, symbol: string, exchange: StockExchange): Promise<void> {
    try {
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
        const quote = fromFyersQuote(response.data[0] as Record<string, unknown>);

        const tick: TickData = {
          symbol: quote.symbol || symbol,
          exchange: quote.exchange || exchange,
          mode: 'quote',
          lastPrice: quote.lastPrice,
          open: quote.open,
          high: quote.high,
          low: quote.low,
          close: quote.close,
          volume: quote.volume,
          change: quote.change,
          changePercent: quote.changePercent,
          bid: quote.bid,
          ask: quote.ask,
          bidQty: quote.bidQty,
          askQty: quote.askQty,
          timestamp: quote.timestamp,
        };

        this.emitTick(tick);
        console.log(`[Fyers] Fetched quote for ${fyersSymbol}: ${tick.lastPrice}`);
      }
    } catch (error) {
      console.error(`[Fyers] Failed to fetch quote for ${fyersSymbol}:`, error);
    }
  }

  /**
   * Unsubscribe from symbol
   */
  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    try {
      const fyersSymbol = toFyersSymbol(symbol, exchange);

      // Remove from polling map
      this.pollingSymbols.delete(fyersSymbol);

      // If WebSocket connected, unsubscribe there too
      if (this.wsConnected && this.wsSubscriptions.has(fyersSymbol)) {
        const response = await invoke<{
          success: boolean;
          data?: boolean;
          error?: string;
        }>('fyers_ws_unsubscribe', {
          symbol: fyersSymbol,
        });

        if (!response.success) {
          console.warn(`[Fyers] WebSocket unsubscribe failed: ${response.error}`);
        }

        this.wsSubscriptions.delete(fyersSymbol);
      }

      console.log(`[Fyers] Unsubscribed from ${fyersSymbol} (${this.pollingSymbols.size} remaining)`);

      // Stop polling if no more symbols
      if (this.pollingSymbols.size === 0 && this.pollingInterval) {
        this.stopPolling();
      }

    } catch (error) {
      console.error('[Fyers] Unsubscribe error:', error);
    }
  }

  /**
   * Disconnect WebSocket (override base class method)
   */
  async disconnectWebSocket(): Promise<void> {
    try {
      await invoke('fyers_ws_disconnect');
      this.wsConnected = false;
      this.wsSubscriptions.clear();
      this.cleanupWebSocketListeners();
      console.log('[Fyers] WebSocket disconnected');
    } catch (error) {
      console.error('[Fyers] WebSocket disconnect error:', error);
    }
  }

  /**
   * Check if WebSocket is connected
   */
  async isWebSocketConnected(): Promise<boolean> {
    try {
      const connected = await invoke<boolean>('fyers_ws_is_connected');
      this.wsConnected = connected;
      return connected;
    } catch {
      return false;
    }
  }

  /**
   * Cleanup on logout
   */
  async logout(): Promise<void> {
    // Stop all data feeds
    this.stopPolling();
    if (this.marketHoursCheckInterval) {
      clearInterval(this.marketHoursCheckInterval);
      this.marketHoursCheckInterval = null;
    }
    await this.disconnectWebSocket();
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

  // ensureAuthenticated() inherited from BaseStockBrokerAdapter

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

  // ============================================================================
  // Symbol Search & Master Contract
  // ============================================================================

  /**
   * Search for symbols in master contract database
   */
  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: {
          results: Array<Record<string, unknown>>;
          count: number;
        };
        error?: string;
      }>('fyers_search_symbol', {
        keyword: query,
        exchange: exchange || null,
        limit: 20,
      });

      if (!response.success || !response.data) {
        console.warn('[Fyers] searchSymbols: No results or error:', response.error);
        return [];
      }

      return response.data.results.map(item => ({
        symbol: String(item.symbol || ''),
        exchange: String(item.exchange || 'NSE') as StockExchange,
        name: String(item.name || item.symbol || ''),
        token: String(item.fytoken || item.token || '0'),
        lotSize: Number(item.lot_size || 1),
        tickSize: Number(item.tick_size || 0.05),
        instrumentType: String(item.segment || item.instrument_type || 'EQ') as any,
        currency: 'INR',
        expiry: item.expiry ? String(item.expiry) : undefined,
        strike: item.strike ? Number(item.strike) : undefined,
      }));
    } catch (error) {
      console.error('[Fyers] searchSymbols error:', error);
      return [];
    }
  }

  /**
   * Get instrument details by symbol and exchange
   */
  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      // First try to get token for the symbol
      const tokenResponse = await invoke<{
        success: boolean;
        data?: number;
        error?: string;
      }>('fyers_get_token_for_symbol', {
        symbol,
        exchange,
      });

      if (!tokenResponse.success || !tokenResponse.data) {
        // Return fallback instrument if not found
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

      // Get full symbol info by token
      const symbolResponse = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('fyers_get_symbol_by_token', {
        token: tokenResponse.data,
      });

      if (!symbolResponse.success || !symbolResponse.data) {
        return {
          symbol,
          exchange,
          name: symbol,
          token: String(tokenResponse.data),
          lotSize: 1,
          tickSize: 0.05,
          instrumentType: 'EQUITY',
          currency: 'INR',
        };
      }

      const data = symbolResponse.data;
      return {
        symbol: String(data.symbol || symbol),
        exchange: String(data.exchange || exchange) as StockExchange,
        name: String(data.name || data.symbol || symbol),
        token: String(data.fytoken || data.token || tokenResponse.data),
        lotSize: Number(data.lot_size || 1),
        tickSize: Number(data.tick_size || 0.05),
        instrumentType: String(data.segment || data.instrument_type || 'EQ') as any,
        currency: 'INR',
        expiry: data.expiry ? String(data.expiry) : undefined,
        strike: data.strike ? Number(data.strike) : undefined,
      };
    } catch (error) {
      console.error('[Fyers] getInstrument error:', error);
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

  /**
   * Download master contract from Fyers
   * Downloads 6 CSV segments: NSE_CM, NSE_FO, NSE_CD, BSE_CM, BSE_FO, MCX_COM
   */
  async downloadMasterContract(): Promise<void> {
    console.log('[Fyers] Downloading master contract...');
    try {
      const response = await invoke<{
        success: boolean;
        message?: string;
        segments?: Record<string, number>;
        total_symbols?: number;
        error?: string;
      }>('download_fyers_master_contract');

      if (!response.success) {
        throw new Error(response.error || 'Failed to download master contract');
      }

      console.log(`[Fyers] Master contract downloaded: ${response.total_symbols} symbols`);
      if (response.segments) {
        console.log('[Fyers] Segments:', response.segments);
      }
    } catch (error) {
      console.error('[Fyers] downloadMasterContract error:', error);
      throw error;
    }
  }

  /**
   * Get last updated timestamp of master contract
   */
  async getMasterContractLastUpdated(): Promise<Date | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: {
          last_updated: number;
          symbol_count: number;
          age_seconds: number;
        };
        error?: string;
      }>('fyers_get_master_contract_metadata');

      if (!response.success || !response.data) {
        return null;
      }

      // last_updated is Unix timestamp in seconds
      return new Date(response.data.last_updated * 1000);
    } catch (error) {
      console.error('[Fyers] getMasterContractLastUpdated error:', error);
      return null;
    }
  }
}
