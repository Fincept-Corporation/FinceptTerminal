// Apache ORC File Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class ORCAdapter extends BaseAdapter {
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

      console.log('Testing ORC file accessibility...');

      try {
        // Check if file is accessible via HEAD request
        const response = await fetch(filepath, { method: 'HEAD' });

        if (!response.ok) {
          throw new Error(`Cannot access ORC file: ${response.status} ${response.statusText}`);
        }

        const contentLength = response.headers.get('content-length');
        const contentType = response.headers.get('content-type');

        return this.createSuccessResult('Successfully accessed ORC file', {
          filepath,
          fileSize: contentLength ? `${contentLength} bytes` : 'Unknown',
          contentType: contentType || 'Unknown',
          note: 'ORC parsing requires backend integration with Apache ORC libraries or pyorc.',
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
      // ORC operations need to go through backend
      const response = await fetch('/api/data-sources/orc', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'orc',
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
      throw new Error(`ORC operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      filepath: this.getConfig('filepath'),
      provider: 'Apache ORC',
    };
  }

  /**
   * Get ORC schema
   */
  async getSchema(): Promise<any> {
    return await this.query('getSchema');
  }

  /**
   * Read rows
   */
  async readRows(limit?: number, offset?: number): Promise<any> {
    return await this.query('readRows', { limit, offset });
  }

  /**
   * Get row count
   */
  async getRowCount(): Promise<any> {
    return await this.query('getRowCount');
  }

  /**
   * Get file statistics
   */
  async getFileStats(): Promise<any> {
    return await this.query('getFileStats');
  }

  /**
   * Get column statistics
   */
  async getColumnStats(): Promise<any> {
    return await this.query('getColumnStats');
  }

  /**
   * Get stripe information
   */
  async getStripeInfo(): Promise<any> {
    return await this.query('getStripeInfo');
  }

  /**
   * Get compression type
   */
  async getCompression(): Promise<any> {
    return await this.query('getCompression');
  }

  /**
   * Convert to JSON
   */
  async toJSON(limit?: number): Promise<any> {
    return await this.query('toJSON', { limit });
  }

  /**
   * Get user metadata
   */
  async getUserMetadata(): Promise<any> {
    return await this.query('getUserMetadata');
  }

  /**
   * Read specific columns
   */
  async readColumns(columns: string[], limit?: number): Promise<any> {
    return await this.query('readColumns', { columns, limit });
  }
}
