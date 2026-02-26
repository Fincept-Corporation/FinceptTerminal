import React, { useState, useEffect } from 'react';
import { Key, RefreshCw, Edit3, Check } from 'lucide-react';
import type { ProviderConfigPanelProps } from '../types';
import { createStyles } from '../styles';
import { LLM_PROVIDERS, DEFAULT_OLLAMA_BASE_URL, CUSTOM_MODEL_VALUE } from '../constants';

export function ProviderConfigPanel({
  colors,
  currentConfig,
  activeProvider,
  updateLLMConfig,
  ollamaModels,
  ollamaLoading,
  ollamaError,
  fetchOllamaModels,
  providerModels,
  providerModelsLoading,
  providerModelsError,
  fetchProviderModels,
  useManualEntry,
  setUseManualEntry,
}: ProviderConfigPanelProps) {
  const styles = createStyles(colors);
  const [customModelId, setCustomModelId] = useState('');
  const [isCustomSelected, setIsCustomSelected] = useState(false);

  // Check if current model is a custom one (not in the list)
  useEffect(() => {
    if (currentConfig?.model) {
      const isInOllamaList = ollamaModels.includes(currentConfig.model);
      const isInProviderList = providerModels.some(m => m.id === currentConfig.model);
      const isCustom = currentConfig.model && !isInOllamaList && !isInProviderList && currentConfig.model !== '';

      if (isCustom) {
        setIsCustomSelected(true);
        setCustomModelId(currentConfig.model);
      }
    }
  }, [currentConfig?.model, ollamaModels, providerModels]);

  if (!currentConfig) return null;

  const isOllama = currentConfig.provider === LLM_PROVIDERS.OLLAMA;
  const isFincept = currentConfig.provider === LLM_PROVIDERS.FINCEPT;
  const needsBaseUrl = isOllama ||
    currentConfig.provider === LLM_PROVIDERS.DEEPSEEK ||
    currentConfig.provider === 'openrouter';

  const isModelSelectDisabled = isOllama
    ? ollamaLoading || ollamaModels.length === 0
    : !isFincept && (providerModelsLoading || providerModels.length === 0);

  const handleSelectChange = (value: string) => {
    if (value === CUSTOM_MODEL_VALUE) {
      setIsCustomSelected(true);
      setCustomModelId('');
    } else {
      setIsCustomSelected(false);
      setCustomModelId('');
      updateLLMConfig(activeProvider, 'model', value);
    }
  };

  const handleCustomModelChange = (value: string) => {
    setCustomModelId(value);
    updateLLMConfig(activeProvider, 'model', value);
  };

  return (
    <div style={styles.panel}>
      <div style={{ ...styles.panelHeader, ...styles.sectionDivider, marginBottom: '20px' }}>
        <Key size={16} color={colors.primary} />
        {currentConfig.provider.toUpperCase()} CONFIGURATION
      </div>

      <div style={styles.flexColumn}>
        {/* API Key - not for Ollama */}
        {!isOllama && (
          <div>
            <label style={styles.label}>
              API KEY <span style={styles.labelRequired}>*</span>
            </label>
            <input
              type="password"
              value={currentConfig.api_key || ''}
              onChange={(e) => updateLLMConfig(activeProvider, 'api_key', e.target.value)}
              placeholder={`Enter your ${activeProvider} API key`}
              style={styles.input}
            />
          </div>
        )}

        {/* Base URL - only for specific providers */}
        {needsBaseUrl && (
          <div>
            <label style={styles.label}>BASE URL</label>
            <input
              type="text"
              value={currentConfig.base_url || ''}
              onChange={(e) => updateLLMConfig(activeProvider, 'base_url', e.target.value)}
              placeholder={isOllama ? DEFAULT_OLLAMA_BASE_URL : 'https://api.example.com'}
              style={styles.input}
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
            <label style={{ ...styles.label, marginBottom: 0 }}>MODEL</label>
            <div style={{ display: 'flex', gap: '8px' }}>
              {isOllama ? (
                <button
                  onClick={fetchOllamaModels}
                  disabled={ollamaLoading}
                  style={styles.secondaryButton(false)}
                >
                  <RefreshCw size={12} className={ollamaLoading ? 'animate-spin' : ''} />
                  {ollamaLoading ? 'LOADING...' : 'REFRESH MODELS'}
                </button>
              ) : !isFincept && (
                <button
                  onClick={fetchProviderModels}
                  disabled={providerModelsLoading}
                  style={styles.secondaryButton(false)}
                >
                  <RefreshCw size={12} className={providerModelsLoading ? 'animate-spin' : ''} />
                  {providerModelsLoading ? 'LOADING...' : 'REFRESH MODELS'}
                </button>
              )}
              <button
                onClick={() => setUseManualEntry(!useManualEntry)}
                style={styles.secondaryButton(useManualEntry)}
              >
                <Edit3 size={12} />
                {useManualEntry ? 'USE DROPDOWN' : 'MANUAL ENTRY'}
              </button>
            </div>
          </div>

          {!useManualEntry ? (
            <div>
              <select
                value={isCustomSelected ? CUSTOM_MODEL_VALUE : currentConfig.model}
                onChange={(e) => handleSelectChange(e.target.value)}
                disabled={isModelSelectDisabled}
                style={styles.select}
              >
                {isOllama ? (
                  <>
                    {ollamaModels.length === 0 && !ollamaLoading && (
                      <option value="">No models found - Click REFRESH</option>
                    )}
                    {ollamaLoading && <option value="">Loading models...</option>}
                    {ollamaModels.map((model) => (
                      <option key={model} value={model}>{model}</option>
                    ))}
                    <option value={CUSTOM_MODEL_VALUE}>── Custom Model ──</option>
                  </>
                ) : (
                  <>
                    {providerModels.length === 0 && !providerModelsLoading && (
                      <option value="">Click "REFRESH MODELS" to load available models</option>
                    )}
                    {providerModelsLoading && <option value="">Loading models...</option>}
                    {providerModels.map((model) => (
                      <option key={model.id} value={model.id}>
                        {model.name} {model.context_window ? `(${(model.context_window / 1000).toFixed(0)}K)` : ''}
                      </option>
                    ))}
                    <option value={CUSTOM_MODEL_VALUE}>── Custom Model ──</option>
                  </>
                )}
              </select>

              {/* Custom Model Input */}
              {isCustomSelected && (
                <input
                  type="text"
                  value={customModelId}
                  onChange={(e) => handleCustomModelChange(e.target.value)}
                  placeholder="Enter custom model ID (e.g., gpt-4-turbo-preview)"
                  style={{ ...styles.input, marginTop: '8px' }}
                />
              )}

              {/* Error Messages */}
              {isOllama && ollamaError && (
                <div style={styles.errorMessage}>{ollamaError}</div>
              )}
              {!isOllama && providerModelsError && (
                <div style={styles.errorMessage}>{providerModelsError}</div>
              )}

              {/* Success Messages */}
              {isOllama && ollamaModels.length > 0 && (
                <div style={styles.successMessage}>
                  <Check size={14} />
                  {ollamaModels.length} model{ollamaModels.length !== 1 ? 's' : ''} available
                </div>
              )}
              {!isOllama && !isFincept && (
                providerModels.length > 0 ? (
                  <div style={{
                    marginTop: '8px',
                    fontSize: '11px',
                    color: '#AAA',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '4px'
                  }}>
                    <div style={styles.successMessage}>
                      <Check size={14} />
                      {providerModels.length} model{providerModels.length !== 1 ? 's' : ''} available
                    </div>
                    <div style={{ fontSize: '10px', color: '#666' }}>
                      Models fetched from {currentConfig.provider.toUpperCase()} API
                      {providerModelsLoading && ' (updating...)'}
                    </div>
                  </div>
                ) : !providerModelsLoading && (
                  <div style={{ marginTop: '8px', fontSize: '10px', color: '#888', fontStyle: 'italic' }}>
                    💡 Tip: Click "REFRESH MODELS" to see available models from {currentConfig.provider.toUpperCase()}
                  </div>
                )
              )}
            </div>
          ) : (
            <input
              type="text"
              value={currentConfig.model}
              onChange={(e) => updateLLMConfig(activeProvider, 'model', e.target.value)}
              placeholder="Enter model name (e.g., gpt-4, claude-3-sonnet)"
              style={styles.input}
            />
          )}
        </div>
      </div>
    </div>
  );
}
