// DigitalOcean Spaces Object Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class DigitalOceanSpacesAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const region = this.getConfig<string>('region');
      const space = this.getConfig<string>('space');
      const accessKey = this.getConfig<string>('accessKey');
      const secretKey = this.getConfig<string>('secretKey');

      if (!region || !space || !accessKey || !secretKey) {
        return this.createErrorResult('Region, space name, access key, and secret key are required');
      }

      console.log('Validating DigitalOcean Spaces configuration (client-side only)');

      // DigitalOcean Spaces uses S3-compatible API
      // Actual testing requires AWS SDK with proper signing
      return this.createSuccessResult(`Configuration validated for DigitalOcean Space "${space}" in ${region}`, {
        validatedAt: new Date().toISOString(),
        region,
        space,
        endpoint: `https://${region}.digitaloceanspaces.com`,
        hasCredentials: true,
        note: 'Client-side validation only. DigitalOcean Spaces requires backend integration with AWS SDK.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // Spaces operations need to go through backend
      const response = await fetch('/api/data-sources/digitalocean-spaces', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'digitalocean-spaces',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`DigitalOcean Spaces operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      region: this.getConfig('region'),
      space: this.getConfig('space'),
      endpoint: `https://${this.getConfig('region')}.digitaloceanspaces.com`,
      hasCredentials: !!(this.getConfig('accessKey') && this.getConfig('secretKey')),
    };
  }

  /**
   * List objects in space
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
  async getObjectMetadata(key: string): Promise<any> {
    return await this.query('getObjectMetadata', { key });
  }

  /**
   * Get presigned URL
   */
  async getPresignedUrl(key: string, expiresIn: number = 3600): Promise<any> {
    return await this.query('getPresignedUrl', { key, expiresIn });
  }

  /**
   * Set object ACL
   */
  async setObjectACL(key: string, acl: 'private' | 'public-read'): Promise<any> {
    return await this.query('setObjectACL', { key, acl });
  }
}
