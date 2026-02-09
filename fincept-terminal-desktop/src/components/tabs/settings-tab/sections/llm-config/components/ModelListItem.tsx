import React from 'react';
import { ToggleLeft, ToggleRight, Eye, EyeOff, Trash2 } from 'lucide-react';
import type { ModelListItemProps } from '../types';
import { createStyles } from '../styles';

export function ModelListItem({
  colors,
  model,
  showApiKey,
  onToggleApiKey,
  onToggleEnabled,
  onDelete,
}: ModelListItemProps) {
  const styles = createStyles(colors);

  return (
    <div style={styles.modelListItem(model.is_enabled)}>
      {/* Toggle Button */}
      <button
        onClick={onToggleEnabled}
        style={{
          background: 'transparent',
          border: 'none',
          cursor: 'pointer',
          padding: 0
        }}
        aria-label={model.is_enabled ? 'Disable model' : 'Enable model'}
      >
        {model.is_enabled ? (
          <ToggleRight size={24} color={colors.success} />
        ) : (
          <ToggleLeft size={24} color="#666" />
        )}
      </button>

      {/* Model Info */}
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
          gap: '8px',
          flexWrap: 'wrap'
        }}>
          <span style={styles.providerBadge}>
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

      {/* Action Buttons */}
      <div style={{ display: 'flex', gap: '8px' }}>
        {model.api_key && (
          <button
            onClick={onToggleApiKey}
            style={{
              background: '#1A1A1A',
              border: '1px solid #2A2A2A',
              color: '#888',
              padding: '6px 8px',
              cursor: 'pointer'
            }}
            aria-label={showApiKey ? 'Hide API key' : 'Show API key'}
          >
            {showApiKey ? <EyeOff size={14} /> : <Eye size={14} />}
          </button>
        )}
        <button
          onClick={onDelete}
          style={{
            background: '#1A1A1A',
            border: '1px solid #2A2A2A',
            color: colors.alert,
            padding: '6px 12px',
            fontSize: '11px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            fontWeight: 600
          }}
          aria-label="Delete model"
        >
          <Trash2 size={12} />
          DELETE
        </button>
      </div>
    </div>
  );
}
