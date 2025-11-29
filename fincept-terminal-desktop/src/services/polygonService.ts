import { invoke } from '@tauri-apps/api/core';

import { sqliteService } from './sqliteService';

// Polygon.io Service Layer - Comprehensive Coverage of 31 Endpoints
class PolygonService {
  private apiKey: string = '';

  async loadApiKey(): Promise<string> {
    try {
      console.log('[PolygonService] Loading API key from database...');
      const credentials = await sqliteService.getCredentials();
      console.log('[PolygonService] Found credentials:', credentials.map(c => c.service_name));

      const polygonCred = credentials.find(c => c.service_name.toLowerCase() === 'polygon.io');
      console.log('[PolygonService] Polygon credential found:', !!polygonCred);

      if (polygonCred?.api_key) {
        this.apiKey = polygonCred.api_key;
        // Set environment variable for Python script
        localStorage.setItem('POLYGON_API_KEY', polygonCred.api_key);
        console.log('[PolygonService] API key loaded successfully, length:', polygonCred.api_key.length);
        return polygonCred.api_key;
      }
      console.warn('[PolygonService] No Polygon.io API key found in credentials');
    } catch (error) {
      console.error('[PolygonService] Failed to load Polygon API key from database:', error);
    }
    return '';
  }

  getApiKey(): string {
    return this.apiKey;
  }

  private async executeCommand(command: string, args: string[]): Promise<any> {
    try {
      // Ensure API key is loaded
      if (!this.apiKey) {
        console.log('[PolygonService] API key not set, loading...');
        await this.loadApiKey();
      }

      console.log('[PolygonService] Executing command:', command, 'with args:', args);
      console.log('[PolygonService] API key present:', !!this.apiKey);

      const result = await invoke<string>('execute_polygon_command', {
        command,
        args,
        apiKey: this.apiKey || null
      });

      console.log('[PolygonService] Command result received, length:', result.length);
      const parsed = JSON.parse(result);
      console.log('[PolygonService] Parsed result success:', parsed.success);

      return parsed;
    } catch (error) {
      console.error('[PolygonService] Polygon API error:', error);
      throw error;
    }
  }

  // ==================== REFERENCE DATA (11 endpoints) ====================

  async getAllTickers(options?: any): Promise<any> {
    const args: string[] = [];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('tickers', args);
  }

  async getTickerDetails(ticker: string, date?: string): Promise<any> {
    const args = [`--ticker=${ticker}`];
    if (date) args.push(`--date=${date}`);
    return this.executeCommand('ticker-details', args);
  }

  async getTickerTypes(assetClass?: string, locale?: string): Promise<any> {
    const args: string[] = [];
    if (assetClass) args.push(`--asset-class=${assetClass}`);
    if (locale) args.push(`--locale=${locale}`);
    return this.executeCommand('ticker-types', args);
  }

  async getRelatedTickers(ticker: string): Promise<any> {
    return this.executeCommand('related-tickers', [`--ticker=${ticker}`]);
  }

  async getNews(options?: any): Promise<any> {
    const args: string[] = [];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('news', args);
  }

  async getIPOs(options?: any): Promise<any> {
    const args: string[] = [];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('ipos', args);
  }

  async getSplits(options?: any): Promise<any> {
    const args: string[] = [];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('splits', args);
  }

  async getDividends(options?: any): Promise<any> {
    const args: string[] = [];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('dividends', args);
  }

  async getTickerEvents(identifier: string, types?: string): Promise<any> {
    const args = [`--identifier=${identifier}`];
    if (types) args.push(`--types=${types}`);
    return this.executeCommand('ticker-events', args);
  }

  async getExchanges(assetClass?: string, locale?: string): Promise<any> {
    const args: string[] = [];
    if (assetClass) args.push(`--asset-class=${assetClass}`);
    if (locale) args.push(`--locale=${locale}`);
    return this.executeCommand('exchanges', args);
  }

  async getConditionCodes(options?: any): Promise<any> {
    const args: string[] = [];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('condition-codes', args);
  }

  // ==================== MARKET DATA (10 endpoints) ====================

