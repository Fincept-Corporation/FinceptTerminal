// Unified Cache Service - Production-ready TypeScript wrapper for Rust cache backend
// Uses separate SQLite database: fincept_cache.db
// Features: Initialization checks, versioning, size limits, LRU eviction

import { invoke } from '@tauri-apps/api/core';

// ============================================================================
// Configuration
// ============================================================================

const CACHE_VERSION = '1.0.0'; // Increment when cache format changes
const CACHE_VERSION_KEY = '__cache_version__';
const MAX_CACHE_SIZE_MB = 100; // Maximum cache size in MB
const MAX_CACHE_ENTRIES = 10000; // Maximum number of cache entries
const CLEANUP_THRESHOLD = 0.9; // Start cleanup when cache is 90% full

// ============================================================================
// Types
// ============================================================================

export interface CacheEntry {
  cache_key: string;
  category: string;
  data: string;
  ttl_seconds: number;
  created_at: number;
  expires_at: number;
  last_accessed_at: number;
  hit_count: number;
  is_expired: boolean;
}

export interface TabSession {
  tab_id: string;
  tab_name: string;
  state: string;
  scroll_position: string | null;
  active_filters: string | null;
  selected_items: string | null;
  updated_at: number;
  created_at: number;
}

export interface CacheStats {
  total_entries: number;
  total_size_bytes: number;
  expired_entries: number;
  categories: CategoryStats[];
}

export interface CategoryStats {
  category: string;
  entry_count: number;
  total_size: number;
}

// TTL presets in seconds
export const TTL = {
  '1m': 60,
  '5m': 300,
  '10m': 600,
  '15m': 900,
  '30m': 1800,
  '1h': 3600,
  '6h': 21600,
  '12h': 43200,
  '24h': 86400,
  '7d': 604800,
  '30d': 2592000,
} as const;

export type TTLPreset = keyof typeof TTL;

// Backward compatibility alias
export const CacheTTL = {
  SHORT: TTL['1m'],
  MEDIUM: TTL['5m'],
  LONG: TTL['10m'],
  HOUR: TTL['1h'],
  DAY: TTL['24h'],
  WEEK: TTL['7d'],
} as const;

// Cache categories with default TTLs
export const CACHE_CATEGORIES = {
  'market-quotes': TTL['5m'],
  'market-historical': TTL['1h'],
  'news': TTL['10m'],
  'forum': TTL['5m'],
  'economic': TTL['1h'],
  'reference': TTL['24h'],
  'user-data': TTL['30d'],
  'api-response': TTL['15m'],
} as const;

export type CacheCategory = keyof typeof CACHE_CATEGORIES;

// ============================================================================
// Helper Functions
// ============================================================================

function parseTTL(ttl: number | TTLPreset): number {
  if (typeof ttl === 'number') return ttl;
  return TTL[ttl] || 600; // Default 10 minutes
}

// ============================================================================
// Cache Service Class
// ============================================================================

export class CacheService {
  private initialized = false;
  private initializing = false;
  private initPromise: Promise<boolean> | null = null;
  private cleanupInterval: ReturnType<typeof setInterval> | null = null;

  // ============================================================================
  // Initialization
  // ============================================================================

  /**
   * Initialize the cache service
   * Checks version and clears cache if version mismatch
   */
  async initialize(): Promise<boolean> {
    if (this.initialized) return true;

    // Prevent concurrent initialization
    if (this.initializing && this.initPromise) {
      return this.initPromise;
    }

    this.initializing = true;
    this.initPromise = this._doInitialize();

    try {
      const result = await this.initPromise;
      this.initialized = result;
      return result;
    } finally {
      this.initializing = false;
    }
  }

  private async _doInitialize(): Promise<boolean> {
    try {
      // Check if Rust backend is available
      const isHealthy = await this.healthCheck();
      if (!isHealthy) {
        console.error('[CacheService] Rust backend not available');
        return false;
      }

      // Check cache version
      const storedVersion = await this.getRawValue(CACHE_VERSION_KEY);
      if (storedVersion !== CACHE_VERSION) {
        console.log(`[CacheService] Cache version mismatch (${storedVersion} -> ${CACHE_VERSION}), clearing cache`);
        await this.clearAll();
        await this.setRawValue(CACHE_VERSION_KEY, CACHE_VERSION, 'system', TTL['30d']);
      }

      // Start background cleanup
      this.startBackgroundCleanup();

      console.log('[CacheService] Initialized successfully');
      return true;
    } catch (error) {
      console.error('[CacheService] Initialization failed:', error);
      return false;
    }
  }

