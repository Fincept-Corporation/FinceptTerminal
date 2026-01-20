/**
 * LLM Models Service
 * Fetches available models from LiteLLM library via Tauri commands
 * Supports 52+ providers and 1700+ models
 * CACHING: Uses unified cache system (cacheService)
 */

import { invoke } from '@tauri-apps/api/core';
import { cacheService } from './cache/cacheService';

export interface ModelInfo {
  id: string;
  name: string;
  provider?: string;
  description?: string;
  context_window?: number;
  input_cost_per_token?: number;
  output_cost_per_token?: number;
  deprecated?: boolean;
}

export interface ProviderInfo {
  id: string;
  name: string;
  model_count: number;
}

export interface ProviderModels {
  provider: string;
  models: ModelInfo[];
  lastFetched?: number;
  error?: string;
}

export interface LLMStats {
  total_providers: number;
  total_models: number;
  top_providers: Array<{
    provider: string;
    model_count: number;
  }>;
}

/**
 * Provider name mappings (UI name → LiteLLM provider name)
 * Some providers have different names in the UI vs LiteLLM data
 */
const PROVIDER_MAPPINGS: Record<string, string> = {
  'google': 'gemini',      // UI uses 'google', LiteLLM uses 'gemini'
  'azure_openai': 'azure', // UI might use 'azure_openai', LiteLLM uses 'azure'
};

/**
 * Reverse mappings (LiteLLM name → UI name)
 */
const REVERSE_PROVIDER_MAPPINGS: Record<string, string> = {
  'gemini': 'google',
  'azure': 'azure_openai',
};

/**
 * Get the LiteLLM provider name for a UI provider name
 */
function getLiteLLMProviderName(uiProvider: string): string {
  return PROVIDER_MAPPINGS[uiProvider.toLowerCase()] || uiProvider;
}

/**
 * Main service class for LLM models
 * Uses LiteLLM library via Tauri Python commands
 */
export class LLMModelsService {
  /**
   * Get all available providers from LiteLLM
   */
  static async getProviders(): Promise<ProviderInfo[]> {
    const cacheKey = 'llm-providers:all';

    // Check cache first
    const cached = await cacheService.get<ProviderInfo[]>(cacheKey);
    if (cached) {
      console.log('[LLMModels] Serving providers from cache');
      return cached.data;
    }

    try {
      const providers = await invoke<ProviderInfo[]>('get_llm_providers');
      console.log(`[LLMModels] Loaded ${providers.length} providers from LiteLLM`);

      // Cache for 24 hours (provider list rarely changes)
      await cacheService.set(cacheKey, providers, 'reference', '24h');

      return providers;
    } catch (error) {
      console.error('[LLMModels] Failed to fetch providers:', error);
      // Return empty list on error - all models come from LiteLLM data
      return [];
    }
  }

  /**
   * Get models for a specific provider from LiteLLM
   */
  static async getModelsByProvider(provider: string): Promise<ModelInfo[]> {
    // Map UI provider name to LiteLLM provider name
    const litellmProvider = getLiteLLMProviderName(provider);
    const cacheKey = `llm-models:${provider}`; // Use original provider for cache key

    // Check cache first
    const cached = await cacheService.get<ModelInfo[]>(cacheKey);
    if (cached) {
      console.log(`[LLMModels] Serving ${provider} models from cache`);
      return cached.data;
    }

    try {
      // Use mapped provider name for the API call
      const models = await invoke<ModelInfo[]>('get_llm_models_by_provider', { provider: litellmProvider });
      console.log(`[LLMModels] Loaded ${models.length} models for ${provider} (litellm: ${litellmProvider})`);

      // Cache for 6 hours
      if (models.length > 0) {
        await cacheService.set(cacheKey, models, 'reference', '6h');
      }

      return models;
    } catch (error) {
      console.error(`[LLMModels] Failed to fetch models for ${provider}:`, error);
      // Return empty list on error - all models come from LiteLLM data
      return [];
    }
  }

  /**
   * Get models for a specific provider (alias for backward compatibility)
   */
  static async getModels(provider: string, _apiKey?: string, _baseUrl?: string): Promise<ProviderModels> {
    try {
      const models = await this.getModelsByProvider(provider);

      return {
        provider,
        models,
        lastFetched: Date.now(),
      };
    } catch (error) {
      console.error(`[LLMModels] Error fetching models for ${provider}:`, error);

      return {
        provider,
        models: [],
        error: error instanceof Error ? error.message : 'Failed to fetch models',
      };
    }
  }

