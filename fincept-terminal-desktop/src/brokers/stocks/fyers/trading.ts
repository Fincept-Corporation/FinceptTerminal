// Fyers Trading Module
// Handles order placement, modification, cancellation, and GTT orders

import { PlaceOrderRequest, FyersOrder } from './types';

export class FyersTrading {
  private fyers: any;

  constructor(fyersClient: any) {
    this.fyers = fyersClient;
  }

  /**
   * Place an order
   */
  async placeOrder(orderRequest: PlaceOrderRequest): Promise<{ orderId: string; message: string }> {
    const response = await this.fyers.place_order(orderRequest);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to place order');
    }

    return {
      orderId: response.id || '',
      message: response.message || 'Order placed successfully'
    };
  }

  /**
   * Get order book
   */
  async getOrders(): Promise<FyersOrder[]> {
    const response = await this.fyers.get_orders();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch orders');
    }

    return response.orderBook || [];
  }

  /**
   * Cancel an order
   */
  async cancelOrder(orderId: string): Promise<void> {
    const response = await this.fyers.cancel_order({ id: orderId });

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to cancel order');
    }
  }

  /**
   * Place GTT order
   */
  async placeGTTOrder(gttPayload: any): Promise<any> {
    const response = await this.fyers.place_gtt_order(gttPayload);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'GTT Order placement failed');
    }

    return response;
  }

  /**
   * Calculate margin for a single order (span_margin endpoint)
   */
  async calculateSpanMargin(order: {
    symbol: string;
    qty: number;
    side: 'BUY' | 'SELL';
    type: 'MARKET' | 'LIMIT' | 'STOP';
    productType: 'CNC' | 'INTRADAY' | 'MARGIN';
    limitPrice?: number;
    stopLoss?: number;
    stopPrice?: number;
    takeProfit?: number;
  }): Promise<any> {
    const payload = {
      symbol: order.symbol,
      qty: order.qty,
      side: order.side === 'BUY' ? 1 : -1,
      type: order.type === 'MARKET' ? 2 : order.type === 'LIMIT' ? 1 : 3,
      productType: order.productType,
      limitPrice: order.limitPrice || 0,
      stopLoss: order.stopLoss || 0,
      stopPrice: order.stopPrice || 0,
      takeProfit: order.takeProfit || 0
    };

    const response = await this.fyers.span_margin(payload);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to calculate margin');
    }

    return response;
  }

  /**
   * Calculate margin for multiple orders (multiorder/margin endpoint)
   */
  async calculateMultiOrderMargin(orders: Array<{
    symbol: string;
    qty: number;
    side: 'BUY' | 'SELL';
    type: 'MARKET' | 'LIMIT' | 'STOP';
    productType: 'CNC' | 'INTRADAY' | 'MARGIN';
    limitPrice?: number;
    stopLoss?: number;
    stopPrice?: number;
    takeProfit?: number;
  }>): Promise<any> {
    const payload = orders.map(order => ({
      symbol: order.symbol,
      qty: order.qty,
      side: order.side === 'BUY' ? 1 : -1,
      type: order.type === 'MARKET' ? 2 : order.type === 'LIMIT' ? 1 : 3,
      productType: order.productType,
      limitPrice: order.limitPrice || 0,
      stopLoss: order.stopLoss || 0,
      stopPrice: order.stopPrice || 0,
      takeProfit: order.takeProfit || 0
    }));

    const response = await this.fyers.multiorder_margin(payload);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to calculate multi-order margin');
    }

    return response;
  }
}
