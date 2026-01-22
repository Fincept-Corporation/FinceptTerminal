/**
 * YFinance Paper Trading Adapter
 *
 * Simple paper trading broker using YFinance for market data.
 * Fetches quotes on-demand with caching - no real-time streaming.
 * All orders executed against local paper trading engine.
 */

import { invoke } from '@tauri-apps/api/core';
import { BaseStockBrokerAdapter } from '../BaseStockBrokerAdapter';
import { STOCK_BROKER_REGISTRY } from '../registry';
import type {
  Region,
  StockBrokerMetadata,
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
  Instrument,
  WebSocketConfig,
  SubscriptionMode,
  BulkOperationResult,
  SmartOrderParams,
} from '../types';

// Paper trading imports
import {
  createPortfolio,
  getPortfolio,
  listPortfolios,
  placeOrder as ptPlaceOrder,
  cancelOrder as ptCancelOrder,
  getOrders as ptGetOrders,
  getPositions as ptGetPositions,
  getTrades as ptGetTrades,
  fillOrder as ptFillOrder,
  type Portfolio,
  type Order as PaperOrder,
  type Position as PaperPosition,
  type Trade as PaperTrade,
} from '@/paper-trading';

// ============================================================================
// TYPES
// ============================================================================

export type UpdateInterval = 15 | 30 | 60 | 300 | 600; // seconds

interface YFinanceQuote {
  symbol: string;
  price: number;
  change: number;
  change_percent: number;
  volume: number | null;
  high: number | null;
  low: number | null;
  open: number | null;
  previous_close: number;
  timestamp: number;
  exchange?: string;
}

interface CachedQuote {
  quote: Quote;
  fetchedAt: number;
}

// ============================================================================
// YFINANCE PAPER TRADING ADAPTER
// ============================================================================

