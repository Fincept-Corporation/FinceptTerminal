// AWS DynamoDB Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class DynamoDBAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const region = this.getConfig<string>('region');
      const accessKeyId = this.getConfig<string>('accessKeyId');
      const secretAccessKey = this.getConfig<string>('secretAccessKey');

      if (!region || !accessKeyId || !secretAccessKey) {
        return this.createErrorResult('Region, access key ID, and secret access key are required');
      }

      console.log('Validating DynamoDB configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for DynamoDB in ${region}`, {
        validatedAt: new Date().toISOString(),
        region,
        note: 'Client-side validation only. DynamoDB requires backend integration with AWS SDK.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/dynamodb', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'dynamodb',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`DynamoDB query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      region: this.getConfig('region'),
      hasCredentials: !!(this.getConfig('accessKeyId') && this.getConfig('secretAccessKey')),
    };
  }

  async listTables(): Promise<any> {
    return await this.query('listTables');
  }

  async getItem(tableName: string, key: Record<string, any>): Promise<any> {
    return await this.query('getItem', { tableName, key });
  }

  async putItem(tableName: string, item: Record<string, any>): Promise<any> {
    return await this.query('putItem', { tableName, item });
  }

  async deleteItem(tableName: string, key: Record<string, any>): Promise<any> {
    return await this.query('deleteItem', { tableName, key });
  }

  async scan(tableName: string, filter?: Record<string, any>): Promise<any> {
    return await this.query('scan', { tableName, filter });
  }

  async queryTable(tableName: string, keyCondition: Record<string, any>): Promise<any> {
    return await this.query('query', { tableName, keyCondition });
  }
}
