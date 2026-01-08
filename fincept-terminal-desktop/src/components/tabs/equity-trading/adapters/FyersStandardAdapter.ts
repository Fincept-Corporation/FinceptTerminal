/**
 * Fyers Standardized Adapter
 * Wraps existing Fyers modules with BaseBrokerAdapter interface
 */

import { BaseBrokerAdapter } from './BaseBrokerAdapter';
import {
  BrokerCredentials,
  AuthStatus,
  BrokerStatus,
  UnifiedOrder,
  OrderRequest,
  OrderResponse,
  UnifiedPosition,
  UnifiedHolding,
  UnifiedQuote,
  UnifiedMarketDepth,
  Candle,
  TimeInterval,
  UnifiedFunds,
  UnifiedProfile,
  OrderSide,
  OrderType,
  OrderStatus,
  ProductType,
  Validity,
} from '../core/types';
import { fyersAdapter } from '../../../../brokers/stocks/fyers/adapter';
import { FyersAuth } from '../../../../brokers/stocks/fyers/auth';
import { FyersDataWebSocket, DataSocketMessage } from '../../../../brokers/stocks/fyers/dataWebsocket';
import type {
  FyersOrder,
  FyersPosition,
  FyersHolding,
  FyersQuote,
  FyersMarketDepth,
  FyersHistoryCandle,
  PlaceOrderRequest,
} from '../../../../brokers/stocks/fyers/types';

export class FyersStandardAdapter extends BaseBrokerAdapter {
  private fyersAdapter = fyersAdapter; // Use singleton instance
  private authClient: FyersAuth;
  private dataWebSocket: FyersDataWebSocket;
  private lastAuthTime?: Date;
  private tokenExpiryTime?: Date;
  private quoteCallbacks: Map<string, (quote: UnifiedQuote) => void> = new Map();
  private depthCallbacks: Map<string, (depth: UnifiedMarketDepth) => void> = new Map();
  private quoteCache: Map<string, UnifiedQuote> = new Map(); // Cache OHLC data from REST API

  // Store unsubscribe functions for cleanup
  private messageUnsubscribe?: () => void;
  private connectUnsubscribe?: () => void;
  private errorUnsubscribe?: () => void;
  private closeUnsubscribe?: () => void;

  // Configurable connection timeout (default 15s)
  private connectionTimeoutMs = 15000;

  constructor() {
    super('fyers');
    this.authClient = new FyersAuth();
    this.dataWebSocket = new FyersDataWebSocket();
  }

  /**
   * Set connection timeout
   */
  setConnectionTimeout(timeoutMs: number): void {
    this.connectionTimeoutMs = timeoutMs;
  }

  // ==================== BROKER CAPABILITIES ====================

  getSupportedExchanges(): string[] {
    // Fyers supports Indian equity markets
    return ['NSE', 'BSE', 'MCX', 'NFO', 'CDS', 'BFO'];
  }

  async initialize(credentials: BrokerCredentials): Promise<boolean> {
    console.log('[FyersStandardAdapter] Initialize called with credentials:', {
      hasApiKey: !!credentials.apiKey,
      hasApiSecret: !!credentials.apiSecret,
      hasAdditionalData: !!credentials.additionalData
    });
    this.credentials = credentials;
    this.setStatus(BrokerStatus.CONNECTING);
    try {
      const success = await this.fyersAdapter.initialize();
      console.log('[FyersStandardAdapter] fyersAdapter.initialize() returned:', success);
      if (success) this.setStatus(BrokerStatus.CONNECTED);
      else this.setStatus(BrokerStatus.ERROR);
      return success;
    } catch (error) {
      console.error('[FyersStandardAdapter] Initialize error:', error);
      this.setStatus(BrokerStatus.ERROR);
      return false;
    }
  }

