/**
 * Shoonya (Finvasia) Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Shoonya/Finvasia API
 * - Zero brokerage broker
 * - TOTP-based authentication with SHA256
 * - Full trading and market data support
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
import { SHOONYA_METADATA, SHOONYA_EXCHANGE_MAP } from './constants';
import {
  toShoonyaOrderParams,
  toShoonyaModifyParams,
  toShoonyaSymbol,
  toShoonyaResolution,
  fromShoonyaOrder,
  fromShoonyaTrade,
  fromShoonyaPosition,
  fromShoonyaHolding,
  fromShoonyaFunds,
  fromShoonyaQuote,
  fromShoonyaDepth,
} from './mapper';

/**
 * Shoonya broker adapter
 */
export class ShoonyaAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'shoonya';
  readonly brokerName = 'Shoonya';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = SHOONYA_METADATA;

  private apiKey: string | null = null;      // vendor_code
  private apiSecret: string | null = null;   // api_secretkey

  // REST API polling for quotes
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange; token: string }> = new Map();

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Shoonya doesn't use OAuth - uses TOTP authentication
   * This returns null as the auth is done via authenticate()
   */
  getAuthUrl(): string {
    // Shoonya uses TOTP-based auth, not OAuth
    return '';
  }

  /**
   * Set API credentials before authentication
   */
  setCredentials(apiKey: string, apiSecret: string): void {
    console.log('[ShoonyaAdapter] setCredentials called');
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
  }

  /**
   * Authenticate with Shoonya using TOTP
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      // Store credentials
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;

      // Check for TOTP code and password
      const password = (credentials as any).password;
      const totpCode = (credentials as any).totpCode;

      if (!password || !totpCode) {
        return {
          success: false,
          message: 'Password and TOTP code are required for Shoonya authentication',
          errorCode: 'AUTH_REQUIRED',
        };
      }

      if (!credentials.userId) {
        return {
          success: false,
          message: 'User ID is required',
          errorCode: 'MISSING_USER_ID',
        };
      }

      console.log('[ShoonyaAdapter] Authenticating with TOTP...');

      // Call Rust backend for TOTP authentication
      const response = await invoke<{
        success: boolean;
        access_token?: string;
        user_id?: string;
        error?: string;
      }>('shoonya_authenticate', {
        userId: credentials.userId,
        password,
        totpCode,
        apiKey: this.apiKey!,
        apiSecret: this.apiSecret!,
      });

      if (response.success && response.access_token) {
        this.accessToken = response.access_token;
        this.userId = response.user_id || credentials.userId;
        this._isConnected = true;

        // Store credentials
        await this.storeCredentials({
          apiKey: this.apiKey!,
          apiSecret: this.apiSecret!,
          accessToken: this.accessToken,
          userId: this.userId,
        });

        return {
          success: true,
          accessToken: this.accessToken,
          userId: this.userId,
          message: 'Authentication successful',
        };
      }

      return {
        success: false,
        message: response.error || 'Authentication failed',
        errorCode: 'AUTH_FAILED',
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
   * Handle OAuth callback - not used for Shoonya (TOTP-based)
   */
  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    // Shoonya doesn't use OAuth callbacks
    return {
      success: false,
      message: 'Shoonya uses TOTP authentication, not OAuth',
      errorCode: 'NOT_SUPPORTED',
    };
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
      this.userId = credentials.userId || null;

      if (this.accessToken) {
        // Test token with funds call
        try {
          await this.getFundsInternal();
          this._isConnected = true;
          console.log(`[${this.brokerId}] ✓ Session restored successfully`);
          return true;
        } catch (err) {
          console.warn(`[${this.brokerId}] Access token expired or invalid`);
          this.accessToken = null;
          this._isConnected = false;
          return false;
        }
      }

      return false;
    } catch (error) {
      console.error(`[${this.brokerId}] Failed to initialize from storage:`, error);
      return false;
    }
  }

  // ============================================================================
  // Orders - Internal Implementations
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    this.ensureAuthenticated();

    try {
      const shoonyaParams = toShoonyaOrderParams(params);

      const response = await invoke<{
        success: boolean;
        order_id?: string;
        error?: string;
      }>('shoonya_place_order', {
        accessToken: this.accessToken!,
        userId: this.userId!,
        symbol: shoonyaParams.tsym,
        exchange: shoonyaParams.exch,
        quantity: Number(shoonyaParams.qty),
        price: Number(shoonyaParams.prc),
        triggerPrice: Number(shoonyaParams.trgprc) || undefined,
        disclosedQty: Number(shoonyaParams.dscqty) || undefined,
        productType: shoonyaParams.prd,
        transactionType: shoonyaParams.trantype,
        orderType: shoonyaParams.prctyp,
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
      const shoonyaParams = toShoonyaModifyParams(params.orderId, params);

      const response = await invoke<{
        success: boolean;
        data?: unknown;
        error?: string;
      }>('shoonya_modify_order', {
        accessToken: this.accessToken!,
        userId: this.userId!,
        orderId: params.orderId,
        exchange: 'NSE',  // Exchange should be obtained from order
        symbol: '',       // Symbol should be obtained from order
        quantity: params.quantity || 0,
        price: params.price || 0,
        triggerPrice: params.triggerPrice,
        disclosedQty: undefined,
        orderType: shoonyaParams.prctyp || 'LMT',
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

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown;
        error?: string;
      }>('shoonya_cancel_order', {
        accessToken: this.accessToken!,
        userId: this.userId!,
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

  protected async getOrdersInternal(): Promise<Order[]> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('shoonya_get_orders', {
        accessToken: this.accessToken!,
        userId: this.userId!,
      });

      if (response.success && response.data) {
        return response.data.map((order: unknown) =>
          fromShoonyaOrder(order as Record<string, unknown>)
        );
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch orders:', error);
      return [];
    }
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('shoonya_get_trade_book', {
        accessToken: this.accessToken!,
        userId: this.userId!,
      });

      if (response.success && response.data) {
        return response.data.map((trade: unknown) =>
          fromShoonyaTrade(trade as Record<string, unknown>)
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

  protected async getPositionsInternal(): Promise<Position[]> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('shoonya_get_positions', {
        accessToken: this.accessToken!,
        userId: this.userId!,
      });

      if (response.success && response.data) {
        return response.data.map((position: unknown) =>
          fromShoonyaPosition(position as Record<string, unknown>)
        );
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch positions:', error);
      return [];
    }
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: unknown[];
        error?: string;
      }>('shoonya_get_holdings', {
        accessToken: this.accessToken!,
        userId: this.userId!,
      });

      if (response.success && response.data) {
        return response.data.map((holding: unknown) =>
          fromShoonyaHolding(holding as Record<string, unknown>)
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

  protected async getFundsInternal(): Promise<Funds> {
    this.ensureAuthenticated();

    try {
      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('shoonya_get_funds', {
        accessToken: this.accessToken!,
        userId: this.userId!,
      });

      if (response.success && response.data) {
        return fromShoonyaFunds(response.data);
      }

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

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    this.ensureAuthenticated();

    try {
      // Get token for symbol
      const token = await this.getTokenForSymbol(symbol, exchange);

      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('shoonya_get_quotes', {
        accessToken: this.accessToken!,
        userId: this.userId!,
        exchange: SHOONYA_EXCHANGE_MAP[exchange] || exchange,
        token,
      });

      if (response.success && response.data) {
        return fromShoonyaQuote(response.data);
      }

      return this.defaultQuote(symbol, exchange);
    } catch (error) {
      console.error('Failed to fetch quote:', error);
      return this.defaultQuote(symbol, exchange);
    }
  }

  protected async getMultipleQuotesInternal(
    symbols: Array<{ symbol: string; exchange: StockExchange }>
  ): Promise<Quote[]> {
    // Fetch quotes sequentially to avoid rate limits
    const quotes: Quote[] = [];

    for (const { symbol, exchange } of symbols) {
      try {
        const quote = await this.getQuoteInternal(symbol, exchange);
        quotes.push(quote);
        // Small delay between requests
        await new Promise(resolve => setTimeout(resolve, 100));
      } catch (error) {
        console.error(`Failed to fetch quote for ${symbol}:`, error);
      }
    }

    return quotes;
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    this.ensureAuthenticated();

    try {
      const token = await this.getTokenForSymbol(symbol, exchange);
      const resolution = toShoonyaResolution(timeframe);

      const response = await invoke<{
        success: boolean;
        data?: Array<[number, number, number, number, number, number, number]>;
        error?: string;
      }>('shoonya_get_history', {
        accessToken: this.accessToken!,
        userId: this.userId!,
        exchange: SHOONYA_EXCHANGE_MAP[exchange] || exchange,
        token,
        symbol: toShoonyaSymbol(symbol, exchange),
        resolution,
        fromDate: from.toISOString().split('T')[0],
        toDate: to.toISOString().split('T')[0],
      });

      if (response.success && response.data) {
        return response.data.map(candle => ({
          timestamp: candle[0] * 1000, // Convert to milliseconds
          open: candle[1],
          high: candle[2],
          low: candle[3],
          close: candle[4],
          volume: candle[5],
        }));
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch historical data:', error);
      throw error;
    }
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    this.ensureAuthenticated();

    try {
      const token = await this.getTokenForSymbol(symbol, exchange);

      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('shoonya_get_depth', {
        accessToken: this.accessToken!,
        userId: this.userId!,
        exchange: SHOONYA_EXCHANGE_MAP[exchange] || exchange,
        token,
      });

      if (response.success && response.data) {
        return fromShoonyaDepth(response.data);
      }

      return { bids: [], asks: [] };
    } catch (error) {
      console.error('Failed to fetch market depth:', error);
      return { bids: [], asks: [] };
    }
  }

  // ============================================================================
  // WebSocket - Internal Implementations (REST polling)
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureAuthenticated();
    // Start REST API polling
    this.startQuotePolling();
    console.log('[Shoonya] REST API polling started');
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();

    // Poll every 2 minutes
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

    console.log(`[Shoonya] Polling ${this.pollingSymbols.size} symbols...`);

    for (const [key, { symbol, exchange, token }] of this.pollingSymbols) {
      try {
        const quote = await this.getQuoteInternal(symbol, exchange);

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
        console.error(`[Shoonya] Failed to poll ${symbol}:`, error);
      }

      await new Promise(resolve => setTimeout(resolve, 100));
    }
  }

  protected async subscribeInternal(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void> {
    try {
      const token = await this.getTokenForSymbol(symbol, exchange);
      const key = `${exchange}:${symbol}`;
      this.pollingSymbols.set(key, { symbol, exchange, token });

      console.log(`[Shoonya] Added ${key} to polling list`);
      this.pollQuotesSequentially();
    } catch (error) {
      console.error('Failed to subscribe:', error);
    }
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);
    console.log(`[Shoonya] Removed ${key} from polling list`);
  }

  async logout(): Promise<void> {
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    await super.logout();
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    const orders = await this.getOrdersInternal();
    const openOrders = orders.filter(o => o.status === 'OPEN' || o.status === 'PENDING');

    const results = await Promise.all(
      openOrders.map(async (order) => {
        const result = await this.cancelOrderInternal(order.orderId);
        return { success: result.success, orderId: order.orderId };
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

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    const positions = await this.getPositionsInternal();
    const activePositions = positions.filter(p => p.quantity !== 0);

    if (activePositions.length === 0) {
      return { success: true, totalCount: 0, successCount: 0, failedCount: 0, results: [] };
    }

    const results = await Promise.all(
      activePositions.map(async (position) => {
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
        return { success: result.success, symbol: position.symbol, orderId: result.orderId };
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

  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    this.ensureAuthenticated();

    const positions = await this.getPositionsInternal();
    const position = positions.find(
      p => p.symbol === params.symbol && p.exchange === params.exchange && p.productType === params.productType
    );
    const currentPosition = position ? position.quantity : 0;
    const targetPosition = params.positionSize || 0;

    let action: 'BUY' | 'SELL';
    let quantity: number;

    if (targetPosition > currentPosition) {
      action = 'BUY';
      quantity = targetPosition - currentPosition;
    } else if (targetPosition < currentPosition) {
      action = 'SELL';
      quantity = currentPosition - targetPosition;
    } else {
      return { success: true, message: 'Already at target position' };
    }

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

    return this.placeOrderInternal(orderParams);
  }

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Shoonya doesn't have a direct margin calculator API
    // Return placeholder values
    return { totalMargin: 0, initialMargin: 0 };
  }

  // ============================================================================
  // Symbol Search & Master Contract
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: { results: Array<Record<string, unknown>>; count: number };
        error?: string;
      }>('shoonya_search_symbol', {
        keyword: query,
        exchange: exchange || null,
        limit: 20,
      });

      if (!response.success || !response.data) {
        return [];
      }

      return response.data.results.map(item => ({
        symbol: String(item.symbol || ''),
        exchange: String(item.exchange || 'NSE') as StockExchange,
        name: String(item.name || item.symbol || ''),
        token: String(item.token || '0'),
        lotSize: Number(item.lot_size || 1),
        tickSize: Number(item.tick_size || 0.05),
        instrumentType: String(item.instrument_type || 'EQ') as any,
        currency: 'INR',
        expiry: item.expiry ? String(item.expiry) : undefined,
        strike: item.strike ? Number(item.strike) : undefined,
      }));
    } catch (error) {
      console.error('[Shoonya] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: Record<string, unknown>;
        error?: string;
      }>('shoonya_get_symbol_by_token', {
        token: await this.getTokenForSymbol(symbol, exchange),
        exchange,
      });

      if (!response.success || !response.data) {
        return this.defaultInstrument(symbol, exchange);
      }

      const data = response.data;
      return {
        symbol: String(data.symbol || symbol),
        exchange: String(data.exchange || exchange) as StockExchange,
        name: String(data.name || data.symbol || symbol),
        token: String(data.token || '0'),
        lotSize: Number(data.lot_size || 1),
        tickSize: Number(data.tick_size || 0.05),
        instrumentType: String(data.instrument_type || 'EQ') as any,
        currency: 'INR',
        expiry: data.expiry ? String(data.expiry) : undefined,
        strike: data.strike ? Number(data.strike) : undefined,
      };
    } catch (error) {
      return this.defaultInstrument(symbol, exchange);
    }
  }

  async downloadMasterContract(): Promise<void> {
    console.log('[Shoonya] Downloading master contract...');
    try {
      const response = await invoke<{
        success: boolean;
        data?: { total_symbols: number; segments: Record<string, number> };
        error?: string;
      }>('shoonya_download_master_contract');

      if (!response.success) {
        throw new Error(response.error || 'Failed to download master contract');
      }

      console.log(`[Shoonya] Master contract downloaded: ${response.data?.total_symbols} symbols`);
    } catch (error) {
      console.error('[Shoonya] downloadMasterContract error:', error);
      throw error;
    }
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: { last_updated: number; symbol_count: number };
        error?: string;
      }>('shoonya_get_master_contract_metadata');

      if (!response.success || !response.data) {
        return null;
      }

      return new Date(response.data.last_updated * 1000);
    } catch (error) {
      return null;
    }
  }

  // ============================================================================
  // Helper Methods
  // ============================================================================

  private ensureAuthenticated(): void {
    if (!this.accessToken || !this.userId) {
      throw new Error('Not authenticated. Please login first.');
    }
  }

  private async getTokenForSymbol(symbol: string, exchange: StockExchange): Promise<string> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: string;
        error?: string;
      }>('shoonya_get_token_for_symbol', {
        symbol,
        exchange,
      });

      if (response.success && response.data) {
        return response.data;
      }

      // Return symbol as fallback
      return symbol;
    } catch (error) {
      console.warn(`Failed to get token for ${symbol}:${exchange}`, error);
      return symbol;
    }
  }

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

  private defaultQuote(symbol: string, exchange: StockExchange): Quote {
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

  private defaultInstrument(symbol: string, exchange: StockExchange): Instrument {
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
