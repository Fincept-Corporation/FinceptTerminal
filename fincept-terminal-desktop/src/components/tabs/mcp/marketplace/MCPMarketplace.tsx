// MCP Marketplace Component
// Browse and install curated MCP servers

import React, { useState } from 'react';
import { Search, Download, Zap, CheckCircle } from 'lucide-react';
import { MARKETPLACE_SERVERS, MCPServerDefinition } from './serverDefinitions';
import { MCPServerWithStats } from '../../../../services/mcpManager';
import PostgresForm from './forms/PostgresForm';
import QuestDBForm from './forms/QuestDBForm';
import KiteForm from './forms/KiteForm';
import GenericForm from './forms/GenericForm';

interface MCPMarketplaceProps {
  onInstall: (serverConfig: any) => void;
  installedServers: MCPServerWithStats[];
}

const MCPMarketplace: React.FC<MCPMarketplaceProps> = ({ onInstall, installedServers }) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [configureServer, setConfigureServer] = useState<MCPServerDefinition | null>(null);

  // Bloomberg colors
  const ORANGE = '#FFA500';
  const WHITE = '#FFFFFF';
  const GRAY = '#787878';
  const DARK_BG = '#0a0a0a';
  const PANEL_BG = '#1a1a1a';
  const BORDER = '#2d2d2d';
  const GREEN = '#10b981';
  const YELLOW = '#FFFF00';

  // Check if server is already installed
  const isServerInstalled = (serverId: string) => {
    return installedServers.some(s => s.id === serverId);
  };

  const filteredServers = MARKETPLACE_SERVERS.filter(server => {
    const matchesSearch = server.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         server.description.toLowerCase().includes(searchQuery.toLowerCase());
    const matchesCategory = selectedCategory === 'all' || server.category === selectedCategory;
    return matchesSearch && matchesCategory;
  });

  const categories = ['all', 'data', 'web', 'productivity', 'dev'];
  const categoryLabels = {
    all: 'ALL',
    data: 'DATA',
    web: 'WEB',
    productivity: 'TOOLS',
    dev: 'DEV'
  };

  const handleInstallClick = (server: MCPServerDefinition) => {
    if (server.requiresConfig) {
      setConfigureServer(server);
    } else {
      onInstall(server);
    }
  };

  const handleConfiguredInstall = (config: { args: string[]; env: Record<string, string> }) => {
    if (configureServer) {
      const serverWithConfig = {
        ...configureServer,
        args: [...configureServer.args, ...config.args],
        env: config.env
      };

      onInstall(serverWithConfig);
      setConfigureServer(null);
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Search and Filters */}
      <div style={{ marginBottom: '16px' }}>
        <div style={{ position: 'relative', marginBottom: '12px' }}>
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
            placeholder="Search marketplace..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
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

        {/* Categories */}
        <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
          {categories.map(cat => (
            <button
              key={cat}
              onClick={() => setSelectedCategory(cat)}
              style={{
                backgroundColor: selectedCategory === cat ? BORDER : 'transparent',
                color: selectedCategory === cat ? ORANGE : GRAY,
                border: `1px solid ${selectedCategory === cat ? ORANGE : BORDER}`,
                padding: '6px 12px',
                fontSize: '10px',
                cursor: 'pointer',
                borderRadius: '3px',
                textTransform: 'uppercase',
                fontWeight: selectedCategory === cat ? 'bold' : 'normal',
              }}
            >
              {categoryLabels[cat as keyof typeof categoryLabels]}
            </button>
          ))}
        </div>
      </div>

      {/* Server Grid */}
      <div style={{ flex: 1, overflow: 'auto', scrollbarWidth: 'none', msOverflowStyle: 'none' }}>
        {filteredServers.length === 0 ? (
          <div style={{
            textAlign: 'center',
            color: GRAY,
            padding: '60px 20px',
            fontSize: '11px'
          }}>
            <Zap size={48} style={{ marginBottom: '16px', opacity: 0.3, color: ORANGE }} />
            <div style={{ marginBottom: '8px' }}>No servers found matching your search.</div>
            <div>Try a different search term or category.</div>
          </div>
        ) : (
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))',
            gap: '16px',
          }}>
            {filteredServers.map(server => {
              const installed = isServerInstalled(server.id);
              return (
                <div
                  key={server.id}
                  style={{
                    backgroundColor: PANEL_BG,
                    border: `1px solid ${BORDER}`,
                    borderRadius: '6px',
                    padding: '16px',
                    cursor: installed ? 'default' : 'pointer',
                    transition: 'all 0.2s',
                    display: 'flex',
                    flexDirection: 'column',
                    minHeight: '280px',
                  }}
                  onMouseEnter={(e) => {
                    if (!installed) {
                      e.currentTarget.style.borderColor = ORANGE;
                      e.currentTarget.style.boxShadow = `0 0 12px ${ORANGE}40`;
                    }
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.borderColor = BORDER;
                    e.currentTarget.style.boxShadow = 'none';
                  }}
                >
                  {/* Server Header */}
                  <div style={{ display: 'flex', alignItems: 'flex-start', gap: '12px', marginBottom: '12px' }}>
                    <span style={{ fontSize: '32px' }}>{server.icon}</span>
                    <div style={{ flex: 1 }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                        <h3 style={{
                          color: WHITE,
                          fontSize: '13px',
                          fontWeight: 'bold',
                        }}>
                          {server.name}
                        </h3>
                        {installed && <CheckCircle size={14} color={GREEN} />}
                      </div>
                      <div style={{
                        display: 'inline-block',
                        backgroundColor: `${ORANGE}20`,
                        color: ORANGE,
                        padding: '2px 8px',
                        fontSize: '9px',
                        borderRadius: '3px',
                        textTransform: 'uppercase',
                        fontWeight: 'bold',
                      }}>
                        {server.category}
                      </div>
                    </div>
                  </div>

                  {/* Description */}
                  <p style={{
                    color: GRAY,
                    fontSize: '11px',
                    lineHeight: '1.5',
                    marginBottom: '12px',
                    flex: 1,
                  }}>
                    {server.description}
                  </p>

                  {/* Fixed height container for badges and info */}
                  <div style={{ marginBottom: '12px', minHeight: '60px' }}>
                    {/* Requires Config Badge */}
                    {server.requiresConfig && (
                      <div style={{
                        backgroundColor: `${YELLOW}20`,
                        color: YELLOW,
                        padding: '4px 8px',
                        fontSize: '9px',
                        borderRadius: '3px',
                        marginBottom: '8px',
                        display: 'inline-block',
                      }}>
                        ⚙️ Requires Configuration
                      </div>
                    )}

                    {/* Tools Count */}
                    <div style={{
                      color: GRAY,
                      fontSize: '10px',
                      marginBottom: '8px',
                    }}>
                      <span style={{ color: WHITE, fontWeight: 'bold' }}>{server.tools.length}</span> tools available
                    </div>

                    {/* Command Preview */}
                    <div style={{
                      backgroundColor: DARK_BG,
                      border: `1px solid ${BORDER}`,
                      padding: '6px 8px',
                      fontSize: '9px',
                      borderRadius: '3px',
                      color: GRAY,
                      fontFamily: 'monospace',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap',
                    }}>
                      {server.command} {server.args.join(' ')}
                    </div>
                  </div>

                  {/* Install Button - Always at bottom */}
                  {installed ? (
                    <button
                      disabled
                      style={{
                        width: '100%',
                        backgroundColor: `${GREEN}30`,
                        color: GREEN,
                        border: `1px solid ${GREEN}`,
                        padding: '8px 12px',
                        fontSize: '10px',
                        fontWeight: 'bold',
                        cursor: 'not-allowed',
                        borderRadius: '4px',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '6px',
                      }}
                    >
                      <CheckCircle size={12} />
                      INSTALLED
                    </button>
                  ) : (
                    <button
                      onClick={() => handleInstallClick(server)}
                      style={{
                        width: '100%',
                        backgroundColor: GREEN,
                        color: 'black',
                        border: 'none',
                        padding: '8px 12px',
                        fontSize: '10px',
                        fontWeight: 'bold',
                        cursor: 'pointer',
                        borderRadius: '4px',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '6px',
                      }}
                    >
                      <Download size={12} />
                      {server.requiresConfig ? 'CONFIGURE & INSTALL' : 'INSTALL NOW'}
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
                          color: GRAY,
                          fontSize: '9px',
                          textDecoration: 'none',
                        }}
                        onClick={(e) => e.stopPropagation()}
                      >
                        View Documentation →
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
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: PANEL_BG,
            border: `2px solid ${ORANGE}`,
            borderRadius: '8px',
            width: '90%',
            maxWidth: '600px',
            maxHeight: '80vh',
            overflow: 'auto',
            scrollbarWidth: 'none',
            msOverflowStyle: 'none',
          }}>
            {/* Modal Header */}
            <div style={{
              padding: '16px',
              borderBottom: `1px solid ${BORDER}`,
              display: 'flex',
              alignItems: 'center',
              gap: '12px',
            }}>
              <span style={{ fontSize: '32px' }}>{configureServer.icon}</span>
              <div>
                <h2 style={{ color: WHITE, fontSize: '16px', fontWeight: 'bold' }}>
                  Configure {configureServer.name}
                </h2>
                <p style={{ color: GRAY, fontSize: '11px' }}>{configureServer.description}</p>
              </div>
            </div>

            {/* Form */}
            <div style={{ padding: '16px' }}>
              {configureServer.id === 'postgres' ? (
                <PostgresForm
                  onSubmit={handleConfiguredInstall}
                  onCancel={() => setConfigureServer(null)}
                />
              ) : configureServer.id === 'questdb' ? (
                <QuestDBForm
                  onSubmit={handleConfiguredInstall}
                  onCancel={() => setConfigureServer(null)}
                />
              ) : configureServer.id === 'kite' ? (
                <KiteForm
                  onSubmit={handleConfiguredInstall}
                  onCancel={() => setConfigureServer(null)}
                />
              ) : (
                <GenericForm
                  server={configureServer}
                  onSubmit={handleConfiguredInstall}
                  onCancel={() => setConfigureServer(null)}
                />
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default MCPMarketplace;
