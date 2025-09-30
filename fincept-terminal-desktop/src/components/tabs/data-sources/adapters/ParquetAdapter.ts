// Apache Parquet File Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class ParquetAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const filePath = this.getConfig<string>('filePath');
      const url = this.getConfig<string>('url');

      if (!filePath && !url) {
        return this.createErrorResult('Either file path or URL is required');
      }

      console.log('Validating Parquet file configuration (client-side only)');

      // If URL is provided, check accessibility
      if (url) {
        try {
          const response = await fetch(url, { method: 'HEAD' });

          if (!response.ok) {
            throw new Error(`URL not accessible: ${response.status} ${response.statusText}`);
          }

          const contentType = response.headers.get('content-type');
          const contentLength = response.headers.get('content-length');

          return this.createSuccessResult('Parquet file accessible at URL', {
            url,
            contentType,
            size: contentLength ? `${(parseInt(contentLength) / 1024 / 1024).toFixed(2)} MB` : 'Unknown',
            timestamp: new Date().toISOString(),
          });
        } catch (fetchError) {
          return this.createErrorResult(
            `Failed to access Parquet file at URL: ${fetchError instanceof Error ? fetchError.message : String(fetchError)}`
          );
        }
      }

      return this.createSuccessResult(`Configuration validated for Parquet file "${filePath}"`, {
        validatedAt: new Date().toISOString(),
        filePath,
        note: 'Client-side validation only. Parquet parsing requires backend integration with parquetjs or arrow.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(sql?: string): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/parquet', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'parquet',
          operation: 'query',
          params: { sql },
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Parquet query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      filePath: this.getConfig('filePath'),
      url: this.getConfig('url'),
    };
  }

  /**
   * Get file metadata
   */
  async getFileMetadata(): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/parquet', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'parquet',
          operation: 'getMetadata',
        }),
      });

      if (!response.ok) {
        throw new Error(`Failed to get metadata: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Parquet metadata error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  /**
   * Get schema
   */
  async getSchema(): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/parquet', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'parquet',
          operation: 'getSchema',
        }),
      });

      if (!response.ok) {
        throw new Error(`Failed to get schema: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Parquet schema error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  /**
   * Read rows with limit and offset
   */
  async readRows(limit?: number, offset?: number): Promise<any[]> {
    try {
      const response = await fetch('/api/data-sources/parquet', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'parquet',
          operation: 'readRows',
          params: { limit, offset },
        }),
      });

      if (!response.ok) {
        throw new Error(`Failed to read rows: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Parquet read error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  /**
   * Get row count
   */
  async getRowCount(): Promise<number> {
    try {
      const response = await fetch('/api/data-sources/parquet', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'parquet',
          operation: 'getRowCount',
        }),
      });

      if (!response.ok) {
        throw new Error(`Failed to get row count: ${response.statusText}`);
      }

      const result = await response.json();
      return result.count || 0;
    } catch (error) {
      throw new Error(`Parquet count error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  /**
   * Get column statistics
   */
  async getColumnStats(columnName: string): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/parquet', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'parquet',
          operation: 'getColumnStats',
          params: { columnName },
        }),
      });

      if (!response.ok) {
        throw new Error(`Failed to get column stats: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Parquet stats error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  /**
   * Convert to JSON
   */
  async toJSON(limit?: number): Promise<any[]> {
    return await this.readRows(limit);
  }
}
