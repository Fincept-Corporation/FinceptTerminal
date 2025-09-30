// CoinMarketCap Crypto Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CoinMarketCapAdapter extends BaseAdapter {
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

      console.log('Testing CoinMarketCap API connection...');

      try {
        // Test with key info endpoint
        const response = await fetch('https://pro-api.coinmarketcap.com/v1/key/info', {
          method: 'GET',
          headers: {
            'X-CMC_PRO_API_KEY': apiKey,
            Accept: 'application/json',
          },
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        if (data.status && data.status.error_code !== 0) {
          throw new Error(data.status.error_message);
        }

        return this.createSuccessResult('Successfully connected to CoinMarketCap API', {
          plan: data.data?.plan || 'Unknown',
          credits: data.data?.usage?.current_day?.credits_used || 0,
          creditsRemaining: data.data?.usage?.current_day?.credits_left || 0,
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

      const queryParams = params ? `?${new URLSearchParams(params).toString()}` : '';
      const url = `https://pro-api.coinmarketcap.com${endpoint}${queryParams}`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'X-CMC_PRO_API_KEY': apiKey,
          Accept: 'application/json',
        },
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data.status && data.status.error_code !== 0) {
        throw new Error(data.status.error_message);
      }

      return data.data;
    } catch (error) {
      throw new Error(`CoinMarketCap query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'CoinMarketCap',
    };
  }

  /**
   * Get cryptocurrency listings
   */
  async getListings(start: number = 1, limit: number = 100, convert: string = 'USD'): Promise<any> {
    return await this.query('/v1/cryptocurrency/listings/latest', {
      start: start.toString(),
      limit: limit.toString(),
      convert,
    });
  }

  /**
   * Get cryptocurrency quotes
   */
  async getQuotes(symbols: string[], convert: string = 'USD'): Promise<any> {
    return await this.query('/v2/cryptocurrency/quotes/latest', {
      symbol: symbols.join(','),
      convert,
    });
  }

  /**
   * Get cryptocurrency info
   */
  async getCryptoInfo(symbols: string[]): Promise<any> {
    return await this.query('/v2/cryptocurrency/info', {
      symbol: symbols.join(','),
    });
  }

  /**
   * Get global metrics
   */
  async getGlobalMetrics(convert: string = 'USD'): Promise<any> {
    return await this.query('/v1/global-metrics/quotes/latest', { convert });
  }

  /**
   * Get trending cryptocurrencies
   */
  async getTrending(): Promise<any> {
    return await this.query('/v1/cryptocurrency/trending/latest');
  }

  /**
   * Get most visited
   */
  async getMostVisited(): Promise<any> {
    return await this.query('/v1/cryptocurrency/trending/most-visited');
  }

  /**
   * Get gainers and losers
   */
  async getGainersLosers(convert: string = 'USD', limit: number = 10): Promise<any> {
    return await this.query('/v1/cryptocurrency/trending/gainers-losers', {
      convert,
      limit: limit.toString(),
    });
  }

  /**
   * Get historical OHLCV
   */
  async getOHLCV(symbol: string, convert: string = 'USD', timeStart?: string, timeEnd?: string): Promise<any> {
    const params: Record<string, string> = { symbol, convert };
    if (timeStart) params.time_start = timeStart;
    if (timeEnd) params.time_end = timeEnd;

    return await this.query('/v2/cryptocurrency/ohlcv/historical', params);
  }

  /**
   * Search for cryptocurrencies
   */
  async search(query: string): Promise<any> {
    return await this.query('/v1/cryptocurrency/map', { symbol: query });
  }
}
