// useWatchlist Hooks - Cached data fetching for Watchlist Tab
// Uses unified cache system for stock quotes, market movers, and volume leaders

import { useMemo } from 'react';
import { useCache, cacheKey } from './useCache';
import { watchlistService, WatchlistStockWithQuote } from '../services/core/watchlistService';
import { marketDataService, QuoteData } from '../services/markets/marketDataService';

// ============================================================================
// Types
// ============================================================================

export interface MarketMovers {
  gainers: WatchlistStockWithQuote[];
  losers: WatchlistStockWithQuote[];
}

// ============================================================================
// useWatchlistStocks - Fetch stocks with quotes for a watchlist
// ============================================================================

export function useWatchlistStocks(
  watchlistId: string | null,
  options: {
    enabled?: boolean;
    refetchInterval?: number;
  } = {}
) {
  const { enabled = true, refetchInterval = 10 * 60 * 1000 } = options; // 10 min default

  return useCache<WatchlistStockWithQuote[]>({
    key: cacheKey('watchlist', 'stocks', watchlistId || 'none'),
    category: 'watchlist',
    fetcher: async () => {
      if (!watchlistId) return [];
      return watchlistService.getWatchlistStocksWithQuotes(watchlistId);
    },
    ttl: '10m',
    enabled: enabled && !!watchlistId,
    refetchInterval,
    staleWhileRevalidate: true,
    resetOnKeyChange: true,
  });
}

// ============================================================================
// useMarketMovers - Fetch top gainers and losers across all watchlists
// ============================================================================

export function useMarketMovers(
  limit: number = 5,
  options: {
    enabled?: boolean;
    refetchInterval?: number;
  } = {}
) {
  const { enabled = true, refetchInterval = 10 * 60 * 1000 } = options;

  return useCache<MarketMovers>({
    key: cacheKey('watchlist', 'market-movers', String(limit)),
    category: 'watchlist',
    fetcher: async () => {
      const result = await watchlistService.getMarketMovers(limit);
      return result;
    },
    ttl: '10m',
    enabled,
    refetchInterval,
    staleWhileRevalidate: true,
  });
}

// ============================================================================
// useVolumeLeaders - Fetch top volume stocks across all watchlists
// ============================================================================

export function useVolumeLeaders(
  limit: number = 5,
  options: {
    enabled?: boolean;
    refetchInterval?: number;
  } = {}
) {
  const { enabled = true, refetchInterval = 10 * 60 * 1000 } = options;

  return useCache<WatchlistStockWithQuote[]>({
    key: cacheKey('watchlist', 'volume-leaders', String(limit)),
    category: 'watchlist',
    fetcher: async () => {
      return watchlistService.getVolumeLeaders(limit);
    },
    ttl: '10m',
    enabled,
    refetchInterval,
    staleWhileRevalidate: true,
  });
}

// ============================================================================
// useStockQuote - Fetch a single stock quote (for detail panel)
// ============================================================================

export function useStockQuote(
  symbol: string | null,
  options: {
    enabled?: boolean;
    refetchInterval?: number;
  } = {}
) {
  const { enabled = true, refetchInterval = 60 * 1000 } = options; // 1 min for single stock

  return useCache<QuoteData | null>({
    key: cacheKey('watchlist', 'quote', symbol || 'none'),
    category: 'watchlist',
    fetcher: async () => {
      if (!symbol) return null;
      return marketDataService.getEnhancedQuote(symbol);
    },
    ttl: '1m',
    enabled: enabled && !!symbol,
    refetchInterval,
    staleWhileRevalidate: true,
    resetOnKeyChange: true,
  });
}

// ============================================================================
// Helper: Create a stable cache key from watchlist stocks
// ============================================================================

export function createStocksHash(stocks: { symbol: string }[]): string {
  if (stocks.length === 0) return 'empty';
  const symbols = stocks.map(s => s.symbol).sort();
  return `${symbols.length}:${symbols[0]}:${symbols[symbols.length - 1]}`;
}

export default {
  useWatchlistStocks,
  useMarketMovers,
  useVolumeLeaders,
  useStockQuote,
};
