import { InternalTool } from '../types';

export const tradingTools: InternalTool[] = [
  {
    name: 'place_order',
    description: 'Place a buy or sell order through the active broker',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Trading symbol (e.g., AAPL, BTC/USD)' },
        side: { type: 'string', description: 'Order side', enum: ['BUY', 'SELL'] },
        quantity: { type: 'number', description: 'Number of shares/units to trade' },
        order_type: { type: 'string', description: 'Order type', enum: ['MARKET', 'LIMIT', 'STOP', 'STOP_LIMIT'] },
        price: { type: 'number', description: 'Limit price (required for LIMIT/STOP_LIMIT orders)' },
        stop_price: { type: 'number', description: 'Stop price (required for STOP/STOP_LIMIT orders)' },
      },
      required: ['symbol', 'side', 'quantity', 'order_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.placeTrade) {
        return { success: false, error: 'Trading context not available. Ensure a broker is connected.' };
      }
      const result = await contexts.placeTrade(args);
      return { success: true, data: result, message: `Order placed: ${args.side} ${args.quantity} ${args.symbol}` };
    },
  },
  {
    name: 'cancel_order',
    description: 'Cancel a pending order by its ID',
    inputSchema: {
      type: 'object',
      properties: {
        order_id: { type: 'string', description: 'The order ID to cancel' },
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
    name: 'modify_order',
    description: 'Modify an existing order (price, quantity)',
    inputSchema: {
      type: 'object',
      properties: {
        order_id: { type: 'string', description: 'The order ID to modify' },
        price: { type: 'number', description: 'New limit price' },
        quantity: { type: 'number', description: 'New quantity' },
      },
      required: ['order_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.modifyOrder) {
        return { success: false, error: 'Trading context not available' };
      }
      const { order_id, ...updates } = args;
      const result = await contexts.modifyOrder(order_id, updates);
      return { success: true, data: result, message: `Order ${order_id} modified` };
    },
  },
  {
    name: 'get_positions',
    description: 'Get all open positions from the active broker',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getPositions) {
        return { success: false, error: 'Trading context not available' };
      }
      const positions = await contexts.getPositions();
      return { success: true, data: positions };
    },
  },
  {
    name: 'get_balance',
    description: 'Get account balance from the active broker',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getBalance) {
        return { success: false, error: 'Trading context not available' };
      }
      const balance = await contexts.getBalance();
      return { success: true, data: balance };
    },
  },
  {
    name: 'get_orders',
    description: 'Get order history from the active broker',
    inputSchema: {
      type: 'object',
      properties: {
        status: { type: 'string', description: 'Filter by status', enum: ['open', 'filled', 'cancelled', 'rejected', 'all'] },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getOrders) {
        return { success: false, error: 'Trading context not available' };
      }
      const orders = await contexts.getOrders(args.status ? { status: args.status } : undefined);
      return { success: true, data: orders };
    },
  },
  {
    name: 'get_holdings',
    description: 'Get portfolio holdings from the active broker',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getHoldings) {
        return { success: false, error: 'Trading context not available' };
      }
      const holdings = await contexts.getHoldings();
      return { success: true, data: holdings };
    },
  },
  {
    name: 'close_position',
    description: 'Close an open position for a given symbol',
    inputSchema: {
      type: 'object',
      properties: {
        symbol: { type: 'string', description: 'Symbol to close position for' },
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
];
