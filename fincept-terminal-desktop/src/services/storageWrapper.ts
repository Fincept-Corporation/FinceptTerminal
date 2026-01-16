// Storage Wrapper - Pure SQLite storage (NO localStorage)
// All data stored in SQLite database via Tauri commands

import { invoke } from '@tauri-apps/api/core';

class StorageWrapper {
  /**
   * Get item from SQLite storage
   */
  async getItem(key: string): Promise<string | null> {
    try {
      const value = await invoke<string>('storage_get_optional', { key });
      return value || null;
    } catch (error) {
      console.error(`[StorageWrapper] Failed to get "${key}":`, error);
      return null;
    }
  }

  /**
   * Set item in SQLite storage
   */
  async setItem(key: string, value: string): Promise<void> {
    try {
      await invoke('storage_set', { key, value });
    } catch (error) {
      console.error(`[StorageWrapper] Failed to set "${key}":`, error);
      throw error;
    }
  }

  /**
   * Remove item from SQLite storage
   */
  async removeItem(key: string): Promise<void> {
    try {
      await invoke('storage_remove', { key });
    } catch (error) {
      console.error(`[StorageWrapper] Failed to remove "${key}":`, error);
      throw error;
    }
  }

  /**
   * Clear all storage
   */
  async clear(): Promise<void> {
    try {
      await invoke('storage_clear');
      console.log('[StorageWrapper] All storage cleared');
    } catch (error) {
      console.error('[StorageWrapper] Failed to clear storage:', error);
      throw error;
    }
  }

  /**
   * Check if a key exists in storage
   */
  async has(key: string): Promise<boolean> {
    try {
      return await invoke<boolean>('storage_has', { key });
    } catch (error) {
      console.error(`[StorageWrapper] Failed to check "${key}":`, error);
      return false;
    }
  }

  /**
   * Get all keys in storage
   */
  async keys(): Promise<string[]> {
    try {
      return await invoke<string[]>('storage_keys');
    } catch (error) {
      console.error('[StorageWrapper] Failed to get keys:', error);
      return [];
    }
  }

  /**
   * Get storage size (number of entries)
   */
  async size(): Promise<number> {
    try {
      return await invoke<number>('storage_size');
    } catch (error) {
      console.error('[StorageWrapper] Failed to get size:', error);
      return 0;
    }
  }

  /**
   * Get multiple values at once (batch operation)
   */
  async getMany(keys: string[]): Promise<Map<string, string | null>> {
    try {
      const results = await invoke<Array<{ key: string; value: string | null }>>('storage_get_many', { keys });
      const map = new Map<string, string | null>();
      for (const { key, value } of results) {
        map.set(key, value);
      }
      return map;
    } catch (error) {
      console.error('[StorageWrapper] Failed to get many:', error);
      return new Map();
    }
  }

  /**
   * Set multiple values at once (batch operation)
   */
  async setMany(entries: Array<[string, string]>): Promise<void> {
    try {
      await invoke('storage_set_many', { entries });
    } catch (error) {
      console.error('[StorageWrapper] Failed to set many:', error);
      throw error;
    }
  }

  /**
   * Synchronous getItem (DEPRECATED - for backward compatibility only)
   * Always returns null - use async getItem() instead
   */
  getItemSync(key: string): string | null {
    console.warn(
      `[StorageWrapper] getItemSync("${key}") is deprecated and always returns null. ` +
      'Use async getItem() instead. Pure SQLite storage requires async operations.'
    );
    return null;
  }

  /**
   * Storage is always available (SQLite-based)
   */
  isAvailable(): boolean {
    return true;
  }

  /**
   * Recheck availability (no-op for SQLite)
   */
  recheckAvailability(): boolean {
    return true;
  }
}

export const storageWrapper = new StorageWrapper();
export default storageWrapper;
