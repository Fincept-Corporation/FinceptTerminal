/**
 * Data Source Module
 * Handles WebSocket and REST API data source configurations
 */

import { sqliteLogger } from '../../loggerService';
import type { DataSource, OperationResult, OperationResultWithId, ToggleResult } from '../types';
import type { SQLiteBase } from '../SQLiteBase';

export class DataSourceModule {
    constructor(private base: SQLiteBase) { }

    async saveDataSource(source: Omit<DataSource, 'created_at' | 'updated_at'>): Promise<OperationResultWithId> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                `INSERT OR REPLACE INTO data_sources
         (id, alias, display_name, description, type, provider, category, config, enabled, tags, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, CURRENT_TIMESTAMP)`,
                [
                    source.id,
                    source.alias,
                    source.display_name,
                    source.description || null,
                    source.type,
                    source.provider,
                    source.category || null,
                    source.config,
                    source.enabled ? 1 : 0,
                    source.tags || null,
                ]
            );
            return { success: true, message: 'Data source saved successfully', id: source.id };
        } catch (error) {
            sqliteLogger.error('Failed to save data source:', error);
            return { success: false, message: 'Failed to save data source' };
        }
    }

    async getAllDataSources(): Promise<DataSource[]> {
        await this.base.ensureInitialized();
        return await this.base.select<DataSource[]>('SELECT * FROM data_sources ORDER BY display_name');
    }

    async getDataSourceById(id: string): Promise<DataSource | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<DataSource[]>(
            'SELECT * FROM data_sources WHERE id = $1',
            [id]
        );
        return results[0] || null;
    }

    async getDataSourceByAlias(alias: string): Promise<DataSource | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<DataSource[]>(
            'SELECT * FROM data_sources WHERE alias = $1',
            [alias]
        );
        return results[0] || null;
    }

    async getDataSourcesByType(type: 'websocket' | 'rest_api'): Promise<DataSource[]> {
        await this.base.ensureInitialized();
        return await this.base.select<DataSource[]>(
            'SELECT * FROM data_sources WHERE type = $1',
            [type]
        );
    }

    async getDataSourcesByProvider(provider: string): Promise<DataSource[]> {
        await this.base.ensureInitialized();
        return await this.base.select<DataSource[]>(
            'SELECT * FROM data_sources WHERE provider = $1',
            [provider]
        );
    }

    async getEnabledDataSources(): Promise<DataSource[]> {
        await this.base.ensureInitialized();
        return await this.base.select<DataSource[]>(
            'SELECT * FROM data_sources WHERE enabled = 1'
        );
    }

    async toggleDataSourceEnabled(id: string): Promise<ToggleResult> {
        try {
            await this.base.ensureInitialized();

            // Get current state
            const source = await this.getDataSourceById(id);
            if (!source) {
                return { success: false, message: 'Data source not found', enabled: false };
            }

            const newEnabledState = !source.enabled;

            await this.base.execute(
                'UPDATE data_sources SET enabled = NOT enabled, updated_at = CURRENT_TIMESTAMP WHERE id = $1',
                [id]
            );

            return { success: true, message: `Data source ${newEnabledState ? 'enabled' : 'disabled'}`, enabled: newEnabledState };
        } catch (error) {
            sqliteLogger.error('Failed to toggle data source:', error);
            return { success: false, message: 'Failed to toggle data source', enabled: false };
        }
    }

    async deleteDataSource(id: string): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();
            await this.base.execute('DELETE FROM data_sources WHERE id = $1', [id]);
            return { success: true, message: 'Data source deleted successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to delete data source:', error);
            return { success: false, message: 'Failed to delete data source' };
        }
    }

    async searchDataSources(query: string): Promise<DataSource[]> {
        await this.base.ensureInitialized();
        const searchPattern = `%${query}%`;
        return await this.base.select<DataSource[]>(
            `SELECT * FROM data_sources
       WHERE display_name LIKE $1 OR alias LIKE $1 OR provider LIKE $1 OR tags LIKE $1`,
            [searchPattern]
        );
    }
}
