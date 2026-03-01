import React from 'react';
import { Edit2, Check, X } from 'lucide-react';
import { LLMConfig, LLMGlobalSettings, RecordedContext } from '@/services/core/sqliteService';
import ContextSelector from '@/components/common/ContextSelector';
import { QuickPrompt } from './types';

interface RightSidePanelProps {
  colors: any;
  fontSize: any;
  activeLLMConfig: LLMConfig | null;
  allLLMConfigs?: LLMConfig[];
  llmGlobalSettings: LLMGlobalSettings;
  mcpToolsCount: number;
  currentSessionUuid: string | null;
  quickPrompts: QuickPrompt[];
  isEditingPrompts: boolean;
  editingPromptIndex: number | null;
  editPromptCmd: string;
  editPromptText: string;
  t: (key: string) => string;
  providerSupportsMCP: () => boolean;
  providerSupportsStreaming: () => boolean;
  getApiKeyStatus: () => { required: boolean; configured: boolean };
  hasCustomSystemPrompt: () => boolean;
  shouldShowBaseUrl: () => boolean;
  onContextsChange: (contexts: RecordedContext[]) => void;
  onSetMessageInput: (v: string) => void;
  onStartEditingPrompt: (index: number) => void;
  onSaveEditedPrompt: () => void;
  onCancelEditingPrompt: () => void;
  onResetQuickPrompts: () => void;
  onEditPromptCmdChange: (v: string) => void;
  onEditPromptTextChange: (v: string) => void;
  onNavigateToTab?: (tabName: string) => void;
}

