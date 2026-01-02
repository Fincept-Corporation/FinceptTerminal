/**
 * Alpaca Markets API Adapter
 * Implements full Alpaca Trading API integration
 * Supports both paper trading and live trading
 *
 * API Documentation: https://docs.alpaca.markets/
 * Base URLs:
 * - Paper Trading: https://paper-api.alpaca.markets
 * - Live Trading: https://api.alpaca.markets
 * - Market Data: https://data.alpaca.markets
 */

import { BaseBrokerAdapter } from './BaseBrokerAdapter';
import {
  BrokerType,
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
  BrokerCredentials,
  AuthStatus,
  OrderSide,
  OrderType,
  OrderStatus,
  ProductType,
  Validity,
  MarketDepthLevel,
} from '../core/types';

/**
 * Alpaca-specific interfaces
 */
interface AlpacaConfig {
  apiKey: string;
  apiSecret: string;
  isPaper: boolean; // true for paper trading, false for live
}

interface AlpacaAccount {
  id: string;
  account_number: string;
  status: string;
  currency: string;
  buying_power: string;
  cash: string;
  portfolio_value: string;
  pattern_day_trader: boolean;
  trading_blocked: boolean;
  transfers_blocked: boolean;
  account_blocked: boolean;
  created_at: string;
  trade_suspended_by_user: boolean;
  multiplier: string;
  shorting_enabled: boolean;
  equity: string;
  last_equity: string;
  long_market_value: string;
  short_market_value: string;
  initial_margin: string;
  maintenance_margin: string;
  last_maintenance_margin: string;
  sma: string;
  daytrade_count: number;
}

interface AlpacaOrder {
  id: string;
  client_order_id: string;
  created_at: string;
  updated_at: string;
  submitted_at: string;
  filled_at: string | null;
  expired_at: string | null;
  canceled_at: string | null;
  failed_at: string | null;
  replaced_at: string | null;
  replaced_by: string | null;
  replaces: string | null;
  asset_id: string;
  symbol: string;
  asset_class: string;
  notional: string | null;
  qty: string;
  filled_qty: string;
  filled_avg_price: string | null;
  order_class: string;
  order_type: 'market' | 'limit' | 'stop' | 'stop_limit' | 'trailing_stop';
  type: 'market' | 'limit' | 'stop' | 'stop_limit' | 'trailing_stop';
  side: 'buy' | 'sell';
  time_in_force: 'day' | 'gtc' | 'ioc' | 'fok' | 'opg' | 'cls';
  limit_price: string | null;
  stop_price: string | null;
  status: 'new' | 'accepted' | 'pending_new' | 'accepted_for_bidding' | 'stopped' | 'rejected' | 'suspended' | 'calculated' | 'filled' | 'done_for_day' | 'canceled' | 'expired' | 'replaced' | 'pending_cancel' | 'pending_replace';
  extended_hours: boolean;
  legs: any | null;
  trail_percent: string | null;
  trail_price: string | null;
  hwm: string | null;
}

interface AlpacaPosition {
  asset_id: string;
  symbol: string;
  exchange: string;
  asset_class: string;
  avg_entry_price: string;
  qty: string;
  side: 'long' | 'short';
  market_value: string;
  cost_basis: string;
  unrealized_pl: string;
  unrealized_plpc: string;
  unrealized_intraday_pl: string;
  unrealized_intraday_plpc: string;
  current_price: string;
  lastday_price: string;
  change_today: string;
}

interface AlpacaQuote {
  symbol: string;
  bid_price: number;
  bid_size: number;
  ask_price: number;
  ask_size: number;
  timestamp: string;
}

interface AlpacaTrade {
  symbol: string;
  price: number;
  size: number;
  timestamp: string;
  conditions: string[];
  exchange: string;
}

interface AlpacaBar {
  t: string; // timestamp
  o: number; // open
  h: number; // high
  l: number; // low
  c: number; // close
  v: number; // volume
  n: number; // trade count
  vw: number; // volume weighted average price
}

export class AlpacaAdapter extends BaseBrokerAdapter {
  private apiKey: string = '';
  private apiSecret: string = '';
  private isPaper: boolean = true;
  private baseUrl: string = '';
  private dataUrl: string = 'https://data.alpaca.markets';
  private isAuth: boolean = false;
  private authStatus: AuthStatus;
  private accountInfo: AlpacaAccount | null = null;

  constructor() {
    super('alpaca');
    this.authStatus = {
      brokerId: 'alpaca',
      status: BrokerStatus.DISCONNECTED,
      authenticated: false,
    };
  }