  /**
   * Check if Rust cache backend is healthy
   */
  async healthCheck(): Promise<boolean> {
    try {
      // Try a simple operation to verify backend is working
      await invoke('cache_stats');
      return true;
    } catch (error) {
      console.error('[CacheService] Health check failed:', error);
      return false;
    }
  }

  /**
   * Ensure initialized before operations
   */
  private async ensureInitialized(): Promise<void> {
    if (!this.initialized) {
      await this.initialize();
    }
  }

  // ============================================================================
  // Core Cache Methods
  // ============================================================================

  /**
   * Get a cached value (returns null if expired or not found)
   */
  async get<T>(key: string): Promise<{ data: T; isStale: boolean } | null> {
    await this.ensureInitialized();

    try {
      const entry = await invoke<CacheEntry | null>('cache_get', { key });
      if (!entry) return null;
      return {
        data: JSON.parse(entry.data) as T,
        isStale: entry.is_expired,
      };
    } catch (error) {
      console.error('[CacheService] get error:', error);
      return null;
    }
  }

  /**
   * Get cached value even if stale (for stale-while-revalidate)
   */
  async getWithStale<T>(key: string): Promise<{ data: T; isStale: boolean } | null> {
    await this.ensureInitialized();

    try {
      const entry = await invoke<CacheEntry | null>('cache_get_with_stale', { key });
      if (!entry) return null;
      return {
        data: JSON.parse(entry.data) as T,
        isStale: entry.is_expired,
      };
    } catch (error) {
      console.error('[CacheService] getWithStale error:', error);
      return null;
    }
  }

  /**
   * Set a cached value with automatic size management
   */
  async set<T>(
    key: string,
    data: T,
    category: CacheCategory | string,
    ttl?: number | TTLPreset
  ): Promise<boolean> {
    await this.ensureInitialized();

    try {
      // Check cache size before adding
      await this.ensureCacheSize();

      const ttlSeconds = ttl
        ? parseTTL(ttl)
        : CACHE_CATEGORIES[category as CacheCategory] || 600;

      await invoke('cache_set', {
        key,
        data: JSON.stringify(data),
        category,
        ttlSeconds,
      });
      return true;
    } catch (error) {
      console.error('[CacheService] set error:', error);
      return false;
    }
  }

  /**
   * Delete a cached value
   */
  async delete(key: string): Promise<boolean> {
    await this.ensureInitialized();

    try {
      return await invoke<boolean>('cache_delete', { key });
    } catch (error) {
      console.error('[CacheService] delete error:', error);
      return false;
    }
  }

  /**
   * Get multiple cached values
   */
  async getMany<T>(keys: string[]): Promise<Map<string, T>> {
    await this.ensureInitialized();

    try {
      const entries = await invoke<CacheEntry[]>('cache_get_many', { keys });
      const result = new Map<string, T>();
      for (const entry of entries) {
        try {
          result.set(entry.cache_key, JSON.parse(entry.data) as T);
        } catch {
          // Skip invalid entries
        }
      }
      return result;
    } catch (error) {
      console.error('[CacheService] getMany error:', error);
      return new Map();
    }
  }

  // ============================================================================
  // Size Management & LRU Eviction
  // ============================================================================

