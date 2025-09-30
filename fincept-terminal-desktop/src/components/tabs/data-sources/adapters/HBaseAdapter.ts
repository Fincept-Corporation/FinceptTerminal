// Apache HBase NoSQL Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class HBaseAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const restUrl = this.getConfig<string>('restUrl');

      if (!restUrl) {
        return this.createErrorResult('REST URL is required');
      }

      console.log('Testing HBase connection...');

      try {
        const response = await fetch(`${restUrl}/version/cluster`, {
          headers: { Accept: 'application/json' },
        });

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to HBase', {
          version: data.Version || 'Unknown',
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
      const restUrl = this.getConfig<string>('restUrl');

      const options: RequestInit = {
        method,
        headers: { Accept: 'application/json', 'Content-Type': 'application/json' },
      };

      if (body && (method === 'POST' || method === 'PUT')) {
        options.body = JSON.stringify(body);
      }

      const response = await fetch(`${restUrl}${endpoint}`, options);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`HBase query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return { ...base, restUrl: this.getConfig('restUrl') };
  }

  async listTables(): Promise<any> {
    return await this.query('/');
  }

  async getRow(table: string, rowKey: string): Promise<any> {
    return await this.query(`/${table}/${rowKey}`);
  }

  async putRow(table: string, rowKey: string, data: any): Promise<any> {
    return await this.query(`/${table}/${rowKey}`, 'PUT', data);
  }

  async deleteRow(table: string, rowKey: string): Promise<any> {
    return await this.query(`/${table}/${rowKey}`, 'DELETE');
  }

  async scanTable(table: string, startRow?: string, endRow?: string): Promise<any> {
    let endpoint = `/${table}/*`;
    if (startRow) endpoint += `?startrow=${startRow}`;
    if (endRow) endpoint += `${startRow ? '&' : '?'}endrow=${endRow}`;
    return await this.query(endpoint);
  }
}
