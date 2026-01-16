/**
 * Symbol Cache Service
 * Multi-tier caching: Memory (LRU) + IndexedDB
 */

import type { CacheEntry, CacheStats } from './types';
import { storageWrapper } from '../storageWrapper';

// ==================== LRU CACHE ====================

class LRUCache<T> {
  private cache: Map<string, CacheEntry<T>>;
  private accessOrder: string[];
  private maxSize: number;
  private hits: number = 0;
  private misses: number = 0;

  constructor(maxSize: number = 1000) {
    this.cache = new Map();
    this.accessOrder = [];
    this.maxSize = maxSize;
  }

  /**
   * Get value from cache
   */
  get(key: string): T | null {
    const entry = this.cache.get(key);

    if (!entry) {
      this.misses++;
      return null;
    }

    // Check if expired
    const now = Date.now();
    if (now - entry.timestamp > entry.ttl) {
      this.cache.delete(key);
      this.removeFromAccessOrder(key);
      this.misses++;
      return null;
    }

    // Update access order
    this.updateAccessOrder(key);
    this.hits++;
    return entry.data;
  }

  /**
   * Set value in cache
   */
  set(key: string, value: T, ttl: number): void {
    // Evict if at capacity
    if (this.cache.size >= this.maxSize && !this.cache.has(key)) {
      this.evictLRU();
    }

    const entry: CacheEntry<T> = {
      data: value,
      timestamp: Date.now(),
      ttl,
    };

    this.cache.set(key, entry);
    this.updateAccessOrder(key);
  }

  /**
   * Delete value from cache
   */
  delete(key: string): boolean {
    this.removeFromAccessOrder(key);
    return this.cache.delete(key);
  }

  /**
   * Clear entire cache
   */
  clear(): void {
    this.cache.clear();
    this.accessOrder = [];
    this.hits = 0;
    this.misses = 0;
  }

  /**
   * Get cache statistics
   */
  getStats(): CacheStats {
    const totalRequests = this.hits + this.misses;
    const hitRate = totalRequests > 0 ? this.hits / totalRequests : 0;

    // Estimate memory usage (rough approximation)
    const memoryUsage = this.cache.size * 1024; // ~1KB per entry (rough estimate)

    return {
      size: this.cache.size,
      hits: this.hits,
      misses: this.misses,
      hitRate,
      memoryUsage,
    };
  }

  /**
   * Evict least recently used entry
   */
  private evictLRU(): void {
    if (this.accessOrder.length === 0) return;

    const lruKey = this.accessOrder[0];
    this.cache.delete(lruKey);
    this.accessOrder.shift();
  }

  /**
   * Update access order for key
   */
  private updateAccessOrder(key: string): void {
    this.removeFromAccessOrder(key);
    this.accessOrder.push(key);
  }

  /**
   * Remove key from access order
   */
  private removeFromAccessOrder(key: string): void {
    const index = this.accessOrder.indexOf(key);
    if (index > -1) {
      this.accessOrder.splice(index, 1);
    }
  }

  /**
   * Get all keys
   */
  keys(): string[] {
    return Array.from(this.cache.keys());
  }

  /**
   * Get cache size
   */
  size(): number {
    return this.cache.size;
  }
}

// ==================== INDEXEDDB CACHE ====================

class IndexedDBCache {
  private dbName = 'SymbolMasterDB';
  private version = 1;
  private storeName = 'symbolCache';
  private db: IDBDatabase | null = null;
  private initialized = false;

  /**
   * Initialize IndexedDB
   */
  async initialize(): Promise<void> {
    if (this.initialized) return;

    return new Promise((resolve, reject) => {
      const request = indexedDB.open(this.dbName, this.version);

      request.onerror = () => {
        console.error('[SymbolCache] IndexedDB initialization failed:', request.error);
        reject(request.error);
      };

      request.onsuccess = () => {
        this.db = request.result;
        this.initialized = true;
        console.log('[SymbolCache] IndexedDB initialized');
        resolve();
      };

      request.onupgradeneeded = (event) => {
        const db = (event.target as IDBOpenDBRequest).result;

        // Create object store if it doesn't exist
        if (!db.objectStoreNames.contains(this.storeName)) {
          const objectStore = db.createObjectStore(this.storeName, { keyPath: 'key' });
          objectStore.createIndex('timestamp', 'timestamp', { unique: false });
        }
      };
    });
  }

  /**
   * Get value from IndexedDB
   */
  async get<T>(key: string): Promise<T | null> {
    if (!this.db) {
      await this.initialize();
    }

    return new Promise((resolve, reject) => {
      if (!this.db) {
        resolve(null);
        return;
      }

      const transaction = this.db.transaction([this.storeName], 'readonly');
      const objectStore = transaction.objectStore(this.storeName);
      const request = objectStore.get(key);

      request.onsuccess = () => {
        const entry = request.result as CacheEntry<T> & { key: string } | undefined;

        if (!entry) {
          resolve(null);
          return;
        }

        // Check if expired
        const now = Date.now();
        if (now - entry.timestamp > entry.ttl) {
          this.delete(key); // Clean up expired entry
          resolve(null);
          return;
        }

        resolve(entry.data);
      };

      request.onerror = () => {
        console.error('[SymbolCache] IndexedDB get error:', request.error);
        resolve(null); // Fail gracefully
      };
    });
  }

