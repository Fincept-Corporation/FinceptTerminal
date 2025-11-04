/**
 * BIST (Borsa Istanbul) Service
 * Türkiye hisse senedi piyasası verileri
 */

import { Command } from '@tauri-apps/plugin-shell';

export interface BISTStock {
  success: boolean;
  symbol: string;
  name: string;
  currency: string;
  exchange: string;
  current_price: number;
  previous_close: number;
  day_high: number;
  day_low: number;
  volume: number;
  market_cap: number;
  error?: string;
}

export interface BISTHistoricalData {
  success: boolean;
  symbol: string;
  dates: string[];
  open: number[];
  high: number[];
  low: number[];
  close: number[];
  volume: number[];
  data_points: number;
  error?: string;
}

export interface BIST100Summary {
  success: boolean;
  index: string;
  symbol: string;
  current: number;
  change: number;
  change_percent: number;
  high: number;
  low: number;
  volume: number;
  error?: string;
}

export interface BISTMarketSummary {
  success: boolean;
  timestamp: string;
  bist_100: BIST100Summary;
  top_gainers: Array<{
    symbol: string;
    name: string;
    current_price: number;
    change_percent: number;
  }>;
  top_losers: Array<{
    symbol: string;
    name: string;
    current_price: number;
    change_percent: number;
  }>;
}

class BISTService {
  private pythonScript = 'scripts/DataSources/bist_data.py';

  private async executePython(input: any): Promise<any> {
    try {
      const command = Command.create('run-python', [this.pythonScript]);
      const output = await command.execute();

      if (output.code !== 0) {
        throw new Error(output.stderr || 'Python script execution failed');
      }

      return JSON.parse(output.stdout);
    } catch (error: any) {
      console.error('[BISTService] Execution failed:', error);
      throw error;
    }
  }

  /**
   * Get stock information
   */
  async getStockInfo(symbol: string): Promise<BISTStock> {
    const result = await this.executePython({
      action: 'stock_info',
      symbol,
    });
    return result;
  }

  /**
   * Get historical data
   */
  async getHistoricalData(
    symbol: string,
    period: string = '1y',
    interval: string = '1d'
  ): Promise<BISTHistoricalData> {
    const result = await this.executePython({
      action: 'historical',
      symbol,
      period,
      interval,
    });
    return result;
  }

  /**
   * Get BIST 100 index summary
   */
  async getBIST100Summary(): Promise<BIST100Summary> {
    const result = await this.executePython({
      action: 'bist_100',
    });
    return result;
  }

  /**
   * Get top gainers
   */
  async getTopGainers(limit: number = 10): Promise<any> {
    const result = await this.executePython({
      action: 'top_gainers',
      limit,
    });
    return result;
  }

  /**
   * Get top losers
   */
  async getTopLosers(limit: number = 10): Promise<any> {
    const result = await this.executePython({
      action: 'top_losers',
      limit,
    });
    return result;
  }

  /**
   * Get market summary
   */
  async getMarketSummary(): Promise<BISTMarketSummary> {
    const result = await this.executePython({
      action: 'market_summary',
    });
    return result;
  }
}

export const bistService = new BISTService();
export default bistService;
