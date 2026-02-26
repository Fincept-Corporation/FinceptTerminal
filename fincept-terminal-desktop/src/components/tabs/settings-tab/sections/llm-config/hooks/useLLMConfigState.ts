import { useState, useEffect, useCallback, useMemo } from 'react';
import type { LLMConfig, LLMModelConfig } from '@/services/core/sqliteService';
import { sqliteService } from '@/services/core/sqliteService';
import { ollamaService } from '@/services/chat/ollamaService';
import { LLMModelsService, type ModelInfo } from '@/services/llmModelsService';
import { LLM_PROVIDERS, SPECIAL_PROVIDERS, DEFAULT_NEW_MODEL_CONFIG, DEFAULT_OLLAMA_BASE_URL } from '../constants';
import type { NewModelConfig } from '../types';

interface UseLLMConfigStateProps {
  llmConfigs: LLMConfig[];
  setLlmConfigs: React.Dispatch<React.SetStateAction<LLMConfig[]>>;
  modelConfigs: LLMModelConfig[];
  setLoading: (loading: boolean) => void;
  showMessage: (type: 'success' | 'error', text: string) => void;
  loadLLMConfigs: () => Promise<{ configs: LLMConfig[]; modelCfgs: LLMModelConfig[] }>;
}

export function useLLMConfigState({
  llmConfigs,
  setLlmConfigs,
  modelConfigs: initialModelConfigs,
  setLoading,
  showMessage,
  loadLLMConfigs,
}: UseLLMConfigStateProps) {
  // Provider selection state
  const [activeProvider, setActiveProvider] = useState<string>(LLM_PROVIDERS.FINCEPT);

  // Model configs local state (synced with props)
  const [modelConfigs, setModelConfigs] = useState<LLMModelConfig[]>(initialModelConfigs);

  // Ollama specific state
  const [ollamaModels, setOllamaModels] = useState<string[]>([]);
  const [ollamaLoading, setOllamaLoading] = useState(false);
  const [ollamaError, setOllamaError] = useState<string | null>(null);

  // Provider models state (for non-Ollama providers)
  const [providerModels, setProviderModels] = useState<ModelInfo[]>([]);
  const [providerModelsLoading, setProviderModelsLoading] = useState(false);
  const [providerModelsError, setProviderModelsError] = useState<string | null>(null);

  // Manual entry state - SEPARATE states for provider config and add model form
  const [useManualEntryProvider, setUseManualEntryProvider] = useState(false);
  const [useManualEntryAddModel, setUseManualEntryAddModel] = useState(false);

  // Add model form state
  const [showAddModel, setShowAddModel] = useState(false);
  const [newModelConfig, setNewModelConfig] = useState<NewModelConfig>({ ...DEFAULT_NEW_MODEL_CONFIG });
  const [newModelConfigModels, setNewModelConfigModels] = useState<ModelInfo[]>([]);
  const [availableProviders, setAvailableProviders] = useState<Array<{ value: string; label: string }>>([]);

  // Load dynamic providers from LiteLLM data
  useEffect(() => {
    LLMModelsService.getProviders().then(providers => {
      if (providers.length > 0) {
        setAvailableProviders(providers.map(p => ({ value: p.id, label: LLMModelsService.getProviderDisplayName(p.id) })));
      }
    }).catch(() => {});
  }, []);

  // API key visibility state
  const [showApiKeys, setShowApiKeys] = useState<Record<string, boolean>>({});

  // Sync model configs with props
  useEffect(() => {
    setModelConfigs(initialModelConfigs);
  }, [initialModelConfigs]);

  // Set active provider based on current configs
  useEffect(() => {
    const activeConfig = llmConfigs.find(c => c.is_active);
    const providersWithModels = new Set(
      modelConfigs.filter(m => m.is_enabled).map(m => m.provider)
    );

    const isProviderVisible = (provider: string, config?: LLMConfig) => {
      if (SPECIAL_PROVIDERS.includes(provider as typeof SPECIAL_PROVIDERS[number])) return true;
      if (config?.api_key && config.api_key.trim() !== '') return true;
      if (providersWithModels.has(provider)) return true;
      return false;
    };

    if (activeConfig && isProviderVisible(activeConfig.provider, activeConfig)) {
      setActiveProvider(activeConfig.provider);
    } else {
      setActiveProvider(LLM_PROVIDERS.FINCEPT);
    }
  }, [llmConfigs, modelConfigs]);

  // Get visible provider configs
  const visibleProviderConfigs = useMemo(() => {
    const providersWithModels = new Set(
      modelConfigs.filter(m => m.is_enabled).map(m => m.provider)
    );

    return llmConfigs.filter(config => {
      if (SPECIAL_PROVIDERS.includes(config.provider as typeof SPECIAL_PROVIDERS[number])) return true;
      if (config.api_key && config.api_key.trim() !== '') return true;
      if (providersWithModels.has(config.provider)) return true;
      return false;
    });
  }, [llmConfigs, modelConfigs]);

  // Get current LLM config based on active provider
  const getCurrentLLMConfig = useCallback((): LLMConfig | undefined => {
    if (activeProvider.startsWith('model:')) {
      const modelId = activeProvider.replace('model:', '');
      const modelConfig = modelConfigs.find(m => m.id === modelId);
      if (modelConfig) {
        return {
          provider: modelConfig.provider,
          api_key: modelConfig.api_key || '',
          base_url: modelConfig.base_url || '',
          model: modelConfig.model_id,
          is_active: true,
          created_at: modelConfig.created_at || new Date().toISOString(),
          updated_at: modelConfig.updated_at || new Date().toISOString()
        } as LLMConfig;
      }
    }
    return llmConfigs.find(c => c.provider === activeProvider);
  }, [activeProvider, llmConfigs, modelConfigs]);

  // Get selected model config (for custom models)
  const getSelectedModelConfig = useCallback((): LLMModelConfig | undefined => {
    if (activeProvider.startsWith('model:')) {
      const modelId = activeProvider.replace('model:', '');
      return modelConfigs.find(m => m.id === modelId);
    }
    return undefined;
  }, [activeProvider, modelConfigs]);

  // Update LLM config helper
  const updateLLMConfig = useCallback((provider: string, field: keyof LLMConfig, value: any) => {
    setLlmConfigs(prev => prev.map(config =>
      config.provider === provider ? { ...config, [field]: value } : config
    ));
  }, [setLlmConfigs]);

  // Fetch Ollama models
  const fetchOllamaModels = useCallback(async () => {
    setOllamaLoading(true);
    setOllamaError(null);

    const currentConfig = getCurrentLLMConfig();
    const baseUrl = currentConfig?.base_url || DEFAULT_OLLAMA_BASE_URL;

    const result = await ollamaService.listModels(baseUrl);

    if (result.success && result.models) {
      setOllamaModels(result.models);
      setOllamaError(null);
    } else {
      setOllamaModels([]);
      setOllamaError(result.error || 'Failed to fetch models');
    }

    setOllamaLoading(false);
  }, [getCurrentLLMConfig]);

  // Fetch provider models (for non-Ollama/Fincept providers)
  const fetchProviderModels = useCallback(async () => {
    const currentConfig = getCurrentLLMConfig();
    if (!currentConfig) return;

    const provider = currentConfig.provider;
    if (provider === LLM_PROVIDERS.OLLAMA || provider === LLM_PROVIDERS.FINCEPT) return;

    setProviderModelsLoading(true);
    setProviderModelsError(null);

    try {
      const result = await LLMModelsService.getModels(
        provider,
        currentConfig.api_key || undefined,
        currentConfig.base_url || undefined
      );

      if (result.models && result.models.length > 0) {
        setProviderModels(result.models);
        setProviderModelsError(null);
      } else if (result.error) {
        setProviderModelsError(result.error);
        setProviderModels([]);
      } else {
        setProviderModels([]);
        setProviderModelsError('No models available for this provider');
      }
    } catch (error) {
      setProviderModelsError(error instanceof Error ? error.message : 'Failed to fetch models');
      setProviderModels([]);
    } finally {
      setProviderModelsLoading(false);
    }
  }, [getCurrentLLMConfig]);

  // Load models when active provider changes
  useEffect(() => {
    const loadModels = async () => {
      const currentConfig = getCurrentLLMConfig();
      if (!currentConfig) return;

      const provider = currentConfig.provider;
      setProviderModels([]);
      setProviderModelsError(null);

      if (provider === LLM_PROVIDERS.OLLAMA) {
        fetchOllamaModels();
      }
      // Skip auto-fetching external provider models - only load on user click
    };

    loadModels();
  }, [activeProvider, getCurrentLLMConfig, fetchOllamaModels]);

  // Load models for new model config form (LAZY - only on manual trigger)
  useEffect(() => {
    const loadNewModelConfigModels = async () => {
      if (!showAddModel) return;
      // Don't auto-fetch - let user click button or use manual entry
      setNewModelConfigModels([]);
      /* Disabled auto-fetch for performance
      try {
        const models = await LLMModelsService.getModelsByProvider(newModelConfig.provider);
        if (models && models.length > 0) {
          setNewModelConfigModels(models);
        } else {
          setNewModelConfigModels([]);
        }
      } catch {
        setNewModelConfigModels([]);
      } */
    };

    loadNewModelConfigModels();
  }, [showAddModel]);

  // Reset new model config when closing form
  const resetNewModelConfig = useCallback(() => {
    setNewModelConfig({ ...DEFAULT_NEW_MODEL_CONFIG });
    setUseManualEntryAddModel(false);
  }, []);

  return {
    // Provider state
    activeProvider,
    setActiveProvider,
    visibleProviderConfigs,
    getCurrentLLMConfig,
    getSelectedModelConfig,
    updateLLMConfig,

    // Model configs
    modelConfigs,

    // Ollama state
    ollamaModels,
    ollamaLoading,
    ollamaError,
    fetchOllamaModels,

    // Provider models state
    providerModels,
    providerModelsLoading,
    providerModelsError,
    fetchProviderModels,

    // Manual entry state (separate for each form)
    useManualEntryProvider,
    setUseManualEntryProvider,
    useManualEntryAddModel,
    setUseManualEntryAddModel,

    // Add model form state
    showAddModel,
    setShowAddModel,
    newModelConfig,
    setNewModelConfig,
    newModelConfigModels,
    resetNewModelConfig,
    availableProviders,

    // API key visibility
    showApiKeys,
    setShowApiKeys,
  };
}
