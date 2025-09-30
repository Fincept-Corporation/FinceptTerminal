// Tradier Brokerage and Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class TradierAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const accessToken = this.getConfig<string>('accessToken');
      const sandbox = this.getConfig<boolean>('sandbox', false);

      if (!accessToken) {
        return this.createErrorResult('Access token is required');
      }

      console.log('Testing Tradier API connection...');

      try {
        const baseUrl = sandbox ? 'https://sandbox.tradier.com' : 'https://api.tradier.com';

        // Test with user profile endpoint
        const response = await fetch(`${baseUrl}/v1/user/profile`, {
          method: 'GET',
          headers: {
            Authorization: `Bearer ${accessToken}`,
            Accept: 'application/json',
          },
        });

        if (!response.ok) {
          if (response.status === 401) {
            throw new Error('Invalid access token');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Tradier API', {
          accountId: data.profile?.account?.account_number || 'Unknown',
          sandbox,
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
      const accessToken = this.getConfig<string>('accessToken');
      const sandbox = this.getConfig<boolean>('sandbox', false);

      const baseUrl = sandbox ? 'https://sandbox.tradier.com' : 'https://api.tradier.com';
      const queryParams = params ? `?${new URLSearchParams(params).toString()}` : '';
      const url = `${baseUrl}${endpoint}${queryParams}`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          Authorization: `Bearer ${accessToken}`,
          Accept: 'application/json',
        },
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Tradier query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasAccessToken: !!this.getConfig('accessToken'),
      sandbox: this.getConfig('sandbox', false),
      provider: 'Tradier',
    };
  }

  /**
   * Get user profile
   */
  async getUserProfile(): Promise<any> {
    return await this.query('/v1/user/profile');
  }

  /**
   * Get quotes
   */
  async getQuotes(symbols: string[]): Promise<any> {
    return await this.query('/v1/markets/quotes', { symbols: symbols.join(',') });
  }

  /**
   * Get option chains
   */
  async getOptionChains(symbol: string, expiration?: string): Promise<any> {
    const params: Record<string, string> = { symbol };
    if (expiration) params.expiration = expiration;
    return await this.query('/v1/markets/options/chains', params);
  }

  /**
   * Get historical prices
   */
  async getHistoricalPrices(symbol: string, interval?: string, start?: string, end?: string): Promise<any> {
    const params: Record<string, string> = { symbol };
    if (interval) params.interval = interval;
    if (start) params.start = start;
    if (end) params.end = end;
    return await this.query('/v1/markets/history', params);
  }

  /**
   * Get market calendar
   */
  async getMarketCalendar(month?: number, year?: number): Promise<any> {
    const params: Record<string, string> = {};
    if (month) params.month = month.toString();
    if (year) params.year = year.toString();
    return await this.query('/v1/markets/calendar', params);
  }
}
