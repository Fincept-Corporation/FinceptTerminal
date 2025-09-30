// OrientDB Multi-Model Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class OrientDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 2480);
      const database = this.getConfig<string>('database');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!host || !database || !username || !password) {
        return this.createErrorResult('Host, database, username, and password are required');
      }

      console.log('Testing OrientDB connection...');

      try {
        const credentials = btoa(`${username}:${password}`);
        const response = await fetch(`http://${host}:${port}/connect/${database}`, {
          headers: { Authorization: `Basic ${credentials}` },
        });

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        return this.createSuccessResult('Successfully connected to OrientDB', {
          host,
          port,
          database,
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

  async query(sql: string, params?: any[]): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/orientdb', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'orientdb',
          sql,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`OrientDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 2480),
      database: this.getConfig('database'),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }
}
