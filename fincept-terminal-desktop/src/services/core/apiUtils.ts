/**
 * API Utilities - Production-ready helpers for API calls
 *
 * Features:
 * - Timeout protection
 * - Rate limiting
 * - Retry with exponential backoff
 *
 * Usage:
 *   import { withTimeout, rateLimiter, fetchWithRetry } from '@/services/core/apiUtils';
 */

// ============================================================================
// Timeout Protection
// ============================================================================

/**
 * Wrap a promise with a timeout
 * @param promise - The promise to wrap
 * @param timeoutMs - Timeout in milliseconds
 * @param errorMessage - Custom error message on timeout
 */
export async function withTimeout<T>(
  promise: Promise<T>,
  timeoutMs: number,
  errorMessage = 'Request timeout'
): Promise<T> {
  let timeoutId: ReturnType<typeof setTimeout>;

  const timeoutPromise = new Promise<never>((_, reject) => {
    timeoutId = setTimeout(() => reject(new Error(errorMessage)), timeoutMs);
  });

  try {
    return await Promise.race([promise, timeoutPromise]);
  } finally {
    clearTimeout(timeoutId!);
  }
}

// ============================================================================
// Rate Limiting
// ============================================================================

export interface RateLimiter {
  /** Check if a request can proceed */
  canProceed: () => boolean;
  /** Record a request */
  record: () => void;
  /** Get remaining requests in current window */
  remaining: () => number;
  /** Reset the rate limiter */
  reset: () => void;
}

/**
 * Create a rate limiter with sliding window
 * @param maxRequests - Maximum requests allowed in window
 * @param windowMs - Window size in milliseconds
 */
export function createRateLimiter(maxRequests: number, windowMs: number): RateLimiter {
  const timestamps: number[] = [];

  const cleanup = () => {
    const now = Date.now();
    while (timestamps.length > 0 && timestamps[0] < now - windowMs) {
      timestamps.shift();
    }
  };

  return {
    canProceed: () => {
      cleanup();
      return timestamps.length < maxRequests;
    },
    record: () => {
      timestamps.push(Date.now());
    },
    remaining: () => {
      cleanup();
      return Math.max(0, maxRequests - timestamps.length);
    },
    reset: () => {
      timestamps.length = 0;
    },
  };
}

// Pre-configured rate limiters for common use cases
export const rateLimiters = {
  /** 60 requests per minute - general API calls */
  standard: createRateLimiter(60, 60000),
  /** 10 requests per minute - expensive operations */
  conservative: createRateLimiter(10, 60000),
  /** 120 requests per minute - high-frequency data */
  aggressive: createRateLimiter(120, 60000),
};

// ============================================================================
// Retry with Exponential Backoff
// ============================================================================

export interface RetryOptions {
  /** Maximum retry attempts (default: 3) */
  maxRetries?: number;
  /** Base delay in ms for exponential backoff (default: 1000) */
  baseDelay?: number;
  /** Maximum delay in ms (default: 30000) */
  maxDelay?: number;
  /** AbortSignal for cancellation */
  signal?: AbortSignal;
  /** Condition to determine if error is retryable */
  isRetryable?: (error: Error) => boolean;
  /** Callback on each retry */
  onRetry?: (attempt: number, error: Error) => void;
}

const defaultRetryOptions: Required<Omit<RetryOptions, 'signal' | 'onRetry'>> = {
  maxRetries: 3,
  baseDelay: 1000,
  maxDelay: 30000,
  isRetryable: () => true,
};

/**
 * Execute a function with retry logic and exponential backoff
 */
export async function fetchWithRetry<T>(
  fn: () => Promise<T>,
  options: RetryOptions = {}
): Promise<T> {
  const opts = { ...defaultRetryOptions, ...options };
  let lastError: Error | null = null;

  for (let attempt = 0; attempt <= opts.maxRetries; attempt++) {
    // Check if aborted
    if (opts.signal?.aborted) {
      throw new Error('Request aborted');
    }

    try {
      return await fn();
    } catch (err) {
      lastError = err instanceof Error ? err : new Error(String(err));

      // Don't retry on abort
      if (opts.signal?.aborted) {
        throw lastError;
      }

      // Check if retryable
      if (!opts.isRetryable(lastError)) {
        throw lastError;
      }

      // Don't retry on last attempt
      if (attempt === opts.maxRetries) {
        break;
      }

      // Calculate delay with jitter
      const delay = Math.min(
        opts.baseDelay * Math.pow(2, attempt) + Math.random() * 100,
        opts.maxDelay
      );

      opts.onRetry?.(attempt + 1, lastError);

      // Wait before retry
      await new Promise((resolve) => setTimeout(resolve, delay));
    }
  }

  throw lastError || new Error('Fetch failed after retries');
}

// ============================================================================
// Combined Utility
// ============================================================================

export interface SafeFetchOptions extends RetryOptions {
  /** Timeout in milliseconds */
  timeout?: number;
  /** Rate limiter to use */
  rateLimiter?: RateLimiter;
}

/**
 * Safe fetch with timeout, rate limiting, and retry
 */
export async function safeFetch<T>(
  fn: () => Promise<T>,
  options: SafeFetchOptions = {}
): Promise<T> {
  const { timeout = 30000, rateLimiter, ...retryOptions } = options;

  // Check rate limit
  if (rateLimiter && !rateLimiter.canProceed()) {
    throw new Error('Rate limit exceeded. Please wait a moment.');
  }

  // Record request
  rateLimiter?.record();

  // Execute with timeout and retry
  return fetchWithRetry(
    () => withTimeout(fn(), timeout),
    retryOptions
  );
}

// ============================================================================
// Request Deduplication
// ============================================================================

interface PendingRequest<T> {
  promise: Promise<T>;
  timestamp: number;
}

const pendingRequests = new Map<string, PendingRequest<unknown>>();
const DEDUP_WINDOW_MS = 100;

/**
 * Deduplicate concurrent requests with the same key
 */
export async function deduplicatedFetch<T>(
  key: string,
  fn: () => Promise<T>
): Promise<T> {
  const now = Date.now();

  // Check for existing request
  const existing = pendingRequests.get(key);
  if (existing && now - existing.timestamp < DEDUP_WINDOW_MS) {
    return existing.promise as Promise<T>;
  }

  // Create new request
  const promise = fn();
  pendingRequests.set(key, { promise, timestamp: now });

  try {
    return await promise;
  } finally {
    // Cleanup after window
    setTimeout(() => {
      const current = pendingRequests.get(key);
      if (current?.promise === promise) {
        pendingRequests.delete(key);
      }
    }, DEDUP_WINDOW_MS);
  }
}
