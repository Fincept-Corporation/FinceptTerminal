// Amazon S3 Cloud Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class S3Adapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const accessKeyId = this.getConfig<string>('accessKeyId');
      const secretAccessKey = this.getConfig<string>('secretAccessKey');
      const region = this.getConfig<string>('region', 'us-east-1');
      const bucket = this.getConfig<string>('bucket');

      if (!accessKeyId || !secretAccessKey) {
        return this.createErrorResult('Access Key ID and Secret Access Key are required');
      }

      console.log('Validating AWS S3 configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for S3${bucket ? ` bucket "${bucket}"` : ''}`,
        {
          validatedAt: new Date().toISOString(),
          region,
          bucket,
          hasCredentials: true,
          note: 'Client-side validation only. S3 operations require backend integration with AWS SDK.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/s3', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 's3',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`S3 operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      region: this.getConfig('region', 'us-east-1'),
      bucket: this.getConfig('bucket'),
      hasCredentials: !!(this.getConfig('accessKeyId') && this.getConfig('secretAccessKey')),
    };
  }

  /**
   * List buckets
   */
  async listBuckets(): Promise<any> {
    return await this.query('listBuckets');
  }

  /**
   * List objects in a bucket
   */
  async listObjects(bucket?: string, prefix?: string, maxKeys: number = 1000): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('listObjects', { bucket: bucketName, prefix, maxKeys });
  }

  /**
   * Get object metadata
   */
  async getObjectMetadata(key: string, bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('getObjectMetadata', { bucket: bucketName, key });
  }

  /**
   * Get object (download)
   */
  async getObject(key: string, bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('getObject', { bucket: bucketName, key });
  }

  /**
   * Put object (upload)
   */
  async putObject(key: string, data: string | ArrayBuffer, bucket?: string, contentType?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('putObject', {
      bucket: bucketName,
      key,
      data,
      contentType,
    });
  }

  /**
   * Delete object
   */
  async deleteObject(key: string, bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('deleteObject', { bucket: bucketName, key });
  }

  /**
   * Delete multiple objects
   */
  async deleteObjects(keys: string[], bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('deleteObjects', { bucket: bucketName, keys });
  }

  /**
   * Copy object
   */
  async copyObject(sourceKey: string, destinationKey: string, bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('copyObject', {
      bucket: bucketName,
      sourceKey,
      destinationKey,
    });
  }

  /**
   * Get presigned URL for object
   */
  async getPresignedUrl(key: string, expiresIn: number = 3600, bucket?: string): Promise<string> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    const result = await this.query('getPresignedUrl', {
      bucket: bucketName,
      key,
      expiresIn,
    });
    return result.url;
  }

  /**
   * Create bucket
   */
  async createBucket(bucketName: string, region?: string): Promise<any> {
    const bucketRegion = region || this.getConfig<string>('region', 'us-east-1');
    return await this.query('createBucket', { bucket: bucketName, region: bucketRegion });
  }

  /**
   * Delete bucket
   */
  async deleteBucket(bucketName: string): Promise<any> {
    return await this.query('deleteBucket', { bucket: bucketName });
  }

  /**
   * Get bucket versioning status
   */
  async getBucketVersioning(bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('getBucketVersioning', { bucket: bucketName });
  }

  /**
   * Set bucket versioning
   */
  async setBucketVersioning(enabled: boolean, bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('setBucketVersioning', { bucket: bucketName, enabled });
  }
}
