/**
 * Get Orders Node
 * Fetches order history from broker
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { TradingBridge } from '../../adapters/TradingBridge';

export class GetOrdersNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Order History',
    name: 'getOrders',
    icon: 'file:orders.svg',
    group: ['trading'],
    version: 1,
    subtitle: '={{$parameter["orderStatus"]}} orders',
    description: 'Fetches order history from broker',
    defaults: {
      name: 'Order History',
      color: '#3b82f6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Broker',
        name: 'broker',
        type: NodePropertyType.Options,
        default: 'paper',
        options: [
          { name: 'Paper Trading', value: 'paper' },
          { name: 'Zerodha Kite', value: 'zerodha' },
          { name: 'Fyers', value: 'fyers' },
          { name: 'Alpaca', value: 'alpaca' },
          { name: 'Binance', value: 'binance' },
          { name: 'Kraken', value: 'kraken' },
          { name: 'All Connected', value: 'all' },
        ],
        description: 'Broker to fetch orders from',
      },
      {
        displayName: 'Order Status',
        name: 'orderStatus',
        type: NodePropertyType.Options,
        default: 'all',
        options: [
          { name: 'All Orders', value: 'all' },
          { name: 'Open/Pending', value: 'open' },
          { name: 'Filled/Executed', value: 'filled' },
          { name: 'Cancelled', value: 'cancelled' },
          { name: 'Rejected', value: 'rejected' },
        ],
        description: 'Filter orders by status',
      },
      {
        displayName: 'Filter by Symbol',
        name: 'filterSymbol',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'AAPL, MSFT',
        description: 'Filter orders by symbols (comma-separated)',
      },
      {
        displayName: 'Time Period',
        name: 'timePeriod',
        type: NodePropertyType.Options,
        default: 'today',
        options: [
          { name: 'Today', value: 'today' },
          { name: 'Last 7 Days', value: '7d' },
          { name: 'Last 30 Days', value: '30d' },
          { name: 'Last 90 Days', value: '90d' },
          { name: 'All Time', value: 'all' },
        ],
        description: 'Time period for order history',
      },
      {
        displayName: 'Order Side',
        name: 'orderSide',
        type: NodePropertyType.Options,
        default: 'all',
        options: [
          { name: 'All', value: 'all' },
          { name: 'Buy Only', value: 'buy' },
          { name: 'Sell Only', value: 'sell' },
        ],
        description: 'Filter by order side',
      },
      {
        displayName: 'Limit',
        name: 'limit',
        type: NodePropertyType.Number,
        default: 50,
        description: 'Maximum number of orders to return',
      },
      {
        displayName: 'Include Trade Details',
        name: 'includeTradeDetails',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include execution details and fills',
      },
      {
        displayName: 'Sort Order',
        name: 'sortOrder',
        type: NodePropertyType.Options,
        default: 'desc',
        options: [
          { name: 'Newest First', value: 'desc' },
          { name: 'Oldest First', value: 'asc' },
        ],
        description: 'Sort order for results',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const broker = this.getNodeParameter('broker', 0) as string;
    const orderStatus = this.getNodeParameter('orderStatus', 0) as string;
    const filterSymbol = this.getNodeParameter('filterSymbol', 0) as string;
    const timePeriod = this.getNodeParameter('timePeriod', 0) as string;
    const orderSide = this.getNodeParameter('orderSide', 0) as string;
    const limit = this.getNodeParameter('limit', 0) as number;
    const includeTradeDetails = this.getNodeParameter('includeTradeDetails', 0) as boolean;
    const sortOrder = this.getNodeParameter('sortOrder', 0) as string;

    const isPaper = broker === 'paper';

    try {
      // Fetch real orders from TradingBridge
      const statusFilter = orderStatus !== 'all' ? orderStatus.toUpperCase() : undefined;
      let orders: any[] = (await TradingBridge.getOrders(broker as any, statusFilter as any)).map(o => ({
        orderId: o.orderId,
        symbol: o.symbol,
        exchange: o.exchange,
        side: o.side,
        orderType: o.type,
        quantity: o.quantity,
        filledQuantity: o.filledQuantity,
        price: o.price,
        averagePrice: o.filledPrice,
        triggerPrice: o.triggerPrice,
        status: o.status?.toLowerCase() || 'unknown',
        product: o.product,
        validity: o.validity,
        tag: o.tag,
        timestamp: o.timestamp,
      }));

      // Filter by symbol
      if (filterSymbol) {
        const symbols = filterSymbol.split(',').map(s => s.trim().toUpperCase());
        orders = orders.filter((o: any) => symbols.includes(o.symbol.toUpperCase()));
      }

      // Filter by side
      if (orderSide !== 'all') {
        orders = orders.filter((o: any) => o.side.toLowerCase() === orderSide.toLowerCase());
      }

      // Filter by time period
      const now = new Date();
      const periodMs: Record<string, number> = {
        today: 24 * 60 * 60 * 1000,
        '7d': 7 * 24 * 60 * 60 * 1000,
        '30d': 30 * 24 * 60 * 60 * 1000,
        '90d': 90 * 24 * 60 * 60 * 1000,
      };

      if (timePeriod !== 'all' && periodMs[timePeriod]) {
        const cutoff = new Date(now.getTime() - periodMs[timePeriod]);
        orders = orders.filter((o: any) => new Date(o.timestamp) >= cutoff);
      }

      // Sort orders
      orders.sort((a: any, b: any) => {
        const timeA = new Date(a.timestamp).getTime();
        const timeB = new Date(b.timestamp).getTime();
        return sortOrder === 'desc' ? timeB - timeA : timeA - timeB;
      });

      // Apply limit
      orders = orders.slice(0, limit);

      // Calculate summary statistics
      const summary = {
        totalOrders: orders.length,
        openOrders: orders.filter((o: any) => o.status === 'open' || o.status === 'pending').length,
        filledOrders: orders.filter((o: any) => o.status === 'filled').length,
        cancelledOrders: orders.filter((o: any) => o.status === 'cancelled').length,
        rejectedOrders: orders.filter((o: any) => o.status === 'rejected').length,
        buyOrders: orders.filter((o: any) => o.side === 'BUY').length,
        sellOrders: orders.filter((o: any) => o.side === 'SELL').length,
        totalVolume: orders.reduce((sum: any, o: any) => sum + (o.filledQuantity || 0) * (o.averagePrice || o.price), 0),
        broker,
        paperTrading: isPaper,
        period: timePeriod,
        fetchedAt: new Date().toISOString(),
      };

      if (orders.length === 0) {
        return [[{
          json: {
            message: 'No orders found matching criteria',
            summary,
          },
        }]];
      }

      // Format output
      const formattedOrders = orders.map(o => ({
        json: {
          ...o,
          broker,
          paperTrading: isPaper,
        },
      }));

      return [formattedOrders];
    } catch (error) {
      return [[{
        json: {
          success: false,
          error: error instanceof Error ? error.message : 'Unknown error',
          broker,
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }

}
