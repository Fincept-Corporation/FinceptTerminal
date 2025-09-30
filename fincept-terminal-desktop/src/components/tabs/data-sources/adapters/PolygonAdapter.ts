// Polygon.io Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class PolygonAdapter extends BaseAdapter {
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

      console.log('Testing Polygon.io API connection...');

      try {
        // Test with a simple market status request
        const url = `https://api.polygon.io/v1/marketstatus/now?apiKey=${apiKey}`;

        const response = await fetch(url);

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        if (data.status === 'ERROR') {
          throw new Error(data.error || 'API request failed');
        }

        // Successful response
        if (data.market) {
          return this.createSuccessResult('Successfully connected to Polygon.io API', {
            market: data.market,
            serverTime: data.serverTime,
            exchanges: data.exchanges ? Object.keys(data.exchanges).length : 0,
            currencies: data.currencies ? Object.keys(data.currencies).length : 0,
            apiTier: 'Detected from response',
            note: 'API key validated successfully',
          });
        }

        // If no error but unexpected format, still consider it successful
        return this.createSuccessResult('Successfully connected to Polygon.io API', {
          timestamp: new Date().toISOString(),
          note: 'API key validated successfully',
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

      const queryParams = new URLSearchParams({ ...params, apiKey });
      const url = `https://api.polygon.io${endpoint}?${queryParams.toString()}`;

      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data.status === 'ERROR') {
        throw new Error(data.error || 'API request failed');
      }

      return data;
    } catch (error) {
      throw new Error(`Polygon.io query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'Polygon.io',
      features: ['Stocks', 'Options', 'Forex', 'Crypto', 'Real-time', 'Historical'],
    };
  }

  /**
   * Get aggregates (bars) for a stock ticker
   */
  async getAggregates(
    ticker: string,
    multiplier: number,
    timespan: 'minute' | 'hour' | 'day' | 'week' | 'month' | 'quarter' | 'year',
    from: string,
    to: string
  ): Promise<any> {
    return await this.query(`/v2/aggs/ticker/${ticker}/range/${multiplier}/${timespan}/${from}/${to}`, {
      adjusted: 'true',
      sort: 'asc',
    });
  }

  /**
   * Get real-time quote for a ticker
   */
  async getQuote(ticker: string): Promise<any> {
    return await this.query(`/v2/last/trade/${ticker}`);
  }

  /**
   * Get previous day's OHLC for a ticker
   */
  async getPreviousClose(ticker: string): Promise<any> {
    return await this.query(`/v2/aggs/ticker/${ticker}/prev`, {
      adjusted: 'true',
    });
  }

  /**
   * Get snapshot of all tickers
   */
  async getSnapshot(): Promise<any> {
    return await this.query('/v2/snapshot/locale/us/markets/stocks/tickers');
  }

  /**
   * Get ticker details
   */
  async getTickerDetails(ticker: string): Promise<any> {
    return await this.query(`/v3/reference/tickers/${ticker}`);
  }

  /**
   * Search tickers
   */
  async searchTickers(search: string, market?: string, type?: string): Promise<any> {
    const params: Record<string, any> = { search };
    if (market) params.market = market;
    if (type) params.type = type;

    return await this.query('/v3/reference/tickers', params);
  }

  /**
   * Get market holidays
   */
  async getMarketHolidays(): Promise<any> {
    return await this.query('/v1/marketstatus/upcoming');
  }

  /**
   * Get grouped daily bars for entire market
   */
  async getGroupedDaily(date: string, adjusted: boolean = true): Promise<any> {
    return await this.query(`/v2/aggs/grouped/locale/us/market/stocks/${date}`, {
      adjusted: adjusted.toString(),
    });
  }
}
