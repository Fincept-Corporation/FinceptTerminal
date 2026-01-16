/**
 * Common HTTP Interceptors
 */

import type { RequestInterceptor, ResponseInterceptor } from './HttpClient';

/**
 * Logging interceptor - logs all requests and responses
 */
export const loggingInterceptor: RequestInterceptor & ResponseInterceptor = {
  onRequest: (config) => {
    console.log(`[HTTP] ${config.method} ${config.url}`);
    return config;
  },

  onResponse: (response, data) => {
    console.log(`[HTTP] ${response.status} ${response.url}`);
    return data;
  },

  onResponseError: (error) => {
    console.error('[HTTP Error]', error);
    return error;
  },
};

/**
 * Auth token interceptor - adds authorization header
 */
export function createAuthInterceptor(getToken: () => string | null): RequestInterceptor {
  return {
    onRequest: (config) => {
      const token = getToken();
      if (token) {
        config.headers = {
          ...config.headers,
          Authorization: `Bearer ${token}`,
        };
      }
      return config;
    },
  };
}

/**
 * Rate limit retry interceptor
 */
export const rateLimitRetryInterceptor: ResponseInterceptor = {
  onResponseError: async (error) => {
    if (error.name === 'RateLimitError' && error.retryAfter) {
      console.warn(`[HTTP] Rate limited. Retrying after ${error.retryAfter}s...`);
      await new Promise(resolve => setTimeout(resolve, error.retryAfter! * 1000));
      // Note: Actual retry logic should be handled by the calling code
    }
    return error;
  },
};

/**
 * Token refresh interceptor - automatically refreshes expired tokens
 */
export function createTokenRefreshInterceptor(
  isTokenExpired: () => boolean,
  refreshToken: () => Promise<string>
): RequestInterceptor {
  return {
    onRequest: async (config) => {
      if (isTokenExpired()) {
        console.log('[HTTP] Token expired, refreshing...');
        const newToken = await refreshToken();
        config.headers = {
          ...config.headers,
          Authorization: `Bearer ${newToken}`,
        };
      }
      return config;
    },
  };
}

/**
 * Retry interceptor - retries failed requests
 */
export function createRetryInterceptor(
  maxRetries: number = 3,
  retryDelay: number = 1000
): ResponseInterceptor {
  let retryCount = 0;

  return {
    onResponseError: async (error) => {
      if (retryCount < maxRetries && shouldRetry(error)) {
        retryCount++;
        console.warn(`[HTTP] Retrying request (${retryCount}/${maxRetries})...`);
        await new Promise(resolve => setTimeout(resolve, retryDelay * retryCount));
        // Note: Actual retry should be handled by the calling code
      }
      return error;
    },
  };
}

function shouldRetry(error: any): boolean {
  // Retry on network errors or 5xx server errors
  if (error.name === 'TypeError') return true; // Network error
  if (error.statusCode >= 500) return true; // Server error
  return false;
}

/**
 * Response transform interceptor - transforms broker-specific responses to standard format
 */
export function createResponseTransformInterceptor(
  transform: (data: any) => any
): ResponseInterceptor {
  return {
    onResponse: (_response, data) => {
      return transform(data);
    },
  };
}