export class YFinancePaperTradingAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'yfinance';
  readonly brokerName = 'YFinance Paper Trading';
  readonly region: Region = 'us';
  readonly metadata: StockBrokerMetadata = STOCK_BROKER_REGISTRY['yfinance'];

  // Paper trading state
  private portfolioId: string | null = null;
  private portfolio: Portfolio | null = null;

  // Quote cache - quotes are cached and refreshed on demand
  private quoteCache: Map<string, CachedQuote> = new Map();
  private cacheTTL: number = 60000; // 1 minute default cache TTL in ms

  // ============================================================================
  // SETTINGS
  // ============================================================================

  /**
   * Set cache TTL (how long quotes are considered fresh)
   */
  setCacheTTL(seconds: UpdateInterval): void {
    this.cacheTTL = seconds * 1000;
    console.log(`[YFinance] Cache TTL set to ${seconds}s`);
  }

  getCacheTTL(): number {
    return this.cacheTTL / 1000;
  }

  // ============================================================================
  // AUTHENTICATION (Paper Trading - Creates/loads portfolio)
  // ============================================================================

  async authenticate(_credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      console.log('[YFinance] Initializing paper trading...');

      const portfolios = await listPortfolios();
      let portfolio = portfolios.find(p => p.name === 'YFinance Paper Trading');

      if (!portfolio) {
        portfolio = await createPortfolio(
          'YFinance Paper Trading',
          100000, // $100k default
          'USD',
          1,
          'cross',
          0
        );
        console.log('[YFinance] Created new portfolio:', portfolio.id);
      } else {
        console.log('[YFinance] Using existing portfolio:', portfolio.id);
      }

      this.portfolioId = portfolio.id;
      this.portfolio = portfolio;
      this._isConnected = true;
      this.accessToken = 'paper_trading_enabled';

      return {
        success: true,
        accessToken: 'paper_trading_enabled',
        userId: portfolio.id,
        message: 'Paper trading initialized',
      };
    } catch (error) {
      console.error('[YFinance] Init failed:', error);
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Init failed',
        errorCode: 'PAPER_INIT_FAILED',
      };
    }
  }

  getAuthUrl(): string {
    return '';
  }

  async logout(): Promise<void> {
    this.quoteCache.clear();
    this.portfolioId = null;
    this.portfolio = null;
    this._isConnected = false;
    this.accessToken = null;
  }

  // ============================================================================
  // YFINANCE DATA FETCHING (Simple, on-demand)
  // ============================================================================

  private async fetchQuote(symbol: string): Promise<YFinanceQuote | null> {
    try {
      const result = await invoke<string>('execute_yfinance_command', {
        command: 'quote',
        args: [symbol],
      });
      const data = JSON.parse(result);
      return data.error ? null : data;
    } catch (error) {
      console.error(`[YFinance] Quote error for ${symbol}:`, error);
      return null;
    }
  }

  private convertQuote(yq: YFinanceQuote, fallbackExchange: StockExchange): Quote {
    // Use exchange from API if available, otherwise use fallback
    const exchange = this.getExchange(yq.symbol, yq.exchange) || fallbackExchange;
    return {
      symbol: yq.symbol,
      exchange,
      lastPrice: yq.price,
      open: yq.open || yq.price,
      high: yq.high || yq.price,
      low: yq.low || yq.price,
      close: yq.price,
      previousClose: yq.previous_close,
      change: yq.change,
      changePercent: yq.change_percent,
      volume: yq.volume || 0,
      bid: yq.price - 0.01,
      bidQty: 100,
      ask: yq.price + 0.01,
      askQty: 100,
      timestamp: yq.timestamp * 1000,
    };
  }

  private getExchange(symbol: string, apiExchange?: string): StockExchange {
    // First check symbol suffix
    if (symbol.endsWith('.NS')) return 'NSE';
    if (symbol.endsWith('.BO')) return 'BSE';
    if (symbol.endsWith('.L')) return 'LSE';
    if (symbol.endsWith('.TO')) return 'TSX';
    if (symbol.endsWith('.AX')) return 'ASX';
    if (symbol.endsWith('.HK')) return 'HKEX';

    // Use API exchange info if available
    if (apiExchange) {
      const exchangeUpper = apiExchange.toUpperCase();
      if (exchangeUpper === 'NMS' || exchangeUpper === 'NGM' || exchangeUpper === 'NCM' || exchangeUpper.includes('NASDAQ')) return 'NASDAQ';
      if (exchangeUpper === 'NYQ' || exchangeUpper.includes('NYSE')) return 'NYSE';
      if (exchangeUpper === 'ASE' || exchangeUpper.includes('AMEX')) return 'AMEX';
      if (exchangeUpper === 'NSI' || exchangeUpper.includes('NSE')) return 'NSE';
      if (exchangeUpper === 'BOM' || exchangeUpper.includes('BSE')) return 'BSE';
    }

    // Default to NASDAQ for US stocks (most tech stocks)
    return 'NASDAQ';
  }

  // ============================================================================
  // MARKET DATA
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    // Check cache
    const cached = this.quoteCache.get(symbol);
    if (cached && Date.now() - cached.fetchedAt < this.cacheTTL) {
      return cached.quote;
    }

    // Fetch fresh
    const yq = await this.fetchQuote(symbol);
    if (!yq) {
      throw new Error(`Failed to fetch quote for ${symbol}`);
    }

    const quote = this.convertQuote(yq, exchange);
    this.quoteCache.set(symbol, { quote, fetchedAt: Date.now() });
    return quote;
  }

  protected async getOHLCVInternal(
    symbol: string,
    _exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    try {
      // Map TimeFrame to yfinance interval format
      const intervalMap: Record<TimeFrame, string> = {
        '1m': '1m',
        '2m': '2m',
        '3m': '5m', // yfinance doesn't have 3m, use 5m
        '5m': '5m',
        '10m': '15m', // yfinance doesn't have 10m, use 15m
        '15m': '15m',
        '30m': '30m',
        '1h': '1h',
        '2h': '60m', // yfinance doesn't have 2h, use 60m
        '4h': '60m', // yfinance doesn't have 4h, use 60m
        '1d': '1d',
        '1w': '1wk',
        '1M': '1mo',
      };
      const interval = intervalMap[timeframe] || '1d';

      const result = await invoke<string>('execute_yfinance_command', {
        command: 'historical',
        args: [symbol, from.toISOString().split('T')[0], to.toISOString().split('T')[0], interval],
      });
      const data = JSON.parse(result);
      if (data.error || !Array.isArray(data)) return [];
      return data.map((d: any) => ({
        timestamp: d.timestamp * 1000,
        open: d.open,
        high: d.high,
        low: d.low,
        close: d.close,
        volume: d.volume,
      }));
    } catch {
      return [];
    }
  }

  protected async getMarketDepthInternal(symbol: string, _exchange: StockExchange): Promise<MarketDepth> {
    const cached = this.quoteCache.get(symbol);
    const price = cached?.quote.lastPrice || 100;
    return {
      bids: Array.from({ length: 5 }, (_, i) => ({ price: price - 0.01 * (i + 1), quantity: 100 * (i + 1) })),
      asks: Array.from({ length: 5 }, (_, i) => ({ price: price + 0.01 * (i + 1), quantity: 100 * (i + 1) })),
      timestamp: Date.now(),
    };
  }

  // ============================================================================
  // ORDERS - PAPER TRADING
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    if (!this.portfolioId) {
      return { success: false, message: 'Paper trading not initialized' };
    }

    try {
      let price = params.price;
      if (params.orderType === 'MARKET') {
        const quote = await this.getQuoteInternal(params.symbol, params.exchange);
        price = quote.lastPrice;
      }

      const order = await ptPlaceOrder(
        this.portfolioId,
        params.symbol,
        params.side.toLowerCase() as 'buy' | 'sell',
        params.orderType.toLowerCase(),
        params.quantity,
        price,
        params.triggerPrice
      );

      if (params.orderType === 'MARKET' && price) {
        await ptFillOrder(order.id, price);
      }

      return { success: true, orderId: order.id };
    } catch (error) {
      return { success: false, message: error instanceof Error ? error.message : 'Order failed' };
    }
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    try {
      await ptCancelOrder(params.orderId);
      return { success: true, orderId: params.orderId, message: 'Order cancelled (modify not supported)' };
    } catch {
      return { success: false, message: 'Modify failed' };
    }
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    try {
      await ptCancelOrder(orderId);
      return { success: true, orderId };
    } catch {
      return { success: false, message: 'Cancel failed' };
    }
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    if (!this.portfolioId) return [];
    const orders = await ptGetOrders(this.portfolioId);
    return orders.map(o => this.toOrder(o));
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    if (!this.portfolioId) return [];
    const trades = await ptGetTrades(this.portfolioId);
    return trades.map(t => this.toTrade(t));
  }

  // ============================================================================
  // POSITIONS & HOLDINGS
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    if (!this.portfolioId) return [];
    const positions = await ptGetPositions(this.portfolioId);
    return positions.map(p => this.toPosition(p));
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const positions = await this.getPositionsInternal();
    return positions.map(p => ({
      symbol: p.symbol,
      exchange: p.exchange,
      quantity: p.quantity,
      averagePrice: p.averagePrice,
      lastPrice: p.lastPrice,
      investedValue: p.averagePrice * p.quantity,
      currentValue: p.lastPrice * p.quantity,
      pnl: p.pnl,
      pnlPercent: p.pnlPercent,
    }));
  }

  protected async getFundsInternal(): Promise<Funds> {
    if (!this.portfolioId) {
      return { availableCash: 0, usedMargin: 0, availableMargin: 0, totalBalance: 0, currency: 'USD' };
    }
    const portfolio = await getPortfolio(this.portfolioId);
    return {
      availableCash: portfolio.balance,
      usedMargin: Math.max(0, portfolio.initial_balance - portfolio.balance),
      availableMargin: portfolio.balance,
      totalBalance: portfolio.initial_balance,
      currency: 'USD',
    };
  }

  // ============================================================================
  // CONVERTERS
  // ============================================================================

  private toOrder(po: PaperOrder): Order {
    const statusMap: Record<string, Order['status']> = {
      pending: 'PENDING', filled: 'FILLED', partial: 'PARTIALLY_FILLED', cancelled: 'CANCELLED'
    };
    return {
      orderId: po.id,
      symbol: po.symbol,
      exchange: this.getExchange(po.symbol),
      side: po.side.toUpperCase() as 'BUY' | 'SELL',
      quantity: po.quantity,
      filledQuantity: po.filled_qty,
      pendingQuantity: po.quantity - po.filled_qty,
      price: po.price || 0,
      averagePrice: po.avg_price || 0,
      triggerPrice: po.stop_price,
      orderType: po.order_type.toUpperCase() as Order['orderType'],
      productType: 'CASH',
      validity: 'DAY',
      status: statusMap[po.status] || 'PENDING',
      placedAt: new Date(po.created_at),
      updatedAt: po.filled_at ? new Date(po.filled_at) : undefined,
    };
  }

  private toPosition(pp: PaperPosition): Position {
    return {
      symbol: pp.symbol,
      exchange: this.getExchange(pp.symbol),
      productType: 'CASH',
      quantity: pp.quantity,
      buyQuantity: pp.side === 'long' ? pp.quantity : 0,
      sellQuantity: pp.side === 'short' ? pp.quantity : 0,
      buyValue: pp.side === 'long' ? pp.entry_price * pp.quantity : 0,
      sellValue: pp.side === 'short' ? pp.entry_price * pp.quantity : 0,
      averagePrice: pp.entry_price,
      lastPrice: pp.current_price,
      pnl: pp.unrealized_pnl,
      pnlPercent: pp.entry_price > 0 ? ((pp.current_price - pp.entry_price) / pp.entry_price) * 100 : 0,
      dayPnl: 0,
    };
  }

  private toTrade(pt: PaperTrade): Trade {
    return {
      tradeId: pt.id,
      orderId: pt.order_id,
      symbol: pt.symbol,
      exchange: this.getExchange(pt.symbol),
      side: pt.side.toUpperCase() as 'BUY' | 'SELL',
      quantity: pt.quantity,
      price: pt.price,
      productType: 'CASH',
      tradedAt: new Date(pt.timestamp),
    };
  }

  // ============================================================================
  // WEBSOCKET (Not used - just stubs for interface compliance)
  // ============================================================================

  protected async connectWebSocketInternal(_config: WebSocketConfig): Promise<void> {
    this.wsConnected = true;
  }

  async disconnectWebSocket(): Promise<void> {
    this.wsConnected = false;
  }

  protected async subscribeInternal(_symbol: string, _exchange: StockExchange, _mode: SubscriptionMode): Promise<void> {}
  protected async unsubscribeInternal(_symbol: string, _exchange: StockExchange): Promise<void> {}

  // ============================================================================
  // BULK OPERATIONS
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    if (!this.portfolioId) {
      return { success: false, totalCount: 0, successCount: 0, failedCount: 0, results: [] };
    }
    const orders = await ptGetOrders(this.portfolioId, 'pending');
    let successCount = 0;
    const results = [];
    for (const order of orders) {
      try {
        await ptCancelOrder(order.id);
        results.push({ success: true, orderId: order.id });
        successCount++;
      } catch {
        results.push({ success: false, orderId: order.id, error: 'Failed' });
      }
    }
    return { success: successCount === orders.length, totalCount: orders.length, successCount, failedCount: orders.length - successCount, results };
  }

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    if (!this.portfolioId) {
      return { success: false, totalCount: 0, successCount: 0, failedCount: 0, results: [] };
    }
    const positions = await ptGetPositions(this.portfolioId);
    let successCount = 0;
    const results = [];
    for (const pos of positions) {
      try {
        const side = pos.side === 'long' ? 'sell' : 'buy';
        const order = await ptPlaceOrder(this.portfolioId, pos.symbol, side, 'market', pos.quantity, pos.current_price, undefined, true);
        await ptFillOrder(order.id, pos.current_price);
        results.push({ success: true, symbol: pos.symbol });
        successCount++;
      } catch {
        results.push({ success: false, symbol: pos.symbol, error: 'Failed' });
      }
    }
    return { success: successCount === positions.length, totalCount: positions.length, successCount, failedCount: positions.length - successCount, results };
  }

  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    return this.placeOrderInternal(params);
  }

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    const totalValue = orders.reduce((sum, o) => sum + (o.price || 100) * o.quantity, 0);
    return {
      totalMargin: totalValue,
      initialMargin: totalValue,
      orders: orders.map(o => ({ symbol: o.symbol, margin: (o.price || 100) * o.quantity })),
    };
  }

  // ============================================================================
  // SYMBOL SEARCH
  // ============================================================================

  async searchSymbols(query: string, _exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const result = await invoke<string>('execute_yfinance_command', { command: 'search', args: [query, '20'] });
      const data = JSON.parse(result);
      if (data.error || !data.results) return [];
      return data.results.map((r: any) => ({
        symbol: r.symbol,
        name: r.name,
        exchange: r.exchange as StockExchange,
        instrumentType: 'EQUITY' as const,
        currency: r.currency as any,
        lotSize: 1,
        tickSize: 0.01,
      }));
    } catch {
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    try {
      const result = await invoke<string>('execute_yfinance_command', { command: 'info', args: [symbol] });
      const data = JSON.parse(result);
      if (data.error) return null;
      return {
        symbol: data.symbol || symbol,
        name: data.company_name || symbol,
        exchange,
        instrumentType: 'EQUITY',
        currency: (data.currency || 'USD') as any,
        lotSize: 1,
        tickSize: 0.01,
      };
    } catch {
      return null;
    }
  }
}