  /**
   * Set value in IndexedDB
   */
  async set<T>(key: string, value: T, ttl: number): Promise<void> {
    if (!this.db) {
      await this.initialize();
    }

    return new Promise((resolve, reject) => {
      if (!this.db) {
        resolve();
        return;
      }

      const entry = {
        key,
        data: value,
        timestamp: Date.now(),
        ttl,
      };

      const transaction = this.db.transaction([this.storeName], 'readwrite');
      const objectStore = transaction.objectStore(this.storeName);
      const request = objectStore.put(entry);

      request.onsuccess = () => resolve();
      request.onerror = () => {
        console.error('[SymbolCache] IndexedDB set error:', request.error);
        resolve(); // Fail gracefully
      };
    });
  }

  /**
   * Delete value from IndexedDB
   */
  async delete(key: string): Promise<void> {
    if (!this.db) return;

    return new Promise((resolve) => {
      if (!this.db) {
        resolve();
        return;
      }

      const transaction = this.db.transaction([this.storeName], 'readwrite');
      const objectStore = transaction.objectStore(this.storeName);
      const request = objectStore.delete(key);

      request.onsuccess = () => resolve();
      request.onerror = () => resolve(); // Fail gracefully
    });
  }

  /**
   * Clear all entries
   */
  async clear(): Promise<void> {
    if (!this.db) return;

    return new Promise((resolve) => {
      if (!this.db) {
        resolve();
        return;
      }

      const transaction = this.db.transaction([this.storeName], 'readwrite');
      const objectStore = transaction.objectStore(this.storeName);
      const request = objectStore.clear();

      request.onsuccess = () => resolve();
      request.onerror = () => resolve(); // Fail gracefully
    });
  }

  /**
   * Clean up expired entries
   */
  async cleanup(): Promise<number> {
    if (!this.db) return 0;

    return new Promise((resolve) => {
      if (!this.db) {
        resolve(0);
        return;
      }

      let deletedCount = 0;
      const now = Date.now();

      const transaction = this.db.transaction([this.storeName], 'readwrite');
      const objectStore = transaction.objectStore(this.storeName);
      const request = objectStore.openCursor();

      request.onsuccess = (event) => {
        const cursor = (event.target as IDBRequest).result;

        if (cursor) {
          const entry = cursor.value as CacheEntry<any> & { key: string };

          // Delete if expired
          if (now - entry.timestamp > entry.ttl) {
            cursor.delete();
            deletedCount++;
          }

          cursor.continue();
        } else {
          // Finished iterating
          resolve(deletedCount);
        }
      };

      request.onerror = () => {
        console.error('[SymbolCache] Cleanup error:', request.error);
        resolve(0);
      };
    });
  }
}

// ==================== SYMBOL CACHE SERVICE ====================

export class SymbolCacheService {
  private memoryCache: LRUCache<any>;
  private indexedDBCache: IndexedDBCache;
  private memoryCacheTTL: number;
  private indexedDBCacheTTL: number;
  private enableIndexedDB: boolean;

  constructor(
    memoryCacheSize: number = 1000,
    memoryCacheTTL: number = 5 * 60 * 1000, // 5 minutes
    indexedDBCacheTTL: number = 24 * 60 * 60 * 1000, // 24 hours
    enableIndexedDB: boolean = true
  ) {
    this.memoryCache = new LRUCache(memoryCacheSize);
    this.indexedDBCache = new IndexedDBCache();
    this.memoryCacheTTL = memoryCacheTTL;
    this.indexedDBCacheTTL = indexedDBCacheTTL;
    this.enableIndexedDB = enableIndexedDB;

    // Initialize IndexedDB in background
    if (this.enableIndexedDB) {
      this.indexedDBCache.initialize().catch(err => {
        console.error('[SymbolCache] Failed to initialize IndexedDB:', err);
      });
    }
  }

  /**
   * Get value from cache (checks memory first, then IndexedDB)
   */
  async get<T>(key: string): Promise<T | null> {
    // Try memory cache first
    const memoryValue = this.memoryCache.get(key);
    if (memoryValue !== null) {
      return memoryValue as T;
    }

    // Try IndexedDB if enabled
    if (this.enableIndexedDB) {
      const dbValue = await this.indexedDBCache.get<T>(key);
      if (dbValue !== null) {
        // Populate memory cache
        this.memoryCache.set(key, dbValue, this.memoryCacheTTL);
        return dbValue;
      }
    }

    return null;
  }

  /**
   * Set value in cache (writes to both memory and IndexedDB)
   */
  async set<T>(key: string, value: T): Promise<void> {
    // Write to memory cache
    this.memoryCache.set(key, value, this.memoryCacheTTL);

    // Write to IndexedDB if enabled
    if (this.enableIndexedDB) {
      await this.indexedDBCache.set(key, value, this.indexedDBCacheTTL);
    }
  }

  /**
   * Delete value from cache
   */
  async delete(key: string): Promise<void> {
    this.memoryCache.delete(key);

    if (this.enableIndexedDB) {
      await this.indexedDBCache.delete(key);
    }
  }

  /**
   * Clear all caches
   */
  async clear(): Promise<void> {
    this.memoryCache.clear();

    if (this.enableIndexedDB) {
      await this.indexedDBCache.clear();
    }
  }

  /**
   * Get cache statistics
   */
  getStats(): CacheStats {
    return this.memoryCache.getStats();
  }

  /**
   * Clean up expired entries in IndexedDB
   */
  async cleanupIndexedDB(): Promise<number> {
    if (!this.enableIndexedDB) return 0;
    return this.indexedDBCache.cleanup();
  }

  /**
   * Generate cache key
   */
  static generateKey(prefix: string, ...parts: string[]): string {
    return `${prefix}:${parts.join(':')}`;
  }
}

// Export singleton instance
export const symbolCacheService = new SymbolCacheService();
export default symbolCacheService;
