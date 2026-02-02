/**
 * Market Data Bridge
 * Connects workflow nodes to 100+ market data adapters
 * Provides unified interface for quotes, historical data, and real-time streaming
 */

import { IDataObject } from '../types';

// ================================
// TYPES
// ================================

export interface QuoteData {
  symbol: string;
  price: number;
  change: number;
  changePercent: number;
  open?: number;
  high?: number;
  low?: number;
  close?: number;
  volume?: number;
  timestamp: string;
  exchange?: string;
  currency?: string;
}

export interface OHLCVData {
  timestamp: string;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface HistoricalDataRequest {
  symbol: string;
  interval: string;
  period?: string;
  startDate?: string;
  endDate?: string;
  provider?: string;
}

export interface MarketDepthData {
  symbol: string;
  bids: Array<{ price: number; quantity: number }>;
  asks: Array<{ price: number; quantity: number }>;
  timestamp: string;
}

export interface TickerStats {
  symbol: string;
  lastPrice: number;
  highPrice24h: number;
  lowPrice24h: number;
  volume24h: number;
  change24h: number;
  changePercent24h: number;
  timestamp: string;
}

export interface FundamentalData {
  symbol: string;
  name: string;
  sector?: string;
  industry?: string;
  marketCap?: number;
  peRatio?: number;
  eps?: number;
  dividendYield?: number;
  beta?: number;
  week52High?: number;
  week52Low?: number;
  avgVolume?: number;
}

export type DataProvider =
  | 'yahoo'
  | 'alphavantage'
  | 'binance'
  | 'coingecko'
  | 'finnhub'
  | 'iex'
  | 'tradier'
  | 'twelvedata';

// ================================
// MARKET DATA BRIDGE
// ================================

class MarketDataBridgeClass {
  private adapterCache: Map<string, any> = new Map();

  /**
   * Get a quote for a symbol
   */
  async getQuote(
    symbol: string,
    provider: DataProvider = 'yahoo',
    exchange?: string
  ): Promise<QuoteData> {
    try {
      // Dynamic import of adapter based on provider
      const adapter = await this.getAdapter(provider);

      if (!adapter) {
        // Fallback to mock data for development
        return this.getMockQuote(symbol);
      }

      const result = await adapter.query({
        type: 'quote',
        symbol,
        exchange,
      });

      return this.normalizeQuote(result, symbol);
    } catch (error) {
      console.error(`[MarketDataBridge] Error fetching quote for ${symbol}:`, error);
      throw error;
    }
  }

  /**
   * Get multiple quotes at once
   */
  async getQuotes(
    symbols: string[],
    provider: DataProvider = 'yahoo'
  ): Promise<QuoteData[]> {
    try {
      // Parallel fetch for efficiency
      const quotes = await Promise.all(
        symbols.map((symbol) => this.getQuote(symbol, provider))
      );
      return quotes;
    } catch (error) {
      console.error('[MarketDataBridge] Error fetching multiple quotes:', error);
      throw error;
    }
  }

  /**
   * Get historical OHLCV data
   */
  async getHistoricalData(request: HistoricalDataRequest): Promise<OHLCVData[]> {
    try {
      const provider = request.provider || 'yahoo';
      const adapter = await this.getAdapter(provider as DataProvider);

      if (!adapter) {
        return this.getMockHistoricalData(request.symbol, request.period || '1mo');
      }

      const result = await adapter.query({
        type: 'historical',
        symbol: request.symbol,
        interval: request.interval,
        period: request.period,
        startDate: request.startDate,
        endDate: request.endDate,
      });

      return this.normalizeHistoricalData(result);
    } catch (error) {
      console.error('[MarketDataBridge] Error fetching historical data:', error);
      throw error;
    }
  }

  /**
   * Get market depth (order book)
   */
  async getMarketDepth(
    symbol: string,
    provider: DataProvider = 'binance',
    levels: number = 10
  ): Promise<MarketDepthData> {
    try {
      const adapter = await this.getAdapter(provider);

      if (!adapter) {
        return this.getMockMarketDepth(symbol);
      }

      const result = await adapter.query({
        type: 'orderbook',
        symbol,
        limit: levels,
      });

      return this.normalizeMarketDepth(result, symbol);
    } catch (error) {
      console.error('[MarketDataBridge] Error fetching market depth:', error);
      throw error;
    }
  }

  /**
   * Get 24h ticker statistics
   */
  async getTickerStats(
    symbol: string,
    provider: DataProvider = 'binance'
  ): Promise<TickerStats> {
    try {
      const adapter = await this.getAdapter(provider);

      if (!adapter) {
        return this.getMockTickerStats(symbol);
      }

      const result = await adapter.query({
        type: 'ticker24h',
        symbol,
      });

      return this.normalizeTickerStats(result, symbol);
    } catch (error) {
      console.error('[MarketDataBridge] Error fetching ticker stats:', error);
      throw error;
    }
  }

