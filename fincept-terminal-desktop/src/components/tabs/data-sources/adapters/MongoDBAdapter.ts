// MongoDB Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class MongoDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const connectionString = this.getConfig<string>('connectionString');
      const database = this.getConfig<string>('database');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const authSource = this.getConfig<string>('authSource', 'admin');

      if (!connectionString && !database) {
        return this.createErrorResult('Either connection string or database name is required');
      }

      console.log('Validating MongoDB configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for MongoDB database "${database || 'from connection string'}"`,
        {
          validatedAt: new Date().toISOString(),
          database,
          connectionString: connectionString ? 'Provided' : 'Not provided',
          authSource,
          note: 'Client-side validation only. Full connection test requires backend integration.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(mongoQuery: string): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/query', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'mongodb',
          query: mongoQuery,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`MongoDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      database: this.getConfig('database'),
      authSource: this.getConfig('authSource', 'admin'),
      hasConnectionString: !!this.getConfig('connectionString'),
    };
  }
}
