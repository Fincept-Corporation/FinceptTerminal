/**
 * Market Data Bridge
 * Directly invokes Rust/Python (yfinance) Tauri commands.
 * No HTTP adapters — uses the embedded Python runtime.
 */

import { invoke } from '@tauri-apps/api/core';

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
  previousClose?: number;
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
  adjClose?: number;
}

export interface HistoricalDataRequest {
  symbol: string;
  interval?: string;
  period?: string;
  startDate?: string;
  endDate?: string;
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

// ================================
// MARKET DATA BRIDGE
// ================================

class MarketDataBridgeClass {
  /**
   * Get a real-time quote via Rust get_market_quote → yfinance Python
   */
  async getQuote(symbol: string): Promise<QuoteData> {
    const response: any = await invoke('get_market_quote', { symbol });
    if (!response.success) throw new Error(response.error || `Failed to fetch quote for ${symbol}`);
    return this.normalizeQuote(response.data, symbol);
  }

  /**
   * Get multiple quotes in one call
   */
  async getQuotes(symbols: string[]): Promise<QuoteData[]> {
    const response: any = await invoke('get_market_quotes', { symbols });
    if (!response.success) throw new Error(response.error || 'Failed to fetch quotes');
    return (response.data as any[]).map((d) => this.normalizeQuote(d, d.symbol));
  }

  /**
   * Get OHLCV historical data via Rust get_historical_data → yfinance Python
   */
  async getHistoricalData(request: HistoricalDataRequest): Promise<OHLCVData[]> {
    const today = new Date().toISOString().slice(0, 10);
    const startDate = request.startDate || this.periodToStartDate(request.period || '1mo');
    const endDate = request.endDate || today;

    const response: any = await invoke('get_historical_data', {
      symbol: request.symbol,
      startDate,
      endDate,
    });
    if (!response.success) throw new Error(response.error || `Failed to fetch historical data for ${request.symbol}`);
    return (response.data as any[]).map((d) => ({
      timestamp: typeof d.timestamp === 'number' ? new Date(d.timestamp * 1000).toISOString() : d.timestamp,
      open: d.open,
      high: d.high,
      low: d.low,
      close: d.close,
      volume: d.volume,
      adjClose: d.adj_close,
    }));
  }

  /**
   * Get fundamental/company info via Rust get_stock_info → yfinance Python
   */
  async getFundamentals(symbol: string): Promise<FundamentalData> {
    const response: any = await invoke('get_stock_info', { symbol });
    if (!response.success) throw new Error(response.error || `Failed to fetch fundamentals for ${symbol}`);
    const d = response.data;
    return {
      symbol: d.symbol || symbol,
      name: d.company_name || symbol,
      sector: d.sector,
      industry: d.industry,
      marketCap: d.market_cap,
      peRatio: d.pe_ratio,
      eps: d.eps,
      dividendYield: d.dividend_yield,
      beta: d.beta,
      week52High: d.fifty_two_week_high,
      week52Low: d.fifty_two_week_low,
      avgVolume: d.average_volume,
    };
  }

  /**
   * Ticker stats — derived from quote + period returns
   */
  async getTickerStats(symbol: string): Promise<TickerStats> {
    const [quoteResp, returnsResp] = await Promise.all([
      invoke('get_market_quote', { symbol }) as Promise<any>,
      invoke('get_period_returns', { symbol }) as Promise<any>,
    ]);

    if (!quoteResp.success) throw new Error(quoteResp.error || `Failed to fetch stats for ${symbol}`);
    const q = quoteResp.data;

    return {
      symbol: q.symbol || symbol,
      lastPrice: q.price,
      highPrice24h: q.high ?? 0,
      lowPrice24h: q.low ?? 0,
      volume24h: q.volume ?? 0,
      change24h: q.change ?? 0,
      changePercent24h: q.change_percent ?? 0,
      timestamp: new Date().toISOString(),
    };
  }

  /**
   * Market depth is not available via yfinance — return empty structure.
   * For crypto depth use the Rust WebSocket orderbook commands instead.
   */
  async getMarketDepth(symbol: string): Promise<MarketDepthData> {
    return { symbol, bids: [], asks: [], timestamp: new Date().toISOString() };
  }

  /**
   * Subscribe to real-time quotes via polling (yfinance doesn't stream)
   */
  subscribeToQuotes(
    symbols: string[],
    callback: (quote: QuoteData) => void,
    intervalMs: number = 10000
  ): () => void {
    const timer = setInterval(async () => {
      for (const symbol of symbols) {
        try {
          const quote = await this.getQuote(symbol);
          callback(quote);
        } catch (err) {
          console.error(`[MarketDataBridge] subscription error for ${symbol}:`, err);
        }
      }
    }, intervalMs);
    return () => clearInterval(timer);
  }

  // ================================
  // PRIVATE HELPERS
  // ================================

  private normalizeQuote(d: any, symbol: string): QuoteData {
    return {
      symbol: d.symbol || symbol,
      price: d.price ?? 0,
      change: d.change ?? 0,
      changePercent: d.change_percent ?? 0,
      open: d.open,
      high: d.high,
      low: d.low,
      close: d.price,
      volume: d.volume,
      previousClose: d.previous_close,
      timestamp: d.timestamp
        ? new Date(typeof d.timestamp === 'number' ? d.timestamp * 1000 : d.timestamp).toISOString()
        : new Date().toISOString(),
    };
  }

  private periodToStartDate(period: string): string {
    const now = new Date();
    const map: Record<string, number> = {
      '1d': 1, '5d': 5, '1mo': 30, '3mo': 90,
      '6mo': 180, '1y': 365, '2y': 730, '5y': 1825,
    };
    const days = map[period] ?? 30;
    now.setDate(now.getDate() - days);
    return now.toISOString().slice(0, 10);
  }
}

export const MarketDataBridge = new MarketDataBridgeClass();
export { MarketDataBridgeClass };
