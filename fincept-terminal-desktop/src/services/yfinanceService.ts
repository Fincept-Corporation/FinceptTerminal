// YFinance Service - TypeScript wrapper for Tauri yfinance commands
// Provides historical data and stock information

import { invoke } from '@tauri-apps/api/core';

export interface HistoricalDataPoint {
  symbol: string;
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  adj_close: number;
}

export interface HistoricalResponse {
  success: boolean;
  data: HistoricalDataPoint[];
  error?: string;
}

export interface StockInfo {
  symbol: string;
  company_name: string;
  sector: string;
  industry: string;
  market_cap?: number;
  pe_ratio?: number;
  dividend_yield?: number;
  beta?: number;
  fifty_two_week_high?: number;
  fifty_two_week_low?: number;
  description?: string;
  website?: string;
  country?: string;
  [key: string]: any;
}

export interface StockInfoResponse {
  success: boolean;
  data?: StockInfo;
  error?: string;
}

/**
 * YFinance Service
 * Provides historical market data and stock information via yfinance Python library
 */
class YFinanceService {
  /**
   * Fetch historical data for a symbol
   * @param symbol Stock symbol (e.g., "AAPL")
   * @param startDate Start date in YYYY-MM-DD format
   * @param endDate End date in YYYY-MM-DD format
   */
  async getHistoricalData(
    symbol: string,
    startDate: string,
    endDate: string
  ): Promise<HistoricalDataPoint[]> {
    try {
      const response = await invoke<HistoricalResponse>('get_historical_data', {
        symbol,
        startDate,
        endDate,
      });

      if (response.success && response.data) {
        return response.data;
      } else {
        console.warn(
          `[YFinanceService] Failed to fetch historical data for ${symbol}:`,
          response.error
        );
        return [];
      }
    } catch (error) {
      console.error(
        `[YFinanceService] Error fetching historical data for ${symbol}:`,
        error
      );
      return [];
    }
  }

  /**
   * Fetch stock information and company details
   * @param symbol Stock symbol (e.g., "AAPL")
   */
  async getStockInfo(symbol: string): Promise<StockInfo | null> {
    try {
      const response = await invoke<StockInfoResponse>('get_stock_info', {
        symbol,
      });

      if (response.success && response.data) {
        return response.data;
      } else {
        console.warn(
          `[YFinanceService] Failed to fetch stock info for ${symbol}:`,
          response.error
        );
        return null;
      }
    } catch (error) {
      console.error(
        `[YFinanceService] Error fetching stock info for ${symbol}:`,
        error
      );
      return null;
    }
  }

  /**
   * Helper to get recent historical data (e.g., last 30 days)
   * @param symbol Stock symbol
   * @param days Number of days to look back (default: 30)
   */
  async getRecentHistory(symbol: string, days: number = 30): Promise<HistoricalDataPoint[]> {
    const endDate = new Date();
    const startDate = new Date();
    startDate.setDate(startDate.getDate() - days);

    const formatDate = (date: Date) => {
      return date.toISOString().split('T')[0];
    };

    return this.getHistoricalData(
      symbol,
      formatDate(startDate),
      formatDate(endDate)
    );
  }
}

// Export singleton instance
export const yfinanceService = new YFinanceService();
export default yfinanceService;
