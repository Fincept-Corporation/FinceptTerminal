// Universal Paper Trading Module - TypeScript Wrapper
// Thin wrapper around Rust backend via Tauri invoke
// Uses UnifiedMarketDataService for prices - NO dependency on realAdapter

import { invoke } from '@tauri-apps/api/core';
import {
  UnifiedMarketDataService,
  getMarketDataService,
  type PriceData,
} from '../services/trading/UnifiedMarketDataService';
import { PaperOrderMatcher, getOrderMatcher } from './OrderMatcher';

// Types
export interface Portfolio {
  id: string;
  name: string;
  initial_balance: number;
  balance: number;
  currency: string;
  leverage: number;
  margin_mode: 'cross' | 'isolated';
  fee_rate: number;
  created_at: string;
}

export interface Order {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: 'buy' | 'sell';
  order_type: 'market' | 'limit' | 'stop' | 'stop_limit';
  quantity: number;
  price?: number;
  stop_price?: number;
  filled_qty: number;
  avg_price?: number;
  status: 'pending' | 'filled' | 'partial' | 'cancelled';
  reduce_only: boolean;
  created_at: string;
  filled_at?: string;
}

export interface Position {
  id: string;
  portfolio_id: string;
  symbol: string;
  side: 'long' | 'short';
  quantity: number;
  entry_price: number;
  current_price: number;
  unrealized_pnl: number;
  realized_pnl: number;
  leverage: number;
  liquidation_price?: number;
  opened_at: string;
}

export interface Trade {
  id: string;
  portfolio_id: string;
  order_id: string;
  symbol: string;
  side: string;
  price: number;
  quantity: number;
  fee: number;
  pnl: number;
  timestamp: string;
}

export interface Stats {
  total_pnl: number;
  win_rate: number;
  total_trades: number;
  winning_trades: number;
  losing_trades: number;
  largest_win: number;
  largest_loss: number;
}

// Portfolio
export const createPortfolio = (name: string, balance: number, currency = 'USD', leverage = 1, marginMode = 'cross', feeRate = 0.001) =>
  invoke<Portfolio>('pt_create_portfolio', { name, balance, currency, leverage, marginMode, feeRate });

export const getPortfolio = (id: string) => invoke<Portfolio>('pt_get_portfolio', { id });
export const listPortfolios = () => invoke<Portfolio[]>('pt_list_portfolios');
export const deletePortfolio = (id: string) => invoke<void>('pt_delete_portfolio', { id });
export const resetPortfolio = (id: string) => invoke<Portfolio>('pt_reset_portfolio', { id });

// Orders
export const placeOrder = (portfolioId: string, symbol: string, side: 'buy' | 'sell', orderType: string, quantity: number, price?: number, stopPrice?: number, reduceOnly = false) =>
  invoke<Order>('pt_place_order', { portfolioId, symbol, side, orderType, quantity, price, stopPrice, reduceOnly });

export const cancelOrder = (orderId: string) => invoke<void>('pt_cancel_order', { orderId });
export const getOrders = (portfolioId: string, status?: string) => invoke<Order[]>('pt_get_orders', { portfolioId, status });
export const fillOrder = (orderId: string, fillPrice: number, fillQty?: number) => invoke<Trade>('pt_fill_order', { orderId, fillPrice, fillQty });

// Positions
export const getPositions = (portfolioId: string) => invoke<Position[]>('pt_get_positions', { portfolioId });
export const updatePrice = (portfolioId: string, symbol: string, price: number) => invoke<void>('pt_update_price', { portfolioId, symbol, price });

// Trades & Stats
export const getTrades = (portfolioId: string, limit = 100) => invoke<Trade[]>('pt_get_trades', { portfolioId, limit });
export const getStats = (portfolioId: string) => invoke<Stats>('pt_get_stats', { portfolioId });

// Convenience: Place market order and fill immediately
export async function executeMarketOrder(portfolioId: string, symbol: string, side: 'buy' | 'sell', quantity: number, currentPrice: number, reduceOnly = false): Promise<Trade> {
  const order = await placeOrder(portfolioId, symbol, side, 'market', quantity, currentPrice, undefined, reduceOnly);
  return fillOrder(order.id, currentPrice);
}

// Convenience: Close position
export async function closePosition(portfolioId: string, symbol: string, side: 'long' | 'short', quantity: number, currentPrice: number): Promise<Trade> {
  const closeSide = side === 'long' ? 'sell' : 'buy';
  return executeMarketOrder(portfolioId, symbol, closeSide, quantity, currentPrice, true);
}

