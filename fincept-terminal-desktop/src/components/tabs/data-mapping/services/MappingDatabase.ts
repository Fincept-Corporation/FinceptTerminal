// Mapping Database - LocalStorage-based storage for mappings and cache
// Migrated from SQLite to LocalStorage for simplicity

import { DataMappingConfig } from '../types';
import { encryptionService } from './EncryptionService';

const STORAGE_KEYS = {
  MAPPINGS: 'fincept_data_mappings',
  CACHE: 'fincept_mapping_cache',
  PREFERENCES: 'fincept_mapping_preferences',
};

interface CacheEntry {
  cache_key: string;
  mapping_id: string;
  params_hash: string;
  response_json: string;
  cached_at: string;
  expires_at: string;
}

/**
 * Database service for storing mappings and API response cache
 * Uses localStorage for persistence
 */
export class MappingDatabase {
  private initialized = false;

  /**
   * Initialize database (localStorage setup)
   */
  async initialize(): Promise<void> {
    if (this.initialized) return;

    try {
      // Ensure storage keys exist
      if (!localStorage.getItem(STORAGE_KEYS.MAPPINGS)) {
        localStorage.setItem(STORAGE_KEYS.MAPPINGS, JSON.stringify({}));
      }
      if (!localStorage.getItem(STORAGE_KEYS.CACHE)) {
        localStorage.setItem(STORAGE_KEYS.CACHE, JSON.stringify({}));
      }
      if (!localStorage.getItem(STORAGE_KEYS.PREFERENCES)) {
        localStorage.setItem(STORAGE_KEYS.PREFERENCES, JSON.stringify({}));
      }

      this.initialized = true;
      console.log('MappingDatabase initialized successfully (localStorage)');
    } catch (error) {
      console.error('Failed to initialize MappingDatabase:', error);
      throw error;
    }
  }

  /**
   * Save mapping configuration
   */
  async saveMapping(config: DataMappingConfig): Promise<void> {
    await this.ensureInitialized();

    try {
      // Encrypt sensitive data if enabled
      const encryptionEnabled = await encryptionService.isEncryptionEnabled();
      let configToStore = { ...config };

      if (encryptionEnabled && config.source.type === 'api' && config.source.apiConfig) {
        // Encrypt API credentials
        const apiConfig = { ...config.source.apiConfig };

        if (apiConfig.authentication.config) {
          const authConfig = { ...apiConfig.authentication.config };

          if (authConfig.apiKey) {
            authConfig.apiKey = {
              ...authConfig.apiKey,
              value: await encryptionService.encrypt(authConfig.apiKey.value),
            };
          }
          if (authConfig.bearer) {
            authConfig.bearer = {
              token: await encryptionService.encrypt(authConfig.bearer.token),
            };
          }
          if (authConfig.basic) {
            authConfig.basic = {
              username: authConfig.basic.username,
              password: await encryptionService.encrypt(authConfig.basic.password),
            };
          }
          if (authConfig.oauth2) {
            authConfig.oauth2 = {
              accessToken: await encryptionService.encrypt(authConfig.oauth2.accessToken),
              refreshToken: authConfig.oauth2.refreshToken
                ? await encryptionService.encrypt(authConfig.oauth2.refreshToken)
                : undefined,
            };
          }

          apiConfig.authentication = {
            ...apiConfig.authentication,
            config: authConfig,
          };
        }

        configToStore = {
          ...configToStore,
          source: {
            ...configToStore.source,
            apiConfig,
          },
        };
      }

      // Save to localStorage
      const mappings = this.getMappingsFromStorage();
      mappings[config.id] = configToStore;
      localStorage.setItem(STORAGE_KEYS.MAPPINGS, JSON.stringify(mappings));

      console.log(`Mapping '${config.name}' saved successfully`);
    } catch (error) {
      console.error('Failed to save mapping:', error);
      throw error;
    }
  }

