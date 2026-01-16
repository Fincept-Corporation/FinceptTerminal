// Paper Trading Order Matcher
// Simple touch-based order matching - fills immediately when price touches limit
// No crossing required - simpler and more responsive

import { fillOrder, Order } from './index';
import type { PriceData } from '../services/trading/UnifiedMarketDataService';

// Order fill event
export interface OrderFillEvent {
  orderId: string;
  symbol: string;
  side: 'buy' | 'sell';
  orderType: string;
  fillPrice: number;
  quantity: number;
  timestamp: number;
}

// Callback for order fills
type OrderFillCallback = (event: OrderFillEvent) => void;

/**
 * PaperOrderMatcher - Simple touch-based order matching
 *
 * Fill logic:
 * - Limit Buy: fills when ask price <= limit price
 * - Limit Sell: fills when bid price >= limit price
 * - Stop Buy: fills when last price >= stop price
 * - Stop Sell: fills when last price <= stop price
 * - Stop Limit: becomes limit order when stop is triggered
 */
export class PaperOrderMatcher {
  // Pending orders by symbol: Map<symbol, Order[]>
  private pendingOrders: Map<string, Order[]> = new Map();

  // Callbacks for order fills
  private fillCallbacks: Set<OrderFillCallback> = new Set();

  // Track triggered stop orders (converted to limit)
  private triggeredStops: Set<string> = new Set();

  constructor() {}

  /**
   * Add an order to the matching engine
   */
  addOrder(order: Order): void {
    if (order.status !== 'pending') {
      console.warn(`[OrderMatcher] Skipping non-pending order: ${order.id}`);
      return;
    }

    const orders = this.pendingOrders.get(order.symbol) || [];
    orders.push(order);
    this.pendingOrders.set(order.symbol, orders);
    console.log(`[OrderMatcher] Added ${order.order_type} ${order.side} order for ${order.symbol}: ${order.id}`);
  }

  /**
   * Remove an order from the matching engine
   */
  removeOrder(orderId: string): void {
    for (const [symbol, orders] of this.pendingOrders.entries()) {
      const index = orders.findIndex((o) => o.id === orderId);
      if (index !== -1) {
        orders.splice(index, 1);
        if (orders.length === 0) {
          this.pendingOrders.delete(symbol);
        }
        this.triggeredStops.delete(orderId);
        console.log(`[OrderMatcher] Removed order ${orderId}`);
        return;
      }
    }
  }

  /**
   * Get pending orders for a symbol
   */
  getPendingOrders(symbol?: string): Order[] {
    if (symbol) {
      return this.pendingOrders.get(symbol) || [];
    }

    // Return all pending orders
    const allOrders: Order[] = [];
    for (const orders of this.pendingOrders.values()) {
      allOrders.push(...orders);
    }
    return allOrders;
  }

  /**
   * Check orders against current price and fill if conditions met
   */
  async checkOrders(symbol: string, price: PriceData): Promise<void> {
    const orders = this.pendingOrders.get(symbol);
    if (!orders || orders.length === 0) return;

    const ordersToRemove: string[] = [];

    for (const order of orders) {
      const shouldFill = this.shouldFillOrder(order, price);

      if (shouldFill) {
        const fillPrice = this.getFillPrice(order, price);

        try {
          console.log(`[OrderMatcher] Filling order ${order.id} at ${fillPrice}`);
          await fillOrder(order.id, fillPrice);

          // Notify callbacks
          const event: OrderFillEvent = {
            orderId: order.id,
            symbol: order.symbol,
            side: order.side,
            orderType: order.order_type,
            fillPrice,
            quantity: order.quantity,
            timestamp: Date.now(),
          };

          for (const callback of this.fillCallbacks) {
            try {
              callback(event);
            } catch (error) {
              console.error('[OrderMatcher] Fill callback error:', error);
            }
          }

          ordersToRemove.push(order.id);
        } catch (error) {
          console.error(`[OrderMatcher] Failed to fill order ${order.id}:`, error);
        }
      }
    }

    // Remove filled orders
    for (const orderId of ordersToRemove) {
      this.removeOrder(orderId);
    }
  }