  /**
   * Ensure cache size is within limits, evict LRU entries if needed
   */
  private async ensureCacheSize(): Promise<void> {
    try {
      const stats = await this.getStats();
      if (!stats) return;

      const sizeBytes = stats.total_size_bytes;
      const maxSizeBytes = MAX_CACHE_SIZE_MB * 1024 * 1024;
      const entryCount = stats.total_entries;

      // Check if we need to evict
      const sizeExceeded = sizeBytes > maxSizeBytes * CLEANUP_THRESHOLD;
      const countExceeded = entryCount > MAX_CACHE_ENTRIES * CLEANUP_THRESHOLD;

      if (sizeExceeded || countExceeded) {
        console.log(`[CacheService] Cache cleanup triggered - Size: ${(sizeBytes / 1024 / 1024).toFixed(2)}MB, Entries: ${entryCount}`);

        // First, remove expired entries
        const expiredRemoved = await this.cleanup();
        console.log(`[CacheService] Removed ${expiredRemoved} expired entries`);

        // If still over limit, use LRU eviction
        const newStats = await this.getStats();
        if (newStats && (newStats.total_size_bytes > maxSizeBytes * 0.8 || newStats.total_entries > MAX_CACHE_ENTRIES * 0.8)) {
          const targetRemoval = Math.max(
            Math.floor(newStats.total_entries * 0.2), // Remove 20% of entries
            100 // Minimum 100 entries
          );
          const lruRemoved = await this.evictLRU(targetRemoval);
          console.log(`[CacheService] LRU evicted ${lruRemoved} entries`);
        }
      }
    } catch (error) {
      console.error('[CacheService] ensureCacheSize error:', error);
    }
  }

  /**
   * Evict least recently used entries
   */
  private async evictLRU(count: number): Promise<number> {
    try {
      return await invoke<number>('cache_evict_lru', { count });
    } catch (error) {
      // Fallback: try to use pattern invalidation if LRU not available
      console.warn('[CacheService] LRU eviction not available, using cleanup fallback');
      return await this.cleanup();
    }
  }

  // ============================================================================
  // Category & Pattern Operations
  // ============================================================================

  /**
   * Invalidate all entries in a category
   */
  async invalidateCategory(category: string): Promise<number> {
    await this.ensureInitialized();

    try {
      return await invoke<number>('cache_invalidate_category', { category });
    } catch (error) {
      console.error('[CacheService] invalidateCategory error:', error);
      return 0;
    }
  }

  /**
   * Invalidate entries matching a pattern (use % for wildcard)
   */
  async invalidatePattern(pattern: string): Promise<number> {
    await this.ensureInitialized();

    try {
      return await invoke<number>('cache_invalidate_pattern', { pattern });
    } catch (error) {
      console.error('[CacheService] invalidatePattern error:', error);
      return 0;
    }
  }

  // ============================================================================
  // Cleanup & Maintenance
  // ============================================================================

  /**
   * Remove all expired entries
   */
  async cleanup(): Promise<number> {
    try {
      return await invoke<number>('cache_cleanup');
    } catch (error) {
      console.error('[CacheService] cleanup error:', error);
      return 0;
    }
  }

  /**
   * Get cache statistics
   */
  async getStats(): Promise<CacheStats | null> {
    try {
      return await invoke<CacheStats>('cache_stats');
    } catch (error) {
      console.error('[CacheService] getStats error:', error);
      return null;
    }
  }

  /**
   * Alias for getStats (backward compatibility)
   */
  async stats(): Promise<CacheStats | null> {
    return this.getStats();
  }

  /**
   * Clear entire cache (except version key)
   */
  async clearAll(): Promise<number> {
    try {
      const result = await invoke<number>('cache_clear_all');
      // Restore version key
      await this.setRawValue(CACHE_VERSION_KEY, CACHE_VERSION, 'system', TTL['30d']);
      return result;
    } catch (error) {
      console.error('[CacheService] clearAll error:', error);
      return 0;
    }
  }

  /**
   * Start background cleanup (runs every 15 minutes by default)
   */
  startBackgroundCleanup(intervalMs: number = 15 * 60 * 1000): () => void {
    // Clear any existing interval
    if (this.cleanupInterval) {
      clearInterval(this.cleanupInterval);
    }

    this.cleanupInterval = setInterval(async () => {
      try {
        const deleted = await this.cleanup();
        if (deleted > 0) {
          console.log(`[CacheService] Background cleanup: removed ${deleted} expired entries`);
        }

        // Also check size limits
        await this.ensureCacheSize();
      } catch (error) {
        console.error('[CacheService] Background cleanup error:', error);
      }
    }, intervalMs);

    return () => {
      if (this.cleanupInterval) {
        clearInterval(this.cleanupInterval);
        this.cleanupInterval = null;
      }
    };
  }

  /**
   * Stop background cleanup
   */
  stopBackgroundCleanup(): void {
    if (this.cleanupInterval) {
      clearInterval(this.cleanupInterval);
      this.cleanupInterval = null;
    }
  }

  // ============================================================================
  // Tab Session Methods
  // ============================================================================

