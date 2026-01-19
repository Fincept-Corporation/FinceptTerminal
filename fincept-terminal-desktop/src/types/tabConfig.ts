// Tab Configuration Types
export interface TabDefinition {
  id: string;
  label: string;
  value: string;
  shortcut?: string;
  component: string;
}

export interface MenuSection {
  id: string;
  label: string;
  tabs: string[]; // Array of tab IDs
}

export interface TabConfiguration {
  headerTabs: string[]; // Tab IDs to show in header bar
  menuSections: MenuSection[];
}

export const DEFAULT_TABS: TabDefinition[] = [
  { id: 'dashboard', label: 'Dashboard', value: 'dashboard', shortcut: 'F1', component: 'DashboardTab' },
  { id: 'markets', label: 'Markets', value: 'markets', shortcut: 'F2', component: 'MarketsTab' },
  { id: 'news', label: 'News', value: 'news', shortcut: 'F3', component: 'NewsTab' },
  { id: 'portfolio', label: 'Portfolio', value: 'portfolio', shortcut: 'F4', component: 'PortfolioTab' },
  { id: 'backtesting', label: 'Backtesting', value: 'backtesting', shortcut: 'F5', component: 'BacktestingTab' },
  { id: 'watchlist', label: 'Watchlist', value: 'watchlist', shortcut: 'F6', component: 'WatchlistTab' },
  { id: 'research', label: 'Research', value: 'research', shortcut: 'F7', component: 'EquityResearchTab' },
  { id: 'screener', label: 'Screener', value: 'screener', shortcut: 'F8', component: 'ScreenerTab' },
  { id: 'trading', label: 'Trading', value: 'trading', shortcut: 'F9', component: 'TradingTab' },
  { id: 'chat', label: 'AI Chat', value: 'chat', shortcut: 'F10', component: 'ChatTab' },
  { id: 'notes', label: 'Notes', value: 'notes', shortcut: 'F11', component: 'NotesTab' },
  { id: 'settings', label: 'Settings', value: 'settings', component: 'SettingsTab' },
  { id: 'profile', label: 'Profile', value: 'profile', shortcut: 'F12', component: 'ProfileTab' },
  { id: 'polygon', label: 'Polygon Data', value: 'polygon', component: 'PolygonEqTab' },
  { id: 'economics', label: 'Economics', value: 'economics', component: 'EconomicsTab' },
  { id: 'dbnomics', label: 'DBnomics', value: 'dbnomics', component: 'DBnomicsTab' },
  { id: 'akshare', label: 'AKShare Data', value: 'akshare', component: 'AkShareDataTab' },
  { id: 'geopolitics', label: 'Geopolitics', value: 'geopolitics', component: 'GeopoliticsTab' },
  { id: 'maritime', label: 'Maritime', value: 'maritime', component: 'MaritimeTab' },
  { id: 'fyers', label: 'Fyers', value: 'fyers', component: 'FyersTab' },
  { id: 'mcp', label: 'MCP Servers', value: 'mcp', component: 'MCPTab' },
  { id: 'nodes', label: 'Node Editor', value: 'nodes', component: 'NodeEditorTab' },
  { id: 'code', label: 'Code Editor', value: 'code', component: 'CodeEditorTab' },
  { id: 'datasources', label: 'Data Sources', value: 'datasources', component: 'DataSourcesTab' },
  { id: 'datamapping', label: 'Data Mapping', value: 'datamapping', component: 'DataMappingTab' },
  { id: 'reportbuilder', label: 'Report Builder', value: 'reportbuilder', component: 'ReportBuilderTab' },
  { id: 'contexts', label: 'Contexts', value: 'contexts', component: 'RecordedContextsManager' },
  { id: 'forum', label: 'Forum', value: 'forum', component: 'ForumTab' },
  { id: 'marketplace', label: 'Marketplace', value: 'marketplace', component: 'MarketplaceTab' },
  { id: 'docs', label: 'Documentation', value: 'docs', component: 'DocsTab' },
  { id: 'support', label: 'Support', value: 'support', component: 'SupportTicketTab' },
  { id: 'agents', label: 'Agents', value: 'agents', component: 'AgentConfigTab' },
  { id: 'alpha-arena', label: 'Alpha Arena', value: 'alpha-arena', component: 'AlphaArenaTab' },
  { id: 'ai-quant-lab', label: 'AI Quant Lab', value: 'ai-quant-lab', shortcut: 'Ctrl+Q', component: 'AIQuantLabTab' }
];

export const DEFAULT_TAB_CONFIG: TabConfiguration = {
  headerTabs: ['dashboard', 'markets', 'news', 'portfolio', 'backtesting', 'watchlist', 'research', 'screener', 'trading', 'chat', 'notes', 'agents', 'settings', 'profile'],
  menuSections: [
    { id: 'markets-menu', label: 'Markets', tabs: ['polygon', 'economics', 'dbnomics', 'akshare'] },
    { id: 'research-menu', label: 'Research', tabs: ['geopolitics', 'maritime'] },
    { id: 'trading-menu', label: 'Trading', tabs: ['fyers', 'alpha-arena', 'ai-quant-lab'] },
    { id: 'tools-menu', label: 'Tools', tabs: ['mcp', 'nodes', 'code', 'datasources', 'datamapping', 'reportbuilder', 'contexts'] },
    { id: 'community-menu', label: 'Community', tabs: ['forum', 'marketplace', 'docs'] },
    { id: 'help-menu', label: 'Help', tabs: ['support'] }
  ]
};