  /**
   * Determine if an order should be filled based on current price
   * Simple touch-based logic - fills immediately when price touches limit
   */
  private shouldFillOrder(order: Order, price: PriceData): boolean {
    switch (order.order_type) {
      case 'limit':
        return this.checkLimitOrder(order, price);

      case 'stop':
        return this.checkStopOrder(order, price);

      case 'stop_limit':
        return this.checkStopLimitOrder(order, price);

      default:
        return false;
    }
  }

  /**
   * Check if limit order should fill
   * Buy limit: fills when ask <= limit price
   * Sell limit: fills when bid >= limit price
   */
  private checkLimitOrder(order: Order, price: PriceData): boolean {
    const limitPrice = order.price;
    if (!limitPrice || limitPrice <= 0) return false;

    if (order.side === 'buy') {
      // Buy limit: fills when ask price touches or goes below limit
      const ask = price.ask || price.last;
      return ask <= limitPrice;
    } else {
      // Sell limit: fills when bid price touches or goes above limit
      const bid = price.bid || price.last;
      return bid >= limitPrice;
    }
  }

  /**
   * Check if stop order should fill
   * Buy stop: fills when price >= stop price
   * Sell stop: fills when price <= stop price
   */
  private checkStopOrder(order: Order, price: PriceData): boolean {
    const stopPrice = order.stop_price;
    if (!stopPrice || stopPrice <= 0) return false;

    if (order.side === 'buy') {
      // Buy stop: triggers when price rises to or above stop
      return price.last >= stopPrice;
    } else {
      // Sell stop: triggers when price falls to or below stop
      return price.last <= stopPrice;
    }
  }

  /**
   * Check if stop-limit order should fill
   * First, check if stop is triggered, then treat as limit order
   */
  private checkStopLimitOrder(order: Order, price: PriceData): boolean {
    const stopPrice = order.stop_price;
    const limitPrice = order.price;

    if (!stopPrice || stopPrice <= 0 || !limitPrice || limitPrice <= 0) {
      return false;
    }

    // Check if stop has been triggered
    if (!this.triggeredStops.has(order.id)) {
      // Check stop trigger
      const stopTriggered =
        order.side === 'buy'
          ? price.last >= stopPrice
          : price.last <= stopPrice;

      if (stopTriggered) {
        this.triggeredStops.add(order.id);
        console.log(`[OrderMatcher] Stop triggered for order ${order.id}`);
      } else {
        return false; // Stop not triggered yet
      }
    }

    // Stop is triggered, now check limit
    return this.checkLimitOrder(order, price);
  }

  /**
   * Get the fill price for an order
   */
  private getFillPrice(order: Order, price: PriceData): number {
    switch (order.order_type) {
      case 'limit':
      case 'stop_limit':
        // Fill at the limit price
        return order.price || price.last;

      case 'stop':
        // Fill at stop price (market execution at trigger)
        return order.stop_price || price.last;

      default:
        return price.last;
    }
  }

  /**
   * Register a callback for order fills
   */
  onOrderFill(callback: OrderFillCallback): () => void {
    this.fillCallbacks.add(callback);
    return () => {
      this.fillCallbacks.delete(callback);
    };
  }

  /**
   * Get count of pending orders
   */
  getPendingOrderCount(): number {
    let count = 0;
    for (const orders of this.pendingOrders.values()) {
      count += orders.length;
    }
    return count;
  }

  /**
   * Get all symbols with pending orders
   */
  getSymbolsWithOrders(): string[] {
    return Array.from(this.pendingOrders.keys());
  }

  /**
   * Clear all pending orders
   */
  clearAllOrders(): void {
    this.pendingOrders.clear();
    this.triggeredStops.clear();
    console.log('[OrderMatcher] Cleared all pending orders');
  }

  /**
   * Load pending orders (e.g., from database on startup)
   */
  loadOrders(orders: Order[]): void {
    for (const order of orders) {
      if (order.status === 'pending') {
        this.addOrder(order);
      }
    }
    console.log(`[OrderMatcher] Loaded ${orders.length} pending orders`);
  }
}

// Export singleton factory
let matcherInstance: PaperOrderMatcher | null = null;

export function getOrderMatcher(): PaperOrderMatcher {
  if (!matcherInstance) {
    matcherInstance = new PaperOrderMatcher();
  }
  return matcherInstance;
}