  async getMarketHolidays(): Promise<any> {
    return this.executeCommand('market-holidays', []);
  }

  async getMarketStatus(): Promise<any> {
    return this.executeCommand('market-status', []);
  }

  async getSingleTickerSnapshot(ticker: string): Promise<any> {
    return this.executeCommand('ticker-snapshot', [`--ticker=${ticker}`]);
  }

  async getFullMarketSnapshot(tickers: string[], includeOtc?: boolean): Promise<any> {
    const args = [`--tickers=${tickers.join(',')}`];
    if (includeOtc !== undefined) args.push(`--include-otc=${includeOtc}`);
    return this.executeCommand('market-snapshot', args);
  }

  async getUnifiedSnapshot(options?: any): Promise<any> {
    const args: string[] = [];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('unified-snapshot', args);
  }

  async getTopMarketMovers(direction: 'gainers' | 'losers', includeOtc?: boolean): Promise<any> {
    const args = [`--direction=${direction}`];
    if (includeOtc !== undefined) args.push(`--include-otc=${includeOtc}`);
    return this.executeCommand('top-movers', args);
  }

  async getTrades(ticker: string, options?: any): Promise<any> {
    const args = [`--ticker=${ticker}`];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('trades', args);
  }

  async getLastTrade(ticker: string): Promise<any> {
    return this.executeCommand('last-trade', [`--ticker=${ticker}`]);
  }

  async getQuotes(ticker: string, options?: any): Promise<any> {
    const args = [`--ticker=${ticker}`];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('quotes', args);
  }

  async getLastQuote(ticker: string): Promise<any> {
    return this.executeCommand('last-quote', [`--ticker=${ticker}`]);
  }

  // ==================== TECHNICAL INDICATORS (4 endpoints) ====================

  async getSMA(ticker: string, options: any): Promise<any> {
    const args = [`--ticker=${ticker}`];
    Object.entries(options).forEach(([key, value]) => {
      if (value !== undefined) {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      }
    });
    return this.executeCommand('sma', args);
  }

  async getEMA(ticker: string, options: any): Promise<any> {
    const args = [`--ticker=${ticker}`];
    Object.entries(options).forEach(([key, value]) => {
      if (value !== undefined) {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      }
    });
    return this.executeCommand('ema', args);
  }

  async getMACD(ticker: string, options: any): Promise<any> {
    const args = [`--ticker=${ticker}`];
    Object.entries(options).forEach(([key, value]) => {
      if (value !== undefined) {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      }
    });
    return this.executeCommand('macd', args);
  }

  async getRSI(ticker: string, options: any): Promise<any> {
    const args = [`--ticker=${ticker}`];
    Object.entries(options).forEach(([key, value]) => {
      if (value !== undefined) {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      }
    });
    return this.executeCommand('rsi', args);
  }

  // ==================== FINANCIALS (4 endpoints) ====================

  async getBalanceSheets(tickers: string, options?: any): Promise<any> {
    const args = [`--tickers=${tickers}`];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('balance-sheets', args);
  }

  async getCashFlowStatements(tickers: string, options?: any): Promise<any> {
    const args = [`--tickers=${tickers}`];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('cash-flow-statements', args);
  }

  async getIncomeStatements(tickers: string, options?: any): Promise<any> {
    const args = [`--tickers=${tickers}`];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('income-statements', args);
  }

  async getFinancialRatios(ticker: string, options?: any): Promise<any> {
    const args = [`--ticker=${ticker}`];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('ratios', args);
  }

  // ==================== SHORT INTEREST (2 endpoints) ====================

  async getShortInterest(ticker: string, options?: any): Promise<any> {
    const args = [`--ticker=${ticker}`];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('short-interest', args);
  }

  async getShortVolume(ticker: string, options?: any): Promise<any> {
    const args = [`--ticker=${ticker}`];
    if (options) {
      Object.entries(options).forEach(([key, value]) => {
        const kebabKey = key.replace(/([A-Z])/g, '-$1').toLowerCase();
        args.push(`--${kebabKey}=${value}`);
      });
    }
    return this.executeCommand('short-volume', args);
  }
}

export const polygonService = new PolygonService();