  // ==================== BROKER CAPABILITIES ====================

  getSupportedExchanges(): string[] {
    // Alpaca supports US equity markets
    return ['NYSE', 'NASDAQ', 'AMEX', 'ARCA', 'US'];
  }

  // ==================== AUTHENTICATION ====================

  async initialize(credentials: BrokerCredentials): Promise<boolean> {
    try {
      this.credentials = credentials;
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret;

      // Determine if paper or live based on additional data
      this.isPaper = credentials.additionalData?.isPaper ?? true;

      // Set base URL based on paper/live mode
      this.baseUrl = this.isPaper
        ? 'https://paper-api.alpaca.markets'
        : 'https://api.alpaca.markets';

      console.log(`[Alpaca] Initializing in ${this.isPaper ? 'PAPER' : 'LIVE'} trading mode`);
      console.log(`[Alpaca] Base URL: ${this.baseUrl}`);

      this.setStatus(BrokerStatus.CONNECTING);

      // Test authentication by fetching account info
      const authenticated = await this.testAuthentication();

      if (authenticated) {
        this.isAuth = true;
        this.setStatus(BrokerStatus.AUTHENTICATED);
        this.authStatus = {
          brokerId: 'alpaca',
          status: BrokerStatus.AUTHENTICATED,
          authenticated: true,
          lastAuth: new Date(),
        };
        console.log('[Alpaca] ✅ Authentication successful');
        return true;
      } else {
        throw new Error('Authentication failed');
      }
    } catch (error: any) {
      console.error('[Alpaca] ❌ Initialization failed:', error);
      this.setStatus(BrokerStatus.ERROR);
      this.authStatus = {
        brokerId: 'alpaca',
        status: BrokerStatus.ERROR,
        authenticated: false,
        error: error.message,
      };
      return false;
    }
  }

  private async testAuthentication(): Promise<boolean> {
    try {
      const account = await this.fetchAccount();
      this.accountInfo = account;
      return true;
    } catch (error) {
      console.error('[Alpaca] Authentication test failed:', error);
      return false;
    }
  }

  async authenticate(): Promise<AuthStatus> {
    if (this.isAuth) {
      return this.authStatus;
    }

    const success = await this.initialize(this.credentials!);
    return this.authStatus;
  }

  isAuthenticated(): boolean {
    return this.isAuth;
  }

  getAuthStatus(): AuthStatus {
    return this.authStatus;
  }

  async refreshToken(): Promise<boolean> {
    // Alpaca uses API key authentication, no token refresh needed
    return this.isAuth;
  }

  async disconnect(): Promise<void> {
    this.isAuth = false;
    this.setStatus(BrokerStatus.DISCONNECTED);
    this.authStatus = {
      brokerId: 'alpaca',
      status: BrokerStatus.DISCONNECTED,
      authenticated: false,
    };
    console.log('[Alpaca] Disconnected');
  }

  // ==================== HTTP REQUEST HELPER ====================

  private async makeRequest<T>(
    endpoint: string,
    method: 'GET' | 'POST' | 'PATCH' | 'DELETE' = 'GET',
    body?: any,
    useDataApi: boolean = false
  ): Promise<T> {
    const baseUrl = useDataApi ? this.dataUrl : this.baseUrl;
    const url = `${baseUrl}${endpoint}`;

    const headers: Record<string, string> = {
      'APCA-API-KEY-ID': this.apiKey,
      'APCA-API-SECRET-KEY': this.apiSecret,
      'Content-Type': 'application/json',
    };

    const options: RequestInit = {
      method,
      headers,
    };

    if (body && (method === 'POST' || method === 'PATCH')) {
      options.body = JSON.stringify(body);
    }

    try {
      console.log(`[Alpaca] ${method} ${url}`);
      const response = await fetch(url, options);

      if (!response.ok) {
        const errorText = await response.text();
        let errorMessage = `HTTP ${response.status}: ${response.statusText}`;
        try {
          const errorJson = JSON.parse(errorText);
          errorMessage = errorJson.message || errorMessage;
        } catch {
          errorMessage = errorText || errorMessage;
        }
        throw new Error(errorMessage);
      }

      // Handle empty responses (e.g., DELETE operations)
      const text = await response.text();
      if (!text) {
        return {} as T;
      }

      return JSON.parse(text) as T;
    } catch (error: any) {
      console.error(`[Alpaca] Request failed:`, error);
      throw error;
    }
  }

