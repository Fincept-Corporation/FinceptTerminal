// LLM Configuration Service
// Manages settings for different LLM providers with localStorage persistence

export type LLMProvider = 'openai' | 'gemini' | 'deepseek' | 'ollama' | 'openrouter';

export interface LLMConfig {
  provider: LLMProvider;
  apiKey: string;
  baseUrl?: string; // For Ollama and OpenRouter
  model: string;
  temperature?: number;
  maxTokens?: number;
  systemPrompt?: string;
}

export interface LLMSettings {
  openai: {
    apiKey: string;
    model: string;
    baseUrl?: string;
  };
  gemini: {
    apiKey: string;
    model: string;
  };
  deepseek: {
    apiKey: string;
    model: string;
    baseUrl?: string;
  };
  ollama: {
    baseUrl: string;
    model: string;
  };
  openrouter: {
    apiKey: string;
    model: string;
    baseUrl?: string;
  };
  activeProvider: LLMProvider;
  globalSettings: {
    temperature: number;
    maxTokens: number;
    systemPrompt: string;
  };
}

// Default models for each provider
export const DEFAULT_MODELS = {
  openai: 'gpt-4o-mini',
  gemini: 'gemini-2.0-flash-exp',
  deepseek: 'deepseek-chat',
  ollama: 'llama3.2:3b', // Changed from llama3.2:latest to a specific version that's commonly available
  openrouter: 'google/gemini-2.0-flash-exp:free' // Free model with function calling support
};

// Default base URLs
export const DEFAULT_BASE_URLS = {
  openai: 'https://api.openai.com/v1',
  deepseek: 'https://api.deepseek.com',
  ollama: 'http://localhost:11434',
  openrouter: 'https://openrouter.ai/api/v1'
};

const STORAGE_KEY = 'fincept-llm-settings';

// Default settings
const getDefaultSettings = (): LLMSettings => ({
  openai: {
    apiKey: '',
    model: DEFAULT_MODELS.openai
  },
  gemini: {
    apiKey: '',
    model: DEFAULT_MODELS.gemini
  },
  deepseek: {
    apiKey: '',
    model: DEFAULT_MODELS.deepseek
  },
  ollama: {
    baseUrl: DEFAULT_BASE_URLS.ollama,
    model: DEFAULT_MODELS.ollama
  },
  openrouter: {
    apiKey: '',
    model: DEFAULT_MODELS.openrouter
  },
  activeProvider: 'ollama', // Default to Ollama as it's free and local
  globalSettings: {
    temperature: 0.7,
    maxTokens: 2048,
    systemPrompt: 'You are Fincept AI, a helpful financial analysis assistant. You provide insights on markets, trading, portfolio management, and financial data analysis.'
  }
});

class LLMConfigService {
  private settings: LLMSettings;

  constructor() {
    this.settings = this.loadSettings();
  }

  // Load settings from localStorage
  private loadSettings(): LLMSettings {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored);
        // Merge with defaults to ensure all fields exist
        return { ...getDefaultSettings(), ...parsed };
      }
    } catch (error) {
      console.error('Failed to load LLM settings:', error);
    }
    return getDefaultSettings();
  }

  // Save settings to localStorage
  private saveSettings(): void {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(this.settings));
    } catch (error) {
      console.error('Failed to save LLM settings:', error);
    }
  }

  // Get all settings
  getSettings(): LLMSettings {
    return { ...this.settings };
  }

  // Get current active configuration
  getActiveConfig(): LLMConfig {
    const provider = this.settings.activeProvider;
    const providerSettings = this.settings[provider];
    const global = this.settings.globalSettings;

    return {
      provider,
      apiKey: 'apiKey' in providerSettings ? providerSettings.apiKey : '',
      baseUrl: 'baseUrl' in providerSettings ? providerSettings.baseUrl : undefined,
      model: providerSettings.model,
      temperature: global.temperature,
      maxTokens: global.maxTokens,
      systemPrompt: global.systemPrompt
    };
  }

  // Update settings for specific provider
  updateProviderSettings(provider: LLMProvider, settings: Partial<LLMSettings[LLMProvider]>): void {
    this.settings[provider] = { ...this.settings[provider], ...settings } as any;
    this.saveSettings();
  }

  // Update global settings
  updateGlobalSettings(settings: Partial<LLMSettings['globalSettings']>): void {
    this.settings.globalSettings = { ...this.settings.globalSettings, ...settings };
    this.saveSettings();
  }

  // Set active provider
  setActiveProvider(provider: LLMProvider): void {
    this.settings.activeProvider = provider;
    this.saveSettings();
  }

  // Get active provider
  getActiveProvider(): LLMProvider {
    return this.settings.activeProvider;
  }

  // Validate configuration
  validateConfig(provider?: LLMProvider): { valid: boolean; error?: string } {
    const prov = provider || this.settings.activeProvider;
    const providerSettings = this.settings[prov];

    switch (prov) {
      case 'openai':
      case 'gemini':
      case 'deepseek':
      case 'openrouter':
        if (!('apiKey' in providerSettings) || !providerSettings.apiKey) {
          return { valid: false, error: `API key required for ${prov}` };
        }
        break;
      case 'ollama':
        if (!('baseUrl' in providerSettings) || !providerSettings.baseUrl) {
          return { valid: false, error: 'Ollama base URL required' };
        }
        break;
    }

    if (!providerSettings.model) {
      return { valid: false, error: 'Model selection required' };
    }

    return { valid: true };
  }

  // Reset to defaults
  resetToDefaults(): void {
    this.settings = getDefaultSettings();
    this.saveSettings();
  }

  // Export settings
  exportSettings(): string {
    return JSON.stringify(this.settings, null, 2);
  }

  // Import settings
  importSettings(json: string): boolean {
    try {
      const imported = JSON.parse(json);
      this.settings = { ...getDefaultSettings(), ...imported };
      this.saveSettings();
      return true;
    } catch (error) {
      console.error('Failed to import settings:', error);
      return false;
    }
  }
}

// Singleton instance
export const llmConfigService = new LLMConfigService();
