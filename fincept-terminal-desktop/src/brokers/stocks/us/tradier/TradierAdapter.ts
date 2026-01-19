/**
 * Tradier Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Tradier Brokerage API
 * Supports both live trading (production) and paper trading (sandbox)
 */

import { invoke } from '@tauri-apps/api/core';
import { listen, UnlistenFn } from '@tauri-apps/api/event';
import { BaseStockBrokerAdapter } from '../../BaseStockBrokerAdapter';
import type {
  StockBrokerMetadata,
  Region,
  BrokerCredentials,
  AuthResponse,
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
  Instrument,
  InstrumentType,
} from '../../types';
import {
  TRADIER_METADATA,
  TRADIER_LIVE_API_BASE,
  TRADIER_SANDBOX_API_BASE,
} from './constants';
import {
  toTradierOrderRequest,
  toTradierModifyRequest,
  toTradierInterval,
  fromTradierOrder,
  fromTradierOrderToTrade,
  fromTradierPosition,
  fromTradierPositionToHolding,
  fromTradierBalances,
  fromTradierQuote,
  fromTradierBar,
  fromTradierSecurity,
  fromTradierStreamTrade,
  fromTradierStreamQuote,
  normalizeArray,
} from './mapper';
import type {
  TradierOrder,
  TradierPosition,
  TradierQuote,
  TradierBalances,
  TradierHistoricalData,
  TradierProfile,
  TradierOrders,
  TradierPositions,
  TradierQuotes,
  TradierSymbolLookup,
  TradierClock,
  TradierStreamSession,
} from './types';

/**
 * Tradier broker adapter
 *
 * Key Features:
 * - Bearer token authentication
 * - Commission-free stock trading
 * - Full options support with greeks
 * - Paper trading via sandbox environment
 * - Real-time streaming via WebSocket
 * - Extended hours trading (pre/post market)
 */
