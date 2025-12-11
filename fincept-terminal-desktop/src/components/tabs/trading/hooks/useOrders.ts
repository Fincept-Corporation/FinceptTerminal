/**
 * Order Management Hook
 * Fetches and manages orders across all exchanges
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import type { UnifiedOrder } from '../types';

export function useOrders(symbol?: string, autoRefresh: boolean = true, refreshInterval: number = 2000) {
  const { activeAdapter } = useBrokerContext();
  const [openOrders, setOpenOrders] = useState<UnifiedOrder[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchOrders = useCallback(async () => {
    if (!activeAdapter || !activeAdapter.isConnected()) {
      setOpenOrders([]);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawOrders = await activeAdapter.fetchOpenOrders(symbol);

      // Normalize to unified format
      const normalized: UnifiedOrder[] = (rawOrders || []).map((order: any) => ({
        id: order.id,
        symbol: order.symbol,
        type: order.type,
        side: order.side,
        quantity: order.amount || 0,
        price: order.price,
        stopPrice: order.stopPrice,
        status: order.status,
        filled: order.filled || 0,
        remaining: order.remaining || order.amount || 0,
        timestamp: order.timestamp || Date.now(),
        createdAt: order.datetime || new Date().toISOString(),
        updatedAt: order.lastUpdateTimestamp ? new Date(order.lastUpdateTimestamp).toISOString() : undefined,
        average: order.average,
        cost: order.cost,
        fee: order.fee
          ? {
              cost: order.fee.cost || 0,
              currency: order.fee.currency || 'USD',
            }
          : undefined,
        clientOrderId: order.clientOrderId,
        timeInForce: order.timeInForce,
        reduceOnly: order.reduceOnly,
        postOnly: order.postOnly,
      }));

      setOpenOrders(normalized);
    } catch (err) {
      const error = err as Error;
      console.error('[useOrders] Failed to fetch orders:', error);
      setError(error);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol]);

  // Auto-refresh
  useEffect(() => {
    fetchOrders();

    if (!autoRefresh) return;

    const interval = setInterval(fetchOrders, refreshInterval);
    return () => clearInterval(interval);
  }, [fetchOrders, autoRefresh, refreshInterval]);

  return {
    openOrders,
    isLoading,
    error,
    refresh: fetchOrders,
  };
}

/**
 * Hook to cancel order(s)
 */
export function useCancelOrder() {
  const { activeAdapter } = useBrokerContext();
  const [isCanceling, setIsCanceling] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const cancelOrder = useCallback(
    async (orderId: string, symbol: string) => {
      if (!activeAdapter || !activeAdapter.isConnected()) {
        throw new Error('Exchange not connected');
      }

      setIsCanceling(true);
      setError(null);

      try {
        await activeAdapter.cancelOrder(orderId, symbol);
        return true;
      } catch (err) {
        const error = err as Error;
        setError(error);
        throw error;
      } finally {
        setIsCanceling(false);
      }
    },
    [activeAdapter]
  );

  const cancelAllOrders = useCallback(
    async (symbol?: string) => {
      if (!activeAdapter || !activeAdapter.isConnected()) {
        throw new Error('Exchange not connected');
      }

      // Check if exchange supports cancel all
      if (typeof (activeAdapter as any).cancelAllOrders !== 'function') {
        throw new Error('This exchange does not support cancel all orders');
      }

      setIsCanceling(true);
      setError(null);

      try {
        await (activeAdapter as any).cancelAllOrders(symbol);
        return true;
      } catch (err) {
        const error = err as Error;
        setError(error);
        throw error;
      } finally {
        setIsCanceling(false);
      }
    },
    [activeAdapter]
  );

  return { cancelOrder, cancelAllOrders, isCanceling, error };
}

/**
 * Hook to fetch closed orders
 */
export function useClosedOrders(symbol?: string, limit: number = 50) {
  const { activeAdapter } = useBrokerContext();
  const [closedOrders, setClosedOrders] = useState<UnifiedOrder[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchClosedOrders = useCallback(async () => {
    if (!activeAdapter || !activeAdapter.isConnected()) {
      setClosedOrders([]);
      return;
    }

    // Check if exchange supports fetching closed orders
    if (typeof (activeAdapter as any).fetchClosedOrders !== 'function') {
      setClosedOrders([]);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawOrders = await (activeAdapter as any).fetchClosedOrders(symbol, undefined, limit);

      const normalized: UnifiedOrder[] = (rawOrders || []).map((order: any) => ({
        id: order.id,
        symbol: order.symbol,
        type: order.type,
        side: order.side,
        quantity: order.amount || 0,
        price: order.price,
        stopPrice: order.stopPrice,
        status: order.status,
        filled: order.filled || 0,
        remaining: 0,
        timestamp: order.timestamp || Date.now(),
        createdAt: order.datetime || new Date().toISOString(),
        average: order.average,
        cost: order.cost,
        fee: order.fee
          ? {
              cost: order.fee.cost || 0,
              currency: order.fee.currency || 'USD',
            }
          : undefined,
      }));

      setClosedOrders(normalized);
    } catch (err) {
      const error = err as Error;
      console.error('[useClosedOrders] Failed to fetch closed orders:', error);
      setError(error);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, limit]);

  useEffect(() => {
    fetchClosedOrders();
  }, [fetchClosedOrders]);

  return {
    closedOrders,
    isLoading,
    error,
    refresh: fetchClosedOrders,
  };
}
