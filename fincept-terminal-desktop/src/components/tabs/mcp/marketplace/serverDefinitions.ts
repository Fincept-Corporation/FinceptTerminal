// MCP Server Definitions
// Add new MCP servers here following the structure below

export interface MCPServerDefinition {
  id: string;
  name: string;
  description: string;
  category: 'data' | 'web' | 'productivity' | 'dev' | 'custom';
  icon: string;
  command: string;
  args: string[];
  env?: Record<string, string>;
  requiresArg?: string; // If the server needs a CLI argument instead of env var
  requiresConfig: boolean; // Whether it needs a custom configuration form
  tools: string[]; // List of available tools (for display purposes)
  documentation?: string; // Link to MCP server documentation
}

/**
 * Curated MCP servers available in the marketplace
 * Only PostgreSQL and QuestDB are currently implemented
 */
export const MARKETPLACE_SERVERS: MCPServerDefinition[] = [
  {
    id: 'postgres',
    name: 'PostgreSQL',
    description: 'Connect to PostgreSQL databases for SQL queries and analysis',
    category: 'data',
    icon: '🐘',
    command: 'bunx',
    args: ['-y', '@modelcontextprotocol/server-postgres'],
    requiresArg: 'DATABASE_URL', // Connection string passed as CLI argument
    requiresConfig: true, // Uses PostgresForm.tsx
    tools: ['query', 'describe_table', 'list_tables', 'explain_query', 'get_indexes'],
    documentation: 'https://github.com/modelcontextprotocol/servers/tree/main/src/postgres'
  },
  {
    id: 'questdb',
    name: 'QuestDB',
    description: 'Connect to QuestDB time-series databases for fast analytics',
    category: 'data',
    icon: '⚡',
    command: 'bunx',
    args: ['-y', '@questdb/mcp-server'],
    requiresArg: 'DATABASE_URL', // Connection string passed as CLI argument
    requiresConfig: true, // Uses QuestDBForm.tsx
    tools: ['query', 'list_tables', 'describe_table', 'insert_data', 'get_metrics'],
    documentation: 'https://questdb.io/docs/third-party-tools/mcp/'
  },
  {
    id: 'wikipedia',
    name: 'Wikipedia',
    description: 'Search and retrieve Wikipedia articles with summaries and key facts',
    category: 'web',
    icon: '📚',
    command: 'bunx',
    args: ['-y', 'wikipedia-mcp'],
    env: {},
    requiresConfig: false,
    tools: ['search_wikipedia', 'get_article', 'get_summary', 'get_sections', 'get_links', 'get_coordinates', 'get_related_topics', 'summarize_article_for_query', 'summarize_article_section', 'extract_key_facts'],
    documentation: 'https://github.com/Rudra-ravi/wikipedia-mcp'
  },
  {
    id: 'kite',
    name: 'Zerodha Kite',
    description: 'Indian stock trading, portfolio management, order execution, and real-time market data via Kite Connect API',
    category: 'data',
    icon: '📈',
    command: 'bunx',
    args: ['-y', 'mcp-remote', 'https://mcp.kite.trade/mcp'],
    env: {
      KITE_API_KEY: '',
      KITE_API_SECRET: ''
    },
    requiresConfig: true, // Uses KiteForm.tsx
    tools: [
      'get_profile',
      'get_holdings',
      'get_positions',
      'get_orders',
      'place_order',
      'modify_order',
      'cancel_order',
      'get_gtt_orders',
      'place_gtt_order',
      'modify_gtt_order',
      'delete_gtt_order',
      'get_quote',
      'get_ohlc',
      'get_ltp',
      'get_instruments'
    ],
    documentation: 'https://github.com/zerodha/kite-mcp-server'
  }
];

/**
 * Get server definition by ID
 */
export function getServerById(id: string): MCPServerDefinition | undefined {
  return MARKETPLACE_SERVERS.find(server => server.id === id);
}

/**
 * Filter servers by category
 */
export function getServersByCategory(category: string): MCPServerDefinition[] {
  if (category === 'all') return MARKETPLACE_SERVERS;
  return MARKETPLACE_SERVERS.filter(server => server.category === category);
}

/**
 * Search servers by name or description
 */
export function searchServers(query: string): MCPServerDefinition[] {
  const lowerQuery = query.toLowerCase();
  return MARKETPLACE_SERVERS.filter(
    server =>
      server.name.toLowerCase().includes(lowerQuery) ||
      server.description.toLowerCase().includes(lowerQuery)
  );
}
