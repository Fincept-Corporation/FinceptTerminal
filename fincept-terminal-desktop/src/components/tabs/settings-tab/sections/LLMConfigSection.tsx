import React, { useState, useEffect } from 'react';
import { Save, RefreshCw, Bot, Edit3, Zap, Key, Check, Plus, X, Trash2, ToggleLeft, ToggleRight, Eye, EyeOff, Settings as SettingsIcon } from 'lucide-react';
import { sqliteService, type LLMConfig, type LLMGlobalSettings, type LLMModelConfig } from '@/services/core/sqliteService';
import { ollamaService } from '@/services/chat/ollamaService';
import { LLMModelsService, type ModelInfo } from '@/services/llmModelsService';
import type { SettingsColors } from '../types';

// Fincept Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
};

interface LLMConfigSectionProps {
  colors: SettingsColors;
  llmConfigs: LLMConfig[];
  setLlmConfigs: React.Dispatch<React.SetStateAction<LLMConfig[]>>;
  llmGlobalSettings: LLMGlobalSettings;
  setLlmGlobalSettings: React.Dispatch<React.SetStateAction<LLMGlobalSettings>>;
  modelConfigs: LLMModelConfig[];
  loading: boolean;
  setLoading: (loading: boolean) => void;
  showMessage: (type: 'success' | 'error', text: string) => void;
  loadLLMConfigs: () => Promise<{ configs: LLMConfig[]; modelCfgs: LLMModelConfig[] }>;
  sessionApiKey?: string;
}

