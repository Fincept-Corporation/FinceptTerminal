// Redis Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class RedisAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host', 'localhost');
      const port = this.getConfig<number>('port', 6379);
      const password = this.getConfig<string>('password');
      const database = this.getConfig<number>('database', 0);
      const ssl = this.getConfig<boolean>('ssl', false);

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Validating Redis configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for Redis at ${host}:${port} (DB ${database})`,
        {
          validatedAt: new Date().toISOString(),
          host,
          port,
          database,
          ssl,
          hasPassword: !!password,
          note: 'Client-side validation only. Full connection test requires backend integration.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(command: string): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/query', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'redis',
          command,
        }),
      });

      if (!response.ok) {
        throw new Error(`Command failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Redis command error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 6379),
      database: this.getConfig('database', 0),
      ssl: this.getConfig('ssl', false),
    };
  }
}
