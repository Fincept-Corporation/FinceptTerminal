/**
 * Stock Paper Trading Adapter
 * Bridges stock broker interface to Rust paper trading backend
 */

import { invoke } from '@tauri-apps/api/core';
import { BaseStockBrokerAdapter } from '@/brokers/stocks/base/BaseStockBrokerAdapter';
import {
  OrderStatus,
  OrderSide,
} from '@/brokers/stocks/base/types';
import type {
  BrokerCredentials,
  AuthResult,
  OrderRequest,
  OrderResponse,
  Order,
  Position,
  Holding,
  MarketData,
  OHLCV,
  MarketDepth,
  FundInfo,
  MarginInfo,
  WebSocketSubscription,
  DateRange,
  ProductType,
} from '@/brokers/stocks/base/types';

export interface StockPaperTradingConfig {
  portfolioId: string;
  portfolioName: string;
  provider: string;
  initialBalance: number;
  currency?: string;
  realBrokerAdapter: BaseStockBrokerAdapter; // For real-time market data
}

export class StockPaperTradingAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'paper';
  readonly brokerName = 'Paper Trading';

  private config: StockPaperTradingConfig;
  private realBroker: BaseStockBrokerAdapter;
  private connected = false;
  private authenticated = false;

  constructor(config: StockPaperTradingConfig) {
    super();
    this.config = config;
    this.realBroker = config.realBrokerAdapter;
  }

  // ==================== CONNECTION & AUTH ====================

  isConnected(): boolean {
    return this.connected;
  }

  isAuthenticated(): boolean {
    return this.authenticated;
  }

  async authenticate(credentials: BrokerCredentials): Promise<AuthResult> {
    try {
      // Ensure portfolio exists in Rust backend
      const portfolio = await invoke<any>('paper_trading_get_portfolio', {
        portfolioId: this.config.portfolioId,
      }).catch(() => null);

      if (!portfolio) {
        // Create new portfolio
        await invoke('paper_trading_create_portfolio', {
          id: this.config.portfolioId,
          name: this.config.portfolioName,
          provider: this.config.provider,
          initialBalance: this.config.initialBalance,
          currency: this.config.currency || 'INR',
          marginMode: 'cross',
          leverage: 1,
        });
      }

      this.connected = true;
      this.authenticated = true;

      return {
        success: true,
        message: 'Paper trading portfolio initialized',
      };
    } catch (error: any) {
      return {
        success: false,
        message: error.toString(),
      };
    }
  }

  async refreshToken(): Promise<AuthResult> {
    // Paper trading doesn't need token refresh
    return {
      success: true,
      message: 'No token refresh needed for paper trading',
    };
  }

  async logout(): Promise<void> {
    this.connected = false;
    this.authenticated = false;
  }

  // ==================== ORDER MANAGEMENT ====================

  async placeOrder(request: OrderRequest): Promise<OrderResponse> {
    try {
      // Get current market price from real broker
      const marketData = await this.realBroker.getMarketData(
        [request.symbol],
        request.exchange
      );

      if (!marketData.length) {
        return {
          success: false,
          message: 'Unable to fetch current market price',
        };
      }

      const currentPrice = marketData[0].ltp;

      // Call Rust paper trading
      const result = await invoke<any>('paper_trading_place_stock_order', {
        portfolioId: this.config.portfolioId,
        order: {
          symbol: request.symbol,
          exchange: request.exchange,
          side: request.side,
          orderType: request.type,
          quantity: request.quantity,
          price: request.price,
          triggerPrice: request.triggerPrice,
          product: request.product,
          validity: request.validity || 'DAY',
          currentPrice,
        },
      });

      return {
        success: result.success,
        orderId: result.order_id,
        message: result.message,
        timestamp: new Date(),
        data: result,
      };
    } catch (error: any) {
      return {
        success: false,
        message: error.toString(),
      };
    }
  }

  async modifyOrder(
    orderId: string,
    modifications: Partial<OrderRequest>
  ): Promise<OrderResponse> {
    try {
      const result = await invoke<any>('paper_trading_modify_order', {
        portfolioId: this.config.portfolioId,
        orderId,
        modifications: {
          quantity: modifications.quantity,
          price: modifications.price,
          triggerPrice: modifications.triggerPrice,
          orderType: modifications.type,
        },
      });

      return {
        success: result.success,
        orderId: result.order_id,
        message: result.message,
      };
    } catch (error: any) {
      return {
        success: false,
        message: error.toString(),
      };
    }
  }

  async cancelOrder(orderId: string): Promise<OrderResponse> {
    try {
      const result = await invoke<any>('paper_trading_cancel_order', {
        portfolioId: this.config.portfolioId,
        orderId,
      });

      return {
        success: result.success,
        message: result.message,
      };
    } catch (error: any) {
      return {
        success: false,
        message: error.toString(),
      };
    }
  }

  async getOrders(): Promise<Order[]> {
    try {
      const orders = await invoke<any[]>('paper_trading_get_orders', {
        portfolioId: this.config.portfolioId,
        filter: 'open',
      });

      return orders.map(this.transformOrder);
    } catch (error) {
      console.error('Failed to get orders:', error);
      return [];
    }
  }

  async getOrderHistory(): Promise<Order[]> {
    try {
      const orders = await invoke<any[]>('paper_trading_get_orders', {
        portfolioId: this.config.portfolioId,
        filter: 'all',
      });

      return orders.map(this.transformOrder);
    } catch (error) {
      console.error('Failed to get order history:', error);
      return [];
    }
  }

  async getOrderById(orderId: string): Promise<Order> {
    try {
      const order = await invoke<any>('paper_trading_get_order_by_id', {
        portfolioId: this.config.portfolioId,
        orderId,
      });

      return this.transformOrder(order);
    } catch (error: any) {
      throw new Error(`Failed to get order: ${error.toString()}`);
    }
  }

  // ==================== POSITION MANAGEMENT ====================

  async getPositions(): Promise<Position[]> {
    try {
      const positions = await invoke<any[]>('paper_trading_get_positions', {
        portfolioId: this.config.portfolioId,
      });

      return positions.map(p => ({
        symbol: p.symbol,
        exchange: p.exchange,
        product: p.product as ProductType,
        quantity: p.quantity,
        averagePrice: p.average_price,
        ltp: p.current_price ?? p.average_price,
        pnl: p.unrealized_pnl ?? 0,
        pnlPercent: ((p.unrealized_pnl ?? 0) / (p.average_price * Math.abs(p.quantity))) * 100,
        dayPnl: p.today_realized_pnl,
        realizedPnl: p.realized_pnl,
        unrealizedPnl: p.unrealized_pnl,
      }));
    } catch (error) {
      console.error('Failed to get positions:', error);
      return [];
    }
  }

  async closePosition(
    symbol: string,
    exchange: string,
    product: string
  ): Promise<OrderResponse> {
    try {
      // Get current position
      const positions = await this.getPositions();
      const position = positions.find(
        p => p.symbol === symbol && p.exchange === exchange && p.product === product
      );

      if (!position) {
        return {
          success: false,
          message: 'Position not found',
        };
      }

      // Place reverse order
      const reverseSide = position.quantity > 0 ? OrderSide.SELL : OrderSide.BUY;

      return this.placeOrder({
        symbol,
        exchange,
        side: reverseSide,
        type: 'MARKET' as any,
        quantity: Math.abs(position.quantity),
        product: product as ProductType,
      });
    } catch (error: any) {
      return {
        success: false,
        message: error.toString(),
      };
    }
  }

  // ==================== HOLDINGS ====================

  async getHoldings(): Promise<Holding[]> {
    try {
      const holdings = await invoke<any[]>('paper_trading_get_holdings', {
        portfolioId: this.config.portfolioId,
      });

      return holdings.map(h => ({
        symbol: h.symbol,
        exchange: h.exchange,
        quantity: h.quantity,
        averagePrice: h.average_price,
        ltp: h.current_price ?? h.average_price,
        pnl: h.pnl ?? 0,
        pnlPercent: h.pnl_percent ?? 0,
      }));
    } catch (error) {
      console.error('Failed to get holdings:', error);
      return [];
    }
  }

  // ==================== MARKET DATA ====================

  async getMarketData(symbols: string[], exchange: string): Promise<MarketData[]> {
    // Delegate to real broker for market data
    return this.realBroker.getMarketData(symbols, exchange);
  }

  async getHistoricalData(
    symbol: string,
    exchange: string,
    dateRange: DateRange,
    interval: string
  ): Promise<OHLCV[]> {
    // Delegate to real broker for historical data
    return this.realBroker.getHistoricalData(symbol, exchange, dateRange, interval);
  }

  async getMarketDepth(symbol: string, exchange: string): Promise<MarketDepth> {
    // Delegate to real broker
    if (this.realBroker.getMarketDepth) {
      return this.realBroker.getMarketDepth(symbol, exchange);
    }
    throw new Error('Market depth not supported by real broker');
  }

  // ==================== FUNDS & MARGIN ====================

  async getFunds(): Promise<FundInfo> {
    try {
      const funds = await invoke<any>('paper_trading_get_funds', {
        portfolioId: this.config.portfolioId,
      });

      return {
        availableBalance: funds.available_balance,
        usedMargin: funds.used_margin,
        totalBalance: funds.total_capital,
        realizedPnl: funds.realized_pnl,
        unrealizedPnl: funds.unrealized_pnl,
      };
    } catch (error: any) {
      throw new Error(`Failed to get funds: ${error.toString()}`);
    }
  }

  async getMargins(): Promise<MarginInfo> {
    const funds = await this.getFunds();
    return {
      available: funds.availableBalance,
      used: funds.usedMargin,
    };
  }

  // ==================== WEBSOCKET ====================

  async subscribeMarketData(
    subscription: WebSocketSubscription
  ): Promise<() => void> {
    // Delegate to real broker
    return this.realBroker.subscribeMarketData(subscription);
  }

  async unsubscribeMarketData(symbols: string[]): Promise<void> {
    // Delegate to real broker
    return this.realBroker.unsubscribeMarketData(symbols);
  }

  // ==================== HELPER METHODS ====================

  private transformOrder(order: any): Order {
    return {
      id: order.id,
      symbol: order.symbol,
      exchange: order.exchange,
      side: order.side as OrderSide,
      type: order.order_type as any,
      quantity: order.quantity,
      filledQuantity: order.filled_quantity,
      pendingQuantity: order.quantity - order.filled_quantity,
      price: order.price,
      triggerPrice: order.stop_price,
      averagePrice: order.avg_fill_price,
      product: order.product as ProductType,
      status: this.mapOrderStatus(order.status),
      validity: order.time_in_force as any,
      orderTime: new Date(order.created_at),
      updateTime: order.updated_at ? new Date(order.updated_at) : undefined,
      tag: order.tag,
    };
  }

  private mapOrderStatus(status: string): OrderStatus {
    const statusMap: Record<string, OrderStatus> = {
      open: OrderStatus.OPEN,
      pending: OrderStatus.PENDING,
      complete: OrderStatus.COMPLETE,
      completed: OrderStatus.COMPLETE,
      cancelled: OrderStatus.CANCELLED,
      rejected: OrderStatus.REJECTED,
    };
    return statusMap[status.toLowerCase()] || OrderStatus.PENDING;
  }

  // ==================== STATISTICS (PAPER TRADING SPECIFIC) ====================

  /**
   * Get paper trading statistics
   */
  async getStatistics(): Promise<{
    totalPnL: number;
    realizedPnL: number;
    unrealizedPnL: number;
    totalTrades: number;
    winRate: number;
    profitFactor: number;
  }> {
    try {
      const stats = await invoke<any>('paper_trading_get_statistics', {
        portfolioId: this.config.portfolioId,
      });

      return {
        totalPnL: parseFloat(stats.total_pnl || '0'),
        realizedPnL: parseFloat(stats.realized_pnl || '0'),
        unrealizedPnL: parseFloat(stats.unrealized_pnl || '0'),
        totalTrades: parseInt(stats.total_trades || '0'),
        winRate: parseFloat(stats.win_rate || '0'),
        profitFactor: parseFloat(stats.profit_factor || '0'),
      };
    } catch (error) {
      console.error('Failed to get statistics:', error);
      return {
        totalPnL: 0,
        realizedPnL: 0,
        unrealizedPnL: 0,
        totalTrades: 0,
        winRate: 0,
        profitFactor: 0,
      };
    }
  }

  /**
   * Reset paper trading portfolio
   */
  async resetPortfolio(): Promise<void> {
    await invoke('paper_trading_reset_portfolio', {
      portfolioId: this.config.portfolioId,
    });
  }

  // ==================== OPTIONAL ABSTRACT METHODS ====================

  /**
   * Get login URL - not applicable for paper trading
   */
  getLoginUrl(): string {
    throw new Error('Paper trading does not require login URL');
  }

  /**
   * Convert position - delegate to real broker or handle in paper trading
   */
  async convertPosition(
    symbol: string,
    exchange: string,
    fromProduct: string,
    toProduct: string,
    quantity: number
  ): Promise<OrderResponse> {
    try {
      // In paper trading, we simulate position conversion
      const result = await invoke<any>('paper_trading_convert_position', {
        portfolioId: this.config.portfolioId,
        symbol,
        exchange,
        fromProduct,
        toProduct,
        quantity,
      });

      return {
        success: result.success,
        message: result.message,
      };
    } catch (error: any) {
      return {
        success: false,
        message: error.toString(),
      };
    }
  }

  /**
   * Calculate margin for an order
   */
  async calculateMargin(request: OrderRequest): Promise<number> {
    try {
      // Get current market price
      const marketData = await this.realBroker.getMarketData(
        [request.symbol],
        request.exchange
      );

      if (!marketData.length) {
        return 0;
      }

      const price = request.price || marketData[0].ltp;
      const quantity = request.quantity;

      // Calculate margin based on product type
      let leverageMultiplier = 1;
      if (request.product === 'MIS') {
        leverageMultiplier = 5; // 5x leverage for MIS
      } else if (request.product === 'NRML') {
        leverageMultiplier = 10; // 10x leverage for NRML
      }

      const orderValue = price * quantity;
      return orderValue / leverageMultiplier;
    } catch (error) {
      console.error('Failed to calculate margin:', error);
      return 0;
    }
  }

  /**
   * Search symbols - delegate to real broker
   */
  async searchSymbols(
    query: string,
    exchange?: string
  ): Promise<Array<{
    symbol: string;
    name: string;
    exchange: string;
    token?: string;
    instrumentType?: string;
  }>> {
    if (this.realBroker.searchSymbols) {
      return this.realBroker.searchSymbols(query, exchange);
    }
    throw new Error('Symbol search not supported by real broker');
  }

  /**
   * Get instrument details - delegate to real broker
   */
  async getInstrumentDetails(
    symbol: string,
    exchange: string
  ): Promise<{
    symbol: string;
    name: string;
    exchange: string;
    token: string;
    lotSize: number;
    tickSize: number;
    instrumentType: string;
    expiry?: Date;
    strike?: number;
    optionType?: 'CE' | 'PE';
  }> {
    if (this.realBroker.getInstrumentDetails) {
      return this.realBroker.getInstrumentDetails(symbol, exchange);
    }
    throw new Error('Instrument details not supported by real broker');
  }
}
