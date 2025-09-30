// Yahoo Finance API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class YahooFinanceAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const apiKey = this.getConfig<string>('apiKey');
      const useRapidAPI = this.getConfig<boolean>('useRapidAPI', false);
      const testSymbol = this.getConfig<string>('testSymbol', 'AAPL');

      // Yahoo Finance has a public unofficial API, but RapidAPI provides a stable version
      const baseUrl = useRapidAPI
        ? 'https://yahoo-finance15.p.rapidapi.com'
        : 'https://query1.finance.yahoo.com/v8/finance/chart';

      try {
        const headers: Record<string, string> = {
          'Content-Type': 'application/json',
        };

        if (useRapidAPI && apiKey) {
          headers['X-RapidAPI-Key'] = apiKey;
          headers['X-RapidAPI-Host'] = 'yahoo-finance15.p.rapidapi.com';
        }

        const testUrl = useRapidAPI
          ? `${baseUrl}/api/v1/markets/quote?ticker=${testSymbol}`
          : `${baseUrl}/${testSymbol}?interval=1d&range=1d`;

        const response = await fetch(testUrl, {
          method: 'GET',
          headers,
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Authentication failed. Check your API key.');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        // Validate response structure
        const hasValidData = useRapidAPI
          ? data && data.body
          : data && data.chart && data.chart.result;

        if (!hasValidData) {
          throw new Error('Invalid response structure from Yahoo Finance API');
        }

        return this.createSuccessResult('Successfully connected to Yahoo Finance API', {
          apiType: useRapidAPI ? 'RapidAPI' : 'Public API',
          testSymbol,
          authenticated: !!apiKey,
          timestamp: new Date().toISOString(),
          rateLimit: response.headers.get('x-ratelimit-limit'),
          rateLimitRemaining: response.headers.get('x-ratelimit-remaining'),
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

  async query(symbol: string, options?: {
    interval?: string;
    range?: string;
    indicators?: string[];
  }): Promise<any> {
    try {
      const apiKey = this.getConfig<string>('apiKey');
      const useRapidAPI = this.getConfig<boolean>('useRapidAPI', false);

      const interval = options?.interval || '1d';
      const range = options?.range || '1mo';

      const headers: Record<string, string> = {
        'Content-Type': 'application/json',
      };

      if (useRapidAPI && apiKey) {
        headers['X-RapidAPI-Key'] = apiKey;
        headers['X-RapidAPI-Host'] = 'yahoo-finance15.p.rapidapi.com';
      }

      const baseUrl = useRapidAPI
        ? 'https://yahoo-finance15.p.rapidapi.com'
        : 'https://query1.finance.yahoo.com/v8/finance/chart';

      const url = useRapidAPI
        ? `${baseUrl}/api/v1/markets/stock/history?symbol=${symbol}&interval=${interval}&range=${range}`
        : `${baseUrl}/${symbol}?interval=${interval}&range=${range}`;

      const response = await fetch(url, {
        method: 'GET',
        headers,
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Yahoo Finance query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      useRapidAPI: this.getConfig('useRapidAPI', false),
      hasApiKey: !!this.getConfig('apiKey'),
      testSymbol: this.getConfig('testSymbol', 'AAPL'),
    };
  }

  /**
   * Get real-time quote for a symbol
   */
  async getQuote(symbol: string): Promise<any> {
    const useRapidAPI = this.getConfig<boolean>('useRapidAPI', false);
    const apiKey = this.getConfig<string>('apiKey');

    const headers: Record<string, string> = {};
    if (useRapidAPI && apiKey) {
      headers['X-RapidAPI-Key'] = apiKey;
      headers['X-RapidAPI-Host'] = 'yahoo-finance15.p.rapidapi.com';
    }

    const url = useRapidAPI
      ? `https://yahoo-finance15.p.rapidapi.com/api/v1/markets/quote?ticker=${symbol}`
      : `https://query1.finance.yahoo.com/v8/finance/chart/${symbol}?interval=1d&range=1d`;

    const response = await fetch(url, { headers });

    if (!response.ok) {
      throw new Error(`Failed to fetch quote for ${symbol}`);
    }

    return await response.json();
  }
}
