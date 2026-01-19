/**
 * LLM Models Service
 * Fetches available models from various LLM providers
 * Uses official API endpoints to get real-time model listings
 * CACHING: Uses unified cache system (cacheService)
 */

import { cacheService } from './cache/cacheService';

export interface ModelInfo {
  id: string;
  name: string;
  description?: string;
  context_window?: number;
  deprecated?: boolean;
}

export interface ProviderModels {
  provider: string;
  models: ModelInfo[];
  lastFetched?: number;
  error?: string;
}

/**
 * Static fallback model lists (minimal, used only if API fails)
 */
const FALLBACK_MODELS: Record<string, ModelInfo[]> = {
  openai: [
    { id: 'gpt-4o', name: 'GPT-4o', description: 'Latest flagship', context_window: 128000 },
    { id: 'gpt-4o-mini', name: 'GPT-4o Mini', description: 'Fast & affordable', context_window: 128000 },
    { id: 'o1', name: 'o1', description: 'Reasoning model', context_window: 200000 },
  ],
  anthropic: [
    { id: 'claude-opus-4-5-20251101', name: 'Claude Opus 4.5', description: 'Most capable', context_window: 200000 },
    { id: 'claude-sonnet-4-5-20250929', name: 'Claude Sonnet 4.5', description: 'Balanced', context_window: 200000 },
    { id: 'claude-haiku-4-5-20251031', name: 'Claude Haiku 4.5', description: 'Fastest', context_window: 200000 },
  ],
  google: [
    { id: 'gemini-2.5-pro-latest', name: 'Gemini 2.5 Pro', description: 'Latest flagship', context_window: 1000000 },
    { id: 'gemini-2.5-flash-latest', name: 'Gemini 2.5 Flash', description: 'Fast', context_window: 1000000 },
  ],
  groq: [
    { id: 'llama-3.3-70b-versatile', name: 'Llama 3.3 70B', description: 'Meta Llama', context_window: 128000 },
    { id: 'mixtral-8x7b-32768', name: 'Mixtral 8x7B', description: 'Mistral', context_window: 32768 },
  ],
  deepseek: [
    { id: 'deepseek-chat', name: 'DeepSeek Chat', description: 'General chat', context_window: 64000 },
    { id: 'deepseek-coder', name: 'DeepSeek Coder', description: 'Code generation', context_window: 64000 },
  ],
  openrouter: [],
};

/**
 * Fetch models from OpenAI API
 */
async function fetchOpenAIModels(apiKey: string): Promise<ModelInfo[]> {
  try {
    const response = await fetch('https://api.openai.com/v1/models', {
      headers: {
        'Authorization': `Bearer ${apiKey}`,
      },
    });

    if (!response.ok) {
      throw new Error(`OpenAI API error: ${response.status}`);
    }

    const data = await response.json();

    // Filter to only show GPT and o1 models
    const models = data.data
      .filter((m: any) => m.id.startsWith('gpt-') || m.id.startsWith('o1'))
      .map((m: any) => ({
        id: m.id,
        name: m.id,
        description: `OpenAI ${m.id}`,
      }))
      .sort((a: ModelInfo, b: ModelInfo) => b.id.localeCompare(a.id));

    return models.length > 0 ? models : FALLBACK_MODELS.openai;
  } catch (error) {
    console.error('[LLMModels] Failed to fetch OpenAI models:', error);
    return FALLBACK_MODELS.openai;
  }
}

/**
 * Fetch models from Anthropic API
 */
