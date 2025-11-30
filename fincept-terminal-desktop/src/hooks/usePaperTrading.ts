// File: src/hooks/usePaperTrading.ts
// React hook for paper trading functionality

import { useState, useEffect, useCallback, useRef } from 'react';
import { createPaperTradingService } from '../services/trading';
import type {
  VirtualPortfolio,
  Position,
  Order,
  OrderRequest,
  OrderResult
} from '../types/trading';

interface UsePaperTradingOptions {
  portfolioId?: string;
  autoRefreshInterval?: number; // milliseconds, default 1000
}

interface UsePaperTradingReturn {
  portfolio: VirtualPortfolio | null;
  positions: Position[];
  orders: Order[];
  isLoading: boolean;
  error: string | null;
  placeOrder: (order: OrderRequest) => Promise<OrderResult>;
  cancelOrder: (orderId: string) => Promise<boolean>;
  closePosition: (positionId: string) => Promise<boolean>;
  resetPortfolio: () => Promise<boolean>;
  refresh: () => Promise<void>;
}

export function usePaperTrading(
  options: UsePaperTradingOptions = {}
): UsePaperTradingReturn {
  const { portfolioId, autoRefreshInterval = 1000 } = options;

  const [portfolio, setPortfolio] = useState<VirtualPortfolio | null>(null);
  const [positions, setPositions] = useState<Position[]>([]);
  const [orders, setOrders] = useState<Order[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const serviceRef = useRef(createPaperTradingService());
  const refreshIntervalRef = useRef<NodeJS.Timeout | null>(null);

  // Load portfolio data
  const loadPortfolio = useCallback(async () => {
    if (!portfolioId) {
      setPortfolio(null);
      setPositions([]);
      setOrders([]);
      return;
    }

    try {
      const service = serviceRef.current;
      await service.initialize();

      const [portfolioData, positionsData, ordersData] = await Promise.all([
        service.getPortfolio(portfolioId),
        service.getPositions(portfolioId, 'all'),
        service.getOrders(portfolioId)
      ]);

      setPortfolio(portfolioData);
      setPositions(positionsData);
      setOrders(ordersData);
      setError(null);
    } catch (err) {
      console.error('[usePaperTrading] Failed to load portfolio:', err);
      setError(err instanceof Error ? err.message : 'Failed to load portfolio');
    }
  }, [portfolioId]);

  // Initial load
  useEffect(() => {
    setIsLoading(true);
    loadPortfolio().finally(() => setIsLoading(false));
  }, [loadPortfolio]);

  // Auto-refresh positions (for real-time P&L updates)
  useEffect(() => {
    if (!portfolioId || !autoRefreshInterval) return;

    refreshIntervalRef.current = setInterval(() => {
      loadPortfolio();
    }, autoRefreshInterval);

    return () => {
      if (refreshIntervalRef.current) {
        clearInterval(refreshIntervalRef.current);
      }
    };
  }, [portfolioId, autoRefreshInterval, loadPortfolio]);

  // Place order
  const placeOrder = useCallback(async (order: OrderRequest): Promise<OrderResult> => {
    if (!portfolioId) {
      return { success: false, error: 'No portfolio selected' };
    }

    setIsLoading(true);
    try {
      const service = serviceRef.current;
      const result = await service.placeOrder(portfolioId, order);

      if (result.success) {
        // Refresh data
        await loadPortfolio();
      }

      setError(null);
      return result;
    } catch (err) {
      const errorMsg = err instanceof Error ? err.message : 'Failed to place order';
      setError(errorMsg);
      return { success: false, error: errorMsg };
    } finally {
      setIsLoading(false);
    }
  }, [portfolioId, loadPortfolio]);

  // Cancel order
  const cancelOrder = useCallback(async (orderId: string): Promise<boolean> => {
    setIsLoading(true);
    try {
      const service = serviceRef.current;
      const success = await service.cancelOrder(orderId);

      if (success) {
        await loadPortfolio();
      }

      setError(null);
      return success;
    } catch (err) {
      console.error('[usePaperTrading] Failed to cancel order:', err);
      setError(err instanceof Error ? err.message : 'Failed to cancel order');
      return false;
    } finally {
      setIsLoading(false);
    }
  }, [loadPortfolio]);

  // Close position
  const closePosition = useCallback(async (positionId: string): Promise<boolean> => {
    setIsLoading(true);
    try {
      const service = serviceRef.current;
      const success = await service.closePosition(positionId);

      if (success) {
        await loadPortfolio();
      }

      setError(null);
      return success;
    } catch (err) {
      console.error('[usePaperTrading] Failed to close position:', err);
      setError(err instanceof Error ? err.message : 'Failed to close position');
      return false;
    } finally {
      setIsLoading(false);
    }
  }, [loadPortfolio]);

  // Reset portfolio
  const resetPortfolio = useCallback(async (): Promise<boolean> => {
    if (!portfolioId) return false;

    setIsLoading(true);
    try {
      const service = serviceRef.current;
      const success = await service.resetPortfolio(portfolioId);

      if (success) {
        await loadPortfolio();
      }

      setError(null);
      return success;
    } catch (err) {
      console.error('[usePaperTrading] Failed to reset portfolio:', err);
      setError(err instanceof Error ? err.message : 'Failed to reset portfolio');
      return false;
    } finally {
      setIsLoading(false);
    }
  }, [portfolioId, loadPortfolio]);

  // Manual refresh
  const refresh = useCallback(async () => {
    setIsLoading(true);
    await loadPortfolio();
    setIsLoading(false);
  }, [loadPortfolio]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      serviceRef.current.cleanup();
      if (refreshIntervalRef.current) {
        clearInterval(refreshIntervalRef.current);
      }
    };
  }, []);

  return {
    portfolio,
    positions,
    orders,
    isLoading,
    error,
    placeOrder,
    cancelOrder,
    closePosition,
    resetPortfolio,
    refresh
  };
}

/**
 * Hook for creating a new paper trading portfolio
 */
export function useCreatePaperPortfolio() {
  const [isCreating, setIsCreating] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const serviceRef = useRef(createPaperTradingService());

  const createPortfolio = useCallback(async (
    name: string,
    provider: string,
    initialBalance: number = 100000
  ): Promise<string | null> => {
    setIsCreating(true);
    setError(null);

    try {
      const service = serviceRef.current;
      await service.initialize();

      const portfolioId = await service.createPortfolio(name, provider, initialBalance);
      return portfolioId;
    } catch (err) {
      console.error('[useCreatePaperPortfolio] Failed to create portfolio:', err);
      setError(err instanceof Error ? err.message : 'Failed to create portfolio');
      return null;
    } finally {
      setIsCreating(false);
    }
  }, []);

  return {
    createPortfolio,
    isCreating,
    error
  };
}
