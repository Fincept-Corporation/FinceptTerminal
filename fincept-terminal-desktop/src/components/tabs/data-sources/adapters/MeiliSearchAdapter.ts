// MeiliSearch API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class MeiliSearchAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const apiKey = this.getConfig<string>('apiKey');

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Testing MeiliSearch connection...');

      try {
        // Test with health endpoint
        const headers: Record<string, string> = {};
        if (apiKey) {
          headers.Authorization = `Bearer ${apiKey}`;
        }

        const response = await fetch(`${host}/health`, {
          method: 'GET',
          headers,
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid API key');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to MeiliSearch', {
          status: data.status || 'available',
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
      const host = this.getConfig<string>('host');
      const apiKey = this.getConfig<string>('apiKey');

      const headers: Record<string, string> = {
        'Content-Type': 'application/json',
      };

      if (apiKey) {
        headers.Authorization = `Bearer ${apiKey}`;
      }

      const options: RequestInit = {
        method,
        headers,
      };

      if (body && (method === 'POST' || method === 'PUT' || method === 'PATCH')) {
        options.body = JSON.stringify(body);
      }

      const url = `${host}${endpoint}`;
      const response = await fetch(url, options);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`MeiliSearch query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      index: this.getConfig('index'),
      hasApiKey: !!this.getConfig('apiKey'),
    };
  }

  /**
   * Search index
   */
  async search(indexName: string, query: string, options?: Record<string, any>): Promise<any> {
    return await this.query(`/indexes/${indexName}/search`, 'POST', { q: query, ...options });
  }

  /**
   * List indexes
   */
  async listIndexes(): Promise<any> {
    return await this.query('/indexes');
  }

  /**
   * Create index
   */
  async createIndex(uid: string, primaryKey?: string): Promise<any> {
    return await this.query('/indexes', 'POST', { uid, primaryKey });
  }

  /**
   * Delete index
   */
  async deleteIndex(indexName: string): Promise<any> {
    return await this.query(`/indexes/${indexName}`, 'DELETE');
  }

  /**
   * Add documents
   */
  async addDocuments(indexName: string, documents: any[], primaryKey?: string): Promise<any> {
    const params = primaryKey ? `?primaryKey=${primaryKey}` : '';
    return await this.query(`/indexes/${indexName}/documents${params}`, 'POST', documents);
  }

  /**
   * Get document
   */
  async getDocument(indexName: string, documentId: string): Promise<any> {
    return await this.query(`/indexes/${indexName}/documents/${documentId}`);
  }

  /**
   * Delete document
   */
  async deleteDocument(indexName: string, documentId: string): Promise<any> {
    return await this.query(`/indexes/${indexName}/documents/${documentId}`, 'DELETE');
  }

  /**
   * Get index stats
   */
  async getStats(indexName: string): Promise<any> {
    return await this.query(`/indexes/${indexName}/stats`);
  }

  /**
   * Get settings
   */
  async getSettings(indexName: string): Promise<any> {
    return await this.query(`/indexes/${indexName}/settings`);
  }

  /**
   * Update settings
   */
  async updateSettings(indexName: string, settings: any): Promise<any> {
    return await this.query(`/indexes/${indexName}/settings`, 'PATCH', settings);
  }
}