  async authenticate(): Promise<AuthStatus> {
    if (!this.credentials) {
      return { brokerId: 'fyers', status: BrokerStatus.ERROR, authenticated: false, error: 'No credentials' };
    }

    try {
      if (this.credentials.additionalData?.clientId && this.credentials.additionalData?.pin) {
        const authResult = await this.authClient.authenticate({
          clientId: this.credentials.additionalData.clientId,
          pin: this.credentials.additionalData.pin,
          appId: this.credentials.apiKey,
          appType: this.credentials.additionalData.appType || '100',
          appSecret: this.credentials.apiSecret,
          totpKey: this.credentials.additionalData.totpKey,
        });

        await this.fyersAdapter.saveCredentials({
          appId: this.credentials.apiKey,
          redirectUrl: this.credentials.apiSecret,
          accessToken: authResult.accessToken,
          appType: this.credentials.additionalData.appType || '100',
        });

        this.lastAuthTime = new Date();
        this.tokenExpiryTime = new Date(Date.now() + 24 * 60 * 60 * 1000);
        this.setStatus(BrokerStatus.AUTHENTICATED);

        return { brokerId: 'fyers', status: BrokerStatus.AUTHENTICATED, authenticated: true, lastAuth: this.lastAuthTime, expiresAt: this.tokenExpiryTime };
      }

      const isReady = this.fyersAdapter.isReady();
      if (isReady) {
        this.setStatus(BrokerStatus.AUTHENTICATED);
        return { brokerId: 'fyers', status: BrokerStatus.AUTHENTICATED, authenticated: true };
      }

      throw new Error('Insufficient credentials');
    } catch (error: any) {
      this.setStatus(BrokerStatus.ERROR);
      return { brokerId: 'fyers', status: BrokerStatus.ERROR, authenticated: false, error: error.message };
    }
  }

  isAuthenticated(): boolean {
    return this.fyersAdapter.isReady();
  }

  getAuthStatus(): AuthStatus {
    return { brokerId: 'fyers', status: this.status, authenticated: this.isAuthenticated(), lastAuth: this.lastAuthTime, expiresAt: this.tokenExpiryTime };
  }

  async refreshToken(): Promise<boolean> {
    return await this.authenticate().then(status => status.authenticated);
  }

  async disconnect(): Promise<void> {
    this.setStatus(BrokerStatus.DISCONNECTED);
  }

  async placeOrder(order: OrderRequest): Promise<OrderResponse> {
    if (!this.isAuthenticated()) return { success: false, message: 'Not authenticated' };

    try {
      const fyersOrder: PlaceOrderRequest = {
        symbol: order.symbol,
        qty: order.quantity,
        type: this.mapOrderType(order.type),
        side: order.side === OrderSide.BUY ? 1 : -1,
        productType: this.mapProductType(order.product),
        limitPrice: order.price || 0,
        stopPrice: order.triggerPrice || 0,
        disclosedQty: 0,
        validity: order.validity || Validity.DAY,
        offlineOrder: false,
        stopLoss: 0,
        takeProfit: 0,
        orderTag: order.tag,
      };

      const result = await this.fyersAdapter.trading.placeOrder(fyersOrder);
      return { success: true, orderId: result.orderId, message: result.message };
    } catch (error: any) {
      return { success: false, message: error.message || 'Order failed' };
    }
  }

  async modifyOrder(orderId: string, updates: Partial<OrderRequest>): Promise<OrderResponse> {
    return { success: false, message: 'Modify not supported' };
  }

  async cancelOrder(orderId: string): Promise<OrderResponse> {
    if (!this.isAuthenticated()) return { success: false, message: 'Not authenticated' };

    try {
      await this.fyersAdapter.trading.cancelOrder(orderId);
      return { success: true, message: 'Order cancelled' };
    } catch (error: any) {
      return { success: false, message: error.message };
    }
  }

  async getOrders(): Promise<UnifiedOrder[]> {
    if (!this.isAuthenticated()) return [];
    try {
      const orders = await this.fyersAdapter.trading.getOrders();
      return orders.map(o => this.mapFyersOrder(o));
    } catch { return []; }
  }

  async getOrder(orderId: string): Promise<UnifiedOrder> {
    const orders = await this.getOrders();
    const order = orders.find(o => o.id === orderId);
    if (!order) throw new Error(`Order ${orderId} not found`);
    return order;
  }

  async getOrderHistory(orderId: string): Promise<UnifiedOrder[]> {
    return [await this.getOrder(orderId)];
  }

  async getPositions(): Promise<UnifiedPosition[]> {
    if (!this.isAuthenticated()) return [];
    try {
      const positions = await this.fyersAdapter.portfolio.getPositions();
      return positions.map(p => this.mapFyersPosition(p));
    } catch { return []; }
  }

