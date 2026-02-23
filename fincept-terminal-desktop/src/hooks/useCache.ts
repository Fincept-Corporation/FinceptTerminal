// useCache Hook - Production-ready React hook for data caching
// Uses unified SQLite cache backend with proper error handling,
// request deduplication, retry logic, and offline support

import { useState, useEffect, useCallback, useRef, useMemo } from 'react';
import { cacheService, CacheCategory, TTLPreset } from '../services/cache/cacheService';

// ============================================================================
// Types
// ============================================================================

export interface UseCacheOptions<T> {
  key: string;
  category: CacheCategory | string;
  fetcher: () => Promise<T>;
  ttl?: number | TTLPreset;
  enabled?: boolean;
  staleWhileRevalidate?: boolean;
  refetchInterval?: number;
  resetOnKeyChange?: boolean;
  retry?: number; // Number of retry attempts (default: 3)
  retryDelay?: number; // Base delay in ms for exponential backoff (default: 1000)
  onSuccess?: (data: T) => void;
  onError?: (error: Error) => void;
}

export interface UseCacheResult<T> {
  data: T | null;
  isLoading: boolean;
  isStale: boolean;
  isFetching: boolean;
  error: Error | null;
  isOffline: boolean;
  refresh: () => Promise<void>;
  invalidate: () => Promise<void>;
}

// ============================================================================
// Request Deduplication - Shared pending requests across components
// ============================================================================

interface PendingRequest<T> {
  promise: Promise<T>;
  timestamp: number;
}

const pendingRequests = new Map<string, PendingRequest<unknown>>();
const REQUEST_DEDUP_WINDOW = 100; // ms - requests within this window are deduplicated

function getPendingRequest<T>(key: string): Promise<T> | null {
  const pending = pendingRequests.get(key);
  if (pending && Date.now() - pending.timestamp < REQUEST_DEDUP_WINDOW) {
    return pending.promise as Promise<T>;
  }
  return null;
}

function setPendingRequest<T>(key: string, promise: Promise<T>): void {
  pendingRequests.set(key, { promise, timestamp: Date.now() });
  // Cleanup after promise resolves
  promise.finally(() => {
    setTimeout(() => {
      const current = pendingRequests.get(key);
      if (current?.promise === promise) {
        pendingRequests.delete(key);
      }
    }, REQUEST_DEDUP_WINDOW);
  });
}

// ============================================================================
// Retry with Exponential Backoff
// ============================================================================

async function fetchWithRetry<T>(
  fetcher: () => Promise<T>,
  maxRetries: number,
  baseDelay: number,
  signal?: AbortSignal
): Promise<T> {
  let lastError: Error | null = null;

  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    // Check if aborted
    if (signal?.aborted) {
      throw new Error('Request aborted');
    }

    try {
      return await fetcher();
    } catch (err) {
      lastError = err instanceof Error ? err : new Error(String(err));

      // Don't retry on abort
      if (signal?.aborted) {
        throw lastError;
      }

      // Don't retry on last attempt
      if (attempt === maxRetries) {
        break;
      }

      // Exponential backoff with jitter
      const delay = baseDelay * Math.pow(2, attempt) + Math.random() * 100;
      await new Promise(resolve => setTimeout(resolve, delay));
    }
  }

  throw lastError || new Error('Fetch failed after retries');
}

// ============================================================================
// Offline Detection
// ============================================================================

function useOnlineStatus(): boolean {
  const [isOnline, setIsOnline] = useState(
    typeof navigator !== 'undefined' ? navigator.onLine : true
  );

  useEffect(() => {
    const handleOnline = () => setIsOnline(true);
    const handleOffline = () => setIsOnline(false);

    window.addEventListener('online', handleOnline);
    window.addEventListener('offline', handleOffline);

    return () => {
      window.removeEventListener('online', handleOnline);
      window.removeEventListener('offline', handleOffline);
    };
  }, []);

  return isOnline;
}

// ============================================================================
// Main Hook
// ============================================================================

