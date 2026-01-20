/**
 * Motilal Oswal Adapter
 *
 * Implementation of BaseStockBrokerAdapter for Motilal Oswal
 * Reference: https://openapi.motilaloswal.com
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
} from '../../types';
import {
  MOTILAL_METADATA,
  MOTILAL_EXCHANGE_MAP,
} from './constants';
import {
  fromMotilalOrder,
  fromMotilalTrade,
  fromMotilalPosition,
  fromMotilalHolding,
  fromMotilalFunds,
  fromMotilalQuote,
  fromMotilalDepth,
  getMotilalExchange,
} from './mapper';

interface TauriResponse {
  success: boolean;
  data?: Record<string, unknown>;
  error?: string;
  timestamp?: number;
}

export class MotilalAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'motilal';
  readonly brokerName = 'Motilal Oswal';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = MOTILAL_METADATA;

  private authToken: string = '';
  private apiKey: string = '';
  private vendorCode: string = '';

  // Tauri event unlisteners for WebSocket events
  private tickerUnlisten: UnlistenFn | null = null;
  private orderbookUnlisten: UnlistenFn | null = null;
  private statusUnlisten: UnlistenFn | null = null;

  // REST API polling for quotes (fallback)
  private quotePollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange }> = new Map();
  private usePollingFallback: boolean = false;

  // ============================================================================
  // AUTHENTICATION
  // ============================================================================

  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      // Extract credentials
      const userId = credentials.userId || '';
      const password = credentials.apiSecret || credentials.password || '';
      const totp = credentials.totpSecret || '';
      const dob = credentials.pin || '';
      this.apiKey = credentials.apiKey || '';
      this.vendorCode = userId;

      const response = await invoke<TauriResponse>('motilal_authenticate', {
        userId,
        password,
        totp: totp || null,
        dob,
        apiKey: this.apiKey,
      });

      if (response.success && response.data) {
        this.authToken = response.data.auth_token as string || '';
        this.accessToken = this.authToken;
        this.userId = userId;
        this._isConnected = true;

        // Store credentials
        await this.storeCredentials({
          apiKey: this.apiKey,
          apiSecret: password,
          accessToken: this.authToken,
          userId,
        });

        return {
          success: true,
          accessToken: this.authToken,
          userId,
          message: 'Authentication successful',
        };
      }

      return {
        success: false,
        message: response.error || 'Authentication failed',
        errorCode: 'AUTH_FAILED',
      };
    } catch (error) {
      console.error('Motilal authentication error:', error);
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Authentication failed',
        errorCode: 'AUTH_FAILED',
      };
    }
  }

  async validateToken(token: string): Promise<boolean> {
    try {
      const response = await invoke<TauriResponse>('motilal_validate_token', {
        authToken: token,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
      });
      return response.success;
    } catch {
      return false;
    }
  }

  /**
   * Try to restore session from stored credentials
   */
  async tryRestoreSession(): Promise<boolean> {
    try {
      const credentials = await this.loadCredentials();
      if (!credentials) return false;

      this.apiKey = credentials.apiKey;
      this.authToken = credentials.accessToken || '';
      this.vendorCode = credentials.userId || '';

      if (this.authToken) {
        const isValid = await this.validateToken(this.authToken);
        if (isValid) {
          this.accessToken = this.authToken;
          this._isConnected = true;
          return true;
        }
      }

      return false;
    } catch {
      return false;
    }
  }

  // ============================================================================
  // ORDERS (Internal implementations)
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    try {
      const motilalExchange = MOTILAL_EXCHANGE_MAP[params.exchange] || 'NSE';
      // Token lookup would need to be done via master contract
      // For now, backend will need to handle this or token passed via tag
      const symbolToken = 0;

      const response = await invoke<TauriResponse>('motilal_place_order', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
        exchange: motilalExchange,
        symbolToken,
        buyOrSell: params.side,
        orderType: params.orderType,
        productType: params.productType,
        quantity: params.quantity,
        price: params.price || 0,
        triggerPrice: params.triggerPrice || 0,
        disclosedQuantity: params.disclosedQuantity || 0,
        validity: params.validity || 'DAY',
        amo: false,
      });

      if (response.success && response.data) {
        return {
          success: true,
          orderId: response.data.order_id as string,
          message: 'Order placed successfully',
        };
      }

      return {
        success: false,
        message: response.error || 'Order placement failed',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Unknown error',
      };
    }
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    try {
      // For Motilal, we need to fetch lastmodifiedtime and qtytradedtoday from order book
      const ordersResponse = await invoke<TauriResponse>('motilal_get_orders', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
      });

      let lastModifiedTime = '';
      let qtyTradedToday = 0;

      if (ordersResponse.success && ordersResponse.data) {
        const orders = ordersResponse.data.orders as Array<Record<string, unknown>> || [];
        const order = orders.find(o => o.uniqueorderid === params.orderId);
        if (order) {
          lastModifiedTime = order.lastmodifiedtime as string || '';
          qtyTradedToday = Number(order.qtytradedtoday || 0);
        }
      }

      const response = await invoke<TauriResponse>('motilal_modify_order', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
        orderId: params.orderId,
        orderType: params.orderType || 'LIMIT',
        quantity: params.quantity || 0,
        price: params.price || 0,
        triggerPrice: params.triggerPrice || 0,
        disclosedQuantity: 0,
        lastModifiedTime,
        qtyTradedToday,
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
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Unknown error',
      };
    }
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    try {
      const response = await invoke<TauriResponse>('motilal_cancel_order', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
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
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Unknown error',
      };
    }
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    try {
      const response = await invoke<TauriResponse>('motilal_get_orders', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
      });

      if (response.success && response.data) {
        const orders = response.data.orders as Array<Record<string, unknown>> || [];
        return orders.map(fromMotilalOrder);
      }

      return [];
    } catch (error) {
      console.error('Error fetching orders:', error);
      return [];
    }
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    try {
      const response = await invoke<TauriResponse>('motilal_get_trade_book', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
      });

      if (response.success && response.data) {
        const trades = response.data.trades as Array<Record<string, unknown>> || [];
        return trades.map(fromMotilalTrade);
      }

      return [];
    } catch (error) {
      console.error('Error fetching trades:', error);
      return [];
    }
  }

  // ============================================================================
  // PORTFOLIO (Internal implementations)
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    try {
      const response = await invoke<TauriResponse>('motilal_get_positions', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
      });

      if (response.success && response.data) {
        const positions = response.data.positions as Array<Record<string, unknown>> || [];
        return positions.map(fromMotilalPosition);
      }

      return [];
    } catch (error) {
      console.error('Error fetching positions:', error);
      return [];
    }
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    try {
      const response = await invoke<TauriResponse>('motilal_get_holdings', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
      });

      if (response.success && response.data) {
        const holdings = response.data.holdings as Array<Record<string, unknown>> || [];
        return holdings.map(fromMotilalHolding);
      }

      return [];
    } catch (error) {
      console.error('Error fetching holdings:', error);
      return [];
    }
  }

  protected async getFundsInternal(): Promise<Funds> {
    try {
      const response = await invoke<TauriResponse>('motilal_get_funds', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
      });

      if (response.success && response.data) {
        return fromMotilalFunds(response.data);
      }

      // Return default funds if fetch fails
      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        totalBalance: 0,
        currency: 'INR',
      };
    } catch (error) {
      console.error('Error fetching funds:', error);
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
  // MARKET DATA (Internal implementations)
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    try {
      const motilalExchange = getMotilalExchange(exchange);
      const symbolToken = 0; // TODO: Implement token lookup from master contract

      const response = await invoke<TauriResponse>('motilal_get_quotes', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
        exchange: motilalExchange,
        symbolToken,
      });

      if (response.success && response.data) {
        return fromMotilalQuote(response.data, symbol, exchange);
      }

      // Return empty quote
      return this.createEmptyQuote(symbol, exchange);
    } catch (error) {
      console.error('Error fetching quote:', error);
      return this.createEmptyQuote(symbol, exchange);
    }
  }

  private createEmptyQuote(symbol: string, exchange: StockExchange): Quote {
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

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    _timeframe: TimeFrame,
    _from: Date,
    _to: Date
  ): Promise<OHLCV[]> {
    // Motilal Oswal does not support historical data API
    console.warn('Motilal Oswal does not support historical data API');
    return [];
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    try {
      const motilalExchange = getMotilalExchange(exchange);
      const symbolToken = 0; // TODO: Implement token lookup

      const response = await invoke<TauriResponse>('motilal_get_depth', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
        exchange: motilalExchange,
        symbolToken,
      });

      if (response.success && response.data) {
        return fromMotilalDepth(response.data);
      }

      return { bids: [], asks: [] };
    } catch (error) {
      console.error('Error fetching market depth:', error);
      return { bids: [], asks: [] };
    }
  }

  // ============================================================================
  // BULK OPERATIONS (Internal implementations)
  // ============================================================================

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    try {
      const response = await invoke<TauriResponse>('motilal_close_all_positions', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
      });

      if (response.success) {
        const data = response.data || {};
        const closed = Number(data.closed || 0);
        const failed = Number(data.failed || 0);
        return {
          success: true,
          totalCount: closed + failed,
          successCount: closed,
          failedCount: failed,
          results: [],
        };
      }

      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    } catch (error) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }
  }

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    try {
      const response = await invoke<TauriResponse>('motilal_cancel_all_orders', {
        authToken: this.authToken,
        apiKey: this.apiKey,
        vendorCode: this.vendorCode,
      });

      if (response.success) {
        const data = response.data || {};
        const cancelled = Number(data.cancelled || 0);
        const failed = Number(data.failed || 0);
        return {
          success: true,
          totalCount: cancelled + failed,
          successCount: cancelled,
          failedCount: failed,
          results: [],
        };
      }

      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    } catch (error) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }
  }

  // ============================================================================
  // SMART ORDER (Internal implementation)
  // ============================================================================

  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    // Motilal doesn't have native smart orders - use regular order
    return this.placeOrderInternal({
      ...params,
      orderType: params.orderType || 'MARKET',
      productType: params.productType || 'INTRADAY',
      validity: params.validity || 'DAY',
    });
  }

  // ============================================================================
  // MARGIN CALCULATOR (Internal implementation)
  // ============================================================================

  protected async calculateMarginInternal(_orders: OrderParams[]): Promise<MarginRequired> {
    // Motilal doesn't have margin calculator API
    return {
      totalMargin: 0,
      initialMargin: 0,
    };
  }

  // ============================================================================
  // WebSocket - Real WebSocket via Rust Backend with Polling Fallback
  // ============================================================================

  protected async connectWebSocketInternal(_config: WebSocketConfig): Promise<void> {
    this.ensureConnected();

    try {
      await this.connectRealWebSocket();
      console.log('[Motilal] ✓ WebSocket connected via Rust backend');
    } catch (error) {
      console.warn('[Motilal] WebSocket connection failed, falling back to REST polling:', error);
      this.usePollingFallback = true;
      this.startQuotePolling();
    }
  }

  private async connectRealWebSocket(): Promise<void> {
    if (!this.authToken) {
      throw new Error('Auth token required');
    }

    const result = await invoke<{ success: boolean; data?: boolean; error?: string }>('motilal_ws_connect', {
      authToken: this.authToken,
      apiKey: this.apiKey,
      vendorCode: this.vendorCode,
    });

    if (!result.success) throw new Error(result.error || 'WebSocket connection failed');

    this.tickerUnlisten = await listen<{
      provider: string; symbol: string; price: number; bid?: number; ask?: number;
      volume?: number; change_percent?: number; timestamp: number;
    }>('motilal_ticker', (event) => {
      const tick = event.payload;
      this.emitTick({
        symbol: tick.symbol, exchange: 'NSE' as StockExchange, mode: 'quote',
        lastPrice: tick.price, bid: tick.bid, ask: tick.ask,
        volume: tick.volume || 0, changePercent: tick.change_percent || 0, timestamp: tick.timestamp,
      });
    });

    this.orderbookUnlisten = await listen<{
      provider: string; symbol: string;
      bids: Array<{ price: number; quantity: number; orders?: number }>; asks: Array<{ price: number; quantity: number; orders?: number }>;
      timestamp: number;
    }>('motilal_orderbook', (event) => {
      const data = event.payload;
      const [exchangePart, symbolPart] = data.symbol.includes(':') ? data.symbol.split(':') : ['NSE', data.symbol];
      const symbol = symbolPart || data.symbol;
      const exchange = (exchangePart as StockExchange) || 'NSE';

      console.log(`[Motilal WebSocket] Depth update for ${symbol}`);

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
      provider: string; status: string; message?: string; timestamp: number;
    }>('motilal_status', (event) => {
      const status = event.payload;
      if (status.status === 'disconnected' || status.status === 'error') {
        this.wsConnected = false;
        if (!this.usePollingFallback) { this.usePollingFallback = true; this.startQuotePolling(); }
      } else if (status.status === 'connected') {
        this.wsConnected = true;
        if (this.usePollingFallback) { this.usePollingFallback = false; this.stopQuotePolling(); }
      }
    });

    this.wsConnected = true;
  }

  private async disconnectRealWebSocket(): Promise<void> {
    if (this.tickerUnlisten) { this.tickerUnlisten(); this.tickerUnlisten = null; }
    if (this.orderbookUnlisten) { this.orderbookUnlisten(); this.orderbookUnlisten = null; }
    if (this.statusUnlisten) { this.statusUnlisten(); this.statusUnlisten = null; }
    try { await invoke('motilal_ws_disconnect'); } catch (err) { console.warn('[Motilal] WS disconnect error:', err); }
    this.wsConnected = false;
  }

  private startQuotePolling(): void {
    this.stopQuotePolling();
    console.log('[Motilal] Starting REST API polling (fallback mode, 2 min interval)');
    this.quotePollingInterval = setInterval(async () => { await this.pollQuotesSequentially(); }, 120000);
    this.pollQuotesSequentially();
  }

  private stopQuotePolling(): void {
    if (this.quotePollingInterval) { clearInterval(this.quotePollingInterval); this.quotePollingInterval = null; }
  }

  private async pollQuotesSequentially(): Promise<void> {
    if (this.pollingSymbols.size === 0) return;

    for (const [key, { symbol, exchange }] of this.pollingSymbols) {
      try {
        const quote = await this.getQuoteInternal(symbol, exchange);
        this.emitTick({
          symbol, exchange, mode: 'full',
          lastPrice: quote.lastPrice, open: quote.open, high: quote.high, low: quote.low,
          close: quote.close, volume: quote.volume, change: quote.change,
          changePercent: quote.changePercent, timestamp: quote.timestamp,
          bid: quote.bid, bidQty: quote.bidQty, ask: quote.ask, askQty: quote.askQty,
        });
      } catch (error) { console.error(`[Motilal] Failed to poll ${symbol}:`, error); }
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    console.log(`[Motilal] Polling complete for ${this.pollingSymbols.size} symbols`);
  }

  async disconnectWebSocket(): Promise<void> {
    await this.disconnectRealWebSocket();
    this.stopQuotePolling();
    this.pollingSymbols.clear();
    this.usePollingFallback = false;
    console.log('[Motilal] WebSocket disconnected');
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    try {
      const key = `${exchange}:${symbol}`;
      this.pollingSymbols.set(key, { symbol, exchange });

      // If real WebSocket is connected, subscribe via Tauri command
      if (this.wsConnected && !this.usePollingFallback) {
        try {
          await invoke('motilal_ws_subscribe', {
            symbol: `${exchange}:${symbol}`,
            mode: mode === 'full' ? 'depth' : mode === 'quote' ? 'quotes' : 'ltp',
          });
          console.log(`[Motilal] WebSocket subscribed to ${key}`);
        } catch (err) {
          console.warn(`[Motilal] WebSocket subscribe failed for ${key}, using polling:`, err);
          this.pollQuotesSequentially();
        }
      } else {
        console.log(`Motilal: Added ${key} to polling list (${this.pollingSymbols.size} symbols total)`);
        this.pollQuotesSequentially();
      }
    } catch (error) {
      console.error('Failed to subscribe:', error);
      throw error;
    }
  }

  protected async unsubscribeInternal(
    symbol: string,
    exchange: StockExchange
  ): Promise<void> {
    const key = `${exchange}:${symbol}`;
    this.pollingSymbols.delete(key);

    // If real WebSocket is connected, unsubscribe via Tauri command
    if (this.wsConnected && !this.usePollingFallback) {
      try {
        await invoke('motilal_ws_unsubscribe', { symbol: `${exchange}:${symbol}` });
        console.log(`[Motilal] WebSocket unsubscribed from ${key}`);
      } catch (err) {
        console.warn(`[Motilal] WebSocket unsubscribe failed for ${key}:`, err);
      }
    } else {
      console.log(`Motilal: Removed ${key} from polling list (${this.pollingSymbols.size} symbols remaining)`);
    }
  }

  // ============================================================================
  // METADATA
  // ============================================================================

  getSupportedExchanges(): StockExchange[] {
    return ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'];
  }

  getSupportedTimeframes(): TimeFrame[] {
    // Motilal doesn't support historical data
    return [];
  }
}

// Export singleton instance
export const motilalAdapter = new MotilalAdapter();
