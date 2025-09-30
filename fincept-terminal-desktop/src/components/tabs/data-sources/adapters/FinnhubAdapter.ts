// Finnhub Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class FinnhubAdapter extends BaseAdapter {
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

      console.log('Testing Finnhub API connection...');

      try {
        // Test with a simple quote request
        const testSymbol = 'AAPL';
        const url = `https://finnhub.io/api/v1/quote?symbol=${testSymbol}&token=${apiKey}`;

        const response = await fetch(url);

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        // Check for API errors
        if (data.error) {
          throw new Error(data.error);
        }

        // Successful response - Finnhub returns current price data
        if (data.c !== undefined) {
          return this.createSuccessResult('Successfully connected to Finnhub API', {
            testSymbol,
            currentPrice: data.c,
            change: data.d,
            percentChange: data.dp,
            high: data.h,
            low: data.l,
            open: data.o,
            previousClose: data.pc,
            timestamp: data.t,
            note: 'API key validated successfully',
          });
        }

        // If no price data but no error, still consider it successful
        return this.createSuccessResult('Successfully connected to Finnhub API', {
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

      const queryParams = new URLSearchParams({ ...params, token: apiKey });
      const url = `https://finnhub.io/api/v1${endpoint}?${queryParams.toString()}`;

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
      throw new Error(`Finnhub query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'Finnhub',
      features: ['Stocks', 'Forex', 'Crypto', 'News', 'Company Info', 'Financial Statements'],
    };
  }

  /**
   * Get real-time quote for a symbol
   */
  async getQuote(symbol: string): Promise<any> {
    return await this.query('/quote', { symbol });
  }

  /**
   * Get candles/OHLC data
   */
  async getCandles(
    symbol: string,
    resolution: '1' | '5' | '15' | '30' | '60' | 'D' | 'W' | 'M',
    from: number,
    to: number
  ): Promise<any> {
    return await this.query('/stock/candle', {
      symbol,
      resolution,
      from,
      to,
    });
  }

  /**
   * Get company profile
   */
  async getCompanyProfile(symbol: string): Promise<any> {
    return await this.query('/stock/profile2', { symbol });
  }

  /**
   * Get company news
   */
  async getCompanyNews(symbol: string, from: string, to: string): Promise<any> {
    return await this.query('/company-news', { symbol, from, to });
  }

  /**
   * Get market news
   */
  async getMarketNews(category: string = 'general'): Promise<any> {
    return await this.query('/news', { category });
  }

  /**
   * Get peers/similar companies
   */
  async getPeers(symbol: string): Promise<any> {
    return await this.query('/stock/peers', { symbol });
  }

  /**
   * Get basic financials
   */
  async getBasicFinancials(symbol: string, metric: string = 'all'): Promise<any> {
    return await this.query('/stock/metric', { symbol, metric });
  }

  /**
   * Get recommendation trends
   */
  async getRecommendations(symbol: string): Promise<any> {
    return await this.query('/stock/recommendation', { symbol });
  }

  /**
   * Get price target
   */
  async getPriceTarget(symbol: string): Promise<any> {
    return await this.query('/stock/price-target', { symbol });
  }

  /**
   * Get earnings calendar
   */
  async getEarningsCalendar(from?: string, to?: string, symbol?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (from) params.from = from;
    if (to) params.to = to;
    if (symbol) params.symbol = symbol;

    return await this.query('/calendar/earnings', params);
  }

  /**
   * Get IPO calendar
   */
  async getIPOCalendar(from: string, to: string): Promise<any> {
    return await this.query('/calendar/ipo', { from, to });
  }

  /**
   * Symbol lookup/search
   */
  async searchSymbol(query: string): Promise<any> {
    return await this.query('/search', { q: query });
  }

  /**
   * Get stock symbols for an exchange
   */
  async getSymbols(exchange: string): Promise<any> {
    return await this.query('/stock/symbol', { exchange });
  }

  /**
   * Get forex rates
   */
  async getForexRates(base: string = 'USD'): Promise<any> {
    return await this.query('/forex/rates', { base });
  }

  /**
   * Get crypto exchanges
   */
  async getCryptoExchanges(): Promise<any> {
    return await this.query('/crypto/exchange', {});
  }

  /**
   * Get crypto symbols
   */
  async getCryptoSymbols(exchange: string): Promise<any> {
    return await this.query('/crypto/symbol', { exchange });
  }
}
