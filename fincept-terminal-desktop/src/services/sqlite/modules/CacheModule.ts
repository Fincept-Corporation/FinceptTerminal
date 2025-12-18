/**
 * Cache Module
 * Handles market data cache, forum cache, and data source connections
 * 
 * Note: Cache methods use `any` types for flexibility since cached data
 * structures may vary by caller (ForumTab, marketDataService, etc.)
 */

import { sqliteLogger } from '../../loggerService';
import type { SQLiteBase } from '../SQLiteBase';

export interface DataSourceConnection {
    id: string;
    name: string;
    type: string;
    category: string;
    config: string;
    status: string;
    created_at?: string;
    updated_at?: string;
    lastTested?: string;
    errorMessage?: string;
}

export class CacheModule {
    constructor(private base: SQLiteBase) { }

    // =========================
    // Market Data Cache
    // =========================

    async saveMarketDataCache(symbol: string, category: string, quoteData: any): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `INSERT OR REPLACE INTO market_data_cache (symbol, category, quote_data, cached_at)
       VALUES ($1, $2, $3, CURRENT_TIMESTAMP)`,
            [symbol, category, JSON.stringify(quoteData)]
        );
    }

    async getCachedMarketData(symbol: string, category: string, maxAgeMinutes: number = 5): Promise<any | null> {
        await this.base.ensureInitialized();

        const results = await this.base.select<{ quote_data: string; cached_at: string }[]>(
            `SELECT quote_data, cached_at FROM market_data_cache
       WHERE symbol = $1 AND category = $2
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`,
            [symbol, category]
        );

        if (!results[0]) return null;

        return JSON.parse(results[0].quote_data);
    }

    async clearMarketDataCache(): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute('DELETE FROM market_data_cache');
    }

    async clearExpiredMarketCache(maxAgeMinutes: number = 60): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute(
            `DELETE FROM market_data_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
        );
    }

    async getCachedCategoryData(category: string, symbols: string[], maxAgeMinutes: number = 5): Promise<Map<string, any>> {
        await this.base.ensureInitialized();

        const result = new Map<string, any>();

        try {
            const placeholders = symbols.map((_, i) => `$${i + 1}`).join(',');
            const categoryParam = symbols.length + 1;

            const results = await this.base.select<{ symbol: string; quote_data: string; cached_at: string }[]>(
                `SELECT symbol, quote_data, cached_at FROM market_data_cache
         WHERE symbol IN (${placeholders}) AND category = $${categoryParam}
         AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`,
                [...symbols, category]
            );

            for (const row of results) {
                result.set(row.symbol, JSON.parse(row.quote_data));
            }
        } catch (error) {
            sqliteLogger.error('Error getting cached category data:', error);
        }

        return result;
    }

    // =========================
    // Data Source Connections
    // =========================

    async saveDataSourceConnection(connection: DataSourceConnection): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `INSERT OR REPLACE INTO data_source_connections
       (id, name, type, category, config, status, updated_at, last_tested, error_message)
       VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP, $7, $8)`,
            [
                connection.id,
                connection.name,
                connection.type,
                connection.category,
                connection.config,
                connection.status,
                connection.lastTested || null,
                connection.errorMessage || null,
            ]
        );
    }

    async getDataSourceConnection(id: string): Promise<any | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<any[]>(
            'SELECT * FROM data_source_connections WHERE id = $1',
            [id]
        );
        return results[0] || null;
    }

    async getAllDataSourceConnections(): Promise<any[]> {
        await this.base.ensureInitialized();
        return await this.base.select<any[]>(
            'SELECT * FROM data_source_connections ORDER BY created_at DESC'
        );
    }

    async getDataSourceConnectionsByCategory(category: string): Promise<any[]> {
        await this.base.ensureInitialized();
        return await this.base.select<any[]>(
            'SELECT * FROM data_source_connections WHERE category = $1 ORDER BY created_at DESC',
            [category]
        );
    }

    async getDataSourceConnectionsByType(type: string): Promise<any[]> {
        await this.base.ensureInitialized();
        return await this.base.select<any[]>(
            'SELECT * FROM data_source_connections WHERE type = $1',
            [type]
        );
    }

    async updateDataSourceConnection(
        id: string,
        updates: {
            name?: string;
            config?: string;
            status?: string;
            lastTested?: string;
            errorMessage?: string;
        }
    ): Promise<void> {
        await this.base.ensureInitialized();

        const fields: string[] = [];
        const values: any[] = [];
        let paramIndex = 1;

        if (updates.name !== undefined) {
            fields.push(`name = $${paramIndex++}`);
            values.push(updates.name);
        }
        if (updates.config !== undefined) {
            fields.push(`config = $${paramIndex++}`);
            values.push(updates.config);
        }
        if (updates.status !== undefined) {
            fields.push(`status = $${paramIndex++}`);
            values.push(updates.status);
        }
        if (updates.lastTested !== undefined) {
            fields.push(`last_tested = $${paramIndex++}`);
            values.push(updates.lastTested);
        }
        if (updates.errorMessage !== undefined) {
            fields.push(`error_message = $${paramIndex++}`);
            values.push(updates.errorMessage);
        }

        if (fields.length === 0) return;

        fields.push(`updated_at = CURRENT_TIMESTAMP`);
        values.push(id);

        const sql = `UPDATE data_source_connections SET ${fields.join(', ')} WHERE id = $${paramIndex}`;
        await this.base.execute(sql, values);
    }

    async deleteDataSourceConnection(id: string): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute('DELETE FROM data_source_connections WHERE id = $1', [id]);
    }

    // =========================
    // Forum Cache
    // =========================

    async cacheForumCategories(categories: any[]): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute(
            `INSERT OR REPLACE INTO forum_categories_cache (id, data, cached_at)
       VALUES (1, $1, CURRENT_TIMESTAMP)`,
            [JSON.stringify(categories)]
        );
    }

    async getCachedForumCategories(maxAgeMinutes: number = 5): Promise<any[] | null> {
        await this.base.ensureInitialized();

        const results = await this.base.select<{ data: string; cached_at: string }[]>(
            `SELECT data, cached_at FROM forum_categories_cache
       WHERE id = 1
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`
        );

        if (!results[0]) return null;

        return JSON.parse(results[0].data);
    }

    async cacheForumPosts(categoryId: number | null, sortBy: string, posts: any[]): Promise<void> {
        await this.base.ensureInitialized();

        const cacheKey = `${categoryId || 'all'}_${sortBy}`;
        await this.base.execute(
            `INSERT OR REPLACE INTO forum_posts_cache (cache_key, category_id, sort_by, data, cached_at)
       VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP)`,
            [cacheKey, categoryId, sortBy, JSON.stringify(posts)]
        );
    }

    async getCachedForumPosts(categoryId: number | null, sortBy: string, maxAgeMinutes: number = 5): Promise<any[] | null> {
        await this.base.ensureInitialized();

        const cacheKey = `${categoryId || 'all'}_${sortBy}`;
        const results = await this.base.select<{ data: string; cached_at: string }[]>(
            `SELECT data, cached_at FROM forum_posts_cache
       WHERE cache_key = $1
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`,
            [cacheKey]
        );

        if (!results[0]) return null;

        return JSON.parse(results[0].data);
    }

    async cacheForumStats(stats: any): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute(
            `INSERT OR REPLACE INTO forum_stats_cache (id, data, cached_at)
       VALUES (1, $1, CURRENT_TIMESTAMP)`,
            [JSON.stringify(stats)]
        );
    }

    async getCachedForumStats(maxAgeMinutes: number = 5): Promise<any | null> {
        await this.base.ensureInitialized();

        const results = await this.base.select<{ data: string; cached_at: string }[]>(
            `SELECT data, cached_at FROM forum_stats_cache
       WHERE id = 1
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`
        );

        if (!results[0]) return null;

        return JSON.parse(results[0].data);
    }

    async cacheForumPostDetails(postUuid: string, post: any, comments: any[]): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute(
            `INSERT OR REPLACE INTO forum_post_details_cache (post_uuid, data, comments_data, cached_at)
       VALUES ($1, $2, $3, CURRENT_TIMESTAMP)`,
            [postUuid, JSON.stringify(post), JSON.stringify(comments)]
        );
    }

    async getCachedForumPostDetails(postUuid: string, maxAgeMinutes: number = 5): Promise<{ post: any; comments: any[] } | null> {
        await this.base.ensureInitialized();

        const results = await this.base.select<{ data: string; comments_data: string; cached_at: string }[]>(
            `SELECT data, comments_data, cached_at FROM forum_post_details_cache
       WHERE post_uuid = $1
       AND datetime(cached_at) >= datetime('now', '-${maxAgeMinutes} minutes')`,
            [postUuid]
        );

        if (!results[0]) return null;

        return {
            post: JSON.parse(results[0].data),
            comments: JSON.parse(results[0].comments_data)
        };
    }

    async clearForumCache(): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute('DELETE FROM forum_categories_cache');
        await this.base.execute('DELETE FROM forum_posts_cache');
        await this.base.execute('DELETE FROM forum_stats_cache');
        await this.base.execute('DELETE FROM forum_post_details_cache');
    }

    async clearExpiredForumCache(maxAgeMinutes: number = 60): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `DELETE FROM forum_categories_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
        );

        await this.base.execute(
            `DELETE FROM forum_posts_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
        );

        await this.base.execute(
            `DELETE FROM forum_stats_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
        );

        await this.base.execute(
            `DELETE FROM forum_post_details_cache
       WHERE datetime(cached_at) < datetime('now', '-${maxAgeMinutes} minutes')`
        );
    }
}
