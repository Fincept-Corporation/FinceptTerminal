// Vertica Analytics Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class VerticaAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 5433);
      const database = this.getConfig<string>('database');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!host || !database || !username || !password) {
        return this.createErrorResult('Host, database, username, and password are required');
      }

      console.log('Validating Vertica configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for Vertica "${database}" at ${host}:${port}`, {
        validatedAt: new Date().toISOString(),
        host,
        port,
        database,
        note: 'Client-side validation only. Vertica requires backend integration with vertica-nodejs driver.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(sql: string, params?: any[]): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/vertica', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'vertica',
          sql,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Vertica query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 5433),
      database: this.getConfig('database'),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  async executeQuery(sql: string, params?: any[]): Promise<any> {
    return await this.query(sql, params);
  }

  async getTables(schema: string = 'public'): Promise<any> {
    return await this.query('SELECT table_name FROM v_catalog.tables WHERE table_schema = $1', [schema]);
  }

  async getTableSchema(tableName: string, schema: string = 'public'): Promise<any> {
    return await this.query(
      'SELECT column_name, data_type FROM v_catalog.columns WHERE table_schema = $1 AND table_name = $2',
      [schema, tableName]
    );
  }
}
