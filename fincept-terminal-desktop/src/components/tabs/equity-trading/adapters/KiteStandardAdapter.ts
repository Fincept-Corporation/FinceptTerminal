/**
 * Kite Standardized Adapter
 * Wraps existing Kite modules with BaseBrokerAdapter interface
 */

import { BaseBrokerAdapter } from './BaseBrokerAdapter';
import {
  BrokerCredentials, AuthStatus, BrokerStatus, UnifiedOrder, OrderRequest, OrderResponse,
  UnifiedPosition, UnifiedHolding, UnifiedQuote, UnifiedMarketDepth, Candle, TimeInterval,
  UnifiedFunds, UnifiedProfile, OrderSide, OrderType, OrderStatus, ProductType, Validity,
} from '../core/types';
import { KiteConnectAdapter } from '../../../../brokers/stocks/kite/adapter';
import type { KiteConfig, Order, Position, Holding, Quote, CandleData, OrderParams, MarginOrder, MarginResponse } from '../../../../brokers/stocks/kite/types';

export class KiteStandardAdapter extends BaseBrokerAdapter {
  private kiteAdapter: KiteConnectAdapter | null = null;
  private lastAuthTime?: Date;

  constructor() {
    super('kite');
  }

  // ==================== BROKER CAPABILITIES ====================

  getSupportedExchanges(): string[] {
    // Kite (Zerodha) supports Indian equity markets
    return ['NSE', 'BSE', 'MCX', 'NFO', 'CDS', 'BFO', 'BCD'];
  }

  async initialize(credentials: BrokerCredentials): Promise<boolean> {
    this.credentials = credentials;
    this.setStatus(BrokerStatus.CONNECTING);

    try {
      const config: KiteConfig = {
        apiKey: credentials.apiKey,
        apiSecret: credentials.apiSecret,
        accessToken: credentials.accessToken,
      };

      this.kiteAdapter = new KiteConnectAdapter(config);
      this.setStatus(BrokerStatus.CONNECTED);
      return true;
    } catch (error) {
      this.setStatus(BrokerStatus.ERROR);
      return false;
    }
  }

  async authenticate(): Promise<AuthStatus> {
    if (!this.kiteAdapter) {
      return { brokerId: 'kite', status: BrokerStatus.ERROR, authenticated: false, error: 'Not initialized' };
    }

    const isAuth = this.kiteAdapter.isAuthenticated();
    if (isAuth) {
      this.setStatus(BrokerStatus.AUTHENTICATED);
      this.lastAuthTime = new Date();
      return { brokerId: 'kite', status: BrokerStatus.AUTHENTICATED, authenticated: true, lastAuth: this.lastAuthTime };
    }

    return { brokerId: 'kite', status: BrokerStatus.ERROR, authenticated: false, error: 'No access token' };
  }

  isAuthenticated(): boolean {
    return this.kiteAdapter?.isAuthenticated() || false;
  }

  getAuthStatus(): AuthStatus {
    return { brokerId: 'kite', status: this.status, authenticated: this.isAuthenticated(), lastAuth: this.lastAuthTime };
  }

  async refreshToken(): Promise<boolean> {
    return this.isAuthenticated();
  }

  async disconnect(): Promise<void> {
    this.setStatus(BrokerStatus.DISCONNECTED);
    this.kiteAdapter = null;
  }

  async placeOrder(order: OrderRequest): Promise<OrderResponse> {
    if (!this.kiteAdapter || !this.isAuthenticated()) return { success: false, message: 'Not authenticated' };

    try {
      const kiteOrder: OrderParams = {
        tradingsymbol: order.symbol,
        exchange: order.exchange,
        transaction_type: order.side === OrderSide.BUY ? 'BUY' : 'SELL',
        order_type: this.mapOrderType(order.type),
        quantity: order.quantity,
        product: this.mapProductType(order.product),
        validity: order.validity === Validity.IOC ? 'IOC' : 'DAY',
        price: order.price,
        trigger_price: order.triggerPrice,
        tag: order.tag,
      };

      const result = await this.kiteAdapter.trading.placeOrder(kiteOrder);
      return { success: true, orderId: result.order_id, message: 'Order placed successfully' };
    } catch (error: any) {
      return { success: false, message: error.message || 'Order failed' };
    }
  }

