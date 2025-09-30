// Elasticsearch Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class ElasticsearchAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const url = this.getConfig<string>('url', 'http://localhost:9200');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const apiKey = this.getConfig<string>('apiKey');
      const cloudId = this.getConfig<string>('cloudId');

      if (!url && !cloudId) {
        return this.createErrorResult('Either URL or Cloud ID is required');
      }

      console.log('Testing Elasticsearch connection...');

      try {
        // Determine the base URL
        let baseUrl = url;
        if (cloudId) {
          // For Elastic Cloud, decode the cloud ID to get the URL
          // Format: cluster_name:base64(deployment_url)
          const [, encodedUrl] = cloudId.split(':');
          if (encodedUrl) {
            try {
              const decodedUrl = atob(encodedUrl);
              baseUrl = `https://${decodedUrl.split('$')[0]}`;
            } catch {
              return this.createErrorResult('Invalid Cloud ID format');
            }
          }
        }

        // Prepare headers
        const headers: Record<string, string> = {
          'Content-Type': 'application/json',
        };

        if (apiKey) {
          headers['Authorization'] = `ApiKey ${apiKey}`;
        } else if (username && password) {
          const credentials = btoa(`${username}:${password}`);
          headers['Authorization'] = `Basic ${credentials}`;
        }

        // Test with cluster health endpoint
        const response = await fetch(`${baseUrl}/_cluster/health`, {
          method: 'GET',
          headers,
        });

        if (!response.ok) {
          if (response.status === 401) {
            throw new Error('Authentication failed. Check your credentials or API key.');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Elasticsearch cluster', {
          clusterName: data.cluster_name,
          status: data.status,
          numberOfNodes: data.number_of_nodes,
          activeShards: data.active_shards,
          relocatingShards: data.relocating_shards,
          initializingShards: data.initializing_shards,
          unassignedShards: data.unassigned_shards,
          activePrimaryShards: data.active_primary_shards,
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          if (fetchError.message.includes('Failed to fetch')) {
            return this.createErrorResult(`Cannot reach Elasticsearch at ${url}. Is the server running?`);
          }
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async queryIndex(index: string, queryBody: any): Promise<any> {
    try {
      const url = this.getConfig<string>('url');
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');
      const apiKey = this.getConfig<string>('apiKey');

      const headers: Record<string, string> = {
        'Content-Type': 'application/json',
      };

      if (apiKey) {
        headers['Authorization'] = `ApiKey ${apiKey}`;
      } else if (username && password) {
        const credentials = btoa(`${username}:${password}`);
        headers['Authorization'] = `Basic ${credentials}`;
      }

      const response = await fetch(`${url}/${index}/_search`, {
        method: 'POST',
        headers,
        body: JSON.stringify(queryBody),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Elasticsearch query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      url: this.getConfig('url'),
      cloudId: this.getConfig('cloudId') ? 'Configured' : 'Not configured',
      hasAuth: !!(this.getConfig('username') || this.getConfig('apiKey')),
    };
  }

  /**
   * Get cluster info
   */
  async getClusterInfo(): Promise<any> {
    const url = this.getConfig<string>('url');
    const headers = this.buildHeaders();

    const response = await fetch(`${url}/`, { headers });
    if (!response.ok) {
      throw new Error('Failed to get cluster info');
    }

    return await response.json();
  }

  /**
   * Get all indices
   */
  async getIndices(): Promise<any> {
    const url = this.getConfig<string>('url');
    const headers = this.buildHeaders();

    const response = await fetch(`${url}/_cat/indices?format=json`, { headers });
    if (!response.ok) {
      throw new Error('Failed to get indices');
    }

    return await response.json();
  }

  /**
   * Create an index
   */
  async createIndex(indexName: string, settings?: any): Promise<any> {
    const url = this.getConfig<string>('url');
    const headers = this.buildHeaders();

    const response = await fetch(`${url}/${indexName}`, {
      method: 'PUT',
      headers,
      body: settings ? JSON.stringify(settings) : undefined,
    });

    if (!response.ok) {
      throw new Error('Failed to create index');
    }

    return await response.json();
  }

  /**
   * Index a document
   */
  async indexDocument(index: string, document: any, id?: string): Promise<any> {
    const url = this.getConfig<string>('url');
    const headers = this.buildHeaders();

    const endpoint = id ? `${url}/${index}/_doc/${id}` : `${url}/${index}/_doc`;

    const response = await fetch(endpoint, {
      method: 'POST',
      headers,
      body: JSON.stringify(document),
    });

    if (!response.ok) {
      throw new Error('Failed to index document');
    }

    return await response.json();
  }

  /**
   * Build authorization headers
   */
  private buildHeaders(): Record<string, string> {
    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
    };

    const apiKey = this.getConfig<string>('apiKey');
    const username = this.getConfig<string>('username');
    const password = this.getConfig<string>('password');

    if (apiKey) {
      headers['Authorization'] = `ApiKey ${apiKey}`;
    } else if (username && password) {
      const credentials = btoa(`${username}:${password}`);
      headers['Authorization'] = `Basic ${credentials}`;
    }

    return headers;
  }
}
