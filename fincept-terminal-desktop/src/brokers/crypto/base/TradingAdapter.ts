/**
 * Trading Adapter
 *
 * Order management and trading methods
 * Extends CoreAdapter with trading functionality
 */

import { Order, Trade } from 'ccxt';
import { CoreExchangeAdapter } from './CoreAdapter';
import { OrderType, OrderSide, OrderParams } from '../types';

export abstract class TradingAdapter extends CoreExchangeAdapter {
  // ============================================================================
  // ORDER CREATION
  // ============================================================================

  async createOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number,
    params?: OrderParams
  ): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.createOrder(symbol, type, side, amount, price, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async createOrders(orders: Array<{
    symbol: string;
    type: OrderType;
    side: OrderSide;
    amount: number;
    price?: number;
    params?: OrderParams;
  }>): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createOrders']) {
        const results: Order[] = [];
        for (const order of orders) {
          const result = await this.createOrder(
            order.symbol,
            order.type,
            order.side,
            order.amount,
            order.price,
            order.params
          );
          results.push(result);
        }
        return results;
      }
      return await this.exchange.createOrders(orders);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async createMarketOrderWithCost(
    symbol: string,
    side: OrderSide,
    cost: number,
    params?: OrderParams
  ): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createMarketOrderWithCost']) {
        throw new Error('createMarketOrderWithCost not supported');
      }
      return await (this.exchange as any).createMarketOrderWithCost(symbol, side, cost, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async createMarketBuyOrderWithCost(
    symbol: string,
    cost: number,
    params?: OrderParams
  ): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createMarketBuyOrderWithCost']) {
        return await this.createMarketOrderWithCost(symbol, 'buy', cost, params);
      }
      return await (this.exchange as any).createMarketBuyOrderWithCost(symbol, cost, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ORDER MODIFICATION
  // ============================================================================

  async editOrder(
    orderId: string,
    symbol: string,
    type?: OrderType,
    side?: OrderSide,
    amount?: number,
    price?: number,
    params?: OrderParams
  ): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['editOrder']) {
        throw new Error('editOrder not supported by this exchange');
      }
      return await this.exchange.editOrder(orderId, symbol, type as any, side as any, amount, price, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ORDER CANCELLATION
  // ============================================================================

  async cancelOrder(orderId: string, symbol: string): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.cancelOrder(orderId, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async cancelOrders(orderIds: string[], symbol?: string): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['cancelOrders']) {
        const results: Order[] = [];
        for (const orderId of orderIds) {
          if (!symbol) throw new Error('Symbol required for bulk cancel fallback');
          const result = await this.cancelOrder(orderId, symbol);
          results.push(result);
        }
        return results;
      }
      return await this.exchange.cancelOrders(orderIds, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async cancelAllOrders(symbol?: string): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['cancelAllOrders']) {
        throw new Error('Exchange does not support cancelAllOrders');
      }
      return await this.exchange.cancelAllOrders(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async cancelAllOrdersAfter(timeout: number): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['cancelAllOrdersAfter']) {
        throw new Error('cancelAllOrdersAfter not supported by this exchange');
      }
      return await (this.exchange as any).cancelAllOrdersAfter(timeout);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ORDER QUERIES
  // ============================================================================

  async fetchOrder(orderId: string, symbol: string): Promise<Order> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchOrder(orderId, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOrdersByIds(orderIds: string[], symbol?: string): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchOrdersByIds']) {
        const results: Order[] = [];
        for (const orderId of orderIds) {
          if (!symbol) throw new Error('Symbol required for fetchOrdersByIds fallback');
          const order = await this.fetchOrder(orderId, symbol);
          results.push(order);
        }
        return results;
      }
      return await (this.exchange as any).fetchOrdersByIds(orderIds, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOrderTrades(orderId: string, symbol: string): Promise<Trade[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchOrderTrades']) {
        throw new Error('fetchOrderTrades not supported by this exchange');
      }
      return await this.exchange.fetchOrderTrades(orderId, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchMyTrades(symbol?: string, since?: number, limit?: number): Promise<Trade[]> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchMyTrades']) {
        throw new Error('fetchMyTrades not supported by this exchange');
      }
      return await this.exchange.fetchMyTrades(symbol, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchOpenOrders(symbol?: string): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchOpenOrders(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchClosedOrders(symbol?: string, limit?: number): Promise<Order[]> {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchClosedOrders(symbol, undefined, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }
}
