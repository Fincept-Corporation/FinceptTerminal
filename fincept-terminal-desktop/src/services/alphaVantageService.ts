import { invoke } from '@tauri-apps/api/core';

// Alpha Vantage Service Layer - Fault-Tolerant Modular Design
// Each endpoint works independently with comprehensive error handling
class AlphaVantageService {
  private apiKey: string = '';

  setApiKey(key: string) {
    this.apiKey = key;
    localStorage.setItem('alphavantage_api_key', key);
  }

  getApiKey(): string {
    if (!this.apiKey) {
      this.apiKey = localStorage.getItem('alphavantage_api_key') || 'demo';
    }
    return this.apiKey;
  }

  private async executeCommand(command: string, args: string[]): Promise<any> {
    try {
      const result = await invoke<string>('execute_alphavantage_command', { command, args });
      return JSON.parse(result);
    } catch (error) {
      console.error('Alpha Vantage API error:', error);
      throw error;
    }
  }

  // ==================== MARKET DATA ENDPOINTS ====================

  /**
   * Get real-time quote for a symbol
   * Fault tolerant: Returns error structure if API call fails
   */
  async getQuote(symbol: string): Promise<any> {
    try {
      const result = await invoke<string>('get_alphavantage_quote', { symbol });
      return JSON.parse(result);
    } catch (error) {
      console.error(`Failed to get quote for ${symbol}:`, error);
      return {
        success: false,
        error: `Failed to fetch quote: ${error}`,
        endpoint: 'quote',
        symbol,
        timestamp: Date.now()
      };
    }
  }

  /**
   * Get daily historical data
   * @param symbol Stock symbol
   * @param outputsize 'compact' (last 100 days) or 'full' (all available data)
   */
  async getDailyData(symbol: string, outputsize: 'compact' | 'full' = 'compact'): Promise<any> {
    try {
      const result = await invoke<string>('get_alphavantage_daily', { symbol, outputsize });
      return JSON.parse(result);
    } catch (error) {
      console.error(`Failed to get daily data for ${symbol}:`, error);
      return {
        success: false,
        error: `Failed to fetch daily data: ${error}`,
        endpoint: 'daily',
        symbol,
        outputsize,
        timestamp: Date.now()
      };
    }
  }

  /**
   * Get intraday time series data
   * @param symbol Stock symbol
   * @param interval Time interval: '1min', '5min', '15min', '30min', '60min'
   */
  async getIntradayData(
    symbol: string,
    interval: '1min' | '5min' | '15min' | '30min' | '60min' = '5min'
  ): Promise<any> {
    try {
      const result = await invoke<string>('get_alphavantage_intraday', { symbol, interval });
      return JSON.parse(result);
    } catch (error) {
      console.error(`Failed to get intraday data for ${symbol}:`, error);
      return {
        success: false,
        error: `Failed to fetch intraday data: ${error}`,
        endpoint: 'intraday',
        symbol,
        interval,
        timestamp: Date.now()
      };
    }
  }

  // ==================== FUNDAMENTAL DATA ENDPOINTS ====================

  /**
   * Get company overview and fundamental data
   * Returns comprehensive company information
   */
  async getCompanyOverview(symbol: string): Promise<any> {
    try {
      const result = await invoke<string>('get_alphavantage_overview', { symbol });
      return JSON.parse(result);
    } catch (error) {
      console.error(`Failed to get company overview for ${symbol}:`, error);
      return {
        success: false,
        error: `Failed to fetch company overview: ${error}`,
        endpoint: 'overview',
        symbol,
        timestamp: Date.now()
      };
    }
  }

  // ==================== SEARCH ENDPOINTS ====================

  /**
   * Search for symbols and companies
   * @param keywords Search terms
   */
  async searchSymbols(keywords: string): Promise<any> {
    try {
      const result = await invoke<string>('search_alphavantage_symbols', { keywords });
      return JSON.parse(result);
    } catch (error) {
      console.error(`Failed to search symbols for ${keywords}:`, error);
      return {
        success: false,
        error: `Failed to search symbols: ${error}`,
        endpoint: 'search',
        keywords,
        timestamp: Date.now()
      };
    }
  }

  // ==================== COMPOSITE ENDPOINTS ====================

