import { InternalTool } from '../types';

export const marketDataTools: InternalTool[] = [
  {
    name: 'get_quote',
    description: 'Get current market quote/price for a symbol',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Symbol to get quote for (e.g., AAPL, TSLA, BTC/USD)' },
      },
      required: ['symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getQuote) {
        return { success: false, error: 'Market data context not available' };
      }
      const quote = await contexts.getQuote(args.symbol);
      return { success: true, data: quote };
    },
  },
  {
    name: 'add_to_watchlist',
    description: 'Add a symbol to the watchlist',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Symbol to add (e.g., AAPL)' },
      },
      required: ['symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addToWatchlist) {
        return { success: false, error: 'Watchlist context not available' };
      }
      await contexts.addToWatchlist(args.symbol);
      return { success: true, message: `${args.symbol} added to watchlist` };
    },
  },
  {
    name: 'remove_from_watchlist',
    description: 'Remove a symbol from the watchlist',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Symbol to remove' },
      },
      required: ['symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.removeFromWatchlist) {
        return { success: false, error: 'Watchlist context not available' };
      }
      await contexts.removeFromWatchlist(args.symbol);
      return { success: true, message: `${args.symbol} removed from watchlist` };
    },
  },
  {
    name: 'search_symbol',
    description: 'Search for trading symbols by name or keyword',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Search query (e.g., "Apple", "Bitcoin", "NIFTY")' },
      },
      required: ['query'],
    },
    handler: async (args, contexts) => {
      if (!contexts.searchSymbol) {
        return { success: false, error: 'Market data context not available' };
      }
      const results = await contexts.searchSymbol(args.query);
      return { success: true, data: results };
    },
  },
];
