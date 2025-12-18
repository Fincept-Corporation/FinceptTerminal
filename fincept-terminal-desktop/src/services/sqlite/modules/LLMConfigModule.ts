/**
 * LLM Config Module
 * Handles LLM provider configurations and global settings
 */

import { sqliteLogger } from '../../loggerService';
import type { LLMConfig, LLMGlobalSettings } from '../types';
import type { SQLiteBase } from '../SQLiteBase';

export class LLMConfigModule {
    constructor(private base: SQLiteBase) { }

    async getLLMConfigs(): Promise<LLMConfig[]> {
        await this.base.ensureInitialized();
        return await this.base.select<LLMConfig[]>('SELECT * FROM llm_configs ORDER BY provider');
    }

    async getLLMConfig(provider: string): Promise<LLMConfig | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<LLMConfig[]>(
            'SELECT * FROM llm_configs WHERE provider = $1',
            [provider]
        );
        return results[0] || null;
    }

    async getActiveLLMConfig(): Promise<LLMConfig | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<LLMConfig[]>(
            'SELECT * FROM llm_configs WHERE is_active = 1 LIMIT 1'
        );
        return results[0] || null;
    }

    async saveLLMConfig(config: Omit<LLMConfig, 'created_at' | 'updated_at'>): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `INSERT OR REPLACE INTO llm_configs (provider, api_key, base_url, model, is_active, updated_at)
       VALUES ($1, $2, $3, $4, $5, CURRENT_TIMESTAMP)`,
            [
                config.provider,
                config.api_key || null,
                config.base_url || null,
                config.model,
                config.is_active ? 1 : 0,
            ]
        );
    }

    async setActiveLLMProvider(provider: string): Promise<void> {
        await this.base.ensureInitialized();

        // Deactivate all
        await this.base.execute('UPDATE llm_configs SET is_active = 0');

        // Activate the selected one
        await this.base.execute(
            'UPDATE llm_configs SET is_active = 1 WHERE provider = $1',
            [provider]
        );
    }

    async ensureDefaultLLMConfigs(): Promise<void> {
        await this.base.ensureInitialized();

        const configs = await this.getLLMConfigs();

        // If no configs exist, create defaults
        if (configs.length === 0) {
            const defaultConfigs = [
                { provider: 'ollama', model: 'llama3.2:3b', base_url: 'http://localhost:11434', is_active: true },
                { provider: 'openai', model: 'gpt-4o-mini', is_active: false },
                { provider: 'anthropic', model: 'claude-sonnet-4', is_active: false },
                { provider: 'google', model: 'gemini-2.0-flash-exp', is_active: false },
                { provider: 'groq', model: 'llama-3.1-70b-versatile', is_active: false },
                { provider: 'deepseek', model: 'deepseek-chat', base_url: 'https://api.deepseek.com', is_active: false },
                { provider: 'xai', model: 'grok-beta', is_active: false },
                { provider: 'cohere', model: 'command-r-plus', is_active: false },
                { provider: 'mistral', model: 'mistral-large-latest', is_active: false },
                { provider: 'together', model: 'meta-llama/Meta-Llama-3.1-70B-Instruct-Turbo', is_active: false },
                { provider: 'fireworks', model: 'accounts/fireworks/models/llama-v3p1-70b-instruct', is_active: false },
                { provider: 'openrouter', model: 'meta-llama/llama-3.1-8b-instruct:free', base_url: 'https://openrouter.ai/api/v1', is_active: false },
            ];

            for (const config of defaultConfigs) {
                await this.saveLLMConfig(config);
            }

            sqliteLogger.info('Initialized default LLM configurations');
        }
    }

    async getLLMGlobalSettings(): Promise<LLMGlobalSettings> {
        await this.base.ensureInitialized();
        const results = await this.base.select<LLMGlobalSettings[]>(
            'SELECT temperature, max_tokens, system_prompt FROM llm_global_settings WHERE id = 1'
        );

        return results[0] || {
            temperature: 0.7,
            max_tokens: 2000,
            system_prompt: 'You are a helpful AI assistant specialized in financial analysis and market data.',
        };
    }

    async saveLLMGlobalSettings(settings: LLMGlobalSettings): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `UPDATE llm_global_settings
       SET temperature = $1, max_tokens = $2, system_prompt = $3
       WHERE id = 1`,
            [settings.temperature, settings.max_tokens, settings.system_prompt]
        );
    }
}