  // ==================== ACCOUNT API ====================

  private async fetchAccount(): Promise<AlpacaAccount> {
    return this.makeRequest<AlpacaAccount>('/v2/account');
  }

  async getFunds(): Promise<UnifiedFunds> {
    const account = await this.fetchAccount();
    this.accountInfo = account;

    return {
      brokerId: 'alpaca',
      availableCash: parseFloat(account.cash),
      usedMargin: parseFloat(account.initial_margin || '0'),
      availableMargin: parseFloat(account.buying_power),
      totalCollateral: parseFloat(account.portfolio_value),
      equity: parseFloat(account.equity),
      timestamp: new Date(),
    };
  }

  async getProfile(): Promise<UnifiedProfile> {
    const account = await this.fetchAccount();

    return {
      brokerId: 'alpaca',
      userId: account.account_number,
      name: account.id,
      email: '', // Not available in API response
      exchanges: ['NASDAQ', 'NYSE', 'ARCA', 'AMEX'],
      products: ['stocks', 'etfs'],
    };
  }

  // ==================== TRADING OPERATIONS ====================

  async placeOrder(order: OrderRequest): Promise<OrderResponse> {
    try {
      // Convert unified order to Alpaca format
      const alpacaOrder: any = {
        symbol: order.symbol,
        qty: order.quantity.toString(),
        side: order.side.toLowerCase(),
        type: this.convertOrderType(order.type),
        time_in_force: this.convertValidity(order.validity || Validity.DAY),
      };

      // Add price for limit orders
      if (order.type === OrderType.LIMIT && order.price) {
        alpacaOrder.limit_price = order.price.toString();
      }

      // Add stop price for stop orders
      if (order.triggerPrice) {
        alpacaOrder.stop_price = order.triggerPrice.toString();
      }

      // Extended hours trading (optional)
      if (order.product === ProductType.INTRADAY) {
        alpacaOrder.extended_hours = false;
      }

      // Client order ID (optional tag)
      if (order.tag) {
        alpacaOrder.client_order_id = order.tag;
      }

      console.log('[Alpaca] Placing order:', alpacaOrder);

      const result = await this.makeRequest<AlpacaOrder>(
        '/v2/orders',
        'POST',
        alpacaOrder
      );

      return {
        success: true,
        orderId: result.id,
        message: `Order placed successfully. Status: ${result.status}`,
        data: result,
      };
    } catch (error: any) {
      console.error('[Alpaca] Place order failed:', error);
      return {
        success: false,
        message: error.message || 'Failed to place order',
      };
    }
  }

  async modifyOrder(orderId: string, updates: Partial<OrderRequest>): Promise<OrderResponse> {
    try {
      const patchData: any = {};

      if (updates.quantity) {
        patchData.qty = updates.quantity.toString();
      }

      if (updates.price) {
        patchData.limit_price = updates.price.toString();
      }

      if (updates.triggerPrice) {
        patchData.stop_price = updates.triggerPrice.toString();
      }

      if (updates.validity) {
        patchData.time_in_force = this.convertValidity(updates.validity);
      }

      const result = await this.makeRequest<AlpacaOrder>(
        `/v2/orders/${orderId}`,
        'PATCH',
        patchData
      );

      return {
        success: true,
        orderId: result.id,
        message: 'Order modified successfully',
        data: result,
      };
    } catch (error: any) {
      console.error('[Alpaca] Modify order failed:', error);
      return {
        success: false,
        message: error.message || 'Failed to modify order',
      };
    }
  }

  async cancelOrder(orderId: string): Promise<OrderResponse> {
    try {
      await this.makeRequest<void>(`/v2/orders/${orderId}`, 'DELETE');

      return {
        success: true,
        orderId,
        message: 'Order cancelled successfully',
      };
    } catch (error: any) {
      console.error('[Alpaca] Cancel order failed:', error);
      return {
        success: false,
        message: error.message || 'Failed to cancel order',
      };
    }
  }

  async getOrders(): Promise<UnifiedOrder[]> {
    try {
      // Get all orders (both open and closed from today)
      const orders = await this.makeRequest<AlpacaOrder[]>(
        '/v2/orders?status=all&limit=500'
      );

      return orders.map(order => this.convertToUnifiedOrder(order));
    } catch (error: any) {
      console.error('[Alpaca] Get orders failed:', error);
      return [];
    }
  }