  async getHoldings(): Promise<UnifiedHolding[]> {
    if (!this.isAuthenticated()) return [];
    try {
      const response = await this.fyersAdapter.portfolio.getHoldings();
      return response.holdings.map(h => this.mapFyersHolding(h));
    } catch { return []; }
  }

  async getFunds(): Promise<UnifiedFunds> {
    const funds = await this.fyersAdapter.portfolio.getFunds();
    return {
      brokerId: 'fyers',
      availableCash: funds[0]?.equityAmount || 0,
      usedMargin: 0,
      availableMargin: funds[0]?.equityAmount || 0,
      totalCollateral: 0,
      equity: funds[0]?.equityAmount || 0,
      timestamp: new Date(),
    };
  }

  async getProfile(): Promise<UnifiedProfile> {
    const profile = await this.fyersAdapter.portfolio.getProfile();
    return {
      brokerId: 'fyers',
      userId: profile.PAN || '',
      name: profile.name,
      email: profile.email_id,
      exchanges: ['NSE', 'BSE', 'MCX'],
      products: ['CNC', 'INTRADAY', 'MARGIN'],
    };
  }

  async getQuote(symbol: string, exchange: string): Promise<UnifiedQuote> {
    const fyersSymbol = `${exchange}:${symbol}`;
    const quotes = await this.fyersAdapter.market.getQuotes([fyersSymbol]);
    const quote = quotes[fyersSymbol];
    if (!quote) {
      throw new Error(`Quote not found for ${fyersSymbol}`);
    }
    return this.mapFyersQuote(quote, symbol, exchange);
  }

  async getQuotes(symbols: Array<{ symbol: string; exchange: string }>): Promise<UnifiedQuote[]> {
    const promises = symbols.map(s => this.getQuote(s.symbol, s.exchange).catch(() => null));
    const results = await Promise.all(promises);
    return results.filter(r => r !== null) as UnifiedQuote[];
  }

  async getMarketDepth(symbol: string, exchange: string): Promise<UnifiedMarketDepth> {
    const fyersSymbol = `${exchange}:${symbol}`;
    const depths = await this.fyersAdapter.market.getMarketDepth([fyersSymbol]);
    const depth = depths[fyersSymbol];
    if (!depth) {
      throw new Error(`Market depth not found for ${fyersSymbol}`);
    }
    return this.mapFyersMarketDepth(depth, symbol, exchange);
  }

  async getHistoricalData(symbol: string, exchange: string, interval: TimeInterval, from: Date, to: Date): Promise<Candle[]> {
    const fyersSymbol = `${exchange}:${symbol}`;
    const resolution = this.mapTimeInterval(interval);

    // Convert dates to Unix timestamps (seconds)
    const range_from = Math.floor(from.getTime() / 1000).toString();
    const range_to = Math.floor(to.getTime() / 1000).toString();

    const response = await this.fyersAdapter.market.getHistory({
      symbol: fyersSymbol,
      resolution,
      date_format: '0', // 0 = Unix timestamp
      range_from,
      range_to,
      cont_flag: '1'
    });

    const candles = response.candles || [];
    return candles.map((c: any) => this.mapFyersCandle(c));
  }

