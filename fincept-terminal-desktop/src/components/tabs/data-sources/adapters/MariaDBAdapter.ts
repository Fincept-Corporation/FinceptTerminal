// MariaDB Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class MariaDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 3306);
      const database = this.getConfig<string>('database');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const ssl = this.getConfig<boolean>('ssl', false);

      if (!host || !database || !username || !password) {
        return this.createErrorResult('Missing required connection parameters');
      }

      console.log('Validating MariaDB configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for MariaDB database "${database}"`,
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
          type: 'mariadb',
          query: sql,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`MariaDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 3306),
      database: this.getConfig('database'),
      ssl: this.getConfig('ssl', false),
    };
  }
}
