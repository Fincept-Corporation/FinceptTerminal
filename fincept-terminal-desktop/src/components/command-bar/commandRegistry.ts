// File: src/components/command-bar/commandRegistry.ts
// Command registry for Fincept Terminal - Fincept-style command system

export interface Command {
  id: string;
  name: string;
  description: string;
  aliases: string[];
  category: 'navigation' | 'action' | 'data' | 'tool' | 'system';
  action: string | (() => void);
  shortcut?: string;
  keywords?: string[];
}

export const COMMANDS: Command[] = [
  // Navigation Commands - Primary Tabs (F-keys)
  {
    id: 'dash',
    name: 'Dashboard',
    description: 'Navigate to Dashboard overview',
    aliases: ['dash', 'home', 'main', 'overview'],
    category: 'navigation',
    action: 'dashboard',
    shortcut: 'F1',
    keywords: ['dashboard', 'home', 'main', 'overview', 'summary']
  },
  {
    id: 'mkts',
    name: 'Markets',
    description: 'View live market data',
    aliases: ['mkts', 'markets', 'market', 'live'],
    category: 'navigation',
    action: 'markets',
    shortcut: 'F2',
    keywords: ['markets', 'stocks', 'quotes', 'prices', 'live', 'real-time']
  },
  {
    id: 'news',
    name: 'News',
    description: 'Financial news feed',
    aliases: ['news', 'headlines', 'feed'],
    category: 'navigation',
    action: 'news',
    shortcut: 'F3',
    keywords: ['news', 'headlines', 'articles', 'feed', 'financial news']
  },
  {
    id: 'port',
    name: 'Portfolio',
    description: 'Portfolio management',
    aliases: ['port', 'portfolio', 'pf', 'holdings'],
    category: 'navigation',
    action: 'portfolio',
    shortcut: 'F4',
    keywords: ['portfolio', 'holdings', 'positions', 'assets']
  },
  {
    id: 'bktest',
    name: 'Backtesting',
    description: 'Strategy backtesting',
    aliases: ['bktest', 'backtest', 'bt', 'test'],
    category: 'navigation',
    action: 'backtesting',
    shortcut: 'F5',
    keywords: ['backtest', 'strategy', 'testing', 'historical']
  },
  {
    id: 'watch',
    name: 'Watchlist',
    description: 'Manage watchlists',
    aliases: ['watch', 'watchlist', 'wl', 'list'],
    category: 'navigation',
    action: 'watchlist',
    shortcut: 'F6',
    keywords: ['watchlist', 'favorites', 'monitor', 'track']
  },
  {
    id: 'rsrch',
    name: 'Equity Research',
    description: 'Equity research tools',
    aliases: ['rsrch', 'research', 'equity', 'analysis'],
    category: 'navigation',
    action: 'research',
    shortcut: 'F7',
    keywords: ['research', 'equity', 'analysis', 'fundamental', 'valuation']
  },
  {
    id: 'scrn',
    name: 'Screener',
    description: 'Stock screener',
    aliases: ['scrn', 'screener', 'filter', 'scan'],
    category: 'navigation',
    action: 'screener',
    shortcut: 'F8',
    keywords: ['screener', 'filter', 'scan', 'search', 'criteria']
  },
  {
    id: 'trade',
    name: 'Crypto Trading',
    description: 'Cryptocurrency trading',
    aliases: ['trade', 'trading', 'crypto', 'kraken'],
    category: 'navigation',
    action: 'trading',
    shortcut: 'F9',
    keywords: ['trading', 'crypto', 'cryptocurrency', 'kraken', 'exchange']
  },
  {
    id: 'ai',
    name: 'AI Chat',
    description: 'AI Assistant chat',
    aliases: ['ai', 'chat', 'assistant', 'bot'],
    category: 'navigation',
    action: 'chat',
    shortcut: 'F10',
    keywords: ['ai', 'chat', 'assistant', 'bot', 'help']
  },
  {
    id: 'notes',
    name: 'Notes',
    description: 'Notes and reports',
    aliases: ['notes', 'note', 'reports', 'docs'],
    category: 'navigation',
    action: 'notes',
    shortcut: 'F11',
    keywords: ['notes', 'reports', 'documents', 'save', 'write']
  },
  {
    id: 'prof',
    name: 'Profile',
    description: 'User profile settings',
    aliases: ['prof', 'profile', 'account'],
    category: 'navigation',
    action: 'profile',
    shortcut: 'F12',
    keywords: ['profile', 'account', 'user', 'preferences']
  },

  // Additional Navigation Commands
  {
    id: 'eqtrade',
    name: 'Equity Trading',
    description: 'Stock trading interface',
    aliases: ['eqtrade', 'stocks', 'equities'],
    category: 'navigation',
    action: 'equity-trading',
    keywords: ['equity', 'stocks', 'trading', 'shares']
  },
  {
    id: 'poly',
    name: 'Polymarket',
    description: 'Prediction markets',
    aliases: ['poly', 'polymarket', 'prediction'],
    category: 'navigation',
    action: 'polymarket',
    keywords: ['polymarket', 'prediction', 'markets', 'betting']
  },
  {
    id: 'deriv',
    name: 'Derivatives',
    description: 'Options and derivatives',
    aliases: ['deriv', 'derivatives', 'options'],
    category: 'navigation',
    action: 'derivatives',
    keywords: ['derivatives', 'options', 'futures', 'pricing']
  },
  {
    id: 'econ',
    name: 'Economics',
    description: 'Economic indicators',
    aliases: ['econ', 'economics', 'indicators'],
    category: 'navigation',
    action: 'economics',
    keywords: ['economics', 'indicators', 'macro', 'economic data']
  },
  {
    id: 'dbn',
    name: 'DBnomics',
    description: 'Economic database',
    aliases: ['dbn', 'dbnomics', 'database'],
    category: 'navigation',
    action: 'dbnomics',
    keywords: ['dbnomics', 'database', 'economic', 'data']
  },
  {
    id: 'geo',
    name: 'Geopolitics',
    description: 'Geopolitical analysis',
    aliases: ['geo', 'geopolitics', 'politics'],
    category: 'navigation',
    action: 'geopolitics',
    keywords: ['geopolitics', 'politics', 'global', 'analysis']
  },
  {
    id: 'marine',
    name: 'Maritime',
    description: 'Maritime intelligence',
    aliases: ['marine', 'maritime', 'shipping'],
    category: 'navigation',
    action: 'maritime',
    keywords: ['maritime', 'shipping', 'vessels', 'ports']
  },
  {
    id: 'relmap',
    name: 'Relationship Map',
    description: 'Entity relationship mapping',
    aliases: ['relmap', 'relationships', 'map'],
    category: 'navigation',
    action: 'relationship-map',
    keywords: ['relationship', 'map', 'network', 'connections']
  },
  {
    id: '3dviz',
    name: 'Surface Analytics',
    description: 'Options volatility surface, correlation matrix, yield curve, PCA analysis',
    aliases: ['surface', 'volsurface', 'iv', 'vol', 'volatility', 'correlation', '3d', '3dviz'],
    category: 'navigation',
    action: '3d-viz',
    keywords: ['surface', 'volatility', 'options', 'iv', 'correlation', 'yield', 'pca', 'databento', '3d']
  },

  // Tools
  {
    id: 'quantlab',
    name: 'AI Quant Lab',
    description: 'Quantitative analysis lab',
    aliases: ['quantlab', 'quant', 'lab'],
    category: 'tool',
    action: 'ai-quant-lab',
    keywords: ['quant', 'quantitative', 'lab', 'analysis']
  },
  {
    id: 'agents',
    name: 'Agent Config',
    description: 'Configure AI agents',
    aliases: ['agents', 'config', 'agent'],
    category: 'tool',
    action: 'agents',
    keywords: ['agents', 'ai', 'configuration', 'setup']
  },
  {
    id: 'mcp',
    name: 'MCP Servers',
    description: 'Model Context Protocol',
    aliases: ['mcp', 'servers', 'protocol'],
    category: 'tool',
    action: 'mcp',
    keywords: ['mcp', 'servers', 'protocol', 'model']
  },
  {
    id: 'excel',
    name: 'Excel Workbook',
    description: 'Excel integration',
    aliases: ['excel', 'spreadsheet', 'xls'],
    category: 'tool',
    action: 'excel',
    keywords: ['excel', 'spreadsheet', 'workbook', 'xls']
  },
  {
    id: 'nodes',
    name: 'Node Editor',
    description: 'Visual workflow editor',
    aliases: ['nodes', 'workflow', 'editor'],
    category: 'tool',
    action: 'nodes',
    keywords: ['nodes', 'workflow', 'visual', 'editor']
  },
  {
    id: 'code',
    name: 'Code Editor',
    description: 'Code development',
    aliases: ['code', 'editor', 'dev'],
    category: 'tool',
    action: 'code',
    keywords: ['code', 'editor', 'development', 'programming']
  },
  {
    id: 'datasrc',
    name: 'Data Sources',
    description: 'Manage data sources',
    aliases: ['datasrc', 'datasources', 'sources'],
    category: 'data',
    action: 'datasources',
    keywords: ['data', 'sources', 'connections', 'api']
  },
  {
    id: 'datamap',
    name: 'Data Mapping',
    description: 'Data transformation',
    aliases: ['datamap', 'mapping', 'transform'],
    category: 'data',
    action: 'datamapping',
    keywords: ['data', 'mapping', 'transformation', 'etl']
  },
  {
    id: 'report',
    name: 'Report Builder',
    description: 'Create reports',
    aliases: ['report', 'builder', 'reports'],
    category: 'tool',
    action: 'reportbuilder',
    keywords: ['report', 'builder', 'document', 'generate']
  },
  {
    id: 'contexts',
    name: 'Recorded Contexts',
    description: 'View recorded contexts',
    aliases: ['contexts', 'recorded', 'history'],
    category: 'data',
    action: 'contexts',
    keywords: ['contexts', 'recorded', 'history', 'sessions']
  },
  {
    id: 'monitor',
    name: 'Monitoring',
    description: 'System monitoring',
    aliases: ['monitor', 'monitoring', 'system'],
    category: 'system',
    action: 'monitoring',
    keywords: ['monitoring', 'system', 'performance', 'status']
  },

  // Community
  {
    id: 'forum',
    name: 'Forum',
    description: 'Community forum',
    aliases: ['forum', 'community', 'discuss'],
    category: 'navigation',
    action: 'forum',
    keywords: ['forum', 'community', 'discussion', 'posts']
  },
  {
    id: 'market',
    name: 'Marketplace',
    description: 'Extension marketplace',
    aliases: ['market', 'marketplace', 'store'],
    category: 'navigation',
    action: 'marketplace',
    keywords: ['marketplace', 'store', 'extensions', 'plugins']
  },
  {
    id: 'docs',
    name: 'Documentation',
    description: 'User documentation',
    aliases: ['docs', 'help', 'manual'],
    category: 'navigation',
    action: 'docs',
    keywords: ['documentation', 'help', 'manual', 'guide']
  },
  {
    id: 'support',
    name: 'Support',
    description: 'Support tickets',
    aliases: ['support', 'ticket', 'tickets'],
    category: 'system',
    action: 'support',
    keywords: ['support', 'ticket', 'help', 'assistance']
  },
  {
    id: 'settings',
    name: 'Settings',
    description: 'Application settings',
    aliases: ['settings', 'preferences', 'config'],
    category: 'system',
    action: 'settings',
    keywords: ['settings', 'preferences', 'configuration', 'options']
  },

  // Missing Tabs - Added
  {
    id: 'analytics',
    name: 'Analytics',
    description: 'Financial analytics and models',
    aliases: ['analytics', 'analysis', 'models'],
    category: 'data',
    action: 'analytics',
    keywords: ['analytics', 'analysis', 'financial', 'models', 'statistics']
  },
  {
    id: 'akshare',
    name: 'AKShare Data',
    description: 'Chinese financial data (AKShare)',
    aliases: ['akshare', 'aks', 'chinese'],
    category: 'data',
    action: 'akshare',
    keywords: ['akshare', 'chinese', 'data', 'china', 'financial']
  },
  {
    id: 'asia',
    name: 'Asia Markets',
    description: 'Asian market data',
    aliases: ['asia', 'asian', 'apac'],
    category: 'navigation',
    action: 'asia-markets',
    keywords: ['asia', 'asian', 'markets', 'apac', 'regional']
  },
  {
    id: 'tradeviz',
    name: 'Trade Visualization',
    description: 'Trade flow visualization',
    aliases: ['tradeviz', 'tradegraph', 'flow'],
    category: 'tool',
    action: 'trade-viz',
    keywords: ['trade', 'visualization', 'flow', 'graph', 'network']
  },
  {
    id: 'about',
    name: 'About',
    description: 'About Fincept Terminal',
    aliases: ['about', 'info', 'version'],
    category: 'system',
    action: 'about',
    keywords: ['about', 'information', 'version', 'credits']
  },
  {
    id: 'alphaarena',
    name: 'Alpha Arena',
    description: 'Trading competition platform',
    aliases: ['alphaarena', 'alpha', 'arena', 'competition'],
    category: 'navigation',
    action: 'alpha-arena',
    keywords: ['alpha', 'arena', 'competition', 'trading', 'leaderboard']
  }
];

