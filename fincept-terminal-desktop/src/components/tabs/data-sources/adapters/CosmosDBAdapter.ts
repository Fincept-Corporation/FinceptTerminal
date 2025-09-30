// Azure Cosmos DB Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CosmosDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const endpoint = this.getConfig<string>('endpoint');
      const key = this.getConfig<string>('key');

      if (!endpoint || !key) {
        return this.createErrorResult('Endpoint and key are required');
      }

      console.log('Validating Cosmos DB configuration (client-side only)');

      return this.createSuccessResult('Configuration validated for Azure Cosmos DB', {
        validatedAt: new Date().toISOString(),
        endpoint,
        note: 'Client-side validation only. Cosmos DB requires backend integration with @azure/cosmos SDK.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/cosmosdb', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'cosmosdb',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Cosmos DB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      endpoint: this.getConfig('endpoint'),
      database: this.getConfig('database'),
      hasCredentials: !!this.getConfig('key'),
    };
  }

  async getItem(container: string, itemId: string, partitionKey: string): Promise<any> {
    return await this.query('getItem', { container, itemId, partitionKey });
  }

  async createItem(container: string, item: any): Promise<any> {
    return await this.query('createItem', { container, item });
  }

  async updateItem(container: string, itemId: string, item: any, partitionKey: string): Promise<any> {
    return await this.query('updateItem', { container, itemId, item, partitionKey });
  }

  async deleteItem(container: string, itemId: string, partitionKey: string): Promise<any> {
    return await this.query('deleteItem', { container, itemId, partitionKey });
  }

  async queryItems(container: string, sqlQuery: string): Promise<any> {
    return await this.query('queryItems', { container, sqlQuery });
  }
}
