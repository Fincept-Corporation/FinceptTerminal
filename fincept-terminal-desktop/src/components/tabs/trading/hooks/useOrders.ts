/**
 * Order Management Hook
 * Fetches and manages orders across all exchanges
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import type { UnifiedOrder } from '../types';

export function useOrders(symbol?: string, autoRefresh: boolean = true, refreshInterval: number = 2000) {
  const { activeAdapter, tradingMode, paperAdapter } = useBrokerContext();
  const [openOrders, setOpenOrders] = useState<UnifiedOrder[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchOrders = useCallback(async () => {
    // In paper trading mode, we can use the paperAdapter even if activeAdapter check fails
    const adapter = tradingMode === 'paper' && paperAdapter?.isConnected() ? paperAdapter : activeAdapter;

    if (!adapter || !adapter.isConnected()) {
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawOrders = await adapter.fetchOpenOrders(symbol);

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
  }, [activeAdapter, paperAdapter, tradingMode, symbol]);

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
  const { activeAdapter, tradingMode, paperAdapter } = useBrokerContext();
  const [isCanceling, setIsCanceling] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const cancelOrder = useCallback(
    async (orderId: string, symbol: string) => {
      // In paper trading mode, use paperAdapter
      const adapter = tradingMode === 'paper' && paperAdapter?.isConnected() ? paperAdapter : activeAdapter;

      if (!adapter || !adapter.isConnected()) {
        throw new Error('Exchange not connected');
      }

      setIsCanceling(true);
      setError(null);

      try {
        await adapter.cancelOrder(orderId, symbol);
        return true;
      } catch (err) {
        const error = err as Error;
        setError(error);
        throw error;
      } finally {
        setIsCanceling(false);
      }
    },
    [activeAdapter, tradingMode, paperAdapter]
  );

  const cancelAllOrders = useCallback(
    async (symbol?: string) => {
      // In paper trading mode, use paperAdapter
      const adapter = tradingMode === 'paper' && paperAdapter?.isConnected() ? paperAdapter : activeAdapter;

      if (!adapter || !adapter.isConnected()) {
        throw new Error('Exchange not connected');
      }

      // Check if exchange supports cancel all
      if (typeof adapter.cancelAllOrders !== 'function') {
        throw new Error('This exchange does not support cancel all orders');
      }

      setIsCanceling(true);
      setError(null);

      try {
        await adapter.cancelAllOrders(symbol);
        return true;
      } catch (err) {
        const error = err as Error;
        console.error('[useCancelOrder] Cancel all failed:', error);
        setError(error);
        throw error;
      } finally {
        setIsCanceling(false);
      }
    },
    [activeAdapter, tradingMode, paperAdapter]
  );

  return { cancelOrder, cancelAllOrders, isCanceling, error };
}

/**
 * Hook to fetch closed orders
 * Supports both live exchanges (fetchClosedOrders) and paper trading (getOrders with filled/cancelled status)
 */
export function useClosedOrders(symbol?: string, limit: number = 50, autoRefresh: boolean = true, refreshInterval: number = 5000) {
  const { activeAdapter, tradingMode, paperTradingApi, paperAdapter } = useBrokerContext();
  const [closedOrders, setClosedOrders] = useState<UnifiedOrder[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchClosedOrders = useCallback(async () => {
    // For paper trading, we can fetch even if activeAdapter isn't fully connected
    // because we use the paperTradingApi directly
    const canFetchPaper = tradingMode === 'paper' && paperAdapter && paperTradingApi && paperAdapter.portfolioId;
    const canFetchLive = activeAdapter && activeAdapter.isConnected();

    if (!canFetchPaper && !canFetchLive) {
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      let normalized: UnifiedOrder[] = [];

      // Check if we're in paper trading mode
      if (tradingMode === 'paper' && paperAdapter && paperTradingApi && paperAdapter.portfolioId) {
        // For paper trading, get orders with filled/cancelled status from Rust backend
        const portfolioId = paperAdapter.portfolioId;

        // Get filled orders
        const filledOrders = await paperTradingApi.getOrders(portfolioId, 'filled');
        // Get cancelled orders
        const cancelledOrders = await paperTradingApi.getOrders(portfolioId, 'cancelled');

        // Combine and sort by timestamp (newest first)
        const allClosedOrders = [...filledOrders, ...cancelledOrders]
          .sort((a, b) => new Date(b.created_at).getTime() - new Date(a.created_at).getTime())
          .slice(0, limit);

        normalized = allClosedOrders.map((order: any) => ({
          id: order.id,
          symbol: order.symbol,
          type: order.order_type || order.type,
          side: order.side,
          quantity: order.quantity || order.amount || 0,
          price: order.price,
          stopPrice: order.stop_price || order.stopPrice,
          status: order.status,
          filled: order.filled_qty || order.filled || 0,
          remaining: 0,
          timestamp: new Date(order.created_at).getTime(),
          createdAt: order.created_at || new Date().toISOString(),
          average: order.avg_price || order.average,
          cost: order.price && (order.filled_qty || order.filled)
            ? order.price * (order.filled_qty || order.filled)
            : undefined,
          fee: undefined, // Paper trading tracks fees separately in trades
        }));
      } else if (canFetchLive && typeof (activeAdapter as any).fetchClosedOrders === 'function') {
        // Live trading - use exchange's fetchClosedOrders if available
        const rawOrders = await (activeAdapter as any).fetchClosedOrders(symbol, undefined, limit);

        normalized = (rawOrders || []).map((order: any) => ({
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
      } else {
        // Exchange doesn't support closed orders - return empty
        setClosedOrders([]);
        return;
      }

      // Filter by symbol if specified
      if (symbol) {
        normalized = normalized.filter(o => o.symbol === symbol);
      }

      setClosedOrders(normalized);
    } catch (err) {
      const error = err as Error;
      console.error('[useClosedOrders] Failed to fetch closed orders:', error);
      setError(error);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, tradingMode, paperAdapter, paperTradingApi, symbol, limit]);

  // Initial fetch and auto-refresh
  useEffect(() => {
    fetchClosedOrders();

    if (!autoRefresh) return;

    const interval = setInterval(fetchClosedOrders, refreshInterval);
    return () => clearInterval(interval);
  }, [fetchClosedOrders, autoRefresh, refreshInterval]);

  return {
    closedOrders,
    isLoading,
    error,
    refresh: fetchClosedOrders,
  };
}
