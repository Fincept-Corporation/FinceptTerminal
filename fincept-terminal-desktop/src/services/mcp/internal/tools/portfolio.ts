import { InternalTool } from '../types';

export const portfolioTools: InternalTool[] = [
  {
    name: 'create_portfolio',
    description: 'Create a new portfolio for tracking investments',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Portfolio name' },
        currency: { type: 'string', description: 'Base currency (e.g., USD, EUR, INR)' },
        description: { type: 'string', description: 'Portfolio description' },
      },
      required: ['name'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createPortfolio) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const result = await contexts.createPortfolio(args);
      return { success: true, data: result, message: `Portfolio "${args.name}" created` };
    },
  },
  {
    name: 'list_portfolios',
    description: 'List all portfolios',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getPortfolios) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const portfolios = await contexts.getPortfolios();
      return { success: true, data: portfolios };
    },
  },
  {
    name: 'add_asset',
    description: 'Add an asset/holding to a portfolio',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        symbol: { type: 'string', description: 'Asset symbol (e.g., AAPL)' },
        quantity: { type: 'number', description: 'Number of shares/units' },
        avg_buy_price: { type: 'number', description: 'Average purchase price per unit' },
      },
      required: ['portfolio_id', 'symbol', 'quantity', 'avg_buy_price'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addAsset) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const { portfolio_id, ...asset } = args;
      const result = await contexts.addAsset(portfolio_id, asset);
      return { success: true, data: result, message: `Added ${args.quantity} ${args.symbol} to portfolio` };
    },
  },
  {
    name: 'remove_asset',
    description: 'Remove an asset from a portfolio',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        asset_id: { type: 'string', description: 'Asset ID to remove' },
      },
      required: ['portfolio_id', 'asset_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.removeAsset) {
        return { success: false, error: 'Portfolio context not available' };
      }
      await contexts.removeAsset(args.portfolio_id, args.asset_id);
      return { success: true, message: `Asset removed from portfolio` };
    },
  },
  {
    name: 'add_transaction',
    description: 'Record a transaction (buy, sell, dividend) in a portfolio',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        type: { type: 'string', description: 'Transaction type', enum: ['buy', 'sell', 'dividend'] },
        symbol: { type: 'string', description: 'Asset symbol' },
        quantity: { type: 'number', description: 'Number of shares/units' },
        price: { type: 'number', description: 'Price per unit' },
        date: { type: 'string', description: 'Transaction date (YYYY-MM-DD)' },
      },
      required: ['portfolio_id', 'type', 'symbol', 'quantity', 'price'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addTransaction) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const { portfolio_id, ...tx } = args;
      const result = await contexts.addTransaction(portfolio_id, tx);
      return { success: true, data: result, message: `${args.type} transaction recorded: ${args.quantity} ${args.symbol} @ ${args.price}` };
    },
  },
  {
    name: 'get_portfolio_summary',
    description: 'Get portfolio summary with metrics, P&L, and allocation breakdown',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const summary = await contexts.getPortfolioSummary(args.portfolio_id);
      return { success: true, data: summary };
    },
  },
];
