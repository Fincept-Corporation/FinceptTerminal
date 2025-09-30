// RethinkDB Real-Time Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class RethinkDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 28015);

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Validating RethinkDB configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for RethinkDB at ${host}:${port}`, {
        validatedAt: new Date().toISOString(),
        host,
        port,
        note: 'Client-side validation only. RethinkDB requires backend integration with rethinkdb driver.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(reql: string, params?: any): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/rethinkdb', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'rethinkdb',
          reql,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`RethinkDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 28015),
      database: this.getConfig('database'),
    };
  }
}
