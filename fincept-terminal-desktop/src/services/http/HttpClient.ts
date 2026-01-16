/**
 * HTTP Client Service
 * Unified HTTP client with interceptors for broker API calls
 */

// TODO: Re-enable once broker types are properly defined
// import type { BrokerError, RateLimitError } from '@/brokers/stocks/base/types';

// Temporary inline types until broker types are properly defined
interface BrokerError extends Error {
  code?: string;
  statusCode?: number;
  response?: any;
}

interface RateLimitError extends BrokerError {
  retryAfter?: number;
}

export interface HttpClientConfig {
  baseURL: string;
  timeout?: number;
  headers?: Record<string, string>;
  retryAttempts?: number;
  retryDelay?: number;
}

export interface RequestInterceptor {
  onRequest?: (config: RequestConfig) => RequestConfig | Promise<RequestConfig>;
  onRequestError?: (error: any) => any;
}

export interface ResponseInterceptor {
  onResponse?: (response: Response, data: any) => any;
  onResponseError?: (error: any) => any;
}

export interface RequestConfig extends RequestInit {
  url: string;
  params?: Record<string, any>;
  timeout?: number;
}

export class HttpClient {
  private config: HttpClientConfig;
  private requestInterceptors: RequestInterceptor[] = [];
  private responseInterceptors: ResponseInterceptor[] = [];

  constructor(config: HttpClientConfig) {
    this.config = {
      timeout: 30000,
      retryAttempts: 3,
      retryDelay: 1000,
      ...config,
    };
  }

  /**
   * Add request interceptor
   */
  addRequestInterceptor(interceptor: RequestInterceptor): void {
    this.requestInterceptors.push(interceptor);
  }

  /**
   * Add response interceptor
   */
  addResponseInterceptor(interceptor: ResponseInterceptor): void {
    this.responseInterceptors.push(interceptor);
  }

  /**
   * GET request
   */
  async get<T = any>(url: string, config?: Partial<RequestConfig>): Promise<T> {
    return this.request<T>({
      method: 'GET',
      url,
      ...config,
    });
  }

  /**
   * POST request
   */
  async post<T = any>(url: string, data?: any, config?: Partial<RequestConfig>): Promise<T> {
    return this.request<T>({
      method: 'POST',
      url,
      body: JSON.stringify(data),
      headers: {
        'Content-Type': 'application/json',
        ...config?.headers,
      },
      ...config,
    });
  }

  /**
   * PUT request
   */
  async put<T = any>(url: string, data?: any, config?: Partial<RequestConfig>): Promise<T> {
    return this.request<T>({
      method: 'PUT',
      url,
      body: JSON.stringify(data),
      headers: {
        'Content-Type': 'application/json',
        ...config?.headers,
      },
      ...config,
    });
  }

  /**
   * DELETE request
   */
  async delete<T = any>(url: string, config?: Partial<RequestConfig>): Promise<T> {
    return this.request<T>({
      method: 'DELETE',
      url,
      ...config,
    });
  }

  /**
   * Generic request method
   */
  private async request<T = any>(requestConfig: RequestConfig): Promise<T> {
    // Apply request interceptors
    let config = requestConfig;
    for (const interceptor of this.requestInterceptors) {
      if (interceptor.onRequest) {
        try {
          config = await interceptor.onRequest(config);
        } catch (error) {
          if (interceptor.onRequestError) {
            throw interceptor.onRequestError(error);
          }
          throw error;
        }
      }
    }

    // Build full URL
    const fullUrl = this.buildUrl(config.url, config.params);

    // Merge headers
    const headers = {
      ...this.config.headers,
      ...config.headers,
    };

    // Create AbortController for timeout
    const controller = new AbortController();
    const timeout = config.timeout || this.config.timeout;
    const timeoutId = setTimeout(() => controller.abort(), timeout);

    try {
      // Make request
      const response = await fetch(fullUrl, {
        ...config,
        headers,
        signal: controller.signal,
      });

      clearTimeout(timeoutId);

      // Parse response
      let data: any;
      const contentType = response.headers.get('content-type');

      if (contentType?.includes('application/json')) {
        data = await response.json();
      } else {
        data = await response.text();
      }

      // Check for errors
      if (!response.ok) {
        throw this.createError(response, data);
      }

      // Apply response interceptors
      for (const interceptor of this.responseInterceptors) {
        if (interceptor.onResponse) {
          data = interceptor.onResponse(response, data);
        }
      }

      return data as T;

    } catch (error: any) {
      clearTimeout(timeoutId);

      // Apply response error interceptors
      let finalError = error;
      for (const interceptor of this.responseInterceptors) {
        if (interceptor.onResponseError) {
          finalError = interceptor.onResponseError(finalError);
        }
      }

      throw finalError;
    }
  }

  /**
   * Build full URL with query params
   */
  private buildUrl(url: string, params?: Record<string, any>): string {
    const baseURL = this.config.baseURL;
    let fullUrl = url.startsWith('http') ? url : `${baseURL}${url}`;

    if (params) {
      const searchParams = new URLSearchParams();
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          searchParams.append(key, String(value));
        }
      });

      const queryString = searchParams.toString();
      if (queryString) {
        fullUrl += (fullUrl.includes('?') ? '&' : '?') + queryString;
      }
    }

    return fullUrl;
  }

  /**
   * Create error from response
   */
  private createError(response: Response, data: any): Error {
    const message = data?.message || data?.error || response.statusText || 'Request failed';

    if (response.status === 401) {
      const error = new Error(message) as BrokerError;
      error.name = 'AuthenticationError';
      error.code = 'UNAUTHORIZED';
      error.statusCode = 401;
      error.response = data;
      return error;
    }

    if (response.status === 429) {
      const retryAfter = response.headers.get('Retry-After');
      const error = new Error(message) as RateLimitError;
      error.name = 'RateLimitError';
      error.code = 'RATE_LIMIT';
      error.statusCode = 429;
      error.retryAfter = retryAfter ? parseInt(retryAfter) : undefined;
      error.response = data;
      return error;
    }

    const error = new Error(message) as BrokerError;
    error.name = 'BrokerError';
    error.statusCode = response.status;
    error.response = data;
    return error;
  }

  /**
   * Update base URL
   */
  setBaseURL(baseURL: string): void {
    this.config.baseURL = baseURL;
  }

  /**
   * Update default headers
   */
  setHeaders(headers: Record<string, string>): void {
    this.config.headers = {
      ...this.config.headers,
      ...headers,
    };
  }

  /**
   * Remove a header
   */
  removeHeader(key: string): void {
    if (this.config.headers) {
      delete this.config.headers[key];
    }
  }
}
