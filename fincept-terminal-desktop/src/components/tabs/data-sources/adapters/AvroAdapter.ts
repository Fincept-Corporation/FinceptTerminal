// Apache Avro File Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class AvroAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const filepath = this.getConfig<string>('filepath');

      if (!filepath) {
        return this.createErrorResult('File path is required');
      }

      console.log('Testing Avro file accessibility...');

      try {
        // Check if file is accessible via HEAD request
        const response = await fetch(filepath, { method: 'HEAD' });

        if (!response.ok) {
          throw new Error(`Cannot access Avro file: ${response.status} ${response.statusText}`);
        }

        const contentLength = response.headers.get('content-length');
        const contentType = response.headers.get('content-type');

        return this.createSuccessResult('Successfully accessed Avro file', {
          filepath,
          fileSize: contentLength ? `${contentLength} bytes` : 'Unknown',
          contentType: contentType || 'Unknown',
          note: 'Avro parsing requires backend integration with avro-js or Apache Avro libraries.',
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
      // Avro operations need to go through backend
      const response = await fetch('/api/data-sources/avro', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'avro',
          filepath: this.getConfig('filepath'),
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Avro operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      filepath: this.getConfig('filepath'),
      provider: 'Apache Avro',
    };
  }

  /**
   * Get Avro schema
   */
  async getSchema(): Promise<any> {
    return await this.query('getSchema');
  }

  /**
   * Read all records
   */
  async readRecords(limit?: number, offset?: number): Promise<any> {
    return await this.query('readRecords', { limit, offset });
  }

  /**
   * Get record count
   */
  async getRecordCount(): Promise<any> {
    return await this.query('getRecordCount');
  }

  /**
   * Get file metadata
   */
  async getFileMetadata(): Promise<any> {
    return await this.query('getFileMetadata');
  }

  /**
   * Convert to JSON
   */
  async toJSON(limit?: number): Promise<any> {
    return await this.query('toJSON', { limit });
  }

  /**
   * Get codec information
   */
  async getCodec(): Promise<any> {
    return await this.query('getCodec');
  }

  /**
   * Get sync marker
   */
  async getSyncMarker(): Promise<any> {
    return await this.query('getSyncMarker');
  }
}
