/**
 * Context Selector Component
 * Allows users to select recorded contexts to include in chat
 */

import React, { useState, useEffect } from 'react';
import { Database, X, Eye, Plus, Trash2 } from 'lucide-react';
import { contextRecorderService } from '@/services/contextRecorderService';
import { RecordedContext } from '@/services/sqliteService';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface ContextSelectorProps {
  chatSessionUuid: string | null;
  onContextsChange?: (contexts: RecordedContext[]) => void;
}

export const ContextSelector: React.FC<ContextSelectorProps> = ({
  chatSessionUuid,
  onContextsChange,
}) => {
  const { colors } = useTerminalTheme();
  const [linkedContexts, setLinkedContexts] = useState<RecordedContext[]>([]);
  const [availableContexts, setAvailableContexts] = useState<RecordedContext[]>([]);
  const [isDialogOpen, setIsDialogOpen] = useState(false);
  const [previewContext, setPreviewContext] = useState<RecordedContext | null>(null);
  const [previewContent, setPreviewContent] = useState('');
  const [hoveredPreview, setHoveredPreview] = useState<string | null>(null);
  const [previewPopoverContext, setPreviewPopoverContext] = useState<RecordedContext | null>(null);

  useEffect(() => {
    if (chatSessionUuid) {
      loadLinkedContexts();
    }
  }, [chatSessionUuid]);

  useEffect(() => {
    if (isDialogOpen) {
      loadAvailableContexts();
    }
  }, [isDialogOpen]);

  const loadLinkedContexts = async () => {
    if (!chatSessionUuid) return;

    try {
      const contexts = await contextRecorderService.getChatContexts(chatSessionUuid);
      setLinkedContexts(contexts);
      onContextsChange?.(contexts);
    } catch (error) {
      console.error('[ContextSelector] Failed to load linked contexts:', error);
    }
  };

  const loadAvailableContexts = async () => {
    try {
      const all = await contextRecorderService.getRecordedContexts({ limit: 500 });
      console.log('[ContextSelector] Loaded available contexts:', all.length, all);
      setAvailableContexts(all);
    } catch (error) {
      console.error('[ContextSelector] Failed to load available contexts:', error);
    }
  };

  const handleLinkContext = async (contextId: string) => {
    if (!chatSessionUuid) return;

    try {
      await contextRecorderService.linkToChat(chatSessionUuid, contextId);
      await loadLinkedContexts();
    } catch (error) {
      console.error('[ContextSelector] Failed to link context:', error);
    }
  };

  const handleUnlinkContext = async (contextId: string) => {
    if (!chatSessionUuid) return;

    try {
      await contextRecorderService.unlinkFromChat(chatSessionUuid, contextId);
      await loadLinkedContexts();
    } catch (error) {
      console.error('[ContextSelector] Failed to unlink context:', error);
    }
  };

  const handleToggleActive = async (contextId: string) => {
    if (!chatSessionUuid) return;

    try {
      await contextRecorderService.toggleChatContextActive(chatSessionUuid, contextId);
      await loadLinkedContexts();
    } catch (error) {
      console.error('[ContextSelector] Failed to toggle context:', error);
    }
  };

  const handlePreview = async (context: RecordedContext) => {
    setPreviewContext(context);
    try {
      const content = await contextRecorderService.formatContextForLLM(context.id, 'markdown');
      setPreviewContent(content);
    } catch (error) {
      console.error('[ContextSelector] Failed to load preview:', error);
      setPreviewContent('Error loading preview');
    }
  };

  const isLinked = (contextId: string) => {
    return linkedContexts.some(ctx => ctx.id === contextId);
  };

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
  };

  if (!chatSessionUuid) {
    return (
      <div style={{
        fontSize: '11px',
        color: colors.textMuted,
        padding: '8px'
      }}>
        Start a chat session to use contexts
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <span style={{ fontSize: '11px', fontWeight: 'bold', color: colors.text }}>
          ACTIVE CONTEXTS ({linkedContexts.length})
        </span>
        <button
          onClick={() => setIsDialogOpen(true)}
          style={{
            backgroundColor: colors.primary,
            color: 'black',
            border: 'none',
            padding: '3px 8px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
          onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
          onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
        >
          <Plus size={12} />
          ADD
        </button>
      </div>

      {/* Linked Contexts List */}
      {linkedContexts.length === 0 ? (
        <div style={{
          fontSize: '10px',
          color: colors.textMuted,
          padding: '8px',
          border: `1px dashed ${colors.textMuted}`,
          textAlign: 'center'
        }}>
          No contexts linked. Click "ADD" to include recorded data.
        </div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          {linkedContexts.map((ctx) => (
            <div
              key={ctx.id}
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                padding: '6px',
                border: `1px solid ${colors.textMuted}`,
                backgroundColor: colors.background,
                fontSize: '10px'
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, minWidth: 0 }}>
                <input
                  type="checkbox"
                  checked={true}
                  onChange={() => handleToggleActive(ctx.id)}
                  style={{
                    width: '12px',
                    height: '12px',
                    cursor: 'pointer',
                    accentColor: colors.primary
                  }}
                />
                <Database size={12} style={{ flexShrink: 0, color: colors.primary }} />
                <span style={{
                  fontWeight: 'bold',
                  color: colors.text,
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                  whiteSpace: 'nowrap'
                }}>
                  {ctx.label || 'Unnamed'}
                </span>
                <span style={{
                  fontSize: '9px',
                  padding: '1px 4px',
                  backgroundColor: 'rgba(255,120,0,0.2)',
                  border: `1px solid ${colors.primary}`,
                  color: colors.primary,
                  flexShrink: 0
                }}>
                  {ctx.tab_name}
                </span>
              </div>
              <div style={{ display: 'flex', gap: '4px', flexShrink: 0 }}>
                <button
                  onMouseEnter={() => setHoveredPreview(ctx.id)}
                  onMouseLeave={() => setHoveredPreview(null)}
                  onClick={() => setPreviewPopoverContext(ctx)}
                  style={{
                    backgroundColor: 'transparent',
                    border: 'none',
                    padding: '2px',
                    cursor: 'pointer',
                    color: hoveredPreview === ctx.id ? colors.primary : colors.textMuted
                  }}
                >
                  <Eye size={12} />
                </button>
                <button
                  onClick={() => handleUnlinkContext(ctx.id)}
                  style={{
                    backgroundColor: 'transparent',
                    border: 'none',
                    padding: '2px',
                    cursor: 'pointer',
                    color: colors.alert
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.opacity = '0.7'}
                  onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                >
                  <Trash2 size={12} />
                </button>
              </div>
            </div>
          ))}
        </div>
      )}

      {/* Preview Popover */}
      {previewPopoverContext && (
        <>
          <div
            style={{
              position: 'fixed',
              top: 0,
              left: 0,
              right: 0,
              bottom: 0,
              zIndex: 999
            }}
            onClick={() => setPreviewPopoverContext(null)}
          />
          <div style={{
            position: 'fixed',
            top: '50%',
            right: '420px',
            transform: 'translateY(-50%)',
            backgroundColor: colors.panel,
            border: `1px solid ${colors.primary}`,
            width: '384px',
            maxHeight: '300px',
            zIndex: 1000,
            boxShadow: '0 4px 12px rgba(0,0,0,0.5)'
          }}>
            <div style={{ padding: '12px', borderBottom: `1px solid ${colors.textMuted}` }}>
              <div style={{ fontWeight: 'bold', fontSize: '11px', color: colors.text }}>
                {previewPopoverContext.label}
              </div>
              <div style={{ fontSize: '9px', color: colors.textMuted, marginTop: '2px' }}>
                {previewPopoverContext.tab_name} • {previewPopoverContext.data_type} • {formatBytes(previewPopoverContext.data_size)}
              </div>
            </div>
            <div style={{
              padding: '8px',
              backgroundColor: colors.background,
              maxHeight: '200px',
              overflow: 'auto'
            }}>
              <pre style={{
                fontSize: '9px',
                whiteSpace: 'pre-wrap',
                fontFamily: 'monospace',
                color: colors.text,
                margin: 0
              }}>
                {previewPopoverContext.raw_data.substring(0, 500)}
                {previewPopoverContext.raw_data.length > 500 ? '...' : ''}
              </pre>
            </div>
          </div>
        </>
      )}

      {/* Add Context Dialog */}
      {isDialogOpen && (
        <>
          {/* Backdrop */}
          <div
            style={{
              position: 'fixed',
              top: 0,
              left: 0,
              right: 0,
              bottom: 0,
              backgroundColor: 'rgba(0,0,0,0.7)',
              zIndex: 1000
            }}
            onClick={() => setIsDialogOpen(false)}
          />

          {/* Dialog */}
          <div style={{
            position: 'fixed',
            top: '50%',
            left: '50%',
            transform: 'translate(-50%, -50%)',
            backgroundColor: colors.panel,
            border: `2px solid ${colors.primary}`,
            width: '90%',
            maxWidth: '800px',
            maxHeight: '80vh',
            zIndex: 1001,
            boxShadow: '0 8px 24px rgba(0,0,0,0.7)',
            display: 'flex',
            flexDirection: 'column'
          }}>
            {/* Header */}
            <div style={{
              padding: '16px',
              borderBottom: `2px solid ${colors.primary}`,
              backgroundColor: colors.background
            }}>
              <div style={{
                fontSize: '14px',
                fontWeight: 'bold',
                color: colors.primary,
                textTransform: 'uppercase'
              }}>
                ADD CONTEXT TO CHAT
              </div>
              <div style={{
                fontSize: '10px',
                color: colors.textMuted,
                marginTop: '4px'
              }}>
                Select recorded contexts to provide additional information to the AI
              </div>
            </div>

            {/* Content */}
            <div style={{
              padding: '16px',
              overflowY: 'auto',
              flex: 1
            }}>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {availableContexts.map((ctx) => (
                  <div
                    key={ctx.id}
                    style={{
                      display: 'flex',
                      alignItems: 'start',
                      justifyContent: 'space-between',
                      padding: '12px',
                      border: `1px solid ${colors.textMuted}`,
                      backgroundColor: colors.background
                    }}
                  >
                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                        <span style={{
                          fontSize: '11px',
                          fontWeight: 'bold',
                          color: colors.text,
                          overflow: 'hidden',
                          textOverflow: 'ellipsis',
                          whiteSpace: 'nowrap'
                        }}>
                          {ctx.label || 'Unnamed Context'}
                        </span>
                        <span style={{
                          fontSize: '9px',
                          padding: '2px 6px',
                          border: `1px solid ${colors.primary}`,
                          color: colors.primary,
                          backgroundColor: 'rgba(255,120,0,0.1)',
                          flexShrink: 0
                        }}>
                          {ctx.tab_name}
                        </span>
                        <span style={{
                          fontSize: '9px',
                          padding: '2px 6px',
                          border: `1px solid ${colors.textMuted}`,
                          color: colors.textMuted,
                          flexShrink: 0
                        }}>
                          {ctx.data_type}
                        </span>
                      </div>
                      <div style={{ fontSize: '10px', color: colors.textMuted }}>
                        {formatBytes(ctx.data_size)} • {new Date(ctx.created_at).toLocaleDateString()}
                      </div>
                    </div>
                    <div style={{ display: 'flex', gap: '4px', marginLeft: '12px' }}>
                      <button
                        onClick={() => handlePreview(ctx)}
                        style={{
                          backgroundColor: colors.background,
                          border: `1px solid ${colors.textMuted}`,
                          color: colors.text,
                          padding: '4px 8px',
                          fontSize: '10px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px'
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.backgroundColor = colors.panel;
                          e.currentTarget.style.borderColor = colors.primary;
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.backgroundColor = colors.background;
                          e.currentTarget.style.borderColor = colors.textMuted;
                        }}
                      >
                        <Eye size={12} />
                      </button>
                      {isLinked(ctx.id) ? (
                        <button
                          onClick={() => handleUnlinkContext(ctx.id)}
                          style={{
                            backgroundColor: colors.alert,
                            border: 'none',
                            color: 'black',
                            padding: '4px 10px',
                            fontSize: '10px',
                            fontWeight: 'bold',
                            cursor: 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px'
                          }}
                          onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
                          onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                        >
                          <X size={12} />
                          REMOVE
                        </button>
                      ) : (
                        <button
                          onClick={() => handleLinkContext(ctx.id)}
                          style={{
                            backgroundColor: colors.primary,
                            border: 'none',
                            color: 'black',
                            padding: '4px 10px',
                            fontSize: '10px',
                            fontWeight: 'bold',
                            cursor: 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px'
                          }}
                          onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
                          onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                        >
                          <Plus size={12} />
                          ADD
                        </button>
                      )}
                    </div>
                  </div>
                ))}
                {availableContexts.length === 0 && (
                  <div style={{
                    textAlign: 'center',
                    padding: '32px',
                    color: colors.textMuted,
                    fontSize: '11px'
                  }}>
                    No recorded contexts available. Start recording data from tabs.
                  </div>
                )}
              </div>
            </div>

            {/* Footer */}
            <div style={{
              padding: '12px 16px',
              borderTop: `1px solid ${colors.textMuted}`,
              backgroundColor: colors.background,
              display: 'flex',
              justifyContent: 'flex-end'
            }}>
              <button
                onClick={() => setIsDialogOpen(false)}
                style={{
                  backgroundColor: colors.panel,
                  border: `1px solid ${colors.textMuted}`,
                  color: colors.text,
                  padding: '6px 16px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = colors.background;
                  e.currentTarget.style.borderColor = colors.primary;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = colors.panel;
                  e.currentTarget.style.borderColor = colors.textMuted;
                }}
              >
                CLOSE
              </button>
            </div>
          </div>
        </>
      )}

      {/* Full Preview Dialog */}
      {previewContext && (
        <>
          <div
            style={{
              position: 'fixed',
              top: 0,
              left: 0,
              right: 0,
              bottom: 0,
              backgroundColor: 'rgba(0,0,0,0.7)',
              zIndex: 1000
            }}
            onClick={() => setPreviewContext(null)}
          />

          <div style={{
            position: 'fixed',
            top: '50%',
            left: '50%',
            transform: 'translate(-50%, -50%)',
            backgroundColor: colors.panel,
            border: `2px solid ${colors.primary}`,
            width: '90%',
            maxWidth: '800px',
            maxHeight: '80vh',
            zIndex: 1001,
            boxShadow: '0 8px 24px rgba(0,0,0,0.7)',
            display: 'flex',
            flexDirection: 'column'
          }}>
            {/* Header */}
            <div style={{
              padding: '16px',
              borderBottom: `2px solid ${colors.primary}`,
              backgroundColor: colors.background
            }}>
              <div style={{
                fontSize: '12px',
                fontWeight: 'bold',
                color: colors.primary,
                textTransform: 'uppercase'
              }}>
                {previewContext.label || 'CONTEXT PREVIEW'}
              </div>
              <div style={{
                fontSize: '10px',
                color: colors.textMuted,
                marginTop: '4px'
              }}>
                {previewContext.tab_name} • {previewContext.data_type}
              </div>
            </div>

            {/* Content */}
            <div style={{
              padding: '16px',
              overflowY: 'auto',
              flex: 1,
              backgroundColor: colors.background
            }}>
              <pre style={{
                fontSize: '10px',
                whiteSpace: 'pre-wrap',
                fontFamily: 'monospace',
                color: colors.text,
                margin: 0
              }}>
                {previewContent}
              </pre>
            </div>

            {/* Footer */}
            <div style={{
              padding: '12px 16px',
              borderTop: `1px solid ${colors.textMuted}`,
              backgroundColor: colors.background,
              display: 'flex',
              justifyContent: 'flex-end'
            }}>
              <button
                onClick={() => setPreviewContext(null)}
                style={{
                  backgroundColor: colors.panel,
                  border: `1px solid ${colors.textMuted}`,
                  color: colors.text,
                  padding: '6px 16px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = colors.background;
                  e.currentTarget.style.borderColor = colors.primary;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = colors.panel;
                  e.currentTarget.style.borderColor = colors.textMuted;
                }}
              >
                CLOSE
              </button>
            </div>
          </div>
        </>
      )}
    </div>
  );
};

export default ContextSelector;
