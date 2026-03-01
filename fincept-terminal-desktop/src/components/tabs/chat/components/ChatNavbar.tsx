import React, { useRef } from 'react';
import { Settings, Bot, Plus } from 'lucide-react';
import { LLMConfig } from '@/services/core/sqliteService';

interface ChatNavbarProps {
  colors: any;
  fontSize: any;
  currentTime: Date;
  activeLLMConfig: LLMConfig | null;
  allLLMConfigs: LLMConfig[];
  showLLMSelector: boolean;
  setShowLLMSelector: (v: boolean) => void;
  llmSelectorRef: React.RefObject<HTMLDivElement | null>;
  t: (key: string) => string;
  onSwitchLLMProvider: (provider: string) => void;
  onNavigateToSettings?: () => void;
  onCreateNewSession: () => void;
}

export function ChatNavbar({
  colors,
  fontSize,
  currentTime,
  activeLLMConfig,
  allLLMConfigs,
  showLLMSelector,
  setShowLLMSelector,
  llmSelectorRef,
  t,
  onSwitchLLMProvider,
  onNavigateToSettings,
  onCreateNewSession,
}: ChatNavbarProps) {
  return (
    <div style={{
      backgroundColor: colors.panel,
      borderBottom: `2px solid ${colors.primary}`,
      padding: '8px 16px',
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      flexShrink: 0,
      boxShadow: `0 2px 8px ${colors.primary}20`
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <span style={{
          color: colors.primary,
          fontWeight: 700,
          fontSize: fontSize.body,
          letterSpacing: '0.5px'
        }}>
          {t('title').toUpperCase()}
        </span>
        <span style={{ color: colors.textMuted }}>|</span>

        {/* LLM Provider Selector */}
        <div ref={llmSelectorRef} style={{ position: 'relative' }}>
          <button
            onClick={() => setShowLLMSelector(!showLLMSelector)}
            style={{
              backgroundColor: `${colors.warning}20`,
              border: `1px solid ${colors.warning}40`,
              color: colors.warning,
              padding: '4px 10px',
              fontSize: fontSize.small,
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              letterSpacing: '0.5px',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = `${colors.warning}30`;
              e.currentTarget.style.borderColor = colors.warning;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = `${colors.warning}20`;
              e.currentTarget.style.borderColor = `${colors.warning}40`;
            }}
            title="Switch LLM Provider"
          >
            <Bot size={10} />
            {activeLLMConfig?.provider.toUpperCase() || 'NO PROVIDER'}
            <span style={{ fontSize: fontSize.tiny }}>▼</span>
          </button>

          {/* Dropdown Menu */}
          {showLLMSelector && (
            <div style={{
              position: 'absolute',
              top: '100%',
              left: 0,
              marginTop: '4px',
              backgroundColor: colors.panel,
              border: `1px solid ${colors.primary}`,
              borderRadius: '2px',
              padding: '4px',
              minWidth: '220px',
              maxHeight: '400px',
              overflowY: 'auto',
              zIndex: 1000,
              boxShadow: `0 4px 12px ${colors.background}80`
            }}>
              <div style={{
                padding: '6px 8px',
                fontSize: fontSize.tiny,
                fontWeight: 700,
                color: colors.textMuted,
                letterSpacing: '0.5px',
                borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`,
                marginBottom: '4px'
              }}>
                SELECT LLM PROVIDER
              </div>
              {allLLMConfigs.length > 0 ? (
                allLLMConfigs.map((config) => (
                  <button
                    key={config.provider}
                    onClick={() => onSwitchLLMProvider(config.provider)}
                    style={{
                      width: '100%',
                      backgroundColor: activeLLMConfig?.provider === config.provider ? `${colors.primary}20` : 'transparent',
                      border: activeLLMConfig?.provider === config.provider ? `1px solid ${colors.primary}` : `1px solid ${'rgba(255,255,255,0.1)'}`,
                      color: activeLLMConfig?.provider === config.provider ? colors.primary : colors.text,
                      padding: '8px',
                      fontSize: fontSize.small,
                      fontWeight: 600,
                      borderRadius: '2px',
                      cursor: 'pointer',
                      textAlign: 'left',
                      marginBottom: '2px',
                      display: 'flex',
                      flexDirection: 'column',
                      gap: '4px',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      if (activeLLMConfig?.provider !== config.provider) {
                        e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.05)';
                        e.currentTarget.style.borderColor = colors.textMuted;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (activeLLMConfig?.provider !== config.provider) {
                        e.currentTarget.style.backgroundColor = 'transparent';
                        e.currentTarget.style.borderColor = 'rgba(255,255,255,0.1)';
                      }
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{ fontWeight: 700, letterSpacing: '0.3px' }}>
                        {config.provider.toUpperCase()}
                      </span>
                      {activeLLMConfig?.provider === config.provider && (
                        <span style={{ color: colors.success, fontSize: fontSize.tiny }}>● ACTIVE</span>
                      )}
                    </div>
                    <div style={{ fontSize: fontSize.tiny, color: colors.textMuted }}>
                      Model: {config.model || 'N/A'}
                    </div>
                    {config.base_url && (
                      <div style={{ fontSize: fontSize.tiny, color: colors.textMuted }}>
                        {config.base_url}
                      </div>
                    )}
                  </button>
                ))
              ) : (
                <div style={{
                  padding: '12px',
                  textAlign: 'center',
                  fontSize: fontSize.small,
                  color: colors.textMuted
                }}>
                  No LLM providers configured. Go to Settings to add one.
                </div>
              )}
            </div>
          )}
        </div>

        <span style={{ color: colors.textMuted }}>|</span>
        <span style={{
          color: colors.text,
          fontSize: fontSize.small,
          letterSpacing: '0.5px'
        }}>
          {currentTime.toLocaleTimeString('en-US', { hour12: false })}
        </span>
      </div>
      <div style={{ display: 'flex', gap: '8px' }}>
        <button
          onClick={() => onNavigateToSettings?.()}
          style={{
            backgroundColor: 'transparent',
            border: `1px solid ${'rgba(255,255,255,0.1)'}`,
            color: colors.textMuted,
            padding: '6px 10px',
            fontSize: fontSize.small,
            fontWeight: 700,
            borderRadius: '2px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            letterSpacing: '0.5px',
            transition: 'all 0.2s'
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.borderColor = colors.primary;
            e.currentTarget.style.color = colors.text;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.borderColor = 'rgba(255,255,255,0.1)';
            e.currentTarget.style.color = colors.textMuted;
          }}
          title="Go to Settings tab to configure LLM providers"
        >
          <Settings size={12} />
          {t('header.settings').toUpperCase()}
        </button>
        <button
          onClick={onCreateNewSession}
          style={{
            backgroundColor: colors.primary,
            border: 'none',
            color: colors.background,
            padding: '8px 16px',
            fontSize: fontSize.small,
            fontWeight: 700,
            borderRadius: '2px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            letterSpacing: '0.5px',
            transition: 'all 0.2s'
          }}
        >
          <Plus size={12} />
          {t('header.newChat').toUpperCase()}
        </button>
      </div>
    </div>
  );
}
