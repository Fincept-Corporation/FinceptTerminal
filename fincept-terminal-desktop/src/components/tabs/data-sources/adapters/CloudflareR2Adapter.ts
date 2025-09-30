// Cloudflare R2 Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CloudflareR2Adapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const accountId = this.getConfig<string>('accountId');
      const bucket = this.getConfig<string>('bucket');
      const accessKeyId = this.getConfig<string>('accessKeyId');
      const secretAccessKey = this.getConfig<string>('secretAccessKey');

      if (!accountId || !bucket || !accessKeyId || !secretAccessKey) {
        return this.createErrorResult('Account ID, bucket, access key ID, and secret access key are required');
      }

      console.log('Validating Cloudflare R2 configuration (client-side only)');

      // R2 uses S3-compatible API
      return this.createSuccessResult(`Configuration validated for Cloudflare R2 bucket "${bucket}"`, {
        validatedAt: new Date().toISOString(),
        accountId,
        bucket,
        endpoint: `https://${accountId}.r2.cloudflarestorage.com`,
        hasCredentials: true,
        note: 'Client-side validation only. Cloudflare R2 requires backend integration with AWS SDK.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // R2 operations need to go through backend
      const response = await fetch('/api/data-sources/cloudflare-r2', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'cloudflare-r2',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Cloudflare R2 operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      accountId: this.getConfig('accountId'),
      bucket: this.getConfig('bucket'),
      endpoint: `https://${this.getConfig('accountId')}.r2.cloudflarestorage.com`,
      hasCredentials: !!(this.getConfig('accessKeyId') && this.getConfig('secretAccessKey')),
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
   * Create multipart upload
   */
  async createMultipartUpload(key: string): Promise<any> {
    return await this.query('createMultipartUpload', { key });
  }

  /**
   * Generate presigned URL
   */
  async getPresignedUrl(key: string, expiresIn: number = 3600): Promise<any> {
    return await this.query('getPresignedUrl', { key, expiresIn });
  }

  /**
   * List buckets
   */
  async listBuckets(): Promise<any> {
    return await this.query('listBuckets');
  }

  /**
   * Delete multiple objects
   */
  async deleteObjects(keys: string[]): Promise<any> {
    return await this.query('deleteObjects', { keys });
  }
}