// =============================================================================
// PaperTradingAdapter - Compatibility wrapper for TradingTab
// Implements IExchangeAdapter interface using Rust backend
// =============================================================================

export interface PaperTradingConfig {
  portfolioId: string;
  portfolioName: string;
  provider: string;
  assetClass: string;
  initialBalance: number;
  currency: string;
  fees: { maker: number; taker: number };
  slippage: { market: number; limit: number };
  enableMarginTrading: boolean;
  defaultLeverage: number;
  maxLeverage: number;
}

/**
 * PaperTradingAdapter - Simulated trading using Rust backend
 *
 * Key improvements:
 * - NO dependency on realAdapter for prices
 * - Uses UnifiedMarketDataService for market data
 * - Order matching via PaperOrderMatcher
 * - Live position P&L updates via price subscriptions
 */
export class PaperTradingAdapter {
  public name: string;
  public portfolioId: string;
  private config: PaperTradingConfig;
  private connected: boolean = false;

  // Services
  private marketDataService: UnifiedMarketDataService;
  private orderMatcher: PaperOrderMatcher;

  // Price subscription unsubscribers
  private priceUnsubscribers: Map<string, () => void> = new Map();

  // Symbols we're actively tracking
  private trackedSymbols: Set<string> = new Set();

  constructor(config: PaperTradingConfig, marketDataService?: UnifiedMarketDataService) {
    this.config = config;
    this.portfolioId = config.portfolioId;
    this.name = `${config.provider.toUpperCase()} (Paper Trading)`;

    // Use provided service or get singleton
    this.marketDataService = marketDataService || getMarketDataService();
    this.orderMatcher = getOrderMatcher();
  }

  async connect(): Promise<void> {
    // Create or get portfolio from Rust
    const portfolios = await listPortfolios();
    let portfolio = portfolios.find(p => p.name === this.config.portfolioName);

    if (!portfolio) {
      portfolio = await createPortfolio(
        this.config.portfolioName,
        this.config.initialBalance,
        this.config.currency,
        this.config.defaultLeverage,
        'cross',
        this.config.fees.taker
      );
    }

    this.portfolioId = portfolio.id;

    // Load existing pending orders into the matcher
    const pendingOrders = await getOrders(this.portfolioId, 'pending');
    if (pendingOrders.length > 0) {
      this.orderMatcher.loadOrders(pendingOrders);

      // Subscribe to price updates for symbols with pending orders
      for (const order of pendingOrders) {
        this.subscribeToSymbol(order.symbol);
      }
    }

    // Subscribe to price updates for existing positions
    const positions = await getPositions(this.portfolioId);
    for (const position of positions) {
      this.subscribeToSymbol(position.symbol);
    }

    this.connected = true;
    console.log(`[PaperTradingAdapter] Connected. Portfolio: ${this.portfolioId}`);
  }

  async disconnect(): Promise<void> {
    // Unsubscribe from all price feeds
    for (const [symbol, unsubscribe] of this.priceUnsubscribers) {
      unsubscribe();
      console.log(`[PaperTradingAdapter] Unsubscribed from ${symbol}`);
    }
    this.priceUnsubscribers.clear();
    this.trackedSymbols.clear();
    this.connected = false;
  }

  isConnected(): boolean {
    return this.connected;
  }

  /**
   * Subscribe to price updates for a symbol
   * Updates position P&L and checks pending orders
   */
  private subscribeToSymbol(symbol: string): void {
    if (this.priceUnsubscribers.has(symbol)) return; // Already subscribed

    const unsubscribe = this.marketDataService.subscribeToPrice(symbol, async (price: PriceData) => {
      // 1. Update position P&L in Rust backend
      try {
        await updatePrice(this.portfolioId, symbol, price.last);
      } catch (error) {
        // Position might not exist, that's fine
      }

      // 2. Check pending orders for fills
      await this.orderMatcher.checkOrders(symbol, price);
    });

    this.priceUnsubscribers.set(symbol, unsubscribe);
    this.trackedSymbols.add(symbol);
    console.log(`[PaperTradingAdapter] Subscribed to ${symbol} price updates`);
  }

  /**
   * Unsubscribe from a symbol if no longer needed
   */
  private async maybeUnsubscribeFromSymbol(symbol: string): Promise<void> {
    // Check if we still need this symbol
    const pendingOrders = this.orderMatcher.getPendingOrders(symbol);
    const positions = await getPositions(this.portfolioId);
    const hasPosition = positions.some(p => p.symbol === symbol);

    if (pendingOrders.length === 0 && !hasPosition) {
      const unsubscribe = this.priceUnsubscribers.get(symbol);
      if (unsubscribe) {
        unsubscribe();
        this.priceUnsubscribers.delete(symbol);
        this.trackedSymbols.delete(symbol);
        console.log(`[PaperTradingAdapter] Unsubscribed from ${symbol} - no longer needed`);
      }
    }
  }

