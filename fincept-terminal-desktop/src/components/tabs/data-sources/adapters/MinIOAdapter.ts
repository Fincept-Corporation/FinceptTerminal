// MinIO S3-Compatible Object Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class MinIOAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const endpoint = this.getConfig<string>('endpoint');
      const bucket = this.getConfig<string>('bucket');
      const accessKey = this.getConfig<string>('accessKey');
      const secretKey = this.getConfig<string>('secretKey');

      if (!endpoint || !bucket || !accessKey || !secretKey) {
        return this.createErrorResult('Endpoint, bucket, access key, and secret key are required');
      }

      console.log('Validating MinIO configuration (client-side only)');

      // MinIO uses S3-compatible API, actual testing requires AWS SDK
      return this.createSuccessResult(`Configuration validated for MinIO bucket "${bucket}"`, {
        validatedAt: new Date().toISOString(),
        endpoint,
        bucket,
        hasCredentials: true,
        note: 'Client-side validation only. MinIO requires backend integration with AWS SDK or MinIO client.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // MinIO operations need to go through backend
      const response = await fetch('/api/data-sources/minio', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'minio',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`MinIO operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      endpoint: this.getConfig('endpoint'),
      bucket: this.getConfig('bucket'),
      hasCredentials: !!(this.getConfig('accessKey') && this.getConfig('secretKey')),
    };
  }

  /**
   * List buckets
   */
  async listBuckets(): Promise<any> {
    return await this.query('listBuckets');
  }

  /**
   * List objects in bucket
   */
  async listObjects(prefix?: string, recursive?: boolean): Promise<any> {
    return await this.query('listObjects', { prefix, recursive });
  }

  /**
   * Get object
   */
  async getObject(objectName: string): Promise<any> {
    return await this.query('getObject', { objectName });
  }

  /**
   * Put object
   */
  async putObject(objectName: string, data: any, metadata?: Record<string, any>): Promise<any> {
    return await this.query('putObject', { objectName, data, metadata });
  }

  /**
   * Delete object
   */
  async removeObject(objectName: string): Promise<any> {
    return await this.query('removeObject', { objectName });
  }

  /**
   * Copy object
   */
  async copyObject(sourceObject: string, destObject: string): Promise<any> {
    return await this.query('copyObject', { sourceObject, destObject });
  }

  /**
   * Get object stats
   */
  async statObject(objectName: string): Promise<any> {
    return await this.query('statObject', { objectName });
  }

  /**
   * Get presigned URL
   */
  async presignedGetObject(objectName: string, expires: number = 604800): Promise<any> {
    return await this.query('presignedGetObject', { objectName, expires });
  }

  /**
   * Create bucket
   */
  async makeBucket(bucketName: string, region?: string): Promise<any> {
    return await this.query('makeBucket', { bucketName, region });
  }

  /**
   * Remove bucket
   */
  async removeBucket(bucketName: string): Promise<any> {
    return await this.query('removeBucket', { bucketName });
  }

  /**
   * Check if bucket exists
   */
  async bucketExists(bucketName: string): Promise<any> {
    return await this.query('bucketExists', { bucketName });
  }
}
