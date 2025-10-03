// MCP Integrations Tab - Main Component
// Manage MCP servers and tools in Bloomberg-style interface

import React, { useState, useEffect } from 'react';
import { Plus, RefreshCw } from 'lucide-react';
import { mcpManager, MCPServerWithStats } from '../../../services/mcpManager';
import { sqliteService } from '../../../services/sqliteService';
import MCPServerCard from './MCPServerCard';
import MCPMarketplace from './MCPMarketplace';
import MCPAddServerModal from './MCPAddServerModal';

const MCPTab: React.FC = () => {
  const [servers, setServers] = useState<MCPServerWithStats[]>([]);
  const [tools, setTools] = useState<any[]>([]);
  const [isAddModalOpen, setIsAddModalOpen] = useState(false);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [statusMessage, setStatusMessage] = useState('Initializing...');

  // Bloomberg colors
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';
  const BLOOMBERG_GREEN = '#00C800';

  useEffect(() => {
    // Auto-start servers on mount
    const initializeServers = async () => {
      try {
        await mcpManager.startAutoStartServers();
      } catch (error) {
        console.error('Failed to auto-start servers:', error);
      }
      loadData();
    };

    initializeServers();

    // Auto-refresh every 5 seconds
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
      setStatusMessage(`${runningCount} servers connected | ${toolsData.length} tools available`);
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

  const runningServers = servers.filter(s => s.status === 'running').length;

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px'
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '8px 12px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold', fontSize: '12px' }}>
            MCP INTEGRATIONS
          </span>
          <span style={{ color: BLOOMBERG_GRAY }}>|</span>
          <span style={{ color: runningServers > 0 ? BLOOMBERG_GREEN : BLOOMBERG_GRAY, fontSize: '10px' }}>
            {runningServers} Running
          </span>
          <span style={{ color: BLOOMBERG_GRAY }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE, fontSize: '10px' }}>
            {tools.length} Tools
          </span>
        </div>

        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={handleRefresh}
            disabled={isRefreshing}
            style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '4px 10px',
              fontSize: '10px',
              cursor: isRefreshing ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: isRefreshing ? 0.5 : 1
            }}
          >
            <RefreshCw size={12} style={{ animation: isRefreshing ? 'spin 1s linear infinite' : 'none' }} />
            REFRESH
          </button>

          <button
            onClick={() => setIsAddModalOpen(true)}
            style={{
              backgroundColor: BLOOMBERG_ORANGE,
              color: 'black',
              border: 'none',
              padding: '4px 10px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <Plus size={12} />
            ADD SERVER
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel: Installed Servers */}
        <div style={{
          width: '50%',
          borderRight: `1px solid ${BLOOMBERG_GRAY}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          <div style={{
            padding: '8px 12px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
            flexShrink: 0
          }}>
            <span style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
              INSTALLED SERVERS ({servers.length})
            </span>
          </div>

          <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
            {servers.length === 0 ? (
              <div style={{
                textAlign: 'center',
                color: BLOOMBERG_GRAY,
                padding: '40px 20px',
                fontSize: '10px'
              }}>
                <div style={{ marginBottom: '8px' }}>No MCP servers installed.</div>
                <div>Install from marketplace or add a custom server.</div>
              </div>
            ) : (
              servers.map(server => {
                const healthInfo = mcpManager.getServerHealthInfo(server.id);
                return (
                  <MCPServerCard
                    key={server.id}
                    server={server}
                    onStart={() => handleStartServer(server.id)}
                    onStop={() => handleStopServer(server.id)}
                    onRemove={() => handleRemoveServer(server.id)}
                    onToggleAutoStart={(enabled) => handleToggleAutoStart(server.id, enabled)}
                    healthInfo={healthInfo}
                  />
                );
              })
            )}
          </div>
        </div>

        {/* Right Panel: Marketplace */}
        <div style={{ width: '50%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          <MCPMarketplace onInstall={handleInstallServer} />
        </div>
      </div>

      {/* Bottom Panel: Available Tools */}
      <div style={{
        height: '140px',
        borderTop: `1px solid ${BLOOMBERG_GRAY}`,
        backgroundColor: BLOOMBERG_PANEL_BG,
        overflow: 'auto',
        padding: '8px',
        flexShrink: 0
      }}>
        <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '6px' }}>
          AVAILABLE TOOLS ({tools.length})
        </div>

        {tools.length === 0 ? (
          <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', textAlign: 'center', padding: '20px' }}>
            No tools available. Start an MCP server to see its tools.
          </div>
        ) : (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
            {tools.map(tool => (
              <div
                key={tool.id}
                style={{
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  padding: '4px 6px',
                  fontSize: '9px'
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginBottom: '2px' }}>
                  <span style={{ color: BLOOMBERG_ORANGE }}>🔧</span>
                  <span style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>{tool.name}</span>
                </div>
                {tool.description && (
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px' }}>
                    {tool.description.substring(0, 60)}{tool.description.length > 60 ? '...' : ''}
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderTop: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '4px 12px',
        fontSize: '9px',
        color: BLOOMBERG_GRAY,
        flexShrink: 0
      }}>
        STATUS: {statusMessage}
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

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
};

export default MCPTab;
