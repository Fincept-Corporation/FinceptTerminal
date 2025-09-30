// etcd Distributed Key-Value Store Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class EtcdAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const hosts = this.getConfig<string>('hosts');

      if (!hosts) {
        return this.createErrorResult('Hosts are required');
      }

      console.log('Testing etcd connection...');

      try {
        const hostArray = hosts.split(',');
        const response = await fetch(`${hostArray[0]}/version`);

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to etcd', {
          etcdserver: data.etcdserver || 'Unknown',
          etcdcluster: data.etcdcluster || 'Unknown',
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

  async query(endpoint: string, method: string = 'GET', body?: any): Promise<any> {
    try {
      const hosts = this.getConfig<string>('hosts');
      const hostArray = hosts.split(',');

      const options: RequestInit = {
        method,
        headers: { 'Content-Type': 'application/json' },
      };

      if (body && (method === 'POST' || method === 'PUT')) {
        options.body = JSON.stringify(body);
      }

      const response = await fetch(`${hostArray[0]}${endpoint}`, options);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`etcd query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return { ...base, hosts: this.getConfig('hosts') };
  }

  async get(key: string): Promise<any> {
    return await this.query('/v3/kv/range', 'POST', { key: btoa(key) });
  }

  async put(key: string, value: string): Promise<any> {
    return await this.query('/v3/kv/put', 'POST', { key: btoa(key), value: btoa(value) });
  }

  async delete(key: string): Promise<any> {
    return await this.query('/v3/kv/deleterange', 'POST', { key: btoa(key) });
  }
}
