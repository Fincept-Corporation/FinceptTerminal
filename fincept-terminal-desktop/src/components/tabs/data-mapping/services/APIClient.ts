// API Client - Comprehensive HTTP request handler

import { fetch } from '@tauri-apps/plugin-http';
import { APIConfig, APIResponse } from '../types';
import { buildURL, buildHeaders, validateURL, validateParameters } from '../utils/urlBuilder';

/**
 * API Client with full error handling, timeout, and retry logic
 */
export class APIClient {
  private readonly DEFAULT_TIMEOUT = 30000; // 30 seconds

  /**
   * Execute an API request with full error handling
   */
  async executeRequest(
    config: APIConfig,
    runtimeParams: Record<string, any> = {}
  ): Promise<APIResponse> {
    const startTime = Date.now();

    try {
      // 1. Validate configuration
      const validation = this.validateConfig(config, runtimeParams);
      if (!validation.valid) {
        return this.createErrorResponse(
          'client',
          validation.error || 'Invalid configuration',
          startTime,
          config,
          runtimeParams
        );
      }

      // 2. Build URL and headers
      const url = buildURL(config, runtimeParams);
      const headers = buildHeaders(config);

      // 3. Prepare request options
      const requestOptions: RequestInit = {
        method: config.method,
        headers,
        signal: AbortSignal.timeout(config.timeout || this.DEFAULT_TIMEOUT),
      };

      // 4. Add body for POST/PUT/PATCH
      if (['POST', 'PUT', 'PATCH'].includes(config.method) && config.body) {
        try {
          // Validate JSON if provided
          JSON.parse(config.body);
          requestOptions.body = config.body;
        } catch {
          return this.createErrorResponse(
            'client',
            'Invalid JSON in request body',
            startTime,
            config,
            runtimeParams
          );
        }
      }

      // 5. Execute request
      const response = await fetch(url, requestOptions);

      // 6. Handle response
      return await this.handleResponse(response, startTime, config, runtimeParams);

    } catch (error) {
      return this.handleError(error, startTime, config, runtimeParams);
    }
  }

  /**
   * Handle successful response
   */
  private async handleResponse(
    response: Response,
    startTime: number,
    config: APIConfig,
    runtimeParams: Record<string, any>
  ): Promise<APIResponse> {
    const duration = Date.now() - startTime;

    // Check status code
    if (!response.ok) {
      const errorType = this.getErrorTypeFromStatus(response.status);
      let errorMessage = `HTTP ${response.status}: ${response.statusText}`;

      // Try to get error details from response body
      try {
        const errorBody = await response.text();
        if (errorBody) {
          try {
            const errorJson = JSON.parse(errorBody);
            errorMessage += ` - ${JSON.stringify(errorJson)}`;
          } catch {
            errorMessage += ` - ${errorBody.substring(0, 200)}`;
          }
        }
      } catch {
        // Ignore if can't read body
      }

      return {
        success: false,
        error: {
          type: errorType,
          statusCode: response.status,
          message: errorMessage,
        },
        metadata: {
          duration,
          url: response.url,
          method: config.method,
          timestamp: new Date().toISOString(),
        },
      };
    }

    // Parse response data
    try {
      const contentType = response.headers.get('content-type');
      let data: any;

      if (contentType?.includes('application/json')) {
        data = await response.json();
      } else {
        data = await response.text();
      }

      return {
        success: true,
        data,
        metadata: {
          duration,
          url: response.url,
          method: config.method,
          timestamp: new Date().toISOString(),
        },
      };
    } catch (error) {
      return {
        success: false,
        error: {
          type: 'parse',
          message: 'Failed to parse response: ' + (error instanceof Error ? error.message : String(error)),
        },
        metadata: {
          duration,
          url: response.url,
          method: config.method,
          timestamp: new Date().toISOString(),
        },
      };
    }
  }