  async connectWebSocket(): Promise<void> {
    // Get access token from fyersAdapter credentials
    const creds = await this.fyersAdapter.getCredentials();
    if (!creds || !creds.accessToken) {
      throw new Error('No access token available for WebSocket connection. Please authenticate Fyers first.');
    }

    // Format: APP_ID-APP_TYPE:ACCESS_TOKEN
    const appType = creds.appType || '100';
    const fullAccessToken = `${creds.appId}-${appType}:${creds.accessToken}`;

    console.log('[FyersStandardAdapter] Connecting Data WebSocket with token:', {
      appId: creds.appId,
      appType: appType,
      hasToken: !!creds.accessToken
    });

    // Clean up any existing listeners before setting up new ones
    this.cleanupWebSocketListeners();

    // Setup message handler BEFORE connecting
    this.messageUnsubscribe = this.dataWebSocket.onMessage((message: DataSocketMessage) => {
      console.log('[FyersStandardAdapter] Received message:', message);

      // Handle quote updates
      const symbolKey = message.symbol;
      const quoteCallback = this.quoteCallbacks.get(symbolKey);

      if (quoteCallback) {
        const quote = this.mapDataSocketToQuote(message);
        quoteCallback(quote);
      }

      // Handle depth updates (if in FullMode)
      const depthCallback = this.depthCallbacks.get(symbolKey);
      if (depthCallback && message.market_depth) {
        const depth = this.mapDataSocketToDepth(message);
        depthCallback(depth);
      }

      // Forward to Rust monitoring service
      this.forwardToMonitoringService(message);
    });

    // Wait for connection
    return new Promise((resolve, reject) => {
      let settled = false;

      const timeout = setTimeout(() => {
        if (!settled) {
          settled = true;
          reject(new Error(`WebSocket connection timeout after ${this.connectionTimeoutMs}ms`));
        }
      }, this.connectionTimeoutMs);

      this.connectUnsubscribe = this.dataWebSocket.onConnect(() => {
        if (!settled) {
          settled = true;
          clearTimeout(timeout);
          console.log('[FyersStandardAdapter] Data WebSocket connected successfully');
          resolve();
        }
      });

      this.errorUnsubscribe = this.dataWebSocket.onError((error) => {
        if (!settled) {
          settled = true;
          clearTimeout(timeout);
          reject(error);
        }
      });

      // Setup close handler for reconnection scenarios
      this.closeUnsubscribe = this.dataWebSocket.onClose(() => {
        console.log('[FyersStandardAdapter] Data WebSocket connection closed');
      });

      // Connect Data WebSocket for market quotes in SymbolUpdate mode
      this.dataWebSocket.connect(fullAccessToken);
    });
  }

  /**
   * Clean up WebSocket event listeners
   */
  private cleanupWebSocketListeners(): void {
    if (this.messageUnsubscribe) {
      this.messageUnsubscribe();
      this.messageUnsubscribe = undefined;
    }
    if (this.connectUnsubscribe) {
      this.connectUnsubscribe();
      this.connectUnsubscribe = undefined;
    }
    if (this.errorUnsubscribe) {
      this.errorUnsubscribe();
      this.errorUnsubscribe = undefined;
    }
    if (this.closeUnsubscribe) {
      this.closeUnsubscribe();
      this.closeUnsubscribe = undefined;
    }
  }

  subscribeOrders(callback: (order: UnifiedOrder) => void): void {
    this.fyersAdapter.websocket.onOrderUpdate((fyersOrder) => callback(this.mapFyersOrder(fyersOrder as any)));
  }

  subscribePositions(callback: (position: UnifiedPosition) => void): void {
    this.fyersAdapter.websocket.onPositionUpdate((fyersPos) => {
      callback({
        id: fyersPos.symbol, brokerId: 'fyers', symbol: fyersPos.symbol, exchange: '', product: ProductType.MIS,
        quantity: fyersPos.netQty, averagePrice: fyersPos.avgPrice, lastPrice: fyersPos.ltp, pnl: fyersPos.pl,
        realizedPnl: 0, unrealizedPnl: fyersPos.pl, side: fyersPos.side === 1 ? OrderSide.BUY : OrderSide.SELL,
        value: fyersPos.netQty * fyersPos.ltp,
      });
    });
  }

  subscribeQuotes(symbols: Array<{ symbol: string; exchange: string }>, callback: (quote: UnifiedQuote) => void): void {
    // Format symbols for Fyers: "NSE:SBIN-EQ"
    const fyersSymbols = symbols.map(s => `${s.exchange}:${s.symbol}`);

    console.log(`[FyersStandardAdapter] Subscribing to ${fyersSymbols.length} symbols in SymbolUpdate mode (full OHLCV)`);

    // Store callback for each symbol
    fyersSymbols.forEach(fyersSymbol => {
      this.quoteCallbacks.set(fyersSymbol, callback);
    });

    // Subscribe via Data WebSocket in SymbolUpdate mode - will get full OHLCV data
    this.dataWebSocket.subscribe(fyersSymbols);

    console.log(`[FyersStandardAdapter] ✓ Subscription queued for ${fyersSymbols.length} symbols`);
  }

