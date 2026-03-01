import React from 'react';
import { Bot, User, Clock } from 'lucide-react';
import { ChatMessage, LLMConfig } from '@/services/core/sqliteService';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';
import FinancialSparklines from '../FinancialSparklines';

interface MessageRendererProps {
  colors: any;
  fontSize: any;
  activeLLMConfig: LLMConfig | null;
  streamingContent: string;
  activeToolCalls: Array<{ name: string; args: any; status: 'calling' | 'success' | 'error' }>;
  t: (key: string) => string;
  formatTime: (date: Date | string | undefined) => string;
}

export function WelcomeScreen({
  colors,
  fontSize,
  activeLLMConfig,
  t,
  onCreateNewSession,
}: {
  colors: any;
  fontSize: any;
  activeLLMConfig: LLMConfig | null;
  t: (key: string) => string;
  onCreateNewSession: () => void;
}) {
  return (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      height: '100%',
      backgroundColor: colors.background
    }}>
      <div style={{
        textAlign: 'center',
        padding: '24px',
        maxWidth: '400px'
      }}>
        <Bot size={48} color={colors.primary} style={{ marginBottom: '16px', opacity: 0.8 }} />
        <div style={{
          color: colors.primary,
          fontSize: fontSize.heading,
          fontWeight: 700,
          marginBottom: '8px',
          letterSpacing: '0.5px'
        }}>
          {t('title').toUpperCase()}
        </div>
        <div style={{
          color: colors.text,
          fontSize: fontSize.body,
          marginBottom: '16px',
          lineHeight: '1.5'
        }}>
          {t('subtitle')}
        </div>
        <div style={{
          padding: '6px 12px',
          backgroundColor: `${colors.warning}20`,
          color: colors.warning,
          fontSize: fontSize.small,
          fontWeight: 700,
          borderRadius: '2px',
          marginBottom: '16px',
          display: 'inline-block',
          letterSpacing: '0.5px'
        }}>
          PROVIDER: {activeLLMConfig?.provider.toUpperCase() || 'NONE'} | MODEL: {activeLLMConfig?.model || 'N/A'}
        </div>
        <br />
        <button
          onClick={onCreateNewSession}
          style={{
            padding: '8px 16px',
            backgroundColor: colors.primary,
            color: colors.background,
            border: 'none',
            fontSize: fontSize.small,
            fontWeight: 700,
            borderRadius: '2px',
            cursor: 'pointer',
            letterSpacing: '0.5px',
            transition: 'all 0.2s'
          }}
        >
          {t('header.newChat').toUpperCase()}
        </button>
      </div>
    </div>
  );
}