async function fetchAnthropicModels(apiKey: string): Promise<ModelInfo[]> {
  try {
    const response = await fetch('https://api.anthropic.com/v1/models', {
      headers: {
        'x-api-key': apiKey,
        'anthropic-version': '2023-06-01',
      },
    });

    if (!response.ok) {
      throw new Error(`Anthropic API error: ${response.status}`);
    }

    const data = await response.json();

    const models = data.data.map((m: any) => ({
      id: m.id,
      name: m.display_name || m.id,
      description: `Anthropic ${m.display_name || m.id}`,
      context_window: m.max_tokens,
    }));

    return models.length > 0 ? models : FALLBACK_MODELS.anthropic;
  } catch (error) {
    console.error('[LLMModels] Failed to fetch Anthropic models:', error);
    return FALLBACK_MODELS.anthropic;
  }
}

/**
 * Fetch models from Google Gemini API
 */
async function fetchGoogleModels(apiKey: string): Promise<ModelInfo[]> {
  try {
    const response = await fetch(
      `https://generativelanguage.googleapis.com/v1beta/models?key=${apiKey}`
    );

    if (!response.ok) {
      throw new Error(`Google API error: ${response.status}`);
    }

    const data = await response.json();

    // Filter to only Gemini models that support generateContent
    const models = data.models
      .filter((m: any) =>
        m.name.includes('gemini') &&
        m.supportedGenerationMethods?.includes('generateContent')
      )
      .map((m: any) => ({
        id: m.name.replace('models/', ''),
        name: m.displayName || m.name.replace('models/', ''),
        description: m.description || `Google ${m.displayName}`,
        context_window: m.inputTokenLimit,
      }));

    return models.length > 0 ? models : FALLBACK_MODELS.google;
  } catch (error) {
    console.error('[LLMModels] Failed to fetch Google models:', error);
    return FALLBACK_MODELS.google;
  }
}

/**
 * Fetch all models from OpenRouter API (public, no auth required)
 * Returns 400+ models from multiple providers, grouped and sorted
 */
async function fetchOpenRouterModels(): Promise<ModelInfo[]> {
  try {
    const response = await fetch('https://openrouter.ai/api/v1/models');

    if (!response.ok) {
      throw new Error(`OpenRouter API error: ${response.status}`);
    }

    const data = await response.json();

    if (!data.data || !Array.isArray(data.data)) {
      throw new Error('Invalid response format');
    }

    // Group models by provider
    const providerGroups: Record<string, ModelInfo[]> = {};

    data.data.forEach((m: any) => {
      const provider = m.id.split('/')[0]; // e.g., "openai/gpt-4" -> "openai"
      const modelName = m.id.split('/')[1] || m.id;

      if (!providerGroups[provider]) {
        providerGroups[provider] = [];
      }

      providerGroups[provider].push({
        id: m.id,
        name: m.name || modelName,
        description: m.description?.substring(0, 150) || modelName,
        context_window: m.context_length || 0,
      });
    });

    // Sort providers and flatten models
    const sortedModels: ModelInfo[] = [];
    const providers = Object.keys(providerGroups).sort();

    console.log(`[LLMModels] OpenRouter providers found: ${providers.join(', ')}`);
    console.log(`[LLMModels] Total models: ${data.data.length}`);

    providers.forEach(provider => {
      // Sort models within each provider by name
      const sortedProviderModels = providerGroups[provider].sort((a, b) =>
        a.name.localeCompare(b.name)
      );
      sortedModels.push(...sortedProviderModels);
    });

    return sortedModels;
  } catch (error) {
    console.error('[LLMModels] Failed to fetch OpenRouter models:', error);
    return FALLBACK_MODELS.openrouter;
  }
}

/**
 * Fetch models from Groq API
 */
async function fetchGroqModels(apiKey: string): Promise<ModelInfo[]> {
  try {
    const response = await fetch('https://api.groq.com/openai/v1/models', {
      headers: {
        'Authorization': `Bearer ${apiKey}`,
      },
    });

    if (!response.ok) {
      throw new Error(`Groq API error: ${response.status}`);
    }

    const data = await response.json();

    const models = data.data.map((m: any) => ({
      id: m.id,
      name: m.id,
      description: `Groq ${m.id}`,
      context_window: m.context_window,
    }));

    return models.length > 0 ? models : FALLBACK_MODELS.groq;
  } catch (error) {
    console.error('[LLMModels] Failed to fetch Groq models:', error);
    return FALLBACK_MODELS.groq;
  }
}

