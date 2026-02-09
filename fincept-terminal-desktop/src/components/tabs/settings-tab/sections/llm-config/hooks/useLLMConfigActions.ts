import { useCallback } from 'react';
import type { LLMConfig, LLMGlobalSettings, LLMModelConfig } from '@/services/core/sqliteService';
import { sqliteService } from '@/services/core/sqliteService';
import { LLMModelsService } from '@/services/llmModelsService';
import { LLM_PROVIDERS } from '../constants';
import type { NewModelConfig } from '../types';

interface UseLLMConfigActionsProps {
  llmConfigs: LLMConfig[];
  llmGlobalSettings: LLMGlobalSettings;
  modelConfigs: LLMModelConfig[];
  activeProvider: string;
  setActiveProvider: (provider: string) => void;
  setLoading: (loading: boolean) => void;
  showMessage: (type: 'success' | 'error', text: string) => void;
  loadLLMConfigs: () => Promise<{ configs: LLMConfig[]; modelCfgs: LLMModelConfig[] }>;
  setShowAddModel: (show: boolean) => void;
  resetNewModelConfig: () => void;
}

export function useLLMConfigActions({
  llmConfigs,
  llmGlobalSettings,
  modelConfigs,
  activeProvider,
  setActiveProvider,
  setLoading,
  showMessage,
  loadLLMConfigs,
  setShowAddModel,
  resetNewModelConfig,
}: UseLLMConfigActionsProps) {
  // Save all LLM configurations
  const handleSaveLLMConfig = useCallback(async () => {
    try {
      setLoading(true);

      if (activeProvider.startsWith('model:')) {
        const modelId = activeProvider.replace('model:', '');
        const selectedModel = modelConfigs.find(m => m.id === modelId);

        if (selectedModel) {
          const modelLLMConfig: LLMConfig = {
            provider: selectedModel.provider,
            api_key: selectedModel.api_key || '',
            base_url: selectedModel.base_url || LLMModelsService.getProviderBaseUrl(selectedModel.provider),
            model: selectedModel.model_id.includes('/')
              ? selectedModel.model_id.split('/').pop() || selectedModel.model_id
              : selectedModel.model_id,
            is_active: true,
            created_at: new Date().toISOString(),
            updated_at: new Date().toISOString()
          };

          await sqliteService.saveLLMConfig(modelLLMConfig);
          await sqliteService.setActiveLLMProvider(selectedModel.provider);
        }
      } else {
        for (const config of llmConfigs) {
          await sqliteService.saveLLMConfig(config);
        }
        await sqliteService.setActiveLLMProvider(activeProvider);
      }

      await sqliteService.saveLLMGlobalSettings(llmGlobalSettings);
      showMessage('success', 'LLM configuration saved successfully');
      await loadLLMConfigs();

      // Notify other components (like ChatTab) that LLM config has changed
      window.dispatchEvent(new CustomEvent('llm-config-changed'));
    } catch (error) {
      console.error('Failed to save LLM config:', error);
      showMessage('error', 'Failed to save LLM configuration');
    } finally {
      setLoading(false);
    }
  }, [activeProvider, llmConfigs, llmGlobalSettings, modelConfigs, setLoading, showMessage, loadLLMConfigs]);

  // Add new model configuration
  const handleAddModelConfig = useCallback(async (newModelConfig: NewModelConfig) => {
    if (!newModelConfig.model_id.trim()) {
      showMessage('error', 'Model ID is required');
      return;
    }

    try {
      setLoading(true);
      const id = crypto.randomUUID();
      const displayName = newModelConfig.display_name.trim() ||
        `${newModelConfig.provider.toUpperCase()}: ${newModelConfig.model_id}`;

      const result = await sqliteService.saveLLMModelConfig({
        id,
        provider: newModelConfig.provider,
        model_id: newModelConfig.model_id,
        display_name: displayName,
        api_key: newModelConfig.api_key || undefined,
        base_url: newModelConfig.base_url || undefined,
        is_enabled: true,
        is_default: modelConfigs.filter(m => m.provider === newModelConfig.provider).length === 0
      });

      if (result.success) {
        showMessage('success', 'Model configuration added');
        resetNewModelConfig();
        setShowAddModel(false);
        await loadLLMConfigs();
      } else {
        showMessage('error', result.message);
      }
    } catch (error) {
      console.error('Failed to add model config:', error);
      showMessage('error', 'Failed to add model configuration');
    } finally {
      setLoading(false);
    }
  }, [modelConfigs, setLoading, showMessage, loadLLMConfigs, resetNewModelConfig, setShowAddModel]);

  // Delete model configuration
  const handleDeleteModelConfig = useCallback(async (id: string) => {
    try {
      setLoading(true);
      const modelToDelete = modelConfigs.find(m => m.id === id);
      const deletedProvider = modelToDelete?.provider;

      const result = await sqliteService.deleteLLMModelConfig(id);
      if (result.success) {
        showMessage('success', 'Model configuration deleted');

        // Handle active provider changes after deletion
        if (activeProvider === `model:${id}`) {
          setActiveProvider(LLM_PROVIDERS.FINCEPT);
        } else if (deletedProvider && activeProvider === deletedProvider) {
          const remainingModelsForProvider = modelConfigs.filter(
            m => m.id !== id && m.provider === deletedProvider && m.is_enabled
          );
          const providerConfig = llmConfigs.find(c => c.provider === deletedProvider);
          const hasApiKey = providerConfig?.api_key && providerConfig.api_key.trim() !== '';
          const isSpecial = [LLM_PROVIDERS.FINCEPT, LLM_PROVIDERS.OLLAMA].includes(
            deletedProvider as typeof LLM_PROVIDERS.FINCEPT
          );

          if (remainingModelsForProvider.length === 0 && !hasApiKey && !isSpecial) {
            setActiveProvider(LLM_PROVIDERS.FINCEPT);
          }
        }

        await loadLLMConfigs();
      } else {
        showMessage('error', result.message);
      }
    } catch (error) {
      console.error('Failed to delete model config:', error);
      showMessage('error', 'Failed to delete model configuration');
    } finally {
      setLoading(false);
    }
  }, [activeProvider, llmConfigs, modelConfigs, setActiveProvider, setLoading, showMessage, loadLLMConfigs]);

  // Toggle model enabled state
  const handleToggleModelEnabled = useCallback(async (id: string) => {
    try {
      const modelToToggle = modelConfigs.find(m => m.id === id);

      const result = await sqliteService.toggleLLMModelConfigEnabled(id);
      if (result.success) {
        // If disabling the currently selected model, switch to default
        if (modelToToggle?.is_enabled && activeProvider === `model:${id}`) {
          const defaultProvider = llmConfigs.find(c => c.is_active)?.provider || LLM_PROVIDERS.FINCEPT;
          setActiveProvider(defaultProvider);
        }

        await loadLLMConfigs();
      }
    } catch (error) {
      console.error('Failed to toggle model:', error);
      showMessage('error', 'Failed to toggle model');
    }
  }, [activeProvider, llmConfigs, modelConfigs, setActiveProvider, showMessage, loadLLMConfigs]);

  return {
    handleSaveLLMConfig,
    handleAddModelConfig,
    handleDeleteModelConfig,
    handleToggleModelEnabled,
  };
}
