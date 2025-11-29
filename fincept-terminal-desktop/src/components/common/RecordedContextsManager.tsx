/**
 * Recorded Contexts Manager
 * UI for managing all recorded contexts across tabs
 * Bloomberg Terminal Style
 */

import React, { useState, useEffect } from 'react';
import { Trash2, Download, Search, Eye, FileJson, FileText, ListTree, X } from 'lucide-react';
import { contextRecorderService } from '@/services/contextRecorderService';
import { RecordedContext } from '@/services/sqliteService';
import { useTerminalTheme } from '@/contexts/ThemeContext';

export const RecordedContextsManager: React.FC = () => {
  const { colors } = useTerminalTheme();
  const [contexts, setContexts] = useState<RecordedContext[]>([]);
  const [filteredContexts, setFilteredContexts] = useState<RecordedContext[]>([]);
  const [searchQuery, setSearchQuery] = useState('');
  const [filterTab, setFilterTab] = useState<string>('all');
  const [filterType, setFilterType] = useState<string>('all');
  const [selectedContext, setSelectedContext] = useState<RecordedContext | null>(null);
  const [previewFormat, setPreviewFormat] = useState<'json' | 'markdown' | 'summary'>('json');
  const [previewContent, setPreviewContent] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [storageStats, setStorageStats] = useState({ count: 0, totalSize: 0, formattedSize: '0 Bytes' });

  useEffect(() => {
    loadContexts();
    loadStorageStats();
  }, []);

  useEffect(() => {
    applyFilters();
  }, [contexts, searchQuery, filterTab, filterType]);

  const loadContexts = async () => {
    setIsLoading(true);
    try {
      const data = await contextRecorderService.getRecordedContexts({ limit: 1000 });
      setContexts(data);
    } catch (error) {
      console.error('[ContextsManager] Failed to load contexts:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const loadStorageStats = async () => {
    try {
      const stats = await contextRecorderService.getStorageStats();
      setStorageStats(stats);
    } catch (error) {
      console.error('[ContextsManager] Failed to load stats:', error);
    }
  };

  const applyFilters = () => {
    let filtered = [...contexts];

    // Filter by tab
    if (filterTab !== 'all') {
      filtered = filtered.filter(ctx => ctx.tab_name === filterTab);
    }

    // Filter by type
    if (filterType !== 'all') {
      filtered = filtered.filter(ctx => ctx.data_type === filterType);
    }

    // Search filter
    if (searchQuery) {
      const query = searchQuery.toLowerCase();
      filtered = filtered.filter(ctx =>
        ctx.label?.toLowerCase().includes(query) ||
        ctx.tab_name.toLowerCase().includes(query) ||
        ctx.data_type.toLowerCase().includes(query) ||
        ctx.tags?.toLowerCase().includes(query)
      );
    }

    setFilteredContexts(filtered);
  };

  const handleDelete = async (id: string) => {
    if (!confirm('Are you sure you want to delete this recorded context?')) return;

    try {
      await contextRecorderService.deleteContext(id);
      await loadContexts();
      await loadStorageStats();
      if (selectedContext?.id === id) {
        setSelectedContext(null);
      }
    } catch (error) {
      console.error('[ContextsManager] Failed to delete context:', error);
    }
  };

  const handleExport = async (id: string) => {
    try {
      const json = await contextRecorderService.exportContext(id);
      const blob = new Blob([json], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `context_${id}.json`;
      a.click();
      URL.revokeObjectURL(url);
    } catch (error) {
      console.error('[ContextsManager] Failed to export context:', error);
    }
  };

  const handlePreview = async (context: RecordedContext) => {
    setSelectedContext(context);
    await loadPreview(context.id, previewFormat);
  };

  const loadPreview = async (contextId: string, format: 'json' | 'markdown' | 'summary') => {
    try {
      const content = await contextRecorderService.formatContextForLLM(contextId, format);
      setPreviewContent(content);
    } catch (error) {
      console.error('[ContextsManager] Failed to load preview:', error);
      setPreviewContent('Error loading preview');
    }
  };

  const handleFormatChange = async (format: 'json' | 'markdown' | 'summary') => {
    setPreviewFormat(format);
    if (selectedContext) {
      await loadPreview(selectedContext.id, format);
    }
  };

  const getUniqueTabNames = () => {
    return Array.from(new Set(contexts.map(ctx => ctx.tab_name))).sort();
  };

  const getUniqueDataTypes = () => {
    return Array.from(new Set(contexts.map(ctx => ctx.data_type))).sort();
  };

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
  };

  return (
    <div style={{ padding: '16px', height: '100%', overflow: 'auto' }}>
      {/* Header with Stats */}
      <div style={{
        backgroundColor: colors.panel,
        border: `1px solid ${colors.textMuted}`,
        marginBottom: '16px',
        padding: '16px'
      }}>
        <div style={{
          borderBottom: `1px solid ${colors.textMuted}`,
          paddingBottom: '12px',
          marginBottom: '16px'
        }}>
          <div style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', marginBottom: '4px' }}>
            RECORDED CONTEXTS
          </div>
          <div style={{ color: colors.textMuted, fontSize: '11px' }}>
            Manage all recorded data contexts from tabs
          </div>
        </div>

        {/* Stats */}
        <div style={{ display: 'flex', gap: '24px', fontSize: '11px' }}>
          <div>
            <span style={{ color: colors.textMuted }}>TOTAL CONTEXTS:</span>{' '}
            <span style={{ color: colors.text, fontWeight: 'bold' }}>{storageStats.count}</span>
          </div>
          <div>
            <span style={{ color: colors.textMuted }}>STORAGE USED:</span>{' '}
            <span style={{ color: colors.text, fontWeight: 'bold' }}>{storageStats.formattedSize}</span>
          </div>
        </div>
      </div>

      {/* Search and Filters */}
      <div style={{ display: 'flex', gap: '8px', marginBottom: '16px', alignItems: 'center' }}>
        {/* Search Input */}
        <div style={{ position: 'relative', flex: 1 }}>
          <Search
            size={14}
            style={{
              position: 'absolute',
              left: '8px',
              top: '50%',
              transform: 'translateY(-50%)',
              color: colors.textMuted
            }}
          />
          <input
            type="text"
            placeholder="Search contexts..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              width: '100%',
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              color: colors.text,
              padding: '6px 8px 6px 32px',
              fontSize: '11px',
              outline: 'none'
            }}
          />
        </div>

        {/* Tab Filter */}
        <select
          value={filterTab}
          onChange={(e) => setFilterTab(e.target.value)}
          style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            color: colors.text,
            padding: '6px 8px',
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: 'pointer',
            outline: 'none',
            minWidth: '150px'
          }}
        >
          <option value="all">ALL TABS</option>
          {getUniqueTabNames().map(tab => (
            <option key={tab} value={tab}>{tab.toUpperCase()}</option>
          ))}
        </select>

        {/* Type Filter */}
        <select
          value={filterType}
          onChange={(e) => setFilterType(e.target.value)}
          style={{
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            color: colors.text,
            padding: '6px 8px',
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: 'pointer',
            outline: 'none',
            minWidth: '150px'
          }}
        >
          <option value="all">ALL TYPES</option>
          {getUniqueDataTypes().map(type => (
            <option key={type} value={type}>{type.toUpperCase()}</option>
          ))}
        </select>
      </div>

      {/* Contexts Table */}
      <div style={{
        backgroundColor: colors.panel,
        border: `1px solid ${colors.textMuted}`,
        overflow: 'auto'
      }}>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ backgroundColor: colors.background, borderBottom: `1px solid ${colors.textMuted}` }}>
              <th style={headerCellStyle(colors)}>LABEL</th>
              <th style={headerCellStyle(colors)}>TAB</th>
              <th style={headerCellStyle(colors)}>TYPE</th>
              <th style={headerCellStyle(colors)}>SIZE</th>
              <th style={headerCellStyle(colors)}>CREATED</th>
              <th style={headerCellStyle(colors)}>TAGS</th>
              <th style={{ ...headerCellStyle(colors), textAlign: 'right' }}>ACTIONS</th>
            </tr>
          </thead>
          <tbody>
            {isLoading ? (
              <tr>
                <td colSpan={7} style={{
                  padding: '32px',
                  textAlign: 'center',
                  color: colors.textMuted,
                  fontSize: '11px'
                }}>
                  Loading contexts...
                </td>
              </tr>
            ) : filteredContexts.length === 0 ? (
              <tr>
                <td colSpan={7} style={{
                  padding: '32px',
                  textAlign: 'center',
                  color: colors.textMuted,
                  fontSize: '11px'
                }}>
                  No recorded contexts found
                </td>
              </tr>
            ) : (
              filteredContexts.map((ctx, index) => (
                <tr
                  key={ctx.id}
                  style={{
                    borderBottom: `1px solid ${colors.textMuted}`,
                    backgroundColor: index % 2 === 0 ? colors.background : 'transparent'
                  }}
                >
                  <td style={cellStyle(colors)}>
                    <span style={{ fontWeight: 'bold' }}>{ctx.label || 'Unnamed'}</span>
                  </td>
                  <td style={cellStyle(colors)}>
                    <span style={badgeStyle(colors.primary)}>{ctx.tab_name}</span>
                  </td>
                  <td style={cellStyle(colors)}>
                    <span style={badgeStyle(colors.textMuted)}>{ctx.data_type}</span>
                  </td>
                  <td style={{ ...cellStyle(colors), color: colors.textMuted }}>
                    {formatBytes(ctx.data_size)}
                  </td>
                  <td style={{ ...cellStyle(colors), color: colors.textMuted }}>
                    {new Date(ctx.created_at).toLocaleString()}
                  </td>
                  <td style={cellStyle(colors)}>
                    {ctx.tags && (
                      <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                        {ctx.tags.split(',').map((tag, i) => (
                          <span key={i} style={tagStyle(colors)}>
                            {tag}
                          </span>
                        ))}
                      </div>
                    )}
                  </td>
                  <td style={{ ...cellStyle(colors), textAlign: 'right' }}>
                    <div style={{ display: 'flex', gap: '4px', justifyContent: 'flex-end' }}>
                      <button
                        onClick={() => handlePreview(ctx)}
                        style={actionButtonStyle(colors)}
                        onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
                        onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                        title="Preview"
                      >
                        <Eye size={12} />
                      </button>
                      <button
                        onClick={() => handleExport(ctx.id)}
                        style={actionButtonStyle(colors)}
                        onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
                        onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                        title="Export"
                      >
                        <Download size={12} />
                      </button>
                      <button
                        onClick={() => handleDelete(ctx.id)}
                        style={{ ...actionButtonStyle(colors), backgroundColor: colors.alert }}
                        onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
                        onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                        title="Delete"
                      >
                        <Trash2 size={12} />
                      </button>
                    </div>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* Preview Modal */}
      {selectedContext && (
        <>
          {/* Backdrop */}
          <div
            style={{
              position: 'fixed',
              top: 0,
              left: 0,
              right: 0,
              bottom: 0,
              backgroundColor: 'rgba(0, 0, 0, 0.8)',
              zIndex: 999
            }}
            onClick={() => setSelectedContext(null)}
          />

          {/* Modal */}
          <div style={{
            position: 'fixed',
            top: '50%',
            left: '50%',
            transform: 'translate(-50%, -50%)',
            backgroundColor: colors.panel,
            border: `2px solid ${colors.primary}`,
            width: '90%',
            maxWidth: '1200px',
            maxHeight: '80vh',
            zIndex: 1000,
            display: 'flex',
            flexDirection: 'column',
            boxShadow: '0 8px 32px rgba(0,0,0,0.8)'
          }}>
            {/* Modal Header */}
            <div style={{
              borderBottom: `1px solid ${colors.textMuted}`,
              padding: '16px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'flex-start'
            }}>
              <div>
                <div style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', marginBottom: '4px' }}>
                  {selectedContext.label || 'CONTEXT PREVIEW'}
                </div>
                <div style={{ color: colors.textMuted, fontSize: '10px' }}>
                  {selectedContext.tab_name} • {selectedContext.data_type}
                </div>
              </div>
              <button
                onClick={() => setSelectedContext(null)}
                style={{
                  backgroundColor: 'transparent',
                  border: 'none',
                  color: colors.textMuted,
                  cursor: 'pointer',
                  padding: '4px'
                }}
                onMouseEnter={(e) => e.currentTarget.style.color = colors.primary}
                onMouseLeave={(e) => e.currentTarget.style.color = colors.textMuted}
              >
                <X size={16} />
              </button>
            </div>

            {/* Format Selector */}
            <div style={{
              borderBottom: `1px solid ${colors.textMuted}`,
              padding: '12px 16px',
              display: 'flex',
              gap: '8px'
            }}>
              <button
                onClick={() => handleFormatChange('json')}
                style={{
                  ...formatButtonStyle(colors, previewFormat === 'json'),
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
                onMouseEnter={(e) => {
                  if (previewFormat !== 'json') e.currentTarget.style.opacity = '0.8';
                }}
                onMouseLeave={(e) => {
                  if (previewFormat !== 'json') e.currentTarget.style.opacity = '1';
                }}
              >
                <FileJson size={12} />
                JSON
              </button>
              <button
                onClick={() => handleFormatChange('markdown')}
                style={{
                  ...formatButtonStyle(colors, previewFormat === 'markdown'),
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
                onMouseEnter={(e) => {
                  if (previewFormat !== 'markdown') e.currentTarget.style.opacity = '0.8';
                }}
                onMouseLeave={(e) => {
                  if (previewFormat !== 'markdown') e.currentTarget.style.opacity = '1';
                }}
              >
                <FileText size={12} />
                MARKDOWN
              </button>
              <button
                onClick={() => handleFormatChange('summary')}
                style={{
                  ...formatButtonStyle(colors, previewFormat === 'summary'),
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
                onMouseEnter={(e) => {
                  if (previewFormat !== 'summary') e.currentTarget.style.opacity = '0.8';
                }}
                onMouseLeave={(e) => {
                  if (previewFormat !== 'summary') e.currentTarget.style.opacity = '1';
                }}
              >
                <ListTree size={12} />
                SUMMARY
              </button>
            </div>

            {/* Preview Content */}
            <div style={{
              flex: 1,
              overflow: 'auto',
              padding: '16px',
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
          </div>
        </>
      )}
    </div>
  );
};

// Helper styles
const headerCellStyle = (colors: any): React.CSSProperties => ({
  padding: '12px',
  fontSize: '10px',
  fontWeight: 'bold',
  color: colors.primary,
  textAlign: 'left'
});

const cellStyle = (colors: any): React.CSSProperties => ({
  padding: '12px',
  fontSize: '11px',
  color: colors.text
});

const badgeStyle = (bgColor: string): React.CSSProperties => ({
  backgroundColor: bgColor,
  color: 'black',
  padding: '2px 6px',
  fontSize: '9px',
  fontWeight: 'bold',
  display: 'inline-block'
});

const tagStyle = (colors: any): React.CSSProperties => ({
  backgroundColor: 'transparent',
  border: `1px solid ${colors.textMuted}`,
  color: colors.textMuted,
  padding: '1px 4px',
  fontSize: '9px',
  fontWeight: 'bold'
});

const actionButtonStyle = (colors: any): React.CSSProperties => ({
  backgroundColor: colors.primary,
  color: 'black',
  border: 'none',
  padding: '4px 8px',
  cursor: 'pointer',
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center'
});

const formatButtonStyle = (colors: any, isActive: boolean): React.CSSProperties => ({
  backgroundColor: isActive ? colors.primary : colors.background,
  color: isActive ? 'black' : colors.text,
  border: `1px solid ${isActive ? colors.primary : colors.textMuted}`,
  padding: '4px 12px',
  fontSize: '10px',
  fontWeight: 'bold',
  cursor: 'pointer'
});

export default RecordedContextsManager;