  async getOrder(orderId: string): Promise<UnifiedOrder> {
    const order = await this.makeRequest<AlpacaOrder>(`/v2/orders/${orderId}`);
    return this.convertToUnifiedOrder(order);
  }

  async getOrderHistory(orderId: string): Promise<UnifiedOrder[]> {
    // Alpaca doesn't have a separate order history endpoint
    // Return the single order wrapped in an array
    const order = await this.getOrder(orderId);
    return [order];
  }

  // ==================== POSITIONS & HOLDINGS ====================

  async getPositions(): Promise<UnifiedPosition[]> {
    try {
      const positions = await this.makeRequest<AlpacaPosition[]>('/v2/positions');

      return positions.map(position => this.convertToUnifiedPosition(position));
    } catch (error: any) {
      console.error('[Alpaca] Get positions failed:', error);
      return [];
    }
  }

  async getHoldings(): Promise<UnifiedHolding[]> {
    // In Alpaca, positions and holdings are the same (no separate holdings API)
    const positions = await this.getPositions();

    return positions.map(position => ({
      id: `${position.symbol}-${position.brokerId}`,
      brokerId: position.brokerId,
      symbol: position.symbol,
      exchange: position.exchange,
      quantity: position.quantity,
      averagePrice: position.averagePrice,
      lastPrice: position.lastPrice,
      pnl: position.unrealizedPnl,
      pnlPercentage: position.unrealizedPnl / (position.averagePrice * position.quantity) * 100,
      currentValue: position.value,
      investedValue: position.averagePrice * position.quantity,
    }));
  }

  // ==================== MARKET DATA ====================

  async getQuote(symbol: string, exchange: string): Promise<UnifiedQuote> {
    try {
      // Get latest quote from market data API
      const quote = await this.makeRequest<{ quote: AlpacaQuote }>(
        `/v2/stocks/${symbol}/quotes/latest`,
        'GET',
        undefined,
        true // use data API
      );

      // Get latest trade for last price
      const trade = await this.makeRequest<{ trade: AlpacaTrade }>(
        `/v2/stocks/${symbol}/trades/latest`,
        'GET',
        undefined,
        true
      );

      // Get snapshot for OHLC and volume
      const snapshot = await this.makeRequest<any>(
        `/v2/stocks/${symbol}/snapshot`,
        'GET',
        undefined,
        true
      );

      const lastPrice = trade.trade.price;
      const prevClose = snapshot.prevDailyBar?.c || lastPrice;
      const change = lastPrice - prevClose;
      const changePercent = (change / prevClose) * 100;

      return {
        symbol,
        exchange,
        lastPrice,
        open: snapshot.dailyBar?.o || lastPrice,
        high: snapshot.dailyBar?.h || lastPrice,
        low: snapshot.dailyBar?.l || lastPrice,
        close: prevClose,
        volume: snapshot.dailyBar?.v || 0,
        change,
        changePercent,
        timestamp: new Date(trade.trade.timestamp),
        bid: quote.quote.bid_price,
        ask: quote.quote.ask_price,
        bidQty: quote.quote.bid_size,
        askQty: quote.quote.ask_size,
      };
    } catch (error: any) {
      console.error('[Alpaca] Get quote failed:', error);
      throw error;
    }
  }

  async getQuotes(symbols: Array<{ symbol: string; exchange: string }>): Promise<UnifiedQuote[]> {
    const quotes: UnifiedQuote[] = [];

    // Fetch quotes in parallel
    await Promise.all(
      symbols.map(async ({ symbol, exchange }) => {
        try {
          const quote = await this.getQuote(symbol, exchange);
          quotes.push(quote);
        } catch (error) {
          console.error(`[Alpaca] Failed to get quote for ${symbol}:`, error);
        }
      })
    );

    return quotes;
  }

  async getMarketDepth(symbol: string, exchange: string): Promise<UnifiedMarketDepth> {
    // Alpaca doesn't provide full order book via REST API
    // We'll use the latest quote as a simplified depth
    const quote = await this.getQuote(symbol, exchange);

    const bids: MarketDepthLevel[] = quote.bid && quote.bidQty ? [{
      price: quote.bid,
      quantity: quote.bidQty,
      orders: 1,
    }] : [];

    const asks: MarketDepthLevel[] = quote.ask && quote.askQty ? [{
      price: quote.ask,
      quantity: quote.askQty,
      orders: 1,
    }] : [];

    return {
      symbol,
      exchange,
      bids,
      asks,
      lastPrice: quote.lastPrice,
      timestamp: quote.timestamp,
    };
  }