  /**
   * Fetch initial OHLC data from REST API
   * WebSocket tick data doesn't include OHLC, so we need to fetch it separately
   */
  private async fetchInitialQuotes(
    symbols: Array<{ symbol: string; exchange: string }>,
    callback: (quote: UnifiedQuote) => void
  ): Promise<void> {
    // Fetch quotes in batches of 50 (Fyers API limit)
    const batchSize = 50;
    for (let i = 0; i < symbols.length; i += batchSize) {
      const batch = symbols.slice(i, i + batchSize);
      const fyersSymbols = batch.map(s => `${s.exchange}:${s.symbol}`);

      try {
        const quotesMap = await this.fyersAdapter.market.getQuotes(fyersSymbols);

        console.log('[FyersStandardAdapter] getQuotes returned:', quotesMap);

        if (quotesMap && Object.keys(quotesMap).length > 0) {
          // Process each quote in the response
          Object.entries(quotesMap).forEach(([symbol, quoteData]: [string, any]) => {
            const parts = symbol.split(':');
            const exchange = parts[0] || 'NSE';
            const symbolName = parts[1] || symbol;

            const quote = this.mapFyersQuote(quoteData, symbolName, exchange);

            console.log(`[FyersStandardAdapter] Caching quote for ${symbol}:`, quote);

            // Cache the quote for merging with WebSocket tick updates
            this.quoteCache.set(symbol, quote);

            callback(quote);
          });
        }
      } catch (error) {
        console.error(`[FyersStandardAdapter] Failed to fetch quote batch:`, error);
      }
    }
  }

  subscribeMarketDepth(symbol: string, exchange: string, callback: (depth: UnifiedMarketDepth) => void): void {
    if (!this.dataWebSocket.getConnectionStatus()) {
      console.error('[FyersStandardAdapter] WebSocket not connected, cannot subscribe to market depth');
      return;
    }

    const fyersSymbol = `${exchange}:${symbol}`;
    this.depthCallbacks.set(fyersSymbol, callback);

    // Subscribe for market depth (SymbolUpdate mode includes depth data)
    this.dataWebSocket.subscribe([fyersSymbol]);

    console.log(`[FyersStandardAdapter] Subscribed to ${fyersSymbol} for market depth`);
  }

  unsubscribeQuotes(symbols: Array<{ symbol: string; exchange: string }>): void {
    const fyersSymbols = symbols.map(s => `${s.exchange}:${s.symbol}`);

    // Remove callbacks
    fyersSymbols.forEach(fyersSymbol => {
      this.quoteCallbacks.delete(fyersSymbol);
    });

    // Unsubscribe from Data WebSocket
    if (this.dataWebSocket.getConnectionStatus()) {
      this.dataWebSocket.unsubscribe(fyersSymbols);
    }

    console.log(`[FyersStandardAdapter] Unsubscribed from ${fyersSymbols.length} symbols`);
  }

  async disconnectWebSocket(): Promise<void> {
    // Clean up event listeners first
    this.cleanupWebSocketListeners();

    // Clear all callbacks
    this.quoteCallbacks.clear();
    this.depthCallbacks.clear();
    this.quoteCache.clear();

    // Disconnect the WebSocket
    this.dataWebSocket.disconnect();

    console.log('[FyersStandardAdapter] Data WebSocket disconnected and cleaned up');
  }

  async calculateMargin(order: OrderRequest): Promise<{ required: number; available: number; leverage?: number }> {
    try {
      const result = await this.fyersAdapter.trading.calculateSpanMargin({
        symbol: order.symbol, qty: order.quantity, side: order.side as any, type: order.type as any,
        productType: this.mapProductType(order.product) as any, limitPrice: order.price,
      });
      return { required: result.margin || 0, available: result.availableMargin || 0 };
    } catch { return { required: 0, available: 0 }; }
  }

  async searchSymbols(query: string, exchange?: string): Promise<Array<{ symbol: string; exchange: string; name: string; instrumentType?: string }>> {
    return [];
  }

  private mapOrderType(type: OrderType): number {
    const map = { [OrderType.MARKET]: 2, [OrderType.LIMIT]: 1, [OrderType.STOP_LOSS]: 3, [OrderType.STOP_LOSS_MARKET]: 4 };
    return map[type] || 1;
  }

  private mapProductType(product: ProductType): string {
    const map = { [ProductType.CNC]: 'CNC', [ProductType.MIS]: 'INTRADAY', [ProductType.INTRADAY]: 'INTRADAY', [ProductType.NRML]: 'MARGIN', [ProductType.MARGIN]: 'MARGIN' };
    return map[product] || 'CNC';
  }

