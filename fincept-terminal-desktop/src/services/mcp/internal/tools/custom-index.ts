// Custom Index MCP Tools - AI-callable tools for the Custom Index feature

import { InternalTool } from '../types';

export const customIndexTools: InternalTool[] = [

  // ==================== INDEX CRUD ====================
  {
    name: 'create_custom_index',
    description: 'Create a new custom index (like a personal S&P 500 or Dow Jones). Supports 10 calculation methods: equal_weighted, price_weighted, market_cap_weighted, float_adjusted, fundamental_weighted, modified_market_cap, factor_weighted, risk_parity, geometric_mean, capped_weighted.',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Index name (e.g. "My Tech Index")' },
        description: { type: 'string', description: 'Optional description' },
        calculation_method: {
          type: 'string',
          description: 'Index calculation method',
          enum: ['price_weighted', 'market_cap_weighted', 'equal_weighted', 'float_adjusted', 'fundamental_weighted', 'modified_market_cap', 'factor_weighted', 'risk_parity', 'geometric_mean', 'capped_weighted'],
          default: 'equal_weighted',
        },
        base_value: { type: 'number', description: 'Starting index value (e.g. 100 or 1000)', default: 100 },
        currency: { type: 'string', description: 'Currency code (USD, EUR, INR, etc.)', default: 'USD' },
        cap_weight: { type: 'number', description: 'Max weight % per constituent for capped_weighted method (0 = no cap)', default: 0 },
        historical_start_date: { type: 'string', description: 'Optional YYYY-MM-DD to backfill historical snapshots from that date' },
        portfolio_id: { type: 'string', description: 'Optional portfolio ID to link this index to' },
      },
      required: ['name'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createCustomIndex) {
        return { success: false, error: 'Custom index context not available' };
      }
      const index = await contexts.createCustomIndex({
        name: args.name,
        description: args.description,
        calculation_method: args.calculation_method || 'equal_weighted',
        base_value: args.base_value ?? 100,
        currency: args.currency || 'USD',
        cap_weight: args.cap_weight ?? 0,
        historical_start_date: args.historical_start_date,
        portfolio_id: args.portfolio_id,
      });
      return {
        success: true,
        data: index,
        message: `Index "${index.name}" created (ID: ${index.id}) using ${index.calculation_method} method`,
      };
    },
  },

  {
    name: 'list_custom_indices',
    description: 'List all custom indices with their current values, day change, and constituent count.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getAllCustomIndices) {
        return { success: false, error: 'Custom index context not available' };
      }
      const indices = await contexts.getAllCustomIndices();
      if (indices.length === 0) {
        return { success: true, data: [], message: 'No custom indices found. Create one with create_custom_index.' };
      }
      let message = `Found ${indices.length} index(es):\n`;
      indices.forEach((idx: any) => {
        const chg = idx.current_value - idx.previous_close;
        const chgPct = idx.previous_close > 0 ? (chg / idx.previous_close) * 100 : 0;
        message += `• ${idx.name} (${idx.calculation_method}): ${idx.current_value?.toFixed(2)} (${chgPct >= 0 ? '+' : ''}${chgPct.toFixed(2)}%)\n`;
        message += `  ID: ${idx.id} | Currency: ${idx.currency}\n`;
      });
      return { success: true, data: indices, count: indices.length, message };
    },
  },

  {
    name: 'get_custom_index_summary',
    description: 'Get full details of a custom index including all constituents, their weights, contributions, and current values.',
    inputSchema: {
      type: 'object',
      properties: {
        index_id: { type: 'string', description: 'Custom index ID' },
      },
      required: ['index_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getCustomIndexSummary) {
        return { success: false, error: 'Custom index context not available' };
      }
      const summary = await contexts.getCustomIndexSummary(args.index_id);
      let message = `=== ${summary.index.name} ===\n`;
      message += `Method: ${summary.index.calculation_method} | Currency: ${summary.index.currency}\n`;
      message += `Value: ${summary.index_value?.toFixed(2)} | Day Change: ${summary.day_change >= 0 ? '+' : ''}${summary.day_change?.toFixed(2)} (${summary.day_change_percent?.toFixed(2)}%)\n`;
      message += `Total Market Value: $${summary.total_market_value?.toFixed(2)}\n`;
      message += `Constituents: ${summary.constituents.length}\n\n`;
      summary.constituents.forEach((c: any) => {
        message += `  ${c.symbol}: weight ${c.effective_weight?.toFixed(2)}% | price $${c.current_price?.toFixed(2)} | day ${c.day_change_percent >= 0 ? '+' : ''}${c.day_change_percent?.toFixed(2)}% | contribution ${c.contribution >= 0 ? '+' : ''}${c.contribution?.toFixed(4)}\n`;
      });
      return { success: true, data: summary, message };
    },
  },

  {
    name: 'delete_custom_index',
    description: 'Delete a custom index and all its snapshots.',
    inputSchema: {
      type: 'object',
      properties: {
        index_id: { type: 'string', description: 'Custom index ID to delete' },
      },
      required: ['index_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.deleteCustomIndex) {
        return { success: false, error: 'Custom index context not available' };
      }
      await contexts.deleteCustomIndex(args.index_id);
      return { success: true, message: `Custom index ${args.index_id} deleted` };
    },
  },

  // ==================== CONSTITUENTS ====================
  {
    name: 'add_index_constituent',
    description: 'Add a stock/symbol to a custom index.',
    inputSchema: {
      type: 'object',
      properties: {
        index_id: { type: 'string', description: 'Custom index ID' },
        symbol: { type: 'string', description: 'Stock symbol to add (e.g. AAPL, MSFT, RELIANCE.NS)' },
        weight: { type: 'number', description: 'Manual weight % (0-100). For equal_weighted this is ignored.' },
        shares: { type: 'number', description: 'Shares outstanding for price_weighted method' },
        market_cap: { type: 'number', description: 'Market cap for market_cap_weighted method' },
      },
      required: ['index_id', 'symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addIndexConstituent) {
        return { success: false, error: 'Custom index context not available' };
      }
      const constituent = await contexts.addIndexConstituent(args.index_id, {
        symbol: args.symbol.toUpperCase(),
        weight: args.weight,
        shares: args.shares,
        marketCap: args.market_cap,
      });
      return {
        success: true,
        data: constituent,
        message: `Added ${args.symbol.toUpperCase()} to index`,
      };
    },
  },

  {
    name: 'remove_index_constituent',
    description: 'Remove a stock/symbol from a custom index.',
    inputSchema: {
      type: 'object',
      properties: {
        index_id: { type: 'string', description: 'Custom index ID' },
        symbol: { type: 'string', description: 'Stock symbol to remove' },
      },
      required: ['index_id', 'symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.removeIndexConstituent) {
        return { success: false, error: 'Custom index context not available' };
      }
      await contexts.removeIndexConstituent(args.index_id, args.symbol.toUpperCase());
      return { success: true, message: `Removed ${args.symbol.toUpperCase()} from index` };
    },
  },

  {
    name: 'get_index_constituents',
    description: 'List all stocks/symbols in a custom index with their weights and current prices.',
    inputSchema: {
      type: 'object',
      properties: {
        index_id: { type: 'string', description: 'Custom index ID' },
      },
      required: ['index_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getIndexConstituents) {
        return { success: false, error: 'Custom index context not available' };
      }
      const constituents = await contexts.getIndexConstituents(args.index_id);
      let message = `${constituents.length} constituent(s):\n`;
      constituents.forEach((c: any) => {
        message += `  ${c.symbol}: $${c.current_price?.toFixed(2)} | weight ${c.effective_weight?.toFixed(2)}%\n`;
      });
      return { success: true, data: constituents, count: constituents.length, message };
    },
  },

  // ==================== SNAPSHOTS & HISTORY ====================
  {
    name: 'save_index_snapshot',
    description: 'Save a daily snapshot of the index value. Used to build historical performance charts.',
    inputSchema: {
      type: 'object',
      properties: {
        index_id: { type: 'string', description: 'Custom index ID' },
      },
      required: ['index_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.saveIndexSnapshot) {
        return { success: false, error: 'Custom index context not available' };
      }
      const snapshot = await contexts.saveIndexSnapshot(args.index_id);
      return {
        success: true,
        data: snapshot,
        message: `Snapshot saved: index value ${snapshot.index_value?.toFixed(2)} at ${snapshot.snapshot_date}`,
      };
    },
  },

  {
    name: 'get_index_history',
    description: 'Get historical value snapshots for a custom index to see performance over time.',
    inputSchema: {
      type: 'object',
      properties: {
        index_id: { type: 'string', description: 'Custom index ID' },
        limit: { type: 'number', description: 'Number of snapshots to return (default 30)', default: 30 },
      },
      required: ['index_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getIndexSnapshots) {
        return { success: false, error: 'Custom index context not available' };
      }
      const snapshots = await contexts.getIndexSnapshots(args.index_id, args.limit || 30);
      return {
        success: true,
        data: snapshots,
        count: snapshots.length,
        message: `Retrieved ${snapshots.length} historical snapshot(s)`,
      };
    },
  },

  // ==================== UTILITIES ====================
  {
    name: 'create_index_from_portfolio',
    description: 'Create a custom index directly from an existing portfolio — each holding becomes a constituent.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID to build the index from' },
        index_name: { type: 'string', description: 'Name for the new index' },
        calculation_method: {
          type: 'string',
          description: 'Index calculation method',
          enum: ['price_weighted', 'market_cap_weighted', 'equal_weighted', 'float_adjusted', 'fundamental_weighted', 'modified_market_cap', 'factor_weighted', 'risk_parity', 'geometric_mean', 'capped_weighted'],
          default: 'equal_weighted',
        },
        base_value: { type: 'number', description: 'Starting index value', default: 100 },
      },
      required: ['portfolio_id', 'index_name'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createIndexFromPortfolio) {
        return { success: false, error: 'Custom index context not available' };
      }
      const index = await contexts.createIndexFromPortfolio(
        args.portfolio_id,
        args.index_name,
        args.calculation_method || 'equal_weighted',
        args.base_value ?? 100,
      );
      return {
        success: true,
        data: index,
        message: `Index "${index.name}" created from portfolio with ${index.calculation_method} method (ID: ${index.id})`,
      };
    },
  },

  {
    name: 'rebase_custom_index',
    description: 'Reset the base value of a custom index (e.g. rebase to 100 or 1000 from today).',
    inputSchema: {
      type: 'object',
      properties: {
        index_id: { type: 'string', description: 'Custom index ID' },
        new_base_value: { type: 'number', description: 'New base value (e.g. 100 or 1000)' },
      },
      required: ['index_id', 'new_base_value'],
    },
    handler: async (args, contexts) => {
      if (!contexts.rebaseCustomIndex) {
        return { success: false, error: 'Custom index context not available' };
      }
      await contexts.rebaseCustomIndex(args.index_id, args.new_base_value);
      return { success: true, message: `Index ${args.index_id} rebased to ${args.new_base_value}` };
    },
  },
];
