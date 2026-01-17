/**
 * Stock Grid Adapter
 *
 * Wraps IStockBrokerAdapter (stock brokers) for use with grid trading.
 * Provides a unified interface for the GridTradingService.
 */

import type {
  IGridBrokerAdapter,
  GridOrderParams,
  GridOrderResult,
  GridOrderStatus,
  BrokerType,
} from '../types';
import type {
  IStockBrokerAdapter,
  OrderType,
  OrderSide,
  ProductType,
  StockExchange,
} from '../../../brokers/stocks/types';

export class StockGridAdapter implements IGridBrokerAdapter {
  private adapter: IStockBrokerAdapter;
  private priceCallbacks: Map<string, Set<(price: number) => void>> = new Map();
  private tickUnsubscribers: Map<string, () => void> = new Map();

  public readonly brokerId: string;
  public readonly brokerType: BrokerType = 'stock';

  constructor(adapter: IStockBrokerAdapter) {
    this.adapter = adapter;
    this.brokerId = adapter.brokerId;
  }

  get isConnected(): boolean {
    return this.adapter.isConnected;
  }

  /**
   * Place a grid order
   */
  async placeGridOrder(params: GridOrderParams): Promise<GridOrderResult> {
    try {
      const response = await this.adapter.placeOrder({
        symbol: params.symbol,
        exchange: (params.exchange || 'NSE') as StockExchange,
        side: params.side.toUpperCase() as OrderSide,
        quantity: params.quantity,
        orderType: 'LIMIT' as OrderType,
        productType: (params.productType || 'CASH') as ProductType,
        price: params.price,
        tag: params.tag,
      });

      return {
        success: response.success,
        orderId: response.orderId,
        message: response.message,
        errorCode: response.errorCode,
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Failed to place order',
        errorCode: 'ORDER_FAILED',
      };
    }
  }

  /**
   * Cancel a grid order
   */
  async cancelGridOrder(orderId: string, _symbol: string): Promise<boolean> {
    try {
      const response = await this.adapter.cancelOrder(orderId);
      return response.success;
    } catch (error) {
      console.error(`[StockGridAdapter] Failed to cancel order ${orderId}:`, error);
      return false;
    }
  }

  /**
   * Get order status
   */
  async getOrderStatus(orderId: string, _symbol: string): Promise<GridOrderStatus> {
    try {
      const orders = await this.adapter.getOrders();
      const order = orders.find(o => o.orderId === orderId);

      if (!order) {
        return {
          orderId,
          status: 'unknown',
          filledQuantity: 0,
          remainingQuantity: 0,
          averagePrice: 0,
        };
      }

      let status: GridOrderStatus['status'] = 'unknown';
      switch (order.status) {
        case 'OPEN':
        case 'PENDING':
        case 'TRIGGER_PENDING':
          status = 'open';
          break;
        case 'FILLED':
          status = 'filled';
          break;
        case 'PARTIALLY_FILLED':
          status = 'partial';
          break;
        case 'CANCELLED':
        case 'EXPIRED':
          status = 'cancelled';
          break;
        case 'REJECTED':
          status = 'rejected';
          break;
      }

      return {
        orderId,
        status,
        filledQuantity: order.filledQuantity,
        remainingQuantity: order.pendingQuantity,
        averagePrice: order.averagePrice,
      };
    } catch (error) {
      return {
        orderId,
        status: 'unknown',
        filledQuantity: 0,
        remainingQuantity: 0,
        averagePrice: 0,
      };
    }
  }

  /**
   * Get current price for symbol
   */
  async getCurrentPrice(symbol: string, exchange: string = 'NSE'): Promise<number> {
    try {
      const quote = await this.adapter.getQuote(symbol, exchange as StockExchange);
      return quote.lastPrice;
    } catch (error) {
      console.error(`[StockGridAdapter] Failed to get price for ${symbol}:`, error);
      throw error;
    }
  }

  /**
   * Subscribe to price updates
   */
  subscribeToPrice(
    symbol: string,
    callback: (price: number) => void,
    exchange: string = 'NSE'
  ): () => void {
    const key = `${exchange}:${symbol}`;

    // Add callback to set
    if (!this.priceCallbacks.has(key)) {
      this.priceCallbacks.set(key, new Set());
    }
    this.priceCallbacks.get(key)!.add(callback);

    // Set up WebSocket subscription if not already
    if (!this.tickUnsubscribers.has(key)) {
      // Create tick handler
      const tickHandler = (tick: { symbol: string; exchange: string; lastPrice: number }) => {
        if (tick.symbol === symbol && tick.exchange === exchange) {
          const callbacks = this.priceCallbacks.get(key);
          if (callbacks) {
            callbacks.forEach(cb => cb(tick.lastPrice));
          }
        }
      };

      // Subscribe to ticks
      this.adapter.onTick(tickHandler);

      // Subscribe to symbol
      this.adapter.subscribe(symbol, exchange as StockExchange, 'ltp')
        .catch(err => console.error(`[StockGridAdapter] Subscribe error:`, err));

      // Store unsubscriber
      this.tickUnsubscribers.set(key, () => {
        this.adapter.offTick(tickHandler);
        this.adapter.unsubscribe(symbol, exchange as StockExchange)
          .catch(err => console.error(`[StockGridAdapter] Unsubscribe error:`, err));
      });
    }

    // Return unsubscribe function
    return () => {
      const callbacks = this.priceCallbacks.get(key);
      if (callbacks) {
        callbacks.delete(callback);
        if (callbacks.size === 0) {
          this.priceCallbacks.delete(key);
          const unsubscribe = this.tickUnsubscribers.get(key);
          if (unsubscribe) {
            unsubscribe();
            this.tickUnsubscribers.delete(key);
          }
        }
      }
    };
  }

  /**
   * Get available balance
   */
  async getAvailableBalance(_currency?: string): Promise<number> {
    try {
      const funds = await this.adapter.getFunds();
      return funds.availableCash;
    } catch (error) {
      console.error(`[StockGridAdapter] Failed to get balance:`, error);
      return 0;
    }
  }

  /**
   * Clean up resources
   */
  dispose(): void {
    // Unsubscribe from all
    for (const unsubscribe of this.tickUnsubscribers.values()) {
      try {
        unsubscribe();
      } catch (error) {
        // Ignore cleanup errors
      }
    }
    this.tickUnsubscribers.clear();
    this.priceCallbacks.clear();
  }
}
