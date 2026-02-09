import React from 'react';
import { Zap, Bot } from 'lucide-react';
import type { ProviderSelectorProps } from '../types';
import { createStyles } from '../styles';
import { LLM_PROVIDERS } from '../constants';

export function ProviderSelector({
  colors,
  activeProvider,
  setActiveProvider,
  visibleProviderConfigs,
  modelConfigs,
}: ProviderSelectorProps) {
  const styles = createStyles(colors);
  const enabledModelConfigs = modelConfigs.filter(m => m.is_enabled);

  return (
    <div style={styles.panel}>
      <div style={styles.panelHeader}>
        <Zap size={16} color={colors.primary} />
        SELECT AI PROVIDER
      </div>

      {/* Provider Buttons */}
      <div style={{ ...styles.flexRow, marginBottom: '16px' }}>
        {visibleProviderConfigs.map(config => (
          <button
            key={config.provider}
            onClick={() => setActiveProvider(config.provider)}
            style={styles.providerButton(activeProvider === config.provider)}
          >
            {config.provider === LLM_PROVIDERS.FINCEPT && (
              <span style={{ color: activeProvider === config.provider ? '#000' : colors.success }}>
                ●
              </span>
            )}
            {config.provider.toUpperCase()}
          </button>
        ))}
      </div>

      {/* Custom Models from Model Library */}
      {enabledModelConfigs.length > 0 && (
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
          <div style={styles.flexRow}>
            {enabledModelConfigs.map(model => {
              const isSelected = activeProvider === `model:${model.id}`;
              return (
                <button
                  key={model.id}
                  onClick={() => setActiveProvider(`model:${model.id}`)}
                  style={styles.customModelButton(isSelected, colors.secondary)}
                >
                  <span style={styles.providerBadge}>
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
  );
}
