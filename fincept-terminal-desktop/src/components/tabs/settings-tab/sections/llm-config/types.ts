// LLM Configuration Types
import type { LLMConfig, LLMGlobalSettings, LLMModelConfig } from '@/services/core/sqliteService';
import type { ModelInfo } from '@/services/llmModelsService';
import type { SettingsColors } from '../../types';

export interface LLMConfigSectionProps {
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

export interface NewModelConfig {
  provider: string;
  model_id: string;
  display_name: string;
  api_key: string;
  base_url: string;
}

export interface ProviderSelectorProps {
  colors: SettingsColors;
  activeProvider: string;
  setActiveProvider: (provider: string) => void;
  visibleProviderConfigs: LLMConfig[];
  modelConfigs: LLMModelConfig[];
}

export interface ProviderConfigPanelProps {
  colors: SettingsColors;
  currentConfig: LLMConfig | undefined;
  activeProvider: string;
  updateLLMConfig: (provider: string, field: keyof LLMConfig, value: any) => void;
  ollamaModels: string[];
  ollamaLoading: boolean;
  ollamaError: string | null;
  fetchOllamaModels: () => void;
  providerModels: ModelInfo[];
  providerModelsLoading: boolean;
  providerModelsError: string | null;
  fetchProviderModels: () => void;
  useManualEntry: boolean;
  setUseManualEntry: (value: boolean) => void;
}

export interface FinceptConfigPanelProps {
  colors: SettingsColors;
  sessionApiKey?: string;
}

export interface CustomModelConfigPanelProps {
  colors: SettingsColors;
  modelConfig: LLMModelConfig;
}

export interface GlobalSettingsPanelProps {
  colors: SettingsColors;
  llmGlobalSettings: LLMGlobalSettings;
  setLlmGlobalSettings: React.Dispatch<React.SetStateAction<LLMGlobalSettings>>;
}

export interface ModelLibraryPanelProps {
  colors: SettingsColors;
  modelConfigs: LLMModelConfig[];
  showAddModel: boolean;
  setShowAddModel: (value: boolean) => void;
  loading: boolean;
  handleAddModelConfig: () => void;
  handleDeleteModelConfig: (id: string) => void;
  handleToggleModelEnabled: (id: string) => void;
  newModelConfig: NewModelConfig;
  setNewModelConfig: React.Dispatch<React.SetStateAction<NewModelConfig>>;
  newModelConfigModels: ModelInfo[];
  useManualEntry: boolean;
  setUseManualEntry: (value: boolean) => void;
  showApiKeys: Record<string, boolean>;
  setShowApiKeys: React.Dispatch<React.SetStateAction<Record<string, boolean>>>;
}

export interface AddModelFormProps {
  colors: SettingsColors;
  loading: boolean;
  newModelConfig: NewModelConfig;
  setNewModelConfig: React.Dispatch<React.SetStateAction<NewModelConfig>>;
  newModelConfigModels: ModelInfo[];
  useManualEntry: boolean;
  setUseManualEntry: (value: boolean) => void;
  onSubmit: () => void;
  onCancel: () => void;
}

export interface ModelListItemProps {
  colors: SettingsColors;
  model: LLMModelConfig;
  showApiKey: boolean;
  onToggleApiKey: () => void;
  onToggleEnabled: () => void;
  onDelete: () => void;
}
