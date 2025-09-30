// JSON File/API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class JSONAdapter extends BaseAdapter {
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

      console.log('Testing JSON source...');

      // If URL is provided, we can try to fetch and validate JSON
      if (url) {
        try {
          const response = await fetch(url, { method: 'GET' });

          if (!response.ok) {
            throw new Error(`URL not accessible: ${response.status} ${response.statusText}`);
          }

          const contentType = response.headers.get('content-type');

          // Try to parse JSON
          const text = await response.text();
          let jsonData;
          try {
            jsonData = JSON.parse(text);
          } catch (parseError) {
            throw new Error('Response is not valid JSON');
          }

          // Get some stats about the JSON
          const isArray = Array.isArray(jsonData);
          const itemCount = isArray ? jsonData.length : Object.keys(jsonData).length;
          const dataType = isArray ? 'array' : typeof jsonData;

          return this.createSuccessResult('Successfully loaded JSON from URL', {
            url,
            contentType,
            dataType,
            itemCount,
            size: text.length,
            timestamp: new Date().toISOString(),
          });
        } catch (fetchError) {
          return this.createErrorResult(
            `Failed to load JSON from URL: ${fetchError instanceof Error ? fetchError.message : String(fetchError)}`
          );
        }
      }

      // For file path, perform basic validation
      return this.createSuccessResult(`Configuration validated for JSON file "${filePath}"`, {
        validatedAt: new Date().toISOString(),
        filePath,
        note: 'Client-side validation only. Full file parsing requires backend integration.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(jsonPath?: string): Promise<any> {
    try {
      const filePath = this.getConfig<string>('filePath');
      const url = this.getConfig<string>('url');

      if (url) {
        // Fetch from URL
        const response = await fetch(url);
        if (!response.ok) {
          throw new Error(`Failed to fetch JSON: ${response.status} ${response.statusText}`);
        }

        let data = await response.json();

        // Apply JSONPath query if provided
        if (jsonPath) {
          data = this.applyJSONPath(data, jsonPath);
        }

        return data;
      }

      // For file path, use backend
      const response = await fetch('/api/data-sources/json', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'json',
          operation: 'read',
          params: { filePath, jsonPath },
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`JSON query error: ${error instanceof Error ? error.message : String(error)}`);
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
   * Simple JSONPath implementation
   */
  private applyJSONPath(data: any, path: string): any {
    if (!path || path === '$') {
      return data;
    }

    // Remove leading '$.'
    const cleanPath = path.replace(/^\$\.?/, '');

    // Split path by dots
    const parts = cleanPath.split('.');

    let result = data;
    for (const part of parts) {
      // Handle array access like 'items[0]'
      const arrayMatch = part.match(/^(.+)\[(\d+)\]$/);
      if (arrayMatch) {
        const [, key, index] = arrayMatch;
        result = result[key]?.[parseInt(index)];
      } else {
        result = result[part];
      }

      if (result === undefined) {
        return null;
      }
    }

    return result;
  }

  /**
   * Get data as array (flattens if needed)
   */
  async getAsArray(jsonPath?: string): Promise<any[]> {
    const data = await this.query(jsonPath);

    if (Array.isArray(data)) {
      return data;
    }

    if (typeof data === 'object' && data !== null) {
      return Object.values(data);
    }

    return [data];
  }

  /**
   * Filter data
   */
  async filter(predicate: (item: any) => boolean, jsonPath?: string): Promise<any[]> {
    const data = await this.getAsArray(jsonPath);
    return data.filter(predicate);
  }

  /**
   * Map data
   */
  async map(mapper: (item: any) => any, jsonPath?: string): Promise<any[]> {
    const data = await this.getAsArray(jsonPath);
    return data.map(mapper);
  }

  /**
   * Find item
   */
  async find(predicate: (item: any) => boolean, jsonPath?: string): Promise<any> {
    const data = await this.getAsArray(jsonPath);
    return data.find(predicate);
  }

  /**
   * Count items
   */
  async count(jsonPath?: string): Promise<number> {
    const data = await this.query(jsonPath);

    if (Array.isArray(data)) {
      return data.length;
    }

    if (typeof data === 'object' && data !== null) {
      return Object.keys(data).length;
    }

    return 1;
  }

  /**
   * Get keys at path
   */
  async getKeys(jsonPath?: string): Promise<string[]> {
    const data = await this.query(jsonPath);

    if (typeof data === 'object' && data !== null) {
      return Object.keys(data);
    }

    return [];
  }

  /**
   * Get value at specific path
   */
  async getValue(jsonPath: string): Promise<any> {
    return await this.query(jsonPath);
  }
}
