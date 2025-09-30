// Intrinio Market Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class IntrinioAdapter extends BaseAdapter {
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

      console.log('Testing Intrinio API connection...');

      try {
        // Test with companies search endpoint
        const response = await fetch('https://api-v2.intrinio.com/companies?page_size=1', {
          method: 'GET',
          headers: {
            'Content-Type': 'application/json',
            Authorization: `Bearer ${apiKey}`,
          },
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key or unauthorized access');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Intrinio API', {
          companiesAvailable: data.total_results || 'Unknown',
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
      const url = `https://api-v2.intrinio.com${endpoint}${queryParams}`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json',
          Authorization: `Bearer ${apiKey}`,
        },
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Intrinio query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      hasApiKey: !!this.getConfig('apiKey'),
      provider: 'Intrinio',
    };
  }

  /**
   * Get company
   */
  async getCompany(identifier: string): Promise<any> {
    return await this.query(`/companies/${identifier}`);
  }

  /**
   * Search companies
   */
  async searchCompanies(query: string, pageSize: number = 100): Promise<any> {
    return await this.query('/companies', { query, page_size: pageSize.toString() });
  }

  /**
   * Get company securities
   */
  async getCompanySecurities(identifier: string): Promise<any> {
    return await this.query(`/companies/${identifier}/securities`);
  }

  /**
   * Get security
   */
  async getSecurity(identifier: string): Promise<any> {
    return await this.query(`/securities/${identifier}`);
  }

  /**
   * Get security realtime price
   */
  async getSecurityRealtimePrice(identifier: string): Promise<any> {
    return await this.query(`/securities/${identifier}/prices/realtime`);
  }

  /**
   * Get security historical data
   */
  async getSecurityHistoricalData(
    identifier: string,
    tag: string,
    frequency?: string,
    startDate?: string,
    endDate?: string
  ): Promise<any> {
    const params: Record<string, string> = { tag };
    if (frequency) params.frequency = frequency;
    if (startDate) params.start_date = startDate;
    if (endDate) params.end_date = endDate;

    return await this.query(`/securities/${identifier}/historical_data/${tag}`, params);
  }

  /**
   * Get security intraday prices
   */
  async getSecurityIntradayPrices(
    identifier: string,
    startDate?: string,
    startTime?: string,
    endDate?: string,
    endTime?: string
  ): Promise<any> {
    const params: Record<string, string> = {};
    if (startDate) params.start_date = startDate;
    if (startTime) params.start_time = startTime;
    if (endDate) params.end_date = endDate;
    if (endTime) params.end_time = endTime;

    return await this.query(`/securities/${identifier}/prices/intraday`, params);
  }

  /**
   * Get company fundamentals
   */
  async getCompanyFundamentals(identifier: string, statementCode?: string, fiscalYear?: number): Promise<any> {
    const params: Record<string, string> = {};
    if (statementCode) params.statement_code = statementCode;
    if (fiscalYear) params.fiscal_year = fiscalYear.toString();

    return await this.query(`/companies/${identifier}/fundamentals`, params);
  }

  /**
   * Get company news
   */
  async getCompanyNews(identifier: string, pageSize: number = 100): Promise<any> {
    return await this.query(`/companies/${identifier}/news`, { page_size: pageSize.toString() });
  }

  /**
   * Get company filings
   */
  async getCompanyFilings(identifier: string, pageSize: number = 100): Promise<any> {
    return await this.query(`/companies/${identifier}/filings`, { page_size: pageSize.toString() });
  }

  /**
   * Get indices
   */
  async getIndices(pageSize: number = 100): Promise<any> {
    return await this.query('/indices', { page_size: pageSize.toString() });
  }

  /**
   * Get index by ID
   */
  async getIndex(identifier: string): Promise<any> {
    return await this.query(`/indices/${identifier}`);
  }

  /**
   * Get economic data
   */
  async getEconomicData(indicator: string, startDate?: string, endDate?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (startDate) params.start_date = startDate;
    if (endDate) params.end_date = endDate;

    return await this.query(`/economic_data/${indicator}`, params);
  }

  /**
   * Get options chain
   */
  async getOptionsChain(symbol: string, expiration?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (expiration) params.expiration = expiration;

    return await this.query(`/options/chain/${symbol}`, params);
  }
}
