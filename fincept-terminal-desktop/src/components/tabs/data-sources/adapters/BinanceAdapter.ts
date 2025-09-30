// Binance Crypto Exchange API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class BinanceAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const apiKey = this.getConfig<string>('apiKey');
      const apiSecret = this.getConfig<string>('apiSecret');
      const testnet = this.getConfig<boolean>('testnet', false);

      console.log('Testing Binance API connection...');

      try {
        const baseUrl = testnet ? 'https://testnet.binance.vision' : 'https://api.binance.com';

        // Test with ping endpoint (no auth required)
        const pingResponse = await fetch(`${baseUrl}/api/v3/ping`);

        if (!pingResponse.ok) {
          throw new Error(`HTTP ${pingResponse.status}: ${pingResponse.statusText}`);
        }

        // Get server time
        const timeResponse = await fetch(`${baseUrl}/api/v3/time`);
        const timeData = await timeResponse.json();

        // If API key provided, test authenticated endpoint
        let accountStatus = 'Not authenticated';
        if (apiKey && apiSecret) {
          accountStatus = 'Authenticated (key provided)';
        }

        return this.createSuccessResult('Successfully connected to Binance API', {
          serverTime: new Date(timeData.serverTime).toISOString(),
          testnet,
          authenticated: !!(apiKey && apiSecret),
          accountStatus,
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

  async query(endpoint: string, params?: Record<string, any>, signed: boolean = false): Promise<any> {
    try {
      const apiKey = this.getConfig<string>('apiKey');
      const testnet = this.getConfig<boolean>('testnet', false);

      const baseUrl = testnet ? 'https://testnet.binance.vision' : 'https://api.binance.com';

      const headers: Record<string, string> = {};
      if (apiKey) {
        headers['X-MBX-APIKEY'] = apiKey;
      }

      const queryParams = params ? `?${new URLSearchParams(params).toString()}` : '';
      const url = `${baseUrl}${endpoint}${queryParams}`;

      const response = await fetch(url, {
        method: 'GET',
        headers,
      });

      if (!response.ok) {
        const error = await response.json();
        throw new Error(error.msg || `Query failed: ${response.status}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Binance query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      hasApiSecret: !!this.getConfig('apiSecret'),
      testnet: this.getConfig('testnet', false),
      provider: 'Binance',
    };
  }

  /**
   * Get ticker price
   */
  async getTickerPrice(symbol: string): Promise<any> {
    return await this.query('/api/v3/ticker/price', { symbol });
  }

  /**
   * Get 24hr ticker statistics
   */
  async get24hrTicker(symbol?: string): Promise<any> {
    const params = symbol ? { symbol } : {};
    return await this.query('/api/v3/ticker/24hr', params);
  }

  /**
   * Get order book
   */
  async getOrderBook(symbol: string, limit: number = 100): Promise<any> {
    return await this.query('/api/v3/depth', { symbol, limit: limit.toString() });
  }

  /**
   * Get recent trades
   */
  async getRecentTrades(symbol: string, limit: number = 500): Promise<any> {
    return await this.query('/api/v3/trades', { symbol, limit: limit.toString() });
  }

  /**
   * Get klines/candlestick data
   */
  async getKlines(symbol: string, interval: string, limit: number = 500): Promise<any> {
    return await this.query('/api/v3/klines', {
      symbol,
      interval,
      limit: limit.toString(),
    });
  }

  /**
   * Get exchange info
   */
  async getExchangeInfo(): Promise<any> {
    return await this.query('/api/v3/exchangeInfo');
  }

  /**
   * Get current average price
   */
  async getAvgPrice(symbol: string): Promise<any> {
    return await this.query('/api/v3/avgPrice', { symbol });
  }

  /**
   * Get all ticker prices
   */
  async getAllPrices(): Promise<any> {
    return await this.query('/api/v3/ticker/price');
  }

  /**
   * Get ticker order book
   */
  async getBookTicker(symbol?: string): Promise<any> {
    const params = symbol ? { symbol } : {};
    return await this.query('/api/v3/ticker/bookTicker', params);
  }
}
