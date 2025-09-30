// Azure Blob Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class AzureBlobAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const accountName = this.getConfig<string>('accountName');
      const accountKey = this.getConfig<string>('accountKey');
      const connectionString = this.getConfig<string>('connectionString');
      const container = this.getConfig<string>('container');

      if (!accountName && !connectionString) {
        return this.createErrorResult('Either account name or connection string is required');
      }

      if (accountName && !accountKey && !connectionString) {
        return this.createErrorResult('Account key is required when using account name');
      }

      console.log('Validating Azure Blob Storage configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for Azure Blob Storage account "${accountName || 'from connection string'}"`, {
        validatedAt: new Date().toISOString(),
        accountName: accountName || 'From connection string',
        container: container || 'Not specified',
        hasCredentials: true,
        note: 'Client-side validation only. Azure Blob operations require backend integration with @azure/storage-blob.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/azure-blob', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'azure-blob',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Azure Blob operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      accountName: this.getConfig('accountName'),
      container: this.getConfig('container'),
      hasCredentials: !!(this.getConfig('accountKey') || this.getConfig('connectionString')),
    };
  }

  /**
   * List containers
   */
  async listContainers(): Promise<any> {
    return await this.query('listContainers');
  }

  /**
   * List blobs in container
   */
  async listBlobs(container?: string, prefix?: string): Promise<any> {
    const containerName = container || this.getConfig<string>('container');
    return await this.query('listBlobs', { container: containerName, prefix });
  }

  /**
   * Get blob
   */
  async getBlob(blobName: string, container?: string): Promise<any> {
    const containerName = container || this.getConfig<string>('container');
    return await this.query('getBlob', { container: containerName, blobName });
  }

  /**
   * Upload blob
   */
  async uploadBlob(blobName: string, data: string | Buffer, container?: string): Promise<any> {
    const containerName = container || this.getConfig<string>('container');
    return await this.query('uploadBlob', { container: containerName, blobName, data });
  }

  /**
   * Delete blob
   */
  async deleteBlob(blobName: string, container?: string): Promise<any> {
    const containerName = container || this.getConfig<string>('container');
    return await this.query('deleteBlob', { container: containerName, blobName });
  }

  /**
   * Get blob properties
   */
  async getBlobProperties(blobName: string, container?: string): Promise<any> {
    const containerName = container || this.getConfig<string>('container');
    return await this.query('getBlobProperties', { container: containerName, blobName });
  }

  /**
   * Copy blob
   */
  async copyBlob(sourceBlobName: string, destBlobName: string, container?: string): Promise<any> {
    const containerName = container || this.getConfig<string>('container');
    return await this.query('copyBlob', {
      container: containerName,
      sourceBlobName,
      destBlobName,
    });
  }
}