export class TradierAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'tradier';
  readonly brokerName = 'Tradier';
  readonly region: Region = 'us';
  readonly metadata: StockBrokerMetadata = TRADIER_METADATA;

  private bearerToken: string | null = null;
  private accountId: string | null = null;
  private isPaperTrading: boolean = true; // Default to paper trading for safety

  // WebSocket state
  private wsUnlisteners: UnlistenFn[] = [];
  private streamSessionId: string | null = null;
  private subscribedSymbols: Map<string, SubscriptionMode> = new Map();

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Set API credentials
   */
  setCredentials(bearerToken: string): void {
    this.bearerToken = bearerToken;
  }

  /**
   * Set trading mode (live or paper)
   */
  setTradingMode(isPaper: boolean): void {
    this.isPaperTrading = isPaper;
    console.log(`[Tradier] Trading mode set to: ${isPaper ? 'SANDBOX (Paper)' : 'PRODUCTION (Live)'}`);
  }

  /**
   * Get the base API URL based on trading mode
   */
  private getApiBase(): string {
    return this.isPaperTrading ? TRADIER_SANDBOX_API_BASE : TRADIER_LIVE_API_BASE;
  }

  /**
   * Authenticate with Tradier API
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      // Tradier uses Bearer token (access token)
      this.bearerToken = credentials.accessToken || credentials.apiKey || null;

      if (!this.bearerToken) {
        return {
          success: false,
          message: 'Bearer token (API Access Token) is required',
          errorCode: 'AUTH_FAILED',
        };
      }

      // Determine trading mode from credentials or default
      const tradingMode = (credentials as { isPaperTrading?: boolean }).isPaperTrading;
      if (tradingMode !== undefined) {
        this.isPaperTrading = tradingMode;
      }

      console.log(`[Tradier] Authenticating in ${this.isPaperTrading ? 'SANDBOX' : 'PRODUCTION'} mode...`);

      // Validate credentials by fetching user profile
      const response = await invoke<{
        success: boolean;
        data?: TradierProfile;
        error?: string;
      }>('tradier_get_profile', {
        token: this.bearerToken,
        isPaper: this.isPaperTrading,
      });

      if (!response.success || !response.data) {
        return {
          success: false,
          message: response.error || 'Authentication failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      // Get the first active account
      const accounts = normalizeArray(response.data.profile.account);
      const activeAccount = accounts.find(acc => acc.status === 'active');

      if (!activeAccount) {
        return {
          success: false,
          message: 'No active account found',
          errorCode: 'AUTH_FAILED',
        };
      }

      this.accountId = activeAccount.account_number;
      this.accessToken = this.bearerToken;
      this.userId = response.data.profile.id;
      this._isConnected = true;

      await this.storeCredentials({
        apiKey: this.bearerToken, // Use bearer token as apiKey for consistency
        accessToken: this.bearerToken,
        userId: this.userId,
      });

      console.log(`[Tradier] ✓ Authenticated as ${response.data.profile.name} (Account: ${this.accountId})`);

      return {
        success: true,
        accessToken: this.bearerToken,
        userId: this.userId,
        message: `Authenticated successfully (${this.isPaperTrading ? 'Paper Trading' : 'Live Trading'})`,
      };
    } catch (error) {
      console.error('[Tradier] Authentication error:', error);
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Authentication failed',
        errorCode: 'AUTH_FAILED',
      };
    }
  }

  /**
   * Initialize from stored credentials
   */
  async initFromStorage(): Promise<boolean> {
    try {
      const credentials = await this.loadCredentials();
      if (!credentials || !credentials.accessToken) {
        return false;
      }

      this.bearerToken = credentials.accessToken;

      // Validate stored credentials
      const response = await invoke<{
        success: boolean;
        data?: TradierProfile;
        error?: string;
      }>('tradier_get_profile', {
        token: this.bearerToken,
        isPaper: this.isPaperTrading,
      });

      if (!response.success || !response.data) {
        console.warn('[Tradier] Stored credentials are invalid');
        return false;
      }

      const accounts = normalizeArray(response.data.profile.account);
      const activeAccount = accounts.find(acc => acc.status === 'active');

      if (!activeAccount) {
        return false;
      }

      this.accountId = activeAccount.account_number;
      this.accessToken = this.bearerToken;
      this.userId = response.data.profile.id;
      this._isConnected = true;

      console.log('[Tradier] ✓ Session restored from storage');
      return true;
    } catch (error) {
      console.error('[Tradier] Error loading from storage:', error);
      return false;
    }
  }

  /**
   * Get available accounts
   */
  async getAccounts(): Promise<Array<{ id: string; name: string; type: string }>> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: TradierProfile;
        error?: string;
      }>('tradier_get_profile', {
        token: this.bearerToken,
        isPaper: this.isPaperTrading,
      });

      if (!response.success || !response.data) {
        return [];
      }

      const accounts = normalizeArray(response.data.profile.account);
      return accounts.map(acc => ({
        id: acc.account_number,
        name: `${acc.classification} - ${acc.account_number}`,
        type: acc.type,
      }));
    } catch (error) {
      console.error('[Tradier] Failed to get accounts:', error);
      return [];
    }
  }

  /**
   * Set the active account
   */
  setAccount(accountId: string): void {
    this.accountId = accountId;
    console.log(`[Tradier] Active account set to: ${accountId}`);
  }

  // ============================================================================
  // Orders
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    if (!this.accountId) {
      return { success: false, message: 'No account selected', errorCode: 'NO_ACCOUNT' };
    }

    const tradierParams = toTradierOrderRequest(params);

    const response = await invoke<{
      success: boolean;
      data?: { order: { id: number; status: string } };
      error?: string;
    }>('tradier_place_order', {
      token: this.bearerToken,
      accountId: this.accountId,
      isPaper: this.isPaperTrading,
      params: tradierParams,
    });

    if (!response.success || !response.data) {
      return {
        success: false,
        message: response.error || 'Order placement failed',
        errorCode: 'ORDER_FAILED',
      };
    }

    return {
      success: true,
      orderId: response.data.order.id.toString(),
      message: `Order placed: ${response.data.order.status}`,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    if (!this.accountId) {
      return { success: false, message: 'No account selected', errorCode: 'NO_ACCOUNT' };
    }

    const tradierParams = toTradierModifyRequest(params);

    const response = await invoke<{
      success: boolean;
      data?: { order: { id: number; status: string } };
      error?: string;
    }>('tradier_modify_order', {
      token: this.bearerToken,
      accountId: this.accountId,
      orderId: params.orderId,
      isPaper: this.isPaperTrading,
      params: tradierParams,
    });

    if (!response.success || !response.data) {
      return {
        success: false,
        message: response.error || 'Order modification failed',
        errorCode: 'ORDER_FAILED',
      };
    }

    return {
      success: true,
      orderId: response.data.order.id.toString(),
      message: 'Order modified successfully',
    };
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    if (!this.accountId) {
      return { success: false, message: 'No account selected', errorCode: 'NO_ACCOUNT' };
    }

    const response = await invoke<{
      success: boolean;
      data?: { order: { id: number; status: string } };
      error?: string;
    }>('tradier_cancel_order', {
      token: this.bearerToken,
      accountId: this.accountId,
      orderId,
      isPaper: this.isPaperTrading,
    });

    if (!response.success) {
      return {
        success: false,
        message: response.error || 'Order cancellation failed',
        errorCode: 'ORDER_FAILED',
      };
    }

    return {
      success: true,
      orderId,
      message: 'Order cancelled successfully',
    };
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    if (!this.accountId) {
      return [];
    }

    const response = await invoke<{
      success: boolean;
      data?: TradierOrders;
      error?: string;
    }>('tradier_get_orders', {
      token: this.bearerToken,
      accountId: this.accountId,
      isPaper: this.isPaperTrading,
    });

    if (!response.success || !response.data) {
      console.error('[Tradier] Failed to fetch orders:', response.error);
      return [];
    }

    const orders = normalizeArray(response.data.orders.order);
    return orders.map(fromTradierOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    if (!this.accountId) {
      return [];
    }

    // Tradier doesn't have separate trade book, get filled orders
    const response = await invoke<{
      success: boolean;
      data?: TradierOrders;
      error?: string;
    }>('tradier_get_orders', {
      token: this.bearerToken,
      accountId: this.accountId,
      isPaper: this.isPaperTrading,
      filter: 'filled', // Only get filled orders
    });

    if (!response.success || !response.data) {
      return [];
    }

    const orders = normalizeArray(response.data.orders.order);
    return orders
      .map(fromTradierOrderToTrade)
      .filter((trade): trade is Trade => trade !== null);
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    const orders = await this.getOrdersInternal();
    const openOrders = orders.filter(o => o.status === 'OPEN' || o.status === 'PENDING');

    let successCount = 0;
    let failedCount = 0;
    const results: Array<{ success: boolean; orderId?: string; error?: string }> = [];

    for (const order of openOrders) {
      const result = await this.cancelOrderInternal(order.orderId);
      if (result.success) {
        successCount++;
        results.push({ success: true, orderId: order.orderId });
      } else {
        failedCount++;
        results.push({ success: false, orderId: order.orderId, error: result.message });
      }
    }

    return {
      success: failedCount === 0,
      totalCount: openOrders.length,
      successCount,
      failedCount,
      results,
    };
  }

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    const positions = await this.getPositionsInternal();

    let successCount = 0;
    let failedCount = 0;
    const results: Array<{ success: boolean; symbol?: string; error?: string }> = [];

    for (const position of positions) {
      const closeOrder: OrderParams = {
        symbol: position.symbol,
        exchange: position.exchange,
        side: position.quantity > 0 ? 'SELL' : 'BUY',
        orderType: 'MARKET',
        quantity: Math.abs(position.quantity),
        productType: 'CASH',
      };

      const result = await this.placeOrderInternal(closeOrder);
      if (result.success) {
        successCount++;
        results.push({ success: true, symbol: position.symbol });
      } else {
        failedCount++;
        results.push({ success: false, symbol: position.symbol, error: result.message });
      }
    }

    return {
      success: failedCount === 0,
      totalCount: positions.length,
      successCount,
      failedCount,
      results,
    };
  }

  // ============================================================================
  // Smart Order
  // ============================================================================

  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    // For Tradier, smart order is basically a regular order
    // We determine the side based on position if not specified
    let side = params.side;

    if (!side && params.positionSize !== undefined) {
      const position = await this.getOpenPosition(params.symbol, params.exchange, params.productType);
      if (position) {
        // If we have a position, place opposite side to close/reduce
        side = position.quantity > 0 ? 'SELL' : 'BUY';
      }
    }

    return this.placeOrderInternal({
      ...params,
      side: side || 'BUY',
    });
  }

  // ============================================================================
  // Positions & Holdings
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    if (!this.accountId) {
      return [];
    }

    const response = await invoke<{
      success: boolean;
      data?: TradierPositions;
      error?: string;
    }>('tradier_get_positions', {
      token: this.bearerToken,
      accountId: this.accountId,
      isPaper: this.isPaperTrading,
    });

    if (!response.success || !response.data) {
      return [];
    }

    const positions = normalizeArray(response.data.positions.position);

    // Get quotes for current prices
    if (positions.length > 0) {
      const symbols = positions.map(p => p.symbol);
      const quotes = await this.getQuotesInternal(symbols);
      const quoteMap = new Map(quotes.map(q => [q.symbol, q]));

      return positions.map(pos => {
        const quote = quoteMap.get(pos.symbol);
        return fromTradierPosition(pos, quote ? {
          symbol: quote.symbol,
          last: quote.lastPrice,
        } as TradierQuote : undefined);
      });
    }

    return positions.map(pos => fromTradierPosition(pos));
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    if (!this.accountId) {
      return [];
    }

    const response = await invoke<{
      success: boolean;
      data?: TradierPositions;
      error?: string;
    }>('tradier_get_positions', {
      token: this.bearerToken,
      accountId: this.accountId,
      isPaper: this.isPaperTrading,
    });

    if (!response.success || !response.data) {
      return [];
    }

    const positions = normalizeArray(response.data.positions.position);

    // Get quotes for current prices
    if (positions.length > 0) {
      const symbols = positions.map(p => p.symbol);
      const quotes = await this.getQuotesInternal(symbols);
      const quoteMap = new Map(quotes.map(q => [q.symbol, q]));

      return positions.map(pos => {
        const quote = quoteMap.get(pos.symbol);
        return fromTradierPositionToHolding(pos, quote ? {
          symbol: quote.symbol,
          last: quote.lastPrice,
        } as TradierQuote : undefined);
      });
    }

    return positions.map(pos => fromTradierPositionToHolding(pos));
  }

  protected async getFundsInternal(): Promise<Funds> {
    if (!this.accountId) {
      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        totalBalance: 0,
        currency: 'USD',
      };
    }

    const response = await invoke<{
      success: boolean;
      data?: TradierBalances;
      error?: string;
    }>('tradier_get_balances', {
      token: this.bearerToken,
      accountId: this.accountId,
      isPaper: this.isPaperTrading,
    });

    if (!response.success || !response.data) {
      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        totalBalance: 0,
        currency: 'USD',
      };
    }

    return fromTradierBalances(response.data.balances);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Tradier doesn't have a margin preview API
    // Estimate based on standard margin rules
    const funds = await this.getFundsInternal();

    let totalRequired = 0;
    for (const order of orders) {
      const price = order.price || 0;
      const value = price * order.quantity;
      // Assume 50% margin requirement (Reg-T)
      totalRequired += value * 0.5;
    }

    return {
      totalMargin: totalRequired,
      initialMargin: totalRequired,
    };
  }

  // ============================================================================
  // Market Data
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    const quotes = await this.getQuotesInternal([symbol]);
    if (quotes.length === 0) {
      throw new Error(`Failed to fetch quote for ${symbol}`);
    }
    return quotes[0];
  }

  private async getQuotesInternal(symbols: string[]): Promise<Quote[]> {
    const response = await invoke<{
      success: boolean;
      data?: TradierQuotes;
      error?: string;
    }>('tradier_get_quotes', {
      token: this.bearerToken,
      symbols: symbols.join(','),
      isPaper: this.isPaperTrading,
    });

    if (!response.success || !response.data) {
      return [];
    }

    // Handle 'null' string response
    if (response.data.quotes.quote === 'null') {
      return [];
    }

    const quotes = normalizeArray(response.data.quotes.quote as TradierQuote | TradierQuote[]);
    return quotes.map(fromTradierQuote);
  }

  /**
   * Get multiple quotes in batch
   */
  async getQuotes(
    instruments: Array<{ symbol: string; exchange: StockExchange }>
  ): Promise<Quote[]> {
    this.ensureConnected();
    const symbols = instruments.map(i => i.symbol);
    return this.getQuotesInternal(symbols);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    const interval = toTradierInterval(timeframe);

    const response = await invoke<{
      success: boolean;
      data?: TradierHistoricalData;
      error?: string;
    }>('tradier_get_history', {
      token: this.bearerToken,
      symbol,
      interval,
      start: from.toISOString().split('T')[0],
      end: to.toISOString().split('T')[0],
      isPaper: this.isPaperTrading,
    });

    if (!response.success || !response.data?.history?.day) {
      console.error('[Tradier] Failed to fetch OHLCV:', response.error);
      return [];
    }

    const bars = normalizeArray(response.data.history.day);
    return bars.map(fromTradierBar);
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    // Tradier only provides top of book (Level 1), not full depth
    const quote = await this.getQuoteInternal(symbol, exchange);

    return {
      bids: [{ price: quote.bid, quantity: quote.bidQty, orders: 1 }],
      asks: [{ price: quote.ask, quantity: quote.askQty, orders: 1 }],
      timestamp: quote.timestamp,
    };
  }

  // ============================================================================
  // WebSocket
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    try {
      if (!this.bearerToken) {
        throw new Error('Bearer token not set');
      }

      // Get streaming session ID
      const sessionResponse = await invoke<{
        success: boolean;
        data?: TradierStreamSession;
        error?: string;
      }>('tradier_get_stream_session', {
        token: this.bearerToken,
        isPaper: this.isPaperTrading,
      });

      if (!sessionResponse.success || !sessionResponse.data) {
        throw new Error(sessionResponse.error || 'Failed to get stream session');
      }

      this.streamSessionId = sessionResponse.data.stream.sessionid;

      // Connect via Rust backend
      const response = await invoke<{
        success: boolean;
        error?: string;
      }>('tradier_ws_connect', {
        sessionId: this.streamSessionId,
        isPaper: this.isPaperTrading,
      });

      if (!response.success) {
        throw new Error(response.error || 'WebSocket connection failed');
      }

      // Set up event listeners
      await this.setupWebSocketListeners();

      this.wsConnected = true;
      console.log('[Tradier] ✓ WebSocket connected');
    } catch (error) {
      console.error('[Tradier] WebSocket connection error:', error);
      throw error;
    }
  }

  private async setupWebSocketListeners(): Promise<void> {
    // Clean up existing listeners
    await this.cleanupWebSocketListeners();

    // Listen for trade updates
    const tradeUnlisten = await listen<{
      type: 'trade';
      symbol: string;
      price: number;
      size: number;
      cvol: number;
      date: string;
      last: number;
      exch: string;
    }>('tradier_trade', (event) => {
      const tick = fromTradierStreamTrade(event.payload);
      this.emitTick(tick);
    });
    this.wsUnlisteners.push(tradeUnlisten);

    // Listen for quote updates
    const quoteUnlisten = await listen<{
      type: 'quote';
      symbol: string;
      bid: number;
      bidsz: number;
      bidexch: string;
      biddate: string;
      ask: number;
      asksz: number;
      askexch: string;
      askdate: string;
    }>('tradier_quote', (event) => {
      const tick = fromTradierStreamQuote(event.payload);
      this.emitTick(tick);
    });
    this.wsUnlisteners.push(quoteUnlisten);

    console.log('[Tradier] WebSocket listeners set up');
  }

  private async cleanupWebSocketListeners(): Promise<void> {
    for (const unlisten of this.wsUnlisteners) {
      unlisten();
    }
    this.wsUnlisteners = [];
  }

  async disconnectWebSocket(): Promise<void> {
    await this.cleanupWebSocketListeners();

    try {
      await invoke('tradier_ws_disconnect');
    } catch (error) {
      console.error('[Tradier] WebSocket disconnect error:', error);
    }

    this.subscribedSymbols.clear();
    this.streamSessionId = null;
    this.wsConnected = false;
    console.log('[Tradier] WebSocket disconnected');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    if (!this.streamSessionId) {
      throw new Error('WebSocket not connected');
    }

    await invoke('tradier_ws_subscribe', {
      sessionId: this.streamSessionId,
      symbols: [symbol],
    });

    this.subscribedSymbols.set(symbol, mode);
    console.log(`[Tradier] Subscribed to ${symbol} (${mode})`);
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    if (!this.streamSessionId) {
      return;
    }

    await invoke('tradier_ws_unsubscribe', {
      sessionId: this.streamSessionId,
      symbols: [symbol],
    });

    this.subscribedSymbols.delete(symbol);
    console.log(`[Tradier] Unsubscribed from ${symbol}`);
  }

  // ============================================================================
  // Symbol Search & Instruments
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: TradierSymbolLookup;
        error?: string;
      }>('tradier_search_symbols', {
        token: this.bearerToken,
        query,
        isPaper: this.isPaperTrading,
      });

      if (!response.success || !response.data?.securities?.security) {
        return [];
      }

      const securities = normalizeArray(response.data.securities.security);
      return securities.map(fromTradierSecurity);
    } catch (error) {
      console.error('[Tradier] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: TradierSymbolLookup;
        error?: string;
      }>('tradier_lookup_symbol', {
        token: this.bearerToken,
        symbol,
        isPaper: this.isPaperTrading,
      });

      if (!response.success || !response.data?.securities?.security) {
        return null;
      }

      const securities = normalizeArray(response.data.securities.security);
      const security = securities.find(s => s.symbol.toUpperCase() === symbol.toUpperCase());

      if (!security) {
        return null;
      }

      return fromTradierSecurity(security);
    } catch (error) {
      console.error('[Tradier] getInstrument error:', error);
      return null;
    }
  }

  // ============================================================================
  // Utility Methods
  // ============================================================================

  /**
   * Check if market is open
   */
  async isMarketOpen(): Promise<boolean> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: TradierClock;
        error?: string;
      }>('tradier_get_clock', {
        token: this.bearerToken,
        isPaper: this.isPaperTrading,
      });

      return response.success && response.data?.clock.state === 'open';
    } catch {
      return false;
    }
  }

  /**
   * Get market clock
   */
  async getMarketClock(): Promise<{
    state: string;
    nextChange: string;
    timestamp: number;
  } | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: TradierClock;
        error?: string;
      }>('tradier_get_clock', {
        token: this.bearerToken,
        isPaper: this.isPaperTrading,
      });

      if (!response.success || !response.data) {
        return null;
      }

      return {
        state: response.data.clock.state,
        nextChange: response.data.clock.next_change,
        timestamp: response.data.clock.timestamp,
      };
    } catch {
      return null;
    }
  }
}

// Export singleton instance
export const tradierAdapter = new TradierAdapter();
