/**
 * Advanced Order Hooks
 * Batch orders, market orders with cost, cancel all orders
 */

import { useState, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

// ============================================================================
// TYPES
// ============================================================================

export interface OrderParams {
  symbol: string;
  type: 'market' | 'limit' | 'stop' | 'stopLimit';
  side: 'buy' | 'sell';
  amount: number;
  price?: number;
  stopPrice?: number;
  params?: Record<string, any>;
}

export interface BatchOrderResult {
  id: string;
  symbol: string;
  type: string;
  side: string;
  status: 'open' | 'closed' | 'canceled' | 'rejected';
  amount: number;
  price?: number;
  filled?: number;
  remaining?: number;
  error?: string;
  info?: any;
}

export interface CancelOrderResult {
  id: string;
  symbol: string;
  status: 'canceled' | 'failed';
  error?: string;
  info?: any;
}

// ============================================================================
// CREATE ORDERS (BATCH) HOOK
// ============================================================================

export function useCreateOrders() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [results, setResults] = useState<BatchOrderResult[]>([]);
  const [isSupported, setIsSupported] = useState(true);

  const createOrders = useCallback(
    async (orders: OrderParams[]) => {
      if (tradingMode === 'paper') {
        setError(new Error('Batch orders not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).createOrders !== 'function') {
        setIsSupported(false);
        setError(new Error('Batch orders not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).createOrders(orders);

        const normalized: BatchOrderResult[] = (raw || []).map((order: any) => ({
          id: order.id,
          symbol: order.symbol,
          type: order.type,
          side: order.side,
          status: order.status,
          amount: order.amount,
          price: order.price,
          filled: order.filled,
          remaining: order.remaining,
          error: order.error,
          info: order.info,
        }));

        setResults(normalized);
        setIsSupported(true);
        return normalized;
      } catch (err) {
        const error = err as Error;
        console.error('[useCreateOrders] Failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { createOrders, isLoading, error, results, isSupported };
}

// ============================================================================
// CREATE MARKET ORDER WITH COST HOOK
// ============================================================================

export function useCreateMarketOrderWithCost() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [lastOrder, setLastOrder] = useState<BatchOrderResult | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const createMarketBuyWithCost = useCallback(
    async (symbol: string, cost: number, params?: Record<string, any>) => {
      if (tradingMode === 'paper') {
        setError(new Error('Not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).createMarketBuyOrderWithCost !== 'function') {
        setIsSupported(false);
        setError(new Error('Market order with cost not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).createMarketBuyOrderWithCost(symbol, cost, params);

        const order: BatchOrderResult = {
          id: raw.id,
          symbol: raw.symbol,
          type: raw.type,
          side: raw.side,
          status: raw.status,
          amount: raw.amount,
          price: raw.price || raw.average,
          filled: raw.filled,
          remaining: raw.remaining,
          info: raw.info,
        };

        setLastOrder(order);
        setIsSupported(true);
        return order;
      } catch (err) {
        const error = err as Error;
        console.error('[useCreateMarketOrderWithCost] Buy failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  const createMarketSellWithCost = useCallback(
    async (symbol: string, cost: number, params?: Record<string, any>) => {
      if (tradingMode === 'paper') {
        setError(new Error('Not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).createMarketSellOrderWithCost !== 'function') {
        setIsSupported(false);
        setError(new Error('Market order with cost not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).createMarketSellOrderWithCost(symbol, cost, params);

        const order: BatchOrderResult = {
          id: raw.id,
          symbol: raw.symbol,
          type: raw.type,
          side: raw.side,
          status: raw.status,
          amount: raw.amount,
          price: raw.price || raw.average,
          filled: raw.filled,
          remaining: raw.remaining,
          info: raw.info,
        };

        setLastOrder(order);
        setIsSupported(true);
        return order;
      } catch (err) {
        const error = err as Error;
        console.error('[useCreateMarketOrderWithCost] Sell failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { createMarketBuyWithCost, createMarketSellWithCost, isLoading, error, lastOrder, isSupported };
}

// ============================================================================
// CANCEL ORDERS (BATCH) HOOK
// ============================================================================

export function useCancelOrders() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [results, setResults] = useState<CancelOrderResult[]>([]);
  const [isSupported, setIsSupported] = useState(true);

  const cancelOrders = useCallback(
    async (orderIds: string[], symbol?: string) => {
      if (tradingMode === 'paper') {
        setError(new Error('Batch cancel not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).cancelOrders !== 'function') {
        setIsSupported(false);
        setError(new Error('Batch cancel not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).cancelOrders(orderIds, symbol);

        const normalized: CancelOrderResult[] = (raw || []).map((result: any, index: number) => ({
          id: result.id || orderIds[index],
          symbol: result.symbol || symbol,
          status: result.status === 'canceled' || result.success ? 'canceled' : 'failed',
          error: result.error,
          info: result.info,
        }));

        setResults(normalized);
        setIsSupported(true);
        return normalized;
      } catch (err) {
        const error = err as Error;
        console.error('[useCancelOrders] Failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { cancelOrders, isLoading, error, results, isSupported };
}

// ============================================================================
// CANCEL ALL ORDERS HOOK
// ============================================================================

export function useCancelAllOrders() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [results, setResults] = useState<CancelOrderResult[]>([]);
  const [isSupported, setIsSupported] = useState(true);

  const cancelAllOrders = useCallback(
    async (symbol?: string, params?: { side?: 'buy' | 'sell' }) => {
      if (tradingMode === 'paper') {
        setError(new Error('Cancel all not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).cancelAllOrders !== 'function') {
        setIsSupported(false);
        setError(new Error('Cancel all orders not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).cancelAllOrders(symbol, params);

        const normalized: CancelOrderResult[] = Array.isArray(raw)
          ? raw.map((result: any) => ({
              id: result.id,
              symbol: result.symbol || symbol,
              status: 'canceled' as const,
              info: result.info,
            }))
          : [];

        setResults(normalized);
        setIsSupported(true);
        return normalized;
      } catch (err) {
        const error = err as Error;
        console.error('[useCancelAllOrders] Failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { cancelAllOrders, isLoading, error, results, isSupported };
}

// ============================================================================
// CANCEL ALL ORDERS AFTER (TIMEOUT) HOOK
// ============================================================================

export function useCancelAllOrdersAfter() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);
  const [scheduledTime, setScheduledTime] = useState<number | null>(null);

  const cancelAllOrdersAfter = useCallback(
    async (timeout: number, params?: Record<string, any>) => {
      if (tradingMode === 'paper') {
        setError(new Error('Not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).cancelAllOrdersAfter !== 'function') {
        setIsSupported(false);
        setError(new Error('Cancel orders after timeout not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).cancelAllOrdersAfter(timeout, params);

        const cancelTime = raw?.triggerTime || Date.now() + timeout * 1000;
        setScheduledTime(cancelTime);
        setIsSupported(true);
        return { triggerTime: cancelTime, info: raw?.info };
      } catch (err) {
        const error = err as Error;
        console.error('[useCancelAllOrdersAfter] Failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { cancelAllOrdersAfter, isLoading, error, scheduledTime, isSupported };
}

// ============================================================================
// EDIT ORDER HOOK
// ============================================================================

export function useEditOrder() {
  const { activeAdapter, tradingMode } = useBrokerContext();
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [lastResult, setLastResult] = useState<BatchOrderResult | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const editOrder = useCallback(
    async (
      orderId: string,
      symbol: string,
      type: string,
      side: 'buy' | 'sell',
      amount?: number,
      price?: number,
      params?: Record<string, any>
    ) => {
      if (tradingMode === 'paper') {
        setError(new Error('Edit order not available in paper trading mode'));
        return null;
      }

      if (!activeAdapter || !activeAdapter.isConnected()) {
        setError(new Error('Not connected to exchange'));
        return null;
      }

      if (typeof (activeAdapter as any).editOrder !== 'function') {
        setIsSupported(false);
        setError(new Error('Edit order not supported by this exchange'));
        return null;
      }

      setIsLoading(true);
      setError(null);

      try {
        const raw = await (activeAdapter as any).editOrder(orderId, symbol, type, side, amount, price, params);

        const result: BatchOrderResult = {
          id: raw.id,
          symbol: raw.symbol,
          type: raw.type,
          side: raw.side,
          status: raw.status,
          amount: raw.amount,
          price: raw.price,
          filled: raw.filled,
          remaining: raw.remaining,
          info: raw.info,
        };

        setLastResult(result);
        setIsSupported(true);
        return result;
      } catch (err) {
        const error = err as Error;
        console.error('[useEditOrder] Failed:', error);
        setError(error);
        if (error.message?.includes('not supported')) {
          setIsSupported(false);
        }
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [activeAdapter, tradingMode]
  );

  return { editOrder, isLoading, error, lastResult, isSupported };
}
