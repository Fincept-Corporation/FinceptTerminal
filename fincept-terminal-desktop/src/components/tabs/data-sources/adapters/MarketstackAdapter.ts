// Marketstack Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class MarketstackAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const accessKey = this.getConfig<string>('accessKey');

      if (!accessKey) {
        return this.createErrorResult('Access key is required');
      }

      console.log('Testing Marketstack API connection...');

      try {
        // Test with exchanges endpoint
        const response = await fetch(`http://api.marketstack.com/v1/exchanges?access_key=${accessKey}&limit=1`);

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid access key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        if (data.error) {
          throw new Error(data.error.message || 'API request failed');
        }

        return this.createSuccessResult('Successfully connected to Marketstack API', {
          exchangesAvailable: data.pagination?.total || 'Unknown',
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
      const accessKey = this.getConfig<string>('accessKey');

      const queryParams = new URLSearchParams({ ...params, access_key: accessKey });
      const url = `http://api.marketstack.com/v1${endpoint}?${queryParams.toString()}`;

      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data.error) {
        throw new Error(data.error.message || 'Query failed');
      }

      return data;
    } catch (error) {
      throw new Error(`Marketstack query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasAccessKey: !!this.getConfig('accessKey'),
      provider: 'Marketstack',
    };
  }

  /**
   * Get end of day data
   */
  async getEOD(symbols: string | string[], dateFrom?: string, dateTo?: string, limit: number = 100): Promise<any> {
    const symbolsStr = Array.isArray(symbols) ? symbols.join(',') : symbols;
    const params: Record<string, string> = { symbols: symbolsStr, limit: limit.toString() };
    if (dateFrom) params.date_from = dateFrom;
    if (dateTo) params.date_to = dateTo;

    return await this.query('/eod', params);
  }

  /**
   * Get latest EOD data
   */
  async getEODLatest(symbols: string | string[]): Promise<any> {
    const symbolsStr = Array.isArray(symbols) ? symbols.join(',') : symbols;
    return await this.query('/eod/latest', { symbols: symbolsStr });
  }

  /**
   * Get intraday data
   */
  async getIntraday(
    symbols: string | string[],
    interval?: string,
    dateFrom?: string,
    dateTo?: string,
    limit: number = 100
  ): Promise<any> {
    const symbolsStr = Array.isArray(symbols) ? symbols.join(',') : symbols;
    const params: Record<string, string> = { symbols: symbolsStr, limit: limit.toString() };
    if (interval) params.interval = interval;
    if (dateFrom) params.date_from = dateFrom;
    if (dateTo) params.date_to = dateTo;

    return await this.query('/intraday', params);
  }

  /**
   * Get tickers
   */
  async getTickers(exchange?: string, limit: number = 100): Promise<any> {
    const params: Record<string, string> = { limit: limit.toString() };
    if (exchange) params.exchange = exchange;

    return await this.query('/tickers', params);
  }

  /**
   * Get exchanges
   */
  async getExchanges(limit: number = 100): Promise<any> {
    return await this.query('/exchanges', { limit: limit.toString() });
  }

  /**
   * Get currencies
   */
  async getCurrencies(limit: number = 100): Promise<any> {
    return await this.query('/currencies', { limit: limit.toString() });
  }

  /**
   * Get timezones
   */
  async getTimezones(): Promise<any> {
    return await this.query('/timezones');
  }

  /**
   * Get splits
   */
  async getSplits(symbols?: string | string[], dateFrom?: string, dateTo?: string, limit: number = 100): Promise<any> {
    const params: Record<string, string> = { limit: limit.toString() };
    if (symbols) {
      const symbolsStr = Array.isArray(symbols) ? symbols.join(',') : symbols;
      params.symbols = symbolsStr;
    }
    if (dateFrom) params.date_from = dateFrom;
    if (dateTo) params.date_to = dateTo;

    return await this.query('/splits', params);
  }

  /**
   * Get dividends
   */
  async getDividends(symbols?: string | string[], dateFrom?: string, dateTo?: string, limit: number = 100): Promise<any> {
    const params: Record<string, string> = { limit: limit.toString() };
    if (symbols) {
      const symbolsStr = Array.isArray(symbols) ? symbols.join(',') : symbols;
      params.symbols = symbolsStr;
    }
    if (dateFrom) params.date_from = dateFrom;
    if (dateTo) params.date_to = dateTo;

    return await this.query('/dividends', params);
  }
}