// Search and filter commands
export function searchCommands(query: string): Command[] {
  if (!query || query.trim().length === 0) {
    return COMMANDS;
  }

  const lowerQuery = query.toLowerCase().trim();

  return COMMANDS.filter(cmd => {
    // Check if query matches command name
    if (cmd.name.toLowerCase().includes(lowerQuery)) return true;

    // Check if query matches any alias
    if (cmd.aliases.some(alias => alias.toLowerCase().includes(lowerQuery))) return true;

    // Check if query matches description
    if (cmd.description.toLowerCase().includes(lowerQuery)) return true;

    // Check if query matches any keyword
    if (cmd.keywords && cmd.keywords.some(kw => kw.toLowerCase().includes(lowerQuery))) return true;

    return false;
  }).sort((a, b) => {
    // Prioritize exact alias matches
    const aExactAlias = a.aliases.some(alias => alias.toLowerCase() === lowerQuery);
    const bExactAlias = b.aliases.some(alias => alias.toLowerCase() === lowerQuery);
    if (aExactAlias && !bExactAlias) return -1;
    if (!aExactAlias && bExactAlias) return 1;

    // Prioritize name matches
    const aNameMatch = a.name.toLowerCase().includes(lowerQuery);
    const bNameMatch = b.name.toLowerCase().includes(lowerQuery);
    if (aNameMatch && !bNameMatch) return -1;
    if (!aNameMatch && bNameMatch) return 1;

    // Otherwise keep original order
    return 0;
  });
}

// Get command by ID or alias
export function getCommand(identifier: string): Command | undefined {
  const lowerIdentifier = identifier.toLowerCase().trim();
  return COMMANDS.find(cmd =>
    cmd.id === lowerIdentifier ||
    cmd.aliases.some(alias => alias.toLowerCase() === lowerIdentifier)
  );
}

// Get all commands by category
export function getCommandsByCategory(category: Command['category']): Command[] {
  return COMMANDS.filter(cmd => cmd.category === category);
}
