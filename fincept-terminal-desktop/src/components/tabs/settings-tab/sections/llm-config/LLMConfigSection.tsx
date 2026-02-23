import React from 'react';
import { Save, Bot } from 'lucide-react';
import type { LLMConfigSectionProps } from './types';
import { createStyles } from './styles';
import { LLM_PROVIDERS } from './constants';
import { useLLMConfigState } from './hooks/useLLMConfigState';
import { useLLMConfigActions } from './hooks/useLLMConfigActions';
import {
  ProviderSelector,
  FinceptConfigPanel,
  CustomModelConfigPanel,
  ProviderConfigPanel,
  GlobalSettingsPanel,
  ModelLibraryPanel,
} from './components';

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
  const styles = createStyles(colors);

  // Initialize state management
  const state = useLLMConfigState({
    llmConfigs,
    setLlmConfigs,
    modelConfigs: initialModelConfigs,
    setLoading,
    showMessage,
    loadLLMConfigs,
  });

  // Initialize actions
  const actions = useLLMConfigActions({
    llmConfigs,
    llmGlobalSettings,
    modelConfigs: state.modelConfigs,
    activeProvider: state.activeProvider,
    setActiveProvider: state.setActiveProvider,
    setLoading,
    showMessage,
    loadLLMConfigs,
    setShowAddModel: state.setShowAddModel,
    resetNewModelConfig: state.resetNewModelConfig,
  });

  const currentConfig = state.getCurrentLLMConfig();
  const selectedModelConfig = state.getSelectedModelConfig();

  // Determine which panel to show
  const showFinceptPanel = currentConfig?.provider === LLM_PROVIDERS.FINCEPT;
  const showCustomModelPanel = !!selectedModelConfig;
  const showProviderConfigPanel = currentConfig &&
    currentConfig.provider !== LLM_PROVIDERS.FINCEPT &&
    !selectedModelConfig;

  return (
    <div style={styles.container}>
      {/* Header */}
      <div style={styles.header}>
        <div style={styles.headerTitle}>
          <Bot size={24} color={colors.primary} />
          <div>
            <div style={styles.headerTitleText}>LLM CONFIGURATION</div>
            <div style={styles.headerSubtext}>
              Configure AI providers, models, and global settings
            </div>
          </div>
        </div>
        <button
          onClick={actions.handleSaveLLMConfig}
          disabled={loading}
          style={styles.primaryButton(loading)}
        >
          <Save size={14} />
          {loading ? 'SAVING...' : 'SAVE ALL SETTINGS'}
        </button>
      </div>

      {/* Provider Selection */}
      <ProviderSelector
        colors={colors}
        activeProvider={state.activeProvider}
        setActiveProvider={state.setActiveProvider}
        visibleProviderConfigs={state.visibleProviderConfigs}
        modelConfigs={state.modelConfigs}
      />

      {/* Main Content Grid */}
      <div style={styles.twoColumnGrid}>
        {/* Left Column - Provider Config */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
          {/* Fincept Auto-Configured */}
          {showFinceptPanel && (
            <FinceptConfigPanel
              colors={colors}
              sessionApiKey={sessionApiKey}
            />
          )}

          {/* Custom Model from Model Library */}
          {showCustomModelPanel && selectedModelConfig && (
            <CustomModelConfigPanel
              colors={colors}
              modelConfig={selectedModelConfig}
            />
          )}

          {/* Provider Configuration */}
          {showProviderConfigPanel && (
            <ProviderConfigPanel
              colors={colors}
              currentConfig={currentConfig}
              activeProvider={state.activeProvider}
              updateLLMConfig={state.updateLLMConfig}
              ollamaModels={state.ollamaModels}
              ollamaLoading={state.ollamaLoading}
              ollamaError={state.ollamaError}
              fetchOllamaModels={state.fetchOllamaModels}
              providerModels={state.providerModels}
              providerModelsLoading={state.providerModelsLoading}
              providerModelsError={state.providerModelsError}
              fetchProviderModels={state.fetchProviderModels}
              useManualEntry={state.useManualEntryProvider}
              setUseManualEntry={state.setUseManualEntryProvider}
            />
          )}

          {/* Global Settings */}
          <GlobalSettingsPanel
            colors={colors}
            llmGlobalSettings={llmGlobalSettings}
            setLlmGlobalSettings={setLlmGlobalSettings}
          />
        </div>

        {/* Right Column - Model Library */}
        <ModelLibraryPanel
          colors={colors}
          modelConfigs={state.modelConfigs}
          showAddModel={state.showAddModel}
          setShowAddModel={state.setShowAddModel}
          loading={loading}
          handleAddModelConfig={() => actions.handleAddModelConfig(state.newModelConfig)}
          handleDeleteModelConfig={actions.handleDeleteModelConfig}
          handleToggleModelEnabled={actions.handleToggleModelEnabled}
          newModelConfig={state.newModelConfig}
          setNewModelConfig={state.setNewModelConfig}
          newModelConfigModels={state.newModelConfigModels}
          availableProviders={state.availableProviders}
          useManualEntry={state.useManualEntryAddModel}
          setUseManualEntry={state.setUseManualEntryAddModel}
          showApiKeys={state.showApiKeys}
          setShowApiKeys={state.setShowApiKeys}
        />
      </div>
    </div>
  );
}
