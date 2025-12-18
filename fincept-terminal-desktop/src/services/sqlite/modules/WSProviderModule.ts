/**
 * WebSocket Provider Module
 * Handles WebSocket provider configurations
 */

import { sqliteLogger } from '../../loggerService';
import type { WSProviderConfig, OperationResult, ToggleResult } from '../types';
import type { SQLiteBase } from '../SQLiteBase';

export class WSProviderModule {
    constructor(private base: SQLiteBase) { }

    async saveWSProviderConfig(config: Omit<WSProviderConfig, 'id' | 'created_at' | 'updated_at'>): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                `INSERT OR REPLACE INTO ws_provider_configs
         (provider_name, enabled, api_key, api_secret, endpoint, config_data, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP)`,
                [
                    config.provider_name,
                    config.enabled ? 1 : 0,
                    config.api_key || null,
                    config.api_secret || null,
                    config.endpoint || null,
                    config.config_data || null,
                ]
            );
            return { success: true, message: 'WebSocket provider configuration saved successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to save WS provider config:', error);
            return { success: false, message: 'Failed to save WebSocket provider configuration' };
        }
    }

    async getWSProviderConfigs(): Promise<WSProviderConfig[]> {
        await this.base.ensureInitialized();
        return await this.base.select<WSProviderConfig[]>('SELECT * FROM ws_provider_configs ORDER BY provider_name');
    }

    async getWSProviderConfig(providerName: string): Promise<WSProviderConfig | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<WSProviderConfig[]>(
            'SELECT * FROM ws_provider_configs WHERE provider_name = $1',
            [providerName]
        );
        return results[0] || null;
    }

    async getEnabledWSProviderConfigs(): Promise<WSProviderConfig[]> {
        await this.base.ensureInitialized();
        return await this.base.select<WSProviderConfig[]>(
            'SELECT * FROM ws_provider_configs WHERE enabled = 1'
        );
    }

    async toggleWSProviderEnabled(providerName: string): Promise<ToggleResult> {
        try {
            await this.base.ensureInitialized();

            const config = await this.getWSProviderConfig(providerName);
            if (!config) {
                return { success: false, message: 'Provider configuration not found', enabled: false };
            }

            const newEnabledState = !config.enabled;

            await this.base.execute(
                'UPDATE ws_provider_configs SET enabled = NOT enabled, updated_at = CURRENT_TIMESTAMP WHERE provider_name = $1',
                [providerName]
            );

            return { success: true, message: `Provider ${newEnabledState ? 'enabled' : 'disabled'}`, enabled: newEnabledState };
        } catch (error) {
            sqliteLogger.error('Failed to toggle WS provider:', error);
            return { success: false, message: 'Failed to toggle provider', enabled: false };
        }
    }

    async deleteWSProviderConfig(providerName: string): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();
            await this.base.execute('DELETE FROM ws_provider_configs WHERE provider_name = $1', [providerName]);
            return { success: true, message: 'Provider configuration deleted successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to delete WS provider config:', error);
            return { success: false, message: 'Failed to delete provider configuration' };
        }
    }
}
