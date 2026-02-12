/**
 * useCrossExchange - Cross-exchange portfolio aggregation and arbitrage detection
 * Uses real data from connected brokers and paper trading
 */

import { useState, useEffect, useCallback } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import { getMarketDataService } from '../../../../services/trading/UnifiedMarketDataService';

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
 * Fetches real balances from paper trading and connected exchanges
 */
export function useCrossExchangePortfolio() {
  const { activeAdapter, tradingMode, paperTradingApi, paperPortfolio, activeBroker } = useBrokerContext();
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

      // Fetch from paper trading if in paper mode
      if (tradingMode === 'paper' && paperPortfolio && paperTradingApi) {
        try {
          const portfolio = await paperTradingApi.getPortfolio(paperPortfolio.id);
          const positions = await paperTradingApi.getPositions(paperPortfolio.id);

          // Calculate total unrealized P&L
          let totalUnrealizedPnl = 0;
          for (const pos of positions) {
            totalUnrealizedPnl += pos.unrealized_pnl || 0;
          }

          // For derivatives/margin trading:
          // - Balance = Available cash (what you can use for new trades)
          // - Equity = Balance + Unrealized P&L (total account value)
          const equity = portfolio.balance + totalUnrealizedPnl;

          // Show equity as the total portfolio value
          allBalances.push({
            exchange: 'paper',
            currency: 'USD',
            total: equity,
            free: portfolio.balance,
            used: totalUnrealizedPnl,
            usdValue: equity,
          });
        } catch (err) {
          console.warn('[useCrossExchangePortfolio] Paper trading fetch error:', err);
        }
      }

      // Fetch from real adapter if connected
      if (activeAdapter && activeAdapter.isConnected() && tradingMode === 'live') {
        try {
          const balance = await activeAdapter.fetchBalance();

          if (balance) {
            // Process free balances
            const freeBalances = balance.free || {};
            const usedBalances = balance.used || {};
            const totalBalances = balance.total || {};

            for (const [currency, amount] of Object.entries(totalBalances)) {
              const total = typeof amount === 'number' ? amount : 0;
              if (total <= 0) continue;

              const free = (freeBalances as any)[currency] || 0;
              const used = (usedBalances as any)[currency] || 0;

              // Get USD value - for stablecoins use 1:1, for others try to get price
              let usdValue = total;
              if (!['USD', 'USDT', 'USDC', 'DAI', 'BUSD'].includes(currency)) {
                const marketData = getMarketDataService();
                const priceData = marketData.getCurrentPrice(`${currency}/USD`, activeBroker || undefined) ||
                                  marketData.getCurrentPrice(`${currency}/USDT`, activeBroker || undefined);
                if (priceData) {
                  usdValue = total * priceData.last;
                }
              }

              allBalances.push({
                exchange: activeBroker || 'unknown',
                currency,
                total,
                free,
                used,
                usdValue,
              });
            }
          }
        } catch (err) {
          console.warn('[useCrossExchangePortfolio] Live adapter fetch error:', err);
        }
      }

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

      if (allBalances.length === 0) {
        setError('No balances found. Connect an exchange or start paper trading.');
      }

    } catch (err: any) {
      setError(err?.message || 'Failed to fetch cross-exchange balances');
      console.error('[useCrossExchangePortfolio] Error:', err);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, tradingMode, paperTradingApi, paperPortfolio, activeBroker]);

  useEffect(() => {
    fetchCrossExchangeBalances();
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
 * Compares prices across multiple providers using UnifiedMarketDataService
 */
export function useArbitrageDetection(symbols: string[] = ['BTC/USD', 'ETH/USD', 'SOL/USD']) {
  const { availableBrokers } = useBrokerContext();
  const [opportunities, setOpportunities] = useState<ArbitrageOpportunity[]>([]);
  const [isScanning, setIsScanning] = useState(false);
  const [lastScanTime, setLastScanTime] = useState<number | null>(null);

  const scanArbitrageOpportunities = useCallback(async () => {
    if (availableBrokers.length < 2) {
      setOpportunities([]);
      return;
    }

    setIsScanning(true);

    try {
      const foundOpportunities: ArbitrageOpportunity[] = [];
      const marketData = getMarketDataService();

      for (const symbol of symbols) {
        const prices: { exchange: string; price: number }[] = [];

        // Get prices from all available providers
        const allPrices = marketData.getAllPrices(symbol);

        for (const [provider, priceData] of allPrices) {
          if (priceData.last > 0) {
            prices.push({
              exchange: provider,
              price: priceData.last,
            });
          }
        }

        // Need at least 2 prices to find arbitrage
        if (prices.length < 2) continue;

        // Sort by price to find min and max
        prices.sort((a, b) => a.price - b.price);

        const lowest = prices[0];
        const highest = prices[prices.length - 1];

        const spread = highest.price - lowest.price;
        const spreadPercent = (spread / lowest.price) * 100;

        // Only report opportunities with spread > 0.2%
        if (spreadPercent > 0.2) {
          foundOpportunities.push({
            symbol,
            buyExchange: lowest.exchange,
            sellExchange: highest.exchange,
            buyPrice: lowest.price,
            sellPrice: highest.price,
            spread,
            spreadPercent,
            potentialProfit: spread, // Per unit profit
            timestamp: Date.now(),
          });
        }
      }

      // Sort by spread percent descending
      foundOpportunities.sort((a, b) => b.spreadPercent - a.spreadPercent);

      setOpportunities(foundOpportunities);
      setLastScanTime(Date.now());

    } catch (err) {
      console.error('[useArbitrageDetection] Scan error:', err);
    } finally {
      setIsScanning(false);
    }
  }, [symbols, availableBrokers]);

  useEffect(() => {
    scanArbitrageOpportunities();
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
 * Fetches real positions from paper trading and connected exchanges
 */
export function useCrossExchangePositions() {
  const { activeAdapter, tradingMode, paperTradingApi, paperPortfolio, activeBroker } = useBrokerContext();
  const [positions, setPositions] = useState<CrossExchangePosition[]>([]);
  const [totalUnrealizedPnl, setTotalUnrealizedPnl] = useState<number>(0);
  const [isLoading, setIsLoading] = useState(true);

  const fetchCrossExchangePositions = useCallback(async () => {
    setIsLoading(true);

    try {
      const allPositions: CrossExchangePosition[] = [];

      // Fetch from paper trading
      if (tradingMode === 'paper' && paperPortfolio && paperTradingApi) {
        try {
          const paperPositions = await paperTradingApi.getPositions(paperPortfolio.id);

          for (const pos of paperPositions) {
            const pnlPercent = pos.entry_price > 0
              ? ((pos.current_price - pos.entry_price) / pos.entry_price) * 100
              : 0;

            allPositions.push({
              symbol: pos.symbol,
              exchange: 'paper',
              side: pos.side,
              quantity: pos.quantity,
              entryPrice: pos.entry_price,
              currentPrice: pos.current_price,
              unrealizedPnl: pos.unrealized_pnl,
              pnlPercent: pos.side === 'short' ? -pnlPercent : pnlPercent,
            });
          }
        } catch (err) {
          console.warn('[useCrossExchangePositions] Paper trading fetch error:', err);
        }
      }

      // Fetch from real adapter
      if (activeAdapter && activeAdapter.isConnected() && tradingMode === 'live') {
        try {
          if (typeof (activeAdapter as any).fetchPositions === 'function') {
            const livePositions = await (activeAdapter as any).fetchPositions();

            for (const pos of livePositions || []) {
              allPositions.push({
                symbol: pos.symbol,
                exchange: activeBroker || 'unknown',
                side: pos.side,
                quantity: pos.contracts || pos.quantity || 0,
                entryPrice: pos.entryPrice || pos.entry_price || 0,
                currentPrice: pos.markPrice || pos.current_price || 0,
                unrealizedPnl: pos.unrealizedPnl || pos.unrealized_pnl || 0,
                pnlPercent: pos.percentage || 0,
              });
            }
          }
        } catch (err) {
          console.warn('[useCrossExchangePositions] Live adapter fetch error:', err);
        }
      }

      setPositions(allPositions);

      const totalPnl = allPositions.reduce((sum, pos) => sum + pos.unrealizedPnl, 0);
      setTotalUnrealizedPnl(totalPnl);

    } catch (err) {
      console.error('[useCrossExchangePositions] Error:', err);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, tradingMode, paperTradingApi, paperPortfolio, activeBroker]);

  useEffect(() => {
    fetchCrossExchangePositions();
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
