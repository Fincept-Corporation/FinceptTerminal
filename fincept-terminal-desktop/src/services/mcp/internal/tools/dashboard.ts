import { InternalTool } from '../types';

const WIDGET_TYPES = [
  'portfolio', 'quick-trade', 'market-data', 'news', 'economic-calendar',
  'geopolitics', 'forex', 'crypto', 'indices', 'maritime', 'alerts',
  'performance', 'watchlist', 'calendar', 'economic-indicators', 'polymarket', 'forum'
];

export const dashboardTools: InternalTool[] = [
  {
    name: 'add_widget',
    description: 'Add a widget to the dashboard',
    inputSchema: {
      type: 'object',
      properties: {
        widget_type: {
          type: 'string',
          description: `Widget type to add. Available: ${WIDGET_TYPES.join(', ')}`,
          enum: WIDGET_TYPES,
        },
      },
      required: ['widget_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addWidget) {
        return { success: false, error: 'Dashboard context not available' };
      }
      contexts.addWidget(args.widget_type);
      return { success: true, message: `Widget "${args.widget_type}" added to dashboard` };
    },
  },
  {
    name: 'remove_widget',
    description: 'Remove a widget from the dashboard by ID',
    inputSchema: {
      type: 'object',
      properties: {
        widget_id: { type: 'string', description: 'Widget ID to remove' },
      },
      required: ['widget_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.removeWidget) {
        return { success: false, error: 'Dashboard context not available' };
      }
      contexts.removeWidget(args.widget_id);
      return { success: true, message: `Widget removed` };
    },
  },
  {
    name: 'list_widgets',
    description: 'List all widgets currently on the dashboard',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getWidgets) {
        return { success: false, error: 'Dashboard context not available' };
      }
      const widgets = contexts.getWidgets();
      return { success: true, data: widgets };
    },
  },
  {
    name: 'configure_widget',
    description: 'Update configuration of a dashboard widget',
    inputSchema: {
      type: 'object',
      properties: {
        widget_id: { type: 'string', description: 'Widget ID to configure' },
        config: { type: 'string', description: 'JSON string of configuration to apply' },
      },
      required: ['widget_id', 'config'],
    },
    handler: async (args, contexts) => {
      if (!contexts.configureWidget) {
        return { success: false, error: 'Dashboard context not available' };
      }
      let config: any;
      try {
        config = typeof args.config === 'string' ? JSON.parse(args.config) : args.config;
      } catch {
        return { success: false, error: 'Invalid config JSON' };
      }
      contexts.configureWidget(args.widget_id, config);
      return { success: true, message: `Widget ${args.widget_id} configured` };
    },
  },
];
