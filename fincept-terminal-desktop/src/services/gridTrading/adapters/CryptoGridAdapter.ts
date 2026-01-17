/**
 * Crypto Grid Adapter
 *
 * Wraps IExchangeAdapter (crypto exchanges) for use with grid trading.
 * Provides a unified interface for the GridTradingService.
 */

import type {
  IGridBrokerAdapter,
  GridOrderParams,
  GridOrderResult,
  GridOrderStatus,
  BrokerType,
} from '../types';
import type { IExchangeAdapter, OrderType, OrderSide } from '../../../brokers/crypto/types';

export class CryptoGridAdapter implements IGridBrokerAdapter {
  private adapter: IExchangeAdapter;
  private priceCallbacks: Map<string, Set<(price: number) => void>> = new Map();
  private priceIntervals: Map<string, NodeJS.Timeout> = new Map();

  public readonly brokerId: string;
  public readonly brokerType: BrokerType = 'crypto';

  constructor(adapter: IExchangeAdapter) {
    this.adapter = adapter;
    this.brokerId = adapter.id;
  }

  get isConnected(): boolean {
    return this.adapter.isConnected();
  }

  /**
   * Place a grid order
   */
  async placeGridOrder(params: GridOrderParams): Promise<GridOrderResult> {
    try {
      const order = await this.adapter.createOrder(
        params.symbol,
        'limit' as OrderType,
        params.side as OrderSide,
        params.quantity,
        params.price,
        {
          clientOrderId: params.tag,
        }
      );

      return {
        success: true,
        orderId: order.id,
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
  async cancelGridOrder(orderId: string, symbol: string): Promise<boolean> {
    try {
      await this.adapter.cancelOrder(orderId, symbol);
      return true;
    } catch (error) {
      console.error(`[CryptoGridAdapter] Failed to cancel order ${orderId}:`, error);
      return false;
    }
  }

  /**
   * Get order status
   */
  async getOrderStatus(orderId: string, symbol: string): Promise<GridOrderStatus> {
    try {
      const order = await this.adapter.fetchOrder(orderId, symbol);

      let status: GridOrderStatus['status'] = 'unknown';
      if (order.status === 'open') status = 'open';
      else if (order.status === 'closed') status = 'filled';
      else if (order.status === 'canceled') status = 'cancelled';
      else if (order.status === 'rejected') status = 'rejected';

      const filled = order.filled || 0;
      const remaining = order.remaining || (order.amount - filled);

      if (order.status === 'open' && filled > 0) {
        status = 'partial';
      }

      return {
        orderId,
        status,
        filledQuantity: filled,
        remainingQuantity: remaining,
        averagePrice: order.average || order.price || 0,
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
  async getCurrentPrice(symbol: string): Promise<number> {
    try {
      const ticker = await this.adapter.fetchTicker(symbol);
      return ticker.last || ticker.close || 0;
    } catch (error) {
      console.error(`[CryptoGridAdapter] Failed to get price for ${symbol}:`, error);
      throw error;
    }
  }

  /**
   * Subscribe to price updates
   */
  subscribeToPrice(
    symbol: string,
    callback: (price: number) => void,
    _exchange?: string
  ): () => void {
    // Add callback to set
    if (!this.priceCallbacks.has(symbol)) {
      this.priceCallbacks.set(symbol, new Set());
    }
    this.priceCallbacks.get(symbol)!.add(callback);

    // Start polling if not already
    if (!this.priceIntervals.has(symbol)) {
      const interval = setInterval(async () => {
        try {
          const price = await this.getCurrentPrice(symbol);
          const callbacks = this.priceCallbacks.get(symbol);
          if (callbacks) {
            callbacks.forEach(cb => cb(price));
          }
        } catch (error) {
          // Ignore polling errors
        }
      }, 5000); // Poll every 5 seconds

      this.priceIntervals.set(symbol, interval);
    }

    // Return unsubscribe function
    return () => {
      const callbacks = this.priceCallbacks.get(symbol);
      if (callbacks) {
        callbacks.delete(callback);
        if (callbacks.size === 0) {
          this.priceCallbacks.delete(symbol);
          const interval = this.priceIntervals.get(symbol);
          if (interval) {
            clearInterval(interval);
            this.priceIntervals.delete(symbol);
          }
        }
      }
    };
  }

  /**
   * Get available balance
   */
  async getAvailableBalance(currency: string = 'USD'): Promise<number> {
    try {
      const balance = await this.adapter.fetchBalance();

      // Try to find the currency in free balance
      const free = balance.free as Record<string, number> | undefined;
      const currencyBalance = free?.[currency] ||
        free?.['USDT'] ||
        free?.['USDC'] ||
        0;

      return typeof currencyBalance === 'number' ? currencyBalance : 0;
    } catch (error) {
      console.error(`[CryptoGridAdapter] Failed to get balance:`, error);
      return 0;
    }
  }

  /**
   * Clean up resources
   */
  dispose(): void {
    // Clear all intervals
    for (const interval of this.priceIntervals.values()) {
      clearInterval(interval);
    }
    this.priceIntervals.clear();
    this.priceCallbacks.clear();
  }
}
