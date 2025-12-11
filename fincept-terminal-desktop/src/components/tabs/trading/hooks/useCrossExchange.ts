/**
 * useCrossExchange - Cross-exchange portfolio aggregation and arbitrage detection
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';

// Types
export interface CrossExchangeBalance {
  exchange: string;
  currency: string;
  total: number;
  free: number;
  used: number;
  usdValue: number;
}

export interface AggregatedBalance {
  currency: string;
  totalAmount: number;
  totalUsdValue: number;
  exchanges: {
    exchange: string;
    amount: number;
    usdValue: number;
  }[];
}

export interface ArbitrageOpportunity {
  symbol: string;
  buyExchange: string;
  sellExchange: string;
  buyPrice: number;
  sellPrice: number;
  spread: number;
  spreadPercent: number;
  potentialProfit: number;
  timestamp: number;
}

export interface CrossExchangePosition {
  symbol: string;
  exchange: string;
  side: 'long' | 'short';
  quantity: number;
  entryPrice: number;
  currentPrice: number;
  unrealizedPnl: number;
  pnlPercent: number;
}

/**
 * Hook for cross-exchange portfolio aggregation
 */
export function useCrossExchangePortfolio() {
  const { availableBrokers } = useBrokerContext();
  const [balances, setBalances] = useState<CrossExchangeBalance[]>([]);
  const [aggregatedBalances, setAggregatedBalances] = useState<AggregatedBalance[]>([]);
  const [totalPortfolioValue, setTotalPortfolioValue] = useState<number>(0);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchCrossExchangeBalances = useCallback(async () => {
    setIsLoading(true);
    setError(null);

    try {
      const allBalances: CrossExchangeBalance[] = [];

      // Mock data for demonstration (in production, fetch from all connected exchanges)
      const mockBalances: CrossExchangeBalance[] = [
        // Kraken
        { exchange: 'kraken', currency: 'BTC', total: 0.5, free: 0.3, used: 0.2, usdValue: 21625 },
        { exchange: 'kraken', currency: 'ETH', total: 10, free: 8, used: 2, usdValue: 23456 },
        { exchange: 'kraken', currency: 'USDC', total: 5000, free: 4500, used: 500, usdValue: 5000 },

        // HyperLiquid
        { exchange: 'hyperliquid', currency: 'BTC', total: 0.3, free: 0.3, used: 0, usdValue: 12975 },
        { exchange: 'hyperliquid', currency: 'ETH', total: 5, free: 3, used: 2, usdValue: 11728 },
        { exchange: 'hyperliquid', currency: 'USDC', total: 10000, free: 8000, used: 2000, usdValue: 10000 },
      ];

      allBalances.push(...mockBalances);
      setBalances(allBalances);

      // Aggregate by currency
      const aggregated = new Map<string, AggregatedBalance>();

      for (const balance of allBalances) {
        if (!aggregated.has(balance.currency)) {
          aggregated.set(balance.currency, {
            currency: balance.currency,
            totalAmount: 0,
            totalUsdValue: 0,
            exchanges: [],
          });
        }

        const agg = aggregated.get(balance.currency)!;
        agg.totalAmount += balance.total;
        agg.totalUsdValue += balance.usdValue;
        agg.exchanges.push({
          exchange: balance.exchange,
          amount: balance.total,
          usdValue: balance.usdValue,
        });
      }

      const aggregatedArray = Array.from(aggregated.values());
      setAggregatedBalances(aggregatedArray);

      // Calculate total portfolio value
      const totalValue = aggregatedArray.reduce((sum, bal) => sum + bal.totalUsdValue, 0);
      setTotalPortfolioValue(totalValue);

    } catch (err: any) {
      setError(err?.message || 'Failed to fetch cross-exchange balances');
      console.error('Cross-exchange balance error:', err);
    } finally {
      setIsLoading(false);
    }
  }, [availableBrokers]);

  useEffect(() => {
    fetchCrossExchangeBalances();
    // Auto-refresh every 30 seconds
    const interval = setInterval(fetchCrossExchangeBalances, 30000);
    return () => clearInterval(interval);
  }, [fetchCrossExchangeBalances]);

  return {
    balances,
    aggregatedBalances,
    totalPortfolioValue,
    isLoading,
    error,
    refresh: fetchCrossExchangeBalances,
  };
}

/**
 * Hook for arbitrage opportunity detection
 */
