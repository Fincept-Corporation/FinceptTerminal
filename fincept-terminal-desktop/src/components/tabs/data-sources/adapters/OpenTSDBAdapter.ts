// OpenTSDB Time Series Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class OpenTSDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const url = this.getConfig<string>('url');

      if (!url) {
        return this.createErrorResult('URL is required');
      }

      console.log('Testing OpenTSDB connection...');

      try {
        // Test with version endpoint
        const response = await fetch(`${url}/api/version`);

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to OpenTSDB', {
          version: data.version || 'Unknown',
          shortRevision: data.short_revision || 'Unknown',
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

  async query(endpoint: string, method: string = 'GET', body?: any): Promise<any> {
    try {
      const url = this.getConfig<string>('url');

      const options: RequestInit = {
        method,
        headers: {
          'Content-Type': 'application/json',
        },
      };

      if (body && (method === 'POST' || method === 'PUT')) {
        options.body = JSON.stringify(body);
      }

      const fullUrl = `${url}${endpoint}`;
      const response = await fetch(fullUrl, options);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`OpenTSDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      url: this.getConfig('url'),
      provider: 'OpenTSDB',
    };
  }

  /**
   * Query time series data
   */
  async queryTimeSeries(queries: any[]): Promise<any> {
    return await this.query('/api/query', 'POST', { queries });
  }

  /**
   * Get last data point
   */
  async queryLast(queries: any[]): Promise<any> {
    return await this.query('/api/query/last', 'POST', { queries });
  }

  /**
   * Put data points
   */
  async putDataPoints(dataPoints: any[]): Promise<any> {
    return await this.query('/api/put', 'POST', dataPoints);
  }

  /**
   * Get aggregators
   */
  async getAggregators(): Promise<any> {
    return await this.query('/api/aggregators');
  }

  /**
   * Get OpenTSDB config
   */
  async getOpenTSDBConfig(): Promise<any> {
    return await this.query('/api/config');
  }

  /**
   * Search lookup
   */
  async searchLookup(metric: string, limit: number = 25): Promise<any> {
    return await this.query('/api/search/lookup', 'POST', {
      metric,
      limit,
    });
  }

  /**
   * Suggest metrics
   */
  async suggestMetrics(type: string = 'metrics', q?: string, max: number = 25): Promise<any> {
    const params = new URLSearchParams({ type, max: max.toString() });
    if (q) params.append('q', q);

    return await this.query(`/api/suggest?${params.toString()}`);
  }

  /**
   * Get statistics
   */
  async getStats(filters?: string[]): Promise<any> {
    let endpoint = '/api/stats';
    if (filters && filters.length > 0) {
      endpoint += `?${filters.map((f) => `filter=${f}`).join('&')}`;
    }
    return await this.query(endpoint);
  }

  /**
   * Get UIDs for metric/tagk/tagv
   */
  async getUID(type: 'metric' | 'tagk' | 'tagv', name: string): Promise<any> {
    return await this.query(`/api/uid/uidmeta`, 'POST', {
      type,
      name,
    });
  }

  /**
   * Assign UID
   */
  async assignUID(type: 'metric' | 'tagk' | 'tagv', name: string): Promise<any> {
    return await this.query('/api/uid/assign', 'POST', {
      [type]: [name],
    });
  }
}