  /**
   * Search models by name or provider
   */
  static async searchModels(query: string): Promise<ModelInfo[]> {
    // Map provider names in query if needed
    let mappedQuery = query;
    for (const [uiName, litellmName] of Object.entries(PROVIDER_MAPPINGS)) {
      if (query.toLowerCase() === uiName) {
        mappedQuery = litellmName;
        break;
      }
    }

    const cacheKey = `llm-search:${query.toLowerCase()}`;

    // Check cache first
    const cached = await cacheService.get<ModelInfo[]>(cacheKey);
    if (cached) {
      return cached.data;
    }

    try {
      const models = await invoke<ModelInfo[]>('search_llm_models', { query: mappedQuery });
      console.log(`[LLMModels] Search '${query}' (mapped: ${mappedQuery}) returned ${models.length} models`);

      // Cache search results for 1 hour
      if (models.length > 0) {
        await cacheService.set(cacheKey, models, 'reference', '1h');
      }

      return models;
    } catch (error) {
      console.error(`[LLMModels] Search failed for '${query}':`, error);
      return [];
    }
  }

  /**
   * Get LLM statistics
   */
  static async getStats(): Promise<LLMStats | null> {
    const cacheKey = 'llm-stats';

    const cached = await cacheService.get<LLMStats>(cacheKey);
    if (cached) {
      return cached.data;
    }

    try {
      const stats = await invoke<LLMStats>('get_llm_stats');
      console.log(`[LLMModels] Stats: ${stats.total_providers} providers, ${stats.total_models} models`);

      await cacheService.set(cacheKey, stats, 'reference', '24h');

      return stats;
    } catch (error) {
      console.error('[LLMModels] Failed to fetch stats:', error);
      return null;
    }
  }

  /**
   * Clear the cache for a specific provider or all
   */
  static async clearCache(provider?: string): Promise<void> {
    if (provider) {
      await cacheService.invalidatePattern(`llm-models:${provider}`);
      await cacheService.invalidatePattern(`llm-search:%`);
    } else {
      await cacheService.invalidatePattern('llm-models:%');
      await cacheService.invalidatePattern('llm-providers:%');
      await cacheService.invalidatePattern('llm-search:%');
      await cacheService.invalidatePattern('llm-stats');
    }
    console.log(`[LLMModels] Cache cleared${provider ? ` for ${provider}` : ''}`);
  }

  /**
   * Get all supported providers from LiteLLM
   */
  static async getSupportedProviders(): Promise<string[]> {
    try {
      const providers = await this.getProviders();
      return providers.map(p => p.id);
    } catch {
      return [];
    }
  }

  /**
   * Get provider display name
   */
  static getProviderDisplayName(providerId: string): string {
    const displayNames: Record<string, string> = {
      'openai': 'OpenAI',
      'anthropic': 'Anthropic',
      'google': 'Google AI',
      'gemini': 'Google Gemini',
      'groq': 'Groq',
      'deepseek': 'DeepSeek',
      'mistral': 'Mistral AI',
      'openrouter': 'OpenRouter',
      'ollama': 'Ollama (Local)',
      'fincept': 'Fincept AI',
      'azure': 'Azure OpenAI',
      'azure_ai': 'Azure AI',
      'bedrock': 'AWS Bedrock',
      'vertex_ai': 'Google Vertex AI',
      'fireworks_ai': 'Fireworks AI',
      'together_ai': 'Together AI',
      'perplexity': 'Perplexity',
      'cohere': 'Cohere',
      'replicate': 'Replicate',
      'huggingface': 'Hugging Face',
      'deepinfra': 'DeepInfra',
      'anyscale': 'Anyscale',
      'databricks': 'Databricks',
      'xai': 'xAI (Grok)',
      'cerebras': 'Cerebras',
      'sambanova': 'SambaNova',
      'novita': 'Novita AI',
      'cloudflare': 'Cloudflare AI',
      'nlp_cloud': 'NLP Cloud',
      'ai21': 'AI21 Labs',
      'aleph_alpha': 'Aleph Alpha',
      'palm': 'Google PaLM',
      'watsonx': 'IBM watsonx',
    };

    return displayNames[providerId.toLowerCase()] || providerId.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
  }

  /**
   * Get provider base URL
   */
  static getProviderBaseUrl(provider: string): string {
    const urls: Record<string, string> = {
      'openai': 'https://api.openai.com/v1',
      'anthropic': 'https://api.anthropic.com/v1',
      'google': 'https://generativelanguage.googleapis.com/v1beta',
      'gemini': 'https://generativelanguage.googleapis.com/v1beta',
      'groq': 'https://api.groq.com/openai/v1',
      'deepseek': 'https://api.deepseek.com/v1',
      'mistral': 'https://api.mistral.ai/v1',
      'openrouter': 'https://openrouter.ai/api/v1',
      'together_ai': 'https://api.together.xyz/v1',
      'fireworks_ai': 'https://api.fireworks.ai/inference/v1',
      'perplexity': 'https://api.perplexity.ai',
      'cohere': 'https://api.cohere.ai/v1',
      'anyscale': 'https://api.endpoints.anyscale.com/v1',
      'deepinfra': 'https://api.deepinfra.com/v1/openai',
      'xai': 'https://api.x.ai/v1',
    };

    return urls[provider.toLowerCase()] || '';
  }
}
