// CoinGecko Crypto Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CoinGeckoAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const apiKey = this.getConfig<string>('apiKey');
      const isPro = this.getConfig<boolean>('isPro', false);

      console.log('Testing CoinGecko API connection...');

      try {
        // Test with ping endpoint
        const baseUrl = isPro ? 'https://pro-api.coingecko.com/api/v3' : 'https://api.coingecko.com/api/v3';

        const headers: Record<string, string> = {};
        if (apiKey && isPro) {
          headers['x-cg-pro-api-key'] = apiKey;
        }

        const response = await fetch(`${baseUrl}/ping`, {
          method: 'GET',
          headers,
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        if (data.gecko_says) {
          return this.createSuccessResult('Successfully connected to CoinGecko API', {
            message: data.gecko_says,
            apiType: isPro ? 'Pro' : 'Free',
            hasApiKey: !!apiKey,
            timestamp: new Date().toISOString(),
          });
        }

        return this.createSuccessResult('Successfully connected to CoinGecko API', {
          apiType: isPro ? 'Pro' : 'Free',
          hasApiKey: !!apiKey,
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
      const isPro = this.getConfig<boolean>('isPro', false);

      const baseUrl = isPro ? 'https://pro-api.coingecko.com/api/v3' : 'https://api.coingecko.com/api/v3';

      const headers: Record<string, string> = {};
      if (apiKey && isPro) {
        headers['x-cg-pro-api-key'] = apiKey;
      }

      const queryParams = params ? `?${new URLSearchParams(params).toString()}` : '';
      const url = `${baseUrl}${endpoint}${queryParams}`;

      const response = await fetch(url, {
        method: 'GET',
        headers,
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`CoinGecko query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      isPro: this.getConfig('isPro', false),
      provider: 'CoinGecko',
      features: ['Crypto Prices', 'Market Data', 'NFTs', 'Exchanges', 'Trending'],
    };
  }

  /**
   * Get coin price
   */
  async getCoinPrice(coinId: string, vsCurrencies: string = 'usd', includeMarketCap: boolean = false): Promise<any> {
    const params: Record<string, any> = {
      ids: coinId,
      vs_currencies: vsCurrencies,
    };

    if (includeMarketCap) {
      params.include_market_cap = 'true';
      params.include_24hr_vol = 'true';
      params.include_24hr_change = 'true';
    }

    return await this.query('/simple/price', params);
  }

  /**
   * Get coin list
   */
  async getCoinList(): Promise<any> {
    return await this.query('/coins/list');
  }

  /**
   * Get coin markets
   */
  async getCoinMarkets(vsCurrency: string = 'usd', page: number = 1, perPage: number = 100): Promise<any> {
    return await this.query('/coins/markets', {
      vs_currency: vsCurrency,
      page: page.toString(),
      per_page: perPage.toString(),
    });
  }

  /**
   * Get coin details
   */
  async getCoinDetails(coinId: string): Promise<any> {
    return await this.query(`/coins/${coinId}`, {
      localization: 'false',
      tickers: 'false',
      market_data: 'true',
      community_data: 'true',
      developer_data: 'true',
    });
  }

  /**
   * Get coin market chart
   */
  async getCoinMarketChart(coinId: string, vsCurrency: string = 'usd', days: number = 7): Promise<any> {
    return await this.query(`/coins/${coinId}/market_chart`, {
      vs_currency: vsCurrency,
      days: days.toString(),
    });
  }

  /**
   * Get trending coins
   */
  async getTrending(): Promise<any> {
    return await this.query('/search/trending');
  }

  /**
   * Get global crypto data
   */
  async getGlobal(): Promise<any> {
    return await this.query('/global');
  }

  /**
   * Get exchanges list
   */
  async getExchanges(page: number = 1, perPage: number = 100): Promise<any> {
    return await this.query('/exchanges', {
      page: page.toString(),
      per_page: perPage.toString(),
    });
  }

  /**
   * Get exchange details
   */
  async getExchangeDetails(exchangeId: string): Promise<any> {
    return await this.query(`/exchanges/${exchangeId}`);
  }

  /**
   * Search for coins, exchanges, categories
   */
  async search(query: string): Promise<any> {
    return await this.query('/search', { query });
  }

  /**
   * Get supported currencies
   */
  async getSupportedCurrencies(): Promise<string[]> {
    return await this.query('/simple/supported_vs_currencies');
  }

  /**
   * Get coin OHLC data
   */
  async getCoinOHLC(coinId: string, vsCurrency: string = 'usd', days: number = 7): Promise<any> {
    return await this.query(`/coins/${coinId}/ohlc`, {
      vs_currency: vsCurrency,
      days: days.toString(),
    });
  }

  /**
   * Get categories
   */
  async getCategories(): Promise<any> {
    return await this.query('/coins/categories');
  }

  /**
   * Get NFT list
   */
  async getNFTList(): Promise<any> {
    return await this.query('/nfts/list');
  }
}