  /**
   * Get mapping by ID
   */
  async getMapping(id: string): Promise<DataMappingConfig | null> {
    await this.ensureInitialized();

    try {
      const mappings = this.getMappingsFromStorage();
      const config = mappings[id];

      if (!config) {
        return null;
      }

      // Decrypt sensitive data if needed
      const encryptionEnabled = await encryptionService.isEncryptionEnabled();
      if (encryptionEnabled && config.source.type === 'api' && config.source.apiConfig) {
        const apiConfig = { ...config.source.apiConfig };

        if (apiConfig.authentication.config) {
          const authConfig = { ...apiConfig.authentication.config };

          if (authConfig.apiKey) {
            authConfig.apiKey = {
              ...authConfig.apiKey,
              value: await encryptionService.decrypt(authConfig.apiKey.value),
            };
          }
          if (authConfig.bearer) {
            authConfig.bearer = {
              token: await encryptionService.decrypt(authConfig.bearer.token),
            };
          }
          if (authConfig.basic) {
            authConfig.basic = {
              username: authConfig.basic.username,
              password: await encryptionService.decrypt(authConfig.basic.password),
            };
          }
          if (authConfig.oauth2) {
            authConfig.oauth2 = {
              accessToken: await encryptionService.decrypt(authConfig.oauth2.accessToken),
              refreshToken: authConfig.oauth2.refreshToken
                ? await encryptionService.decrypt(authConfig.oauth2.refreshToken)
                : undefined,
            };
          }

          apiConfig.authentication = {
            ...apiConfig.authentication,
            config: authConfig,
          };
        }

        config.source = {
          ...config.source,
          apiConfig,
        };
      }

      return config;
    } catch (error) {
      console.error('Failed to get mapping:', error);
      return null;
    }
  }

  /**
   * Get all mappings
   */
  async getAllMappings(): Promise<DataMappingConfig[]> {
    await this.ensureInitialized();

    try {
      const mappings = this.getMappingsFromStorage();
      const mappingArray = Object.values(mappings);

      // Sort by updated_at DESC
      return mappingArray.sort((a, b) =>
        new Date(b.metadata.updatedAt).getTime() - new Date(a.metadata.updatedAt).getTime()
      );
    } catch (error) {
      console.error('Failed to get all mappings:', error);
      return [];
    }
  }

  /**
   * Delete mapping
   */
  async deleteMapping(id: string): Promise<void> {
    await this.ensureInitialized();

    try {
      const mappings = this.getMappingsFromStorage();
      delete mappings[id];
      localStorage.setItem(STORAGE_KEYS.MAPPINGS, JSON.stringify(mappings));

      // Also clear related cache
      await this.clearCache(id);

      console.log(`Mapping ${id} deleted`);
    } catch (error) {
      console.error('Failed to delete mapping:', error);
      throw error;
    }
  }

  /**
   * Get cached response
   */
  async getCachedResponse(mappingId: string, params: Record<string, any>): Promise<any | null> {
    await this.ensureInitialized();

    try {
      const paramsHash = this.hashParams(params);
      const cacheKey = `${mappingId}_${paramsHash}`;
      const cache = this.getCacheFromStorage();
      const entry = cache[cacheKey];

      if (!entry) {
        return null;
      }

      // Check expiration
      if (new Date(entry.expires_at) <= new Date()) {
        delete cache[cacheKey];
        localStorage.setItem(STORAGE_KEYS.CACHE, JSON.stringify(cache));
        return null;
      }

      console.log(`Cache hit for mapping ${mappingId}`);
      return JSON.parse(entry.response_json);
    } catch (error) {
      console.error('Failed to get cached response:', error);
      return null;
    }
  }

  /**
   * Set cached response
   */
  async setCachedResponse(
    mappingId: string,
    params: Record<string, any>,
    response: any,
    ttlSeconds: number
  ): Promise<void> {
    await this.ensureInitialized();

    try {
      const paramsHash = this.hashParams(params);
      const cacheKey = `${mappingId}_${paramsHash}`;
      const now = new Date().toISOString();
      const expiresAt = new Date(Date.now() + ttlSeconds * 1000).toISOString();
      const responseJson = JSON.stringify(response);

      const cache = this.getCacheFromStorage();
      cache[cacheKey] = {
        cache_key: cacheKey,
        mapping_id: mappingId,
        params_hash: paramsHash,
        response_json: responseJson,
        cached_at: now,
        expires_at: expiresAt,
      };

      localStorage.setItem(STORAGE_KEYS.CACHE, JSON.stringify(cache));
      console.log(`Response cached for mapping ${mappingId}`);
    } catch (error) {
      console.error('Failed to cache response:', error);
      // Don't throw - caching is optional
    }
  }

