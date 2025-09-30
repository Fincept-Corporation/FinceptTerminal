// Coinbase Exchange API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CoinbaseAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const apiKey = this.getConfig<string>('apiKey');
      const apiSecret = this.getConfig<string>('apiSecret');

      console.log('Testing Coinbase API connection...');

      try {
        // Test with public endpoint
        const response = await fetch('https://api.coinbase.com/v2/time');

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Coinbase API', {
          serverTime: data.data.iso,
          epoch: data.data.epoch,
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

  async query(endpoint: string, method: string = 'GET', body?: any): Promise<any> {
    try {
      const apiKey = this.getConfig<string>('apiKey');
      const baseUrl = 'https://api.coinbase.com';

      const headers: Record<string, string> = {
        'Content-Type': 'application/json',
      };

      if (apiKey) {
        headers['CB-ACCESS-KEY'] = apiKey;
      }

      const options: RequestInit = {
        method,
        headers,
      };

      if (body && method !== 'GET') {
        options.body = JSON.stringify(body);
      }

      const response = await fetch(`${baseUrl}${endpoint}`, options);

      if (!response.ok) {
        const error = await response.json();
        throw new Error(error.errors?.[0]?.message || `Query failed: ${response.status}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Coinbase query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      hasApiSecret: !!this.getConfig('apiSecret'),
      provider: 'Coinbase',
    };
  }

  /**
   * Get exchange rates
   */
  async getExchangeRates(currency: string = 'USD'): Promise<any> {
    const result = await this.query(`/v2/exchange-rates?currency=${currency}`);
    return result.data;
  }

  /**
   * Get currencies
   */
  async getCurrencies(): Promise<any> {
    const result = await this.query('/v2/currencies');
    return result.data;
  }

  /**
   * Get spot price
   */
  async getSpotPrice(currencyPair: string = 'BTC-USD'): Promise<any> {
    const result = await this.query(`/v2/prices/${currencyPair}/spot`);
    return result.data;
  }

  /**
   * Get buy price
   */
  async getBuyPrice(currencyPair: string = 'BTC-USD'): Promise<any> {
    const result = await this.query(`/v2/prices/${currencyPair}/buy`);
    return result.data;
  }

  /**
   * Get sell price
   */
  async getSellPrice(currencyPair: string = 'BTC-USD'): Promise<any> {
    const result = await this.query(`/v2/prices/${currencyPair}/sell`);
    return result.data;
  }

  /**
   * Get current time
   */
  async getTime(): Promise<any> {
    const result = await this.query('/v2/time');
    return result.data;
  }
}
