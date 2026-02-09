import React from 'react';
import { Bot } from 'lucide-react';
import type { CustomModelConfigPanelProps } from '../types';
import { createStyles } from '../styles';

export function CustomModelConfigPanel({ colors, modelConfig }: CustomModelConfigPanelProps) {
  const styles = createStyles(colors);
  const displayName = modelConfig.display_name || modelConfig.model_id;
  const modelIdDisplay = modelConfig.model_id.includes('/')
    ? modelConfig.model_id.split('/').pop()
    : modelConfig.model_id;

  return (
    <div style={styles.infoBox}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '10px',
        marginBottom: '12px'
      }}>
        <Bot size={20} color={colors.secondary} />
        <span style={{ color: colors.secondary, fontSize: '14px', fontWeight: 700 }}>
          CUSTOM MODEL - {displayName}
        </span>
      </div>
      <div style={{ color: '#AAA', fontSize: '12px', lineHeight: 1.6 }}>
        <div style={{ marginBottom: '8px' }}>
          This model uses the native{' '}
          <strong style={{ color: colors.primary }}>
            {modelConfig.provider.toUpperCase()}
          </strong>{' '}
          API directly.
        </div>
        <div style={{ ...styles.dataRow, flexWrap: 'wrap' }}>
          <div>
            <div style={styles.dataLabel}>PROVIDER</div>
            <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 600 }}>
              {modelConfig.provider.toUpperCase()}
            </div>
          </div>
          <div>
            <div style={styles.dataLabel}>MODEL</div>
            <div style={{ ...styles.dataValue, fontFamily: 'monospace' }}>
              {modelIdDisplay}
            </div>
          </div>
          <div>
            <div style={styles.dataLabel}>API KEY</div>
            <div style={{
              color: modelConfig.api_key ? colors.success : colors.alert,
              fontSize: '12px'
            }}>
              {modelConfig.api_key ? 'Custom key set' : 'No API key'}
            </div>
          </div>
        </div>
        {!modelConfig.api_key && (
          <div style={styles.warningBox}>
            Warning: No API key set for this model. Add your {modelConfig.provider.toUpperCase()} API key in the Model Library or main provider config.
          </div>
        )}
      </div>
    </div>
  );
}
