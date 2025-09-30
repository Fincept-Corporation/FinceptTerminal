// Kraken Crypto Exchange API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class KrakenAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const apiKey = this.getConfig<string>('apiKey');
      const apiSecret = this.getConfig<string>('apiSecret');

      console.log('Testing Kraken API connection...');

      try {
        // Test with public server time endpoint
        const response = await fetch('https://api.kraken.com/0/public/Time');

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        if (data.error && data.error.length > 0) {
          throw new Error(data.error.join(', '));
        }

        return this.createSuccessResult('Successfully connected to Kraken API', {
          serverTime: new Date(data.result.unixtime * 1000).toISOString(),
          rfc1123: data.result.rfc1123,
          authenticated: !!(apiKey && apiSecret),
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(endpoint: string, params?: Record<string, any>): Promise<any> {
    try {
      const isPublic = endpoint.includes('/public/');
      const url = `https://api.kraken.com/0${endpoint}`;

      const queryParams = params ? `?${new URLSearchParams(params).toString()}` : '';

      const response = await fetch(`${url}${queryParams}`, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json',
        },
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data.error && data.error.length > 0) {
        throw new Error(data.error.join(', '));
      }

      return data.result;
    } catch (error) {
      throw new Error(`Kraken query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      hasApiSecret: !!this.getConfig('apiSecret'),
      provider: 'Kraken',
    };
  }

  /**
   * Get ticker information
   */
  async getTicker(pair: string): Promise<any> {
    return await this.query('/public/Ticker', { pair });
  }

  /**
   * Get OHLC data
   */
  async getOHLC(pair: string, interval: number = 1): Promise<any> {
    return await this.query('/public/OHLC', { pair, interval: interval.toString() });
  }

  /**
   * Get order book
   */
  async getOrderBook(pair: string, count: number = 100): Promise<any> {
    return await this.query('/public/Depth', { pair, count: count.toString() });
  }

  /**
   * Get recent trades
   */
  async getTrades(pair: string): Promise<any> {
    return await this.query('/public/Trades', { pair });
  }

  /**
   * Get tradable asset pairs
   */
  async getAssetPairs(): Promise<any> {
    return await this.query('/public/AssetPairs');
  }

  /**
   * Get asset info
   */
  async getAssets(): Promise<any> {
    return await this.query('/public/Assets');
  }

  /**
   * Get server time
   */
  async getServerTime(): Promise<any> {
    return await this.query('/public/Time');
  }

  /**
   * Get system status
   */
  async getSystemStatus(): Promise<any> {
    return await this.query('/public/SystemStatus');
  }
}
