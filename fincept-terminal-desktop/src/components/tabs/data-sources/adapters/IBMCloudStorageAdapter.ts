// IBM Cloud Object Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class IBMCloudStorageAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const endpoint = this.getConfig<string>('endpoint');
      const bucket = this.getConfig<string>('bucket');
      const apiKey = this.getConfig<string>('apiKey');
      const serviceInstanceId = this.getConfig<string>('serviceInstanceId');

      if (!endpoint || !bucket || !apiKey || !serviceInstanceId) {
        return this.createErrorResult('Endpoint, bucket, API key, and service instance ID are required');
      }

      console.log('Validating IBM Cloud Storage configuration (client-side only)');

      // IBM Cloud uses S3-compatible API with IAM authentication
      return this.createSuccessResult(`Configuration validated for IBM Cloud bucket "${bucket}"`, {
        validatedAt: new Date().toISOString(),
        endpoint,
        bucket,
        serviceInstanceId,
        hasCredentials: true,
        note: 'Client-side validation only. IBM Cloud Storage requires backend integration with IBM Cloud SDK or AWS SDK with IAM.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // IBM Cloud operations need to go through backend
      const response = await fetch('/api/data-sources/ibm-cloud-storage', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'ibm-cloud-storage',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`IBM Cloud Storage operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      endpoint: this.getConfig('endpoint'),
      bucket: this.getConfig('bucket'),
      hasCredentials: !!(this.getConfig('apiKey') && this.getConfig('serviceInstanceId')),
    };
  }

  /**
   * List objects
   */
  async listObjects(prefix?: string, maxKeys?: number): Promise<any> {
    return await this.query('listObjects', { prefix, maxKeys });
  }

  /**
   * Get object
   */
  async getObject(key: string): Promise<any> {
    return await this.query('getObject', { key });
  }

  /**
   * Put object
   */
  async putObject(key: string, body: any, metadata?: Record<string, any>): Promise<any> {
    return await this.query('putObject', { key, body, metadata });
  }

  /**
   * Delete object
   */
  async deleteObject(key: string): Promise<any> {
    return await this.query('deleteObject', { key });
  }

  /**
   * Copy object
   */
  async copyObject(sourceKey: string, destinationKey: string): Promise<any> {
    return await this.query('copyObject', { sourceKey, destinationKey });
  }

  /**
   * Get object metadata
   */
  async headObject(key: string): Promise<any> {
    return await this.query('headObject', { key });
  }

  /**
   * List buckets
   */
  async listBuckets(): Promise<any> {
    return await this.query('listBuckets');
  }

  /**
   * Get presigned URL
   */
  async getPresignedUrl(key: string, expiresIn: number = 3600): Promise<any> {
    return await this.query('getPresignedUrl', { key, expiresIn });
  }

  /**
   * Create multipart upload
   */
  async createMultipartUpload(key: string): Promise<any> {
    return await this.query('createMultipartUpload', { key });
  }

  /**
   * Get bucket CORS
   */
  async getBucketCors(): Promise<any> {
    return await this.query('getBucketCors');
  }
}
