import { InternalTool } from '../types';

const AVAILABLE_TABS = [
  'dashboard', 'markets', 'news', 'watchlist', 'portfolio', 'research',
  'economics', 'dbnomics', 'geopolitics', 'maritime', 'screener',
  'chat', 'docs', 'nodes', 'code', 'forum', 'marketplace',
  'profile', 'support', 'settings', 'backtesting', 'mcp', 'trading',
  'reportbuilder', 'equity-trading', 'datasources', 'ai-quant-lab',
  'notes', 'agents', 'monitoring', 'polymarket', 'derivatives', 'asia-markets', 'akshare'
];

const AVAILABLE_SCREENS = ['pricing', 'dashboard', 'profile', 'login'];

export const navigationTools: InternalTool[] = [
  {
    name: 'navigate_tab',
    description: 'Switch the terminal to a specific tab (e.g., markets, portfolio, chat, settings)',
    inputSchema: {
      type: 'object',
      properties: {
        tab: {
          type: 'string',
          description: `Tab to navigate to. Available: ${AVAILABLE_TABS.join(', ')}`,
          enum: AVAILABLE_TABS,
        },
      },
      required: ['tab'],
    },
    handler: async (args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      const tab = args.tab as string;
      if (!AVAILABLE_TABS.includes(tab)) {
        return { success: false, error: `Unknown tab: ${tab}. Available: ${AVAILABLE_TABS.join(', ')}` };
      }
      contexts.setActiveTab(tab);
      return { success: true, message: `Navigated to ${tab} tab` };
    },
  },
  {
    name: 'navigate_screen',
    description: 'Navigate to a top-level screen (pricing, dashboard, profile)',
    inputSchema: {
      type: 'object',
      properties: {
        screen: {
          type: 'string',
          description: `Screen to navigate to. Available: ${AVAILABLE_SCREENS.join(', ')}`,
          enum: AVAILABLE_SCREENS,
        },
      },
      required: ['screen'],
    },
    handler: async (args, contexts) => {
      if (!contexts.navigateToScreen) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.navigateToScreen(args.screen);
      return { success: true, message: `Navigated to ${args.screen} screen` };
    },
  },
  {
    name: 'get_active_tab',
    description: 'Get the currently active terminal tab',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      const tab = contexts.getActiveTab();
      return { success: true, data: { activeTab: tab } };
    },
  },
  {
    name: 'open_settings_section',
    description: 'Navigate to settings tab and open a specific section',
    inputSchema: {
      type: 'object',
      properties: {
        section: {
          type: 'string',
          description: 'Settings section to open: credentials, llm, data-sources, backtesting, appearance, language, storage',
          enum: ['credentials', 'llm', 'data-sources', 'backtesting', 'appearance', 'language', 'storage'],
        },
      },
      required: ['section'],
    },
    handler: async (args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('settings');
      return { success: true, message: `Opened settings > ${args.section}`, data: { section: args.section } };
    },
  },
  {
    name: 'open_news_tab',
    description: 'Navigate to the News tab to view financial news, RSS feeds, and market sentiment. Use this when user wants to see news, check headlines, or read financial articles',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('news');
      return { success: true, message: 'Navigated to News tab' };
    },
  },
  {
    name: 'open_portfolio_tab',
    description: 'Navigate to the Portfolio tab to view holdings, performance, and manage investments',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('portfolio');
      return { success: true, message: 'Navigated to Portfolio tab' };
    },
  },
  {
    name: 'open_trading_tab',
    description: 'Navigate to the Trading tab for crypto trading with connected exchanges',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('trading');
      return { success: true, message: 'Navigated to Trading tab' };
    },
  },
  {
    name: 'open_markets_tab',
    description: 'Navigate to the Markets tab to view market data, indices, and real-time quotes',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('markets');
      return { success: true, message: 'Navigated to Markets tab' };
    },
  },
  {
    name: 'open_dashboard_tab',
    description: 'Navigate to the Dashboard tab - the main overview page with customizable widgets showing portfolio summary, market overview, and key metrics',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('dashboard');
      return { success: true, message: 'Navigated to Dashboard tab' };
    },
  },
  {
    name: 'open_watchlist_tab',
    description: 'Navigate to the Watchlist tab to view and manage custom ticker watchlists with real-time price updates',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('watchlist');
      return { success: true, message: 'Navigated to Watchlist tab' };
    },
  },
  {
    name: 'open_research_tab',
    description: 'Navigate to the Equity Research tab for fundamental analysis, company research, financial statements, and stock valuation',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('research');
      return { success: true, message: 'Navigated to Equity Research tab' };
    },
  },
  {
    name: 'open_economics_tab',
    description: 'Navigate to the Economics tab to view economic indicators, central bank data, GDP, inflation, unemployment, and macroeconomic analysis',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('economics');
      return { success: true, message: 'Navigated to Economics tab' };
    },
  },
  {
    name: 'open_dbnomics_tab',
    description: 'Navigate to the DBnomics tab for comprehensive economic database access with 1000+ providers and millions of time series',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('dbnomics');
      return { success: true, message: 'Navigated to DBnomics tab' };
    },
  },
  {
    name: 'open_geopolitics_tab',
    description: 'Navigate to the Geopolitics tab for geopolitical analysis, risk assessment, global events tracking, and international relations impact on markets',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('geopolitics');
      return { success: true, message: 'Navigated to Geopolitics tab' };
    },
  },
  {
    name: 'open_maritime_tab',
    description: 'Navigate to the Maritime tab for shipping routes, port activity, maritime trade data, and supply chain tracking',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('maritime');
      return { success: true, message: 'Navigated to Maritime tab' };
    },
  },
  {
    name: 'open_screener_tab',
    description: 'Navigate to the Screener tab for stock screening, filtering tools, and custom criteria-based stock discovery',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('screener');
      return { success: true, message: 'Navigated to Screener tab' };
    },
  },
  {
    name: 'open_chat_tab',
    description: 'Navigate to the AI Chat tab for conversational AI assistant with market analysis, research, and terminal automation capabilities',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('chat');
      return { success: true, message: 'Navigated to AI Chat tab' };
    },
  },
  {
    name: 'open_docs_tab',
    description: 'Navigate to the Documentation tab for terminal guides, API reference, tutorials, and help documentation',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('docs');
      return { success: true, message: 'Navigated to Documentation tab' };
    },
  },
  {
    name: 'open_nodes_tab',
    description: 'Navigate to the Node Editor tab for visual workflow creation, automation pipelines, and no-code data processing',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('nodes');
      return { success: true, message: 'Navigated to Node Editor tab' };
    },
  },
  {
    name: 'open_code_tab',
    description: 'Navigate to the Code Editor tab for writing and executing custom trading scripts, strategies, and analytics code',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('code');
      return { success: true, message: 'Navigated to Code Editor tab' };
    },
  },
  {
    name: 'open_forum_tab',
    description: 'Navigate to the Forum tab for community discussions, trading ideas, and collaboration with other traders',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('forum');
      return { success: true, message: 'Navigated to Forum tab' };
    },
  },
  {
    name: 'open_marketplace_tab',
    description: 'Navigate to the Marketplace tab to browse and install extensions, indicators, strategies, and premium features',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('marketplace');
      return { success: true, message: 'Navigated to Marketplace tab' };
    },
  },
  {
    name: 'open_profile_tab',
    description: 'Navigate to the Profile tab for user account settings, subscription management, and personal information',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('profile');
      return { success: true, message: 'Navigated to Profile tab' };
    },
  },
  {
    name: 'open_support_tab',
    description: 'Navigate to the Support tab to create support tickets, track issues, and get help from the Fincept team',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('support');
      return { success: true, message: 'Navigated to Support tab' };
    },
  },
  {
    name: 'open_settings_tab',
    description: 'Navigate to the Settings tab for terminal configuration, API credentials, themes, language, and preferences',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('settings');
      return { success: true, message: 'Navigated to Settings tab' };
    },
  },
  {
    name: 'open_backtesting_tab',
    description: 'Navigate to the Backtesting tab for strategy backtesting, optimization, walk-forward analysis, and performance metrics',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('backtesting');
      return { success: true, message: 'Navigated to Backtesting tab' };
    },
  },
  {
    name: 'open_mcp_tab',
    description: 'Navigate to the MCP Servers tab for Model Context Protocol server management, tool discovery, and integration configuration',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('mcp');
      return { success: true, message: 'Navigated to MCP Servers tab' };
    },
  },
  {
    name: 'open_reportbuilder_tab',
    description: 'Navigate to the Report Builder tab for creating professional investment reports, presentations, and research documents',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('reportbuilder');
      return { success: true, message: 'Navigated to Report Builder tab' };
    },
  },
  {
    name: 'open_equity_trading_tab',
    description: 'Navigate to the Equity Trading tab for stock trading with broker integration, order placement, and position management',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('equity-trading');
      return { success: true, message: 'Navigated to Equity Trading tab' };
    },
  },
  {
    name: 'open_datasources_tab',
    description: 'Navigate to the Data Sources tab for managing data connections, API integrations, databases, and data feeds',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('datasources');
      return { success: true, message: 'Navigated to Data Sources tab' };
    },
  },
  {
    name: 'open_ai_quant_lab_tab',
    description: 'Navigate to the AI Quant Lab tab for machine learning, quantitative research, AI-powered trading strategies, and advanced analytics',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('ai-quant-lab');
      return { success: true, message: 'Navigated to AI Quant Lab tab' };
    },
  },
  {
    name: 'open_notes_tab',
    description: 'Navigate to the Notes tab for creating and managing trading notes, market observations, and research documentation',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('notes');
      return { success: true, message: 'Navigated to Notes tab' };
    },
  },
  {
    name: 'open_agents_tab',
    description: 'Navigate to the Agents tab for configuring AI trading agents, investor personas, and automated decision-making systems',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('agents');
      return { success: true, message: 'Navigated to Agents tab' };
    },
  },
  {
    name: 'open_monitoring_tab',
    description: 'Navigate to the Monitoring tab for real-time system monitoring, alerts, performance tracking, and diagnostics',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('monitoring');
      return { success: true, message: 'Navigated to Monitoring tab' };
    },
  },
  {
    name: 'open_polymarket_tab',
    description: 'Navigate to the Polymarket tab for prediction markets, event contracts, and decentralized forecasting',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('polymarket');
      return { success: true, message: 'Navigated to Polymarket tab' };
    },
  },
  {
    name: 'open_derivatives_tab',
    description: 'Navigate to the Derivatives tab for options, futures, swaps trading and analysis with Greeks calculation and risk metrics',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('derivatives');
      return { success: true, message: 'Navigated to Derivatives tab' };
    },
  },
  {
    name: 'open_asia_markets_tab',
    description: 'Navigate to the Asia Markets tab for Asian stock exchanges, market data, and regional trading opportunities',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('asia-markets');
      return { success: true, message: 'Navigated to Asia Markets tab' };
    },
  },
  {
    name: 'open_akshare_tab',
    description: 'Navigate to the AKShare Data tab for comprehensive Chinese financial data, A-shares, and Asian market datasets',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.setActiveTab) {
        return { success: false, error: 'Navigation context not available' };
      }
      contexts.setActiveTab('akshare');
      return { success: true, message: 'Navigated to AKShare Data tab' };
    },
  },
];
