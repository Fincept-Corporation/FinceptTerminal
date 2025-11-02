// MCP Integrations Tab - Main Component
// Manage MCP servers and tools in Bloomberg-style interface

import React, { useState, useEffect } from 'react';
import { Plus, RefreshCw, Search, Play, Square, Trash2, Settings, Zap, CheckCircle, XCircle, AlertCircle } from 'lucide-react';
import { mcpManager, MCPServerWithStats } from '../../../services/mcpManager';
import { sqliteService } from '../../../services/sqliteService';
import MCPMarketplace from './marketplace/MCPMarketplace';
import MCPAddServerModal from './MCPAddServerModal';

const MCPTab: React.FC = () => {
  const [view, setView] = useState<'marketplace' | 'installed'>('marketplace');
  const [servers, setServers] = useState<MCPServerWithStats[]>([]);
  const [tools, setTools] = useState<any[]>([]);
  const [isAddModalOpen, setIsAddModalOpen] = useState(false);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [statusMessage, setStatusMessage] = useState('Initializing...');
  const [searchTerm, setSearchTerm] = useState('');

  // Bloomberg colors
  const ORANGE = '#FFA500';
  const WHITE = '#FFFFFF';
  const GRAY = '#787878';
  const DARK_BG = '#0a0a0a';
  const PANEL_BG = '#1a1a1a';
  const BORDER = '#2d2d2d';
  const GREEN = '#10b981';
  const RED = '#ef4444';
  const YELLOW = '#f59e0b';

  useEffect(() => {
    const initializeServers = async () => {
      try {
        // Ensure database is initialized first
        await sqliteService.initialize();

        // Don't auto-start here - DashboardScreen already handles it
        // Just load the data
        await loadData();
      } catch (error) {
        console.error('Failed to initialize MCP tab:', error);
        setStatusMessage('Initialization failed');
      }
    };

    initializeServers();

    const interval = setInterval(() => {
      loadData(true);
    }, 5000);

    return () => clearInterval(interval);
  }, []);

  const loadData = async (silent = false) => {
    try {
      if (!silent) setStatusMessage('Loading servers...');

      const serversData = await mcpManager.getServersWithStats();
      setServers(serversData);

      const toolsData = await sqliteService.getMCPTools();
      setTools(toolsData);

      const runningCount = serversData.filter(s => s.status === 'running').length;
      setStatusMessage(`${runningCount} running | ${toolsData.length} tools available`);
    } catch (error) {
      console.error('Failed to load MCP data:', error);
      setStatusMessage('Error loading data');
    }
  };

  const handleRefresh = async () => {
    setIsRefreshing(true);
    await loadData();
    setTimeout(() => setIsRefreshing(false), 500);
  };

  const handleInstallServer = async (config: any) => {
    try {
      setStatusMessage('Installing server...');
      await mcpManager.installServer(config);
      await loadData();
      setStatusMessage('Server installed successfully');
      setView('installed');
    } catch (error) {
      console.error('Failed to install server:', error);
      setStatusMessage(`Installation failed: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }
  };

  const handleStartServer = async (serverId: string) => {
    try {
      setStatusMessage('Starting server...');
      await mcpManager.startServer(serverId);
      await loadData();
    } catch (error) {
      console.error('Failed to start server:', error);
      setStatusMessage(`Start failed: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }
  };

  const handleStopServer = async (serverId: string) => {
    try {
      setStatusMessage('Stopping server...');
      await mcpManager.stopServer(serverId);
      await loadData();
    } catch (error) {
      console.error('Failed to stop server:', error);
      setStatusMessage(`Stop failed: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }
  };

  const handleRemoveServer = async (serverId: string) => {
    if (!confirm('Remove this MCP server? This will delete all associated data.')) {
      return;
    }

    try {
      setStatusMessage('Removing server...');
      await mcpManager.removeServer(serverId);
      await loadData();
      setStatusMessage('Server removed');
    } catch (error) {
      console.error('Failed to remove server:', error);
      setStatusMessage(`Remove failed: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }
  };

  const handleToggleAutoStart = async (serverId: string, enabled: boolean) => {
    try {
      await mcpManager.updateServerConfig(serverId, { auto_start: enabled });
      await loadData();
      setStatusMessage(enabled ? 'Auto-start enabled' : 'Auto-start disabled');
    } catch (error) {
      console.error('Failed to toggle auto-start:', error);
      setStatusMessage(`Auto-start toggle failed: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'running':
        return <CheckCircle size={14} color={GREEN} />;
      case 'error':
        return <XCircle size={14} color={RED} />;
      default:
        return <AlertCircle size={14} color={GRAY} />;
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'running':
        return GREEN;
      case 'error':
        return RED;
      default:
        return GRAY;
    }
  };

  const runningServers = servers.filter(s => s.status === 'running').length;
  const filteredServers = servers.filter(s =>
    s.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
    s.description.toLowerCase().includes(searchTerm.toLowerCase())
  );

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: DARK_BG,
      color: WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: PANEL_BG,
        borderBottom: `1px solid ${BORDER}`,
        padding: '12px 16px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{
              color: ORANGE,
              fontSize: '14px',
              fontWeight: 'bold',
              letterSpacing: '0.5px'
            }}>
              MCP MARKETPLACE
            </span>
            <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>
            <span style={{ color: GRAY, fontSize: '11px' }}>
              {servers.length} Installed | {runningServers} Running | {tools.length} Tools
            </span>
          </div>

          {/* View Switcher */}
          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
            <button
              onClick={handleRefresh}
              disabled={isRefreshing}
              style={{
                backgroundColor: 'transparent',
                border: `1px solid ${BORDER}`,
                color: WHITE,
                padding: '6px 12px',
                fontSize: '10px',
                cursor: isRefreshing ? 'not-allowed' : 'pointer',
                borderRadius: '3px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                opacity: isRefreshing ? 0.5 : 1,
                fontWeight: 'bold'
              }}
            >
              <RefreshCw size={12} style={{ animation: isRefreshing ? 'spin 1s linear infinite' : 'none' }} />
              REFRESH
            </button>

            <button
              onClick={() => setView('marketplace')}
              style={{
                backgroundColor: view === 'marketplace' ? ORANGE : 'transparent',
                color: view === 'marketplace' ? 'black' : WHITE,
                border: `1px solid ${view === 'marketplace' ? ORANGE : BORDER}`,
                padding: '6px 12px',
                fontSize: '10px',
                cursor: 'pointer',
                borderRadius: '3px',
                fontWeight: 'bold'
              }}
            >
              MARKETPLACE
            </button>
            <button
              onClick={() => setView('installed')}
              style={{
                backgroundColor: view === 'installed' ? ORANGE : 'transparent',
                color: view === 'installed' ? 'black' : WHITE,
                border: `1px solid ${view === 'installed' ? ORANGE : BORDER}`,
                padding: '6px 12px',
                fontSize: '10px',
                cursor: 'pointer',
                borderRadius: '3px',
                fontWeight: 'bold'
              }}
            >
              MY SERVERS ({servers.length})
            </button>

            <button
              onClick={() => setIsAddModalOpen(true)}
              style={{
                backgroundColor: GREEN,
                color: 'black',
                border: 'none',
                padding: '6px 12px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer',
                borderRadius: '3px',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Plus size={12} />
              CUSTOM
            </button>
          </div>
        </div>

        {/* Search Bar (only for installed view) */}
        {view === 'installed' && servers.length > 0 && (
          <div style={{ position: 'relative' }}>
            <Search
              size={14}
              style={{
                position: 'absolute',
                left: '10px',
                top: '50%',
                transform: 'translateY(-50%)',
                color: GRAY,
              }}
            />
            <input
              type="text"
              placeholder="Search installed servers..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              style={{
                width: '100%',
                backgroundColor: DARK_BG,
                border: `1px solid ${BORDER}`,
                color: WHITE,
                padding: '8px 12px 8px 36px',
                fontSize: '11px',
                borderRadius: '4px',
                outline: 'none',
              }}
            />
          </div>
        )}
      </div>

      {/* Content Area */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '16px',
        scrollbarWidth: 'none',
        msOverflowStyle: 'none',
      }}>
        <style>{`
          div::-webkit-scrollbar {
            display: none;
          }
          @keyframes spin {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
          }
        `}</style>

        {view === 'marketplace' ? (
          /* Marketplace View */
          <MCPMarketplace onInstall={handleInstallServer} installedServers={servers} />
        ) : (
          /* Installed Servers View */
          <div>
            {filteredServers.length === 0 ? (
              <div style={{
                textAlign: 'center',
                padding: '60px 20px',
                color: GRAY,
              }}>
                <Zap size={48} style={{ marginBottom: '16px', opacity: 0.3, color: ORANGE }} />
                <h3 style={{ fontSize: '14px', marginBottom: '8px', color: WHITE }}>
                  {searchTerm ? 'No servers found' : 'No MCP servers installed'}
                </h3>
                <p style={{ fontSize: '11px', marginBottom: '16px' }}>
                  {searchTerm ? 'Try a different search term' : 'Install your first MCP server from the marketplace'}
                </p>
                {!searchTerm && (
                  <button
                    onClick={() => setView('marketplace')}
                    style={{
                      backgroundColor: ORANGE,
                      color: 'black',
                      border: 'none',
                      padding: '8px 16px',
                      fontSize: '11px',
                      cursor: 'pointer',
                      borderRadius: '4px',
                      fontWeight: 'bold',
                    }}
                  >
                    BROWSE MARKETPLACE
                  </button>
                )}
              </div>
            ) : (
              <div style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(auto-fill, minmax(320px, 1fr))',
                gap: '16px',
              }}>
                {filteredServers.map(server => {
                  const healthInfo = mcpManager.getServerHealthInfo(server.id);
                  return (
                    <div
                      key={server.id}
                      style={{
                        backgroundColor: PANEL_BG,
                        border: `1px solid ${BORDER}`,
                        borderRadius: '6px',
                        padding: '16px',
                        transition: 'all 0.2s',
                      }}
                      onMouseEnter={(e) => {
                        e.currentTarget.style.borderColor = ORANGE;
                        e.currentTarget.style.boxShadow = `0 0 12px ${ORANGE}40`;
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.borderColor = BORDER;
                        e.currentTarget.style.boxShadow = 'none';
                      }}
                    >
                      {/* Server Header */}
                      <div style={{ display: 'flex', alignItems: 'flex-start', gap: '12px', marginBottom: '12px' }}>
                        <span style={{ fontSize: '28px' }}>{server.icon}</span>
                        <div style={{ flex: 1 }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                            <h3 style={{
                              color: WHITE,
                              fontSize: '13px',
                              fontWeight: 'bold',
                            }}>
                              {server.name}
                            </h3>
                            {getStatusIcon(server.status)}
                          </div>
                          <div style={{
                            display: 'inline-block',
                            backgroundColor: `${getStatusColor(server.status)}20`,
                            color: getStatusColor(server.status),
                            padding: '2px 8px',
                            fontSize: '9px',
                            borderRadius: '3px',
                            textTransform: 'uppercase',
                            fontWeight: 'bold',
                          }}>
                            {server.status}
                          </div>
                        </div>
                      </div>

                      {/* Description */}
                      <p style={{
                        color: GRAY,
                        fontSize: '10px',
                        lineHeight: '1.5',
                        marginBottom: '12px',
                      }}>
                        {server.description}
                      </p>

                      {/* Stats */}
                      <div style={{
                        display: 'flex',
                        gap: '16px',
                        marginBottom: '12px',
                        fontSize: '10px',
                        color: GRAY,
                      }}>
                        <div>
                          <span style={{ color: WHITE, fontWeight: 'bold' }}>{server.toolCount}</span> tools
                        </div>
                        <div>
                          <span style={{ color: WHITE, fontWeight: 'bold' }}>{server.callsToday}</span> calls today
                        </div>
                        {server.auto_start && (
                          <div style={{ color: GREEN }}>
                            <Zap size={10} style={{ display: 'inline', marginRight: '4px' }} />
                            Auto-start
                          </div>
                        )}
                      </div>

                      {/* Error Message */}
                      {healthInfo && healthInfo.errorCount > 0 && (
                        <div style={{
                          padding: '8px',
                          backgroundColor: `${RED}20`,
                          border: `1px solid ${RED}`,
                          borderRadius: '4px',
                          fontSize: '9px',
                          color: RED,
                          marginBottom: '12px',
                        }}>
                          <strong>Errors: {healthInfo.errorCount}</strong>
                          {healthInfo.lastError && (
                            <div style={{ marginTop: '4px' }}>{healthInfo.lastError}</div>
                          )}
                        </div>
                      )}

                      {/* Actions */}
                      <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
                        {server.status === 'running' ? (
                          <button
                            onClick={() => handleStopServer(server.id)}
                            style={{
                              backgroundColor: 'transparent',
                              color: YELLOW,
                              border: `1px solid ${YELLOW}`,
                              padding: '6px 10px',
                              fontSize: '9px',
                              cursor: 'pointer',
                              borderRadius: '3px',
                              display: 'flex',
                              alignItems: 'center',
                              gap: '4px',
                              fontWeight: 'bold',
                            }}
                          >
                            <Square size={10} />
                            STOP
                          </button>
                        ) : (
                          <button
                            onClick={() => handleStartServer(server.id)}
                            style={{
                              backgroundColor: 'transparent',
                              color: GREEN,
                              border: `1px solid ${GREEN}`,
                              padding: '6px 10px',
                              fontSize: '9px',
                              cursor: 'pointer',
                              borderRadius: '3px',
                              display: 'flex',
                              alignItems: 'center',
                              gap: '4px',
                              fontWeight: 'bold',
                            }}
                          >
                            <Play size={10} />
                            START
                          </button>
                        )}

                        <button
                          onClick={() => handleToggleAutoStart(server.id, !server.auto_start)}
                          style={{
                            backgroundColor: 'transparent',
                            color: server.auto_start ? ORANGE : GRAY,
                            border: `1px solid ${server.auto_start ? ORANGE : '#404040'}`,
                            padding: '6px 10px',
                            fontSize: '9px',
                            cursor: 'pointer',
                            borderRadius: '3px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                            fontWeight: 'bold',
                          }}
                        >
                          <Zap size={10} />
                          AUTO
                        </button>

                        <button
                          onClick={() => handleRemoveServer(server.id)}
                          style={{
                            backgroundColor: 'transparent',
                            color: RED,
                            border: `1px solid ${RED}`,
                            padding: '6px 10px',
                            fontSize: '9px',
                            cursor: 'pointer',
                            borderRadius: '3px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                            fontWeight: 'bold',
                          }}
                        >
                          <Trash2 size={10} />
                          REMOVE
                        </button>
                      </div>
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        )}
      </div>

      {/* Tools Footer */}
      <div style={{
        height: '120px',
        borderTop: `1px solid ${BORDER}`,
        backgroundColor: PANEL_BG,
        overflow: 'auto',
        padding: '12px 16px',
        flexShrink: 0,
        scrollbarWidth: 'none',
        msOverflowStyle: 'none',
      }}>
        <div style={{
          color: ORANGE,
          fontSize: '11px',
          fontWeight: 'bold',
          marginBottom: '8px',
          letterSpacing: '0.5px'
        }}>
          AVAILABLE TOOLS ({tools.length})
        </div>

        {tools.length === 0 ? (
          <div style={{ color: GRAY, fontSize: '10px', textAlign: 'center', padding: '20px' }}>
            No tools available. Start an MCP server to see its tools.
          </div>
        ) : (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '8px' }}>
            {tools.map(tool => (
              <div
                key={tool.id}
                style={{
                  backgroundColor: DARK_BG,
                  border: `1px solid ${BORDER}`,
                  padding: '6px 8px',
                  fontSize: '9px',
                  borderRadius: '3px',
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '2px' }}>
                  <Settings size={10} style={{ color: ORANGE }} />
                  <span style={{ color: WHITE, fontWeight: 'bold' }}>{tool.name}</span>
                </div>
                {tool.description && (
                  <div style={{ color: GRAY, fontSize: '8px', marginLeft: '16px' }}>
                    {tool.description.substring(0, 50)}{tool.description.length > 50 ? '...' : ''}
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: DARK_BG,
        borderTop: `1px solid ${BORDER}`,
        padding: '6px 16px',
        fontSize: '9px',
        color: GRAY,
        flexShrink: 0,
        display: 'flex',
        alignItems: 'center',
        gap: '8px'
      }}>
        <span style={{ color: ORANGE }}>●</span>
        {statusMessage}
      </div>

      {/* Add Server Modal */}
      {isAddModalOpen && (
        <MCPAddServerModal
          onClose={() => setIsAddModalOpen(false)}
          onAdd={(config) => {
            handleInstallServer(config);
            setIsAddModalOpen(false);
          }}
        />
      )}
    </div>
  );
};

export default MCPTab;