  async fetchBalance(): Promise<any> {
    const portfolio = await getPortfolio(this.portfolioId);
    const positions = await getPositions(this.portfolioId);

    // Calculate total unrealized P&L and margin used from all open positions
    let totalUnrealizedPnl = 0;
    let totalMarginUsed = 0;

    for (const position of positions) {
      // Get live price from market data service
      const livePrice = this.marketDataService.getCurrentPrice(position.symbol, this.config.provider);
      const currentPrice = livePrice?.last || position.current_price;

      if (currentPrice > 0 && position.entry_price > 0) {
        // Recalculate unrealized P&L with live price
        let pnl = 0;
        if (position.side === 'long') {
          pnl = (currentPrice - position.entry_price) * position.quantity;
        } else {
          pnl = (position.entry_price - currentPrice) * position.quantity;
        }
        totalUnrealizedPnl += pnl;

        // Calculate margin used for this position (entry_value / leverage)
        const entryValue = position.entry_price * position.quantity;
        const leverage = position.leverage || portfolio.leverage || 1;
        totalMarginUsed += entryValue / leverage;
      } else {
        // Fall back to stored unrealized P&L
        totalUnrealizedPnl += position.unrealized_pnl || 0;
        const entryValue = position.entry_price * position.quantity;
        const leverage = position.leverage || portfolio.leverage || 1;
        totalMarginUsed += entryValue / leverage;
      }
    }

    // Balance = available cash (after margin reserved for positions)
    // Equity = Balance + Margin Used + Unrealized P&L = Total account value
    const equity = portfolio.balance + totalMarginUsed + totalUnrealizedPnl;

    return {
      free: { USD: portfolio.balance },           // Available margin (cash)
      used: { USD: totalMarginUsed },             // Margin locked in positions
      total: { USD: equity },                     // Total equity (balance + margin + unrealized P&L)
    };
  }

  async fetchPositions(): Promise<any[]> {
    return getPositions(this.portfolioId);
  }

  async fetchOpenOrders(): Promise<any[]> {
    return getOrders(this.portfolioId, 'pending');
  }

  async fetchMyTrades(_symbol?: string, _since?: number, limit?: number): Promise<any[]> {
    const trades = await getTrades(this.portfolioId, limit || 100);
    return trades.map(t => ({
      ...t,
      isMaker: false,
    }));
  }

  async createOrder(
    symbol: string,
    type: string,
    side: string,
    quantity: number,
    price?: number,
    params?: any
  ): Promise<any> {
    // Get current price from market data service
    const currentPrice = this.marketDataService.getCurrentPrice(symbol, this.config.provider);
    let fillPrice = price;

    // For market orders, we need a valid price
    if (type === 'market') {
      if (currentPrice) {
        fillPrice = currentPrice.last;
      } else if (!fillPrice || fillPrice <= 0) {
        console.error('[PaperTradingAdapter] No market data available for:', symbol);
        throw new Error(`No market data available for ${symbol}. Please ensure market data is streaming.`);
      }
    }

    console.log(`[PaperTradingAdapter] Creating ${type} ${side} order: ${quantity} ${symbol} @ ${fillPrice || 'limit'}`);

    // Place order in Rust backend
    const order = await placeOrder(
      this.portfolioId,
      symbol,
      side as 'buy' | 'sell',
      type,
      quantity,
      type === 'limit' || type === 'stop_limit' ? price : fillPrice,
      params?.stopPrice,
      params?.reduceOnly || false
    );

    // Subscribe to symbol for price updates
    this.subscribeToSymbol(symbol);

    // Handle order based on type
    if (type === 'market') {
      // Market orders fill immediately
      if (fillPrice && fillPrice > 0) {
        console.log(`[PaperTradingAdapter] Filling market order ${order.id} at ${fillPrice}`);
        await fillOrder(order.id, fillPrice);
      }

      return {
        id: order.id,
        symbol: order.symbol,
        type: order.order_type,
        side: order.side,
        amount: order.quantity,
        price: fillPrice,
        filled: order.quantity,
        status: 'filled',
      };
    } else {
      // Limit/Stop orders go to the matcher
      this.orderMatcher.addOrder(order);

      // Check if it fills immediately (e.g., limit buy above market)
      if (currentPrice) {
        await this.orderMatcher.checkOrders(symbol, currentPrice);
      }

      return {
        id: order.id,
        symbol: order.symbol,
        type: order.order_type,
        side: order.side,
        amount: order.quantity,
        price: order.price,
        filled: order.filled_qty,
        status: order.status,
      };
    }
  }

