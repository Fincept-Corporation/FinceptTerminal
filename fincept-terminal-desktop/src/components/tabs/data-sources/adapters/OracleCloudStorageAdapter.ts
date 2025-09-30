// Oracle Cloud Infrastructure Object Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class OracleCloudStorageAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const region = this.getConfig<string>('region');
      const namespace = this.getConfig<string>('namespace');
      const bucket = this.getConfig<string>('bucket');
      const accessKey = this.getConfig<string>('accessKey');
      const secretKey = this.getConfig<string>('secretKey');

      if (!region || !namespace || !bucket || !accessKey || !secretKey) {
        return this.createErrorResult('Region, namespace, bucket, access key, and secret key are required');
      }

      console.log('Validating Oracle Cloud Storage configuration (client-side only)');

      // Oracle Cloud uses S3-compatible API
      return this.createSuccessResult(`Configuration validated for Oracle Cloud bucket "${bucket}" in ${region}`, {
        validatedAt: new Date().toISOString(),
        region,
        namespace,
        bucket,
        endpoint: `https://${namespace}.compat.objectstorage.${region}.oraclecloud.com`,
        hasCredentials: true,
        note: 'Client-side validation only. Oracle Cloud Storage requires backend integration with AWS SDK.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // Oracle Cloud operations need to go through backend
      const response = await fetch('/api/data-sources/oracle-cloud-storage', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'oracle-cloud-storage',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Oracle Cloud Storage operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      region: this.getConfig('region'),
      namespace: this.getConfig('namespace'),
      bucket: this.getConfig('bucket'),
      endpoint: `https://${this.getConfig('namespace')}.compat.objectstorage.${this.getConfig('region')}.oraclecloud.com`,
      hasCredentials: !!(this.getConfig('accessKey') && this.getConfig('secretKey')),
    };
  }

  /**
   * List objects
   */
  async listObjects(prefix?: string, delimiter?: string): Promise<any> {
    return await this.query('listObjects', { prefix, delimiter });
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
  async deleteObject(objectName: string): Promise<any> {
    return await this.query('deleteObject', { objectName });
  }

  /**
   * Copy object
   */
  async copyObject(sourceObject: string, destinationObject: string): Promise<any> {
    return await this.query('copyObject', { sourceObject, destinationObject });
  }

  /**
   * Get object metadata
   */
  async headObject(objectName: string): Promise<any> {
    return await this.query('headObject', { objectName });
  }

  /**
   * List buckets
   */
  async listBuckets(): Promise<any> {
    return await this.query('listBuckets');
  }

  /**
   * Create bucket
   */
  async createBucket(bucketName: string): Promise<any> {
    return await this.query('createBucket', { bucketName });
  }

  /**
   * Delete bucket
   */
  async deleteBucket(bucketName: string): Promise<any> {
    return await this.query('deleteBucket', { bucketName });
  }

  /**
   * Create pre-authenticated request
   */
  async createPreauthRequest(objectName: string, expiresIn: number = 3600): Promise<any> {
    return await this.query('createPreauthRequest', { objectName, expiresIn });
  }
}
