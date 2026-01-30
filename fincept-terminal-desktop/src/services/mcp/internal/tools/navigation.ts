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
];
