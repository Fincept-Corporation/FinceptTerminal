// Memcached In-Memory Cache Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class MemcachedAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const servers = this.getConfig<string>('servers');

      if (!servers) {
        return this.createErrorResult('Servers are required');
      }

      console.log('Validating Memcached configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for Memcached at ${servers}`, {
        validatedAt: new Date().toISOString(),
        servers,
        note: 'Client-side validation only. Memcached requires backend integration with memcached client library.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/memcached', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'memcached',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Memcached query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return { ...base, servers: this.getConfig('servers') };
  }

  async get(key: string): Promise<any> {
    return await this.query('get', { key });
  }

  async set(key: string, value: any, ttl?: number): Promise<any> {
    return await this.query('set', { key, value, ttl });
  }

  async delete(key: string): Promise<any> {
    return await this.query('delete', { key });
  }

  async flush(): Promise<any> {
    return await this.query('flush');
  }

  async stats(): Promise<any> {
    return await this.query('stats');
  }
}
