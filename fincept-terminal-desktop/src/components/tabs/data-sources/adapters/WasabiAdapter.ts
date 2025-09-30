// Wasabi Hot Cloud Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class WasabiAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const region = this.getConfig<string>('region');
      const bucket = this.getConfig<string>('bucket');
      const accessKey = this.getConfig<string>('accessKey');
      const secretKey = this.getConfig<string>('secretKey');

      if (!region || !bucket || !accessKey || !secretKey) {
        return this.createErrorResult('Region, bucket, access key, and secret key are required');
      }

      console.log('Validating Wasabi configuration (client-side only)');

      // Wasabi uses S3-compatible API
      return this.createSuccessResult(`Configuration validated for Wasabi bucket "${bucket}" in ${region}`, {
        validatedAt: new Date().toISOString(),
        region,
        bucket,
        endpoint: `https://s3.${region}.wasabisys.com`,
        hasCredentials: true,
        note: 'Client-side validation only. Wasabi requires backend integration with AWS SDK.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // Wasabi operations need to go through backend
      const response = await fetch('/api/data-sources/wasabi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'wasabi',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Wasabi operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      region: this.getConfig('region'),
      bucket: this.getConfig('bucket'),
      endpoint: `https://s3.${this.getConfig('region')}.wasabisys.com`,
      hasCredentials: !!(this.getConfig('accessKey') && this.getConfig('secretKey')),
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
  async putObject(key: string, body: any, contentType?: string): Promise<any> {
    return await this.query('putObject', { key, body, contentType });
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
   * Get presigned URL
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
}