/**
 * Main service class for LLM models
 */
export class LLMModelsService {
  /**
   * Get models from OpenRouter filtered by provider prefix
   */
  static async getOpenRouterModelsByProvider(providerPrefix: string): Promise<ModelInfo[]> {
    const allModels = await fetchOpenRouterModels();
    return allModels.filter(m => m.id.startsWith(`${providerPrefix}/`));
  }

  /**
   * Get models for a specific provider
   */
  static async getModels(provider: string, apiKey?: string, baseUrl?: string): Promise<ProviderModels> {
    const cacheKey = `llm-models:${provider}:${apiKey?.substring(0, 10) || 'no-key'}`;

    // Check unified cache first
    const cached = await cacheService.get<ModelInfo[]>(cacheKey);
    if (cached) {
      console.log(`[LLMModels] Serving ${provider} models from cache`);
      return {
        provider,
        models: cached.data,
        lastFetched: Date.now(),
      };
    }

    try {
      let models: ModelInfo[] = [];

      switch (provider.toLowerCase()) {
        case 'openai':
          if (!apiKey) {
            models = FALLBACK_MODELS.openai;
          } else {
            models = await fetchOpenAIModels(apiKey);
          }
          break;

        case 'anthropic':
          if (!apiKey) {
            models = FALLBACK_MODELS.anthropic;
          } else {
            models = await fetchAnthropicModels(apiKey);
          }
          break;

        case 'google':
          if (!apiKey) {
            models = FALLBACK_MODELS.google;
          } else {
            models = await fetchGoogleModels(apiKey);
          }
          break;

        case 'groq':
          if (!apiKey) {
            models = FALLBACK_MODELS.groq;
          } else {
            models = await fetchGroqModels(apiKey);
          }
          break;

        case 'deepseek':
          models = FALLBACK_MODELS.deepseek;
          break;

        case 'openrouter':
          models = await fetchOpenRouterModels();
          break;

        case 'ollama':
          // Ollama models are fetched separately via ollamaService
          models = [];
          break;

        case 'fincept':
          // Fincept models - auto-configured
          models = [
            { id: 'fincept-ai', name: 'Fincept AI', description: 'Auto-configured Fincept model' },
          ];
          break;

        default:
          models = [];
      }

      // Cache the result using unified cache (1 hour TTL)
      if (models.length > 0) {
        await cacheService.set(cacheKey, models, 'reference', '1h');
      }

      return {
        provider,
        models,
        lastFetched: Date.now(),
      };
    } catch (error) {
      console.error(`[LLMModels] Error fetching models for ${provider}:`, error);

      // Return fallback models on error
      const fallback = FALLBACK_MODELS[provider.toLowerCase()] || [];
      return {
        provider,
        models: fallback,
        error: error instanceof Error ? error.message : 'Failed to fetch models',
      };
    }
  }

  /**
   * Get fallback models (static list) for a provider
   */
  static getFallbackModels(provider: string): ModelInfo[] {
    return FALLBACK_MODELS[provider.toLowerCase()] || [];
  }

  /**
   * Clear the cache for a specific provider
   */
  static async clearCache(provider?: string): Promise<void> {
    if (provider) {
      await cacheService.invalidatePattern(`llm-models:${provider}:%`);
    } else {
      await cacheService.invalidatePattern('llm-models:%');
    }
    console.log(`[LLMModels] Cache cleared${provider ? ` for ${provider}` : ''}`);
  }

  /**
   * Get all supported providers
   */
  static getSupportedProviders(): string[] {
    return [
      'fincept',
      'openai',
      'anthropic',
      'google',
      'groq',
      'deepseek',
      'openrouter',
      'ollama',
    ];
  }
}
