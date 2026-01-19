/**
 * Alpaca Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Alpaca Trading API
 * Supports both live and paper trading modes
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
  TickData,
  Instrument,
  InstrumentType,
} from '../../types';
import {
  ALPACA_METADATA,
  ALPACA_LIVE_API_BASE,
  ALPACA_PAPER_API_BASE,
} from './constants';
import {
  toAlpacaOrderParams,
  toAlpacaModifyParams,
  toAlpacaTimeframe,
  fromAlpacaOrder,
  fromAlpacaOrderToTrade,
  fromAlpacaPosition,
  fromAlpacaPositionToHolding,
  fromAlpacaAccount,
  fromAlpacaSnapshot,
  fromAlpacaBar,
  fromAlpacaQuoteToDepth,
  fromAlpacaWSTrade,
  fromAlpacaWSQuote,
  fromAlpacaWSBar,
} from './mapper';
import type {
  AlpacaOrder,
  AlpacaPosition,
  AlpacaAccount,
  AlpacaSnapshot,
  AlpacaBar,
  AlpacaAsset,
} from './types';

/**
 * Alpaca broker adapter
 *
 * Key Features:
 * - Supports both live and paper trading via built-in Alpaca paper trading
 * - API Key + Secret authentication
 * - Real-time WebSocket streaming
 * - Commission-free trading
 * - Fractional shares support
 * - Extended hours trading
 */