  /**
   * Get tab session state
   */
  async getTabSession<T>(tabId: string): Promise<T | null> {
    await this.ensureInitialized();

    try {
      const session = await invoke<TabSession | null>('tab_session_get', { tabId });
      if (!session) return null;
      return JSON.parse(session.state) as T;
    } catch (error) {
      console.error('[CacheService] getTabSession error:', error);
      return null;
    }
  }

  /**
   * Save tab session state
   */
  async setTabSession<T>(
    tabId: string,
    tabName: string,
    state: T,
    scrollPosition?: string,
    activeFilters?: string,
    selectedItems?: string
  ): Promise<boolean> {
    await this.ensureInitialized();

    try {
      await invoke('tab_session_set', {
        tabId,
        tabName,
        state: JSON.stringify(state),
        scrollPosition: scrollPosition || null,
        activeFilters: activeFilters || null,
        selectedItems: selectedItems || null,
      });
      return true;
    } catch (error) {
      console.error('[CacheService] setTabSession error:', error);
      return false;
    }
  }

  /**
   * Delete tab session
   */
  async deleteTabSession(tabId: string): Promise<boolean> {
    await this.ensureInitialized();

    try {
      return await invoke<boolean>('tab_session_delete', { tabId });
    } catch (error) {
      console.error('[CacheService] deleteTabSession error:', error);
      return false;
    }
  }

  /**
   * Get all tab sessions
   */
  async getAllTabSessions(): Promise<TabSession[]> {
    await this.ensureInitialized();

    try {
      return await invoke<TabSession[]>('tab_session_get_all');
    } catch (error) {
      console.error('[CacheService] getAllTabSessions error:', error);
      return [];
    }
  }

  /**
   * Cleanup old tab sessions
   */
  async cleanupTabSessions(maxAgeDays: number = 30): Promise<number> {
    await this.ensureInitialized();

    try {
      return await invoke<number>('tab_session_cleanup', { maxAgeDays });
    } catch (error) {
      console.error('[CacheService] cleanupTabSessions error:', error);
      return 0;
    }
  }

  // ============================================================================
  // Internal Helpers
  // ============================================================================

  /**
   * Get raw string value (for internal use like version checking)
   */
  private async getRawValue(key: string): Promise<string | null> {
    try {
      const entry = await invoke<CacheEntry | null>('cache_get_with_stale', { key });
      if (!entry) return null;
      return JSON.parse(entry.data) as string;
    } catch {
      return null;
    }
  }

  /**
   * Set raw string value (for internal use like version checking)
   */
  private async setRawValue(key: string, value: string, category: string, ttlSeconds: number): Promise<boolean> {
    try {
      await invoke('cache_set', {
        key,
        data: JSON.stringify(value),
        category,
        ttlSeconds,
      });
      return true;
    } catch {
      return false;
    }
  }

  // ============================================================================
  // Backward Compatibility Methods
  // ============================================================================

  /**
   * Get raw string data from cache
   */
  async getData(key: string): Promise<string | null> {
    const result = await this.get<string>(key);
    return result?.data ?? null;
  }

  /**
   * Get cached data as JSON (backward compatibility)
   */
  async getJSON<T>(key: string): Promise<T | null> {
    const result = await this.get<T>(key);
    return result?.data ?? null;
  }

  /**
   * Set cached data as JSON (backward compatibility)
   */
  async setJSON<T>(
    key: string,
    value: T,
    category: CacheCategory | string,
    ttlSeconds: number = 300
  ): Promise<boolean> {
    return this.set(key, value, category, ttlSeconds);
  }

  // ============================================================================
  // Static Utilities
  // ============================================================================

  /**
   * Generate cache key with consistent format
   */
  static key(category: string, ...parts: (string | number)[]): string {
    return `${category}:${parts.join(':')}`;
  }

  /**
   * Generate cache key with hash for dynamic data
   */
  static keyWithHash(category: string, identifier: string, items: string[]): string {
    if (items.length === 0) return `${category}:${identifier}:empty`;
    const hash = `${items.length}:${items[0]}:${items[items.length - 1]}`;
    return `${category}:${identifier}:${hash}`;
  }

  /**
   * Get current cache version
   */
  static getVersion(): string {
    return CACHE_VERSION;
  }
}

// Export singleton instance
export const cacheService = new CacheService();
export default cacheService;
