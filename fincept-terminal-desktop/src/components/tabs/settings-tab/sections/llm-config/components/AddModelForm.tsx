import React from 'react';
import { Plus } from 'lucide-react';
import type { AddModelFormProps } from '../types';
import { createStyles } from '../styles';
import { PROVIDER_OPTIONS } from '../constants';

export function AddModelForm({
  colors,
  loading,
  newModelConfig,
  setNewModelConfig,
  newModelConfigModels,
  useManualEntry,
  setUseManualEntry,
  onSubmit,
}: AddModelFormProps) {
  const styles = createStyles(colors);

  const isSubmitDisabled = loading || !newModelConfig.model_id.trim();

  return (
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

      {/* Provider and Model ID Row */}
      <div style={styles.formGrid}>
        <div>
          <label style={{ ...styles.label, fontSize: '10px', marginBottom: '6px' }}>
            PROVIDER
          </label>
          <select
            value={newModelConfig.provider}
            onChange={(e) => setNewModelConfig({ ...newModelConfig, provider: e.target.value, model_id: '' })}
            style={{
              ...styles.select,
              background: '#1A1A1A',
              padding: '10px 12px',
              fontSize: '12px'
            }}
          >
            {PROVIDER_OPTIONS.map(option => (
              <option key={option.value} value={option.value}>
                {option.label}
              </option>
            ))}
          </select>
        </div>

        <div>
          <label style={{ ...styles.label, fontSize: '10px', marginBottom: '6px' }}>
            MODEL ID <span style={styles.labelRequired}>*</span>
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
                    ...styles.input,
                    background: '#1A1A1A',
                    padding: '10px 12px',
                    fontSize: '12px'
                  }}
                />
              ) : (
                <select
                  value={newModelConfig.model_id}
                  onChange={(e) => setNewModelConfig({ ...newModelConfig, model_id: e.target.value })}
                  style={{
                    ...styles.select,
                    background: '#1A1A1A',
                    padding: '10px 12px',
                    fontSize: '12px'
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

      {/* Display Name and API Key Row */}
      <div style={{ ...styles.formGrid, marginBottom: '16px' }}>
        <div>
          <label style={{ ...styles.label, fontSize: '10px', marginBottom: '6px' }}>
            DISPLAY NAME
          </label>
          <input
            type="text"
            value={newModelConfig.display_name}
            onChange={(e) => setNewModelConfig({ ...newModelConfig, display_name: e.target.value })}
            placeholder="e.g. GPT-4o Mini"
            style={{
              ...styles.input,
              background: '#1A1A1A',
              padding: '10px 12px',
              fontSize: '12px'
            }}
          />
        </div>
        <div>
          <label style={{ ...styles.label, fontSize: '10px', marginBottom: '6px' }}>
            API KEY (Optional)
          </label>
          <input
            type="password"
            value={newModelConfig.api_key}
            onChange={(e) => setNewModelConfig({ ...newModelConfig, api_key: e.target.value })}
            placeholder="Uses provider key if empty"
            style={{
              ...styles.input,
              background: '#1A1A1A',
              padding: '10px 12px',
              fontSize: '12px',
              fontFamily: 'monospace'
            }}
          />
        </div>
      </div>

      {/* Submit Button */}
      <button
        onClick={onSubmit}
        disabled={isSubmitDisabled}
        style={{
          background: isSubmitDisabled ? '#2A2A2A' : colors.success,
          color: isSubmitDisabled ? '#666' : '#000',
          border: 'none',
          padding: '10px 20px',
          fontSize: '12px',
          fontWeight: 700,
          cursor: isSubmitDisabled ? 'not-allowed' : 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}
      >
        <Plus size={14} />
        ADD MODEL
      </button>
    </div>
  );
}
