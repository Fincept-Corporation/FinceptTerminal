// QuestDB Time Series Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class QuestDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host', 'localhost');
      const httpPort = this.getConfig<number>('httpPort', 9000);
      const pgPort = this.getConfig<number>('pgPort', 8812);
      const username = this.getConfig<string>('username', 'admin');
      const password = this.getConfig<string>('password', 'quest');
      const protocol = this.getConfig<string>('protocol', 'http');

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Validating QuestDB configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for QuestDB at ${host}:${httpPort}`,
        {
          validatedAt: new Date().toISOString(),
          host,
          httpPort,
          pgPort,
          protocol,
          authenticated: !!(username && password),
          note: 'Client-side validation only. Full connection test requires backend integration.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(sql: string): Promise<any> {
    try {
      const host = this.getConfig<string>('host');
      const httpPort = this.getConfig<number>('httpPort', 9000);
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const protocol = this.getConfig<string>('protocol', 'http');

      // URL encode the SQL query
      const encodedQuery = encodeURIComponent(sql);
      const queryUrl = `${protocol}://${host}:${httpPort}/exec?query=${encodedQuery}`;

      const headers: Record<string, string> = {};
      if (username && password) {
        const credentials = btoa(`${username}:${password}`);
        headers['Authorization'] = `Basic ${credentials}`;
      }

      const response = await fetch(queryUrl, {
        method: 'GET',
        headers,
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      // Transform QuestDB response format to a more standard format
      if (data.columns && data.dataset) {
        const columns = data.columns.map((col: any) => col.name);
        const rows = data.dataset.map((row: any[]) => {
          const obj: Record<string, any> = {};
          columns.forEach((col: string, idx: number) => {
            obj[col] = row[idx];
          });
          return obj;
        });

        return {
          columns,
          rows,
          count: data.count,
          timings: data.timings,
        };
      }

      return data;
    } catch (error) {
      throw new Error(`QuestDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      httpPort: this.getConfig('httpPort', 9000),
      pgPort: this.getConfig('pgPort', 8812),
      protocol: this.getConfig('protocol', 'http'),
      hasAuth: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  /**
   * Insert data using QuestDB ILP (InfluxDB Line Protocol)
   */
  async insertILP(data: string): Promise<void> {
    const host = this.getConfig<string>('host');
    const ilpPort = this.getConfig<number>('ilpPort', 9009);

    // This would typically use a TCP or UDP connection
    // For now, we'll throw an error indicating it needs backend support
    throw new Error('ILP insertion requires backend support. Use SQL INSERT statements via query() method instead.');
  }
}
