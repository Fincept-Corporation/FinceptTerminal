// Algolia Search API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class AlgoliaAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const appId = this.getConfig<string>('appId');
      const apiKey = this.getConfig<string>('apiKey');

      if (!appId || !apiKey) {
        return this.createErrorResult('Application ID and API key are required');
      }

      console.log('Testing Algolia API connection...');

      try {
        // Test with list indices endpoint
        const response = await fetch(`https://${appId}-dsn.algolia.net/1/indexes`, {
          method: 'GET',
          headers: {
            'X-Algolia-Application-Id': appId,
            'X-Algolia-API-Key': apiKey,
          },
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Invalid application ID or API key');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Algolia', {
          appId,
          indicesCount: data.items?.length || 0,
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
      const appId = this.getConfig<string>('appId');
      const apiKey = this.getConfig<string>('apiKey');

      const options: RequestInit = {
        method,
        headers: {
          'X-Algolia-Application-Id': appId,
          'X-Algolia-API-Key': apiKey,
          'Content-Type': 'application/json',
        },
      };

      if (body && (method === 'POST' || method === 'PUT')) {
        options.body = JSON.stringify(body);
      }

      const url = `https://${appId}-dsn.algolia.net/1${endpoint}`;
      const response = await fetch(url, options);

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Algolia query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      appId: this.getConfig('appId'),
      index: this.getConfig('index'),
      hasCredentials: !!(this.getConfig('appId') && this.getConfig('apiKey')),
    };
  }

  /**
   * Search index
   */
  async search(indexName: string, query: string, params?: Record<string, any>): Promise<any> {
    return await this.query(`/indexes/${indexName}/query`, 'POST', { query, ...params });
  }

  /**
   * List indices
   */
  async listIndices(): Promise<any> {
    return await this.query('/indexes');
  }

  /**
   * Get object
   */
  async getObject(indexName: string, objectID: string): Promise<any> {
    return await this.query(`/indexes/${indexName}/${objectID}`);
  }

  /**
   * Add object
   */
  async addObject(indexName: string, object: any): Promise<any> {
    return await this.query(`/indexes/${indexName}`, 'POST', object);
  }

  /**
   * Update object
   */
  async updateObject(indexName: string, objectID: string, object: any): Promise<any> {
    return await this.query(`/indexes/${indexName}/${objectID}`, 'PUT', object);
  }

  /**
   * Delete object
   */
  async deleteObject(indexName: string, objectID: string): Promise<any> {
    return await this.query(`/indexes/${indexName}/${objectID}`, 'DELETE');
  }

  /**
   * Clear index
   */
  async clearIndex(indexName: string): Promise<any> {
    return await this.query(`/indexes/${indexName}/clear`, 'POST');
  }

  /**
   * Get index settings
   */
  async getSettings(indexName: string): Promise<any> {
    return await this.query(`/indexes/${indexName}/settings`);
  }
}