  /**
   * Handle errors during request
   */
  private handleError(
    error: any,
    startTime: number,
    config: APIConfig,
    runtimeParams: Record<string, any>
  ): APIResponse {
    const duration = Date.now() - startTime;
    const url = buildURL(config, runtimeParams);

    let errorType: 'network' | 'auth' | 'client' | 'server' | 'timeout' | 'parse' = 'network';
    let errorMessage = 'Network error';

    if (error.name === 'AbortError' || error.name === 'TimeoutError') {
      errorType = 'timeout';
      errorMessage = `Request timeout (${config.timeout || this.DEFAULT_TIMEOUT}ms)`;
    } else if (error instanceof TypeError) {
      errorType = 'network';
      errorMessage = 'Network connection failed. Check internet connection or CORS settings.';
    } else {
      errorMessage = error.message || String(error);
    }

    return {
      success: false,
      error: {
        type: errorType,
        message: errorMessage,
        details: error.stack,
      },
      metadata: {
        duration,
        url,
        method: config.method,
        timestamp: new Date().toISOString(),
      },
    };
  }

  /**
   * Validate API configuration before executing
   */
  validateConfig(
    config: APIConfig,
    runtimeParams: Record<string, any> = {}
  ): { valid: boolean; error?: string } {
    // Validate base URL
    const urlValidation = validateURL(config.baseUrl);
    if (!urlValidation.valid) {
      return { valid: false, error: `Invalid base URL: ${urlValidation.error}` };
    }

    // Validate endpoint
    if (!config.endpoint || config.endpoint.trim().length === 0) {
      return { valid: false, error: 'Endpoint is required' };
    }

    // Validate method
    if (!['GET', 'POST', 'PUT', 'DELETE', 'PATCH'].includes(config.method)) {
      return { valid: false, error: 'Invalid HTTP method' };
    }

    // Validate required parameters
    const paramValidation = validateParameters(config, runtimeParams);
    if (!paramValidation.valid) {
      return {
        valid: false,
        error: `Missing required parameters: ${paramValidation.missing?.join(', ')}`,
      };
    }

    // Validate body for POST/PUT/PATCH
    if (['POST', 'PUT', 'PATCH'].includes(config.method) && config.body) {
      try {
        JSON.parse(config.body);
      } catch {
        return { valid: false, error: 'Request body must be valid JSON' };
      }
    }

    return { valid: true };
  }

  /**
   * Determine error type from HTTP status code
   */
  private getErrorTypeFromStatus(status: number): 'auth' | 'client' | 'server' {
    if (status === 401 || status === 403) {
      return 'auth';
    } else if (status >= 400 && status < 500) {
      return 'client';
    } else {
      return 'server';
    }
  }

  /**
   * Create error response helper
   */
  private createErrorResponse(
    type: 'network' | 'auth' | 'client' | 'server' | 'timeout' | 'parse',
    message: string,
    startTime: number,
    config: APIConfig,
    runtimeParams: Record<string, any>
  ): APIResponse {
    return {
      success: false,
      error: {
        type,
        message,
      },
      metadata: {
        duration: Date.now() - startTime,
        url: buildURL(config, runtimeParams),
        method: config.method,
        timestamp: new Date().toISOString(),
      },
    };
  }

  /**
   * Test connection (lightweight request to validate config)
   */
  async testConnection(config: APIConfig): Promise<{
    success: boolean;
    message: string;
  }> {
    try {
      const validation = this.validateConfig(config, {});
      if (!validation.valid) {
        return {
          success: false,
          message: validation.error || 'Invalid configuration',
        };
      }

      // Just validate URL is reachable (don't need full request)
      const url = buildURL(config, {});
      const urlValidation = validateURL(url);

      if (!urlValidation.valid) {
        return {
          success: false,
          message: urlValidation.error || 'Invalid URL',
        };
      }

      return {
        success: true,
        message: 'Configuration is valid',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : String(error),
      };
    }
  }
}

// Export singleton instance
export const apiClient = new APIClient();
export default apiClient;
