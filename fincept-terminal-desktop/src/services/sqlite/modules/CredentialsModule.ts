/**
 * Credentials Module
 * Handles API keys and credentials storage
 */

import { sqliteLogger } from '../../loggerService';
import type { Credential, ApiKeys, Setting, OperationResult } from '../types';
import type { SQLiteBase } from '../SQLiteBase';
import type { SettingsModule } from './SettingsModule';

export class CredentialsModule {
    constructor(
        private base: SQLiteBase,
        private settings: SettingsModule
    ) { }

    async saveCredential(credential: Omit<Credential, 'id' | 'created_at' | 'updated_at'>): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                `INSERT OR REPLACE INTO credentials (service_name, username, password, api_key, api_secret, additional_data, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP)`,
                [
                    credential.service_name,
                    credential.username || null,
                    credential.password || null,
                    credential.api_key || null,
                    credential.api_secret || null,
                    credential.additional_data || null,
                ]
            );
            return { success: true, message: 'Credential saved successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to save credential:', error);
            return { success: false, message: 'Failed to save credential' };
        }
    }

    async getCredentials(): Promise<Credential[]> {
        await this.base.ensureInitialized();
        return await this.base.select<Credential[]>('SELECT * FROM credentials ORDER BY service_name');
    }

    async getCredentialByService(serviceName: string): Promise<Credential | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<Credential[]>(
            'SELECT * FROM credentials WHERE service_name = $1',
            [serviceName]
        );
        return results[0] || null;
    }

    async getApiKey(serviceName: string): Promise<string | null> {
        // Use settings table for predefined API keys
        const keyName = serviceName.toUpperCase().replace(/[^A-Z0-9]/g, '_') + '_API_KEY';
        return await this.settings.getSetting(keyName);
    }

    async setApiKey(keyName: string, value: string): Promise<void> {
        await this.settings.saveSetting(keyName, value, 'api_keys');
    }

    async getAllApiKeys(): Promise<ApiKeys> {
        await this.base.ensureInitialized();
        const results = await this.base.select<Setting[]>(
            "SELECT * FROM settings WHERE category = 'api_keys'"
        );

        const keys: ApiKeys = {};
        results.forEach(row => {
            keys[row.setting_key] = row.setting_value;
        });
        return keys;
    }

    async deleteCredential(id: number): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();
            await this.base.execute('DELETE FROM credentials WHERE id = $1', [id]);
            return { success: true, message: 'Credential deleted successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to delete credential:', error);
            return { success: false, message: 'Failed to delete credential' };
        }
    }
}
