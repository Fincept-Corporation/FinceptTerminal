// Alpha Vantage Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class AlphaVantageAdapter extends BaseAdapter {
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

      console.log('Testing Alpha Vantage API connection...');

      try {
        // Test with a simple quote request
        const testSymbol = 'IBM';
        const url = `https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=${testSymbol}&apikey=${apiKey}`;

        const response = await fetch(url);

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        // Check for API errors
        if (data['Error Message']) {
          throw new Error(data['Error Message']);
        }

        if (data['Note']) {
          // API call frequency limit
          return this.createErrorResult(
            'API call frequency limit reached. Please wait a minute before testing again.'
          );
        }

        if (data['Information']) {
          // Premium endpoint or invalid key
          return this.createErrorResult(data['Information']);
        }

        // Successful response
        if (data['Global Quote']) {
          return this.createSuccessResult('Successfully connected to Alpha Vantage API', {
            testSymbol,
            price: data['Global Quote']['05. price'],
            timestamp: data['Global Quote']['07. latest trading day'],
            apiType: 'Free/Premium',
            note: 'API key validated successfully',
          });
        }

        throw new Error('Unexpected response format from Alpha Vantage API');
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

  async query(symbol: string, functionType: string = 'TIME_SERIES_DAILY'): Promise<any> {
    try {
      const apiKey = this.getConfig<string>('apiKey');
      const url = `https://www.alphavantage.co/query?function=${functionType}&symbol=${symbol}&apikey=${apiKey}`;

      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data['Error Message']) {
        throw new Error(data['Error Message']);
      }

      if (data['Note']) {
        throw new Error('API call frequency limit reached');
      }

      return data;
    } catch (error) {
      throw new Error(`Alpha Vantage query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'Alpha Vantage',
      features: ['Stocks', 'Forex', 'Crypto', 'Technical Indicators', 'Fundamentals'],
    };
  }

  /**
   * Get real-time quote for a symbol
   */
  async getQuote(symbol: string): Promise<any> {
    const apiKey = this.getConfig<string>('apiKey');
    const url = `https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=${symbol}&apikey=${apiKey}`;

    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`Failed to fetch quote for ${symbol}`);
    }

    return await response.json();
  }

  /**
   * Get intraday time series data
   */
  async getIntraday(symbol: string, interval: '1min' | '5min' | '15min' | '30min' | '60min' = '5min'): Promise<any> {
    return await this.query(symbol, `TIME_SERIES_INTRADAY&interval=${interval}`);
  }

  /**
   * Get daily time series data
   */
  async getDaily(symbol: string, outputSize: 'compact' | 'full' = 'compact'): Promise<any> {
    return await this.query(symbol, `TIME_SERIES_DAILY&outputsize=${outputSize}`);
  }

  /**
   * Get company overview (fundamental data)
   */
  async getCompanyOverview(symbol: string): Promise<any> {
    return await this.query(symbol, 'OVERVIEW');
  }

  /**
   * Search for symbols
   */
  async searchSymbol(keywords: string): Promise<any> {
    const apiKey = this.getConfig<string>('apiKey');
    const url = `https://www.alphavantage.co/query?function=SYMBOL_SEARCH&keywords=${keywords}&apikey=${apiKey}`;

    const response = await fetch(url);
    if (!response.ok) {
      throw new Error('Symbol search failed');
    }

    return await response.json();
  }
}
