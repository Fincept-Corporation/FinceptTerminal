// ArangoDB Multi-Model Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class ArangoDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const url = this.getConfig<string>('url');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!url || !username || !password) {
        return this.createErrorResult('URL, username, and password are required');
      }

      console.log('Testing ArangoDB connection...');

      try {
        const credentials = btoa(`${username}:${password}`);
        const response = await fetch(`${url}/_api/version`, {
          headers: { Authorization: `Basic ${credentials}` },
        });

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to ArangoDB', {
          version: data.version || 'Unknown',
          server: data.server || 'arango',
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

  async query(aql: string, bindVars?: Record<string, any>): Promise<any> {
    try {
      const url = this.getConfig<string>('url');
      const database = this.getConfig<string>('database', '_system');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      const credentials = btoa(`${username}:${password}`);
      const response = await fetch(`${url}/_db/${database}/_api/cursor`, {
        method: 'POST',
        headers: {
          Authorization: `Basic ${credentials}`,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ query: aql, bindVars }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`ArangoDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      url: this.getConfig('url'),
      database: this.getConfig('database', '_system'),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }
}
