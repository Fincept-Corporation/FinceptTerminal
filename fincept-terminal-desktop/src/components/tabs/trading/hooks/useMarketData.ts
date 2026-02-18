/**
 * Market Data Hooks
 * Fetches OHLCV, orderbook, and ticker data across all exchanges
 * Uses UnifiedMarketDataService for live price streaming
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import { getMarketDataService } from '../../../../services/trading/UnifiedMarketDataService';
import type { OHLCV, Timeframe, UnifiedOrderBook, UnifiedTicker } from '../types';

// ============================================================================
// OHLCV / CANDLESTICK DATA
// ============================================================================

// Cache for OHLCV data
const ohlcvCache = new Map<string, { data: OHLCV[]; timestamp: number }>();
const OHLCV_CACHE_TTL = 30000; // 30 seconds

export function useOHLCV(symbol?: string, timeframe: Timeframe = '1h', limit: number = 100) {
  // Use realAdapter for OHLCV data - works in both paper and live modes
  const { realAdapter } = useBrokerContext();
  const [ohlcv, setOhlcv] = useState<OHLCV[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const fetchIdRef = useRef(0);

  // Clear stale data immediately when symbol or timeframe changes
  useEffect(() => {
    setOhlcv([]);
    setError(null);
  }, [symbol, timeframe]);

  const fetchOHLCV = useCallback(async () => {
    const currentFetchId = ++fetchIdRef.current;
    // Use realAdapter directly for OHLCV (paper adapter returns empty)
    if (!realAdapter || !symbol) {
      setOhlcv([]);
      return;
    }

    // Check cache
    const cacheKey = `${symbol}_${timeframe}_${limit}`;
    const cached = ohlcvCache.get(cacheKey);
    if (cached && Date.now() - cached.timestamp < OHLCV_CACHE_TTL) {
      setOhlcv(cached.data);
      setIsLoading(false);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Check if realAdapter is connected
      if (!realAdapter.isConnected()) {
        setIsLoading(false);
        return;
      }

      // Fetch OHLCV from real adapter (exchange API)
      const rawData = await realAdapter.fetchOHLCV(symbol, timeframe, limit);

      // Normalize to unified format
      const normalized: OHLCV[] = (rawData || []).map((candle: any) => ({
        timestamp: candle[0],
        open: candle[1],
        high: candle[2],
        low: candle[3],
        close: candle[4],
        volume: candle[5],
      }));

      // Guard against stale fetch (symbol/timeframe changed while awaiting)
      if (currentFetchId !== fetchIdRef.current) return;

      setOhlcv(normalized);

      // Update cache
      ohlcvCache.set(cacheKey, { data: normalized, timestamp: Date.now() });
    } catch (err) {
      if (currentFetchId !== fetchIdRef.current) return;
      const error = err as Error;
      console.warn('[useOHLCV] Failed to fetch OHLCV:', error.message);
      setError(error);
    } finally {
      if (currentFetchId === fetchIdRef.current) {
        setIsLoading(false);
      }
    }
  }, [realAdapter, symbol, timeframe, limit]);

  useEffect(() => {
    fetchOHLCV();
  }, [fetchOHLCV]);

  return {
    ohlcv,
    isLoading,
    error,
    refresh: fetchOHLCV,
  };
}

// ============================================================================
// ORDERBOOK DATA
// ============================================================================

export function useOrderBook(symbol?: string, limit: number = 20, autoRefresh: boolean = true) {
  // Use realAdapter for orderbook data - works in both paper and live modes
  const { realAdapter } = useBrokerContext();
  const [orderBook, setOrderBook] = useState<UnifiedOrderBook | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchOrderBook = useCallback(async () => {
    // Use realAdapter directly for orderbook (paper adapter returns empty)
    if (!realAdapter || !symbol) {
      setOrderBook(null);
      return;
    }

    // Check if connected
    if (!realAdapter.isConnected()) {
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawBook = await realAdapter.fetchOrderBook(symbol, limit);

      // Handle both array format [price, size] and object format {price, size/quantity}
      const normalizeLevels = (levels: any[]) => {
        return (levels || [])
          .filter((level: any) => {
            if (Array.isArray(level)) {
              return level.length >= 2;
            }
            return level && (level.price !== undefined);
          })
          .map((level: any) => {
            if (Array.isArray(level)) {
              return { price: level[0] as number, size: level[1] as number };
            }
            return { price: level.price as number, size: (level.size || level.quantity || level.amount) as number };
          });
      };

      // Normalize to unified format with array element validation
      const normalized: UnifiedOrderBook = {
        symbol,
        bids: normalizeLevels(rawBook.bids),
        asks: normalizeLevels(rawBook.asks),
        timestamp: rawBook.timestamp || Date.now(),
      };

      setOrderBook(normalized);
    } catch (err) {
      const error = err as Error;
      console.warn('[useOrderBook] Failed to fetch orderbook:', error.message);
      setError(error);
      // Don't clear orderbook on error - keep existing data
    } finally {
      setIsLoading(false);
    }
  }, [realAdapter, symbol, limit]);

  useEffect(() => {
    fetchOrderBook();

    if (!autoRefresh) return;

    // Fetch orderbook every 2 seconds for more responsive updates
    const interval = setInterval(fetchOrderBook, 2000);
    return () => clearInterval(interval);
  }, [fetchOrderBook, autoRefresh]);

  return {
    orderBook,
    isLoading,
    error,
    refresh: fetchOrderBook,
  };
}

// ============================================================================
// TICKER DATA
// ============================================================================

export function useTicker(symbol?: string, autoRefresh: boolean = true) {
  // Use realAdapter for additional ticker data (high, low, volume)
  const { realAdapter, activeBroker } = useBrokerContext();
  const [ticker, setTicker] = useState<UnifiedTicker | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const unsubscribeRef = useRef<(() => void) | null>(null);

  // Subscribe to market data service for live updates (primary source)
  useEffect(() => {
    if (!symbol) {
      setTicker(null);
      return;
    }

    const marketDataService = getMarketDataService();

    // Get initial price from cache
    const cachedPrice = marketDataService.getCurrentPrice(symbol, activeBroker || undefined);
    if (cachedPrice) {
      setTicker({
        symbol,
        last: cachedPrice.last,
        bid: cachedPrice.bid,
        ask: cachedPrice.ask,
        high: 0,
        low: 0,
        open: undefined,
        close: undefined,
        volume: 0,
        quoteVolume: undefined,
        change: 0,
        changePercent: 0,
        timestamp: cachedPrice.timestamp,
      });
      setIsLoading(false);
    }

    // Subscribe to live updates from market data service
    const unsubscribe = marketDataService.subscribeToPrice(symbol, (price) => {
      setTicker((prev) => ({
        symbol,
        last: price.last,
        bid: price.bid,
        ask: price.ask,
        high: prev?.high || price.high || 0,
        low: prev?.low || price.low || 0,
        open: prev?.open,
        close: prev?.close,
        volume: prev?.volume || price.volume || 0,
        quoteVolume: prev?.quoteVolume,
        change: prev?.change || price.change || 0,
        changePercent: prev?.changePercent || price.changePercent || 0,
        timestamp: price.timestamp,
      }));
      setIsLoading(false);
      setError(null);
    });

    unsubscribeRef.current = unsubscribe;

    return () => {
      if (unsubscribeRef.current) {
        unsubscribeRef.current();
        unsubscribeRef.current = null;
      }
    };
  }, [symbol, activeBroker]);

  // Fetch full ticker data from realAdapter (works in both paper and live modes)
  const fetchTicker = useCallback(async () => {
    if (!realAdapter || !symbol) {
      return;
    }

    // Check if connected
    if (!realAdapter.isConnected()) {
      return;
    }

    setIsLoading(true);

    try {
      const rawTicker = await realAdapter.fetchTicker(symbol);

      // Get live price from market data service
      const marketDataService = getMarketDataService();
      const livePrice = marketDataService.getCurrentPrice(symbol, activeBroker || undefined);

      // Merge: use live price if available, fallback to adapter
      const lastPrice = livePrice?.last || rawTicker.last || 0;
      const bidPrice = livePrice?.bid || rawTicker.bid || 0;
      const askPrice = livePrice?.ask || rawTicker.ask || 0;

      // Normalize to unified format
      const normalized: UnifiedTicker = {
        symbol,
        last: lastPrice,
        bid: bidPrice,
        ask: askPrice,
        high: rawTicker.high || 0,
        low: rawTicker.low || 0,
        open: rawTicker.open,
        close: rawTicker.close,
        volume: rawTicker.baseVolume || 0,
        quoteVolume: rawTicker.quoteVolume,
        change: rawTicker.change || 0,
        changePercent: rawTicker.percentage || 0,
        timestamp: livePrice?.timestamp || rawTicker.timestamp || Date.now(),
      };

      setTicker(normalized);
      setError(null);
    } catch (err) {
      console.warn('[useTicker] Failed to fetch ticker from adapter:', (err as Error).message);
      // Don't clear ticker on error - keep live price data from market data service
    } finally {
      setIsLoading(false);
    }
  }, [realAdapter, symbol, activeBroker]);

  // Fetch full ticker data on mount and periodically
  useEffect(() => {
    fetchTicker();

    if (!autoRefresh) return;

    // Fetch full ticker data every 5 seconds (for high/low/volume)
    // Live price updates come from market data service subscription
    const interval = setInterval(fetchTicker, 5000);
    return () => clearInterval(interval);
  }, [fetchTicker, autoRefresh]);

  return {
    ticker,
    isLoading,
    error,
    refresh: fetchTicker,
  };
}

// ============================================================================
// VOLUME PROFILE DATA
// ============================================================================

export interface VolumeProfileLevel {
  price: number;
  volume: number;
  buyVolume: number;
  sellVolume: number;
}

export function useVolumeProfile(symbol?: string, timeframe: Timeframe = '1h', limit: number = 100) {
  const { ohlcv } = useOHLCV(symbol, timeframe, limit);
  const [volumeProfile, setVolumeProfile] = useState<VolumeProfileLevel[]>([]);

  useEffect(() => {
    if (!ohlcv.length) {
      setVolumeProfile([]);
      return;
    }

    // Calculate volume profile from OHLCV data
    const priceMap = new Map<number, { volume: number; buyVol: number; sellVol: number }>();

    ohlcv.forEach((candle) => {
      // Use close price as reference
      const priceLevel = Math.round(candle.close / 10) * 10; // Round to nearest $10

      const existing = priceMap.get(priceLevel) || { volume: 0, buyVol: 0, sellVol: 0 };

      // Estimate buy/sell volume based on candle close vs open
      const isBullish = candle.close >= candle.open;
      const buyVol = isBullish ? candle.volume : candle.volume * 0.3;
      const sellVol = isBullish ? candle.volume * 0.3 : candle.volume;

      priceMap.set(priceLevel, {
        volume: existing.volume + candle.volume,
        buyVol: existing.buyVol + buyVol,
        sellVol: existing.sellVol + sellVol,
      });
    });

    // Convert to array and sort by price
    const profile: VolumeProfileLevel[] = Array.from(priceMap.entries())
      .map(([price, data]) => ({
        price,
        volume: data.volume,
        buyVolume: data.buyVol,
        sellVolume: data.sellVol,
      }))
      .sort((a, b) => a.price - b.price);

    setVolumeProfile(profile);
  }, [ohlcv]);

  return {
    volumeProfile,
    isLoading: false,
    error: null,
  };
}
