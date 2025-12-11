/**
 * Account Information Hooks
 * Fetches balance, fees, margin, and account statistics
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import type { UnifiedBalance, UnifiedAccount, TradingStatistics } from '../types';

// ============================================================================
// BALANCE HOOK
// ============================================================================

export function useBalance(autoRefresh: boolean = true, refreshInterval: number = 5000) {
  const { activeAdapter } = useBrokerContext();
  const [balances, setBalances] = useState<UnifiedBalance[]>([]);
  const [totalEquity, setTotalEquity] = useState<number>(0);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchBalance = useCallback(async () => {
    if (!activeAdapter || !activeAdapter.isConnected()) {
      setBalances([]);
      setTotalEquity(0);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawBalance = await activeAdapter.fetchBalance();

      // Normalize balances
      const normalizedBalances: UnifiedBalance[] = Object.entries(rawBalance || {})
        .filter(([currency, data]: [string, any]) => data.total > 0)
        .map(([currency, data]: [string, any]) => ({
          currency,
          free: data.free || 0,
          used: data.used || 0,
          total: data.total || 0,
        }));

      setBalances(normalizedBalances);

      // Calculate total equity (simplified - just sum USD/USDC/USDT)
      const equity = normalizedBalances
        .filter((b) => ['USD', 'USDC', 'USDT'].includes(b.currency))
        .reduce((sum, b) => sum + b.total, 0);

      setTotalEquity(equity);
    } catch (err) {
      const error = err as Error;
      console.error('[useBalance] Failed to fetch balance:', error);
      setError(error);
      setBalances([]);
      setTotalEquity(0);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter]);

  useEffect(() => {
    fetchBalance();

    if (!autoRefresh) return;

    const interval = setInterval(fetchBalance, refreshInterval);
    return () => clearInterval(interval);
  }, [fetchBalance, autoRefresh, refreshInterval]);

  return {
    balances,
    totalEquity,
    isLoading,
    error,
    refresh: fetchBalance,
  };
}

// ============================================================================
// FEES HOOK
// ============================================================================

export interface FeeStructure {
  maker: number;
  taker: number;
  makerPercent: string;
  takerPercent: string;
}

export function useFees() {
  const { activeAdapter, activeBroker } = useBrokerContext();
  const [fees, setFees] = useState<FeeStructure | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchFees = useCallback(async () => {
    if (!activeAdapter) {
      setFees(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Try to fetch trading fees from adapter
      const tradingFees = (activeAdapter as any).getTradingFees?.() || null;

      if (tradingFees) {
        setFees({
          maker: tradingFees.maker || 0,
          taker: tradingFees.taker || 0,
          makerPercent: `${((tradingFees.maker || 0) * 100).toFixed(3)}%`,
          takerPercent: `${((tradingFees.taker || 0) * 100).toFixed(3)}%`,
        });
      } else {
        // Default fees based on exchange
        const defaultFees: Record<string, FeeStructure> = {
          kraken: { maker: 0.0016, taker: 0.0026, makerPercent: '0.160%', takerPercent: '0.260%' },
          hyperliquid: { maker: 0.0002, taker: 0.0005, makerPercent: '0.020%', takerPercent: '0.050%' },
          paper: { maker: 0.0002, taker: 0.0005, makerPercent: '0.020%', takerPercent: '0.050%' },
        };

        setFees(defaultFees[activeBroker || 'paper'] || defaultFees.paper);
      }
    } catch (err) {
      const error = err as Error;
      console.error('[useFees] Failed to fetch fees:', error);
      setError(error);
      setFees(null);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, activeBroker]);

  useEffect(() => {
    fetchFees();
  }, [fetchFees]);

  return {
    fees,
    isLoading,
    error,
    refresh: fetchFees,
  };
}

// ============================================================================
// MARGIN INFO HOOK
// ============================================================================

export interface MarginInfo {
  totalMargin: number;
  usedMargin: number;
  availableMargin: number;
  marginLevel: number; // percentage
  maintenanceMargin: number;
  isMarginCall: boolean;
}

export function useMarginInfo(autoRefresh: boolean = true) {
  const { activeAdapter } = useBrokerContext();
  const [marginInfo, setMarginInfo] = useState<MarginInfo | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchMarginInfo = useCallback(async () => {
    if (!activeAdapter || !activeAdapter.isConnected()) {
      setMarginInfo(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Check if adapter supports margin info
      const balance = await activeAdapter.fetchBalance();

      // Extract margin info if available
      const info = (balance as any).info;

      if (info && info.totalMargin !== undefined) {
        const totalMargin = parseFloat(info.totalMargin || '0');
        const usedMargin = parseFloat(info.usedMargin || '0');
        const availableMargin = totalMargin - usedMargin;
        const marginLevel = totalMargin > 0 ? (availableMargin / totalMargin) * 100 : 100;
        const maintenanceMargin = parseFloat(info.maintenanceMargin || '0');

        setMarginInfo({
          totalMargin,
          usedMargin,
          availableMargin,
          marginLevel,
          maintenanceMargin,
          isMarginCall: marginLevel < 20, // Alert if margin level below 20%
        });
      } else {
        // No margin trading on this account/exchange
        setMarginInfo(null);
      }
    } catch (err) {
      const error = err as Error;
      console.error('[useMarginInfo] Failed to fetch margin info:', error);
      setError(error);
      setMarginInfo(null);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter]);

  useEffect(() => {
    fetchMarginInfo();

    if (!autoRefresh) return;

    const interval = setInterval(fetchMarginInfo, 10000); // Update every 10s
    return () => clearInterval(interval);
  }, [fetchMarginInfo, autoRefresh]);

  return {
    marginInfo,
    isLoading,
    error,
    refresh: fetchMarginInfo,
  };
}

// ============================================================================
// TRADING STATISTICS HOOK
// ============================================================================

export function useTradingStats() {
  const { activeAdapter, tradingMode, paperAdapter } = useBrokerContext();
  const [stats, setStats] = useState<TradingStatistics | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchStats = useCallback(async () => {
    const adapter = tradingMode === 'paper' ? paperAdapter : activeAdapter;

    if (!adapter) {
      setStats(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Get statistics from paper trading adapter if available
      if (tradingMode === 'paper' && (adapter as any).getStatistics) {
        const paperStats = await (adapter as any).getStatistics();
        setStats(paperStats);
      } else {
        // For live trading, calculate stats from closed orders
        setStats(null); // Not implemented for live yet
      }
    } catch (err) {
      const error = err as Error;
      console.error('[useTradingStats] Failed to fetch stats:', error);
      setError(error);
      setStats(null);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, paperAdapter, tradingMode]);

  useEffect(() => {
    fetchStats();

    const interval = setInterval(fetchStats, 10000);
    return () => clearInterval(interval);
  }, [fetchStats]);

  return {
    stats,
    isLoading,
    error,
    refresh: fetchStats,
  };
}