export function useCache<T>(options: UseCacheOptions<T>): UseCacheResult<T> {
  const {
    key,
    category,
    fetcher,
    ttl,
    enabled = true,
    staleWhileRevalidate = true,
    refetchInterval,
    resetOnKeyChange = true,
    retry = 3,
    retryDelay = 1000,
    onSuccess,
    onError,
  } = options;

  // State
  const [data, setData] = useState<T | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [isStale, setIsStale] = useState(false);
  const [isFetching, setIsFetching] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  // Online status
  const isOnline = useOnlineStatus();

  // Refs for managing async operations
  const mountedRef = useRef(true);
  const abortControllerRef = useRef<AbortController | null>(null);
  const fetchIdRef = useRef(0); // Track fetch operations to handle race conditions
  const previousKeyRef = useRef<string>(key);

  // Memoize fetcher and callbacks to prevent stale closures in async operations
  const stableFetcher = useRef(fetcher);
  stableFetcher.current = fetcher;
  const stableOnSuccess = useRef(onSuccess);
  stableOnSuccess.current = onSuccess;
  const stableOnError = useRef(onError);
  stableOnError.current = onError;

  // Reset state when key changes
  useEffect(() => {
    if (previousKeyRef.current !== key) {
      // Abort any pending request for old key
      abortControllerRef.current?.abort();
      abortControllerRef.current = null;

      if (resetOnKeyChange) {
        setData(null);
        setIsLoading(true);
        setIsStale(false);
        setError(null);
      }
    }
    previousKeyRef.current = key;
  }, [key, resetOnKeyChange]);

  // Core fetch function
  const fetchData = useCallback(async (showLoading: boolean = true) => {
    // Generate unique ID for this fetch operation
    const fetchId = ++fetchIdRef.current;

    // Abort previous request
    abortControllerRef.current?.abort();
    const abortController = new AbortController();
    abortControllerRef.current = abortController;

    if (showLoading && !data) {
      setIsLoading(true);
    }
    setIsFetching(true);
    setError(null);

    try {
      // Try to get from cache first
      const cacheMethod = staleWhileRevalidate
        ? cacheService.getWithStale<T>(key)
        : cacheService.get<T>(key);

      const cached = await cacheMethod;

      // Check if this fetch is still valid (not superseded by another)
      if (fetchId !== fetchIdRef.current || !mountedRef.current) {
        return;
      }

      if (cached) {
        setData(cached.data);
        setIsStale(cached.isStale);
        setIsLoading(false);
        stableOnSuccess.current?.(cached.data);

        // If data is stale and staleWhileRevalidate is enabled, fetch fresh in background
        if (cached.isStale && staleWhileRevalidate && isOnline) {
          try {
            // Check for pending request (deduplication)
            let freshDataPromise = getPendingRequest<T>(key);

            if (!freshDataPromise) {
              freshDataPromise = fetchWithRetry(
                stableFetcher.current,
                retry,
                retryDelay,
                abortController.signal
              );
              setPendingRequest(key, freshDataPromise);
            }

            const freshData = await freshDataPromise;

            // Check if still valid
            if (fetchId !== fetchIdRef.current || !mountedRef.current) {
              return;
            }

            await cacheService.set(key, freshData, category, ttl);
            setData(freshData);
            setIsStale(false);
            stableOnSuccess.current?.(freshData);
          } catch (fetchError) {
            // Silently fail background refresh, we already have stale data
            if (!(fetchError instanceof Error && fetchError.message === 'Request aborted')) {
              console.warn('[useCache] Background refresh failed:', fetchError);
            }
          }
        }
      } else {
        // No cache - fetch from source (or serve stale if offline)
        if (!isOnline) {
          // Try to get any stale data when offline
          const staleData = await cacheService.getWithStale<T>(key);
          if (staleData && mountedRef.current && fetchId === fetchIdRef.current) {
            setData(staleData.data);
            setIsStale(true);
            setIsLoading(false);
            stableOnSuccess.current?.(staleData.data);
            return;
          }
          throw new Error('No cached data available offline');
        }

        // Check for pending request (deduplication)
        let freshDataPromise = getPendingRequest<T>(key);

        if (!freshDataPromise) {
          freshDataPromise = fetchWithRetry(
            stableFetcher.current,
            retry,
            retryDelay,
            abortController.signal
          );
          setPendingRequest(key, freshDataPromise);
        }

        const freshData = await freshDataPromise;

        // Check if still valid
        if (fetchId !== fetchIdRef.current || !mountedRef.current) {
          return;
        }

        await cacheService.set(key, freshData, category, ttl);
        setData(freshData);
        setIsStale(false);
        setIsLoading(false);
        stableOnSuccess.current?.(freshData);
      }
    } catch (err) {
      // Check if still valid
      if (fetchId !== fetchIdRef.current || !mountedRef.current) {
        return;
      }

      const error = err instanceof Error ? err : new Error(String(err));

      // Don't set error for aborted requests
      if (error.message !== 'Request aborted') {
        setError(error);
        setIsLoading(false);
        stableOnError.current?.(error);
      }
    } finally {
      // Check if still valid
      if (fetchId === fetchIdRef.current && mountedRef.current) {
        setIsFetching(false);
      }
    }
  }, [key, category, ttl, staleWhileRevalidate, retry, retryDelay, isOnline, data]);

  // Force refresh - bypasses cache
  const refresh = useCallback(async () => {
    const fetchId = ++fetchIdRef.current;

    // Abort previous request
    abortControllerRef.current?.abort();
    const abortController = new AbortController();
    abortControllerRef.current = abortController;

    setIsFetching(true);
    setError(null);

    try {
      if (!isOnline) {
        throw new Error('Cannot refresh while offline');
      }

      const freshData = await fetchWithRetry(
        stableFetcher.current,
        retry,
        retryDelay,
        abortController.signal
      );

      if (fetchId !== fetchIdRef.current || !mountedRef.current) {
        return;
      }

      await cacheService.set(key, freshData, category, ttl);
      setData(freshData);
      setIsStale(false);
      stableOnSuccess.current?.(freshData);
    } catch (err) {
      if (fetchId !== fetchIdRef.current || !mountedRef.current) {
        return;
      }

      const error = err instanceof Error ? err : new Error(String(err));
      if (error.message !== 'Request aborted') {
        setError(error);
        stableOnError.current?.(error);
      }
    } finally {
      if (fetchId === fetchIdRef.current && mountedRef.current) {
        setIsFetching(false);
      }
    }
  }, [key, category, ttl, retry, retryDelay, isOnline]);

  // Invalidate cache and reset state
  const invalidate = useCallback(async () => {
    abortControllerRef.current?.abort();
    await cacheService.delete(key);
    if (mountedRef.current) {
      setData(null);
      setIsLoading(true);
      setIsStale(false);
      setError(null);
    }
  }, [key]);

  // Initial fetch and re-fetch when key changes
  useEffect(() => {
    mountedRef.current = true;

    if (enabled) {
      fetchData(true);
    }

    return () => {
      mountedRef.current = false;
      abortControllerRef.current?.abort();
    };
  }, [key, enabled]); // Note: fetchData is intentionally not in deps to prevent loops

  // Auto-refetch interval
  useEffect(() => {
    if (!refetchInterval || !enabled || !isOnline) return;

    const interval = setInterval(() => {
      fetchData(false);
    }, refetchInterval);

    return () => clearInterval(interval);
  }, [refetchInterval, enabled, isOnline]); // Note: fetchData intentionally not in deps

  return {
    data,
    isLoading,
    isStale,
    isFetching,
    error,
    isOffline: !isOnline,
    refresh,
    invalidate,
  };
}

// ============================================================================
// Helper Functions
// ============================================================================

// Generate cache key with consistent format
export function cacheKey(category: string, ...parts: (string | number)[]): string {
  return `${category}:${parts.join(':')}`;
}

// Generate cache key with hash for dynamic data (like arrays of tickers)
export function cacheKeyWithHash(category: string, identifier: string, items: string[]): string {
  if (items.length === 0) return `${category}:${identifier}:empty`;
  // Create a simple hash from items for cache key
  const hash = `${items.length}:${items[0]}:${items[items.length - 1]}`;
  return `${category}:${identifier}:${hash}`;
}

export default useCache;
