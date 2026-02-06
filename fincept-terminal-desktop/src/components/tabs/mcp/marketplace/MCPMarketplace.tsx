// MCP Marketplace Component
// Browse and install curated MCP servers - Fincept UI Design System
// Production-ready: ErrorBoundary, memoization, installation tracking

import React, { useState, useCallback, useMemo } from 'react';
import { Search, Download, Zap, CheckCircle, Loader2 } from 'lucide-react';
import { MARKETPLACE_SERVERS, MCPServerDefinition } from './serverDefinitions';
import { MCPServerWithStats } from '../../../../services/mcp/mcpManager';
import PostgresForm from './forms/PostgresForm';
import QuestDBForm from './forms/QuestDBForm';
import GenericForm from './forms/GenericForm';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

interface MCPMarketplaceProps {
  onInstall: (serverConfig: any, callbacks?: {
    onProgress?: (status: string) => void;
    onComplete?: (success: boolean, error?: string) => void;
  }) => Promise<void>;
  installedServers: MCPServerWithStats[];
  installingServerIds?: Set<string>;
}

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

const MCPMarketplaceInner: React.FC<MCPMarketplaceProps> = ({
  onInstall,
  installedServers,
  installingServerIds = new Set()
}) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [configureServer, setConfigureServer] = useState<MCPServerDefinition | null>(null);
  const [localInstallingIds, setLocalInstallingIds] = useState<Set<string>>(new Set());
  const [installProgress, setInstallProgress] = useState<Map<string, string>>(new Map());

  // Combine parent and local installing states
  const allInstallingIds = useMemo(() => {
    return new Set([...installingServerIds, ...localInstallingIds]);
  }, [installingServerIds, localInstallingIds]);

  // Memoize installed server IDs for efficient lookup
  const installedServerIds = useMemo(() => {
    return new Set(installedServers.map(s => s.id));
  }, [installedServers]);

  const isServerInstalled = useCallback((serverId: string) => {
    return installedServerIds.has(serverId);
  }, [installedServerIds]);

  const isServerInstalling = useCallback((serverId: string) => {
    return allInstallingIds.has(serverId);
  }, [allInstallingIds]);

  // Memoize filtered servers
  const filteredServers = useMemo(() => {
    return MARKETPLACE_SERVERS.filter(server => {
      const matchesSearch = server.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
                           server.description.toLowerCase().includes(searchQuery.toLowerCase());
      const matchesCategory = selectedCategory === 'all' || server.category === selectedCategory;
      return matchesSearch && matchesCategory;
    });
  }, [searchQuery, selectedCategory]);

  const categories = ['all', 'data', 'web', 'productivity', 'dev'];
  const categoryLabels: Record<string, string> = {
    all: 'ALL',
    data: 'DATA',
    web: 'WEB',
    productivity: 'TOOLS',
    dev: 'DEV',
  };

  const handleInstallClick = useCallback(async (server: MCPServerDefinition) => {
    if (server.requiresConfig) {
      setConfigureServer(server);
    } else {
      // Immediately show installing state - this happens BEFORE any async work
      setLocalInstallingIds(prev => new Set([...prev, server.id]));
      setInstallProgress(prev => new Map(prev).set(server.id, 'Preparing installation...'));

      // Call onInstall with callbacks for progress updates
      // This is non-blocking - the function returns immediately after saving to DB
      await onInstall(server, {
        onProgress: (status) => {
          setInstallProgress(prev => new Map(prev).set(server.id, status));
        },
        onComplete: (success, error) => {
          // Remove from installing state when done
          setLocalInstallingIds(prev => {
            const next = new Set(prev);
            next.delete(server.id);
            return next;
          });
          setInstallProgress(prev => {
            const next = new Map(prev);
            next.delete(server.id);
            return next;
          });
        }
      });
    }
  }, [onInstall]);

  const handleConfiguredInstall = useCallback(async (config: { args: string[]; env: Record<string, string> }) => {
    if (configureServer) {
      const serverWithConfig = {
        ...configureServer,
        args: [...configureServer.args, ...config.args],
        env: config.env,
      };

      // Close modal first for better UX
      const serverId = configureServer.id;
      setConfigureServer(null);

      // Show installing state immediately
      setLocalInstallingIds(prev => new Set([...prev, serverId]));
      setInstallProgress(prev => new Map(prev).set(serverId, 'Preparing installation...'));

      // Call onInstall with callbacks - non-blocking
      await onInstall(serverWithConfig, {
        onProgress: (status) => {
          setInstallProgress(prev => new Map(prev).set(serverId, status));
        },
        onComplete: (success, error) => {
          setLocalInstallingIds(prev => {
            const next = new Set(prev);
            next.delete(serverId);
            return next;
          });
          setInstallProgress(prev => {
            const next = new Map(prev);
            next.delete(serverId);
            return next;
          });
        }
      });
    }
  }, [configureServer, onInstall]);

  const handleCloseConfig = useCallback(() => {
    setConfigureServer(null);
  }, []);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', fontFamily: FONT_FAMILY }}>
      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>

      {/* Search and Filters */}
      <div style={{ marginBottom: '16px' }}>
        <div style={{ position: 'relative', marginBottom: '12px' }}>
          <Search
            size={10}
            style={{
              position: 'absolute',
              left: '10px',
              top: '50%',
              transform: 'translateY(-50%)',
              color: FINCEPT.GRAY,
            }}
          />
          <input
            type="text"
            placeholder="Search marketplace..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 10px 8px 30px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              fontFamily: FONT_FAMILY,
              outline: 'none',
            }}
            onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
            onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
          />
        </div>

        {/* Categories - Tab Button Style */}
        <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
          {categories.map(cat => (
            <button
              key={cat}
              onClick={() => setSelectedCategory(cat)}
              style={{
                padding: '6px 12px',
                backgroundColor: selectedCategory === cat ? FINCEPT.ORANGE : 'transparent',
                color: selectedCategory === cat ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: 'none',
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                borderRadius: '2px',
                transition: 'all 0.2s',
                fontFamily: FONT_FAMILY,
              }}
            >
              {categoryLabels[cat]}
            </button>
          ))}
        </div>
      </div>

      {/* Server Grid */}
      <div className="mcp-scroll" style={{ flex: 1, overflow: 'auto' }}>
        {filteredServers.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center',
          }}>
            <Zap size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No servers found</span>
            <span style={{ fontSize: '9px', marginTop: '4px' }}>Try a different search term or category</span>
          </div>
        ) : (
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
            gap: '12px',
          }}>
            {filteredServers.map(server => {
              const installed = isServerInstalled(server.id);
              const installing = isServerInstalling(server.id);

              return (
                <div
                  key={server.id}
                  style={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${installing ? FINCEPT.CYAN : FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    padding: '16px',
                    cursor: installed || installing ? 'default' : 'pointer',
                    transition: 'all 0.2s',
                    display: 'flex',
                    flexDirection: 'column',
                    minHeight: '240px',
                    opacity: installing ? 0.8 : 1,
                  }}
                  onMouseEnter={(e) => {
                    if (!installed && !installing) {
                      e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                      e.currentTarget.style.boxShadow = `0 0 12px ${FINCEPT.ORANGE}40`;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!installing) {
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    } else {
                      e.currentTarget.style.borderColor = FINCEPT.CYAN;
                    }
                    e.currentTarget.style.boxShadow = 'none';
                  }}
                >
                  {/* Server Header */}
                  <div style={{ display: 'flex', alignItems: 'flex-start', gap: '12px', marginBottom: '12px' }}>
                    <span style={{ fontSize: '24px' }}>{server.icon}</span>
                    <div style={{ flex: 1 }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                        <span style={{
                          color: FINCEPT.WHITE,
                          fontSize: '11px',
                          fontWeight: 700,
                        }}>
                          {server.name}
                        </span>
                        {installed && <CheckCircle size={10} color={FINCEPT.GREEN} />}
                        {installing && (
                          <Loader2
                            size={10}
                            color={FINCEPT.CYAN}
                            style={{ animation: 'spin 1s linear infinite' }}
                          />
                        )}
                      </div>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: `${FINCEPT.ORANGE}20`,
                        color: FINCEPT.ORANGE,
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        textTransform: 'uppercase',
                        letterSpacing: '0.5px',
                      }}>
                        {server.category}
                      </span>
                    </div>
                  </div>

                  {/* Description */}
                  <p style={{
                    color: FINCEPT.GRAY,
                    fontSize: '10px',
                    lineHeight: '1.5',
                    marginBottom: '12px',
                    flex: 1,
                  }}>
                    {server.description}
                  </p>

                  {/* Info Section */}
                  <div style={{ marginBottom: '12px' }}>
                    {server.requiresConfig && (
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: `${FINCEPT.YELLOW}20`,
                        color: FINCEPT.YELLOW,
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        display: 'inline-block',
                        marginBottom: '8px',
                      }}>
                        REQUIRES CONFIG
                      </span>
                    )}

                    <div style={{
                      display: 'flex',
                      gap: '12px',
                      fontSize: '9px',
                      color: FINCEPT.GRAY,
                      marginBottom: '8px',
                    }}>
                      <span>
                        <span style={{ color: FINCEPT.CYAN, fontWeight: 700 }}>{server.tools.length}</span> tools
                      </span>
                    </div>

                    {/* Command Preview */}
                    <div style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      padding: '6px 8px',
                      fontSize: '9px',
                      borderRadius: '2px',
                      color: FINCEPT.MUTED,
                      fontFamily: FONT_FAMILY,
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap',
                    }}>
                      {server.command} {server.args.join(' ')}
                    </div>
                  </div>

                  {/* Install Button */}
                  {installed ? (
                    <button
                      disabled
                      style={{
                        width: '100%',
                        padding: '8px',
                        backgroundColor: `${FINCEPT.GREEN}20`,
                        color: FINCEPT.GREEN,
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        border: 'none',
                        cursor: 'not-allowed',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '4px',
                        fontFamily: FONT_FAMILY,
                      }}
                    >
                      <CheckCircle size={10} />
                      INSTALLED
                    </button>
                  ) : installing ? (
                    <div style={{ width: '100%' }}>
                      <button
                        disabled
                        style={{
                          width: '100%',
                          padding: '8px 16px',
                          backgroundColor: FINCEPT.CYAN,
                          color: FINCEPT.DARK_BG,
                          border: 'none',
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: 'not-allowed',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                          gap: '6px',
                          fontFamily: FONT_FAMILY,
                          animation: 'pulse 1.5s ease-in-out infinite',
                        }}
                      >
                        <Loader2 size={10} style={{ animation: 'spin 1s linear infinite' }} />
                        INSTALLING...
                      </button>
                      {/* Progress status message */}
                      {installProgress.get(server.id) && (
                        <div style={{
                          marginTop: '6px',
                          padding: '4px 8px',
                          backgroundColor: `${FINCEPT.CYAN}10`,
                          border: `1px solid ${FINCEPT.CYAN}30`,
                          borderRadius: '2px',
                          fontSize: '8px',
                          color: FINCEPT.CYAN,
                          textAlign: 'center',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis',
                          whiteSpace: 'nowrap',
                        }}>
                          {installProgress.get(server.id)}
                        </div>
                      )}
                    </div>
                  ) : (
                    <button
                      onClick={() => handleInstallClick(server)}
                      style={{
                        width: '100%',
                        padding: '8px 16px',
                        backgroundColor: FINCEPT.ORANGE,
                        color: FINCEPT.DARK_BG,
                        border: 'none',
                        borderRadius: '2px',
                        fontSize: '9px',
                        fontWeight: 700,
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '4px',
                        fontFamily: FONT_FAMILY,
                        transition: 'all 0.2s',
                      }}
                      onMouseEnter={(e) => {
                        e.currentTarget.style.backgroundColor = '#FF9922';
                        e.currentTarget.style.transform = 'scale(1.02)';
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.backgroundColor = FINCEPT.ORANGE;
                        e.currentTarget.style.transform = 'scale(1)';
                      }}
                    >
                      <Download size={10} />
                      {server.requiresConfig ? 'CONFIGURE & INSTALL' : 'INSTALL'}
                    </button>
                  )}

                  {/* Documentation Link */}
                  {server.documentation && (
                    <div style={{ marginTop: '8px', textAlign: 'center' }}>
                      <a
                        href={server.documentation}
                        target="_blank"
                        rel="noopener noreferrer"
                        style={{
                          color: FINCEPT.GRAY,
                          fontSize: '9px',
                          textDecoration: 'none',
                          transition: 'all 0.2s',
                        }}
                        onClick={(e) => e.stopPropagation()}
                        onMouseEnter={(e) => { e.currentTarget.style.color = FINCEPT.ORANGE; }}
                        onMouseLeave={(e) => { e.currentTarget.style.color = FINCEPT.GRAY; }}
                      >
                        View Documentation &rarr;
                      </a>
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* Configuration Modal */}
      {configureServer && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.85)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000,
        }}>
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.ORANGE}`,
            borderRadius: '2px',
            width: '90%',
            maxWidth: '560px',
            maxHeight: '80vh',
            overflow: 'auto',
          }}>
            {/* Modal Header */}
            <div style={{
              padding: '12px 16px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              gap: '12px',
            }}>
              <span style={{ fontSize: '24px' }}>{configureServer.icon}</span>
              <div>
                <div style={{
                  color: FINCEPT.ORANGE,
                  fontSize: '11px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                }}>
                  CONFIGURE {configureServer.name.toUpperCase()}
                </div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
                  {configureServer.description}
                </div>
              </div>
            </div>

            {/* Form */}
            <div style={{ padding: '16px' }}>
              {configureServer.id === 'postgres' ? (
                <PostgresForm
                  onSubmit={handleConfiguredInstall}
                  onCancel={handleCloseConfig}
                />
              ) : configureServer.id === 'questdb' ? (
                <QuestDBForm
                  onSubmit={handleConfiguredInstall}
                  onCancel={handleCloseConfig}
                />
              ) : (
                <GenericForm
                  server={configureServer}
                  onSubmit={handleConfiguredInstall}
                  onCancel={handleCloseConfig}
                />
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

// ============================================================================
// Export with ErrorBoundary wrapper
// ============================================================================

const MCPMarketplace: React.FC<MCPMarketplaceProps> = (props) => (
  <ErrorBoundary name="MCP Marketplace" variant="default">
    <MCPMarketplaceInner {...props} />
  </ErrorBoundary>
);

export default MCPMarketplace;
