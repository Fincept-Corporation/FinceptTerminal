// IEX Cloud Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class IEXCloudAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const token = this.getConfig<string>('token');
      const sandbox = this.getConfig<boolean>('sandbox', false);

      if (!token) {
        return this.createErrorResult('API token is required');
      }

      console.log('Testing IEX Cloud API connection...');

      try {
        const baseUrl = sandbox ? 'https://sandbox.iexapis.com/stable' : 'https://cloud.iexapis.com/stable';

        // Test with status endpoint
        const response = await fetch(`${baseUrl}/status?token=${token}`);

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API token or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to IEX Cloud API', {
          status: data.status,
          callsTracking: data.currentMonthAPICalls ? 'API calls tracked' : 'Unknown',
          sandbox,
          apiCallsUsed: data.currentMonthAPICalls || 0,
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
      const token = this.getConfig<string>('token');
      const sandbox = this.getConfig<boolean>('sandbox', false);

      const baseUrl = sandbox ? 'https://sandbox.iexapis.com/stable' : 'https://cloud.iexapis.com/stable';

      const queryParams = new URLSearchParams({ ...params, token });
      const url = `${baseUrl}${endpoint}?${queryParams.toString()}`;

      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`IEX Cloud query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasToken: !!this.getConfig('token'),
      sandbox: this.getConfig('sandbox', false),
      provider: 'IEX Cloud',
    };
  }

  /**
   * Get quote
   */
  async getQuote(symbol: string): Promise<any> {
    return await this.query(`/stock/${symbol}/quote`);
  }

  /**
   * Get company info
   */
  async getCompany(symbol: string): Promise<any> {
    return await this.query(`/stock/${symbol}/company`);
  }

  /**
   * Get historical prices
   */
  async getHistoricalPrices(symbol: string, range: string = '1m'): Promise<any> {
    return await this.query(`/stock/${symbol}/chart/${range}`);
  }

  /**
   * Get news
   */
  async getNews(symbol: string, last: number = 10): Promise<any> {
    return await this.query(`/stock/${symbol}/news/last/${last}`);
  }

  /**
   * Get financials
   */
  async getFinancials(symbol: string): Promise<any> {
    return await this.query(`/stock/${symbol}/financials`);
  }

  /**
   * Get earnings
   */
  async getEarnings(symbol: string): Promise<any> {
    return await this.query(`/stock/${symbol}/earnings`);
  }

  /**
   * Get dividends
   */
  async getDividends(symbol: string, range: string = '1y'): Promise<any> {
    return await this.query(`/stock/${symbol}/dividends/${range}`);
  }

  /**
   * Get stats
   */
  async getStats(symbol: string): Promise<any> {
    return await this.query(`/stock/${symbol}/stats`);
  }

  /**
   * Search symbols
   */
  async searchSymbols(fragment: string): Promise<any> {
    return await this.query('/search', { fragment });
  }

  /**
   * Get market volume
   */
  async getMarketVolume(): Promise<any> {
    return await this.query('/market');
  }
}
