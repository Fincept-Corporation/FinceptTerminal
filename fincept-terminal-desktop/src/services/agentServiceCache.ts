/**
 * Agent Service — LRU Cache
 *
 * Extracted from agentService.ts.
 */

import type { AgentConfig } from './agentServiceTypes';

// =============================================================================
// LRU Cache with Smart Invalidation
// =============================================================================

interface LRUEntry<T> {
  data: T;
  timestamp: number;
  ttl: number;
  accessCount: number;
  lastAccess: number;
}

export class LRUCache {
  private cache = new Map<string, LRUEntry<any>>();
  private maxSize: number;
  private responseCache = new Map<string, LRUEntry<any>>(); // Separate cache for query responses

  constructor(maxSize = 100) {
    this.maxSize = maxSize;
  }

  set<T>(key: string, data: T, ttlMs: number = 5 * 60 * 1000): void {
    // Evict if at capacity
    if (this.cache.size >= this.maxSize) {
      this.evictLRU();
    }

    this.cache.set(key, {
      data,
      timestamp: Date.now(),
      ttl: ttlMs,
      accessCount: 1,
      lastAccess: Date.now(),
    });
  }

  get<T>(key: string): T | null {
    const entry = this.cache.get(key);
    if (!entry) return null;

    // Check TTL
    if (Date.now() - entry.timestamp > entry.ttl) {
      this.cache.delete(key);
      return null;
    }

    // Update access stats
    entry.accessCount++;
    entry.lastAccess = Date.now();

    return entry.data as T;
  }

  // Cache query responses with content-based key
  setResponse<T>(query: string, config: AgentConfig, data: T, ttlMs: number = 2 * 60 * 1000): void {
    const key = this.generateResponseKey(query, config);

    if (this.responseCache.size >= this.maxSize) {
      this.evictLRUResponse();
    }

    this.responseCache.set(key, {
      data,
      timestamp: Date.now(),
      ttl: ttlMs,
      accessCount: 1,
      lastAccess: Date.now(),
    });
  }

  getResponse<T>(query: string, config: AgentConfig): T | null {
    const key = this.generateResponseKey(query, config);
    const entry = this.responseCache.get(key);

    if (!entry) return null;

    if (Date.now() - entry.timestamp > entry.ttl) {
      this.responseCache.delete(key);
      return null;
    }

    entry.accessCount++;
    entry.lastAccess = Date.now();

    return entry.data as T;
  }

  private generateResponseKey(query: string, config: AgentConfig): string {
    // Create deterministic key from query and relevant config
    const configKey = `${config.model.provider}:${config.model.model_id}:${config.tools?.join(',') || ''}`;
    return `${query.slice(0, 100)}|${configKey}`;
  }

  private evictLRU(): void {
    let oldestKey: string | null = null;
    let oldestAccess = Infinity;

    for (const [key, entry] of this.cache) {
      if (entry.lastAccess < oldestAccess) {
        oldestAccess = entry.lastAccess;
        oldestKey = key;
      }
    }

    if (oldestKey) {
      this.cache.delete(oldestKey);
    }
  }

  private evictLRUResponse(): void {
    let oldestKey: string | null = null;
    let oldestAccess = Infinity;

    for (const [key, entry] of this.responseCache) {
      if (entry.lastAccess < oldestAccess) {
        oldestAccess = entry.lastAccess;
        oldestKey = key;
      }
    }

    if (oldestKey) {
      this.responseCache.delete(oldestKey);
    }
  }

  clear(): void {
    this.cache.clear();
    this.responseCache.clear();
  }

  invalidate(key: string): void {
    this.cache.delete(key);
  }

  invalidateResponses(): void {
    this.responseCache.clear();
  }

  getStats(): { cacheSize: number; responseCacheSize: number; maxSize: number } {
    return {
      cacheSize: this.cache.size,
      responseCacheSize: this.responseCache.size,
      maxSize: this.maxSize,
    };
  }
}