  /**
   * Clear cache for a specific mapping or all
   */
  async clearCache(mappingId?: string): Promise<void> {
    await this.ensureInitialized();

    try {
      const cache = this.getCacheFromStorage();

      if (mappingId) {
        // Clear cache for specific mapping
        const filtered = Object.fromEntries(
          Object.entries(cache).filter(([_, entry]) => entry.mapping_id !== mappingId)
        );
        localStorage.setItem(STORAGE_KEYS.CACHE, JSON.stringify(filtered));
        console.log(`Cache cleared for mapping ${mappingId}`);
      } else {
        // Clear all cache
        localStorage.setItem(STORAGE_KEYS.CACHE, JSON.stringify({}));
        console.log('All cache cleared');
      }
    } catch (error) {
      console.error('Failed to clear cache:', error);
      throw error;
    }
  }

  /**
   * Clean expired cache entries
   */
  async cleanExpiredCache(): Promise<void> {
    await this.ensureInitialized();

    try {
      const cache = this.getCacheFromStorage();
      const now = new Date();

      const filtered = Object.fromEntries(
        Object.entries(cache).filter(([_, entry]) => new Date(entry.expires_at) > now)
      );

      localStorage.setItem(STORAGE_KEYS.CACHE, JSON.stringify(filtered));
      console.log('Expired cache entries cleaned');
    } catch (error) {
      console.error('Failed to clean expired cache:', error);
    }
  }

  /**
   * Get/Set user preferences
   */
  async getPreference(key: string): Promise<string | null> {
    await this.ensureInitialized();

    try {
      const prefs = this.getPreferencesFromStorage();
      return prefs[key] || null;
    } catch (error) {
      console.error('Failed to get preference:', error);
      return null;
    }
  }

  async setPreference(key: string, value: string): Promise<void> {
    await this.ensureInitialized();

    try {
      const prefs = this.getPreferencesFromStorage();
      prefs[key] = value;
      localStorage.setItem(STORAGE_KEYS.PREFERENCES, JSON.stringify(prefs));
    } catch (error) {
      console.error('Failed to set preference:', error);
      throw error;
    }
  }

  /**
   * Duplicate mapping
   */
  async duplicateMapping(id: string): Promise<DataMappingConfig | null> {
    const original = await this.getMapping(id);
    if (!original) return null;

    const duplicate: DataMappingConfig = {
      ...original,
      id: `map_${Date.now()}`,
      name: `${original.name} (Copy)`,
      metadata: {
        ...original.metadata,
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
      },
    };

    await this.saveMapping(duplicate);
    return duplicate;
  }

  /**
   * Hash parameters for cache key
   */
  private hashParams(params: Record<string, any>): string {
    // Simple hash: serialize and create hash
    const str = JSON.stringify(params, Object.keys(params).sort());
    let hash = 0;
    for (let i = 0; i < str.length; i++) {
      const char = str.charCodeAt(i);
      hash = (hash << 5) - hash + char;
      hash = hash & hash; // Convert to 32-bit integer
    }
    return hash.toString(36);
  }

  /**
   * Ensure database is initialized
   */
  private async ensureInitialized(): Promise<void> {
    if (!this.initialized) {
      await this.initialize();
    }
  }

  /**
   * Get mappings from localStorage
   */
  private getMappingsFromStorage(): Record<string, DataMappingConfig> {
    const data = localStorage.getItem(STORAGE_KEYS.MAPPINGS);
    return data ? JSON.parse(data) : {};
  }

  /**
   * Get cache from localStorage
   */
  private getCacheFromStorage(): Record<string, CacheEntry> {
    const data = localStorage.getItem(STORAGE_KEYS.CACHE);
    return data ? JSON.parse(data) : {};
  }

  /**
   * Get preferences from localStorage
   */
  private getPreferencesFromStorage(): Record<string, string> {
    const data = localStorage.getItem(STORAGE_KEYS.PREFERENCES);
    return data ? JSON.parse(data) : {};
  }
}

// Export singleton instance
export const mappingDatabase = new MappingDatabase();
export default mappingDatabase;
