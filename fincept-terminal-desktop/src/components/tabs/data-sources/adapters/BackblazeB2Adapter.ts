// Backblaze B2 Cloud Storage Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class BackblazeB2Adapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const keyId = this.getConfig<string>('keyId');
      const applicationKey = this.getConfig<string>('applicationKey');

      if (!keyId || !applicationKey) {
        return this.createErrorResult('Application key ID and application key are required');
      }

      console.log('Testing Backblaze B2 API connection...');

      try {
        // Authorize with B2 API
        const credentials = btoa(`${keyId}:${applicationKey}`);
        const response = await fetch('https://api.backblazeb2.com/b2api/v2/b2_authorize_account', {
          method: 'GET',
          headers: {
            Authorization: `Basic ${credentials}`,
          },
        });

        if (!response.ok) {
          if (response.status === 401) {
            throw new Error('Invalid application key ID or key');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        return this.createSuccessResult('Successfully connected to Backblaze B2', {
          accountId: data.accountId,
          apiUrl: data.apiUrl,
          downloadUrl: data.downloadUrl,
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // B2 operations need to go through backend for proper auth flow
      const response = await fetch('/api/data-sources/backblaze-b2', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'backblaze-b2',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Backblaze B2 operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      bucketId: this.getConfig('bucketId'),
      hasCredentials: !!(this.getConfig('keyId') && this.getConfig('applicationKey')),
      provider: 'Backblaze B2',
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
  async listFileNames(bucketId?: string, prefix?: string, maxFileCount?: number): Promise<any> {
    return await this.query('listFileNames', {
      bucketId: bucketId || this.getConfig('bucketId'),
      prefix,
      maxFileCount,
    });
  }

  /**
   * Upload file
   */
  async uploadFile(fileName: string, data: any, contentType?: string): Promise<any> {
    return await this.query('uploadFile', { fileName, data, contentType });
  }

  /**
   * Download file by name
   */
  async downloadFileByName(fileName: string): Promise<any> {
    return await this.query('downloadFileByName', { fileName });
  }

  /**
   * Delete file version
   */
  async deleteFileVersion(fileName: string, fileId: string): Promise<any> {
    return await this.query('deleteFileVersion', { fileName, fileId });
  }

  /**
   * Get file info
   */
  async getFileInfo(fileId: string): Promise<any> {
    return await this.query('getFileInfo', { fileId });
  }

  /**
   * Get download authorization
   */
  async getDownloadAuthorization(fileNamePrefix: string, validDurationInSeconds: number = 3600): Promise<any> {
    return await this.query('getDownloadAuthorization', {
      fileNamePrefix,
      validDurationInSeconds,
    });
  }

  /**
   * Copy file
   */
  async copyFile(sourceFileId: string, destinationFileName: string): Promise<any> {
    return await this.query('copyFile', { sourceFileId, destinationFileName });
  }
}