export function LLMConfigSection({
  colors,
  llmConfigs,
  setLlmConfigs,
  llmGlobalSettings,
  setLlmGlobalSettings,
  modelConfigs: initialModelConfigs,
  loading,
  setLoading,
  showMessage,
  loadLLMConfigs,
  sessionApiKey,
}: LLMConfigSectionProps) {
  const [activeProvider, setActiveProvider] = useState<string>('fincept');
  const [ollamaModels, setOllamaModels] = useState<string[]>([]);
  const [ollamaLoading, setOllamaLoading] = useState(false);
  const [ollamaError, setOllamaError] = useState<string | null>(null);
  const [useManualEntry, setUseManualEntry] = useState(false);
  const [providerModels, setProviderModels] = useState<ModelInfo[]>([]);
  const [providerModelsLoading, setProviderModelsLoading] = useState(false);
  const [providerModelsError, setProviderModelsError] = useState<string | null>(null);
  const [modelConfigs, setModelConfigs] = useState<LLMModelConfig[]>(initialModelConfigs);
  const [showAddModel, setShowAddModel] = useState(false);
  const [newModelConfig, setNewModelConfig] = useState({
    provider: 'openai',
    model_id: '',
    display_name: '',
    api_key: '',
    base_url: ''
  });
  const [newModelConfigModels, setNewModelConfigModels] = useState<ModelInfo[]>([]);
  const [showApiKeys, setShowApiKeys] = useState<Record<string, boolean>>({});

  useEffect(() => {
    setModelConfigs(initialModelConfigs);
  }, [initialModelConfigs]);

  useEffect(() => {
    const activeConfig = llmConfigs.find(c => c.is_active);
    const specialProviders = ['fincept', 'ollama'];
    const providersWithModels = new Set(modelConfigs.filter(m => m.is_enabled).map(m => m.provider));

    const isProviderVisible = (provider: string, config?: LLMConfig) => {
      if (specialProviders.includes(provider)) return true;
      if (config?.api_key && config.api_key.trim() !== '') return true;
      if (providersWithModels.has(provider)) return true;
      return false;
    };

    if (activeConfig && isProviderVisible(activeConfig.provider, activeConfig)) {
      setActiveProvider(activeConfig.provider);
    } else {
      setActiveProvider('fincept');
    }
  }, [llmConfigs, modelConfigs]);

  const getCurrentLLMConfig = () => {
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
  };

  const getSelectedModelConfig = () => {
    if (activeProvider.startsWith('model:')) {
      const modelId = activeProvider.replace('model:', '');
      return modelConfigs.find(m => m.id === modelId);
    }
    return null;
  };

  const getVisibleProviderConfigs = () => {
    const specialProviders = ['fincept', 'ollama'];
    const providersWithModels = new Set(modelConfigs.filter(m => m.is_enabled).map(m => m.provider));

    return llmConfigs.filter(config => {
      if (specialProviders.includes(config.provider)) return true;
      if (config.api_key && config.api_key.trim() !== '') return true;
      if (providersWithModels.has(config.provider)) return true;
      return false;
    });
  };

  const visibleProviderConfigs = getVisibleProviderConfigs();

  const updateLLMConfig = (provider: string, field: keyof LLMConfig, value: any) => {
    setLlmConfigs(prev => prev.map(config =>
      config.provider === provider ? { ...config, [field]: value } : config
    ));
  };

  const fetchOllamaModels = async () => {
    setOllamaLoading(true);
    setOllamaError(null);

    const currentConfig = getCurrentLLMConfig();
    const baseUrl = currentConfig?.base_url || 'http://localhost:11434';

    const result = await ollamaService.listModels(baseUrl);

    if (result.success && result.models) {
      setOllamaModels(result.models);
      setOllamaError(null);
    } else {
      setOllamaModels([]);
      setOllamaError(result.error || 'Failed to fetch models');
    }

    setOllamaLoading(false);
  };

  const fetchProviderModels = async (forceRefresh: boolean = false) => {
    const currentConfig = getCurrentLLMConfig();
    if (!currentConfig) return;

    const provider = currentConfig.provider;
    if (provider === 'ollama' || provider === 'fincept') return;

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
  };

  useEffect(() => {
    const loadModels = async () => {
      const currentConfig = getCurrentLLMConfig();
      if (!currentConfig) return;

      const provider = currentConfig.provider;
      setProviderModels([]);
      setProviderModelsError(null);

      if (provider === 'ollama') {
        fetchOllamaModels();
      } else if (provider !== 'fincept') {
        setProviderModelsLoading(true);
        try {
          const result = await LLMModelsService.getModels(
            provider,
            currentConfig.api_key || undefined,
            currentConfig.base_url || undefined
          );
          if (result.models && result.models.length > 0) {
            setProviderModels(result.models);
          }
        } catch (error) {
          console.error('Error fetching models:', error);
        } finally {
          setProviderModelsLoading(false);
        }
      }
    };

    loadModels();
  }, [activeProvider]);

  useEffect(() => {
    const loadNewModelConfigModels = async () => {
      if (!showAddModel) return;

      try {
        const models = await LLMModelsService.getModelsByProvider(newModelConfig.provider);
        if (models && models.length > 0) {
          setNewModelConfigModels(models);
        } else {
          setNewModelConfigModels([]);
        }
      } catch (error) {
        setNewModelConfigModels([]);
      }
    };

    loadNewModelConfigModels();
  }, [newModelConfig.provider, showAddModel]);

  const handleSaveLLMConfig = async () => {
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
    } catch (error) {
      showMessage('error', 'Failed to save LLM configuration');
    } finally {
      setLoading(false);
    }
  };

  const handleAddModelConfig = async () => {
    if (!newModelConfig.model_id) {
      showMessage('error', 'Model ID is required');
      return;
    }

    try {
      setLoading(true);
      const id = crypto.randomUUID();
      const displayName = newModelConfig.display_name || `${newModelConfig.provider.toUpperCase()}: ${newModelConfig.model_id}`;

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
        setNewModelConfig({
          provider: 'openai',
          model_id: '',
          display_name: '',
          api_key: '',
          base_url: ''
        });
        setShowAddModel(false);
        await loadLLMConfigs();
      } else {
        showMessage('error', result.message);
      }
    } catch (error) {
      showMessage('error', 'Failed to add model configuration');
    } finally {
      setLoading(false);
    }
  };

  const handleDeleteModelConfig = async (id: string) => {
    try {
      setLoading(true);
      const modelToDelete = modelConfigs.find(m => m.id === id);
      const deletedProvider = modelToDelete?.provider;

      const result = await sqliteService.deleteLLMModelConfig(id);
      if (result.success) {
        showMessage('success', 'Model configuration deleted');

        if (activeProvider === `model:${id}`) {
          setActiveProvider('fincept');
        } else if (deletedProvider && activeProvider === deletedProvider) {
          const remainingModelsForProvider = modelConfigs.filter(
            m => m.id !== id && m.provider === deletedProvider && m.is_enabled
          );
          const providerConfig = llmConfigs.find(c => c.provider === deletedProvider);
          const hasApiKey = providerConfig?.api_key && providerConfig.api_key.trim() !== '';
          const isSpecial = ['fincept', 'ollama'].includes(deletedProvider);

          if (remainingModelsForProvider.length === 0 && !hasApiKey && !isSpecial) {
            setActiveProvider('fincept');
          }
        }

        await loadLLMConfigs();
      } else {
        showMessage('error', result.message);
      }
    } catch (error) {
      showMessage('error', 'Failed to delete model configuration');
    } finally {
      setLoading(false);
    }
  };

  const handleToggleModelEnabled = async (id: string) => {
    try {
      const modelToToggle = modelConfigs.find(m => m.id === id);

      const result = await sqliteService.toggleLLMModelConfigEnabled(id);
      if (result.success) {
        if (modelToToggle?.is_enabled && activeProvider === `model:${id}`) {
          const defaultProvider = llmConfigs.find(c => c.is_active)?.provider || 'fincept';
          setActiveProvider(defaultProvider);
        }

        await loadLLMConfigs();
      }
    } catch (error) {
      showMessage('error', 'Failed to toggle model');
    }
  };

  const currentConfig = getCurrentLLMConfig();

  return (
    <div style={{
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      display: 'flex',
      flexDirection: 'column',
      gap: '20px'
    }}>
      {/* Header */}
      <div style={{
        borderBottom: `2px solid ${colors.primary}`,
        padding: '16px 0',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Bot size={24} color={colors.primary} />
          <div>
            <div style={{
              color: colors.primary,
              fontWeight: 700,
              fontSize: '16px',
              letterSpacing: '0.5px',
            }}>
              LLM CONFIGURATION
            </div>
            <div style={{ color: '#888', fontSize: '12px', marginTop: '2px' }}>
              Configure AI providers, models, and global settings
            </div>
          </div>
        </div>
        <button
          onClick={handleSaveLLMConfig}
          disabled={loading}
          style={{
            background: loading ? '#2A2A2A' : colors.primary,
            color: loading ? '#666' : '#000',
            border: 'none',
            padding: '10px 20px',
            fontSize: '12px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            cursor: loading ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '8px'
          }}
        >
          <Save size={14} />
          {loading ? 'SAVING...' : 'SAVE ALL SETTINGS'}
        </button>
      </div>

      {/* Provider Selection */}
      <div style={{
        backgroundColor: '#0F0F0F',
        border: `1px solid #2A2A2A`,
        padding: '20px'
      }}>
        <div style={{
          color: colors.primary,
          fontSize: '13px',
          fontWeight: 700,
          marginBottom: '16px',
          letterSpacing: '0.5px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <Zap size={16} color={colors.primary} />
          SELECT AI PROVIDER
        </div>

        <div style={{ display: 'flex', gap: '10px', flexWrap: 'wrap', marginBottom: '16px' }}>
          {visibleProviderConfigs.map(config => (
            <button
              key={config.provider}
              onClick={() => setActiveProvider(config.provider)}
              style={{
                padding: '12px 20px',
                backgroundColor: activeProvider === config.provider ? colors.primary : '#1A1A1A',
                border: `2px solid ${activeProvider === config.provider ? colors.primary : '#2A2A2A'}`,
                color: activeProvider === config.provider ? '#000' : '#FFF',
                cursor: 'pointer',
                fontSize: '13px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                transition: 'all 0.2s',
                display: 'flex',
                alignItems: 'center',
                gap: '8px'
              }}
            >
              {config.provider === 'fincept' && <span style={{ color: activeProvider === config.provider ? '#000' : colors.success }}>●</span>}
              {config.provider.toUpperCase()}
            </button>
          ))}
        </div>

        {/* Custom Models from Model Library */}
        {modelConfigs.filter(m => m.is_enabled).length > 0 && (
          <div>
            <div style={{
              color: '#888',
              fontSize: '11px',
              fontWeight: 600,
              marginBottom: '10px',
              letterSpacing: '0.5px',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}>
              <Bot size={14} color="#888" />
              CUSTOM MODELS (from Model Library)
            </div>
            <div style={{ display: 'flex', gap: '10px', flexWrap: 'wrap' }}>
              {modelConfigs.filter(m => m.is_enabled).map(model => {
                const isSelected = activeProvider === `model:${model.id}`;
                return (
                  <button
                    key={model.id}
                    onClick={() => setActiveProvider(`model:${model.id}`)}
                    style={{
                      padding: '10px 16px',
                      backgroundColor: isSelected ? colors.secondary : '#1A1A1A',
                      border: `2px solid ${isSelected ? colors.secondary : '#2A2A2A'}`,
                      color: isSelected ? '#000' : '#FFF',
                      cursor: 'pointer',
                      fontSize: '12px',
                      fontWeight: 600,
                      letterSpacing: '0.5px',
                      transition: 'all 0.2s',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px'
                    }}
                  >
                    <span style={{
                      padding: '2px 6px',
                      background: colors.primary + '30',
                      border: `1px solid ${colors.primary}`,
                      fontSize: '9px',
                      fontWeight: 700,
                      color: colors.primary
                    }}>
                      {model.provider.toUpperCase()}
                    </span>
                    {model.display_name || model.model_id.split('/').pop()}
                  </button>
                );
              })}
            </div>
          </div>
        )}
      </div>

      {/* Main Content Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '20px' }}>
        {/* Left Column - Provider Config */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
          {/* Fincept Auto-Configured */}
          {currentConfig && currentConfig.provider === 'fincept' && (
            <div style={{
              backgroundColor: '#0a2a0a',
              border: `2px solid ${colors.success}`,
              padding: '20px'
            }}>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                marginBottom: '12px'
              }}>
                <Check size={20} color={colors.success} />
                <span style={{ color: colors.success, fontSize: '14px', fontWeight: 700 }}>
                  FINCEPT LLM - AUTO CONFIGURED
                </span>
              </div>
              <div style={{ color: '#AAA', fontSize: '12px', lineHeight: 1.6 }}>
                <div style={{ marginBottom: '8px' }}>
                  Your Fincept API is automatically configured and ready to use.
                </div>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '16px',
                  padding: '12px',
                  backgroundColor: '#000',
                  border: `1px solid #2A2A2A`
                }}>
                  <div>
                    <div style={{ color: '#666', fontSize: '10px', marginBottom: '4px' }}>API KEY</div>
                    <div style={{ color: colors.primary, fontSize: '12px', fontFamily: 'monospace' }}>
                      {sessionApiKey?.substring(0, 12)}...
                    </div>
                  </div>
                  <div>
                    <div style={{ color: '#666', fontSize: '10px', marginBottom: '4px' }}>COST</div>
                    <div style={{ color: '#FFF', fontSize: '12px' }}>5 credits/response</div>
                  </div>
                  <div>
                    <div style={{ color: '#666', fontSize: '10px', marginBottom: '4px' }}>MODEL</div>
                    <div style={{ color: '#FFF', fontSize: '12px' }}>fincept-llm</div>
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* Custom Model from Model Library */}
          {getSelectedModelConfig() && (
            <div style={{
              backgroundColor: '#0a1a2a',
              border: `2px solid ${colors.secondary}`,
              padding: '20px'
            }}>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                marginBottom: '12px'
              }}>
                <Bot size={20} color={colors.secondary} />
                <span style={{ color: colors.secondary, fontSize: '14px', fontWeight: 700 }}>
                  CUSTOM MODEL - {getSelectedModelConfig()!.display_name || getSelectedModelConfig()!.model_id}
                </span>
              </div>
              <div style={{ color: '#AAA', fontSize: '12px', lineHeight: 1.6 }}>
                <div style={{ marginBottom: '8px' }}>
                  This model uses the native <strong style={{ color: colors.primary }}>{getSelectedModelConfig()!.provider.toUpperCase()}</strong> API directly.
                </div>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '16px',
                  padding: '12px',
                  backgroundColor: '#000',
                  border: `1px solid #2A2A2A`,
                  flexWrap: 'wrap'
                }}>
                  <div>
                    <div style={{ color: '#666', fontSize: '10px', marginBottom: '4px' }}>PROVIDER</div>
                    <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 600 }}>
                      {getSelectedModelConfig()!.provider.toUpperCase()}
                    </div>
                  </div>
                  <div>
                    <div style={{ color: '#666', fontSize: '10px', marginBottom: '4px' }}>MODEL</div>
                    <div style={{ color: '#FFF', fontSize: '12px', fontFamily: 'monospace' }}>
                      {getSelectedModelConfig()!.model_id.includes('/')
                        ? getSelectedModelConfig()!.model_id.split('/').pop()
                        : getSelectedModelConfig()!.model_id}
                    </div>
                  </div>
                  <div>
                    <div style={{ color: '#666', fontSize: '10px', marginBottom: '4px' }}>API KEY</div>
                    <div style={{ color: getSelectedModelConfig()!.api_key ? colors.success : colors.alert, fontSize: '12px' }}>
                      {getSelectedModelConfig()!.api_key ? 'Custom key set' : 'No API key'}
                    </div>
                  </div>
                </div>
                {!getSelectedModelConfig()!.api_key && (
                  <div style={{
                    marginTop: '12px',
                    padding: '10px',
                    background: '#3a0a0a',
                    border: `1px solid ${colors.alert}`,
                    color: colors.alert,
                    fontSize: '11px'
                  }}>
                    Warning: No API key set for this model. Add your {getSelectedModelConfig()!.provider.toUpperCase()} API key in the Model Library or main provider config.
                  </div>
                )}
              </div>
            </div>
          )}

          {/* Provider Configuration */}
          {currentConfig && currentConfig.provider !== 'fincept' && !getSelectedModelConfig() && (
            <div style={{
              backgroundColor: '#0F0F0F',
              border: `1px solid #2A2A2A`,
              padding: '20px'
            }}>
              <div style={{
                color: colors.primary,
                fontSize: '13px',
                fontWeight: 700,
                marginBottom: '20px',
                letterSpacing: '0.5px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                paddingBottom: '12px',
                borderBottom: `1px solid #2A2A2A`
              }}>
                <Key size={16} color={colors.primary} />
                {currentConfig.provider.toUpperCase()} CONFIGURATION
              </div>

              <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                {/* API Key */}
                {currentConfig.provider !== 'ollama' && (
                  <div>
                    <label style={{
                      color: '#FFF',
                      fontSize: '11px',
                      display: 'block',
                      marginBottom: '8px',
                      fontWeight: 600,
                      letterSpacing: '0.5px'
                    }}>
                      API KEY <span style={{ color: colors.alert }}>*</span>
                    </label>
                    <input
                      type="password"
                      value={currentConfig.api_key || ''}
                      onChange={(e) => updateLLMConfig(activeProvider, 'api_key', e.target.value)}
                      placeholder={`Enter your ${activeProvider} API key`}
                      style={{
                        width: '100%',
                        background: '#000',
                        border: `1px solid #2A2A2A`,
                        color: '#FFF',
                        padding: '12px 14px',
                        fontSize: '13px',
                        fontFamily: '"IBM Plex Mono", monospace',
                        outline: 'none'
                      }}
                    />
                  </div>
                )}

                {/* Base URL */}
                {(currentConfig.provider === 'ollama' || currentConfig.provider === 'deepseek' || currentConfig.provider === 'openrouter') && (
                  <div>
                    <label style={{
                      color: '#FFF',
                      fontSize: '11px',
                      display: 'block',
                      marginBottom: '8px',
                      fontWeight: 600,
                      letterSpacing: '0.5px'
                    }}>
                      BASE URL
                    </label>
                    <input
                      type="text"
                      value={currentConfig.base_url || ''}
                      onChange={(e) => updateLLMConfig(activeProvider, 'base_url', e.target.value)}
                      placeholder={currentConfig.provider === 'ollama' ? 'http://localhost:11434' : 'https://api.example.com'}
                      style={{
                        width: '100%',
                        background: '#000',
                        border: `1px solid #2A2A2A`,
                        color: '#FFF',
                        padding: '12px 14px',
                        fontSize: '13px',
                        fontFamily: '"IBM Plex Mono", monospace',
                        outline: 'none'
                      }}
                    />
                  </div>
                )}

                {/* Model Selection */}
                <div>
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    marginBottom: '8px'
                  }}>
                    <label style={{
                      color: '#FFF',
                      fontSize: '11px',
                      fontWeight: 600,
                      letterSpacing: '0.5px'
                    }}>
                      MODEL
                    </label>
                    <div style={{ display: 'flex', gap: '8px' }}>
                      {currentConfig.provider === 'ollama' ? (
                        <button
                          onClick={fetchOllamaModels}
                          disabled={ollamaLoading}
                          style={{
                            background: '#1A1A1A',
                            border: `1px solid #2A2A2A`,
                            color: colors.primary,
                            padding: '6px 12px',
                            fontSize: '11px',
                            cursor: ollamaLoading ? 'not-allowed' : 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                            fontWeight: 600
                          }}
                        >
                          <RefreshCw size={12} className={ollamaLoading ? 'animate-spin' : ''} />
                          {ollamaLoading ? 'LOADING...' : 'REFRESH MODELS'}
                        </button>
                      ) : currentConfig.provider !== 'fincept' && (
                        <button
                          onClick={() => fetchProviderModels(true)}
                          disabled={providerModelsLoading}
                          style={{
                            background: '#1A1A1A',
                            border: `1px solid #2A2A2A`,
                            color: colors.primary,
                            padding: '6px 12px',
                            fontSize: '11px',
                            cursor: providerModelsLoading ? 'not-allowed' : 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                            fontWeight: 600
                          }}
                        >
                          <RefreshCw size={12} className={providerModelsLoading ? 'animate-spin' : ''} />
                          {providerModelsLoading ? 'LOADING...' : 'REFRESH MODELS'}
                        </button>
                      )}
                      <button
                        onClick={() => setUseManualEntry(!useManualEntry)}
                        style={{
                          background: useManualEntry ? colors.primary : '#1A1A1A',
                          border: `1px solid ${useManualEntry ? colors.primary : '#2A2A2A'}`,
                          color: useManualEntry ? '#000' : '#FFF',
                          padding: '6px 12px',
                          fontSize: '11px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '6px',
                          fontWeight: 600
                        }}
                      >
                        <Edit3 size={12} />
                        {useManualEntry ? 'USE DROPDOWN' : 'MANUAL ENTRY'}
                      </button>
                    </div>
                  </div>

                  {!useManualEntry ? (
                    <div>
                      <select
                        value={currentConfig.model}
                        onChange={(e) => updateLLMConfig(activeProvider, 'model', e.target.value)}
                        disabled={
                          (currentConfig.provider === 'ollama' && (ollamaLoading || ollamaModels.length === 0)) ||
                          (currentConfig.provider !== 'ollama' && currentConfig.provider !== 'fincept' && (providerModelsLoading || providerModels.length === 0))
                        }
                        style={{
                          width: '100%',
                          background: '#000',
                          border: `1px solid #2A2A2A`,
                          color: '#FFF',
                          padding: '12px 14px',
                          fontSize: '13px',
                          cursor: 'pointer'
                        }}
                      >
                        {currentConfig.provider === 'ollama' ? (
                          <>
                            {ollamaModels.length === 0 && !ollamaLoading && <option value="">No models found - Click REFRESH</option>}
                            {ollamaLoading && <option value="">Loading models...</option>}
                            {ollamaModels.map((model) => (
                              <option key={model} value={model}>{model}</option>
                            ))}
                          </>
                        ) : (
                          <>
                            {providerModels.length === 0 && !providerModelsLoading && <option value="">No models found - Click REFRESH</option>}
                            {providerModelsLoading && <option value="">Loading models...</option>}
                            {providerModels.map((model) => (
                              <option key={model.id} value={model.id}>
                                {model.name} {model.context_window ? `(${(model.context_window / 1000).toFixed(0)}K)` : ''}
                              </option>
                            ))}
                          </>
                        )}
                      </select>
                      {currentConfig.provider === 'ollama' && ollamaError && (
                        <div style={{
                          marginTop: '8px',
                          padding: '10px 12px',
                          background: '#3a0a0a',
                          border: `1px solid ${colors.alert}`,
                          fontSize: '12px',
                          color: colors.alert
                        }}>
                          {ollamaError}
                        </div>
                      )}
                      {currentConfig.provider !== 'ollama' && providerModelsError && (
                        <div style={{
                          marginTop: '8px',
                          padding: '10px 12px',
                          background: '#3a0a0a',
                          border: `1px solid ${colors.alert}`,
                          fontSize: '12px',
                          color: colors.alert
                        }}>
                          {providerModelsError}
                        </div>
                      )}
                      {currentConfig.provider === 'ollama' && ollamaModels.length > 0 && (
                        <div style={{
                          marginTop: '8px',
                          fontSize: '12px',
                          color: colors.success,
                          display: 'flex',
                          alignItems: 'center',
                          gap: '6px'
                        }}>
                          <Check size={14} />
                          {ollamaModels.length} model{ollamaModels.length !== 1 ? 's' : ''} available
                        </div>
                      )}
                      {currentConfig.provider !== 'ollama' && currentConfig.provider !== 'fincept' && providerModels.length > 0 && (
                        <div style={{
                          marginTop: '8px',
                          fontSize: '11px',
                          color: '#AAA',
                          display: 'flex',
                          flexDirection: 'column',
                          gap: '4px'
                        }}>
                          <div style={{
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                            color: colors.success
                          }}>
                            <Check size={14} />
                            {providerModels.length} model{providerModels.length !== 1 ? 's' : ''} available
                          </div>
                          <div style={{ fontSize: '10px', color: '#666' }}>
                            Models fetched from {currentConfig.provider.toUpperCase()} API
                            {providerModelsLoading && ' (updating...)'}
                          </div>
                        </div>
                      )}
                    </div>
                  ) : (
                    <input
                      type="text"
                      value={currentConfig.model}
                      onChange={(e) => updateLLMConfig(activeProvider, 'model', e.target.value)}
                      placeholder="Enter model name (e.g., gpt-4, claude-3-sonnet)"
                      style={{
                        width: '100%',
                        background: '#000',
                        border: `1px solid #2A2A2A`,
                        color: '#FFF',
                        padding: '12px 14px',
                        fontSize: '13px',
                        outline: 'none'
                      }}
                    />
                  )}
                </div>
              </div>
            </div>
          )}

          {/* Global Settings */}
          <div style={{
            backgroundColor: '#0F0F0F',
            border: `1px solid #2A2A2A`,
            padding: '20px'
          }}>
            <div style={{
              color: colors.primary,
              fontSize: '13px',
              fontWeight: 700,
              marginBottom: '20px',
              letterSpacing: '0.5px',
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              paddingBottom: '12px',
              borderBottom: `1px solid #2A2A2A`
            }}>
              <SettingsIcon size={16} color={colors.primary} />
              GLOBAL SETTINGS
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
              {/* Temperature */}
              <div>
                <label style={{
                  color: '#FFF',
                  fontSize: '11px',
                  display: 'block',
                  marginBottom: '8px',
                  fontWeight: 600,
                  letterSpacing: '0.5px'
                }}>
                  TEMPERATURE (Creativity)
                </label>
                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                  <input
                    type="range"
                    min="0"
                    max="2"
                    step="0.1"
                    value={llmGlobalSettings.temperature}
                    onChange={(e) => setLlmGlobalSettings({ ...llmGlobalSettings, temperature: parseFloat(e.target.value) })}
                    style={{ flex: 1, height: '6px', cursor: 'pointer' }}
                  />
                  <div style={{
                    minWidth: '50px',
                    padding: '8px 12px',
                    background: colors.primary + '20',
                    border: `1px solid ${colors.primary}`,
                    color: colors.primary,
                    fontSize: '13px',
                    fontWeight: 700,
                    textAlign: 'center'
                  }}>
                    {llmGlobalSettings.temperature.toFixed(1)}
                  </div>
                </div>
                <div style={{ color: '#666', fontSize: '10px', marginTop: '6px' }}>
                  Lower = More focused, Higher = More creative
                </div>
              </div>

              {/* Max Tokens */}
              <div>
                <label style={{
                  color: '#FFF',
                  fontSize: '11px',
                  display: 'block',
                  marginBottom: '8px',
                  fontWeight: 600,
                  letterSpacing: '0.5px'
                }}>
                  MAX TOKENS (Response Length)
                </label>
                <input
                  type="text"
                  inputMode="numeric"
                  value={String(llmGlobalSettings.max_tokens)}
                  onChange={(e) => {
                    const v = e.target.value;
                    if (v === '' || /^\d+$/.test(v)) {
                      setLlmGlobalSettings({ ...llmGlobalSettings, max_tokens: v === '' ? 2048 : parseInt(v) });
                    }
                  }}
                  style={{
                    width: '100%',
                    background: '#000',
                    border: `1px solid #2A2A2A`,
                    color: '#FFF',
                    padding: '12px 14px',
                    fontSize: '13px',
                    outline: 'none'
                  }}
                />
              </div>

              {/* System Prompt */}
              <div>
                <label style={{
                  color: '#FFF',
                  fontSize: '11px',
                  display: 'block',
                  marginBottom: '8px',
                  fontWeight: 600,
                  letterSpacing: '0.5px'
                }}>
                  SYSTEM PROMPT (Optional)
                </label>
                <textarea
                  value={llmGlobalSettings.system_prompt}
                  onChange={(e) => setLlmGlobalSettings({ ...llmGlobalSettings, system_prompt: e.target.value })}
                  rows={4}
                  placeholder="Custom instructions for the AI assistant..."
                  style={{
                    width: '100%',
                    background: '#000',
                    border: `1px solid #2A2A2A`,
                    color: '#FFF',
                    padding: '12px 14px',
                    fontSize: '12px',
                    resize: 'vertical',
                    fontFamily: '"IBM Plex Mono", monospace',
                    lineHeight: '1.5',
                    outline: 'none'
                  }}
                />
              </div>
            </div>
          </div>
        </div>

        {/* Right Column - Model Library */}
        <div style={{
          backgroundColor: '#0F0F0F',
          border: `1px solid #2A2A2A`,
          padding: '20px',
          display: 'flex',
          flexDirection: 'column'
        }}>
          {/* Header */}
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '20px',
            paddingBottom: '12px',
            borderBottom: `1px solid #2A2A2A`
          }}>
            <div>
              <div style={{
                color: colors.primary,
                fontSize: '13px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                marginBottom: '4px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px'
              }}>
                <Bot size={16} color={colors.primary} />
                MODEL LIBRARY
              </div>
              <div style={{ color: '#888', fontSize: '11px' }}>
                Add custom models for different use cases
              </div>
            </div>
            <button
              onClick={() => setShowAddModel(!showAddModel)}
              style={{
                background: showAddModel ? '#3a0a0a' : colors.primary,
                color: showAddModel ? colors.alert : '#000',
                border: 'none',
                padding: '8px 16px',
                fontSize: '12px',
                fontWeight: 700,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '6px'
              }}
            >
              {showAddModel ? <><X size={14} /> CANCEL</> : <><Plus size={14} /> ADD MODEL</>}
            </button>
          </div>

          {/* Add Model Form */}
          {showAddModel && (
            <div style={{
              background: '#000',
              border: `1px solid ${colors.primary}`,
              padding: '16px',
              marginBottom: '16px'
            }}>
              <div style={{
                color: colors.primary,
                fontSize: '12px',
                fontWeight: 700,
                marginBottom: '16px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px'
              }}>
                <Plus size={14} />
                ADD NEW MODEL CONFIGURATION
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '12px' }}>
                <div>
                  <label style={{ color: '#FFF', fontSize: '10px', display: 'block', marginBottom: '6px', fontWeight: 600 }}>
                    PROVIDER
                  </label>
                  <select
                    value={newModelConfig.provider}
                    onChange={(e) => setNewModelConfig({ ...newModelConfig, provider: e.target.value })}
                    style={{
                      width: '100%',
                      background: '#1A1A1A',
                      border: `1px solid #2A2A2A`,
                      color: '#FFF',
                      padding: '10px 12px',
                      fontSize: '12px',
                      cursor: 'pointer'
                    }}
                  >
                    <option value="openai">OpenAI</option>
                    <option value="anthropic">Anthropic</option>
                    <option value="google">Google</option>
                    <option value="meta-llama">Meta (Llama)</option>
                    <option value="mistralai">Mistral AI</option>
                    <option value="x-ai">xAI (Grok)</option>
                    <option value="cohere">Cohere</option>
                    <option value="deepseek">DeepSeek</option>
                    <option value="qwen">Qwen (Alibaba)</option>
                    <option value="nvidia">NVIDIA</option>
                    <option value="microsoft">Microsoft</option>
                    <option value="perplexity">Perplexity</option>
                    <option value="groq">Groq</option>
                    <option value="ai21">AI21 Labs</option>
                    <option value="nousresearch">Nous Research</option>
                    <option value="cognitivecomputations">Cognitive Computations</option>
                  </select>
                </div>
                <div>
                  <label style={{ color: '#FFF', fontSize: '10px', display: 'block', marginBottom: '6px', fontWeight: 600 }}>
                    MODEL ID <span style={{ color: colors.alert }}>*</span>
                  </label>
                  <div style={{ display: 'flex', gap: '8px', alignItems: 'stretch' }}>
                    <div style={{ flex: 1 }}>
                      {useManualEntry ? (
                        <input
                          type="text"
                          value={newModelConfig.model_id}
                          onChange={(e) => setNewModelConfig({ ...newModelConfig, model_id: e.target.value })}
                          placeholder="e.g. gpt-4o or openai/gpt-4o"
                          style={{
                            width: '100%',
                            background: '#1A1A1A',
                            border: `1px solid #2A2A2A`,
                            color: '#FFF',
                            padding: '10px 12px',
                            fontSize: '12px'
                          }}
                        />
                      ) : (
                        <select
                          value={newModelConfig.model_id}
                          onChange={(e) => setNewModelConfig({ ...newModelConfig, model_id: e.target.value })}
                          style={{
                            width: '100%',
                            background: '#1A1A1A',
                            border: `1px solid #2A2A2A`,
                            color: '#FFF',
                            padding: '10px 12px',
                            fontSize: '12px',
                            cursor: 'pointer'
                          }}
                        >
                          <option value="">Select a model...</option>
                          {newModelConfigModels.map((model) => (
                            <option key={model.id} value={model.id}>
                              {model.name} {model.context_window ? `(${(model.context_window / 1000).toFixed(0)}K)` : ''}
                            </option>
                          ))}
                        </select>
                      )}
                    </div>
                    <button
                      onClick={() => setUseManualEntry(!useManualEntry)}
                      style={{
                        background: useManualEntry ? colors.primary : '#1A1A1A',
                        border: `1px solid ${useManualEntry ? colors.primary : '#2A2A2A'}`,
                        color: useManualEntry ? '#000' : colors.primary,
                        padding: '10px 12px',
                        fontSize: '9px',
                        cursor: 'pointer',
                        fontWeight: 600,
                        whiteSpace: 'nowrap'
                      }}
                    >
                      {useManualEntry ? 'DROPDOWN' : 'MANUAL'}
                    </button>
                  </div>
                </div>
              </div>

              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '16px' }}>
                <div>
                  <label style={{ color: '#FFF', fontSize: '10px', display: 'block', marginBottom: '6px', fontWeight: 600 }}>
                    DISPLAY NAME
                  </label>
                  <input
                    type="text"
                    value={newModelConfig.display_name}
                    onChange={(e) => setNewModelConfig({ ...newModelConfig, display_name: e.target.value })}
                    placeholder="e.g. GPT-4o Mini"
                    style={{
                      width: '100%',
                      background: '#1A1A1A',
                      border: `1px solid #2A2A2A`,
                      color: '#FFF',
                      padding: '10px 12px',
                      fontSize: '12px'
                    }}
                  />
                </div>
                <div>
                  <label style={{ color: '#FFF', fontSize: '10px', display: 'block', marginBottom: '6px', fontWeight: 600 }}>
                    API KEY (Optional)
                  </label>
                  <input
                    type="password"
                    value={newModelConfig.api_key}
                    onChange={(e) => setNewModelConfig({ ...newModelConfig, api_key: e.target.value })}
                    placeholder="Uses provider key if empty"
                    style={{
                      width: '100%',
                      background: '#1A1A1A',
                      border: `1px solid #2A2A2A`,
                      color: '#FFF',
                      padding: '10px 12px',
                      fontSize: '12px',
                      fontFamily: 'monospace'
                    }}
                  />
                </div>
              </div>

              <button
                onClick={handleAddModelConfig}
                disabled={loading || !newModelConfig.model_id}
                style={{
                  background: loading || !newModelConfig.model_id ? '#2A2A2A' : colors.success,
                  color: loading || !newModelConfig.model_id ? '#666' : '#000',
                  border: 'none',
                  padding: '10px 20px',
                  fontSize: '12px',
                  fontWeight: 700,
                  cursor: loading || !newModelConfig.model_id ? 'not-allowed' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px'
                }}
              >
                <Plus size={14} />
                ADD MODEL
              </button>
            </div>
          )}

          {/* Models List */}
          <div style={{ flex: 1, overflowY: 'auto' }}>
            {modelConfigs.length === 0 ? (
              <div style={{
                padding: '40px 20px',
                textAlign: 'center',
                color: '#666',
                border: `1px dashed #2A2A2A`,
                background: '#000'
              }}>
                <Bot size={40} color="#2A2A2A" style={{ marginBottom: '16px' }} />
                <div style={{ fontSize: '13px', marginBottom: '8px', fontWeight: 600, color: '#888' }}>
                  No Custom Models Configured
                </div>
                <div style={{ fontSize: '11px', color: '#666' }}>
                  Click "ADD MODEL" to add custom model configurations
                </div>
              </div>
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {modelConfigs.map((model) => (
                  <div
                    key={model.id}
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '12px',
                      padding: '12px 14px',
                      background: model.is_enabled ? '#1A1A1A' : '#0A0A0A',
                      border: `1px solid ${model.is_enabled ? '#2A2A2A' : '#1A1A1A'}`,
                      opacity: model.is_enabled ? 1 : 0.6
                    }}
                  >
                    <button
                      onClick={() => handleToggleModelEnabled(model.id)}
                      style={{
                        background: 'transparent',
                        border: 'none',
                        cursor: 'pointer',
                        padding: 0
                      }}
                    >
                      {model.is_enabled ? (
                        <ToggleRight size={24} color={colors.success} />
                      ) : (
                        <ToggleLeft size={24} color="#666" />
                      )}
                    </button>
                    <div style={{ flex: 1 }}>
                      <div style={{
                        fontSize: '13px',
                        fontWeight: 600,
                        color: model.is_enabled ? '#FFF' : '#666',
                        marginBottom: '4px'
                      }}>
                        {model.display_name || model.model_id}
                      </div>
                      <div style={{
                        fontSize: '11px',
                        color: '#888',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px'
                      }}>
                        <span style={{
                          padding: '2px 8px',
                          background: colors.primary + '20',
                          border: `1px solid ${colors.primary}`,
                          color: colors.primary,
                          fontWeight: 600,
                          fontSize: '10px'
                        }}>
                          {model.provider.toUpperCase()}
                        </span>
                        <span style={{ fontFamily: 'monospace' }}>{model.model_id}</span>
                        {model.api_key && (
                          <span style={{
                            padding: '2px 8px',
                            background: colors.success + '20',
                            border: `1px solid ${colors.success}`,
                            color: colors.success,
                            fontWeight: 600,
                            fontSize: '10px'
                          }}>
                            HAS KEY
                          </span>
                        )}
                      </div>
                    </div>
                    <div style={{ display: 'flex', gap: '8px' }}>
                      {model.api_key && (
                        <button
                          onClick={() => setShowApiKeys(prev => ({ ...prev, [model.id]: !prev[model.id] }))}
                          style={{
                            background: '#1A1A1A',
                            border: `1px solid #2A2A2A`,
                            color: '#888',
                            padding: '6px 8px',
                            cursor: 'pointer'
                          }}
                        >
                          {showApiKeys[model.id] ? <EyeOff size={14} /> : <Eye size={14} />}
                        </button>
                      )}
                      <button
                        onClick={() => handleDeleteModelConfig(model.id)}
                        style={{
                          background: '#1A1A1A',
                          border: `1px solid #2A2A2A`,
                          color: colors.alert,
                          padding: '6px 12px',
                          fontSize: '11px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '6px',
                          fontWeight: 600
                        }}
                      >
                        <Trash2 size={12} />
                        DELETE
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* Summary */}
          {modelConfigs.length > 0 && (
            <div style={{
              marginTop: '16px',
              padding: '12px 14px',
              background: '#0a2a0a',
              border: `1px solid ${colors.success}`,
              display: 'flex',
              alignItems: 'center',
              gap: '10px'
            }}>
              <Check size={16} color={colors.success} />
              <div style={{ fontSize: '12px', color: colors.success, fontWeight: 600 }}>
                {modelConfigs.filter(m => m.is_enabled).length} ENABLED · {modelConfigs.length} TOTAL MODELS
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