  async cancelOrder(orderId: string, _symbol?: string): Promise<any> {
    // Remove from matcher
    this.orderMatcher.removeOrder(orderId);

    // Cancel in Rust backend
    await cancelOrder(orderId);

    return { id: orderId, status: 'cancelled' };
  }

  async fetchOrder(orderId: string, _symbol?: string): Promise<any> {
    const orders = await getOrders(this.portfolioId);
    const order = orders.find(o => o.id === orderId);
    if (!order) throw new Error(`Order ${orderId} not found`);
    return {
      id: order.id,
      symbol: order.symbol,
      type: order.order_type,
      side: order.side,
      amount: order.quantity,
      price: order.price,
      average: order.avg_price,
      filled: order.filled_qty,
      status: order.status,
      fee: { cost: 0 },
    };
  }

  /**
   * Fetch ticker from market data service
   * NO dependency on realAdapter
   */
  async fetchTicker(symbol: string): Promise<any> {
    const price = this.marketDataService.getCurrentPrice(symbol, this.config.provider);
    if (!price) {
      throw new Error(`No market data available for ${symbol}`);
    }

    return {
      symbol,
      last: price.last,
      bid: price.bid,
      ask: price.ask,
      high: price.high,
      low: price.low,
      volume: price.volume,
      change: price.change,
      percentage: price.changePercent,
      timestamp: price.timestamp,
    };
  }

  async getStatistics(): Promise<any> {
    const stats = await getStats(this.portfolioId);
    return {
      totalPnL: stats.total_pnl,
      winRate: stats.win_rate,
      totalTrades: stats.total_trades,
      winningTrades: stats.winning_trades,
      losingTrades: stats.losing_trades,
      largestWin: stats.largest_win,
      largestLoss: stats.largest_loss,
    };
  }

  /**
   * Market data methods - use market data service
   * These still work even without realAdapter
   */
  async fetchOHLCV(_symbol: string, _timeframe?: string, _since?: number, _limit?: number): Promise<any[]> {
    // OHLCV requires historical data - not available from live WebSocket
    // Return empty array; UI should use separate OHLCV data source
    return [];
  }

  async fetchOrderBook(_symbol: string, _limit?: number): Promise<any> {
    // Order book data would come from WebSocket - not directly available here
    // Return empty; UI should use useRustOrderBook hook
    return { bids: [], asks: [] };
  }

  async fetchTickers(symbols?: string[]): Promise<any> {
    const result: Record<string, any> = {};

    const targetSymbols = symbols || this.marketDataService.getAvailableSymbols();
    for (const symbol of targetSymbols) {
      const price = this.marketDataService.getCurrentPrice(symbol);
      if (price) {
        result[symbol] = {
          symbol,
          last: price.last,
          bid: price.bid,
          ask: price.ask,
          timestamp: price.timestamp,
        };
      }
    }

    return result;
  }

  async fetchMarkets(): Promise<any[]> {
    // Return available symbols as markets
    return this.marketDataService.getAvailableSymbols().map(symbol => ({
      symbol,
      base: symbol.split('/')[0],
      quote: symbol.split('/')[1] || 'USD',
      active: true,
    }));
  }

  async cancelAllOrders(symbol?: string): Promise<any[]> {
    const orders = await getOrders(this.portfolioId, 'pending');
    const results = [];
    for (const order of orders) {
      if (!symbol || order.symbol === symbol) {
        this.orderMatcher.removeOrder(order.id);
        await cancelOrder(order.id);
        results.push({ id: order.id, status: 'cancelled' });
      }
    }

    // Cleanup subscriptions
    if (symbol) {
      await this.maybeUnsubscribeFromSymbol(symbol);
    }

    return results;
  }

  /**
   * Get the market data service (for external use)
   */
  getMarketDataService(): UnifiedMarketDataService {
    return this.marketDataService;
  }

  /**
   * Get tracked symbols
   */
  getTrackedSymbols(): string[] {
    return Array.from(this.trackedSymbols);
  }
}

/**
 * Factory function - creates adapter with market data service
 * @param config Paper trading configuration
 * @param marketDataService Optional market data service (uses singleton if not provided)
 */
export function createPaperTradingAdapter(
  config: PaperTradingConfig,
  marketDataService?: UnifiedMarketDataService
): PaperTradingAdapter {
  return new PaperTradingAdapter(config, marketDataService);
}
