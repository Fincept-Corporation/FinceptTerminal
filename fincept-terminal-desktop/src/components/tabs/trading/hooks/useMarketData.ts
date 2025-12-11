/**
 * Market Data Hooks
 * Fetches OHLCV, orderbook, and ticker data across all exchanges
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import type { OHLCV, Timeframe, UnifiedOrderBook, UnifiedTicker } from '../types';

// ============================================================================
// OHLCV / CANDLESTICK DATA
// ============================================================================

export function useOHLCV(symbol?: string, timeframe: Timeframe = '1h', limit: number = 100) {
  const { activeAdapter } = useBrokerContext();
  const [ohlcv, setOhlcv] = useState<OHLCV[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchOHLCV = useCallback(async () => {
    if (!activeAdapter || !symbol) {
      setOhlcv([]);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      // Fetch OHLCV from adapter
      const rawData = await activeAdapter.fetchOHLCV(symbol, timeframe, limit);

      // Normalize to unified format
      const normalized: OHLCV[] = (rawData || []).map((candle: any) => ({
        timestamp: candle[0],
        open: candle[1],
        high: candle[2],
        low: candle[3],
        close: candle[4],
        volume: candle[5],
      }));

      setOhlcv(normalized);
    } catch (err) {
      const error = err as Error;
      console.error('[useOHLCV] Failed to fetch OHLCV:', error);
      setError(error);
      setOhlcv([]);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, timeframe, limit]);

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
  const { activeAdapter } = useBrokerContext();
  const [orderBook, setOrderBook] = useState<UnifiedOrderBook | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchOrderBook = useCallback(async () => {
    if (!activeAdapter || !symbol) {
      setOrderBook(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawBook = await activeAdapter.fetchOrderBook(symbol, limit);

      // Normalize to unified format
      const normalized: UnifiedOrderBook = {
        symbol,
        bids: (rawBook.bids || []).map((level: any) => ({ price: level[0] as number, size: level[1] as number })),
        asks: (rawBook.asks || []).map((level: any) => ({ price: level[0] as number, size: level[1] as number })),
        timestamp: rawBook.timestamp || Date.now(),
      };

      setOrderBook(normalized);
    } catch (err) {
      const error = err as Error;
      console.error('[useOrderBook] Failed to fetch orderbook:', error);
      setError(error);
      setOrderBook(null);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol, limit]);

  useEffect(() => {
    fetchOrderBook();

    if (!autoRefresh) return;

    const interval = setInterval(fetchOrderBook, 1000); // Update every 1s
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
  const { activeAdapter } = useBrokerContext();
  const [ticker, setTicker] = useState<UnifiedTicker | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  const fetchTicker = useCallback(async () => {
    if (!activeAdapter || !symbol) {
      setTicker(null);
      return;
    }

    setIsLoading(true);
    setError(null);

    try {
      const rawTicker = await activeAdapter.fetchTicker(symbol);

      // Normalize to unified format
      const normalized: UnifiedTicker = {
        symbol,
        last: rawTicker.last || 0,
        bid: rawTicker.bid || 0,
        ask: rawTicker.ask || 0,
        high: rawTicker.high || 0,
        low: rawTicker.low || 0,
        open: rawTicker.open,
        close: rawTicker.close,
        volume: rawTicker.baseVolume || 0,
        quoteVolume: rawTicker.quoteVolume,
        change: rawTicker.change || 0,
        changePercent: rawTicker.percentage || 0,
        timestamp: rawTicker.timestamp || Date.now(),
      };

      setTicker(normalized);
    } catch (err) {
      const error = err as Error;
      console.error('[useTicker] Failed to fetch ticker:', error);
      setError(error);
      setTicker(null);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, symbol]);

  useEffect(() => {
    fetchTicker();

    if (!autoRefresh) return;

    const interval = setInterval(fetchTicker, 2000); // Update every 2s
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
