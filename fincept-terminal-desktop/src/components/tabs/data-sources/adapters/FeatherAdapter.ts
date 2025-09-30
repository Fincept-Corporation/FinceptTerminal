// Apache Arrow Feather File Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class FeatherAdapter extends BaseAdapter {
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

      console.log('Testing Feather file accessibility...');

      try {
        // Check if file is accessible via HEAD request
        const response = await fetch(filepath, { method: 'HEAD' });

        if (!response.ok) {
          throw new Error(`Cannot access Feather file: ${response.status} ${response.statusText}`);
        }

        const contentLength = response.headers.get('content-length');
        const contentType = response.headers.get('content-type');

        return this.createSuccessResult('Successfully accessed Feather file', {
          filepath,
          fileSize: contentLength ? `${contentLength} bytes` : 'Unknown',
          contentType: contentType || 'Unknown',
          note: 'Feather parsing requires backend integration with Apache Arrow libraries.',
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
      // Feather operations need to go through backend
      const response = await fetch('/api/data-sources/feather', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'feather',
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
      throw new Error(`Feather operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      filepath: this.getConfig('filepath'),
      provider: 'Apache Arrow Feather',
    };
  }

  /**
   * Get Feather schema
   */
  async getSchema(): Promise<any> {
    return await this.query('getSchema');
  }

  /**
   * Read table
   */
  async readTable(limit?: number, offset?: number): Promise<any> {
    return await this.query('readTable', { limit, offset });
  }

  /**
   * Get row count
   */
  async getRowCount(): Promise<any> {
    return await this.query('getRowCount');
  }

  /**
   * Get column count
   */
  async getColumnCount(): Promise<any> {
    return await this.query('getColumnCount');
  }

  /**
   * Get column names
   */
  async getColumnNames(): Promise<any> {
    return await this.query('getColumnNames');
  }

  /**
   * Read specific columns
   */
  async readColumns(columns: string[], limit?: number): Promise<any> {
    return await this.query('readColumns', { columns, limit });
  }

  /**
   * Convert to JSON
   */
  async toJSON(limit?: number): Promise<any> {
    return await this.query('toJSON', { limit });
  }

  /**
   * Get Arrow metadata
   */
  async getArrowMetadata(): Promise<any> {
    return await this.query('getArrowMetadata');
  }

  /**
   * Get file version
   */
  async getFileVersion(): Promise<any> {
    return await this.query('getFileVersion');
  }

  /**
   * Get data types
   */
  async getDataTypes(): Promise<any> {
    return await this.query('getDataTypes');
  }

  /**
   * Get chunk information
   */
  async getChunkInfo(): Promise<any> {
    return await this.query('getChunkInfo');
  }

  /**
   * Read batch
   */
  async readBatch(batchIndex: number): Promise<any> {
    return await this.query('readBatch', { batchIndex });
  }
}
