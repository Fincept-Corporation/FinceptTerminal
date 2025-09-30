// PostgreSQL Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class PostgreSQLAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 5432);
      const database = this.getConfig<string>('database');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const ssl = this.getConfig<boolean>('ssl', false);

      if (!host || !database || !username || !password) {
        return this.createErrorResult('Missing required connection parameters');
      }

      // For now, perform client-side validation only
      // Backend connection testing would require a backend API endpoint
      console.log('Validating PostgreSQL configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for PostgreSQL database "${database}"`,
        {
          validatedAt: new Date().toISOString(),
          host,
          port,
          database,
          username,
          ssl,
          note: 'Client-side validation only. Full connection test requires backend integration.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(sql: string): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/query', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'postgresql',
          query: sql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`PostgreSQL query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 5432),
      database: this.getConfig('database'),
      ssl: this.getConfig('ssl', false),
    };
  }
}
