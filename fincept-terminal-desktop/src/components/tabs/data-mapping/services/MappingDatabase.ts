// Mapping Database - SQLite storage for mappings and cache

import { DataMappingConfig } from '../types';
import { encryptionService } from './EncryptionService';
import { sqliteService } from '@/services/sqliteService';

/**
 * Database service for storing mappings and API response cache
 */
export class MappingDatabase {
  private initialized = false;

  /**
   * Initialize database tables
   */
  async initialize(): Promise<void> {
    if (this.initialized) return;

    try {
      // Ensure main SQLite service is initialized
      if (!sqliteService.isReady()) {
        await sqliteService.initialize();
      }

      // Create mappings table
      await sqliteService.execute(`
        CREATE TABLE IF NOT EXISTS data_mappings (
          id TEXT PRIMARY KEY,
          name TEXT NOT NULL,
          description TEXT,
          config_json TEXT NOT NULL,
          created_at TEXT NOT NULL,
          updated_at TEXT NOT NULL
        )
      `);

      // Create cache table
      await sqliteService.execute(`
        CREATE TABLE IF NOT EXISTS mapping_cache (
          cache_key TEXT PRIMARY KEY,
          mapping_id TEXT NOT NULL,
          params_hash TEXT NOT NULL,
          response_json TEXT NOT NULL,
          cached_at TEXT NOT NULL,
          expires_at TEXT NOT NULL,
          FOREIGN KEY(mapping_id) REFERENCES data_mappings(id) ON DELETE CASCADE
        )
      `);

      // Create index for cache lookups
      await sqliteService.execute(`
        CREATE INDEX IF NOT EXISTS idx_mapping_cache_lookup
        ON mapping_cache(mapping_id, params_hash)
      `);

      // Create user preferences table
      await sqliteService.execute(`
        CREATE TABLE IF NOT EXISTS mapping_preferences (
          key TEXT PRIMARY KEY,
          value TEXT NOT NULL
        )
      `);

      this.initialized = true;
      console.log('MappingDatabase initialized successfully');
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

      // Serialize config
      const configJson = JSON.stringify(configToStore);

      // Upsert mapping
      await sqliteService.execute(
        `INSERT OR REPLACE INTO data_mappings (id, name, description, config_json, created_at, updated_at)
         VALUES (?, ?, ?, ?, ?, ?)`,
        [
          config.id,
          config.name,
          config.description,
          configJson,
          config.metadata.createdAt,
          config.metadata.updatedAt,
        ]
      );

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
      const result = await sqliteService.select<{ config_json: string }[]>(
        'SELECT config_json FROM data_mappings WHERE id = ?',
        [id]
      );

      if (!result || result.length === 0) {
        return null;
      }

      const config = JSON.parse(result[0].config_json) as DataMappingConfig;

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
      const results = await sqliteService.select<{ config_json: string }[]>(
        'SELECT config_json FROM data_mappings ORDER BY updated_at DESC'
      );

      if (!results || results.length === 0) {
        return [];
      }

      // Parse and decrypt all mappings
      const mappings = await Promise.all(
        results.map(async (row: { config_json: string }) => {
          const config = JSON.parse(row.config_json) as DataMappingConfig;
          // Note: We don't decrypt here for performance - decrypt on demand in getMapping()
          return config;
        })
      );

      return mappings;
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
      await sqliteService.execute('DELETE FROM data_mappings WHERE id = ?', [id]);
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

      const result = await sqliteService.select<{ response_json: string; expires_at: string }[]>(
        `SELECT response_json, expires_at FROM mapping_cache
         WHERE cache_key = ? AND expires_at > datetime('now')`,
        [cacheKey]
      );

      if (!result || result.length === 0) {
        return null;
      }

      console.log(`Cache hit for mapping ${mappingId}`);
      return JSON.parse(result[0].response_json);
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

      await sqliteService.execute(
        `INSERT OR REPLACE INTO mapping_cache (cache_key, mapping_id, params_hash, response_json, cached_at, expires_at)
         VALUES (?, ?, ?, ?, ?, ?)`,
        [cacheKey, mappingId, paramsHash, responseJson, now, expiresAt]
      );

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
      if (mappingId) {
        await sqliteService.execute('DELETE FROM mapping_cache WHERE mapping_id = ?', [mappingId]);
        console.log(`Cache cleared for mapping ${mappingId}`);
      } else {
        await sqliteService.execute('DELETE FROM mapping_cache');
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
      await sqliteService.execute("DELETE FROM mapping_cache WHERE expires_at < datetime('now')");
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
      const result = await sqliteService.select<{ value: string }[]>(
        'SELECT value FROM mapping_preferences WHERE key = ?',
        [key]
      );

      return result && result.length > 0 ? result[0].value : null;
    } catch (error) {
      console.error('Failed to get preference:', error);
      return null;
    }
  }

  async setPreference(key: string, value: string): Promise<void> {
    await this.ensureInitialized();

    try {
      await sqliteService.execute(
        'INSERT OR REPLACE INTO mapping_preferences (key, value) VALUES (?, ?)',
        [key, value]
      );
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
}

// Export singleton instance
export const mappingDatabase = new MappingDatabase();
export default mappingDatabase;