export class AlpacaAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'alpaca';
  readonly brokerName = 'Alpaca';
  readonly region: Region = 'us';
  readonly metadata: StockBrokerMetadata = ALPACA_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;
  private isPaperTrading: boolean = true; // Default to paper trading for safety

  // WebSocket state
  private wsUnlisteners: UnlistenFn[] = [];
  private subscribedSymbols: Map<string, SubscriptionMode> = new Map();

  // ============================================================================
  // Authentication
  // ============================================================================

  /**
   * Set API credentials
   */
  setCredentials(apiKey: string, apiSecret: string): void {
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
  }

  /**
   * Set trading mode (live or paper)
   * IMPORTANT: Alpaca has built-in paper trading, so we use their system
   */
  setTradingMode(isPaper: boolean): void {
    this.isPaperTrading = isPaper;
    console.log(`[Alpaca] Trading mode set to: ${isPaper ? 'PAPER' : 'LIVE'}`);
  }

  /**
   * Get the base API URL based on trading mode
   */
  private getApiBase(): string {
    return this.isPaperTrading ? ALPACA_PAPER_API_BASE : ALPACA_LIVE_API_BASE;
  }

  /**
   * Authenticate with Alpaca API
   */
  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;

      if (!this.apiKey || !this.apiSecret) {
        return {
          success: false,
          message: 'API Key and Secret are required',
          errorCode: 'AUTH_FAILED',
        };
      }

      // Determine trading mode from credentials or default
      const tradingMode = (credentials as { isPaperTrading?: boolean }).isPaperTrading;
      if (tradingMode !== undefined) {
        this.isPaperTrading = tradingMode;
      }

      console.log(`[Alpaca] Authenticating in ${this.isPaperTrading ? 'PAPER' : 'LIVE'} mode...`);

      // Validate credentials by fetching account
      const response = await invoke<{
        success: boolean;
        data?: AlpacaAccount;
        error?: string;
      }>('alpaca_get_account', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        isPaper: this.isPaperTrading,
      });

      if (!response.success || !response.data) {
        return {
          success: false,
          message: response.error || 'Authentication failed',
          errorCode: 'AUTH_FAILED',
        };
      }

      // Store credentials
      this.accessToken = `${this.apiKey}:${this.apiSecret}`; // Combined for storage
      this.userId = response.data.id;
      this._isConnected = true;

      await this.storeCredentials({
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        userId: this.userId,
      });

      console.log(`[Alpaca] ✓ Authenticated as ${response.data.account_number} (${this.isPaperTrading ? 'Paper' : 'Live'})`);

      return {
        success: true,
        accessToken: this.accessToken,
        userId: this.userId,
        message: `Authenticated successfully (${this.isPaperTrading ? 'Paper Trading' : 'Live Trading'})`,
      };
    } catch (error) {
      console.error('[Alpaca] Authentication error:', error);
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
      if (!credentials || !credentials.apiKey || !credentials.apiSecret) {
        return false;
      }

      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret;

      // Validate stored credentials
      const response = await invoke<{
        success: boolean;
        data?: AlpacaAccount;
        error?: string;
      }>('alpaca_get_account', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        isPaper: this.isPaperTrading,
      });

      if (!response.success) {
        console.warn('[Alpaca] Stored credentials are invalid');
        return false;
      }

      this.accessToken = `${this.apiKey}:${this.apiSecret}`;
      this.userId = response.data?.id || null;
      this._isConnected = true;

      console.log('[Alpaca] ✓ Session restored from storage');
      return true;
    } catch (error) {
      console.error('[Alpaca] Error loading from storage:', error);
      return false;
    }
  }

  // ============================================================================
  // Orders
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    const alpacaParams = toAlpacaOrderParams(params);

    const response = await invoke<{
      success: boolean;
      data?: AlpacaOrder;
      error?: string;
    }>('alpaca_place_order', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
      params: alpacaParams,
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
      orderId: response.data.id,
      message: `Order placed: ${response.data.status}`,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const alpacaParams = toAlpacaModifyParams(params);

    const response = await invoke<{
      success: boolean;
      data?: AlpacaOrder;
      error?: string;
    }>('alpaca_modify_order', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
      orderId: params.orderId,
      params: alpacaParams,
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
      orderId: response.data.id,
      message: 'Order modified successfully',
    };
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    const response = await invoke<{
      success: boolean;
      error?: string;
    }>('alpaca_cancel_order', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
      orderId,
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
    const response = await invoke<{
      success: boolean;
      data?: AlpacaOrder[];
      error?: string;
    }>('alpaca_get_orders', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
      status: 'all', // Get all orders
    });

    if (!response.success || !response.data) {
      console.error('[Alpaca] Failed to fetch orders:', response.error);
      return [];
    }

    return response.data.map(fromAlpacaOrder);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    // Alpaca doesn't have a separate trade book, we get filled orders
    const response = await invoke<{
      success: boolean;
      data?: AlpacaOrder[];
      error?: string;
    }>('alpaca_get_orders', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
      status: 'closed', // Get filled/cancelled orders
    });

    if (!response.success || !response.data) {
      return [];
    }

    // Convert filled orders to trades
    return response.data
      .map(fromAlpacaOrderToTrade)
      .filter((trade): trade is Trade => trade !== null);
  }

  // ============================================================================
  // Bulk Operations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    const response = await invoke<{
      success: boolean;
      data?: { cancelled_count: number };
      error?: string;
    }>('alpaca_cancel_all_orders', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
    });

    if (!response.success) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [{
          success: false,
          error: response.error || 'Failed to cancel orders',
        }],
      };
    }

    const count = response.data?.cancelled_count || 0;
    return {
      success: true,
      totalCount: count,
      successCount: count,
      failedCount: 0,
      results: [],
    };
  }

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    const response = await invoke<{
      success: boolean;
      data?: Array<{ success: boolean; symbol?: string; error?: string }>;
      error?: string;
    }>('alpaca_close_all_positions', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
    });

    if (!response.success || !response.data) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [{
          success: false,
          error: response.error || 'Failed to close positions',
        }],
      };
    }

    const results = response.data;
    return {
      success: true,
      totalCount: results.length,
      successCount: results.filter(r => r.success).length,
      failedCount: results.filter(r => !r.success).length,
      results: results.map(r => ({
        success: r.success,
        symbol: r.symbol,
        error: r.error,
      })),
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

    const response = await invoke<{
      success: boolean;
      data?: AlpacaOrder;
      error?: string;
    }>('alpaca_place_smart_order', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
      symbol: params.symbol,
      side: params.side,
      quantity: params.quantity,
      orderType: params.orderType,
      price: params.price,
      stopPrice: params.triggerPrice,
      positionSize,
    });

    if (!response.success || !response.data) {
      return {
        success: false,
        message: response.error || 'Smart order failed',
        errorCode: 'ORDER_FAILED',
      };
    }

    return {
      success: true,
      orderId: response.data.id,
      message: 'Smart order placed successfully',
    };
  }

  // ============================================================================
  // Positions & Holdings
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    const response = await invoke<{
      success: boolean;
      data?: AlpacaPosition[];
      error?: string;
    }>('alpaca_get_positions', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAlpacaPosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    // In US, holdings = positions (long-term holdings are just positions)
    const response = await invoke<{
      success: boolean;
      data?: AlpacaPosition[];
      error?: string;
    }>('alpaca_get_positions', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      isPaper: this.isPaperTrading,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromAlpacaPositionToHolding);
  }

  protected async getFundsInternal(): Promise<Funds> {
    const response = await invoke<{
      success: boolean;
      data?: AlpacaAccount;
      error?: string;
    }>('alpaca_get_account', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
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

    return fromAlpacaAccount(response.data);
  }

  // ============================================================================
  // Margin Calculator
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Alpaca calculates margin automatically
    // We estimate based on buying power rules (4x intraday, 2x overnight)
    const funds = await this.getFundsInternal();

    let totalRequired = 0;
    for (const order of orders) {
      const price = order.price || 0;
      const value = price * order.quantity;
      // Assume 25% margin requirement (4x leverage)
      totalRequired += value * 0.25;
    }

    return {
      totalMargin: totalRequired,
      initialMargin: totalRequired,
      exposureMargin: 0,
      orders: orders.map(o => ({
        symbol: o.symbol,
        margin: (o.price || 0) * o.quantity * 0.25,
      })),
    };
  }

  // ============================================================================
  // Market Data
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    const response = await invoke<{
      success: boolean;
      data?: AlpacaSnapshot;
      error?: string;
    }>('alpaca_get_snapshot', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      symbol,
    });

    if (!response.success || !response.data) {
      throw new Error(response.error || `Failed to fetch quote for ${symbol}`);
    }

    return fromAlpacaSnapshot(response.data, symbol, exchange);
  }

  /**
   * Get multiple quotes in batch
   */
  async getQuotes(
    instruments: Array<{ symbol: string; exchange: StockExchange }>
  ): Promise<Quote[]> {
    this.ensureConnected();

    const symbols = instruments.map(i => i.symbol);

    const response = await invoke<{
      success: boolean;
      data?: Record<string, AlpacaSnapshot>;
      error?: string;
    }>('alpaca_get_snapshots', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      symbols,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return instruments.map(({ symbol, exchange }) => {
      const snapshot = response.data![symbol];
      if (!snapshot) {
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
      return fromAlpacaSnapshot(snapshot, symbol, exchange);
    });
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    const alpacaTimeframe = toAlpacaTimeframe(timeframe);

    const response = await invoke<{
      success: boolean;
      data?: { bars: AlpacaBar[] };
      error?: string;
    }>('alpaca_get_bars', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      symbol,
      timeframe: alpacaTimeframe,
      start: from.toISOString(),
      end: to.toISOString(),
    });

    if (!response.success || !response.data?.bars) {
      console.error('[Alpaca] Failed to fetch OHLCV:', response.error);
      return [];
    }

    return response.data.bars.map(fromAlpacaBar);
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    // Alpaca only provides top of book, not full depth
    const response = await invoke<{
      success: boolean;
      data?: AlpacaSnapshot;
      error?: string;
    }>('alpaca_get_snapshot', {
      apiKey: this.apiKey,
      apiSecret: this.apiSecret,
      symbol,
    });

    if (!response.success || !response.data?.latestQuote) {
      return { bids: [], asks: [] };
    }

    return fromAlpacaQuoteToDepth(response.data.latestQuote);
  }

  // ============================================================================
  // WebSocket
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    try {
      if (!this.apiKey || !this.apiSecret) {
        throw new Error('API credentials not set');
      }

      // Connect via Rust backend
      const response = await invoke<{
        success: boolean;
        error?: string;
      }>('alpaca_ws_connect', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        isPaper: this.isPaperTrading,
      });

      if (!response.success) {
        throw new Error(response.error || 'WebSocket connection failed');
      }

      // Set up event listeners
      await this.setupWebSocketListeners();

      this.wsConnected = true;
      console.log('[Alpaca] ✓ WebSocket connected');
    } catch (error) {
      console.error('[Alpaca] WebSocket connection error:', error);
      throw error;
    }
  }

  private async setupWebSocketListeners(): Promise<void> {
    // Clean up existing listeners
    await this.cleanupWebSocketListeners();

    // Listen for trade updates
    const tradeUnlisten = await listen<{
      T: string;
      S: string;
      p: number;
      s: number;
      t: string;
      x: string;
    }>('alpaca_trade', (event) => {
      const tick = fromAlpacaWSTrade(event.payload as any);
      this.emitTick(tick);
    });
    this.wsUnlisteners.push(tradeUnlisten);

    // Listen for quote updates
    const quoteUnlisten = await listen<{
      T: string;
      S: string;
      bp: number;
      bs: number;
      ap: number;
      as: number;
      t: string;
    }>('alpaca_quote', (event) => {
      const tick = fromAlpacaWSQuote(event.payload as any);
      this.emitTick(tick);
    });
    this.wsUnlisteners.push(quoteUnlisten);

    // Listen for bar updates
    const barUnlisten = await listen<{
      T: string;
      S: string;
      o: number;
      h: number;
      l: number;
      c: number;
      v: number;
      t: string;
    }>('alpaca_bar', (event) => {
      const tick = fromAlpacaWSBar(event.payload as any);
      this.emitTick(tick);
    });
    this.wsUnlisteners.push(barUnlisten);

    // Listen for order updates (trade updates stream)
    const orderUnlisten = await listen<{
      event: string;
      order: AlpacaOrder;
    }>('alpaca_order_update', (event) => {
      console.log('[Alpaca] Order update:', event.payload.event, event.payload.order.symbol);
      // Could emit custom event for order updates
    });
    this.wsUnlisteners.push(orderUnlisten);

    console.log('[Alpaca] WebSocket listeners set up');
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
      await invoke('alpaca_ws_disconnect');
    } catch (error) {
      console.error('[Alpaca] WebSocket disconnect error:', error);
    }

    this.subscribedSymbols.clear();
    this.wsConnected = false;
    console.log('[Alpaca] WebSocket disconnected');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    const channels: { trades?: string[]; quotes?: string[]; bars?: string[] } = {};

    if (mode === 'ltp' || mode === 'full') {
      channels.trades = [symbol];
    }
    if (mode === 'quote' || mode === 'full') {
      channels.quotes = [symbol];
    }
    if (mode === 'full') {
      channels.bars = [symbol];
    }

    await invoke('alpaca_ws_subscribe', {
      trades: channels.trades || [],
      quotes: channels.quotes || [],
      bars: channels.bars || [],
    });

    this.subscribedSymbols.set(symbol, mode);
    console.log(`[Alpaca] Subscribed to ${symbol} (${mode})`);
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    await invoke('alpaca_ws_unsubscribe', {
      trades: [symbol],
      quotes: [symbol],
      bars: [symbol],
    });

    this.subscribedSymbols.delete(symbol);
    console.log(`[Alpaca] Unsubscribed from ${symbol}`);
  }

  // ============================================================================
  // Symbol Search & Instruments
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: AlpacaAsset[];
        error?: string;
      }>('alpaca_search_assets', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        query,
        exchange,
      });

      if (!response.success || !response.data) {
        return [];
      }

      return response.data.map(asset => ({
        symbol: asset.symbol,
        name: asset.name,
        exchange: (asset.exchange || 'NASDAQ') as StockExchange,
        instrumentType: (asset.class === 'us_equity' ? 'EQUITY' : 'ETF') as InstrumentType,
        currency: 'USD',
        token: asset.id,
        lotSize: 1, // Alpaca supports fractional, so lot size is 1
        tickSize: Number(asset.price_increment) || 0.01,
      }));
    } catch (error) {
      console.error('[Alpaca] searchSymbols error:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: AlpacaAsset;
        error?: string;
      }>('alpaca_get_asset', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        symbol,
      });

      if (!response.success || !response.data) {
        return null;
      }

      const asset = response.data;
      return {
        symbol: asset.symbol,
        name: asset.name,
        exchange: (asset.exchange || 'NASDAQ') as StockExchange,
        instrumentType: (asset.class === 'us_equity' ? 'EQUITY' : 'ETF') as InstrumentType,
        currency: 'USD',
        token: asset.id,
        lotSize: 1,
        tickSize: Number(asset.price_increment) || 0.01,
      };
    } catch (error) {
      console.error('[Alpaca] getInstrument error:', error);
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
        data?: { is_open: boolean; next_open: string; next_close: string };
        error?: string;
      }>('alpaca_get_clock', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
      });

      return response.success && response.data?.is_open === true;
    } catch {
      return false;
    }
  }

  /**
   * Get market calendar
   */
  async getMarketCalendar(start: Date, end: Date): Promise<Array<{
    date: string;
    open: string;
    close: string;
  }>> {
    try {
      const response = await invoke<{
        success: boolean;
        data?: Array<{ date: string; open: string; close: string }>;
        error?: string;
      }>('alpaca_get_calendar', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        start: start.toISOString().split('T')[0],
        end: end.toISOString().split('T')[0],
      });

      return response.success ? (response.data || []) : [];
    } catch {
      return [];
    }
  }
}

// Export singleton instance
export const alpacaAdapter = new AlpacaAdapter();
