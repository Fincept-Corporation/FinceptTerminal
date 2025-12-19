import { invoke } from '@tauri-apps/api/core';

import { sqliteService } from './sqliteService';
import { polygonLogger } from './loggerService';
import type {
  PolygonResponse,
  PolygonTickerDetails,
  PolygonQuote,
  PolygonNews,
  PolygonAggregate,
  PolygonTrade,
  PolygonBalanceSheet,
  PolygonIncomeStatement,
  PolygonCashFlow,
  PolygonIndicatorResult
} from '@/types/api';

// =============================================================================
// Polygon API Option Types
// =============================================================================

export interface TickerListOptions {
  ticker?: string;
  type?: string;
  market?: string;
  exchange?: string;
  cusip?: string;
  cik?: string;
  date?: string;
  search?: string;
  active?: boolean;
  sort?: string;
  order?: 'asc' | 'desc';
  limit?: number;
}

export interface NewsOptions {
  ticker?: string;
  publishedUtc?: string;
  order?: 'asc' | 'desc';
  limit?: number;
  sort?: string;
}

export interface CorporateActionOptions {
  ticker?: string;
  exDividendDate?: string;
  recordDate?: string;
  declarationDate?: string;
  payDate?: string;
  frequency?: number;
  cashAmount?: number;
  dividendType?: string;
  sort?: string;
  order?: 'asc' | 'desc';
  limit?: number;
}

export interface AggregatesOptions {
  adjusted?: boolean;
  sort?: 'asc' | 'desc';
  limit?: number;
}

export interface TradesQuotesOptions {
  timestamp?: string;
  timestampGt?: string;
  timestampLt?: string;
  order?: 'asc' | 'desc';
  limit?: number;
  sort?: string;
}

export interface IndicatorOptions {
  timespan?: 'minute' | 'hour' | 'day' | 'week' | 'month' | 'quarter' | 'year';
  adjusted?: boolean;
  window?: number;
  seriesType?: 'close' | 'open' | 'high' | 'low';
  expandUnderlying?: boolean;
  order?: 'asc' | 'desc';
  limit?: number;
  shortWindow?: number;
  longWindow?: number;
  signalWindow?: number;
}

export interface FinancialsOptions {
  period?: 'annual' | 'quarterly' | 'ttm';
  limit?: number;
  order?: 'asc' | 'desc';
  sort?: string;
  filingDateGte?: string;
  filingDateLte?: string;
}

export interface SnapshotOptions {
  tickers?: string;
  includeOtc?: boolean;
}

// =============================================================================
// Polygon.io Service Layer - Comprehensive Coverage of 31 Endpoints
// =============================================================================
class PolygonService {
  private apiKey: string = '';

  async loadApiKey(): Promise<string> {
    try {
      polygonLogger.info('Loading API key from database...');
      const credentials = await sqliteService.getCredentials();
      polygonLogger.debug('Found credentials:', credentials.map(c => c.service_name));

      const polygonCred = credentials.find(c => c.service_name.toLowerCase() === 'polygon.io');
      polygonLogger.debug('Polygon credential found:', !!polygonCred);

      if (polygonCred?.api_key) {
        this.apiKey = polygonCred.api_key;
        // Set environment variable for Python script
        localStorage.setItem('POLYGON_API_KEY', polygonCred.api_key);
        polygonLogger.info('API key loaded successfully, length:', polygonCred.api_key.length);
        return polygonCred.api_key;
      }
      polygonLogger.warn('No Polygon.io API key found in credentials');
    } catch (error) {
      polygonLogger.error('Failed to load Polygon API key from database:', error);
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
        polygonLogger.debug('API key not set, loading...');
        await this.loadApiKey();
      }

      polygonLogger.debug('Executing command: ' + command, { args });
      polygonLogger.debug('API key present:', !!this.apiKey);

      const result = await invoke<string>('execute_polygon_command', {
        command,
        args,
        apiKey: this.apiKey || null
      });

      polygonLogger.debug('Command result received, length:', result.length);
      const parsed = JSON.parse(result);
      polygonLogger.debug('Parsed result success:', parsed.success);

      return parsed;
    } catch (error) {
      polygonLogger.error('Polygon API error:', error);
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
