// InfluxDB Time Series Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class InfluxDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const url = this.getConfig<string>('url', 'http://localhost:8086');
      const token = this.getConfig<string>('token');
      const org = this.getConfig<string>('org');
      const bucket = this.getConfig<string>('bucket');
      const version = this.getConfig<string>('version', '2.x');

      if (!url) {
        return this.createErrorResult('URL is required');
      }

      // For InfluxDB 2.x, token is required; for 1.x, username/password
      if (version === '2.x' && !token) {
        return this.createErrorResult('Token is required for InfluxDB 2.x');
      }

      console.log('Validating InfluxDB configuration (client-side only)');

      const metadata: Record<string, any> = {
        validatedAt: new Date().toISOString(),
        version,
        url,
        note: 'Client-side validation only. Full connection test requires backend integration.',
      };

      if (version === '2.x' && org && bucket) {
        metadata.org = org;
        metadata.bucket = bucket;
        metadata.hasToken = !!token;
      }

      return this.createSuccessResult(
        `Configuration validated for InfluxDB ${version} at ${url}`,
        metadata
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(fluxQuery: string): Promise<any> {
    try {
      const url = this.getConfig<string>('url');
      const token = this.getConfig<string>('token');
      const org = this.getConfig<string>('org');
      const version = this.getConfig<string>('version', '2.x');

      if (version === '2.x') {
        const queryUrl = `${url}/api/v2/query?org=${org}`;

        const response = await fetch(queryUrl, {
          method: 'POST',
          headers: {
            'Authorization': `Token ${token}`,
            'Content-Type': 'application/vnd.flux',
            'Accept': 'application/csv',
          },
          body: fluxQuery,
        });

        if (!response.ok) {
          throw new Error(`Query failed: ${response.status} ${response.statusText}`);
        }

        // Parse CSV response
        const csvData = await response.text();
        return this.parseCSVResponse(csvData);
      } else {
        // For InfluxDB 1.x, use InfluxQL
        throw new Error('InfluxDB 1.x queries not yet implemented');
      }
    } catch (error) {
      throw new Error(`InfluxDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  private parseCSVResponse(csv: string): any[] {
    const lines = csv.split('\n').filter(line => line.trim());
    if (lines.length === 0) return [];

    // Skip annotation lines (starting with #)
    const dataLines = lines.filter(line => !line.startsWith('#'));
    if (dataLines.length === 0) return [];

    const headers = dataLines[0].split(',');
    const results = [];

    for (let i = 1; i < dataLines.length; i++) {
      const values = dataLines[i].split(',');
      const row: Record<string, any> = {};
      headers.forEach((header, index) => {
        row[header] = values[index];
      });
      results.push(row);
    }

    return results;
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      url: this.getConfig('url'),
      version: this.getConfig('version', '2.x'),
      org: this.getConfig('org'),
      bucket: this.getConfig('bucket'),
      hasToken: !!this.getConfig('token'),
    };
  }
}
