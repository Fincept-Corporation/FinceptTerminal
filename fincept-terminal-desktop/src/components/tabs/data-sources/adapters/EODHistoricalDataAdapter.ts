// EOD Historical Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class EODHistoricalDataAdapter extends BaseAdapter {
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

      console.log('Testing EOD Historical Data API connection...');

      try {
        // Test with exchanges list endpoint
        const response = await fetch(`https://eodhistoricaldata.com/api/exchanges-list/?api_token=${apiKey}&fmt=json`);

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        if (data.error) {
          throw new Error(data.error);
        }

        return this.createSuccessResult('Successfully connected to EOD Historical Data API', {
          exchangesAvailable: Array.isArray(data) ? data.length : 'Unknown',
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

      const queryParams = new URLSearchParams({ ...params, api_token: apiKey, fmt: 'json' });
      const url = `https://eodhistoricaldata.com/api${endpoint}?${queryParams.toString()}`;

      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data.error) {
        throw new Error(data.error);
      }

      return data;
    } catch (error) {
      throw new Error(`EOD Historical Data query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'EOD Historical Data',
    };
  }

  /**
   * Get end of day data
   */
  async getEOD(symbol: string, exchange: string = 'US', from?: string, to?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (from) params.from = from;
    if (to) params.to = to;

    return await this.query(`/eod/${symbol}.${exchange}`, params);
  }

  /**
   * Get real-time quote
   */
  async getRealTimeQuote(symbol: string, exchange: string = 'US'): Promise<any> {
    return await this.query(`/real-time/${symbol}.${exchange}`);
  }

  /**
   * Get intraday data
   */
  async getIntraday(symbol: string, exchange: string = 'US', interval: string = '5m', from?: string, to?: string): Promise<any> {
    const params: Record<string, string> = { interval };
    if (from) params.from = from;
    if (to) params.to = to;

    return await this.query(`/intraday/${symbol}.${exchange}`, params);
  }

  /**
   * Get fundamentals
   */
  async getFundamentals(symbol: string, exchange: string = 'US'): Promise<any> {
    return await this.query(`/fundamentals/${symbol}.${exchange}`);
  }

  /**
   * Get dividends
   */
  async getDividends(symbol: string, exchange: string = 'US', from?: string, to?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (from) params.from = from;
    if (to) params.to = to;

    return await this.query(`/div/${symbol}.${exchange}`, params);
  }

  /**
   * Get splits
   */
  async getSplits(symbol: string, exchange: string = 'US', from?: string, to?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (from) params.from = from;
    if (to) params.to = to;

    return await this.query(`/splits/${symbol}.${exchange}`, params);
  }

  /**
   * List exchanges
   */
  async listExchanges(): Promise<any> {
    return await this.query('/exchanges-list/');
  }

  /**
   * Get exchange symbols
   */
  async getExchangeSymbols(exchange: string): Promise<any> {
    return await this.query(`/exchange-symbol-list/${exchange}`);
  }

  /**
   * Search symbols
   */
  async searchSymbols(query: string): Promise<any> {
    return await this.query('/search/', { s: query });
  }

  /**
   * Get calendar data
   */
  async getCalendar(type: 'earnings' | 'trends' | 'ipos' | 'splits', symbols?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (symbols) params.symbols = symbols;

    return await this.query(`/calendar/${type}`, params);
  }
}
