/**
 * Settings Module
 * Handles general application settings storage
 */

import type { Setting } from '../types';
import type { SQLiteBase } from '../SQLiteBase';

export class SettingsModule {
    constructor(private base: SQLiteBase) { }

    async saveSetting(key: string, value: string, category?: string): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `INSERT OR REPLACE INTO settings (setting_key, setting_value, category, updated_at)
       VALUES ($1, $2, $3, CURRENT_TIMESTAMP)`,
            [key, value, category || null]
        );
    }

    async getSetting(key: string): Promise<string | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<Setting[]>(
            'SELECT * FROM settings WHERE setting_key = $1',
            [key]
        );
        return results[0]?.setting_value || null;
    }

    async getSettingsByCategory(category: string): Promise<Setting[]> {
        await this.base.ensureInitialized();
        return await this.base.select<Setting[]>(
            'SELECT * FROM settings WHERE category = $1',
            [category]
        );
    }

    async getAllSettings(): Promise<Setting[]> {
        await this.base.ensureInitialized();
        return await this.base.select<Setting[]>('SELECT * FROM settings');
    }
}
