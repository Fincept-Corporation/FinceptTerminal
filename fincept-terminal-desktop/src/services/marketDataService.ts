// Market Data Service - TypeScript wrapper for Tauri market data commands
// Modular design: gracefully handles failures and can be extended with other data sources

import { invoke } from '@tauri-apps/api/core';

export interface QuoteData {
  symbol: string;
  price: number;
  change: number;
  change_percent: number;
  volume?: number;
  high?: number;
  low?: number;
  open?: number;
  previous_close?: number;
  timestamp: number;
}

export interface QuoteResponse {
  success: boolean;
  data?: QuoteData;
  error?: string;
}

export interface QuotesResponse {
  success: boolean;
  data: QuoteData[];
  error?: string;
}

export interface PeriodReturns {
  symbol: string;
  seven_day: number;
  thirty_day: number;
}

export interface PeriodReturnsResponse {
  success: boolean;
  data?: PeriodReturns;
  error?: string;
}

/**
 * Market Data Service
 * Provides a clean TypeScript interface to fetch market data
 * Handles errors gracefully and provides fallback behavior
 */
class MarketDataService {
  private isAvailable: boolean | null = null;

  /**
   * Check if market data service is available
   */
  async checkHealth(): Promise<boolean> {
    try {
      const result = await invoke<boolean>('check_market_data_health');
      this.isAvailable = result;
      return result;
    } catch (error) {
      console.error('[MarketDataService] Health check failed:', error);
      this.isAvailable = false;
      return false;
    }
  }

  /**
   * Fetch a single quote
   * Returns null if fetch fails (graceful degradation)
   */
  async getQuote(symbol: string): Promise<QuoteData | null> {
    try {
      const response = await invoke<QuoteResponse>('get_market_quote', { symbol });

      if (response.success && response.data) {
        return response.data;
      } else {
        console.warn(`[MarketDataService] Failed to fetch quote for ${symbol}:`, response.error);
        return null;
      }
    } catch (error) {
      console.error(`[MarketDataService] Error fetching quote for ${symbol}:`, error);
      return null;
    }
  }

  /**
   * Fetch multiple quotes in batch
   * Returns only successful fetches (partial success model)
   */
  async getQuotes(symbols: string[]): Promise<QuoteData[]> {
    try {
      const response = await invoke<QuotesResponse>('get_market_quotes', { symbols });

      if (response.success) {
        return response.data;
      } else {
        console.warn('[MarketDataService] Failed to fetch quotes:', response.error);
        return [];
      }
    } catch (error) {
      console.error('[MarketDataService] Error fetching quotes:', error);
      return [];
    }
  }

  /**
   * Fetch period returns (7D, 30D)
   * Returns null if fetch fails
   */
  async getPeriodReturns(symbol: string): Promise<PeriodReturns | null> {
    try {
      const response = await invoke<PeriodReturnsResponse>('get_period_returns', { symbol });

      if (response.success && response.data) {
        return response.data;
      } else {
        console.warn(`[MarketDataService] Failed to fetch returns for ${symbol}:`, response.error);
        return null;
      }
    } catch (error) {
      console.error(`[MarketDataService] Error fetching returns for ${symbol}:`, error);
      return null;
    }
  }

  /**
   * Fetch enhanced quote with period returns
   * Combines quote data with 7D/30D returns
   */
  async getEnhancedQuote(symbol: string): Promise<(QuoteData & { seven_day?: number; thirty_day?: number }) | null> {
    const [quote, returns] = await Promise.all([
      this.getQuote(symbol),
      this.getPeriodReturns(symbol)
    ]);

    if (!quote) return null;

    return {
      ...quote,
      seven_day: returns?.seven_day,
      thirty_day: returns?.thirty_day
    };
  }

  /**
   * Fetch enhanced quotes for multiple symbols
   * Fetches quotes and returns in parallel
   */
  async getEnhancedQuotes(symbols: string[]): Promise<(QuoteData & { seven_day?: number; thirty_day?: number })[]> {
    const quotes = await this.getQuotes(symbols);

    // Fetch returns for all symbols in parallel
    const returnsPromises = symbols.map(symbol => this.getPeriodReturns(symbol));
    const returns = await Promise.all(returnsPromises);

    // Merge quotes with returns
    return quotes.map((quote, index) => ({
      ...quote,
      seven_day: returns[index]?.seven_day,
      thirty_day: returns[index]?.thirty_day
    }));
  }

  /**
   * Format price change with sign
   */
  formatChange(value: number): string {
    return value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  }

  /**
   * Format percentage change with sign
   */
  formatChangePercent(value: number): string {
    return value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;
  }

  /**
   * Check if service is likely available
   * (cached result from last health check)
   */
  isLikelyAvailable(): boolean {
    return this.isAvailable !== false;
  }
}

// Export singleton instance
export const marketDataService = new MarketDataService();
export default marketDataService;
