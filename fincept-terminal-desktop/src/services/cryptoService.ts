/**
 * Cryptocurrency Service
 * Kripto para verileri (Binance, CoinGecko, CoinMarketCap)
 */

import { Command } from '@tauri-apps/plugin-shell';

export interface CryptoPrice {
  success: boolean;
  symbol: string;
  quote: string;
  pair: string;
  price: number;
  change_24h: number;
  change_percent_24h: number;
  high_24h: number;
  low_24h: number;
  volume_24h: number;
  quote_volume_24h: number;
  timestamp: string;
  error?: string;
}

export interface CryptoHistoricalData {
  success: boolean;
  symbol: string;
  quote: string;
  pair: string;
  interval: string;
  dates: string[];
  open: number[];
  high: number[];
  low: number[];
  close: number[];
  volume: number[];
  data_points: number;
  error?: string;
}

export interface CryptoInfo {
  success: boolean;
  id: string;
  symbol: string;
  name: string;
  current_price_usd: number;
  current_price_try: number;
  market_cap_usd: number;
  market_cap_rank: number;
  total_volume_usd: number;
  high_24h: number;
  low_24h: number;
  price_change_24h: number;
  price_change_percentage_24h: number;
  price_change_percentage_7d: number;
  price_change_percentage_30d: number;
  circulating_supply: number;
  total_supply: number;
  max_supply: number;
  ath: number;
  ath_date: string;
  atl: number;
  atl_date: string;
  error?: string;
}

export interface GlobalMarketData {
  success: boolean;
  active_cryptocurrencies: number;
  markets: number;
  total_market_cap_usd: number;
  total_market_cap_try: number;
  total_volume_24h_usd: number;
  market_cap_change_24h: number;
  bitcoin_dominance: number;
  ethereum_dominance: number;
  timestamp: string;
  error?: string;
}

export interface FearGreedIndex {
  success: boolean;
  value: number;
  classification: string;
  timestamp: string;
  error?: string;
}

class CryptoService {
  private pythonScript = 'scripts/DataSources/crypto_data.py';

  private async executePython(input: any): Promise<any> {
    try {
      const command = Command.create('run-python', [this.pythonScript]);
      const output = await command.execute();

      if (output.code !== 0) {
        throw new Error(output.stderr || 'Python script execution failed');
      }

      return JSON.parse(output.stdout);
    } catch (error: any) {
      console.error('[CryptoService] Execution failed:', error);
      throw error;
    }
  }

  /**
   * Get Binance price (USDT pair)
   */
  async getBinancePrice(symbol: string, quote: string = 'USDT'): Promise<CryptoPrice> {
    const result = await this.executePython({
      action: 'binance_price',
      symbol,
      quote,
    });
    return result;
  }

  /**
   * Get Binance price in Turkish Lira
   */
  async getBinanceTRYPrice(symbol: string): Promise<CryptoPrice> {
    const result = await this.executePython({
      action: 'binance_try',
      symbol,
    });
    return result;
  }

  /**
   * Get historical klines/candlestick data
   */
  async getHistoricalData(
    symbol: string,
    interval: string = '1d',
    limit: number = 365,
    quote: string = 'USDT'
  ): Promise<CryptoHistoricalData> {
    const result = await this.executePython({
      action: 'historical',
      symbol,
      interval,
      limit,
      quote,
    });
    return result;
  }

  /**
   * Get CoinGecko coin info
   */
  async getCoinGeckoInfo(coinId: string): Promise<CryptoInfo> {
    const result = await this.executePython({
      action: 'coingecko_info',
      coin_id: coinId,
    });
    return result;
  }

  /**
   * Get top cryptocurrencies
   */
  async getTopCryptocurrencies(limit: number = 100, currency: string = 'usd'): Promise<any> {
    const result = await this.executePython({
      action: 'top_coins',
      limit,
      currency,
    });
    return result;
  }

  /**
   * Get trending coins
   */
  async getTrendingCoins(): Promise<any> {
    const result = await this.executePython({
      action: 'trending',
    });
    return result;
  }

  /**
   * Get global market data
   */
  async getGlobalMarketData(): Promise<GlobalMarketData> {
    const result = await this.executePython({
      action: 'global_data',
    });
    return result;
  }

  /**
   * Get Fear & Greed Index
   */
  async getFearGreedIndex(): Promise<FearGreedIndex> {
    const result = await this.executePython({
      action: 'fear_greed',
    });
    return result;
  }

  /**
   * Get market summary (all data)
   */
  async getMarketSummary(): Promise<any> {
    const result = await this.executePython({
      action: 'market_summary',
    });
    return result;
  }
}

export const cryptoService = new CryptoService();
export default cryptoService;
