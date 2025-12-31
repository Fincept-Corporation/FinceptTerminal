// Fyers Market Data Module
// Handles quotes, market depth, historical data, and symbol search

import { fetch } from '@tauri-apps/plugin-http';
import { FyersQuote, FyersMarketDepth, FyersMarketStatus, SymbolMasterEntry } from './types';

export class FyersMarket {
  private fyers: any;
  private symbolMasterCache: SymbolMasterEntry[] = [];

  constructor(fyersClient: any) {
    this.fyers = fyersClient;
  }

  /**
   * Get market status
   */
  async getMarketStatus(): Promise<FyersMarketStatus[]> {
    const response = await this.fyers.market_status();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch market status');
    }

    return response.marketStatus;
  }

  /**
   * Get quotes for symbols
   */
  async getQuotes(symbols: string[]): Promise<Record<string, FyersQuote>> {
    const response = await this.fyers.getQuotes(symbols);

    if (response.s !== 'ok') {
      console.error('[Fyers] Quote API error:', response);
      throw new Error(response.message || 'Failed to fetch quotes');
    }

    const dataArray = response.d || response.data || [];

    if (!Array.isArray(dataArray) || dataArray.length === 0) {
      console.error('[Fyers] Invalid or empty quote data');
      return {};
    }

    const quotesMap: Record<string, FyersQuote> = {};
    dataArray.forEach((quote: any) => {
      const symbol = quote.n || quote.symbol;
      if (!symbol) return;

      const data = quote.v || quote;

      quotesMap[symbol] = {
        n: symbol,
        lp: data.lp || data.last_price || 0,
        ltp: data.lp || data.ltp || data.last_price || 0,
        ch: data.ch || data.change || 0,
        chp: data.chp || data.change_percent || 0,
        high: data.high_price || data.h || data.high || 0,
        low: data.low_price || data.l || data.low || 0,
        open: data.open_price || data.o || data.open || 0,
        volume: data.volume || data.v || 0,
        v: data.volume || data.v || 0,
        o: data.open_price || data.o || data.open || 0,
        h: data.high_price || data.h || data.high || 0,
        l: data.low_price || data.l || data.low || 0,
        ...data
      };
    });

    return quotesMap;
  }

  /**
   * Get market depth
   */
  async getMarketDepth(symbols: string[], includeOHLCV: boolean = true): Promise<Record<string, FyersMarketDepth>> {
    const response = await this.fyers.getMarketDepth({
      symbol: symbols,
      ohlcv_flag: includeOHLCV ? 1 : 0
    });

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch market depth');
    }

    return response.d;
  }

  /**
   * Get historical data
   */
  async getHistory(params: {
    symbol: string;
    resolution: string;
    date_format: string;
    range_from: string;
    range_to: string;
    cont_flag: string;
  }): Promise<any> {
    const response = await this.fyers.getHistory(params);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch history');
    }

    return response;
  }

  /**
   * Get option chain
   */
  async getOptionChain(symbol: string, strikeCount: number = 5, timestamp: string = ""): Promise<any> {
    const response = await this.fyers.getOptionChain({
      symbol,
      strikecount: strikeCount,
      timestamp
    });

    if (response.code !== 200) {
      throw new Error(response.message || 'Failed to fetch option chain');
    }

    return response.data;
  }

  /**
   * Check if market is currently open (NSE/BSE hours)
   * NSE: 9:15 AM - 3:30 PM IST, Monday-Friday
   */
  isMarketOpen(): boolean {
    const now = new Date();

    const istOffset = 5.5 * 60 * 60 * 1000;
    const istTime = new Date(now.getTime() + istOffset);

    const day = istTime.getUTCDay();
    const hour = istTime.getUTCHours();
    const minute = istTime.getUTCMinutes();
    const timeInMinutes = hour * 60 + minute;

    if (day === 0 || day === 6) {
      return false;
    }

    const marketOpen = 9 * 60 + 15;
    const marketClose = 15 * 60 + 30;

    return timeInMinutes >= marketOpen && timeInMinutes <= marketClose;
  }

  /**
   * Load symbol master from Fyers API
   */
  async loadSymbolMaster(): Promise<void> {
    try {
      const response = await fetch('https://public.fyers.in/sym_details/NSE_CM_sym_master.json');
      const data = await response.json();

      if (data && typeof data === 'object' && !Array.isArray(data)) {
        this.symbolMasterCache = Object.entries(data).map(([symbol, details]: [string, any]) => ({
          symbol: symbol,
          fytoken: details.fyToken || details.fytoken,
          fyToken: details.fyToken || details.fytoken,
          description: details.symbolDesc || details.symDetails || '',
          exchange: details.exchangeName || 'NSE',
          segment: details.exSeries || '',
          lotsize: details.minLotSize || 1,
          lotSize: details.minLotSize || 1,
          tick_size: details.tickSize || 0.05,
          tickSize: details.tickSize || 0.05,
          shortName: details.short_name || details.exSymbol || '',
          displayName: details.display_format_mob || details.exSymName || ''
        }));
      } else if (Array.isArray(data)) {
        this.symbolMasterCache = data;
      } else {
        this.symbolMasterCache = [];
      }
    } catch (error) {
      console.error('[Fyers] Failed to load symbol master:', error);
      this.symbolMasterCache = [];
    }
  }

  /**
   * Search symbols
   */
  searchSymbols(query: string, limit: number = 20): SymbolMasterEntry[] {
    if (!Array.isArray(this.symbolMasterCache) || this.symbolMasterCache.length === 0) {
      console.warn('[Fyers] Symbol master not loaded or invalid');
      return [];
    }

    if (!query || query.trim().length === 0) {
      return [];
    }

    const queryLower = query.toLowerCase().trim();
    try {
      return this.symbolMasterCache
        .filter(entry => {
          if (!entry) return false;
          const symbol = entry.symbol?.toLowerCase() || '';
          const desc = entry.description?.toLowerCase() || '';
          const shortName = entry.shortName?.toLowerCase() || '';
          const displayName = entry.displayName?.toLowerCase() || '';

          return symbol.includes(queryLower) ||
                 desc.includes(queryLower) ||
                 shortName.includes(queryLower) ||
                 displayName.includes(queryLower);
        })
        .slice(0, limit);
    } catch (error) {
      console.error('[Fyers] Error searching symbols:', error);
      return [];
    }
  }

  /**
   * Get symbol by exact match
   */
  getSymbol(symbol: string): SymbolMasterEntry | undefined {
    return this.symbolMasterCache.find(entry => entry.symbol === symbol);
  }
}