  private mapTimeInterval(interval: TimeInterval): string {
    const map: Record<TimeInterval, string> = { '1m': '1', '3m': '3', '5m': '5', '15m': '15', '30m': '30', '1h': '60', '1d': 'D' };
    return map[interval] || '1';
  }

  private mapFyersOrder(order: FyersOrder): UnifiedOrder {
    return {
      id: order.id, brokerId: 'fyers', symbol: order.symbol, exchange: order.exchange.toString(),
      side: order.side === 1 ? OrderSide.BUY : OrderSide.SELL, type: this.mapFyersOrderType(order.type),
      quantity: order.qty, price: order.limitPrice, product: ProductType.CNC, validity: Validity.DAY,
      status: this.mapFyersOrderStatus(order.status), filledQuantity: order.filledQty, pendingQuantity: order.qty - order.filledQty,
      averagePrice: order.filledQty > 0 ? order.limitPrice : 0, orderTime: new Date(order.orderDateTime), message: order.description,
    };
  }

  private mapFyersOrderType(type: number): OrderType {
    const map = { 1: OrderType.LIMIT, 2: OrderType.MARKET, 3: OrderType.STOP_LOSS, 4: OrderType.STOP_LOSS_MARKET };
    return map[type as keyof typeof map] || OrderType.LIMIT;
  }

  private mapFyersOrderStatus(status: number): OrderStatus {
    if (status === 6) return OrderStatus.COMPLETE;
    if (status === 7) return OrderStatus.CANCELLED;
    if (status === 1 || status === 2) return OrderStatus.PENDING;
    return OrderStatus.OPEN;
  }

  private mapFyersPosition(position: FyersPosition): UnifiedPosition {
    return {
      id: position.id, brokerId: 'fyers', symbol: position.symbol, exchange: '', product: ProductType.MIS,
      quantity: position.netQty, averagePrice: position.netAvg, lastPrice: position.ltp, pnl: position.pl,
      realizedPnl: position.realized_profit, unrealizedPnl: position.unrealized_profit,
      side: position.netQty > 0 ? OrderSide.BUY : OrderSide.SELL, value: Math.abs(position.netQty * position.ltp),
    };
  }

  private mapFyersHolding(holding: FyersHolding): UnifiedHolding {
    return {
      id: holding.id.toString(), brokerId: 'fyers', symbol: holding.symbol, exchange: '', isin: holding.isin,
      quantity: holding.quantity, averagePrice: holding.costPrice, lastPrice: holding.ltp, pnl: holding.pl,
      pnlPercentage: (holding.pl / (holding.costPrice * holding.quantity)) * 100, currentValue: holding.marketVal,
      investedValue: holding.costPrice * holding.quantity,
    };
  }

  private mapFyersQuote(quote: FyersQuote, symbol: string, exchange: string): UnifiedQuote {
    const v = typeof quote.v === 'object' ? quote.v : null;
    return {
      symbol, exchange, lastPrice: quote.lp || quote.ltp || (v?.lp || 0), open: quote.o || (v?.open_price || 0),
      high: quote.h || (v?.high_price || 0), low: quote.l || (v?.low_price || 0), close: v?.prev_close_price || 0,
      volume: (typeof quote.volume === 'number' ? quote.volume : (v?.volume || 0)), change: quote.ch || (v?.ch || 0),
      changePercent: quote.chp || (v?.chp || 0), timestamp: new Date(), bid: v?.bid, ask: v?.ask,
    };
  }

