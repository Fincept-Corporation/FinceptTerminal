// CSV File Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class CSVAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const filePath = this.getConfig<string>('filePath');
      const url = this.getConfig<string>('url');
      const delimiter = this.getConfig<string>('delimiter', ',');
      const hasHeader = this.getConfig<boolean>('hasHeader', true);
      const encoding = this.getConfig<string>('encoding', 'utf-8');

      if (!filePath && !url) {
        return this.createErrorResult('Either file path or URL is required');
      }

      console.log('Validating CSV configuration (client-side only)');

      // If URL is provided, we can try to validate it
      if (url) {
        try {
          const response = await fetch(url, { method: 'HEAD' });

          if (!response.ok) {
            throw new Error(`URL not accessible: ${response.status} ${response.statusText}`);
          }

          const contentType = response.headers.get('content-type');
          const contentLength = response.headers.get('content-length');

          return this.createSuccessResult(`CSV file accessible at URL`, {
            url,
            contentType,
            size: contentLength ? `${(parseInt(contentLength) / 1024).toFixed(2)} KB` : 'Unknown',
            delimiter,
            hasHeader,
            encoding,
          });
        } catch (fetchError) {
          return this.createErrorResult(`Failed to access CSV at URL: ${fetchError instanceof Error ? fetchError.message : String(fetchError)}`);
        }
      }

      // For file path, perform basic validation
      return this.createSuccessResult(
        `Configuration validated for CSV file "${filePath}"`,
        {
          validatedAt: new Date().toISOString(),
          filePath,
          delimiter,
          hasHeader,
          encoding,
          note: 'Client-side validation only. Full file validation requires backend integration.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(query: string): Promise<any> {
    try {
      // CSV doesn't support traditional queries, but we can support operations like:
      // - "SELECT * LIMIT 10" to get first 10 rows
      // - "DESCRIBE" to get column info
      const response = await fetch('/api/data-sources/query', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'csv',
          operation: query,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`CSV operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      filePath: this.getConfig('filePath'),
      url: this.getConfig('url'),
      delimiter: this.getConfig('delimiter', ','),
      hasHeader: this.getConfig('hasHeader', true),
      encoding: this.getConfig('encoding', 'utf-8'),
    };
  }
}
