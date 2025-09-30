// OData Service Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class ODataAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const serviceRoot = this.getConfig<string>('serviceRoot');

      if (!serviceRoot) {
        return this.createErrorResult('Service root URL is required');
      }

      console.log('Testing OData service connection...');

      try {
        // Test with service metadata endpoint
        const metadataUrl = `${serviceRoot}/$metadata`;
        const response = await fetch(metadataUrl);

        if (!response.ok) {
          throw new Error(`Cannot access OData service: ${response.status} ${response.statusText}`);
        }

        const metadataText = await response.text();

        // Check if it's valid OData metadata
        if (!metadataText.includes('edmx:Edmx') && !metadataText.includes('Edmx')) {
          throw new Error('Invalid OData metadata format');
        }

        // Parse metadata to extract entity sets
        const parser = new DOMParser();
        const metadataDoc = parser.parseFromString(metadataText, 'text/xml');

        const entitySets = metadataDoc.querySelectorAll('EntitySet');
        const entitySetNames = Array.from(entitySets).map((es) => es.getAttribute('Name')).filter(Boolean);

        return this.createSuccessResult('Successfully connected to OData service', {
          serviceRoot,
          version: metadataText.includes('Version="4.0"') ? 'OData v4' : 'OData v3 or lower',
          entitySets: entitySetNames.length,
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

  async query(endpoint: string, params?: Record<string, any>): Promise<any> {
    try {
      const serviceRoot = this.getConfig<string>('serviceRoot');
      const version = this.getConfig<string>('version', '4.0');

      const queryParams = params ? `?${new URLSearchParams(params).toString()}` : '';
      const url = `${serviceRoot}${endpoint}${queryParams}`;

      const headers: Record<string, string> = {
        Accept: 'application/json',
        'OData-Version': version,
      };

      const response = await fetch(url, {
        method: 'GET',
        headers,
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`OData query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      serviceRoot: this.getConfig('serviceRoot'),
      version: this.getConfig('version', '4.0'),
      provider: 'OData',
    };
  }

  /**
   * Get service metadata
   */
  async getServiceMetadata(): Promise<any> {
    const serviceRoot = this.getConfig<string>('serviceRoot');
    const response = await fetch(`${serviceRoot}/$metadata`);
    const metadata = await response.text();
    return { metadata };
  }

  /**
   * Query entity set
   */
  async queryEntitySet(entitySet: string, options?: {
    $select?: string;
    $filter?: string;
    $orderby?: string;
    $top?: number;
    $skip?: number;
    $expand?: string;
  }): Promise<any> {
    const params: Record<string, string> = {};

    if (options?.$select) params.$select = options.$select;
    if (options?.$filter) params.$filter = options.$filter;
    if (options?.$orderby) params.$orderby = options.$orderby;
    if (options?.$top) params.$top = options.$top.toString();
    if (options?.$skip) params.$skip = options.$skip.toString();
    if (options?.$expand) params.$expand = options.$expand;

    return await this.query(`/${entitySet}`, params);
  }

  /**
   * Get entity by key
   */
  async getEntity(entitySet: string, key: string | number, expand?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (expand) params.$expand = expand;

    return await this.query(`/${entitySet}(${key})`, params);
  }

  /**
   * Count entities
   */
  async countEntities(entitySet: string, filter?: string): Promise<any> {
    const params: Record<string, string> = {};
    if (filter) params.$filter = filter;

    return await this.query(`/${entitySet}/$count`, params);
  }

  /**
   * Create entity
   */
  async createEntity(entitySet: string, entity: any): Promise<any> {
    const serviceRoot = this.getConfig<string>('serviceRoot');
    const version = this.getConfig<string>('version', '4.0');

    const response = await fetch(`${serviceRoot}/${entitySet}`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Accept: 'application/json',
        'OData-Version': version,
      },
      body: JSON.stringify(entity),
    });

    if (!response.ok) {
      throw new Error(`Create failed: ${response.status} ${response.statusText}`);
    }

    return await response.json();
  }

  /**
   * Update entity
   */
  async updateEntity(entitySet: string, key: string | number, entity: any): Promise<any> {
    const serviceRoot = this.getConfig<string>('serviceRoot');
    const version = this.getConfig<string>('version', '4.0');

    const response = await fetch(`${serviceRoot}/${entitySet}(${key})`, {
      method: 'PATCH',
      headers: {
        'Content-Type': 'application/json',
        Accept: 'application/json',
        'OData-Version': version,
      },
      body: JSON.stringify(entity),
    });

    if (!response.ok) {
      throw new Error(`Update failed: ${response.status} ${response.statusText}`);
    }

    return response.status === 204 ? { success: true } : await response.json();
  }

  /**
   * Delete entity
   */
  async deleteEntity(entitySet: string, key: string | number): Promise<any> {
    const serviceRoot = this.getConfig<string>('serviceRoot');
    const version = this.getConfig<string>('version', '4.0');

    const response = await fetch(`${serviceRoot}/${entitySet}(${key})`, {
      method: 'DELETE',
      headers: {
        'OData-Version': version,
      },
    });

    if (!response.ok) {
      throw new Error(`Delete failed: ${response.status} ${response.statusText}`);
    }

    return { success: true };
  }

  /**
   * Call function
   */
  async callFunction(functionName: string, params?: Record<string, any>): Promise<any> {
    return await this.query(`/${functionName}`, params);
  }

  /**
   * Call action
   */
  async callAction(actionName: string, params?: any): Promise<any> {
    const serviceRoot = this.getConfig<string>('serviceRoot');
    const version = this.getConfig<string>('version', '4.0');

    const response = await fetch(`${serviceRoot}/${actionName}`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Accept: 'application/json',
        'OData-Version': version,
      },
      body: params ? JSON.stringify(params) : undefined,
    });

    if (!response.ok) {
      throw new Error(`Action call failed: ${response.status} ${response.statusText}`);
    }

    return response.status === 204 ? { success: true } : await response.json();
  }
}
