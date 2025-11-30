// File: src/services/hyperliquid/hyperliquidApi.ts
// HyperLiquid REST API service
// Documentation: https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/info-endpoint

import type {
  HyperLiquidCandle,
  HyperLiquidCandleInterval,
  HyperLiquidAssetInfo,
  HyperLiquidL2BookData,
  HyperLiquidClearinghouseState
} from '../../types/hyperliquid';

/**
 * HyperLiquid REST API Client
 * Provides methods for fetching historical data and account information
 */
export class HyperLiquidApi {
  private readonly baseUrl: string;
  private readonly testnet: boolean;

  constructor(testnet: boolean = false) {
    this.testnet = testnet;
    this.baseUrl = testnet
      ? 'https://api.hyperliquid-testnet.xyz'
      : 'https://api.hyperliquid.xyz';
  }

  /**
   * Make POST request to /info endpoint
   */
  private async request<T>(body: any): Promise<T> {
    const response = await fetch(`${this.baseUrl}/info`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(body)
    });

    if (!response.ok) {
      throw new Error(`HyperLiquid API error: ${response.status} ${response.statusText}`);
    }

    return response.json();
  }

  /**
   * Get candlestick/OHLC data
   * @param coin - Symbol (e.g., 'BTC', 'ETH')
   * @param interval - Candle interval (1m, 5m, 15m, 30m, 1h, 4h, 1d, etc.)
   * @param startTime - Start timestamp in milliseconds
   * @param endTime - End timestamp in milliseconds (optional, defaults to now)
   * @returns Array of candles (max 5000)
   */
  async getCandles(
    coin: string,
    interval: HyperLiquidCandleInterval,
    startTime: number,
    endTime?: number
  ): Promise<HyperLiquidCandle[]> {
    const body = {
      type: 'candleSnapshot',
      req: {
        coin,
        interval,
        startTime,
        ...(endTime && { endTime })
      }
    };

    const response = await this.request<HyperLiquidCandle[]>(body);
    return response;
  }

  /**
   * Get mid-prices for all coins
   * @returns Record of coin -> mid price
   */
  async getAllMids(): Promise<Record<string, string>> {
    const body = {
      type: 'allMids'
    };

    const response = await this.request<Record<string, string>>(body);
    return response;
  }

  /**
   * Get L2 order book snapshot
   * @param coin - Symbol (e.g., 'BTC', 'ETH')
   * @param nSigFigs - Number of significant figures (2-5, default: 5)
   * @param mantissa - Mantissa value (1, 2, or 5, only with nSigFigs=5)
   * @returns Order book with bids and asks
   */
  async getL2Book(
    coin: string,
    nSigFigs: number = 5,
    mantissa?: 1 | 2 | 5
  ): Promise<HyperLiquidL2BookData> {
    const body: any = {
      type: 'l2Book',
      coin,
      nSigFigs
    };

    if (mantissa && nSigFigs === 5) {
      body.mantissa = mantissa;
    }

    const response = await this.request<HyperLiquidL2BookData>(body);
    return response;
  }

  /**
   * Get user's clearinghouse state (positions, balances, etc.)
   * @param userAddress - User's wallet address
   * @returns User state including positions and account value
   */
  async getUserState(userAddress: string): Promise<HyperLiquidClearinghouseState> {
    const body = {
      type: 'clearinghouseState',
      user: userAddress
    };

    const response = await this.request<HyperLiquidClearinghouseState>(body);
    return response;
  }

  /**
   * Get user's open orders
   * @param userAddress - User's wallet address
   * @returns Array of open orders
   */
  async getOpenOrders(userAddress: string): Promise<any[]> {
    const body = {
      type: 'openOrders',
      user: userAddress
    };

    const response = await this.request<any[]>(body);
    return response;
  }

  /**
   * Get user's fill history
   * @param userAddress - User's wallet address
   * @param aggregateByTime - Whether to aggregate fills by time (default: false)
   * @returns Array of fills (max 2000 most recent)
   */
  async getUserFills(userAddress: string, aggregateByTime: boolean = false): Promise<any[]> {
    const body = {
      type: 'userFills',
      user: userAddress,
      aggregateByTime
    };

    const response = await this.request<any[]>(body);
    return response;
  }

  /**
   * Get user's fills by time range
   * @param userAddress - User's wallet address
   * @param startTime - Start timestamp in milliseconds
   * @param endTime - End timestamp in milliseconds (optional, defaults to now)
   * @returns Array of fills (max 2000)
   */
  async getUserFillsByTime(
    userAddress: string,
    startTime: number,
    endTime?: number
  ): Promise<any[]> {
    const body = {
      type: 'userFillsByTime',
      user: userAddress,
      startTime,
      ...(endTime && { endTime })
    };

    const response = await this.request<any[]>(body);
    return response;
  }

  /**
   * Get metadata about available assets
   * @returns Universe of tradeable assets with their properties
   */
  async getMeta(): Promise<{ universe: HyperLiquidAssetInfo[] }> {
    const body = {
      type: 'meta'
    };

    const response = await this.request<{ universe: HyperLiquidAssetInfo[] }>(body);
    return response;
  }

  /**
   * Get funding rate history for a coin
   * @param coin - Symbol (e.g., 'BTC', 'ETH')
   * @param startTime - Start timestamp in milliseconds
   * @param endTime - End timestamp in milliseconds (optional)
   * @returns Array of funding rate data
   */
  async getFundingHistory(
    coin: string,
    startTime: number,
    endTime?: number
  ): Promise<any[]> {
    const body = {
      type: 'fundingHistory',
      coin,
      startTime,
      ...(endTime && { endTime })
    };

    const response = await this.request<any[]>(body);
    return response;
  }

  /**
   * Convenience method: Get latest price for a coin
   * @param coin - Symbol (e.g., 'BTC', 'ETH')
   * @returns Current mid-price as number
   */
  async getCurrentPrice(coin: string): Promise<number> {
    const allMids = await this.getAllMids();
    const price = allMids[coin];

    if (!price) {
      throw new Error(`No price found for ${coin}`);
    }

    return parseFloat(price);
  }

  /**
   * Convenience method: Get recent candles (last N periods)
   * @param coin - Symbol (e.g., 'BTC', 'ETH')
   * @param interval - Candle interval
   * @param count - Number of candles to fetch (max 5000)
   * @returns Array of recent candles
   */
  async getRecentCandles(
    coin: string,
    interval: HyperLiquidCandleInterval,
    count: number = 100
  ): Promise<HyperLiquidCandle[]> {
    // Calculate time range based on interval
    const intervalMs = this.intervalToMs(interval);
    const endTime = Date.now();
    const startTime = endTime - (intervalMs * count);

    return this.getCandles(coin, interval, startTime, endTime);
  }

  /**
   * Convert interval string to milliseconds
   */
  private intervalToMs(interval: HyperLiquidCandleInterval): number {
    const map: Record<string, number> = {
      '1m': 60 * 1000,
      '3m': 3 * 60 * 1000,
      '5m': 5 * 60 * 1000,
      '15m': 15 * 60 * 1000,
      '30m': 30 * 60 * 1000,
      '1h': 60 * 60 * 1000,
      '2h': 2 * 60 * 60 * 1000,
      '4h': 4 * 60 * 60 * 1000,
      '8h': 8 * 60 * 60 * 1000,
      '12h': 12 * 60 * 60 * 1000,
      '1d': 24 * 60 * 60 * 1000,
      '3d': 3 * 24 * 60 * 60 * 1000,
      '1w': 7 * 24 * 60 * 60 * 1000,
      '1M': 30 * 24 * 60 * 60 * 1000
    };

    return map[interval] || 60 * 1000;
  }
}

/**
 * Create API instance
 * @param testnet - Whether to use testnet (default: false)
 */
export function createHyperLiquidApi(testnet: boolean = false): HyperLiquidApi {
  return new HyperLiquidApi(testnet);
}

/**
 * Usage Examples:
 *
 * // Create API instance
 * const api = createHyperLiquidApi();
 *
 * // Get candles
 * const candles = await api.getCandles('BTC', '1h', startTime, endTime);
 *
 * // Get current price
 * const price = await api.getCurrentPrice('BTC');
 *
 * // Get order book
 * const book = await api.getL2Book('ETH', 5);
 *
 * // Get user state
 * const state = await api.getUserState('0x...');
 *
 * // Get metadata
 * const meta = await api.getMeta();
 */
