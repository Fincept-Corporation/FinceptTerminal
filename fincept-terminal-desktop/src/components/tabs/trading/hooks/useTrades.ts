/**
 * Trade History Hook
 * Fetches user's trade history using fetchMyTrades from the adapter
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

export interface UnifiedTrade {
  id: string;
  orderId?: string;
  symbol: string;
  side: 'buy' | 'sell';
  type?: string;
  price: number;
  quantity: number;
  cost: number;
  fee?: {
    cost: number;
    currency: string;
  };
  timestamp: number;
  datetime: string;
  takerOrMaker?: 'taker' | 'maker';
}

export function useTrades(symbol?: string, limit: number = 50, autoRefresh: boolean = false) {
  const { activeAdapter, activeBroker, tradingMode, paperAdapter } = useBrokerContext();
  const [trades, setTrades] = useState<UnifiedTrade[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchTrades = useCallback(async () => {
    // In paper trading mode, we can use the paperAdapter even if activeAdapter check fails
    const adapter = tradingMode === 'paper' && paperAdapter?.isConnected() ? paperAdapter : activeAdapter;

    if (!adapter || !adapter.isConnected()) {
      return;
    }

    // Check if exchange supports fetchMyTrades
    if (typeof (adapter as any).fetchMyTrades !== 'function') {
      setIsSupported(false);
      setTrades([]);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawTrades = await (adapter as any).fetchMyTrades(symbol, undefined, limit);

      const normalized: UnifiedTrade[] = (rawTrades || []).map((trade: any) => ({
        id: trade.id,
        orderId: trade.order,
        symbol: trade.symbol,
        side: trade.side,
        type: trade.type,
        price: trade.price || 0,
        quantity: trade.amount || 0,
        cost: trade.cost || (trade.price * trade.amount) || 0,
        fee: trade.fee ? {
          cost: trade.fee.cost || 0,
          currency: trade.fee.currency || 'USD',
        } : undefined,
        timestamp: trade.timestamp || Date.now(),
        datetime: trade.datetime || new Date().toISOString(),
        takerOrMaker: trade.takerOrMaker,
      }));

      // Sort by timestamp descending (newest first)
      normalized.sort((a, b) => b.timestamp - a.timestamp);
      setTrades(normalized);
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useTrades] Failed to fetch trades:', error);
      setError(error);

      // Check if it's an "unsupported" error
      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, paperAdapter, tradingMode, symbol, limit]);

  // Fetch on mount and when dependencies change
  useEffect(() => {
    setTrades([]); // Clear trades when broker changes
    fetchTrades();

    if (!autoRefresh) return;

    const interval = setInterval(fetchTrades, 10000); // Refresh every 10s
    return () => clearInterval(interval);
  }, [fetchTrades, autoRefresh, activeBroker]);

  return {
    trades,
    isLoading,
    error,
    isSupported,
    refresh: fetchTrades,
  };
}

/**
 * Hook to fetch trades for a specific order
 */
export function useOrderTrades(orderId: string, symbol: string) {
  const { activeAdapter, tradingMode, paperAdapter } = useBrokerContext();
  const [trades, setTrades] = useState<UnifiedTrade[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchOrderTrades = useCallback(async () => {
    // In paper trading mode, use paperAdapter
    const adapter = tradingMode === 'paper' && paperAdapter?.isConnected() ? paperAdapter : activeAdapter;

    if (!adapter || !adapter.isConnected() || !orderId || !symbol) {
      setTrades([]);
      return;
    }

    if (typeof (adapter as any).fetchOrderTrades !== 'function') {
      setTrades([]);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawTrades = await (adapter as any).fetchOrderTrades(orderId, symbol);

      const normalized: UnifiedTrade[] = (rawTrades || []).map((trade: any) => ({
        id: trade.id,
        orderId: trade.order,
        symbol: trade.symbol,
        side: trade.side,
        type: trade.type,
        price: trade.price || 0,
        quantity: trade.amount || 0,
        cost: trade.cost || 0,
        fee: trade.fee ? {
          cost: trade.fee.cost || 0,
          currency: trade.fee.currency || 'USD',
        } : undefined,
        timestamp: trade.timestamp || Date.now(),
        datetime: trade.datetime || new Date().toISOString(),
        takerOrMaker: trade.takerOrMaker,
      }));

      setTrades(normalized);
    } catch (err) {
      const error = err as Error;
      console.error('[useOrderTrades] Failed to fetch order trades:', error);
      setError(error);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, tradingMode, paperAdapter, orderId, symbol]);

  useEffect(() => {
    fetchOrderTrades();
  }, [fetchOrderTrades]);

  return {
    trades,
    isLoading,
    error,
    refresh: fetchOrderTrades,
  };
}
