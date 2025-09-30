// CockroachDB Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CockroachDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 26257);
      const database = this.getConfig<string>('database');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!host || !database || !username || !password) {
        return this.createErrorResult('Host, database, username, and password are required');
      }

      console.log('Validating CockroachDB configuration (client-side only)');

      // CockroachDB uses PostgreSQL wire protocol
      return this.createSuccessResult(`Configuration validated for CockroachDB "${database}" at ${host}:${port}`, {
        validatedAt: new Date().toISOString(),
        host,
        port,
        database,
        note: 'Client-side validation only. CockroachDB requires backend integration with pg driver.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(sql: string, params?: any[]): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/cockroachdb', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'cockroachdb',
          sql,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`CockroachDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 26257),
      database: this.getConfig('database'),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  /**
   * Execute SQL query
   */
  async executeQuery(sql: string, params?: any[]): Promise<any> {
    return await this.query(sql, params);
  }

  /**
   * Get tables
   */
  async getTables(): Promise<any> {
    return await this.query('SELECT table_name FROM information_schema.tables WHERE table_schema = current_schema()');
  }

  /**
   * Get table schema
   */
  async getTableSchema(tableName: string): Promise<any> {
    return await this.query(
      'SELECT column_name, data_type, is_nullable FROM information_schema.columns WHERE table_name = $1',
      [tableName]
    );
  }
}
