// VictoriaMetrics Time Series Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class VictoriaMetricsAdapter extends BaseAdapter {
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

      console.log('Testing VictoriaMetrics connection...');

      try {
        // Test with health endpoint
        const response = await fetch(`${url}/health`);

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const health = await response.text();

        return this.createSuccessResult('Successfully connected to VictoriaMetrics', {
          status: health || 'OK',
          endpoint: url,
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
      const url = this.getConfig<string>('url');

      const queryParams = params ? `?${new URLSearchParams(params).toString()}` : '';
      const fullUrl = `${url}${endpoint}${queryParams}`;

      const response = await fetch(fullUrl);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`VictoriaMetrics query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      url: this.getConfig('url'),
      provider: 'VictoriaMetrics',
    };
  }

  /**
   * Execute PromQL/MetricsQL query
   */
  async queryRange(query: string, start: string, end: string, step: string): Promise<any> {
    return await this.query('/api/v1/query_range', {
      query,
      start,
      end,
      step,
    });
  }

  /**
   * Execute instant query
   */
  async queryInstant(query: string, time?: string): Promise<any> {
    const params: Record<string, string> = { query };
    if (time) params.time = time;

    return await this.query('/api/v1/query', params);
  }

  /**
   * Get label values
   */
  async getLabelValues(label: string): Promise<any> {
    return await this.query(`/api/v1/label/${label}/values`);
  }

  /**
   * Get series
   */
  async getSeries(match: string[], start?: string, end?: string): Promise<any> {
    const params: Record<string, any> = {};
    match.forEach((m, i) => {
      params[`match[${i}]`] = m;
    });
    if (start) params.start = start;
    if (end) params.end = end;

    return await this.query('/api/v1/series', params);
  }

  /**
   * Get label names
   */
  async getLabels(): Promise<any> {
    return await this.query('/api/v1/labels');
  }

  /**
   * Get targets metadata
   */
  async getTargets(): Promise<any> {
    return await this.query('/api/v1/targets');
  }

  /**
   * Get alerts
   */
  async getAlerts(): Promise<any> {
    return await this.query('/api/v1/alerts');
  }
}
