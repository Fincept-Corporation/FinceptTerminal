import React from 'react';
import { Bot, Plus, X, Check } from 'lucide-react';
import type { ModelLibraryPanelProps } from '../types';
import { createStyles } from '../styles';
import { AddModelForm } from './AddModelForm';
import { ModelListItem } from './ModelListItem';

export function ModelLibraryPanel({
  colors,
  modelConfigs,
  showAddModel,
  setShowAddModel,
  loading,
  handleAddModelConfig,
  handleDeleteModelConfig,
  handleToggleModelEnabled,
  newModelConfig,
  setNewModelConfig,
  newModelConfigModels,
  availableProviders,
  useManualEntry,
  setUseManualEntry,
  showApiKeys,
  setShowApiKeys,
}: ModelLibraryPanelProps) {
  const styles = createStyles(colors);
  const enabledCount = modelConfigs.filter(m => m.is_enabled).length;

  return (
    <div style={{
      ...styles.panel,
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '20px',
        ...styles.sectionDivider
      }}>
        <div>
          <div style={{
            ...styles.panelHeader,
            marginBottom: '4px'
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
          {showAddModel ? (
            <>
              <X size={14} /> CANCEL
            </>
          ) : (
            <>
              <Plus size={14} /> ADD MODEL
            </>
          )}
        </button>
      </div>

      {/* Add Model Form */}
      {showAddModel && (
        <AddModelForm
          colors={colors}
          loading={loading}
          newModelConfig={newModelConfig}
          setNewModelConfig={setNewModelConfig}
          newModelConfigModels={newModelConfigModels}
          availableProviders={availableProviders}
          useManualEntry={useManualEntry}
          setUseManualEntry={setUseManualEntry}
          onSubmit={handleAddModelConfig}
        />
      )}

      {/* Models List */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {modelConfigs.length === 0 ? (
          <div style={styles.emptyState as React.CSSProperties}>
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
              <ModelListItem
                key={model.id}
                colors={colors}
                model={model}
                showApiKey={showApiKeys[model.id] || false}
                onToggleApiKey={() => setShowApiKeys(prev => ({
                  ...prev,
                  [model.id]: !prev[model.id]
                }))}
                onToggleEnabled={() => handleToggleModelEnabled(model.id)}
                onDelete={() => handleDeleteModelConfig(model.id)}
              />
            ))}
          </div>
        )}
      </div>

      {/* Summary */}
      {modelConfigs.length > 0 && (
        <div style={styles.summaryBar}>
          <Check size={16} color={colors.success} />
          <div style={{ fontSize: '12px', color: colors.success, fontWeight: 600 }}>
            {enabledCount} ENABLED · {modelConfigs.length} TOTAL MODELS
          </div>
        </div>
      )}
    </div>
  );
}