  /**
   * Get comprehensive data from multiple endpoints
   * Fault tolerant: Partial failures return available data
   */
  async getComprehensiveData(symbol: string): Promise<any> {
    try {
      const result = await invoke<string>('get_alphavantage_comprehensive', { symbol });
      return JSON.parse(result);
    } catch (error) {
      console.error(`Failed to get comprehensive data for ${symbol}:`, error);
      return {
        success: false,
        error: `Failed to fetch comprehensive data: ${error}`,
        symbol,
        timestamp: Date.now(),
        endpoints: {},
        failed_endpoints: [
          { endpoint: 'all', error: `System error: ${error}` }
        ]
      };
    }
  }

  /**
   * Get market movers (popular stocks with their current data)
   * Returns a curated list of actively traded stocks
   */
  async getMarketMovers(): Promise<any> {
    try {
      const result = await invoke<string>('get_alphavantage_market_movers');
      return JSON.parse(result);
    } catch (error) {
      console.error('Failed to get market movers:', error);
      return {
        success: false,
        error: `Failed to fetch market movers: ${error}`,
        endpoint: 'market_movers',
        timestamp: Date.now(),
        movers: [],
        failed_symbols: []
      };
    }
  }

  // ==================== UTILITY METHODS ====================

  /**
   * Batch quote requests for multiple symbols
   * Processes quotes in parallel with error isolation
   */
  async getBatchQuotes(symbols: string[]): Promise<any[]> {
    const promises = symbols.map(symbol =>
      this.getQuote(symbol).catch(error => ({
        success: false,
        error: error.message || 'Unknown error',
        symbol,
        endpoint: 'quote',
        timestamp: Date.now()
      }))
    );

    const results = await Promise.allSettled(promises);
    return results.map(result =>
      result.status === 'fulfilled' ? result.value : {
        success: false,
        error: 'Promise rejected',
        endpoint: 'quote',
        timestamp: Date.now()
      }
    );
  }

  /**
   * Get multiple data types for a symbol with fault tolerance
   * @param symbol Stock symbol
   * @param dataTypes Array of data types to fetch: ['quote', 'overview', 'daily', 'intraday']
   */
  async getMultiDataTypeData(
    symbol: string,
    dataTypes: string[] = ['quote', 'overview', 'daily']
  ): Promise<any> {
    const results: any = {
      symbol,
      timestamp: Date.now(),
      data: {},
      failed_requests: []
    };

    const requests: Promise<any>[] = [];

    if (dataTypes.includes('quote')) {
      requests.push(
        this.getQuote(symbol).then(data => ({ type: 'quote', data }))
      );
    }

    if (dataTypes.includes('overview')) {
      requests.push(
        this.getCompanyOverview(symbol).then(data => ({ type: 'overview', data }))
      );
    }

    if (dataTypes.includes('daily')) {
      requests.push(
        this.getDailyData(symbol).then(data => ({ type: 'daily', data }))
      );
    }

    if (dataTypes.includes('intraday')) {
      requests.push(
        this.getIntradayData(symbol).then(data => ({ type: 'intraday', data }))
      );
    }

    const settledResults = await Promise.allSettled(requests);

    settledResults.forEach((result, index) => {
      if (result.status === 'fulfilled' && result.value.data.success) {
        results.data[result.value.type] = result.value.data;
      } else {
        const dataType = result.status === 'fulfilled' ? result.value.type : 'unknown';
        results.failed_requests.push({
          type: dataType,
          error: result.status === 'rejected' ?
            result.reason :
            (result.value.data?.error || 'Unknown error')
        });
      }
    });

    results.success = Object.keys(results.data).length > 0;
    return results;
  }

  /**
   * Health check for Alpha Vantage API connectivity
   */
  async healthCheck(): Promise<any> {
    try {
      // Try to get a quote for a known symbol (AAPL)
      const testResult = await this.getQuote('AAPL');

      return {
        success: testResult.success,
        message: testResult.success ?
          'Alpha Vantage API is working' :
          `Alpha Vantage API error: ${testResult.error}`,
        timestamp: Date.now(),
        api_key_configured: this.getApiKey() !== 'demo'
      };
    } catch (error) {
      return {
        success: false,
        message: `Health check failed: ${error}`,
        timestamp: Date.now(),
        api_key_configured: this.getApiKey() !== 'demo'
      };
    }
  }

  /**
   * Check API rate limit status
   */
  async checkRateLimit(): Promise<any> {
    // This is a placeholder since Alpha Vantage doesn't have a dedicated rate limit endpoint
    // We can infer rate limiting from error messages in real usage
    return {
      success: true,
      message: 'Rate limit monitoring is available through error responses',
      limits: {
        free_tier: '5 calls per minute',
        premium_tier: 'Varies by subscription'
      },
      timestamp: Date.now()
    };
  }
}

export const alphaVantageService = new AlphaVantageService();