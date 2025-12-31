/**
 * useOrderExecution Hook
 * Handles cross-broker order execution with smart routing
 */

import { useState, useCallback } from 'react';
import {
  BrokerType,
  OrderRequest,
  OrderResponse,
  OrderSide,
  OrderType,
  ProductType,
  Validity,
} from '../core/types';
import { orderRouter, RoutingStrategy } from '../core/OrderRouter';

interface OrderExecutionState {
  executing: boolean;
  lastOrder?: OrderRequest;
  lastResult?: any;
  error?: string;
}

export function useOrderExecution() {
  const [state, setState] = useState<OrderExecutionState>({
    executing: false,
  });

  // Place order with routing strategy
  const placeOrder = useCallback(async (
    order: OrderRequest,
    strategy: RoutingStrategy = RoutingStrategy.BEST_LATENCY,
    brokers?: BrokerType[]
  ) => {
    setState(prev => ({ ...prev, executing: true, error: undefined, lastOrder: order }));

    try {
      const result = await orderRouter.routeOrder(order, {
        strategy,
        brokers,
      });

      setState(prev => ({
        ...prev,
        executing: false,
        lastResult: result,
      }));

      return result;
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        executing: false,
        error: error.message || 'Order execution failed',
      }));
      throw error;
    }
  }, []);

  // Place market order
  const placeMarketOrder = useCallback(async (
    symbol: string,
    exchange: string,
    side: OrderSide,
    quantity: number,
    product: ProductType = ProductType.MIS,
    brokers?: BrokerType[]
  ) => {
    const order: OrderRequest = {
      symbol,
      exchange,
      side,
      type: OrderType.MARKET,
      quantity,
      product,
      validity: Validity.DAY,
    };

    return await placeOrder(order, RoutingStrategy.BEST_LATENCY, brokers);
  }, [placeOrder]);

  // Place limit order
  const placeLimitOrder = useCallback(async (
    symbol: string,
    exchange: string,
    side: OrderSide,
    quantity: number,
    price: number,
    product: ProductType = ProductType.MIS,
    brokers?: BrokerType[]
  ) => {
    const order: OrderRequest = {
      symbol,
      exchange,
      side,
      type: OrderType.LIMIT,
      quantity,
      price,
      product,
      validity: Validity.DAY,
    };

    return await placeOrder(order, RoutingStrategy.BEST_PRICE, brokers);
  }, [placeOrder]);

  // Place stop-loss order
  const placeStopLossOrder = useCallback(async (
    symbol: string,
    exchange: string,
    side: OrderSide,
    quantity: number,
    triggerPrice: number,
    limitPrice?: number,
    product: ProductType = ProductType.MIS,
    brokers?: BrokerType[]
  ) => {
    const order: OrderRequest = {
      symbol,
      exchange,
      side,
      type: limitPrice ? OrderType.STOP_LOSS : OrderType.STOP_LOSS_MARKET,
      quantity,
      triggerPrice,
      price: limitPrice,
      product,
      validity: Validity.DAY,
    };

    return await placeOrder(order, RoutingStrategy.BEST_LATENCY, brokers);
  }, [placeOrder]);

  // Place order on specific broker
  const placeOrderOnBroker = useCallback(async (
    order: OrderRequest,
    brokerId: BrokerType
  ) => {
    return await placeOrder(order, RoutingStrategy.PARALLEL, [brokerId]);
  }, [placeOrder]);

  // Place order on multiple brokers simultaneously
  const placeOrderOnAllBrokers = useCallback(async (
    order: OrderRequest,
    brokers?: BrokerType[]
  ) => {
    return await placeOrder(order, RoutingStrategy.PARALLEL, brokers);
  }, [placeOrder]);

  // Smart order (auto-selects best strategy)
  const placeSmartOrder = useCallback(async (
    order: OrderRequest
  ) => {
    setState(prev => ({ ...prev, executing: true, error: undefined, lastOrder: order }));

    try {
      const result = await orderRouter.smartRoute(order);

      setState(prev => ({
        ...prev,
        executing: false,
        lastResult: result,
      }));

      return result;
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        executing: false,
        error: error.message || 'Smart order failed',
      }));
      throw error;
    }
  }, []);

  // Modify order
  const modifyOrder = useCallback(async (
    brokerId: BrokerType,
    orderId: string,
    updates: Partial<OrderRequest>
  ) => {
    setState(prev => ({ ...prev, executing: true, error: undefined }));

    try {
      const result = await orderRouter.modifyOrder(brokerId, orderId, updates);

      setState(prev => ({
        ...prev,
        executing: false,
        lastResult: result,
      }));

      return result;
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        executing: false,
        error: error.message || 'Order modification failed',
      }));
      throw error;
    }
  }, []);

  // Cancel order
  const cancelOrder = useCallback(async (
    brokerId: BrokerType,
    orderId: string
  ) => {
    setState(prev => ({ ...prev, executing: true, error: undefined }));

    try {
      const result = await orderRouter.cancelOrder(brokerId, orderId);

      setState(prev => ({
        ...prev,
        executing: false,
        lastResult: result,
      }));

      return result;
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        executing: false,
        error: error.message || 'Order cancellation failed',
      }));
      throw error;
    }
  }, []);

  // Batch orders
  const placeBatchOrders = useCallback(async (
    orders: OrderRequest[],
    strategy: RoutingStrategy = RoutingStrategy.ROUND_ROBIN,
    brokers?: BrokerType[]
  ) => {
    setState(prev => ({ ...prev, executing: true, error: undefined }));

    try {
      const results = await orderRouter.routeBatch(orders, { strategy, brokers });

      setState(prev => ({
        ...prev,
        executing: false,
        lastResult: results,
      }));

      return results;
    } catch (error: any) {
      setState(prev => ({
        ...prev,
        executing: false,
        error: error.message || 'Batch order execution failed',
      }));
      throw error;
    }
  }, []);

  // Quick buy/sell helpers
  const quickBuy = useCallback(async (
    symbol: string,
    exchange: string,
    quantity: number,
    product: ProductType = ProductType.MIS
  ) => {
    return await placeMarketOrder(symbol, exchange, OrderSide.BUY, quantity, product);
  }, [placeMarketOrder]);

  const quickSell = useCallback(async (
    symbol: string,
    exchange: string,
    quantity: number,
    product: ProductType = ProductType.MIS
  ) => {
    return await placeMarketOrder(symbol, exchange, OrderSide.SELL, quantity, product);
  }, [placeMarketOrder]);

  return {
    // State
    executing: state.executing,
    lastOrder: state.lastOrder,
    lastResult: state.lastResult,
    error: state.error,

    // Actions
    placeOrder,
    placeMarketOrder,
    placeLimitOrder,
    placeStopLossOrder,
    placeOrderOnBroker,
    placeOrderOnAllBrokers,
    placeSmartOrder,
    modifyOrder,
    cancelOrder,
    placeBatchOrders,

    // Quick actions
    quickBuy,
    quickSell,
  };
}
