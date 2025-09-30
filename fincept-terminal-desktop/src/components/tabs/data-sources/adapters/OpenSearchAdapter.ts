// OpenSearch (Elasticsearch fork) Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class OpenSearchAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 9200);
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const useSsl = this.getConfig<boolean>('useSsl', true);

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Testing OpenSearch connection...');

      try {
        const protocol = useSsl ? 'https' : 'http';
        const url = `${protocol}://${host}:${port}/`;

        const headers: Record<string, string> = {
          'Content-Type': 'application/json',
        };

        if (username && password) {
          const credentials = btoa(`${username}:${password}`);
          headers.Authorization = `Basic ${credentials}`;
        }

        const response = await fetch(url, {
          method: 'GET',
          headers,
        });

        if (!response.ok) {
          if (response.status === 401) {
            throw new Error('Authentication failed');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to OpenSearch', {
          clusterName: data.cluster_name || 'Unknown',
          version: data.version?.number || 'Unknown',
          distribution: data.version?.distribution || 'opensearch',
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
      const port = this.getConfig<number>('port', 9200);
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const useSsl = this.getConfig<boolean>('useSsl', true);

      const protocol = useSsl ? 'https' : 'http';
      const url = `${protocol}://${host}:${port}${endpoint}`;

      const headers: Record<string, string> = {
        'Content-Type': 'application/json',
      };

      if (username && password) {
        const credentials = btoa(`${username}:${password}`);
        headers.Authorization = `Basic ${credentials}`;
      }

      const options: RequestInit = {
        method,
        headers,
      };

      if (body && (method === 'POST' || method === 'PUT' || method === 'PATCH')) {
        options.body = JSON.stringify(body);
      }

      const response = await fetch(url, options);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`OpenSearch query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 9200),
      useSsl: this.getConfig('useSsl', true),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  /**
   * Get cluster health
   */
  async getClusterHealth(): Promise<any> {
    return await this.query('/_cluster/health');
  }

  /**
   * Get cluster stats
   */
  async getClusterStats(): Promise<any> {
    return await this.query('/_cluster/stats');
  }

  /**
   * List indices
   */
  async listIndices(): Promise<any> {
    return await this.query('/_cat/indices?format=json');
  }

  /**
   * Create index
   */
  async createIndex(indexName: string, settings?: any): Promise<any> {
    return await this.query(`/${indexName}`, 'PUT', settings);
  }

  /**
   * Delete index
   */
  async deleteIndex(indexName: string): Promise<any> {
    return await this.query(`/${indexName}`, 'DELETE');
  }

  /**
   * Search documents
   */
  async search(indexName: string, query: any): Promise<any> {
    return await this.query(`/${indexName}/_search`, 'POST', query);
  }

  /**
   * Index document
   */
  async indexDocument(indexName: string, document: any, documentId?: string): Promise<any> {
    const endpoint = documentId ? `/${indexName}/_doc/${documentId}` : `/${indexName}/_doc`;
    return await this.query(endpoint, 'POST', document);
  }

  /**
   * Get document
   */
  async getDocument(indexName: string, documentId: string): Promise<any> {
    return await this.query(`/${indexName}/_doc/${documentId}`);
  }

  /**
   * Update document
   */
  async updateDocument(indexName: string, documentId: string, document: any): Promise<any> {
    return await this.query(`/${indexName}/_update/${documentId}`, 'POST', { doc: document });
  }

  /**
   * Delete document
   */
  async deleteDocument(indexName: string, documentId: string): Promise<any> {
    return await this.query(`/${indexName}/_doc/${documentId}`, 'DELETE');
  }

  /**
   * Bulk operations
   */
  async bulk(operations: any[]): Promise<any> {
    const body = operations.map((op) => JSON.stringify(op)).join('\n') + '\n';
    return await this.query('/_bulk', 'POST', body);
  }

  /**
   * Get index mapping
   */
  async getMapping(indexName: string): Promise<any> {
    return await this.query(`/${indexName}/_mapping`);
  }

  /**
   * Put index mapping
   */
  async putMapping(indexName: string, mapping: any): Promise<any> {
    return await this.query(`/${indexName}/_mapping`, 'PUT', mapping);
  }
}