  async getHistoricalData(
    symbol: string,
    exchange: string,
    interval: TimeInterval,
    from: Date,
    to: Date
  ): Promise<Candle[]> {
    try {
      const timeframe = this.convertTimeInterval(interval);
      const start = from.toISOString();
      const end = to.toISOString();

      const response = await this.makeRequest<{ bars: AlpacaBar[] }>(
        `/v2/stocks/${symbol}/bars?timeframe=${timeframe}&start=${start}&end=${end}&limit=10000`,
        'GET',
        undefined,
        true // use data API
      );

      return response.bars.map(bar => ({
        timestamp: new Date(bar.t),
        open: bar.o,
        high: bar.h,
        low: bar.l,
        close: bar.c,
        volume: bar.v,
      }));
    } catch (error: any) {
      console.error('[Alpaca] Get historical data failed:', error);
      return [];
    }
  }

  // ==================== WEBSOCKET OPERATIONS (Placeholder) ====================

  async connectWebSocket(): Promise<void> {
    // WebSocket implementation would go here
    // Alpaca provides WebSocket API for real-time data
    console.log('[Alpaca] WebSocket connection not yet implemented');
  }

  subscribeOrders(callback: (order: UnifiedOrder) => void): void {
    console.log('[Alpaca] Order subscription not yet implemented');
  }

  subscribePositions(callback: (position: UnifiedPosition) => void): void {
    console.log('[Alpaca] Position subscription not yet implemented');
  }

  subscribeQuotes(
    symbols: Array<{ symbol: string; exchange: string }>,
    callback: (quote: UnifiedQuote) => void
  ): void {
    console.log('[Alpaca] Quote subscription not yet implemented');
  }

  subscribeMarketDepth(
    symbol: string,
    exchange: string,
    callback: (depth: UnifiedMarketDepth) => void
  ): void {
    console.log('[Alpaca] Market depth subscription not yet implemented');
  }

  unsubscribeQuotes(symbols: Array<{ symbol: string; exchange: string }>): void {
    console.log('[Alpaca] Quote unsubscription not yet implemented');
  }

  async disconnectWebSocket(): Promise<void> {
    console.log('[Alpaca] WebSocket disconnection not yet implemented');
  }

  // ==================== UTILITY METHODS ====================

  async calculateMargin(order: OrderRequest): Promise<{ required: number; available: number; leverage?: number }> {
    const funds = await this.getFunds();
    const estimatedCost = (order.price || 0) * order.quantity;

    return {
      required: estimatedCost,
      available: funds.availableMargin,
      leverage: parseFloat(this.accountInfo?.multiplier || '1'),
    };
  }

  async searchSymbols(query: string, exchange?: string): Promise<Array<{
    symbol: string;
    exchange: string;
    name: string;
    instrumentType?: string;
  }>> {
    // Alpaca doesn't have a search endpoint in the free tier
    // This would require the trading API's asset endpoint
    try {
      const assets = await this.makeRequest<Array<{
        id: string;
        class: string;
        exchange: string;
        symbol: string;
        name: string;
        status: string;
        tradable: boolean;
      }>>('/v2/assets?status=active&asset_class=us_equity');

      const filtered = assets.filter(asset =>
        asset.tradable &&
        (asset.symbol.toLowerCase().includes(query.toLowerCase()) ||
         asset.name.toLowerCase().includes(query.toLowerCase()))
      );

      return filtered.slice(0, 20).map(asset => ({
        symbol: asset.symbol,
        exchange: asset.exchange,
        name: asset.name,
        instrumentType: asset.class,
      }));
    } catch (error) {
      console.error('[Alpaca] Search symbols failed:', error);
      return [];
    }
  }

  // ==================== CONVERSION HELPERS ====================

  private convertOrderType(type: OrderType): string {
    switch (type) {
      case OrderType.MARKET:
        return 'market';
      case OrderType.LIMIT:
        return 'limit';
      case OrderType.STOP_LOSS:
      case OrderType.STOP_LOSS_MARKET:
        return 'stop';
      default:
        return 'market';
    }
  }

  private convertValidity(validity: Validity): string {
    switch (validity) {
      case Validity.DAY:
        return 'day';
      case Validity.IOC:
        return 'ioc';
      default:
        return 'day';
    }
  }