export function RightSidePanel({
  colors,
  fontSize,
  activeLLMConfig,
  llmGlobalSettings,
  mcpToolsCount,
  currentSessionUuid,
  quickPrompts,
  isEditingPrompts,
  editingPromptIndex,
  editPromptCmd,
  editPromptText,
  t,
  providerSupportsMCP,
  providerSupportsStreaming,
  getApiKeyStatus,
  hasCustomSystemPrompt,
  shouldShowBaseUrl,
  onContextsChange,
  onSetMessageInput,
  onStartEditingPrompt,
  onSaveEditedPrompt,
  onCancelEditingPrompt,
  onResetQuickPrompts,
  onEditPromptCmdChange,
  onEditPromptTextChange,
  onNavigateToTab,
}: RightSidePanelProps) {
  return (
    <div style={{
      width: '300px',
      backgroundColor: colors.panel,
      borderLeft: `1px solid ${'rgba(255,255,255,0.1)'}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Context Selector Section */}
      <div style={{
        borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`
        }}>
          <span style={{
            fontSize: fontSize.small,
            fontWeight: 700,
            color: colors.textMuted,
            letterSpacing: '0.5px'
          }}>
            {t('panel.dataContexts').toUpperCase()}
          </span>
        </div>
        <div style={{ padding: '12px' }}>
          <ContextSelector
            chatSessionUuid={currentSessionUuid}
            onContextsChange={onContextsChange}
          />
        </div>
      </div>

      {/* Quick Prompts Section */}
      <div style={{
        borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <span style={{
            fontSize: fontSize.small,
            fontWeight: 700,
            color: colors.textMuted,
            letterSpacing: '0.5px'
          }}>
            {t('panel.quickPrompts').toUpperCase()}
          </span>
          <div style={{ display: 'flex', gap: '4px' }}>
            {isEditingPrompts ? (
              <>
                <button
                  onClick={onCancelEditingPrompt}
                  style={{
                    padding: '2px 6px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${colors.alert}`,
                    color: colors.alert,
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    letterSpacing: '0.5px'
                  }}
                >
                  <X size={10} />
                </button>
                <button
                  onClick={onSaveEditedPrompt}
                  style={{
                    padding: '2px 6px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${colors.success}`,
                    color: colors.success,
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    letterSpacing: '0.5px'
                  }}
                >
                  <Check size={10} />
                </button>
              </>
            ) : (
              <button
                onClick={onResetQuickPrompts}
                style={{
                  padding: '2px 6px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${colors.textMuted}`,
                  color: colors.textMuted,
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  borderRadius: '2px',
                  cursor: 'pointer',
                  letterSpacing: '0.5px'
                }}
                title="Reset to default prompts"
              >
                RESET
              </button>
            )}
          </div>
        </div>
        <div style={{ padding: '12px' }}>
          {/* Editing form */}
          {isEditingPrompts && editingPromptIndex !== null && (
            <div style={{
              marginBottom: '8px',
              padding: '8px',
              backgroundColor: colors.background,
              border: `1px solid ${colors.primary}`,
              borderRadius: '2px'
            }}>
              <input
                type="text"
                value={editPromptCmd}
                onChange={(e) => onEditPromptCmdChange(e.target.value.toUpperCase())}
                placeholder="LABEL"
                style={{
                  width: '100%',
                  padding: '4px 6px',
                  backgroundColor: colors.panel,
                  border: `1px solid ${'rgba(255,255,255,0.1)'}`,
                  color: colors.text,
                  fontSize: fontSize.small,
                  borderRadius: '2px',
                  marginBottom: '4px',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                }}
              />
              <textarea
                value={editPromptText}
                onChange={(e) => onEditPromptTextChange(e.target.value)}
                placeholder="Prompt text..."
                style={{
                  width: '100%',
                  padding: '4px 6px',
                  backgroundColor: colors.panel,
                  border: `1px solid ${'rgba(255,255,255,0.1)'}`,
                  color: colors.text,
                  fontSize: fontSize.small,
                  borderRadius: '2px',
                  resize: 'vertical',
                  minHeight: '40px',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                }}
              />
            </div>
          )}
          {/* Quick prompt buttons */}
          {quickPrompts.map((item, index) => (
            <div
              key={`${item.cmd}-${index}`}
              style={{
                display: 'flex',
                gap: '4px',
                marginBottom: '6px'
              }}
            >
              <button
                onClick={() => !isEditingPrompts && onSetMessageInput(item.prompt)}
                disabled={isEditingPrompts}
                style={{
                  flex: 1,
                  padding: '6px 10px',
                  backgroundColor: editingPromptIndex === index ? `${colors.primary}20` : 'transparent',
                  border: `1px solid ${editingPromptIndex === index ? colors.primary : 'rgba(255,255,255,0.1)'}`,
                  color: editingPromptIndex === index ? colors.primary : colors.textMuted,
                  fontSize: fontSize.small,
                  fontWeight: 700,
                  borderRadius: '2px',
                  textAlign: 'left',
                  cursor: isEditingPrompts ? 'default' : 'pointer',
                  letterSpacing: '0.5px',
                  transition: 'all 0.2s',
                  opacity: isEditingPrompts && editingPromptIndex !== index ? 0.5 : 1
                }}
                onMouseEnter={(e) => {
                  if (!isEditingPrompts) {
                    e.currentTarget.style.borderColor = colors.primary;
                    e.currentTarget.style.color = colors.text;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isEditingPrompts && editingPromptIndex !== index) {
                    e.currentTarget.style.borderColor = 'rgba(255,255,255,0.1)';
                    e.currentTarget.style.color = colors.textMuted;
                  }
                }}
                title={item.prompt}
              >
                {item.cmd}
              </button>
              {!isEditingPrompts && (
                <button
                  onClick={() => onStartEditingPrompt(index)}
                  style={{
                    padding: '4px 6px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${'rgba(255,255,255,0.1)'}`,
                    color: colors.textMuted,
                    fontSize: fontSize.tiny,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.borderColor = colors.warning;
                    e.currentTarget.style.color = colors.warning;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.borderColor = 'rgba(255,255,255,0.1)';
                    e.currentTarget.style.color = colors.textMuted;
                  }}
                  title="Edit this prompt"
                >
                  <Edit2 size={10} />
                </button>
              )}
            </div>
          ))}
        </div>
      </div>

      {/* System Info Section */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`
        }}>
          <span style={{
            fontSize: fontSize.small,
            fontWeight: 700,
            color: colors.textMuted,
            letterSpacing: '0.5px'
          }}>
            {t('panel.systemInfo').toUpperCase()}
          </span>
        </div>
        <div style={{ padding: '12px' }}>
          {activeLLMConfig ? (
            <>
              <div style={{
                color: colors.secondary,
                fontSize: fontSize.small,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace'
              }}>
                PROVIDER: {activeLLMConfig.provider.toUpperCase()}
              </div>
              <div style={{
                color: colors.secondary,
                fontSize: fontSize.small,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace'
              }}>
                MODEL: {activeLLMConfig.model}
              </div>
              {/* Base URL - shown for Ollama or custom endpoints */}
              {shouldShowBaseUrl() && activeLLMConfig.base_url && (
                <div style={{
                  color: colors.textMuted,
                  fontSize: fontSize.tiny,
                  marginBottom: '4px',
                  letterSpacing: '0.5px',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                  wordBreak: 'break-all'
                }}>
                  ENDPOINT: {activeLLMConfig.base_url}
                </div>
              )}
              <div style={{
                color: colors.secondary,
                fontSize: fontSize.small,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace'
              }}>
                TEMP: {llmGlobalSettings.temperature} | MAX TOKENS: {llmGlobalSettings.max_tokens}
              </div>
              {/* Status badges row */}
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px', marginBottom: '8px' }}>
                {/* Streaming status */}
                <div style={{
                  padding: '2px 6px',
                  backgroundColor: providerSupportsStreaming() ? `${colors.success}20` : `${colors.textMuted}20`,
                  color: providerSupportsStreaming() ? colors.success : colors.textMuted,
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  borderRadius: '2px',
                  letterSpacing: '0.5px'
                }}>
                  STREAMING: {providerSupportsStreaming() ? 'ENABLED' : 'DISABLED'}
                </div>
                {/* API Key status */}
                {(() => {
                  const apiKeyStatus = getApiKeyStatus();
                  if (!apiKeyStatus.required) {
                    return (
                      <div style={{
                        padding: '2px 6px',
                        backgroundColor: `${colors.secondary}20`,
                        color: colors.secondary,
                        fontSize: fontSize.tiny,
                        fontWeight: 700,
                        borderRadius: '2px',
                        letterSpacing: '0.5px'
                      }}>
                        API KEY: NOT REQUIRED
                      </div>
                    );
                  }
                  return (
                    <div style={{
                      padding: '2px 6px',
                      backgroundColor: apiKeyStatus.configured ? `${colors.success}20` : `${colors.alert}20`,
                      color: apiKeyStatus.configured ? colors.success : colors.alert,
                      fontSize: fontSize.tiny,
                      fontWeight: 700,
                      borderRadius: '2px',
                      letterSpacing: '0.5px'
                    }}>
                      API KEY: {apiKeyStatus.configured ? 'CONFIGURED' : 'MISSING'}
                    </div>
                  );
                })()}
                {/* Custom system prompt indicator */}
                {hasCustomSystemPrompt() && (
                  <div style={{
                    padding: '2px 6px',
                    backgroundColor: `${colors.primary}20`,
                    color: colors.primary,
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    borderRadius: '2px',
                    letterSpacing: '0.5px'
                  }}>
                    CUSTOM PROMPT: YES
                  </div>
                )}
              </div>
            </>
          ) : (
            <div style={{
              padding: '2px 6px',
              backgroundColor: `${colors.alert}20`,
              color: colors.alert,
              fontSize: fontSize.tiny,
              fontWeight: 700,
              borderRadius: '2px',
              letterSpacing: '0.5px'
            }}>
              NO LLM PROVIDER CONFIGURED
            </div>
          )}
          <div style={{
            marginTop: '12px',
            paddingTop: '12px',
            borderTop: `1px solid ${'rgba(255,255,255,0.1)'}`
          }}>
            <div style={{
              padding: '2px 6px',
              backgroundColor: mcpToolsCount > 0 ? `${colors.secondary}20` : `${colors.textMuted}20`,
              color: mcpToolsCount > 0 ? colors.secondary : colors.textMuted,
              fontSize: fontSize.tiny,
              fontWeight: 700,
              borderRadius: '2px',
              marginBottom: '8px',
              letterSpacing: '0.5px',
              display: 'inline-block'
            }}>
              MCP TOOLS: {mcpToolsCount > 0 ? `${mcpToolsCount} AVAILABLE [OK]` : 'NONE'}
            </div>
            <button
              onClick={() => onNavigateToTab?.('mcp')}
              style={{
                width: '100%',
                padding: '8px 16px',
                backgroundColor: colors.primary,
                color: colors.background,
                border: 'none',
                fontSize: fontSize.small,
                fontWeight: 700,
                borderRadius: '2px',
                cursor: 'pointer',
                marginBottom: '8px',
                letterSpacing: '0.5px',
                transition: 'all 0.2s'
              }}
              title="Configure MCP servers and tools"
            >
              🔧 MCP INTEGRATION
            </button>
            {mcpToolsCount > 0 && !providerSupportsMCP() && activeLLMConfig && (
              <div style={{
                padding: '6px',
                backgroundColor: colors.panel,
                border: `1px solid ${colors.alert}`,
                borderRadius: '2px',
                color: colors.alert,
                fontSize: fontSize.tiny,
                fontWeight: 700,
                letterSpacing: '0.5px'
              }}>
                [WARN] {activeLLMConfig.provider.toUpperCase()} DOESN'T SUPPORT MCP TOOLS
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