  async modifyOrder(orderId: string, updates: Partial<OrderRequest>): Promise<OrderResponse> {
    if (!this.kiteAdapter || !this.isAuthenticated()) return { success: false, message: 'Not authenticated' };

    try {
      const kiteUpdates: Partial<OrderParams> = {};
      if (updates.quantity) kiteUpdates.quantity = updates.quantity;
      if (updates.price) kiteUpdates.price = updates.price;
      if (updates.triggerPrice) kiteUpdates.trigger_price = updates.triggerPrice;
      if (updates.type) kiteUpdates.order_type = this.mapOrderType(updates.type);

      const result = await this.kiteAdapter.trading.modifyOrder(orderId, kiteUpdates);
      return { success: true, orderId: result.order_id, message: 'Order modified' };
    } catch (error: any) {
      return { success: false, message: error.message };
    }
  }

  async cancelOrder(orderId: string): Promise<OrderResponse> {
    if (!this.kiteAdapter || !this.isAuthenticated()) return { success: false, message: 'Not authenticated' };

    try {
      await this.kiteAdapter.trading.cancelOrder(orderId);
      return { success: true, message: 'Order cancelled' };
    } catch (error: any) {
      return { success: false, message: error.message };
    }
  }

  async getOrders(): Promise<UnifiedOrder[]> {
    if (!this.kiteAdapter || !this.isAuthenticated()) return [];
    try {
      const orders = await this.kiteAdapter.trading.getOrders();
      return orders.map(o => this.mapKiteOrder(o));
    } catch { return []; }
  }

  async getOrder(orderId: string): Promise<UnifiedOrder> {
    if (!this.kiteAdapter || !this.isAuthenticated()) throw new Error('Not authenticated');
    const history = await this.kiteAdapter.trading.getOrderHistory(orderId);
    if (!history.length) throw new Error('Order not found');
    return this.mapKiteOrder(history[0]);
  }

  async getOrderHistory(orderId: string): Promise<UnifiedOrder[]> {
    if (!this.kiteAdapter || !this.isAuthenticated()) return [];
    try {
      const history = await this.kiteAdapter.trading.getOrderHistory(orderId);
      return history.map(o => this.mapKiteOrder(o));
    } catch { return []; }
  }

  async getPositions(): Promise<UnifiedPosition[]> {
    if (!this.kiteAdapter || !this.isAuthenticated()) return [];
    try {
      const positions = await this.kiteAdapter.portfolio.getPositions();
      return [...positions.net, ...positions.day].map(p => this.mapKitePosition(p));
    } catch { return []; }
  }

  async getHoldings(): Promise<UnifiedHolding[]> {
    if (!this.kiteAdapter || !this.isAuthenticated()) return [];
    try {
      const holdings = await this.kiteAdapter.portfolio.getHoldings();
      return holdings.map(h => this.mapKiteHolding(h));
    } catch { return []; }
  }

  async getFunds(): Promise<UnifiedFunds> {
    if (!this.kiteAdapter || !this.isAuthenticated()) throw new Error('Not authenticated');
    const margins = await this.kiteAdapter.auth.getMargins();
    const equity = margins.equity;
    return {
      brokerId: 'kite',
      availableCash: equity.available.cash,
      usedMargin: equity.utilised.debits,
      availableMargin: equity.available.live_balance,
      totalCollateral: equity.available.collateral,
      equity: equity.net,
      timestamp: new Date(),
    };
  }

  async getProfile(): Promise<UnifiedProfile> {
    if (!this.kiteAdapter || !this.isAuthenticated()) throw new Error('Not authenticated');
    const profile = await this.kiteAdapter.auth.getProfile();
    return {
      brokerId: 'kite',
      userId: profile.user_id,
      name: profile.user_name,
      email: profile.email,
      exchanges: profile.exchanges,
      products: profile.products,
    };
  }

  async getQuote(symbol: string, exchange: string): Promise<UnifiedQuote> {
    if (!this.kiteAdapter || !this.isAuthenticated()) throw new Error('Not authenticated');
    const key = `${exchange}:${symbol}`;
    const quotes = await this.kiteAdapter.market.getQuotes([key]);
    const quote = quotes[key];
    if (!quote) throw new Error('Quote not found');
    return this.mapKiteQuote(quote, symbol, exchange);
  }

  async getQuotes(symbols: Array<{ symbol: string; exchange: string }>): Promise<UnifiedQuote[]> {
    if (!this.kiteAdapter || !this.isAuthenticated()) return [];
    try {
      const keys = symbols.map(s => `${s.exchange}:${s.symbol}`);
      const quotes = await this.kiteAdapter.market.getQuotes(keys);
      return symbols.map(s => {
        const key = `${s.exchange}:${s.symbol}`;
        return quotes[key] ? this.mapKiteQuote(quotes[key], s.symbol, s.exchange) : null;
      }).filter(q => q !== null) as UnifiedQuote[];
    } catch { return []; }
  }

