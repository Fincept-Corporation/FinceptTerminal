// Google Cloud Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class GCPStorageAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const projectId = this.getConfig<string>('projectId');
      const credentials = this.getConfig<string>('credentials');
      const bucket = this.getConfig<string>('bucket');

      if (!projectId || !credentials) {
        return this.createErrorResult('Project ID and credentials are required');
      }

      // Validate JSON credentials format
      try {
        const creds = JSON.parse(credentials);
        if (!creds.type || !creds.project_id || !creds.private_key) {
          return this.createErrorResult('Invalid credentials format. Expected service account JSON.');
        }
      } catch {
        return this.createErrorResult('Credentials must be valid JSON');
      }

      console.log('Validating Google Cloud Storage configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for GCS project "${projectId}"`, {
        validatedAt: new Date().toISOString(),
        projectId,
        bucket: bucket || 'Not specified',
        hasCredentials: true,
        note: 'Client-side validation only. GCS operations require backend integration with @google-cloud/storage.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/gcs', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'gcs',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`GCS operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      projectId: this.getConfig('projectId'),
      bucket: this.getConfig('bucket'),
      hasCredentials: !!this.getConfig('credentials'),
    };
  }

  /**
   * List buckets
   */
  async listBuckets(): Promise<any> {
    return await this.query('listBuckets');
  }

  /**
   * List files in bucket
   */
  async listFiles(bucket?: string, prefix?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('listFiles', { bucket: bucketName, prefix });
  }

  /**
   * Get file
   */
  async getFile(fileName: string, bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('getFile', { bucket: bucketName, fileName });
  }

  /**
   * Upload file
   */
  async uploadFile(fileName: string, data: string | Buffer, bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('uploadFile', { bucket: bucketName, fileName, data });
  }

  /**
   * Delete file
   */
  async deleteFile(fileName: string, bucket?: string): Promise<any> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    return await this.query('deleteFile', { bucket: bucketName, fileName });
  }

  /**
   * Get signed URL
   */
  async getSignedUrl(fileName: string, expiresIn: number = 3600, bucket?: string): Promise<string> {
    const bucketName = bucket || this.getConfig<string>('bucket');
    const result = await this.query('getSignedUrl', {
      bucket: bucketName,
      fileName,
      expiresIn,
    });
    return result.url;
  }
}
