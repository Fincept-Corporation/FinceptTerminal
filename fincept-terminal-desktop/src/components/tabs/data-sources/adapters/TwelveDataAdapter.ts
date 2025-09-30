// Twelve Data Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class TwelveDataAdapter extends BaseAdapter {
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

      console.log('Testing Twelve Data API connection...');

      try {
        // Test with time series endpoint
        const response = await fetch(`https://api.twelvedata.com/time_series?symbol=AAPL&interval=1min&apikey=${apiKey}&outputsize=1`);

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        if (data.status === 'error') {
          throw new Error(data.message || 'API request failed');
        }

        return this.createSuccessResult('Successfully connected to Twelve Data API', {
          symbol: data.meta?.symbol || 'AAPL',
          interval: data.meta?.interval || '1min',
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
      const url = `https://api.twelvedata.com${endpoint}?${queryParams.toString()}`;

      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data.status === 'error') {
        throw new Error(data.message || 'Query failed');
      }

      return data;
    } catch (error) {
      throw new Error(`Twelve Data query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'Twelve Data',
    };
  }

  /**
   * Get time series data
   */
  async getTimeSeries(symbol: string, interval: string = '1day', outputsize: number = 30): Promise<any> {
    return await this.query('/time_series', {
      symbol,
      interval,
      outputsize: outputsize.toString(),
    });
  }

  /**
   * Get quote
   */
  async getQuote(symbol: string): Promise<any> {
    return await this.query('/quote', { symbol });
  }

  /**
   * Get real-time price
   */
  async getPrice(symbol: string): Promise<any> {
    return await this.query('/price', { symbol });
  }

  /**
   * Get end of day price
   */
  async getEOD(symbol: string): Promise<any> {
    return await this.query('/eod', { symbol });
  }

  /**
   * Get exchange rate
   */
  async getExchangeRate(symbol: string): Promise<any> {
    return await this.query('/exchange_rate', { symbol });
  }

  /**
   * Get market state
   */
  async getMarketState(symbol: string): Promise<any> {
    return await this.query('/market_state', { symbol });
  }

  /**
   * Search symbols
   */
  async searchSymbol(symbol: string): Promise<any> {
    return await this.query('/symbol_search', { symbol });
  }

  /**
   * Get earliest timestamp
   */
  async getEarliestTimestamp(symbol: string, interval: string = '1day'): Promise<any> {
    return await this.query('/earliest_timestamp', { symbol, interval });
  }
}
