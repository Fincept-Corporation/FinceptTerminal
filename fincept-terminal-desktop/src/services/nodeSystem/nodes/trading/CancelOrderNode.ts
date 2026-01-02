/**
 * Cancel Order Node
 * Cancels open orders
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
import { AuditLogger } from '../../safety/AuditLogger';

export class CancelOrderNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Cancel Order',
    name: 'cancelOrder',
    icon: 'file:cancel-order.svg',
    group: ['trading'],
    version: 1,
    subtitle: 'Cancel {{$parameter["orderId"]}}',
    description: 'Cancels open orders',
    defaults: {
      name: 'Cancel Order',
      color: '#ef4444',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Cancel Mode',
        name: 'cancelMode',
        type: NodePropertyType.Options,
        default: 'single',
        options: [
          { name: 'Single Order', value: 'single' },
          { name: 'All Open Orders', value: 'all' },
          { name: 'By Symbol', value: 'symbol' },
          { name: 'From Input', value: 'input' },
        ],
        description: 'What orders to cancel',
      },
      {
        displayName: 'Order ID',
        name: 'orderId',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'ORD123456',
        description: 'ID of order to cancel',
        displayOptions: {
          show: { cancelMode: ['single'] },
        },
      },
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'AAPL',
        description: 'Cancel all open orders for this symbol',
        displayOptions: {
          show: { cancelMode: ['symbol'] },
        },
      },
      {
        displayName: 'Input Order ID Field',
        name: 'inputOrderIdField',
        type: NodePropertyType.String,
        default: 'orderId',
        description: 'Field name in input containing order ID(s)',
        displayOptions: {
          show: { cancelMode: ['input'] },
        },
      },
      {
        displayName: 'Cancel Only',
        name: 'cancelOnly',
        type: NodePropertyType.Options,
        default: 'all',
        options: [
          { name: 'All Order Types', value: 'all' },
          { name: 'Buy Orders', value: 'buy' },
          { name: 'Sell Orders', value: 'sell' },
          { name: 'Limit Orders', value: 'limit' },
          { name: 'Stop Orders', value: 'stop' },
        ],
        description: 'Filter which orders to cancel',
        displayOptions: {
          show: { cancelMode: ['all', 'symbol'] },
        },
      },
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
        ],
        description: 'Broker where orders exist',
      },
      {
        displayName: 'Require Confirmation',
        name: 'requireConfirmation',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Require confirmation before cancelling',
        displayOptions: {
          show: { cancelMode: ['all'] },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const cancelMode = this.getNodeParameter('cancelMode', 0) as string;
    const broker = this.getNodeParameter('broker', 0) as string;

    const tradingBridge = new TradingBridge();
    const isPaper = broker === 'paper';
    const auditLogger = new AuditLogger();

    try {
      const results: Array<{ json: Record<string, unknown> }> = [];
      let orderIds: string[] = [];

      switch (cancelMode) {
        case 'single': {
          const orderId = this.getNodeParameter('orderId', 0) as string;
          if (!orderId) {
            return [[{
              json: {
                success: false,
                error: 'No order ID provided',
                timestamp: new Date().toISOString(),
              },
            }]];
          }
          orderIds = [orderId];
          break;
        }

        case 'symbol': {
          const symbol = this.getNodeParameter('symbol', 0) as string;
          const cancelOnly = this.getNodeParameter('cancelOnly', 0) as string;

          // Get open orders for symbol
          const orders = await this.getOpenOrders(tradingBridge, broker, isPaper);
          let filtered = orders.filter(o => o.symbol.toUpperCase() === symbol.toUpperCase());

          // Apply additional filter
          if (cancelOnly === 'buy') {
            filtered = filtered.filter(o => o.side === 'BUY');
          } else if (cancelOnly === 'sell') {
            filtered = filtered.filter(o => o.side === 'SELL');
          } else if (cancelOnly === 'limit') {
            filtered = filtered.filter(o => o.orderType === 'LIMIT');
          } else if (cancelOnly === 'stop') {
            filtered = filtered.filter(o => o.orderType === 'SL' || o.orderType === 'SL-L');
          }

          orderIds = filtered.map(o => o.orderId);
          break;
        }

        case 'all': {
          const cancelOnly = this.getNodeParameter('cancelOnly', 0) as string;

          // Get all open orders
          const orders = await this.getOpenOrders(tradingBridge, broker, isPaper);
          let filtered = orders;

          // Apply filter
          if (cancelOnly === 'buy') {
            filtered = filtered.filter(o => o.side === 'BUY');
          } else if (cancelOnly === 'sell') {
            filtered = filtered.filter(o => o.side === 'SELL');
          } else if (cancelOnly === 'limit') {
            filtered = filtered.filter(o => o.orderType === 'LIMIT');
          } else if (cancelOnly === 'stop') {
            filtered = filtered.filter(o => o.orderType === 'SL' || o.orderType === 'SL-L');
          }

          orderIds = filtered.map(o => o.orderId);
          break;
        }

        case 'input': {
          const inputData = this.getInputData();
          const field = this.getNodeParameter('inputOrderIdField', 0) as string;

          for (const item of inputData) {
            if (item.json?.[field]) {
              const ids = item.json[field];
              if (Array.isArray(ids)) {
                orderIds.push(...ids.map(String));
              } else {
                orderIds.push(String(ids));
              }
            }
          }
          break;
        }
      }

      if (orderIds.length === 0) {
        return [[{
          json: {
            success: true,
            message: 'No orders to cancel',
            broker,
            paperTrading: isPaper,
            timestamp: new Date().toISOString(),
          },
        }]];
      }

      // Cancel each order
      for (const orderId of orderIds) {
        try {
          const result = await tradingBridge.cancelOrder(orderId, broker, isPaper);

          // Log the cancellation
          await auditLogger.log({
            type: 'ORDER_CANCELLED' as any,
            details: {
              orderId,
              broker,
              paperTrading: isPaper,
            },
          });

          results.push({
            json: {
              success: true,
              orderId,
              status: 'cancelled',
              broker,
              paperTrading: isPaper,
              timestamp: new Date().toISOString(),
            },
          });
        } catch (error) {
          results.push({
            json: {
              success: false,
              orderId,
              error: error instanceof Error ? error.message : 'Unknown error',
              broker,
              timestamp: new Date().toISOString(),
            },
          });
        }
      }

      return [results];
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

  private async getOpenOrders(bridge: TradingBridge, broker: string, isPaper: boolean): Promise<any[]> {
    // Mock implementation - would fetch real orders
    return [
      { orderId: 'ORD001', symbol: 'AAPL', side: 'BUY', orderType: 'LIMIT', status: 'open' },
      { orderId: 'ORD002', symbol: 'MSFT', side: 'SELL', orderType: 'LIMIT', status: 'open' },
      { orderId: 'ORD003', symbol: 'AAPL', side: 'BUY', orderType: 'SL', status: 'open' },
    ];
  }
}
