// CouchDB NoSQL Database Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CouchDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const url = this.getConfig<string>('url');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!url) {
        return this.createErrorResult('URL is required');
      }

      console.log('Testing CouchDB connection...');

      try {
        const headers: Record<string, string> = {};
        if (username && password) {
          headers.Authorization = `Basic ${btoa(`${username}:${password}`)}`;
        }

        const response = await fetch(url, { headers });

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to CouchDB', {
          version: data.version || 'Unknown',
          vendor: data.vendor?.name || 'Unknown',
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
      const url = this.getConfig<string>('url');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      const headers: Record<string, string> = { 'Content-Type': 'application/json' };
      if (username && password) {
        headers.Authorization = `Basic ${btoa(`${username}:${password}`)}`;
      }

      const options: RequestInit = { method, headers };
      if (body && (method === 'POST' || method === 'PUT')) {
        options.body = JSON.stringify(body);
      }

      const response = await fetch(`${url}${endpoint}`, options);
      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`CouchDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return { ...base, url: this.getConfig('url'), hasCredentials: !!(this.getConfig('username') && this.getConfig('password')) };
  }

  async listDatabases(): Promise<any> {
    return await this.query('/_all_dbs');
  }

  async getDocument(db: string, docId: string): Promise<any> {
    return await this.query(`/${db}/${docId}`);
  }

  async createDocument(db: string, doc: any): Promise<any> {
    return await this.query(`/${db}`, 'POST', doc);
  }

  async updateDocument(db: string, docId: string, doc: any): Promise<any> {
    return await this.query(`/${db}/${docId}`, 'PUT', doc);
  }

  async deleteDocument(db: string, docId: string, rev: string): Promise<any> {
    return await this.query(`/${db}/${docId}?rev=${rev}`, 'DELETE');
  }

  async queryView(db: string, designDoc: string, viewName: string, params?: Record<string, any>): Promise<any> {
    const queryParams = params ? `?${new URLSearchParams(params).toString()}` : '';
    return await this.query(`/${db}/_design/${designDoc}/_view/${viewName}${queryParams}`);
  }
}