export function ChatMessageItem({
  message,
  colors,
  fontSize,
  formatTime,
}: {
  message: ChatMessage;
  colors: any;
  fontSize: any;
  formatTime: (date: Date | string | undefined) => string;
}) {
  try {
    return (
      <div key={message.id} style={{ marginBottom: '12px' }}>
        <div style={{
          display: 'flex',
          justifyContent: message.role === 'user' ? 'flex-end' : 'flex-start',
          marginBottom: '4px'
        }}>
          <div style={{
            maxWidth: '85%',
            minWidth: '120px',
            backgroundColor: colors.panel,
            border: `1px solid ${message.role === 'user' ? colors.warning : colors.primary}`,
            borderRadius: '2px',
            padding: '10px'
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              marginBottom: '8px',
              paddingBottom: '6px',
              borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`
            }}>
              {message.role === 'user' ? (
                <User size={12} color={colors.warning} />
              ) : (
                <Bot size={12} color={colors.primary} />
              )}
              <span style={{
                color: message.role === 'user' ? colors.warning : colors.primary,
                fontSize: fontSize.small,
                fontWeight: 700,
                letterSpacing: '0.5px'
              }}>
                {message.role === 'user' ? 'YOU' : 'AI'}
              </span>
              <Clock size={10} color={colors.textMuted} />
              <span style={{
                color: colors.textMuted,
                fontSize: fontSize.small,
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace'
              }}>
                {formatTime(message.timestamp)}
              </span>
              {message.provider && (
                <span style={{
                  color: colors.textMuted,
                  fontSize: fontSize.small,
                  letterSpacing: '0.5px'
                }}>
                  | {message.provider.toUpperCase()}
                </span>
              )}
            </div>
            {/* Tool calls indicator - like Perplexity sources */}
            {message.tool_calls && message.tool_calls.length > 0 && (
              <div style={{
                marginBottom: '12px',
                padding: '8px 12px',
                background: colors.panel,
                border: `1px solid ${'rgba(255,255,255,0.1)'}`,
                borderRadius: '4px',
                display: 'flex',
                flexWrap: 'wrap',
                gap: '6px',
                alignItems: 'center'
              }}>
                <span style={{
                  color: colors.textMuted,
                  fontSize: fontSize.small,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>
                  Tools Used:
                </span>
                {message.tool_calls.map((tool: any, idx: number) => (
                  <div
                    key={idx}
                    style={{
                      padding: '4px 8px',
                      background: tool.status === 'success' ? colors.success + '20' :
                                 tool.status === 'error' ? colors.alert + '20' : colors.primary + '20',
                      border: `1px solid ${tool.status === 'success' ? colors.success :
                                           tool.status === 'error' ? colors.alert : colors.primary}`,
                      borderRadius: '3px',
                      fontSize: fontSize.small,
                      color: tool.status === 'success' ? colors.success :
                             tool.status === 'error' ? colors.alert : colors.primary,
                      letterSpacing: '0.5px',
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px'
                    }}
                  >
                    <span style={{ fontSize: fontSize.tiny }}>
                      {tool.status === 'success' ? '✓' : tool.status === 'error' ? '✗' : '⋯'}
                    </span>
                    {tool.name.replace('edgar_', '').replace(/_/g, ' ')}
                  </div>
                ))}
              </div>
            )}

            {message.role === 'assistant' && message.content ? (
              <>
                <MarkdownRenderer content={message.content} />
                {message.metadata?.chart_data && (
                  <FinancialSparklines
                    data={message.metadata.chart_data}
                    ticker={message.metadata.ticker}
                    company={message.metadata.company}
                    responseText={message.content}
                  />
                )}
              </>
            ) : (
              <div style={{
                color: colors.text,
                fontSize: fontSize.body,
                lineHeight: '1.6',
                whiteSpace: 'pre-wrap',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace'
              }}>
                {message.content || '(Empty message)'}
              </div>
            )}
          </div>
        </div>
      </div>
    );
  } catch (error) {
    console.error('Error rendering message:', error, message);
    return (
      <div key={message.id} style={{
        marginBottom: '12px',
        color: colors.alert,
        fontSize: fontSize.small,
        letterSpacing: '0.5px'
      }}>
        ERROR RENDERING MESSAGE
      </div>
    );
  }
}

export function StreamingMessage({
  colors,
  fontSize,
  streamingContent,
  activeToolCalls,
  t,
}: {
  colors: any;
  fontSize: any;
  streamingContent: string;
  activeToolCalls: Array<{ name: string; args: any; status: 'calling' | 'success' | 'error' }>;
  t: (key: string) => string;
}) {
  if (!streamingContent && activeToolCalls.length === 0) return null;

  return (
    <div style={{ marginBottom: '12px', display: 'flex', justifyContent: 'flex-start' }}>
      <div style={{
        maxWidth: '85%',
        backgroundColor: colors.panel,
        border: `2px solid ${colors.primary}`,
        borderRadius: '2px',
        padding: '10px'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          marginBottom: '8px',
          paddingBottom: '6px',
          borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`
        }}>
          <Bot
            size={12}
            color={colors.primary}
            style={{ animation: 'spin 2s linear infinite' }}
          />
          <span style={{
            color: colors.primary,
            fontSize: fontSize.small,
            fontWeight: 700,
            letterSpacing: '0.5px',
            animation: 'fadeInOut 1.5s ease-in-out infinite'
          }}>
            {t('messages.typing').toUpperCase()}
          </span>
          <span style={{
            color: colors.primary,
            fontSize: fontSize.small,
            animation: 'dots 1.5s steps(4, end) infinite'
          }}>
            <span>.</span>
            <span>.</span>
            <span>.</span>
          </span>
        </div>

        {/* Live tool calls - Perplexity-style */}
        {activeToolCalls.length > 0 && (
          <div style={{
            marginBottom: streamingContent ? '12px' : '0',
            padding: '8px 12px',
            background: colors.background,
            border: `1px solid ${'rgba(255,255,255,0.1)'}`,
            borderRadius: '4px',
            display: 'flex',
            flexWrap: 'wrap',
            gap: '6px',
            alignItems: 'center'
          }}>
            <span style={{
              color: colors.textMuted,
              fontSize: fontSize.small,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>
              Calling:
            </span>
            {activeToolCalls.map((tool, idx) => (
              <div
                key={idx}
                style={{
                  padding: '4px 8px',
                  background: tool.status === 'success' ? colors.success + '20' :
                             tool.status === 'error' ? colors.alert + '20' : colors.primary + '20',
                  border: `1px solid ${tool.status === 'success' ? colors.success :
                                       tool.status === 'error' ? colors.alert : colors.primary}`,
                  borderRadius: '3px',
                  fontSize: fontSize.small,
                  color: tool.status === 'success' ? colors.success :
                         tool.status === 'error' ? colors.alert : colors.primary,
                  letterSpacing: '0.5px',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  animation: tool.status === 'calling' ? 'pulse 1.5s infinite' : 'none'
                }}
              >
                <span style={{ fontSize: fontSize.tiny }}>
                  {tool.status === 'success' ? '✓' : tool.status === 'error' ? '✗' : '⋯'}
                </span>
                {tool.name.replace('edgar_', '').replace('stock_', '').replace(/_/g, ' ')}
              </div>
            ))}
          </div>
        )}

        {streamingContent && (
          <div style={{
            color: colors.text,
            fontSize: fontSize.body,
            lineHeight: '1.6',
            whiteSpace: 'pre-wrap',
            fontFamily: '"IBM Plex Mono", "Consolas", monospace'
          }}>
            {streamingContent}
            <span style={{
              color: colors.primary,
              animation: 'blink 1s infinite'
            }}>▊</span>
          </div>
        )}
      </div>
    </div>
  );
}