export function useArbitrageDetection(symbols: string[] = ['BTC/USDC', 'ETH/USDC']) {
  const { availableBrokers } = useBrokerContext();
  const [opportunities, setOpportunities] = useState<ArbitrageOpportunity[]>([]);
  const [isScanning, setIsScanning] = useState(false);
  const [lastScanTime, setLastScanTime] = useState<number | null>(null);

  const scanArbitrageOpportunities = useCallback(async () => {
    setIsScanning(true);

    try {
      const foundOpportunities: ArbitrageOpportunity[] = [];

      // Mock arbitrage opportunities (in production, fetch real prices from all exchanges)
      const mockOpportunities: ArbitrageOpportunity[] = [
        {
          symbol: 'BTC/USDC',
          buyExchange: 'hyperliquid',
          sellExchange: 'kraken',
          buyPrice: 43150.50,
          sellPrice: 43285.75,
          spread: 135.25,
          spreadPercent: 0.31,
          potentialProfit: 135.25,
          timestamp: Date.now(),
        },
        {
          symbol: 'ETH/USDC',
          buyExchange: 'kraken',
          sellExchange: 'hyperliquid',
          buyPrice: 2340.25,
          sellPrice: 2358.50,
          spread: 18.25,
          spreadPercent: 0.78,
          potentialProfit: 18.25,
          timestamp: Date.now(),
        },
      ];

      // Filter opportunities with spread > 0.2%
      const viableOpportunities = mockOpportunities.filter(
        (opp) => opp.spreadPercent > 0.2
      );

      foundOpportunities.push(...viableOpportunities);
      setOpportunities(foundOpportunities);
      setLastScanTime(Date.now());

    } catch (err) {
      console.error('Arbitrage scan error:', err);
    } finally {
      setIsScanning(false);
    }
  }, [symbols, availableBrokers]);

  useEffect(() => {
    scanArbitrageOpportunities();
    // Auto-scan every 10 seconds
    const interval = setInterval(scanArbitrageOpportunities, 10000);
    return () => clearInterval(interval);
  }, [scanArbitrageOpportunities]);

  return {
    opportunities,
    isScanning,
    lastScanTime,
    scan: scanArbitrageOpportunities,
  };
}

/**
 * Hook for cross-exchange positions aggregation
 */
export function useCrossExchangePositions() {
  const { availableBrokers } = useBrokerContext();
  const [positions, setPositions] = useState<CrossExchangePosition[]>([]);
  const [totalUnrealizedPnl, setTotalUnrealizedPnl] = useState<number>(0);
  const [isLoading, setIsLoading] = useState(true);

  const fetchCrossExchangePositions = useCallback(async () => {
    setIsLoading(true);

    try {
      // Mock positions (in production, fetch from all connected exchanges)
      const mockPositions: CrossExchangePosition[] = [
        {
          symbol: 'BTC/USDC',
          exchange: 'kraken',
          side: 'long',
          quantity: 0.2,
          entryPrice: 42500,
          currentPrice: 43250,
          unrealizedPnl: 150,
          pnlPercent: 1.76,
        },
        {
          symbol: 'ETH/USDC',
          exchange: 'hyperliquid',
          side: 'long',
          quantity: 2,
          entryPrice: 2300,
          currentPrice: 2345,
          unrealizedPnl: 90,
          pnlPercent: 1.96,
        },
        {
          symbol: 'SOL/USDC',
          exchange: 'kraken',
          side: 'short',
          quantity: 10,
          entryPrice: 105,
          currentPrice: 98,
          unrealizedPnl: 70,
          pnlPercent: 6.67,
        },
      ];

      setPositions(mockPositions);

      const totalPnl = mockPositions.reduce((sum, pos) => sum + pos.unrealizedPnl, 0);
      setTotalUnrealizedPnl(totalPnl);

    } catch (err) {
      console.error('Cross-exchange positions error:', err);
    } finally {
      setIsLoading(false);
    }
  }, [availableBrokers]);

  useEffect(() => {
    fetchCrossExchangePositions();
    // Auto-refresh every 5 seconds
    const interval = setInterval(fetchCrossExchangePositions, 5000);
    return () => clearInterval(interval);
  }, [fetchCrossExchangePositions]);

  return {
    positions,
    totalUnrealizedPnl,
    isLoading,
    refresh: fetchCrossExchangePositions,
  };
}
