// Polymarket MCP Tools - Read-only market intelligence tools for agents
// These tools allow AI agents to fetch prediction market data for analysis.
// Write operations (place/cancel order) are intentionally excluded here —
// they are handled by the PolymarketBotService with explicit user approval.

import { InternalTool } from '../types';

export const polymarketTools: InternalTool[] = [
  {
    name: 'polymarket_get_markets',
    description:
      'Fetch active Polymarket prediction markets. Returns a list of markets with questions, current YES/NO prices, volume, liquidity, and end dates. Use this to find trading opportunities.',
    inputSchema: {
      type: 'object',
      properties: {
        limit: {
          type: 'number',
          description: 'Number of markets to return (default: 20, max: 100)',
        },
        active: {
          type: 'boolean',
          description: 'Filter to only active markets (default: true)',
        },
        closed: {
          type: 'boolean',
          description: 'Include closed/resolved markets (default: false)',
        },
        order: {
          type: 'string',
          description: 'Sort order field: volume, liquidity, endDate, startDate',
          enum: ['volume', 'liquidity', 'endDate', 'startDate'],
        },
        ascending: {
          type: 'boolean',
          description: 'Sort ascending (default: false = descending)',
        },
      },
      required: [],
    },
    handler: async (args, contexts) => {
      if (!contexts.polymarketGetMarkets) {
        return { success: false, error: 'Polymarket context not available' };
      }
      try {
        const markets = await contexts.polymarketGetMarkets({
          limit: args.limit ?? 20,
          active: args.active ?? true,
          closed: args.closed ?? false,
          order: args.order,
          ascending: args.ascending ?? false,
        });
        // Return simplified view for LLM consumption
        const simplified = markets.slice(0, args.limit ?? 20).map((m: any) => ({
          id: m.id,
          question: m.question,
          category: m.category,
          outcomes: m.outcomes,
          prices: m.outcomePrices,
          volume: m.volume,
          liquidity: m.liquidity,
          endDate: m.endDate,
          acceptingOrders: m.acceptingOrders,
          bestBid: m.bestBid,
          bestAsk: m.bestAsk,
        }));
        return { success: true, data: simplified, count: simplified.length };
      } catch (error: any) {
        return { success: false, error: error?.message ?? 'Failed to fetch markets' };
      }
    },
  },

  {
    name: 'polymarket_search_markets',
    description:
      'Search Polymarket prediction markets by keyword or topic. Returns matching markets with prices and volume.',
    inputSchema: {
      type: 'object',
      properties: {
        query: {
          type: 'string',
          description: 'Search query (e.g., "bitcoin", "election", "Fed rate")',
        },
        limit: {
          type: 'number',
          description: 'Max results to return (default: 10)',
        },
      },
      required: ['query'],
    },
    handler: async (args, contexts) => {
      if (!contexts.polymarketSearch) {
        return { success: false, error: 'Polymarket context not available' };
      }
      try {
        const results = await contexts.polymarketSearch(args.query, 'markets');
        const markets = Array.isArray(results) ? results : (results?.markets ?? []);
        const limited = markets.slice(0, args.limit ?? 10).map((m: any) => ({
          id: m.id,
          question: m.question,
          category: m.category,
          outcomes: m.outcomes,
          prices: m.outcomePrices,
          volume: m.volume,
          endDate: m.endDate,
        }));
        return { success: true, data: limited, count: limited.length };
      } catch (error: any) {
        return { success: false, error: error?.message ?? 'Search failed' };
      }
    },
  },

  {
    name: 'polymarket_get_market_detail',
    description:
      'Get detailed information about a specific Polymarket prediction market including order book, price history summary, and market metadata.',
    inputSchema: {
      type: 'object',
      properties: {
        market_id: {
          type: 'string',
          description: 'The market ID (condition ID or market ID from polymarket_get_markets)',
        },
      },
      required: ['market_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.polymarketGetMarketDetail) {
        return { success: false, error: 'Polymarket context not available' };
      }
      try {
        const detail = await contexts.polymarketGetMarketDetail(args.market_id);
        return { success: true, data: detail };
      } catch (error: any) {
        return { success: false, error: error?.message ?? 'Failed to fetch market detail' };
      }
    },
  },

  {
    name: 'polymarket_get_order_book',
    description:
      'Get the current order book (bids and asks) for a Polymarket token. Use this to assess liquidity and find optimal entry prices.',
    inputSchema: {
      type: 'object',
      properties: {
        token_id: {
          type: 'string',
          description: 'The CLOB token ID for the outcome (YES or NO token)',
        },
      },
      required: ['token_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.polymarketGetOrderBook) {
        return { success: false, error: 'Polymarket context not available' };
      }
      try {
        const book = await contexts.polymarketGetOrderBook(args.token_id);
        // Compute best bid/ask and spread for the LLM
        const bestBid = book.bids?.[0]?.price ?? null;
        const bestAsk = book.asks?.[0]?.price ?? null;
        const spread = bestBid && bestAsk
          ? (parseFloat(bestAsk) - parseFloat(bestBid)).toFixed(4)
          : null;
        return {
          success: true,
          data: {
            token_id: book.asset_id,
            best_bid: bestBid,
            best_ask: bestAsk,
            spread,
            top_bids: book.bids?.slice(0, 5),
            top_asks: book.asks?.slice(0, 5),
            timestamp: book.timestamp,
          },
        };
      } catch (error: any) {
        return { success: false, error: error?.message ?? 'Failed to fetch order book' };
      }
    },
  },

  {
    name: 'polymarket_get_price_history',
    description:
      'Get historical price data for a Polymarket token. Use this to analyze trends and identify momentum or mean-reversion opportunities.',
    inputSchema: {
      type: 'object',
      properties: {
        token_id: {
          type: 'string',
          description: 'The CLOB token ID for the outcome',
        },
        interval: {
          type: 'string',
          description: 'Time window: max, all, 1m (1 month), 1w, 1d, 6h, 1h (default: 1w)',
          enum: ['max', 'all', '1m', '1w', '1d', '6h', '1h'],
        },
        fidelity: {
          type: 'number',
          description: 'Resolution in minutes (1 = 1-min bars, 60 = hourly). Default: 60',
        },
      },
      required: ['token_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.polymarketGetPriceHistory) {
        return { success: false, error: 'Polymarket context not available' };
      }
      try {
        const history = await contexts.polymarketGetPriceHistory({
          token_id: args.token_id,
          interval: args.interval ?? '1w',
          fidelity: args.fidelity ?? 60,
        });
        // Compute summary stats for the LLM
        const prices = history.prices ?? [];
        const priceValues = prices.map((p: any) => p.price);
        const latest = priceValues[priceValues.length - 1] ?? null;
        const oldest = priceValues[0] ?? null;
        const high = priceValues.length ? Math.max(...priceValues) : null;
        const low = priceValues.length ? Math.min(...priceValues) : null;
        const change = oldest && latest ? ((latest - oldest) / oldest * 100).toFixed(2) : null;
        return {
          success: true,
          data: {
            token_id: args.token_id,
            interval: args.interval ?? '1w',
            latest_price: latest,
            price_change_pct: change,
            high,
            low,
            data_points: prices.length,
            // Return last 20 data points for trend visibility
            recent_prices: prices.slice(-20),
          },
        };
      } catch (error: any) {
        return { success: false, error: error?.message ?? 'Failed to fetch price history' };
      }
    },
  },

  {
    name: 'polymarket_get_events',
    description:
      'Get Polymarket prediction market events (grouped sets of related markets). Useful for understanding broader themes and finding related markets.',
    inputSchema: {
      type: 'object',
      properties: {
        limit: {
          type: 'number',
          description: 'Number of events to return (default: 10)',
        },
        active: {
          type: 'boolean',
          description: 'Filter to active events only (default: true)',
        },
      },
      required: [],
    },
    handler: async (args, contexts) => {
      if (!contexts.polymarketGetEvents) {
        return { success: false, error: 'Polymarket context not available' };
      }
      try {
        const events = await contexts.polymarketGetEvents({
          limit: args.limit ?? 10,
          active: args.active ?? true,
          closed: false,
        });
        const simplified = events.slice(0, args.limit ?? 10).map((e: any) => ({
          id: e.id,
          title: e.title,
          description: e.description?.slice(0, 200),
          startDate: e.startDate,
          endDate: e.endDate,
          marketCount: e.markets?.length ?? 0,
          volume: e.volume,
          liquidity: e.liquidity,
        }));
        return { success: true, data: simplified, count: simplified.length };
      } catch (error: any) {
        return { success: false, error: error?.message ?? 'Failed to fetch events' };
      }
    },
  },

  {
    name: 'polymarket_get_open_orders',
    description:
      'Get the current user\'s open orders on Polymarket. Requires credentials to be configured.',
    inputSchema: {
      type: 'object',
      properties: {},
      required: [],
    },
    handler: async (_args, contexts) => {
      if (!contexts.polymarketGetOpenOrders) {
        return { success: false, error: 'Polymarket context not available or credentials not set' };
      }
      try {
        const orders = await contexts.polymarketGetOpenOrders();
        return { success: true, data: orders, count: orders?.length ?? 0 };
      } catch (error: any) {
        return { success: false, error: error?.message ?? 'Failed to fetch open orders' };
      }
    },
  },

  {
    name: 'polymarket_get_trades',
    description:
      'Get recent trades for a Polymarket token. Useful for understanding market sentiment and trade flow.',
    inputSchema: {
      type: 'object',
      properties: {
        token_id: {
          type: 'string',
          description: 'The CLOB token ID to get trades for',
        },
        limit: {
          type: 'number',
          description: 'Number of trades to return (default: 20)',
        },
      },
      required: ['token_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.polymarketGetTrades) {
        return { success: false, error: 'Polymarket context not available' };
      }
      try {
        const trades = await contexts.polymarketGetTrades(args.token_id, args.limit ?? 20);
        return { success: true, data: trades, count: trades?.length ?? 0 };
      } catch (error: any) {
        return { success: false, error: error?.message ?? 'Failed to fetch trades' };
      }
    },
  },
];
