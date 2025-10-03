// MCP Marketplace Component
// Browse and install curated MCP servers

import React, { useState } from 'react';
import { Search, Download } from 'lucide-react';

interface MCPMarketplaceProps {
  onInstall: (serverConfig: any) => void;
}

// Curated MCP servers from the ecosystem
const MARKETPLACE_SERVERS = [
  {
    id: 'postgres',
    name: 'PostgreSQL',
    description: 'Connect to PostgreSQL databases for queries and analysis',
    category: 'data',
    icon: '📊',
    command: 'npx',
    args: ['-y', '@modelcontextprotocol/server-postgres'],
    env: { DATABASE_URL: '' },
    tools: ['query', 'describe_table', 'list_tables', 'explain_query', 'get_indexes']
  },
  {
    id: 'fetch',
    name: 'Web Fetch',
    description: 'Fetch and extract content from any URL',
    category: 'web',
    icon: '🌐',
    command: 'npx',
    args: ['-y', '@modelcontextprotocol/server-fetch'],
    env: {},
    tools: ['fetch']
  },
  {
    id: 'brave-search',
    name: 'Brave Search',
    description: 'Web search using Brave Search API',
    category: 'web',
    icon: '🔍',
    command: 'npx',
    args: ['-y', '@modelcontextprotocol/server-brave-search'],
    env: { BRAVE_API_KEY: '' },
    tools: ['brave_web_search', 'brave_local_search']
  },
  {
    id: 'filesystem',
    name: 'FileSystem',
    description: 'Read and write files on your system',
    category: 'productivity',
    icon: '📁',
    command: 'npx',
    args: ['-y', '@modelcontextprotocol/server-filesystem'],
    env: { ALLOWED_DIRECTORIES: '' },
    tools: ['read_file', 'write_file', 'list_directory', 'search_files']
  },
  {
    id: 'sqlite',
    name: 'SQLite',
    description: 'Query and manage SQLite databases',
    category: 'data',
    icon: '💾',
    command: 'npx',
    args: ['-y', '@modelcontextprotocol/server-sqlite'],
    env: { SQLITE_DB_PATH: '' },
    tools: ['query', 'list_tables', 'describe_table']
  },
  {
    id: 'github',
    name: 'GitHub',
    description: 'Interact with GitHub repositories and issues',
    category: 'dev',
    icon: '🐙',
    command: 'npx',
    args: ['-y', '@modelcontextprotocol/server-github'],
    env: { GITHUB_TOKEN: '' },
    tools: ['create_issue', 'create_pr', 'search_repos', 'get_file_contents']
  },
  {
    id: 'puppeteer',
    name: 'Puppeteer',
    description: 'Browser automation for web scraping',
    category: 'web',
    icon: '🎭',
    command: 'npx',
    args: ['-y', '@modelcontextprotocol/server-puppeteer'],
    env: {},
    tools: ['puppeteer_navigate', 'puppeteer_screenshot', 'puppeteer_click', 'puppeteer_fill']
  },
  {
    id: 'slack',
    name: 'Slack',
    description: 'Send messages and interact with Slack',
    category: 'productivity',
    icon: '💬',
    command: 'npx',
    args: ['-y', '@modelcontextprotocol/server-slack'],
    env: { SLACK_BOT_TOKEN: '' },
    tools: ['post_message', 'list_channels', 'get_channel_history']
  }
];

const MCPMarketplace: React.FC<MCPMarketplaceProps> = ({ onInstall }) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<string>('all');

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';
  const BLOOMBERG_YELLOW = '#FFFF00';

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

  const hasRequiredEnv = (server: typeof MARKETPLACE_SERVERS[0]) => {
    return Object.keys(server.env).length > 0;
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Header */}
      <div style={{
        padding: '8px 12px',
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        flexShrink: 0
      }}>
        <span style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
          MARKETPLACE
        </span>
      </div>

      {/* Search and Filters */}
      <div style={{ padding: '8px', flexShrink: 0 }}>
        <div style={{ position: 'relative', marginBottom: '8px' }}>
          <Search
            size={12}
            color={BLOOMBERG_GRAY}
            style={{ position: 'absolute', left: '6px', top: '6px' }}
          />
          <input
            type="text"
            placeholder="Search servers..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              width: '100%',
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '4px 4px 4px 24px',
              fontSize: '10px',
              fontFamily: 'Consolas, monospace'
            }}
          />
        </div>

        {/* Categories */}
        <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
          {categories.map(cat => (
            <button
              key={cat}
              onClick={() => setSelectedCategory(cat)}
              style={{
                backgroundColor: selectedCategory === cat ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                color: selectedCategory === cat ? 'black' : BLOOMBERG_WHITE,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                padding: '3px 8px',
                fontSize: '9px',
                cursor: 'pointer',
                fontWeight: selectedCategory === cat ? 'bold' : 'normal'
              }}
            >
              {categoryLabels[cat as keyof typeof categoryLabels]}
            </button>
          ))}
        </div>
      </div>

      {/* Server List */}
      <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
        {filteredServers.length === 0 ? (
          <div style={{
            textAlign: 'center',
            color: BLOOMBERG_GRAY,
            padding: '40px 20px',
            fontSize: '10px'
          }}>
            No servers found matching your search.
          </div>
        ) : (
          filteredServers.map(server => (
            <div
              key={server.id}
              style={{
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                padding: '8px',
                marginBottom: '6px'
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'flex-start',
                marginBottom: '4px'
              }}>
                <div style={{ flex: 1 }}>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    marginBottom: '3px'
                  }}>
                    <span style={{ fontSize: '14px' }}>{server.icon}</span>
                    <span style={{
                      color: BLOOMBERG_WHITE,
                      fontSize: '11px',
                      fontWeight: 'bold'
                    }}>
                      {server.name}
                    </span>
                    <span style={{
                      color: BLOOMBERG_GRAY,
                      fontSize: '8px',
                      backgroundColor: BLOOMBERG_PANEL_BG,
                      padding: '2px 4px'
                    }}>
                      {server.tools.length} tools
                    </span>
                  </div>

                  <div style={{
                    color: BLOOMBERG_GRAY,
                    fontSize: '9px',
                    marginBottom: '4px'
                  }}>
                    {server.description}
                  </div>

                  {hasRequiredEnv(server) && (
                    <div style={{
                      color: BLOOMBERG_YELLOW,
                      fontSize: '8px',
                      marginBottom: '4px'
                    }}>
                      ⚠️ Requires: {Object.keys(server.env).join(', ')}
                    </div>
                  )}

                  <div style={{
                    color: BLOOMBERG_GRAY,
                    fontSize: '8px',
                    fontFamily: 'Consolas, monospace'
                  }}>
                    {server.command} {server.args.join(' ')}
                  </div>
                </div>
              </div>

              <button
                onClick={() => onInstall(server)}
                style={{
                  backgroundColor: BLOOMBERG_ORANGE,
                  color: 'black',
                  border: 'none',
                  padding: '4px 10px',
                  fontSize: '9px',
                  fontWeight: 'bold',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  marginTop: '6px'
                }}
              >
                <Download size={10} />
                INSTALL
              </button>
            </div>
          ))
        )}
      </div>
    </div>
  );
};

export default MCPMarketplace;
