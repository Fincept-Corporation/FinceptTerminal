// Finage Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class FinageAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const apiKey = this.getConfig<string>('apiKey');

      if (!apiKey) {
        return this.createErrorResult('API key is required');
      }

      console.log('Testing Finage API connection...');

      try {
        // Test with stock list endpoint
        const response = await fetch(`https://api.finage.co.uk/agg/stock/list?apikey=${apiKey}&limit=1`);

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Finage API', {
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
      const apiKey = this.getConfig<string>('apiKey');

      const queryParams = new URLSearchParams({ ...params, apikey: apiKey });
      const url = `https://api.finage.co.uk${endpoint}?${queryParams.toString()}`;

      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Finage query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'Finage',
    };
  }

  /**
   * Get stock quote
   */
  async getStockQuote(symbol: string): Promise<any> {
    return await this.query(`/last/stock/${symbol}`);
  }

  /**
   * Get stock history
   */
  async getStockHistory(symbol: string, period: string = '1d'): Promise<any> {
    return await this.query(`/agg/stock/${period}/${symbol}`);
  }

  /**
   * Get forex rates
   */
  async getForexRate(symbol: string): Promise<any> {
    return await this.query(`/last/forex/${symbol}`);
  }

  /**
   * Get crypto price
   */
  async getCryptoPrice(symbol: string): Promise<any> {
    return await this.query(`/last/crypto/${symbol}`);
  }

  /**
   * Get market news
   */
  async getNews(limit: number = 10): Promise<any> {
    return await this.query('/news/market', { limit: limit.toString() });
  }
}
