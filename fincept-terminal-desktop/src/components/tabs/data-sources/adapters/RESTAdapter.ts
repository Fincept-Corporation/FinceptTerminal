// REST API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class RESTAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const baseUrl = this.getConfig<string>('baseUrl');
      const apiKey = this.getConfig<string>('apiKey');
      const authHeader = this.getConfig<string>('authHeader', 'Authorization');
      const authPrefix = this.getConfig<string>('authPrefix', 'Bearer');
      const testEndpoint = this.getConfig<string>('testEndpoint', '');

      if (!baseUrl) {
        return this.createErrorResult('Base URL is required');
      }

      console.log('Testing REST API connection...');

      try {
        const headers: Record<string, string> = {
          'Content-Type': 'application/json',
        };

        if (apiKey) {
          headers[authHeader] = `${authPrefix} ${apiKey}`;
        }

        // Test with provided endpoint or just the base URL
        const testUrl = testEndpoint ? `${baseUrl}${testEndpoint}` : baseUrl;

        const response = await fetch(testUrl, {
          method: 'GET',
          headers,
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Authentication failed. Check your API key or credentials.');
          }
          if (response.status === 404 && testEndpoint) {
            throw new Error(`Test endpoint not found: ${testEndpoint}`);
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const contentType = response.headers.get('content-type');
        const isJson = contentType?.includes('application/json');

        return this.createSuccessResult('Successfully connected to REST API', {
          baseUrl,
          testEndpoint: testEndpoint || 'Base URL only',
          contentType,
          isJson,
          status: response.status,
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          if (fetchError.message.includes('Failed to fetch')) {
            return this.createErrorResult(`Cannot reach REST API at ${baseUrl}.`);
          }
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(endpoint: string, options?: RequestInit): Promise<any> {
    try {
      const baseUrl = this.getConfig<string>('baseUrl');
      const apiKey = this.getConfig<string>('apiKey');
      const authHeader = this.getConfig<string>('authHeader', 'Authorization');
      const authPrefix = this.getConfig<string>('authPrefix', 'Bearer');

      const headers: Record<string, string> = {
        'Content-Type': 'application/json',
        ...(options?.headers as Record<string, string>),
      };

      if (apiKey) {
        headers[authHeader] = `${authPrefix} ${apiKey}`;
      }

      const url = endpoint.startsWith('http') ? endpoint : `${baseUrl}${endpoint}`;

      const response = await fetch(url, {
        ...options,
        headers,
      });

      if (!response.ok) {
        throw new Error(`Request failed: ${response.status} ${response.statusText}`);
      }

      const contentType = response.headers.get('content-type');

      if (contentType?.includes('application/json')) {
        return await response.json();
      }

      return await response.text();
    } catch (error) {
      throw new Error(`REST API error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      baseUrl: this.getConfig('baseUrl'),
      hasApiKey: !!this.getConfig('apiKey'),
      authHeader: this.getConfig('authHeader', 'Authorization'),
      authPrefix: this.getConfig('authPrefix', 'Bearer'),
      testEndpoint: this.getConfig('testEndpoint', ''),
    };
  }

  /**
   * GET request
   */
  async get(endpoint: string, params?: Record<string, any>): Promise<any> {
    const queryString = params ? `?${new URLSearchParams(params).toString()}` : '';
    return await this.query(`${endpoint}${queryString}`, { method: 'GET' });
  }

  /**
   * POST request
   */
  async post(endpoint: string, body: any): Promise<any> {
    return await this.query(endpoint, {
      method: 'POST',
      body: JSON.stringify(body),
    });
  }

  /**
   * PUT request
   */
  async put(endpoint: string, body: any): Promise<any> {
    return await this.query(endpoint, {
      method: 'PUT',
      body: JSON.stringify(body),
    });
  }

  /**
   * PATCH request
   */
  async patch(endpoint: string, body: any): Promise<any> {
    return await this.query(endpoint, {
      method: 'PATCH',
      body: JSON.stringify(body),
    });
  }

  /**
   * DELETE request
   */
  async delete(endpoint: string): Promise<any> {
    return await this.query(endpoint, { method: 'DELETE' });
  }

  /**
   * HEAD request
   */
  async head(endpoint: string): Promise<any> {
    return await this.query(endpoint, { method: 'HEAD' });
  }

  /**
   * OPTIONS request
   */
  async options(endpoint: string): Promise<any> {
    return await this.query(endpoint, { method: 'OPTIONS' });
  }
}