  private convertTimeInterval(interval: TimeInterval): string {
    const mapping: Record<TimeInterval, string> = {
      '1m': '1Min',
      '3m': '3Min',
      '5m': '5Min',
      '15m': '15Min',
      '30m': '30Min',
      '1h': '1Hour',
      '1d': '1Day',
    };
    return mapping[interval] || '1Min';
  }

  private convertOrderStatus(status: string): OrderStatus {
    const statusMap: Record<string, OrderStatus> = {
      'new': OrderStatus.PENDING,
      'accepted': OrderStatus.OPEN,
      'pending_new': OrderStatus.PENDING,
      'filled': OrderStatus.COMPLETE,
      'done_for_day': OrderStatus.COMPLETE,
      'canceled': OrderStatus.CANCELLED,
      'expired': OrderStatus.EXPIRED,
      'replaced': OrderStatus.CANCELLED,
      'pending_cancel': OrderStatus.PENDING,
      'pending_replace': OrderStatus.PENDING,
      'rejected': OrderStatus.REJECTED,
      'suspended': OrderStatus.REJECTED,
      'stopped': OrderStatus.CANCELLED,
    };

    return statusMap[status] || OrderStatus.PENDING;
  }

  private convertToUnifiedOrder(alpacaOrder: AlpacaOrder): UnifiedOrder {
    return {
      id: alpacaOrder.id,
      brokerId: 'alpaca',
      symbol: alpacaOrder.symbol,
      exchange: 'US', // Alpaca primarily deals with US markets
      side: alpacaOrder.side.toUpperCase() as OrderSide,
      type: this.convertAlpacaOrderType(alpacaOrder.type),
      quantity: parseFloat(alpacaOrder.qty),
      price: alpacaOrder.limit_price ? parseFloat(alpacaOrder.limit_price) : undefined,
      triggerPrice: alpacaOrder.stop_price ? parseFloat(alpacaOrder.stop_price) : undefined,
      product: ProductType.CNC, // Alpaca doesn't have product types like Indian brokers
      validity: this.convertAlpacaValidity(alpacaOrder.time_in_force),
      status: this.convertOrderStatus(alpacaOrder.status),
      filledQuantity: parseFloat(alpacaOrder.filled_qty),
      pendingQuantity: parseFloat(alpacaOrder.qty) - parseFloat(alpacaOrder.filled_qty),
      averagePrice: alpacaOrder.filled_avg_price ? parseFloat(alpacaOrder.filled_avg_price) : 0,
      orderTime: new Date(alpacaOrder.created_at),
      updateTime: new Date(alpacaOrder.updated_at),
      tag: alpacaOrder.client_order_id,
    };
  }

  private convertAlpacaOrderType(type: string): OrderType {
    const typeMap: Record<string, OrderType> = {
      'market': OrderType.MARKET,
      'limit': OrderType.LIMIT,
      'stop': OrderType.STOP_LOSS_MARKET,
      'stop_limit': OrderType.STOP_LOSS,
      'trailing_stop': OrderType.STOP_LOSS_MARKET,
    };
    return typeMap[type] || OrderType.MARKET;
  }

  private convertAlpacaValidity(tif: string): Validity {
    const validityMap: Record<string, Validity> = {
      'day': Validity.DAY,
      'gtc': Validity.DAY,
      'ioc': Validity.IOC,
      'fok': Validity.IOC,
      'opg': Validity.DAY,
      'cls': Validity.DAY,
    };
    return validityMap[tif] || Validity.DAY;
  }

  private convertToUnifiedPosition(alpacaPosition: AlpacaPosition): UnifiedPosition {
    const avgPrice = parseFloat(alpacaPosition.avg_entry_price);
    const currentPrice = parseFloat(alpacaPosition.current_price);
    const quantity = parseFloat(alpacaPosition.qty);
    const unrealizedPnl = parseFloat(alpacaPosition.unrealized_pl);

    return {
      id: `${alpacaPosition.symbol}-alpaca`,
      brokerId: 'alpaca',
      symbol: alpacaPosition.symbol,
      exchange: alpacaPosition.exchange || 'US',
      product: ProductType.CNC,
      quantity: alpacaPosition.side === 'short' ? -quantity : quantity,
      averagePrice: avgPrice,
      lastPrice: currentPrice,
      pnl: unrealizedPnl,
      realizedPnl: 0, // Alpaca doesn't separate realized/unrealized in positions
      unrealizedPnl: unrealizedPnl,
      side: alpacaPosition.side === 'long' ? OrderSide.BUY : OrderSide.SELL,
      value: parseFloat(alpacaPosition.market_value),
    };
  }
}