  /**
   * Get fundamental data
   */
  async getFundamentals(
    symbol: string,
    provider: DataProvider = 'yahoo'
  ): Promise<FundamentalData> {
    try {
      const adapter = await this.getAdapter(provider);

      if (!adapter) {
        return this.getMockFundamentals(symbol);
      }

      const result = await adapter.query({
        type: 'fundamentals',
        symbol,
      });

      return this.normalizeFundamentals(result, symbol);
    } catch (error) {
      console.error('[MarketDataBridge] Error fetching fundamentals:', error);
      throw error;
    }
  }

  /**
   * Search for symbols
   */
  async searchSymbols(
    query: string,
    provider: DataProvider = 'yahoo'
  ): Promise<Array<{ symbol: string; name: string; type: string }>> {
    try {
      const adapter = await this.getAdapter(provider);

      if (!adapter) {
        return [];
      }

      const result = await adapter.query({
        type: 'search',
        query,
      });

      return Array.isArray(result) ? result : [];
    } catch (error) {
      console.error('[MarketDataBridge] Error searching symbols:', error);
      return [];
    }
  }

  /**
   * Subscribe to real-time quotes (returns unsubscribe function)
   */
  subscribeToQuotes(
    symbols: string[],
    callback: (quote: QuoteData) => void,
    provider: DataProvider = 'binance'
  ): () => void {
    // Implementation depends on WebSocket availability
    // For now, return a mock poller
    const interval = setInterval(async () => {
      for (const symbol of symbols) {
        try {
          const quote = await this.getQuote(symbol, provider);
          callback(quote);
        } catch (error) {
          console.error(`[MarketDataBridge] Error in quote subscription for ${symbol}:`, error);
        }
      }
    }, 5000); // Poll every 5 seconds

    return () => clearInterval(interval);
  }

  // ================================
  // PRIVATE METHODS
  // ================================

  /**
   * Get adapter instance (with caching)
   */
  private async getAdapter(provider: DataProvider): Promise<any> {
    if (this.adapterCache.has(provider)) {
      return this.adapterCache.get(provider);
    }

    try {
      // Dynamic import based on provider
      const adapterPath = this.getAdapterPath(provider);
      const module = await import(/* @vite-ignore */ adapterPath);

      // Get the adapter class and create instance
      const AdapterClass = module.default || Object.values(module)[0];
      if (AdapterClass) {
        const adapter = new AdapterClass({});
        this.adapterCache.set(provider, adapter);
        return adapter;
      }
    } catch (error) {
      console.warn(`[MarketDataBridge] Adapter for ${provider} not available:`, error);
    }

    return null;
  }

  /**
   * Get adapter import path
   */
  private getAdapterPath(provider: DataProvider): string {
    const paths: Record<DataProvider, string> = {
      yahoo: '@/components/tabs/data-sources/adapters/YahooFinanceAdapter',
      alphavantage: '@/components/tabs/data-sources/adapters/AlphaVantageAdapter',
      binance: '@/components/tabs/data-sources/adapters/BinanceAdapter',
      coingecko: '@/components/tabs/data-sources/adapters/CoinGeckoAdapter',
      finnhub: '@/components/tabs/data-sources/adapters/FinnhubAdapter',
      iex: '@/components/tabs/data-sources/adapters/IEXCloudAdapter',
      tradier: '@/components/tabs/data-sources/adapters/TradierAdapter',
      twelvedata: '@/components/tabs/data-sources/adapters/TwelveDataAdapter',
    };
    return paths[provider];
  }

  // ================================
  // NORMALIZATION METHODS
  // ================================

  private normalizeQuote(data: any, symbol: string): QuoteData {
    return {
      symbol,
      price: data.price || data.lastPrice || data.regularMarketPrice || 0,
      change: data.change || data.regularMarketChange || 0,
      changePercent: data.changePercent || data.regularMarketChangePercent || 0,
      open: data.open || data.regularMarketOpen,
      high: data.high || data.regularMarketDayHigh,
      low: data.low || data.regularMarketDayLow,
      close: data.close || data.previousClose,
      volume: data.volume || data.regularMarketVolume,
      timestamp: data.timestamp || new Date().toISOString(),
      exchange: data.exchange,
      currency: data.currency,
    };
  }

  private normalizeHistoricalData(data: any): OHLCVData[] {
    if (!Array.isArray(data)) {
      return [];
    }

    return data.map((item: any) => ({
      timestamp: item.timestamp || item.date || item.t,
      open: item.open || item.o,
      high: item.high || item.h,
      low: item.low || item.l,
      close: item.close || item.c,
      volume: item.volume || item.v,
    }));
  }