  async getMarketDepth(symbol: string, exchange: string): Promise<UnifiedMarketDepth> {
    if (!this.kiteAdapter || !this.isAuthenticated()) throw new Error('Not authenticated');
    const key = `${exchange}:${symbol}`;
    const quotes = await this.kiteAdapter.market.getQuotes([key]);
    const quote = quotes[key];
    if (!quote?.depth) throw new Error('Market depth not available');
    return {
      symbol, exchange,
      bids: quote.depth.buy.map(b => ({ price: b.price, quantity: b.quantity, orders: b.orders })),
      asks: quote.depth.sell.map(a => ({ price: a.price, quantity: a.quantity, orders: a.orders })),
      lastPrice: quote.last_price,
      timestamp: new Date(),
    };
  }

  async getHistoricalData(symbol: string, exchange: string, interval: TimeInterval, from: Date, to: Date): Promise<Candle[]> {
    if (!this.kiteAdapter || !this.isAuthenticated()) throw new Error('Not authenticated');
    const instrumentToken = await this.getInstrumentToken(symbol, exchange);
    const candles = await this.kiteAdapter.market.getHistoricalData({
      instrument_token: instrumentToken,
      interval: this.mapTimeInterval(interval),
      from: from.toISOString().split('T')[0],
      to: to.toISOString().split('T')[0],
    });
    return candles.map(c => this.mapKiteCandle(c));
  }

  async connectWebSocket(): Promise<void> {}
  subscribeOrders(callback: (order: UnifiedOrder) => void): void {}
  subscribePositions(callback: (position: UnifiedPosition) => void): void {}
  subscribeQuotes(symbols: Array<{ symbol: string; exchange: string }>, callback: (quote: UnifiedQuote) => void): void {}
  subscribeMarketDepth(symbol: string, exchange: string, callback: (depth: UnifiedMarketDepth) => void): void {}
  unsubscribeQuotes(symbols: Array<{ symbol: string; exchange: string }>): void {}
  async disconnectWebSocket(): Promise<void> {}

  async calculateMargin(order: OrderRequest): Promise<{ required: number; available: number; leverage?: number }> {
    if (!this.kiteAdapter || !this.isAuthenticated()) return { required: 0, available: 0 };
    try {
      const orders: MarginOrder[] = [{
        exchange: order.exchange,
        tradingsymbol: order.symbol,
        transaction_type: (order.side === OrderSide.BUY ? 'BUY' : 'SELL') as 'BUY' | 'SELL',
        variety: 'regular',
        product: this.mapProductType(order.product),
        order_type: this.mapOrderType(order.type),
        quantity: order.quantity,
        price: order.price || 0,
        trigger_price: order.triggerPrice || 0,
      }];
      const margins = await this.kiteAdapter.trading.calculateOrderMargins(orders);
      const total = margins[0]?.span + margins[0]?.exposure + margins[0]?.option_premium + margins[0]?.additional || 0;
      return { required: total, available: margins[0]?.cash || 0 };
    } catch { return { required: 0, available: 0 }; }
  }

  async searchSymbols(query: string, exchange?: string): Promise<Array<{ symbol: string; exchange: string; name: string; instrumentType?: string }>> {
    return [];
  }

  private async getInstrumentToken(symbol: string, exchange: string): Promise<string> {
    return '0';
  }

  private mapOrderType(type: OrderType): 'MARKET' | 'LIMIT' | 'SL' | 'SL-M' {
    const map = { [OrderType.MARKET]: 'MARKET' as const, [OrderType.LIMIT]: 'LIMIT' as const, [OrderType.STOP_LOSS]: 'SL' as const, [OrderType.STOP_LOSS_MARKET]: 'SL-M' as const };
    return map[type] || 'LIMIT';
  }

  private mapProductType(product: ProductType): 'CNC' | 'MIS' | 'NRML' {
    const map = { [ProductType.CNC]: 'CNC' as const, [ProductType.MIS]: 'MIS' as const, [ProductType.INTRADAY]: 'MIS' as const, [ProductType.NRML]: 'NRML' as const, [ProductType.MARGIN]: 'NRML' as const };
    return map[product] || 'CNC';
  }

