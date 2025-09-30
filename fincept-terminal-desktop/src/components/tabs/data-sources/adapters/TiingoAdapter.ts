// Tiingo Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class TiingoAdapter extends BaseAdapter {
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

      console.log('Testing Tiingo API connection...');

      try {
        // Test with metadata endpoint
        const response = await fetch('https://api.tiingo.com/api/test', {
          method: 'GET',
          headers: {
            'Content-Type': 'application/json',
            Authorization: `Token ${apiKey}`,
          },
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Tiingo API', {
          message: data.message || 'API access verified',
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
      const url = `https://api.tiingo.com${endpoint}${queryParams}`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json',
          Authorization: `Token ${apiKey}`,
        },
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Tiingo query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'Tiingo',
    };
  }

  /**
   * Get stock metadata
   */
  async getStockMeta(ticker: string): Promise<any> {
    return await this.query(`/tiingo/daily/${ticker}`);
  }

  /**
   * Get end of day prices
   */
  async getEODPrices(ticker: string, startDate?: string, endDate?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (startDate) params.startDate = startDate;
    if (endDate) params.endDate = endDate;

    return await this.query(`/tiingo/daily/${ticker}/prices`, params);
  }

  /**
   * Get intraday prices (IEX)
   */
  async getIntradayPrices(ticker: string, startDate?: string, endDate?: string, resampleFreq?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (startDate) params.startDate = startDate;
    if (endDate) params.endDate = endDate;
    if (resampleFreq) params.resampleFreq = resampleFreq;

    return await this.query(`/iex/${ticker}/prices`, params);
  }

  /**
   * Get crypto prices
   */
  async getCryptoPrices(
    ticker: string,
    startDate?: string,
    endDate?: string,
    resampleFreq?: string
  ): Promise<any> {
    const params: Record<string, string> = {};
    if (startDate) params.startDate = startDate;
    if (endDate) params.endDate = endDate;
    if (resampleFreq) params.resampleFreq = resampleFreq;

    return await this.query(`/tiingo/crypto/prices`, { ...params, tickers: ticker });
  }

  /**
   * Get crypto metadata
   */
  async getCryptoMeta(): Promise<any> {
    return await this.query('/tiingo/crypto');
  }

  /**
   * Get forex prices
   */
  async getForexPrices(ticker: string, startDate?: string, endDate?: string, resampleFreq?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (startDate) params.startDate = startDate;
    if (endDate) params.endDate = endDate;
    if (resampleFreq) params.resampleFreq = resampleFreq;

    return await this.query(`/tiingo/fx/${ticker}/prices`, params);
  }

  /**
   * Get news
   */
  async getNews(tickers?: string[], tags?: string[], startDate?: string, endDate?: string, limit: number = 100): Promise<any> {
    const params: Record<string, string> = { limit: limit.toString() };
    if (tickers && tickers.length > 0) params.tickers = tickers.join(',');
    if (tags && tags.length > 0) params.tags = tags.join(',');
    if (startDate) params.startDate = startDate;
    if (endDate) params.endDate = endDate;

    return await this.query('/tiingo/news', params);
  }

  /**
   * Get fundamentals definitions
   */
  async getFundamentalsDefinitions(): Promise<any> {
    return await this.query('/tiingo/fundamentals/definitions');
  }

  /**
   * Get fundamentals statements
   */
  async getFundamentalsStatements(ticker: string, startDate?: string, endDate?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (startDate) params.startDate = startDate;
    if (endDate) params.endDate = endDate;

    return await this.query(`/tiingo/fundamentals/${ticker}/statements`, params);
  }

  /**
   * Get fundamentals daily
   */
  async getFundamentalsDaily(ticker: string, startDate?: string, endDate?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (startDate) params.startDate = startDate;
    if (endDate) params.endDate = endDate;

    return await this.query(`/tiingo/fundamentals/${ticker}/daily`, params);
  }
}
