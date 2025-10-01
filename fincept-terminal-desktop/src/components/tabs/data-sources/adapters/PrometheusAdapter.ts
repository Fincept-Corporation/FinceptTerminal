// Prometheus Monitoring System Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class PrometheusAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const url = this.getConfig<string>('url', 'http://localhost:9090');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!url) {
        return this.createErrorResult('URL is required');
      }

      console.log('Testing Prometheus connection...');

      try {
        const headers: Record<string, string> = {};
        if (username && password) {
          const credentials = btoa(`${username}:${password}`);
          headers['Authorization'] = `Basic ${credentials}`;
        }

        // Test with health endpoint
        const response = await fetch(`${url}/-/healthy`, {
          method: 'GET',
          headers,
        });

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        // Get build info
        const buildResponse = await fetch(`${url}/api/v1/status/buildinfo`, { headers });
        let buildInfo = {};
        if (buildResponse.ok) {
          const data = await buildResponse.json();
          buildInfo = data.data || {};
        }

        return this.createSuccessResult('Successfully connected to Prometheus', {
          url,
          version: (buildInfo as any)['version'] || 'Unknown',
          goVersion: (buildInfo as any)['goVersion'],
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          if (fetchError.message.includes('Failed to fetch')) {
            return this.createErrorResult(`Cannot reach Prometheus at ${url}. Is the server running?`);
          }
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(promQL: string, time?: number): Promise<any> {
    try {
      const url = this.getConfig<string>('url');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      const headers: Record<string, string> = {};
      if (username && password) {
        const credentials = btoa(`${username}:${password}`);
        headers['Authorization'] = `Basic ${credentials}`;
      }

      const params = new URLSearchParams({ query: promQL });
      if (time) {
        params.append('time', time.toString());
      }

      const response = await fetch(`${url}/api/v1/query?${params.toString()}`, {
        method: 'GET',
        headers,
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data.status !== 'success') {
        throw new Error(data.error || 'Query failed');
      }

      return data.data;
    } catch (error) {
      throw new Error(`Prometheus query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      url: this.getConfig('url'),
      hasAuth: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  /**
   * Query range data
   */
  async queryRange(promQL: string, start: number, end: number, step: string): Promise<any> {
    const url = this.getConfig<string>('url');
    const username = this.getConfig<string>('username');
    const password = this.getConfig<string>('password');

    const headers: Record<string, string> = {};
    if (username && password) {
      const credentials = btoa(`${username}:${password}`);
      headers['Authorization'] = `Basic ${credentials}`;
    }

    const params = new URLSearchParams({
      query: promQL,
      start: start.toString(),
      end: end.toString(),
      step,
    });

    const response = await fetch(`${url}/api/v1/query_range?${params.toString()}`, {
      method: 'GET',
      headers,
    });

    if (!response.ok) {
      throw new Error(`Range query failed: ${response.status} ${response.statusText}`);
    }

    const data = await response.json();
    return data.data;
  }

  /**
   * Get all metric names
   */
  async getMetricNames(): Promise<string[]> {
    const url = this.getConfig<string>('url');
    const username = this.getConfig<string>('username');
    const password = this.getConfig<string>('password');

    const headers: Record<string, string> = {};
    if (username && password) {
      const credentials = btoa(`${username}:${password}`);
      headers['Authorization'] = `Basic ${credentials}`;
    }

    const response = await fetch(`${url}/api/v1/label/__name__/values`, { headers });

    if (!response.ok) {
      throw new Error('Failed to get metric names');
    }

    const data = await response.json();
    return data.data || [];
  }

  /**
   * Get label values
   */
  async getLabelValues(label: string): Promise<string[]> {
    const url = this.getConfig<string>('url');
    const username = this.getConfig<string>('username');
    const password = this.getConfig<string>('password');

    const headers: Record<string, string> = {};
    if (username && password) {
      const credentials = btoa(`${username}:${password}`);
      headers['Authorization'] = `Basic ${credentials}`;
    }

    const response = await fetch(`${url}/api/v1/label/${label}/values`, { headers });

    if (!response.ok) {
      throw new Error(`Failed to get values for label ${label}`);
    }

    const data = await response.json();
    return data.data || [];
  }

  /**
   * Get targets
   */
  async getTargets(): Promise<any> {
    const url = this.getConfig<string>('url');
    const username = this.getConfig<string>('username');
    const password = this.getConfig<string>('password');

    const headers: Record<string, string> = {};
    if (username && password) {
      const credentials = btoa(`${username}:${password}`);
      headers['Authorization'] = `Basic ${credentials}`;
    }

    const response = await fetch(`${url}/api/v1/targets`, { headers });

    if (!response.ok) {
      throw new Error('Failed to get targets');
    }

    const data = await response.json();
    return data.data;
  }
}
