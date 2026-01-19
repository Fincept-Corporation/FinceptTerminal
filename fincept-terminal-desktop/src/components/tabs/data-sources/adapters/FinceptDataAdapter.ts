// Fincept Data API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class FinceptDataAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 8194);

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Validating Fincept Data API configuration (client-side only)');

      // Fincept Data API connection validation
      return this.createSuccessResult(`Configuration validated for Fincept Data API at ${host}:${port}`, {
        validatedAt: new Date().toISOString(),
        host,
        port,
        note: 'Client-side validation only. Fincept Data API requires backend integration.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // Fincept Data operations need to go through backend
      const response = await fetch('/api/data-sources/fincept', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'fincept',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Fincept Data operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Fincept Data query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 8194),
      provider: 'Fincept',
    };
  }

  /**
   * Get reference data
   */
  async getReferenceData(securities: string[], fields: string[]): Promise<any> {
    return await this.query('getReferenceData', { securities, fields });
  }

  /**
   * Get historical data
   */
  async getHistoricalData(
    securities: string[],
    fields: string[],
    startDate: string,
    endDate: string
  ): Promise<any> {
    return await this.query('getHistoricalData', { securities, fields, startDate, endDate });
  }

  /**
   * Get intraday bar data
   */
  async getIntradayBarData(security: string, startDateTime: string, endDateTime: string, interval: number): Promise<any> {
    return await this.query('getIntradayBarData', { security, startDateTime, endDateTime, interval });
  }

  /**
   * Subscribe to real-time data
   */
  async subscribe(securities: string[], fields: string[]): Promise<any> {
    return await this.query('subscribe', { securities, fields });
  }
}
