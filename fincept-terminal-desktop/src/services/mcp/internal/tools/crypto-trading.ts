// Crypto Trading MCP Tools - Unified multi-broker crypto trading tools
import { InternalTool } from '../types';

export const cryptoTradingTools: InternalTool[] = [
  // === ORDER MANAGEMENT ===
  {
    name: 'crypto_place_market_order',
    description: 'Place a market order for crypto (buy/sell at current market price)',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair (e.g., BTC/USD, ETH/USDT)' },
        side: { type: 'string', description: 'Order side', enum: ['buy', 'sell'] },
        amount: { type: 'number', description: 'Amount in base currency (e.g., 0.1 BTC)' },
      },
      required: ['symbol', 'side', 'amount'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeTrade) {
        return { success: false, error: 'Trading context not available. Ensure broker is connected.' };
      }
      const result = await contexts.placeTrade({
        ...args,
        order_type: 'market',
      });
      return { success: true, data: result, message: `Market ${args.side} order placed: ${args.amount} ${args.symbol}` };
    },
  },
  {
    name: 'crypto_place_limit_order',
    description: 'Place a limit order for crypto (buy/sell at specified price)',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair (e.g., BTC/USD, ETH/USDT)' },
        side: { type: 'string', description: 'Order side', enum: ['buy', 'sell'] },
        amount: { type: 'number', description: 'Amount in base currency' },
        price: { type: 'number', description: 'Limit price' },
      },
      required: ['symbol', 'side', 'amount', 'price'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeTrade) {
        return { success: false, error: 'Trading context not available' };
      }
      const result = await contexts.placeTrade({
        ...args,
        order_type: 'limit',
      });
      return { success: true, data: result, message: `Limit ${args.side} order placed: ${args.amount} ${args.symbol} @ ${args.price}` };
    },
  },
  {
    name: 'crypto_place_stop_loss',
    description: 'Place a stop-loss order for crypto (auto-sell when price drops)',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair' },
        side: { type: 'string', description: 'Order side', enum: ['buy', 'sell'] },
        amount: { type: 'number', description: 'Amount in base currency' },
        stop_price: { type: 'number', description: 'Stop trigger price' },
        limit_price: { type: 'number', description: 'Limit price (optional, for stop-limit)' },
      },
      required: ['symbol', 'side', 'amount', 'stop_price'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeTrade) {
        return { success: false, error: 'Trading context not available' };
      }
      const result = await contexts.placeTrade({
        ...args,
        order_type: args.limit_price ? 'stop_limit' : 'stop',
      });
      return { success: true, data: result, message: `Stop order placed: ${args.side} ${args.amount} ${args.symbol} @ stop ${args.stop_price}` };
    },
  },
  {
    name: 'crypto_cancel_order',
    description: 'Cancel a pending crypto order by ID',
    inputSchema: {
      type: 'object',
      properties: {
        order_id: { type: 'string', description: 'Order ID to cancel' },
        symbol: { type: 'string', description: 'Trading pair (required for some exchanges)' },
      },
      required: ['order_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.cancelOrder) {
        return { success: false, error: 'Trading context not available' };
      }
      const result = await contexts.cancelOrder(args.order_id);
      return { success: true, data: result, message: `Order ${args.order_id} cancelled` };
    },
  },
  {
    name: 'crypto_cancel_all_orders',
    description: 'Cancel all open orders for a symbol or all symbols',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair (optional, cancels all if not provided)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getOrders || !contexts.cancelOrder) {
        return { success: false, error: 'Trading context not available' };
      }
      const orders = await contexts.getOrders({ status: 'open' });
      const targetOrders = args.symbol
        ? orders.filter((o: any) => o.symbol === args.symbol)
        : orders;

      const results = await Promise.allSettled(
        targetOrders.map((o: any) => contexts.cancelOrder!(o.id))
      );
      const cancelled = results.filter(r => r.status === 'fulfilled').length;
      return {
        success: true,
        data: { cancelled, total: targetOrders.length },
        message: `Cancelled ${cancelled}/${targetOrders.length} orders`
      };
    },
  },

  // === POSITION MANAGEMENT ===
  {
    name: 'crypto_get_positions',
    description: 'Get all open crypto positions across exchanges',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Filter by trading pair (optional)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getPositions) {
        return { success: false, error: 'Trading context not available' };
      }
      const positions = await contexts.getPositions();
      const filtered = args.symbol
        ? positions.filter((p: any) => p.symbol === args.symbol)
        : positions;
      return { success: true, data: filtered };
    },
  },
  {
    name: 'crypto_close_position',
    description: 'Close an open position for a crypto pair',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair to close position for' },
        percentage: { type: 'number', description: 'Percentage of position to close (1-100, default 100)' },
      },
      required: ['symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.closePosition) {
        return { success: false, error: 'Trading context not available' };
      }
      const result = await contexts.closePosition(args.symbol);
      return { success: true, data: result, message: `Position closed for ${args.symbol}` };
    },
  },

  // === ACCOUNT INFO ===
  {
    name: 'crypto_get_balance',
    description: 'Get account balance and available funds',
    inputSchema: {
      type: 'object',
      properties: {
        currency: { type: 'string', description: 'Filter by currency (e.g., USD, BTC, ETH)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getBalance) {
        return { success: false, error: 'Trading context not available' };
      }
      const balance = await contexts.getBalance();
      if (args.currency) {
        const filtered = balance[args.currency] || { free: 0, used: 0, total: 0 };
        return { success: true, data: { [args.currency]: filtered } };
      }
      return { success: true, data: balance };
    },
  },
  {
    name: 'crypto_get_holdings',
    description: 'Get all crypto holdings with current values',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts) => {
      if (!contexts.getHoldings) {
        return { success: false, error: 'Trading context not available' };
      }
      const holdings = await contexts.getHoldings();
      return { success: true, data: holdings };
    },
  },

  // === ORDER HISTORY ===
  {
    name: 'crypto_get_orders',
    description: 'Get order history with optional filters',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Filter by trading pair' },
        status: { type: 'string', description: 'Filter by status', enum: ['open', 'closed', 'cancelled', 'all'] },
        limit: { type: 'number', description: 'Number of orders to return (default 100)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getOrders) {
        return { success: false, error: 'Trading context not available' };
      }
      const orders = await contexts.getOrders(args);
      return { success: true, data: orders };
    },
  },

  // === MARKET DATA ===
  {
    name: 'crypto_get_ticker',
    description: 'Get real-time ticker data for a crypto pair',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair (e.g., BTC/USD)' },
      },
      required: ['symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getQuote) {
        return { success: false, error: 'Market data context not available' };
      }
      const ticker = await contexts.getQuote(args.symbol);
      return { success: true, data: ticker };
    },
  },
  {
    name: 'crypto_get_orderbook',
    description: 'Get order book (bids/asks) for a crypto pair',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair' },
        limit: { type: 'number', description: 'Number of price levels (default 20)' },
      },
      required: ['symbol'],
    },
    handler: async (args, contexts) => {
      // Order book would need to be added to contexts
      return { success: false, error: 'Order book context not yet implemented' };
    },
  },

  // === WATCHLIST ===
  {
    name: 'crypto_add_to_watchlist',
    description: 'Add a crypto pair to watchlist',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair to add' },
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
    name: 'crypto_remove_from_watchlist',
    description: 'Remove a crypto pair from watchlist',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair to remove' },
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
    name: 'crypto_search_symbol',
    description: 'Search for crypto trading pairs by keyword',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Search query (e.g., BTC, ethereum)' },
      },
      required: ['query'],
    },
    handler: async (args, contexts) => {
      if (!contexts.searchSymbol) {
        return { success: false, error: 'Search context not available' };
      }
      const results = await contexts.searchSymbol(args.query);
      return { success: true, data: results };
    },
  },

  // === TRADING MODE ===
  {
    name: 'crypto_switch_trading_mode',
    description: 'Switch between live and paper trading mode',
    inputSchema: {
      type: 'object',
      properties: {
        mode: { type: 'string', description: 'Trading mode', enum: ['live', 'paper'] },
      },
      required: ['mode'],
    },
    handler: async (args, _contexts) => {
      // This would need to be integrated with BrokerContext
      return { success: false, error: 'Trading mode switching not yet implemented in MCP' };
    },
  },
  {
    name: 'crypto_switch_broker',
    description: 'Switch active crypto exchange/broker',
    inputSchema: {
      type: 'object',
      properties: {
        broker: { type: 'string', description: 'Broker ID (kraken, binance, hyperliquid, etc.)' },
      },
      required: ['broker'],
    },
    handler: async (args, _contexts) => {
      // This would need to be integrated with BrokerContext
      return { success: false, error: 'Broker switching not yet implemented in MCP' };
    },
  },

  // === PORTFOLIO METRICS ===
  {
    name: 'crypto_get_portfolio_value',
    description: 'Get total portfolio value in quote currency',
    inputSchema: {
      type: 'object',
      properties: {
        quote: { type: 'string', description: 'Quote currency (default USD)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getHoldings || !contexts.getQuote) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const holdings = await contexts.getHoldings();
      const quote = args.quote || 'USD';

      // Calculate total value (simplified)
      let totalValue = 0;
      for (const holding of holdings) {
        if (holding.currency === quote) {
          totalValue += holding.amount;
        } else {
          const ticker = await contexts.getQuote(`${holding.currency}/${quote}`);
          totalValue += holding.amount * ticker.last;
        }
      }

      return { success: true, data: { totalValue, quote, holdings: holdings.length } };
    },
  },
  {
    name: 'crypto_get_pnl',
    description: 'Get profit/loss for positions or overall portfolio',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair (optional, returns all if not provided)' },
        timeframe: { type: 'string', description: 'Timeframe', enum: ['day', 'week', 'month', 'all'] },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getPositions) {
        return { success: false, error: 'PnL context not available' };
      }
      const positions = await contexts.getPositions();
      const filtered = args.symbol
        ? positions.filter((p: any) => p.symbol === args.symbol)
        : positions;

      const totalPnl = filtered.reduce((sum: number, p: any) => sum + (p.unrealizedPnl || 0), 0);
      return {
        success: true,
        data: {
          pnl: totalPnl,
          positions: filtered.length,
          timeframe: args.timeframe || 'all'
        }
      };
    },
  },

  // === RISK MANAGEMENT ===
  {
    name: 'crypto_set_stop_loss_for_position',
    description: 'Set stop-loss for an existing position',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair' },
        stop_price: { type: 'number', description: 'Stop price trigger' },
        percentage: { type: 'number', description: 'Stop loss as percentage below entry (e.g., 5 for 5%)' },
      },
      required: ['symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPositions || !contexts.placeTrade) {
        return { success: false, error: 'Risk management context not available' };
      }

      const positions = await contexts.getPositions();
      const position = positions.find((p: any) => p.symbol === args.symbol);

      if (!position) {
        return { success: false, error: `No open position for ${args.symbol}` };
      }

      const stopPrice = args.stop_price || position.entryPrice * (1 - (args.percentage || 5) / 100);

      const result = await contexts.placeTrade({
        symbol: args.symbol,
        side: position.side === 'long' ? 'sell' : 'buy',
        amount: Math.abs(position.amount),
        order_type: 'stop',
        stop_price: stopPrice,
      });

      return {
        success: true,
        data: result,
        message: `Stop-loss set for ${args.symbol} at ${stopPrice}`
      };
    },
  },
  {
    name: 'crypto_set_take_profit',
    description: 'Set take-profit for an existing position',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading pair' },
        target_price: { type: 'number', description: 'Target price' },
        percentage: { type: 'number', description: 'Take profit as percentage above entry (e.g., 10 for 10%)' },
      },
      required: ['symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPositions || !contexts.placeTrade) {
        return { success: false, error: 'Risk management context not available' };
      }

      const positions = await contexts.getPositions();
      const position = positions.find((p: any) => p.symbol === args.symbol);

      if (!position) {
        return { success: false, error: `No open position for ${args.symbol}` };
      }

      const targetPrice = args.target_price || position.entryPrice * (1 + (args.percentage || 10) / 100);

      const result = await contexts.placeTrade({
        symbol: args.symbol,
        side: position.side === 'long' ? 'sell' : 'buy',
        amount: Math.abs(position.amount),
        order_type: 'limit',
        price: targetPrice,
      });

      return {
        success: true,
        data: result,
        message: `Take-profit set for ${args.symbol} at ${targetPrice}`
      };
    },
  },
];
