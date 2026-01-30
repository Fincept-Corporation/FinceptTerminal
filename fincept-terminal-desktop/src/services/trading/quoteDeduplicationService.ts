/**
 * Quote Deduplication Service
 *
 * Prevents multiple concurrent requests for the same symbol.
 * Implements request coalescing and TTL-based caching.
 */

import type { Quote, StockExchange } from '@/brokers/stocks/types';

interface CacheEntry {
  quote: Quote;
  timestamp: number;
}

interface PendingRequest {
  promise: Promise<Quote>;
  timestamp: number;
}

// Cache TTL based on market status
const CACHE_TTL_MARKET_OPEN = 5000;    // 5 seconds
const CACHE_TTL_MARKET_CLOSED = 60000; // 1 minute
const PENDING_REQUEST_TIMEOUT = 30000; // 30 seconds

class QuoteDeduplicationService {
  private cache: Map<string, CacheEntry> = new Map();
  private pendingRequests: Map<string, PendingRequest> = new Map();

  private getKey(symbol: string, exchange: StockExchange): string {
    return `${exchange}:${symbol}`;
  }

  private getCacheTTL(exchange: StockExchange): number {
    // Simple market hours check (can be enhanced with marketHoursService)
    const now = new Date();
    const hour = now.getHours();
    const isWeekend = now.getDay() === 0 || now.getDay() === 6;

    // Indian markets: 9:15 AM - 3:30 PM IST
    const isIndianExchange = ['NSE', 'BSE', 'NFO', 'BFO', 'MCX', 'CDS'].includes(exchange);
    if (isIndianExchange) {
      const isOpen = !isWeekend && hour >= 9 && hour < 16;
      return isOpen ? CACHE_TTL_MARKET_OPEN : CACHE_TTL_MARKET_CLOSED;
    }

    // US markets: 9:30 AM - 4:00 PM EST (simplified)
    return CACHE_TTL_MARKET_OPEN;
  }

  /**
   * Get quote with deduplication and caching
   */
  async getQuote(
    symbol: string,
    exchange: StockExchange,
    fetchFn: () => Promise<Quote>
  ): Promise<Quote> {
    const key = this.getKey(symbol, exchange);
    const now = Date.now();
    const ttl = this.getCacheTTL(exchange);

    // Check cache first
    const cached = this.cache.get(key);
    if (cached && (now - cached.timestamp) < ttl) {
      return cached.quote;
    }

    // Check if request already pending
    const pending = this.pendingRequests.get(key);
    if (pending && (now - pending.timestamp) < PENDING_REQUEST_TIMEOUT) {
      return pending.promise;
    }

    // Create new request
    const promise = fetchFn()
      .then((quote) => {
        this.cache.set(key, { quote, timestamp: Date.now() });
        return quote;
      })
      .finally(() => {
        this.pendingRequests.delete(key);
      });

    this.pendingRequests.set(key, { promise, timestamp: now });
    return promise;
  }

  /**
   * Get multiple quotes with batching
   */
  async getMultiQuotes(
    symbols: Array<{ symbol: string; exchange: StockExchange }>,
    batchFetchFn: (symbols: Array<{ symbol: string; exchange: StockExchange }>) => Promise<Map<string, Quote>>
  ): Promise<Map<string, Quote>> {
    const now = Date.now();
    const result = new Map<string, Quote>();
    const toFetch: Array<{ symbol: string; exchange: StockExchange }> = [];

    // Check cache for each symbol
    for (const { symbol, exchange } of symbols) {
      const key = this.getKey(symbol, exchange);
      const ttl = this.getCacheTTL(exchange);
      const cached = this.cache.get(key);

      if (cached && (now - cached.timestamp) < ttl) {
        result.set(key, cached.quote);
      } else {
        toFetch.push({ symbol, exchange });
      }
    }

    // Fetch missing quotes in batch
    if (toFetch.length > 0) {
      try {
        const fetched = await batchFetchFn(toFetch);
        for (const [key, quote] of fetched) {
          this.cache.set(key, { quote, timestamp: Date.now() });
          result.set(key, quote);
        }
      } catch (error) {
        console.error('[QuoteDedup] Batch fetch failed:', error);
      }
    }

    return result;
  }

  /**
   * Invalidate cache for symbol
   */
  invalidate(symbol: string, exchange: StockExchange): void {
    const key = this.getKey(symbol, exchange);
    this.cache.delete(key);
  }

  /**
   * Clear all cached data
   */
  clear(): void {
    this.cache.clear();
    this.pendingRequests.clear();
  }

  /**
   * Update cache from WebSocket tick
   */
  updateFromTick(symbol: string, exchange: StockExchange, quote: Quote): void {
    const key = this.getKey(symbol, exchange);
    this.cache.set(key, { quote, timestamp: Date.now() });
  }

  /**
   * Get cache stats for debugging
   */
  getStats(): { cacheSize: number; pendingRequests: number } {
    return {
      cacheSize: this.cache.size,
      pendingRequests: this.pendingRequests.size,
    };
  }
}

// Singleton instance
export const quoteDeduplicationService = new QuoteDeduplicationService();
export default quoteDeduplicationService;