  private mapDataSocketToQuote(message: DataSocketMessage): UnifiedQuote {
    // Parse symbol to extract exchange and symbol name
    const parts = message.symbol.split(':');
    const exchange = parts[0] || 'NSE';
    const symbolName = parts[1] || message.symbol;

    // DEBUG: Log only for RELIANCE to reduce noise
    if (message.symbol === 'NSE:RELIANCE-EQ') {
      console.log('[FyersStandardAdapter] Raw WebSocket Message for RELIANCE:', JSON.stringify(message, null, 2));
    }

    // Extract all data from WebSocket message (no caching needed - WebSocket has everything)
    const lastPrice = message.ltp || message.lp || 0;
    const open = message.open_price || message.o || 0;
    const high = message.high_price || message.h || 0;
    const low = message.low_price || message.l || 0;
    const prevClose = message.prev_close_price || message.prev_close || message.pc || message.close_price || message.c || 0;
    const volume = message.volume || message.v || message.vol_traded_today || message.volume_traded_today || 0;
    const change = message.ch || 0;
    const changePercent = message.chp || 0;
    const bid = message.bid || message.bid_price || undefined;
    const ask = message.ask || message.ask_price || undefined;
    const timestamp = message.timestamp
      ? new Date(message.timestamp * 1000)
      : (message.last_traded_time || message.ltt)
        ? new Date((message.last_traded_time || message.ltt)! * 1000)
        : new Date();

    const quote: UnifiedQuote = {
      symbol: symbolName,
      exchange,
      lastPrice,
      open,
      high,
      low,
      close: prevClose, // Using prev_close_price as close
      volume,
      change,
      changePercent,
      timestamp,
      bid,
      ask,
      bidQty: message.bid_size,
      askQty: message.ask_size,
      lastTradedQty: message.last_traded_qty,
      totalBuyQty: message.tot_buy_qty,
      totalSellQty: message.tot_sell_qty,
    };

    // DEBUG: Log quote for RELIANCE
    if (message.symbol === 'NSE:RELIANCE-EQ') {
      console.log('[FyersStandardAdapter] Parsed Quote for RELIANCE:', {
        open: quote.open,
        high: quote.high,
        low: quote.low,
        close: quote.close,
        lastPrice: quote.lastPrice,
        volume: quote.volume,
        change: quote.change,
        changePercent: quote.changePercent
      });
    }

    return quote;
  }

  private mapDataSocketToDepth(message: DataSocketMessage): UnifiedMarketDepth {
    const parts = message.symbol.split(':');
    const exchange = parts[0] || 'NSE';
    const symbol = parts[1] || message.symbol;

    return {
      symbol,
      exchange,
      bids: message.market_depth?.buy?.map(b => ({
        price: b.price,
        quantity: b.qty,
        orders: b.orders,
      })) || [],
      asks: message.market_depth?.sell?.map(s => ({
        price: s.price,
        quantity: s.qty,
        orders: s.orders,
      })) || [],
      lastPrice: message.ltp || message.lp || 0,
      timestamp: new Date(),
    };
  }

  private mapFyersMarketDepth(depth: FyersMarketDepth, symbol: string, exchange: string): UnifiedMarketDepth {
    return {
      symbol, exchange, bids: depth.bids.map(b => ({ price: b.price, quantity: b.volume, orders: b.ord })),
      asks: depth.ask.map(a => ({ price: a.price, quantity: a.volume, orders: a.ord })), lastPrice: depth.ltp, timestamp: new Date(),
    };
  }

  private mapFyersCandle(candle: any): Candle {
    // Fyers returns array: [timestamp, open, high, low, close, volume]
    if (Array.isArray(candle)) {
      return {
        timestamp: new Date(candle[0] * 1000),
        open: candle[1],
        high: candle[2],
        low: candle[3],
        close: candle[4],
        volume: candle[5]
      };
    }
    // Fallback for object format
    return {
      timestamp: candle?.timestamp ? new Date(candle.timestamp * 1000) : new Date(),
      open: candle?.open || 0,
      high: candle?.high || 0,
      low: candle?.low || 0,
      close: candle?.close || 0,
      volume: candle?.volume || 0
    };
  }

  /**
   * Forward Fyers WebSocket message to Rust monitoring service
   * This enables backend monitoring, alerts, and analytics
   */
  private async forwardToMonitoringService(message: DataSocketMessage): Promise<void> {
    try {
      // Import Tauri emit dynamically to avoid issues if not in Tauri context
      const { emit } = await import('@tauri-apps/api/event');

      // Map to monitoring service format
      const tickerData = {
        provider: 'fyers',
        symbol: message.symbol,
        price: message.ltp || 0,
        volume: message.vol_traded_today,
        bid: message.bid_price,
        ask: message.ask_price,
        bid_size: message.bid_size,
        ask_size: message.ask_size,
        high: message.high_price,
        low: message.low_price,
        open: message.open_price,
        close: message.prev_close_price,
        change: message.ch,
        change_percent: message.chp,
        timestamp: Date.now(),
      };

      // Emit to Rust backend
      await emit('fyers_ticker', tickerData);
    } catch (error) {
      // Silently fail if not in Tauri context (e.g., dev mode)
      console.debug('[FyersStandardAdapter] Could not emit to monitoring service:', error);
    }
  }
}
