// MCP Integrations Tab - Main Component
// Three-panel terminal layout per Fincept UI Design System

import React, { useState, useEffect } from 'react';
import { Plus, RefreshCw, Search, Play, Square, Trash2, Settings, Zap, CheckCircle, XCircle, AlertCircle, MessageSquare, Server, Wrench, ShoppingBag, ChevronRight } from 'lucide-react';
import { mcpManager, MCPServerWithStats } from '../../../services/mcp/mcpManager';
import { sqliteService } from '../../../services/core/sqliteService';
import { useBrokerContext } from '../../../contexts/BrokerContext';
import { useStockBrokerContextOptional } from '../../../contexts/StockBrokerContext';
import { brokerMCPBridge } from '../../../services/mcp/internal/BrokerMCPBridge';
import { stockBrokerMCPBridge } from '../../../services/mcp/internal/StockBrokerMCPBridge';
import MCPMarketplace from './marketplace/MCPMarketplace';
import MCPAddServerModal from './MCPAddServerModal';
import MCPToolsManagement from './MCPToolsManagement';

interface MCPTabProps {
  onNavigateToTab?: (tabName: string) => void;
}

// Fincept Design System Colors
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

const MCPTab: React.FC<MCPTabProps> = ({ onNavigateToTab }) => {
  const [view, setView] = useState<'marketplace' | 'installed' | 'tools'>('marketplace');
  const [servers, setServers] = useState<MCPServerWithStats[]>([]);
  const [tools, setTools] = useState<any[]>([]);
  const [isAddModalOpen, setIsAddModalOpen] = useState(false);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [statusMessage, setStatusMessage] = useState('Initializing...');
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedServerId, setSelectedServerId] = useState<string | null>(null);

  // Crypto broker context for MCP bridge
  const { activeAdapter, tradingMode, activeBroker } = useBrokerContext();

  // Stock broker context for MCP bridge (optional - may not be available)
  const stockBrokerCtx = useStockBrokerContextOptional();
  const stockAdapter = stockBrokerCtx?.adapter;
  const stockTradingMode = stockBrokerCtx?.tradingMode;
  const stockActiveBroker = stockBrokerCtx?.activeBroker;
  const stockIsAuthenticated = stockBrokerCtx?.isAuthenticated;

  // Connect crypto broker to MCP bridge
  useEffect(() => {
    if (activeAdapter) {
      brokerMCPBridge.connect({
        activeAdapter,
        tradingMode,
        activeBroker,
      });
      console.log(`[MCP] Connected to crypto broker: ${activeBroker} (${tradingMode} mode)`);
    }

    return () => {
      brokerMCPBridge.disconnect();
    };
  }, [activeAdapter, tradingMode, activeBroker]);

  // Connect stock broker to MCP bridge
  useEffect(() => {
    if (stockAdapter && stockIsAuthenticated && stockActiveBroker) {
      stockBrokerMCPBridge.connect({
        activeAdapter: stockAdapter,
        tradingMode: stockTradingMode || 'paper',
        activeBroker: stockActiveBroker,
        isAuthenticated: stockIsAuthenticated,
      });
      console.log(`[MCP] Connected to stock broker: ${stockActiveBroker} (${stockTradingMode} mode)`);
    }

    return () => {
      stockBrokerMCPBridge.disconnect();
    };
  }, [stockAdapter, stockIsAuthenticated, stockActiveBroker, stockTradingMode]);

  useEffect(() => {
    const initializeServers = async () => {
      try {
        await sqliteService.initialize();
        const { mcpToolService } = await import('../../../services/mcp/mcpToolService');
        await mcpToolService.initialize();
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

      const { mcpToolService } = await import('../../../services/mcp/mcpToolService');
      const toolsData = await mcpToolService.getAllTools();
      setTools(toolsData);

      const runningCount = serversData.filter(s => s.status === 'running').length;
      const internalToolsCount = toolsData.filter((t: any) => t.serverId === 'fincept-terminal').length;
      const externalToolsCount = toolsData.length - internalToolsCount;
      setStatusMessage(`${runningCount} servers running | ${internalToolsCount} internal + ${externalToolsCount} external tools`);
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
      if (selectedServerId === serverId) setSelectedServerId(null);
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
        return <CheckCircle size={10} color={FINCEPT.GREEN} />;
      case 'error':
        return <XCircle size={10} color={FINCEPT.RED} />;
      default:
        return <AlertCircle size={10} color={FINCEPT.GRAY} />;
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'running': return FINCEPT.GREEN;
      case 'error': return FINCEPT.RED;
      default: return FINCEPT.GRAY;
    }
  };

  const runningServers = servers.filter(s => s.status === 'running').length;
  const filteredServers = servers.filter(s =>
    s.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
    s.description.toLowerCase().includes(searchTerm.toLowerCase())
  );

  const selectedServer = selectedServerId
    ? servers.find(s => s.id === selectedServerId) || null
    : null;

  const internalToolCount = tools.filter((t: any) => t.serverId === 'fincept-terminal').length;
  const externalToolCount = tools.length - internalToolCount;

  // Get tools for the selected server
  const selectedServerTools = selectedServerId === 'internal'
    ? tools.filter((t: any) => t.serverId === 'fincept-terminal')
    : selectedServer
      ? tools.filter((t: any) => t.serverId === selectedServer.id)
      : [];

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: FONT_FAMILY,
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px',
      overflow: 'hidden',
    }}>
      <style>{`
        .mcp-scroll::-webkit-scrollbar { width: 6px; height: 6px; }
        .mcp-scroll::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        .mcp-scroll::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        .mcp-scroll::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>

      {/* Top Navigation Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Zap size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{
            color: FINCEPT.ORANGE,
            fontSize: '12px',
            fontWeight: 700,
            letterSpacing: '0.5px',
          }}>
            MCP SERVERS
          </span>
          <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT.BORDER }} />
          <span style={{ color: FINCEPT.GRAY, fontSize: '9px', letterSpacing: '0.5px' }}>
            {servers.length} SERVERS | {runningServers} RUNNING | {tools.length} TOOLS
          </span>
        </div>

        <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
          {/* Tab Buttons */}
          {(['installed', 'marketplace', 'tools'] as const).map((tab) => {
            const labels: Record<string, string> = {
              installed: 'MY SERVERS',
              marketplace: 'MARKETPLACE',
              tools: 'ALL TOOLS',
            };
            return (
              <button
                key={tab}
                onClick={() => setView(tab)}
                style={{
                  padding: '6px 12px',
                  backgroundColor: view === tab ? FINCEPT.ORANGE : 'transparent',
                  color: view === tab ? FINCEPT.DARK_BG : FINCEPT.GRAY,
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
                {labels[tab]}
              </button>
            );
          })}

          <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT.BORDER }} />

          {/* Action Buttons */}
          <button
            onClick={() => onNavigateToTab?.('chat')}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              transition: 'all 0.2s',
              fontFamily: FONT_FAMILY,
            }}
          >
            <MessageSquare size={10} />
            AI CHAT
          </button>

          <button
            onClick={handleRefresh}
            disabled={isRefreshing}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: isRefreshing ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: isRefreshing ? 0.5 : 1,
              transition: 'all 0.2s',
              fontFamily: FONT_FAMILY,
            }}
          >
            <RefreshCw size={10} style={{ animation: isRefreshing ? 'spin 1s linear infinite' : 'none' }} />
            REFRESH
          </button>

          <button
            onClick={() => setIsAddModalOpen(true)}
            style={{
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
              gap: '4px',
              fontFamily: FONT_FAMILY,
            }}
          >
            <Plus size={10} />
            ADD SERVER
          </button>
        </div>
      </div>

      {/* Main Content - Three Panel Layout for installed view, full-width for marketplace/tools */}
      {view === 'installed' ? (
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
          {/* Left Panel - Server List (280px) */}
          <div style={{
            width: '280px',
            flexShrink: 0,
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
          }}>
            {/* Left Panel Header */}
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                INSTALLED SERVERS
              </span>
            </div>

            {/* Search */}
            <div style={{ padding: '8px 12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <div style={{ position: 'relative' }}>
                <Search size={10} style={{
                  position: 'absolute',
                  left: '8px',
                  top: '50%',
                  transform: 'translateY(-50%)',
                  color: FINCEPT.GRAY,
                }} />
                <input
                  type="text"
                  placeholder="Search servers..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  style={{
                    width: '100%',
                    padding: '8px 10px 8px 28px',
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
            </div>

            {/* Server List */}
            <div className="mcp-scroll" style={{ flex: 1, overflow: 'auto' }}>
              {/* Internal Tools Entry */}
              <div
                onClick={() => setSelectedServerId('internal')}
                style={{
                  padding: '10px 12px',
                  backgroundColor: selectedServerId === 'internal' ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: selectedServerId === 'internal' ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                }}
                onMouseEnter={(e) => {
                  if (selectedServerId !== 'internal') e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }}
                onMouseLeave={(e) => {
                  if (selectedServerId !== 'internal') e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                  <Zap size={12} style={{ color: FINCEPT.ORANGE }} />
                  <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE }}>
                    Fincept Terminal
                  </span>
                  <CheckCircle size={10} color={FINCEPT.GREEN} />
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginLeft: '20px' }}>
                  <span style={{
                    padding: '2px 6px',
                    backgroundColor: `${FINCEPT.GREEN}20`,
                    color: FINCEPT.GREEN,
                    fontSize: '8px',
                    fontWeight: 700,
                    borderRadius: '2px',
                  }}>BUILT-IN</span>
                  <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                    {internalToolCount} tools
                  </span>
                </div>
              </div>

              {/* External Servers */}
              {filteredServers.length === 0 && !searchTerm ? (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  padding: '32px 16px',
                  color: FINCEPT.MUTED,
                  fontSize: '10px',
                  textAlign: 'center',
                }}>
                  <Server size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                  <span>No external servers</span>
                  <span style={{ fontSize: '9px', marginTop: '4px' }}>Install from marketplace</span>
                </div>
              ) : (
                filteredServers.map(server => (
                  <div
                    key={server.id}
                    onClick={() => setSelectedServerId(server.id)}
                    style={{
                      padding: '10px 12px',
                      backgroundColor: selectedServerId === server.id ? `${FINCEPT.ORANGE}15` : 'transparent',
                      borderLeft: selectedServerId === server.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    }}
                    onMouseEnter={(e) => {
                      if (selectedServerId !== server.id) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    }}
                    onMouseLeave={(e) => {
                      if (selectedServerId !== server.id) e.currentTarget.style.backgroundColor = 'transparent';
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                      <span style={{ fontSize: '14px' }}>{server.icon}</span>
                      <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                        {server.name}
                      </span>
                      {getStatusIcon(server.status)}
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginLeft: '22px' }}>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: `${getStatusColor(server.status)}20`,
                        color: getStatusColor(server.status),
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        textTransform: 'uppercase',
                      }}>{server.status}</span>
                      <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {server.toolCount} tools
                      </span>
                      {server.auto_start && (
                        <Zap size={8} style={{ color: FINCEPT.ORANGE }} />
                      )}
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>

          {/* Center Panel - Server Detail / Default View */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
            {/* Center Panel Header */}
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                {selectedServerId === 'internal'
                  ? 'FINCEPT TERMINAL (INTERNAL)'
                  : selectedServer
                    ? selectedServer.name.toUpperCase()
                    : 'SERVER DETAILS'}
              </span>
              {(selectedServer || selectedServerId === 'internal') && (
                <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>
                  {selectedServerTools.length} TOOLS AVAILABLE
                </span>
              )}
            </div>

            {/* Center Content */}
            <div className="mcp-scroll" style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
              {selectedServerId === 'internal' ? (
                // Internal server detail
                <div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '16px' }}>
                    <Zap size={20} style={{ color: FINCEPT.ORANGE }} />
                    <div>
                      <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>
                        Fincept Terminal
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '2px' }}>
                        Built-in tools for terminal control, trading, portfolio management, backtesting, and more.
                      </div>
                    </div>
                  </div>

                  {/* Stats Row */}
                  <div style={{
                    display: 'flex',
                    gap: '16px',
                    marginBottom: '16px',
                    padding: '12px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                  }}>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>STATUS</div>
                      <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.GREEN}20`, color: FINCEPT.GREEN, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>ACTIVE</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>TOOLS</div>
                      <span style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{internalToolCount}</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>TYPE</div>
                      <span style={{ fontSize: '10px', color: FINCEPT.WHITE }}>Internal</span>
                    </div>
                  </div>

                  {/* Action */}
                  <button
                    onClick={() => setView('tools')}
                    style={{
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
                      gap: '6px',
                      fontFamily: FONT_FAMILY,
                    }}
                  >
                    <Settings size={10} />
                    MANAGE TOOLS
                  </button>
                </div>
              ) : selectedServer ? (
                // External server detail
                <div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '16px' }}>
                    <span style={{ fontSize: '24px' }}>{selectedServer.icon}</span>
                    <div>
                      <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>
                        {selectedServer.name}
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '2px', lineHeight: '1.5' }}>
                        {selectedServer.description}
                      </div>
                    </div>
                  </div>

                  {/* Stats Row */}
                  <div style={{
                    display: 'flex',
                    gap: '16px',
                    marginBottom: '16px',
                    padding: '12px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    flexWrap: 'wrap',
                  }}>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>STATUS</div>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: `${getStatusColor(selectedServer.status)}20`,
                        color: getStatusColor(selectedServer.status),
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        textTransform: 'uppercase',
                      }}>{selectedServer.status}</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>TOOLS</div>
                      <span style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{selectedServer.toolCount}</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>CALLS TODAY</div>
                      <span style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{selectedServer.callsToday}</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>AUTO-START</div>
                      <span style={{ fontSize: '10px', color: selectedServer.auto_start ? FINCEPT.GREEN : FINCEPT.GRAY }}>
                        {selectedServer.auto_start ? 'ENABLED' : 'DISABLED'}
                      </span>
                    </div>
                  </div>

                  {/* Error Info */}
                  {(() => {
                    const healthInfo = mcpManager.getServerHealthInfo(selectedServer.id);
                    if (healthInfo && healthInfo.errorCount > 0) {
                      return (
                        <div style={{
                          padding: '8px 12px',
                          backgroundColor: `${FINCEPT.RED}20`,
                          border: `1px solid ${FINCEPT.RED}`,
                          borderRadius: '2px',
                          fontSize: '9px',
                          color: FINCEPT.RED,
                          marginBottom: '16px',
                        }}>
                          <strong>ERRORS: {healthInfo.errorCount}</strong>
                          {healthInfo.lastError && (
                            <div style={{ marginTop: '4px', color: FINCEPT.WHITE, opacity: 0.8 }}>{healthInfo.lastError}</div>
                          )}
                        </div>
                      );
                    }
                    return null;
                  })()}

                  {/* Actions */}
                  <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                    {selectedServer.status === 'running' ? (
                      <button
                        onClick={() => handleStopServer(selectedServer.id)}
                        style={{
                          padding: '6px 10px',
                          backgroundColor: 'transparent',
                          border: `1px solid ${FINCEPT.YELLOW}`,
                          color: FINCEPT.YELLOW,
                          fontSize: '9px',
                          fontWeight: 700,
                          borderRadius: '2px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                          fontFamily: FONT_FAMILY,
                        }}
                      >
                        <Square size={10} />
                        STOP
                      </button>
                    ) : (
                      <button
                        onClick={() => handleStartServer(selectedServer.id)}
                        style={{
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
                          gap: '4px',
                          fontFamily: FONT_FAMILY,
                        }}
                      >
                        <Play size={10} />
                        START
                      </button>
                    )}

                    <button
                      onClick={() => handleToggleAutoStart(selectedServer.id, !selectedServer.auto_start)}
                      style={{
                        padding: '6px 10px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${selectedServer.auto_start ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                        color: selectedServer.auto_start ? FINCEPT.ORANGE : FINCEPT.GRAY,
                        fontSize: '9px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                        fontFamily: FONT_FAMILY,
                      }}
                    >
                      <Zap size={10} />
                      AUTO-START
                    </button>

                    <button
                      onClick={() => handleRemoveServer(selectedServer.id)}
                      style={{
                        padding: '6px 10px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${FINCEPT.RED}`,
                        color: FINCEPT.RED,
                        fontSize: '9px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                        fontFamily: FONT_FAMILY,
                      }}
                    >
                      <Trash2 size={10} />
                      REMOVE
                    </button>
                  </div>
                </div>
              ) : (
                // No server selected - empty state
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
                  <Server size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                  <span>Select a server to view details</span>
                </div>
              )}
            </div>
          </div>

          {/* Right Panel - Tools for selected server (300px) */}
          <div style={{
            width: '300px',
            flexShrink: 0,
            borderLeft: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
          }}>
            {/* Right Panel Header */}
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                SERVER TOOLS
              </span>
              <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>
                {selectedServerTools.length}
              </span>
            </div>

            {/* Tools List */}
            <div className="mcp-scroll" style={{ flex: 1, overflow: 'auto' }}>
              {selectedServerTools.length === 0 ? (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  height: '100%',
                  color: FINCEPT.MUTED,
                  fontSize: '10px',
                  textAlign: 'center',
                  padding: '16px',
                }}>
                  <Wrench size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                  <span>Select a server to see its tools</span>
                </div>
              ) : (
                selectedServerTools.map((tool: any, index: number) => (
                  <div
                    key={`${tool.serverId}__${tool.name}__${index}`}
                    style={{
                      padding: '10px 12px',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      transition: 'all 0.2s',
                    }}
                    onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
                    onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '2px' }}>
                      {tool.serverId === 'fincept-terminal' ? (
                        <Zap size={10} style={{ color: FINCEPT.GREEN, flexShrink: 0 }} />
                      ) : (
                        <Settings size={10} style={{ color: FINCEPT.ORANGE, flexShrink: 0 }} />
                      )}
                      <span style={{
                        color: FINCEPT.WHITE,
                        fontWeight: 700,
                        fontSize: '10px',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        whiteSpace: 'nowrap',
                      }}>
                        {tool.name}
                      </span>
                    </div>
                    {tool.description && (
                      <div style={{
                        color: FINCEPT.GRAY,
                        fontSize: '9px',
                        marginLeft: '16px',
                        lineHeight: '1.4',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        whiteSpace: 'nowrap',
                      }}>
                        {tool.description}
                      </div>
                    )}
                  </div>
                ))
              )}
            </div>

            {/* Quick Stats Footer */}
            <div style={{
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              gap: '12px',
              fontSize: '9px',
            }}>
              <span style={{ color: FINCEPT.GREEN }}>
                {internalToolCount} INTERNAL
              </span>
              <span style={{ color: FINCEPT.ORANGE }}>
                {externalToolCount} EXTERNAL
              </span>
            </div>
          </div>
        </div>
      ) : (
        /* Marketplace / Tools views - full width */
        <div className="mcp-scroll" style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {view === 'marketplace' ? (
            <MCPMarketplace onInstall={handleInstallServer} installedServers={servers} />
          ) : (
            <MCPToolsManagement />
          )}
        </div>
      )}

      {/* Status Bar (Bottom) */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        flexShrink: 0,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
      }}>
        <span style={{ color: FINCEPT.ORANGE }}>●</span>
        <span>{statusMessage}</span>
        <div style={{ flex: 1 }} />
        <span style={{ color: FINCEPT.CYAN }}>MCP v1.0</span>
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