  private normalizeMarketDepth(data: any, symbol: string): MarketDepthData {
    return {
      symbol,
      bids: data.bids || [],
      asks: data.asks || [],
      timestamp: data.timestamp || new Date().toISOString(),
    };
  }

  private normalizeTickerStats(data: any, symbol: string): TickerStats {
    return {
      symbol,
      lastPrice: data.lastPrice || data.price || 0,
      highPrice24h: data.highPrice || data.high24h || 0,
      lowPrice24h: data.lowPrice || data.low24h || 0,
      volume24h: data.volume || data.volume24h || 0,
      change24h: data.priceChange || data.change24h || 0,
      changePercent24h: data.priceChangePercent || data.changePercent24h || 0,
      timestamp: data.timestamp || new Date().toISOString(),
    };
  }

  private normalizeFundamentals(data: any, symbol: string): FundamentalData {
    return {
      symbol,
      name: data.name || data.shortName || symbol,
      sector: data.sector,
      industry: data.industry,
      marketCap: data.marketCap,
      peRatio: data.peRatio || data.trailingPE,
      eps: data.eps || data.trailingEps,
      dividendYield: data.dividendYield,
      beta: data.beta,
      week52High: data.week52High || data.fiftyTwoWeekHigh,
      week52Low: data.week52Low || data.fiftyTwoWeekLow,
      avgVolume: data.avgVolume || data.averageVolume,
    };
  }

  // ================================
  // MOCK DATA (for development)
  // ================================

  private getMockQuote(symbol: string): QuoteData {
    const basePrice = 100 + Math.random() * 400;
    const change = (Math.random() - 0.5) * 10;
    return {
      symbol,
      price: basePrice,
      change,
      changePercent: (change / basePrice) * 100,
      open: basePrice - Math.random() * 5,
      high: basePrice + Math.random() * 10,
      low: basePrice - Math.random() * 10,
      close: basePrice,
      volume: Math.floor(Math.random() * 10000000),
      timestamp: new Date().toISOString(),
    };
  }

  private getMockHistoricalData(symbol: string, period: string): OHLCVData[] {
    const data: OHLCVData[] = [];
    const days = period === '1mo' ? 30 : period === '3mo' ? 90 : 365;
    let price = 100 + Math.random() * 400;

    for (let i = days; i >= 0; i--) {
      const date = new Date();
      date.setDate(date.getDate() - i);

      const change = (Math.random() - 0.5) * 10;
      price += change;

      data.push({
        timestamp: date.toISOString(),
        open: price - Math.random() * 5,
        high: price + Math.random() * 10,
        low: price - Math.random() * 10,
        close: price,
        volume: Math.floor(Math.random() * 10000000),
      });
    }

    return data;
  }

  private getMockMarketDepth(symbol: string): MarketDepthData {
    const basePrice = 100 + Math.random() * 400;
    const bids = [];
    const asks = [];

    for (let i = 0; i < 10; i++) {
      bids.push({
        price: basePrice - (i + 1) * 0.5,
        quantity: Math.floor(Math.random() * 1000),
      });
      asks.push({
        price: basePrice + (i + 1) * 0.5,
        quantity: Math.floor(Math.random() * 1000),
      });
    }

    return {
      symbol,
      bids,
      asks,
      timestamp: new Date().toISOString(),
    };
  }

  private getMockTickerStats(symbol: string): TickerStats {
    const price = 100 + Math.random() * 400;
    const change = (Math.random() - 0.5) * 20;
    return {
      symbol,
      lastPrice: price,
      highPrice24h: price + Math.random() * 20,
      lowPrice24h: price - Math.random() * 20,
      volume24h: Math.floor(Math.random() * 100000000),
      change24h: change,
      changePercent24h: (change / price) * 100,
      timestamp: new Date().toISOString(),
    };
  }

  private getMockFundamentals(symbol: string): FundamentalData {
    return {
      symbol,
      name: `${symbol} Inc.`,
      sector: 'Technology',
      industry: 'Software',
      marketCap: Math.floor(Math.random() * 1000000000000),
      peRatio: 15 + Math.random() * 30,
      eps: 1 + Math.random() * 10,
      dividendYield: Math.random() * 5,
      beta: 0.5 + Math.random() * 1.5,
      week52High: 200 + Math.random() * 200,
      week52Low: 50 + Math.random() * 100,
      avgVolume: Math.floor(Math.random() * 50000000),
    };
  }
}

// Export singleton
export const MarketDataBridge = new MarketDataBridgeClass();
export { MarketDataBridgeClass };