  private mapTimeInterval(interval: TimeInterval): 'minute' | 'day' | '3minute' | '5minute' | '10minute' | '15minute' | '30minute' | '60minute' {
    const map: Record<TimeInterval, any> = { '1m': 'minute', '3m': '3minute', '5m': '5minute', '15m': '15minute', '30m': '30minute', '1h': '60minute', '1d': 'day' };
    return map[interval] || 'minute';
  }

  private mapKiteOrder(order: Order): UnifiedOrder {
    return {
      id: order.order_id, brokerId: 'kite', symbol: order.tradingsymbol, exchange: order.exchange,
      side: order.transaction_type === 'BUY' ? OrderSide.BUY : OrderSide.SELL,
      type: this.mapKiteOrderType(order.order_type), quantity: order.quantity,
      price: order.price, product: this.mapKiteProductType(order.product),
      validity: order.validity === 'IOC' ? Validity.IOC : Validity.DAY,
      status: this.mapKiteOrderStatus(order.status), filledQuantity: order.filled_quantity,
      pendingQuantity: order.pending_quantity, averagePrice: order.average_price,
      orderTime: new Date(order.order_timestamp), message: order.status_message,
    };
  }

  private mapKiteOrderType(type: string): OrderType {
    const map: Record<string, OrderType> = { 'MARKET': OrderType.MARKET, 'LIMIT': OrderType.LIMIT, 'SL': OrderType.STOP_LOSS, 'SL-M': OrderType.STOP_LOSS_MARKET };
    return map[type] || OrderType.LIMIT;
  }

  private mapKiteProductType(product: string): ProductType {
    const map: Record<string, ProductType> = { 'CNC': ProductType.CNC, 'MIS': ProductType.MIS, 'NRML': ProductType.NRML };
    return map[product] || ProductType.CNC;
  }

  private mapKiteOrderStatus(status: string): OrderStatus {
    const map: Record<string, OrderStatus> = {
      'COMPLETE': OrderStatus.COMPLETE, 'CANCELLED': OrderStatus.CANCELLED,
      'REJECTED': OrderStatus.REJECTED, 'OPEN': OrderStatus.OPEN,
      'TRIGGER PENDING': OrderStatus.PENDING,
    };
    return map[status] || OrderStatus.OPEN;
  }

  private mapKitePosition(position: Position): UnifiedPosition {
    return {
      id: `${position.exchange}:${position.tradingsymbol}`, brokerId: 'kite',
      symbol: position.tradingsymbol, exchange: position.exchange,
      product: this.mapKiteProductType(position.product), quantity: position.quantity,
      averagePrice: position.average_price, lastPrice: position.last_price,
      pnl: position.pnl, realizedPnl: position.realised, unrealizedPnl: position.unrealised,
      side: position.quantity > 0 ? OrderSide.BUY : OrderSide.SELL,
      value: Math.abs(position.value),
    };
  }

  private mapKiteHolding(holding: Holding): UnifiedHolding {
    return {
      id: `${holding.exchange}:${holding.tradingsymbol}`, brokerId: 'kite',
      symbol: holding.tradingsymbol, exchange: holding.exchange, isin: holding.isin,
      quantity: holding.quantity, averagePrice: holding.average_price,
      lastPrice: holding.last_price, pnl: holding.pnl,
      pnlPercentage: holding.day_change_percentage,
      currentValue: holding.last_price * holding.quantity,
      investedValue: holding.average_price * holding.quantity,
    };
  }

  private mapKiteQuote(quote: Quote, symbol: string, exchange: string): UnifiedQuote {
    return {
      symbol, exchange, lastPrice: quote.last_price,
      open: quote.ohlc?.open || 0, high: quote.ohlc?.high || 0,
      low: quote.ohlc?.low || 0, close: quote.ohlc?.close || 0,
      volume: quote.volume || 0, change: quote.net_change || 0,
      changePercent: ((quote.net_change || 0) / (quote.ohlc?.close || 1)) * 100,
      timestamp: new Date(), bid: quote.depth?.buy[0]?.price,
      ask: quote.depth?.sell[0]?.price,
      bidQty: quote.depth?.buy[0]?.quantity,
      askQty: quote.depth?.sell[0]?.quantity,
    };
  }

  private mapKiteCandle(candle: CandleData): Candle {
    return {
      timestamp: new Date(candle.timestamp),
      open: candle.open, high: candle.high,
      low: candle.low, close: candle.close, volume: candle.volume,
    };
  }
}
