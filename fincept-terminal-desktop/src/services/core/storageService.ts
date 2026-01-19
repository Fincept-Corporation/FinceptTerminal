// Storage Service - Unified abstraction for persistent key-value storage
// Uses SQLite backend via Rust

import { invoke } from '@tauri-apps/api/core';

export interface StorageEntry {
  key: string;
  value: string | null;
}

/**
 * Persistent key-value storage service
 * All data stored in SQLite: key_value_storage table
 */
export const storageService = {
  /**
   * Get a value by key
   * @returns value or null if not found
   */
  async get(key: string): Promise<string | null> {
    try {
      return await invoke<string | null>('storage_get_optional', { key });
    } catch (error) {
      console.error(`[StorageService] Failed to get "${key}":`, error);
      return null;
    }
  },

  /**
   * Get a value and parse as JSON
   * @returns parsed object or null if not found/invalid
   */
  async getJSON<T>(key: string): Promise<T | null> {
    const value = await this.get(key);
    if (!value) return null;
    try {
      return JSON.parse(value) as T;
    } catch {
      console.error(`[StorageService] Failed to parse JSON for "${key}"`);
      return null;
    }
  },

  /**
   * Set a value
   */
  async set(key: string, value: string): Promise<boolean> {
    try {
      await invoke('storage_set', { key, value });
      return true;
    } catch (error) {
      console.error(`[StorageService] Failed to set "${key}":`, error);
      return false;
    }
  },

  /**
   * Set a value as JSON
   */
  async setJSON<T>(key: string, value: T): Promise<boolean> {
    return this.set(key, JSON.stringify(value));
  },

  /**
   * Remove a value
   */
  async remove(key: string): Promise<boolean> {
    try {
      await invoke('storage_remove', { key });
      return true;
    } catch (error) {
      console.error(`[StorageService] Failed to remove "${key}":`, error);
      return false;
    }
  },

  /**
   * Check if key exists
   */
  async has(key: string): Promise<boolean> {
    try {
      return await invoke<boolean>('storage_has', { key });
    } catch (error) {
      console.error(`[StorageService] Failed to check "${key}":`, error);
      return false;
    }
  },

  /**
   * Get all storage keys
   */
  async keys(): Promise<string[]> {
    try {
      return await invoke<string[]>('storage_keys');
    } catch (error) {
      console.error('[StorageService] Failed to get keys:', error);
      return [];
    }
  },

  /**
   * Get keys matching a prefix
   */
  async keysWithPrefix(prefix: string): Promise<string[]> {
    const allKeys = await this.keys();
    return allKeys.filter(k => k.startsWith(prefix));
  },

  /**
   * Get multiple values at once
   */
  async getMany(keys: string[]): Promise<StorageEntry[]> {
    try {
      return await invoke<StorageEntry[]>('storage_get_many', { keys });
    } catch (error) {
      console.error('[StorageService] Failed to get many:', error);
      return keys.map(key => ({ key, value: null }));
    }
  },

  /**
   * Set multiple values at once
   */
  async setMany(entries: Array<{ key: string; value: string }>): Promise<boolean> {
    try {
      const pairs = entries.map(e => [e.key, e.value] as [string, string]);
      await invoke('storage_set_many', { entries: pairs });
      return true;
    } catch (error) {
      console.error('[StorageService] Failed to set many:', error);
      return false;
    }
  },

  /**
   * Remove multiple keys at once
   */
  async removeMany(keys: string[]): Promise<boolean> {
    try {
      for (const key of keys) {
        await invoke('storage_remove', { key });
      }
      return true;
    } catch (error) {
      console.error('[StorageService] Failed to remove many:', error);
      return false;
    }
  },

  /**
   * Remove all keys matching a prefix
   */
  async removeWithPrefix(prefix: string): Promise<number> {
    const keys = await this.keysWithPrefix(prefix);
    if (keys.length === 0) return 0;
    await this.removeMany(keys);
    return keys.length;
  },

  /**
   * Get storage size (number of entries)
   */
  async size(): Promise<number> {
    try {
      return await invoke<number>('storage_size');
    } catch (error) {
      console.error('[StorageService] Failed to get size:', error);
      return 0;
    }
  },

  /**
   * Clear all storage (use with caution)
   */
  async clear(): Promise<boolean> {
    try {
      await invoke('storage_clear');
      return true;
    } catch (error) {
      console.error('[StorageService] Failed to clear:', error);
      return false;
    }
  },
};

export default storageService;
