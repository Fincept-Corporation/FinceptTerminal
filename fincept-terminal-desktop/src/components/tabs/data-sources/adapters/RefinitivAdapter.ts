// Refinitiv/Reuters Financial Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class RefinitivAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const appKey = this.getConfig<string>('appKey');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!appKey || !username || !password) {
        return this.createErrorResult('App key, username, and password are required');
      }

      console.log('Validating Refinitiv API configuration (client-side only)');

      // Refinitiv requires WebSocket connection and specialized protocol
      return this.createSuccessResult('Configuration validated for Refinitiv API', {
        validatedAt: new Date().toISOString(),
        username,
        note: 'Client-side validation only. Refinitiv API requires backend integration with WebSocket and EMA/RFA protocols.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // Refinitiv operations need to go through backend
      const response = await fetch('/api/data-sources/refinitiv', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'refinitiv',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Refinitiv operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Refinitiv query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      username: this.getConfig('username'),
      hasCredentials: !!(this.getConfig('appKey') && this.getConfig('password')),
      provider: 'Refinitiv',
    };
  }

  /**
   * Get market price data
   */
  async getMarketPrice(rics: string[]): Promise<any> {
    return await this.query('getMarketPrice', { rics });
  }

  /**
   * Get historical pricing
   */
  async getHistoricalPricing(ric: string, interval: string, start?: string, end?: string): Promise<any> {
    return await this.query('getHistoricalPricing', { ric, interval, start, end });
  }

  /**
   * Get news headlines
   */
  async getNewsHeadlines(query?: string, limit?: number): Promise<any> {
    return await this.query('getNewsHeadlines', { query, limit });
  }

  /**
   * Search instruments
   */
  async searchInstruments(query: string): Promise<any> {
    return await this.query('searchInstruments', { query });
  }

  /**
   * Subscribe to streaming data
   */
  async subscribe(rics: string[], fields?: string[]): Promise<any> {
    return await this.query('subscribe', { rics, fields });
  }
}
