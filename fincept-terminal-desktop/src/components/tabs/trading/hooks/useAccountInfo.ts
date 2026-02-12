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
  const { activeAdapter, tradingMode, paperAdapter } = useBrokerContext();
  const [balances, setBalances] = useState<UnifiedBalance[]>([]);
  const [totalEquity, setTotalEquity] = useState<number>(0);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchBalance = useCallback(async () => {
    // In paper trading mode, use paperAdapter
    const adapter = tradingMode === 'paper' && paperAdapter?.isConnected() ? paperAdapter : activeAdapter;

    if (!adapter || !adapter.isConnected()) {
      setBalances([]);
      setTotalEquity(0);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawBalance = await adapter.fetchBalance();

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
  }, [activeAdapter, tradingMode, paperAdapter]);

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
  const { activeAdapter, activeBroker, tradingMode, paperAdapter } = useBrokerContext();
  const [fees, setFees] = useState<FeeStructure | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchFees = useCallback(async () => {
    // In paper trading mode, use paperAdapter
    const adapter = tradingMode === 'paper' && paperAdapter ? paperAdapter : activeAdapter;

    if (!adapter) {
      setFees(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Try to fetch trading fees from adapter
      const tradingFees = (adapter as any).getTradingFees?.() || null;

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
  }, [activeAdapter, tradingMode, paperAdapter, activeBroker]);

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
  const { activeAdapter, tradingMode, paperAdapter } = useBrokerContext();
  const [marginInfo, setMarginInfo] = useState<MarginInfo | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchMarginInfo = useCallback(async () => {
    // In paper trading mode, use paperAdapter
    const adapter = tradingMode === 'paper' && paperAdapter?.isConnected() ? paperAdapter : activeAdapter;

    if (!adapter || !adapter.isConnected()) {
      setMarginInfo(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Check if adapter supports margin info
      const balance = await adapter.fetchBalance();

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
  }, [activeAdapter, tradingMode, paperAdapter]);

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
// LEVERAGE HOOK
// ============================================================================

export interface LeverageInfo {
  symbol: string;
  leverage: number;
  maxLeverage?: number;
  marginMode?: 'cross' | 'isolated';
}

export function useLeverage(symbol?: string) {
  const { activeAdapter, activeBroker, tradingMode, paperAdapter } = useBrokerContext();
  const [leverageInfo, setLeverageInfo] = useState<LeverageInfo | null>(null);
  const [leverageTiers, setLeverageTiers] = useState<any[] | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [isSupported, setIsSupported] = useState(true);

  const fetchLeverage = useCallback(async () => {
    // In paper trading mode, use paperAdapter
    const adapter = tradingMode === 'paper' && paperAdapter?.isConnected() ? paperAdapter : activeAdapter;

    if (!adapter || !adapter.isConnected() || !symbol) {
      setLeverageInfo(null);
      return;
    }

    // Check if adapter supports fetchLeverage
    if (typeof (adapter as any).fetchLeverage !== 'function') {
      setIsSupported(false);
      setLeverageInfo(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawLeverage = await (adapter as any).fetchLeverage(symbol);

      setLeverageInfo({
        symbol,
        leverage: rawLeverage?.leverage || rawLeverage?.longLeverage || 1,
        maxLeverage: rawLeverage?.maxLeverage,
        marginMode: rawLeverage?.marginMode,
      });
      setIsSupported(true);
    } catch (err) {
      const error = err as Error;
      console.error('[useLeverage] Failed to fetch leverage:', error);

      if (error.message?.includes('not supported')) {
        setIsSupported(false);
      }
      setError(error);
      setLeverageInfo(null);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, tradingMode, paperAdapter, symbol]);

  const fetchTiers = useCallback(async () => {
    // In paper trading mode, use paperAdapter
    const adapter = tradingMode === 'paper' && paperAdapter?.isConnected() ? paperAdapter : activeAdapter;

    if (!adapter || !adapter.isConnected() || !symbol) {
      setLeverageTiers(null);
      return;
    }

    if (typeof (adapter as any).fetchLeverageTiers !== 'function') {
      setLeverageTiers(null);
      return;
    }

    try {
      const tiers = await (adapter as any).fetchLeverageTiers([symbol]);
      setLeverageTiers(tiers?.[symbol] || null);
    } catch (err) {
      console.error('[useLeverage] Failed to fetch leverage tiers:', err);
      setLeverageTiers(null);
    }
  }, [activeAdapter, tradingMode, paperAdapter, symbol]);

  useEffect(() => {
    fetchLeverage();
    fetchTiers();
  }, [fetchLeverage, fetchTiers, activeBroker]);

  return {
    leverageInfo,
    leverageTiers,
    isLoading,
    error,
    isSupported,
    refresh: fetchLeverage,
  };
}

// ============================================================================
// TRADING STATISTICS HOOK
// ============================================================================

export function useTradingStats() {
  const { activeAdapter, tradingMode, paperTradingApi, paperPortfolio } = useBrokerContext();
  const [stats, setStats] = useState<TradingStatistics | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchStats = useCallback(async () => {
    if (!activeAdapter && tradingMode !== 'paper') {
      setStats(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Get statistics from Rust paper trading module
      if (tradingMode === 'paper' && paperPortfolio) {
        const rustStats = await paperTradingApi.getStats(paperPortfolio.id);
        // Get fresh portfolio data for balance info
        const portfolio = await paperTradingApi.getPortfolio(paperPortfolio.id);
        // Get positions for unrealized P&L
        const positions = await paperTradingApi.getPositions(paperPortfolio.id);
        // Get trades for fee calculation
        const trades = await paperTradingApi.getTrades(paperPortfolio.id, 1000);

        const totalPnL = rustStats.total_pnl;
        const currentBalance = portfolio.balance;
        const initialBalance = portfolio.initial_balance;

        // Calculate unrealized P&L from open positions
        const unrealizedPnL = positions.reduce((sum, pos) => sum + (pos.unrealized_pnl || 0), 0);

        // Calculate total fees from trades
        const totalFees = trades.reduce((sum, trade) => sum + (trade.fee || 0), 0);

        // Calculate equity = balance + unrealized P&L
        const totalEquity = currentBalance + unrealizedPnL;

        // Calculate return percent based on equity vs initial
        const returnPercent = initialBalance > 0 ? ((totalEquity - initialBalance) / initialBalance) * 100 : 0;

        setStats({
          // Account
          initialBalance,
          currentBalance,
          totalEquity,

          // P&L
          totalPnL: totalPnL + unrealizedPnL,
          realizedPnL: totalPnL,
          unrealizedPnL,
          returnPercent,

          // Trading
          totalTrades: rustStats.total_trades,
          winningTrades: rustStats.winning_trades,
          losingTrades: rustStats.losing_trades,
          winRate: rustStats.win_rate,

          // Performance
          largestWin: rustStats.largest_win,
          largestLoss: rustStats.largest_loss,

          // Fees
          totalFees,
        });
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
  }, [activeAdapter, tradingMode, paperPortfolio, paperTradingApi]);

  useEffect(() => {
    // Add isMounted ref for cleanup
    let isMounted = true;

    const runFetch = async () => {
      if (isMounted) {
        await fetchStats();
      }
    };

    runFetch();

    const interval = setInterval(() => {
      if (isMounted) {
        runFetch();
      }
    }, 10000);

    return () => {
      isMounted = false;
      clearInterval(interval);
    };
  }, [fetchStats]);

  return {
    stats,
    isLoading,
    error,
    refresh: fetchStats,
  };
}
