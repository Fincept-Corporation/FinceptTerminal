/**
 * Historical Data Service
 * Fetch and cache historical data for backtesting and analysis
 */

import { BrokerType, Candle, TimeInterval } from '../core/types';
import { authManager } from './AuthManager';

interface CacheKey {
  brokerId: BrokerType;
  symbol: string;
  exchange: string;
  interval: TimeInterval;
  from: string;
  to: string;
}

interface CachedData {
  candles: Candle[];
  fetchedAt: Date;
  expiresAt: Date;
}

export class HistoricalDataService {
  private static instance: HistoricalDataService;
  private cache: Map<string, CachedData> = new Map();
  private cacheDuration = 5 * 60 * 1000; // 5 minutes

  private constructor() {}

  static getInstance(): HistoricalDataService {
    if (!HistoricalDataService.instance) {
      HistoricalDataService.instance = new HistoricalDataService();
    }
    return HistoricalDataService.instance;
  }

  /**
   * Fetch historical data with caching
   */
  async fetchHistoricalData(
    brokerId: BrokerType,
    symbol: string,
    exchange: string,
    interval: TimeInterval,
    from: Date,
    to: Date,
    useCache: boolean = true
  ): Promise<Candle[]> {
    const cacheKey = this.generateCacheKey({
      brokerId,
      symbol,
      exchange,
      interval,
      from: from.toISOString(),
      to: to.toISOString(),
    });

    // Check cache
    if (useCache) {
      const cached = this.getFromCache(cacheKey);
      if (cached) {
        console.log(`[HistoricalData] Cache hit: ${cacheKey}`);
        return cached;
      }
    }

    // Fetch from broker
    console.log(`[HistoricalData] Fetching: ${cacheKey}`);
    const adapter = authManager.getAdapter(brokerId);
    if (!adapter) {
      throw new Error(`Adapter not found for ${brokerId}`);
    }

    try {
      const candles = await adapter.getHistoricalData(symbol, exchange, interval, from, to);

      // Store in cache
      this.storeInCache(cacheKey, candles);

      return candles;
    } catch (error: any) {
      console.error(`[HistoricalData] Fetch failed:`, error);
      throw error;
    }
  }

  /**
   * Fetch from multiple brokers and merge
   */
  async fetchMultiBroker(
    brokers: BrokerType[],
    symbol: string,
    exchange: string,
    interval: TimeInterval,
    from: Date,
    to: Date
  ): Promise<Map<BrokerType, Candle[]>> {
    const results = new Map<BrokerType, Candle[]>();

    const promises = brokers.map(async (brokerId) => {
      try {
        const candles = await this.fetchHistoricalData(
          brokerId,
          symbol,
          exchange,
          interval,
          from,
          to
        );
        return { brokerId, candles };
      } catch (error) {
        console.error(`[HistoricalData] ${brokerId} fetch failed:`, error);
        return { brokerId, candles: [] };
      }
    });

    const responses = await Promise.all(promises);
    responses.forEach(({ brokerId, candles }) => {
      results.set(brokerId, candles);
    });

    return results;
  }

  /**
   * Backfill missing data
   * Useful for continuous data streams
   */
  async backfill(
    brokerId: BrokerType,
    symbol: string,
    exchange: string,
    interval: TimeInterval,
    targetCandles: number = 500
  ): Promise<Candle[]> {
    const to = new Date();
    const from = this.calculateFromDate(interval, targetCandles);

    return await this.fetchHistoricalData(brokerId, symbol, exchange, interval, from, to);
  }

  /**
   * Get continuous data stream (combine historical + live)
   */
  async getContinuousData(
    brokerId: BrokerType,
    symbol: string,
    exchange: string,
    interval: TimeInterval,
    lookbackPeriod: number = 100 // Number of candles
  ): Promise<Candle[]> {
    const to = new Date();
    const from = this.calculateFromDate(interval, lookbackPeriod);

    return await this.fetchHistoricalData(brokerId, symbol, exchange, interval, from, to);
  }

  /**
   * Calculate from date based on interval and count
   */
  private calculateFromDate(interval: TimeInterval, count: number): Date {
    const now = new Date();
    const intervalMs = this.getIntervalMs(interval);
    return new Date(now.getTime() - intervalMs * count);
  }

  /**
   * Get interval in milliseconds
   */
  private getIntervalMs(interval: TimeInterval): number {
    const map: Record<TimeInterval, number> = {
      '1m': 60 * 1000,
      '3m': 3 * 60 * 1000,
      '5m': 5 * 60 * 1000,
      '15m': 15 * 60 * 1000,
      '30m': 30 * 60 * 1000,
      '1h': 60 * 60 * 1000,
      '1d': 24 * 60 * 60 * 1000,
    };
    return map[interval] || 60 * 1000;
  }

  /**
   * Generate cache key
   */
  private generateCacheKey(params: CacheKey): string {
    return `${params.brokerId}_${params.exchange}_${params.symbol}_${params.interval}_${params.from}_${params.to}`;
  }

  /**
   * Get from cache
   */
  private getFromCache(key: string): Candle[] | null {
    const cached = this.cache.get(key);
    if (!cached) return null;

    // Check if expired
    if (new Date() > cached.expiresAt) {
      this.cache.delete(key);
      return null;
    }

    return cached.candles;
  }

  /**
   * Store in cache
   */
  private storeInCache(key: string, candles: Candle[]): void {
    const now = new Date();
    const cached: CachedData = {
      candles,
      fetchedAt: now,
      expiresAt: new Date(now.getTime() + this.cacheDuration),
    };
    this.cache.set(key, cached);
  }

  /**
   * Clear cache
   */
  clearCache(): void {
    this.cache.clear();
    console.log('[HistoricalData] Cache cleared');
  }

  /**
   * Set cache duration
   */
  setCacheDuration(durationMs: number): void {
    this.cacheDuration = durationMs;
  }

  /**
   * Get cache statistics
   */
  getCacheStats(): {
    size: number;
    hitRate: number;
  } {
    return {
      size: this.cache.size,
      hitRate: 0, // TODO: Track hits/misses
    };
  }

  /**
   * Export data to CSV
   */
  exportToCSV(candles: Candle[], symbol: string): string {
    const header = 'Timestamp,Open,High,Low,Close,Volume\n';
    const rows = candles.map(c =>
      `${c.timestamp.toISOString()},${c.open},${c.high},${c.low},${c.close},${c.volume}`
    ).join('\n');
    return header + rows;
  }

  /**
   * Resample data to different interval
   */
  resample(candles: Candle[], targetInterval: TimeInterval): Candle[] {
    // TODO: Implement resampling logic
    // For now, return as-is
    return candles;
  }
}

export const historicalDataService = HistoricalDataService.getInstance();
