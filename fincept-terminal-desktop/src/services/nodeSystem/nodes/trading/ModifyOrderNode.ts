/**
 * Modify Order Node
 * Modifies existing open orders
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

export class ModifyOrderNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Modify Order',
    name: 'modifyOrder',
    icon: 'file:modify-order.svg',
    group: ['trading'],
    version: 1,
    subtitle: 'Modify {{$parameter["orderId"]}}',
    description: 'Modifies existing open orders',
    defaults: {
      name: 'Modify Order',
      color: '#f59e0b',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Order ID',
        name: 'orderId',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'ORD123456',
        description: 'ID of order to modify',
      },
      {
        displayName: 'Use Input Order ID',
        name: 'useInputOrderId',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use order ID from input data',
      },
      {
        displayName: 'Input Order ID Field',
        name: 'inputOrderIdField',
        type: NodePropertyType.String,
        default: 'orderId',
        description: 'Field name in input containing order ID',
        displayOptions: {
          show: { useInputOrderId: [true] },
        },
      },
      {
        displayName: 'Modification Type',
        name: 'modificationType',
        type: NodePropertyType.Options,
        default: 'price',
        options: [
          { name: 'Price Only', value: 'price' },
          { name: 'Quantity Only', value: 'quantity' },
          { name: 'Price and Quantity', value: 'both' },
          { name: 'Order Type', value: 'type' },
        ],
        description: 'What to modify',
      },
      {
        displayName: 'New Price',
        name: 'newPrice',
        type: NodePropertyType.Number,
        default: 0,
        description: 'New limit price',
        displayOptions: {
          show: { modificationType: ['price', 'both'] },
        },
      },
      {
        displayName: 'New Quantity',
        name: 'newQuantity',
        type: NodePropertyType.Number,
        default: 0,
        description: 'New order quantity',
        displayOptions: {
          show: { modificationType: ['quantity', 'both'] },
        },
      },
      {
        displayName: 'New Order Type',
        name: 'newOrderType',
        type: NodePropertyType.Options,
        default: 'LIMIT',
        options: [
          { name: 'Limit', value: 'LIMIT' },
          { name: 'Market', value: 'MARKET' },
          { name: 'Stop Loss', value: 'SL' },
          { name: 'Stop Limit', value: 'SL-L' },
        ],
        description: 'New order type',
        displayOptions: {
          show: { modificationType: ['type'] },
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
        description: 'Broker where order exists',
      },
      {
        displayName: 'Require Confirmation',
        name: 'requireConfirmation',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Require confirmation before modifying',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputOrderId = this.getNodeParameter('useInputOrderId', 0) as boolean;
    const modificationType = this.getNodeParameter('modificationType', 0) as string;
    const broker = this.getNodeParameter('broker', 0) as string;

    let orderId: string;

    if (useInputOrderId) {
      const inputData = this.getInputData();
      const field = this.getNodeParameter('inputOrderIdField', 0) as string;
      orderId = inputData[0]?.json?.[field] as string || '';
    } else {
      orderId = this.getNodeParameter('orderId', 0) as string;
    }

    if (!orderId) {
      return [[{
        json: {
          success: false,
          error: 'No order ID provided',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    const tradingBridge = new TradingBridge();
    const isPaper = broker === 'paper';

    try {
      // Build modification request
      const modifications: Record<string, unknown> = {};

      switch (modificationType) {
        case 'price':
          modifications.price = this.getNodeParameter('newPrice', 0);
          break;
        case 'quantity':
          modifications.quantity = this.getNodeParameter('newQuantity', 0);
          break;
        case 'both':
          modifications.price = this.getNodeParameter('newPrice', 0);
          modifications.quantity = this.getNodeParameter('newQuantity', 0);
          break;
        case 'type':
          modifications.orderType = this.getNodeParameter('newOrderType', 0);
          break;
      }

      const result = await tradingBridge.modifyOrder(orderId, modifications, broker, isPaper);

      // Log the modification
      const auditLogger = new AuditLogger();
      await auditLogger.log({
        type: 'ORDER_MODIFIED' as any,
        details: {
          orderId,
          modifications,
          broker,
          paperTrading: isPaper,
        },
      });

      return [[{
        json: {
          success: true,
          orderId,
          modifications,
          status: result.status,
          broker,
          paperTrading: isPaper,
          timestamp: new Date().toISOString(),
          message: `Order ${orderId} modified successfully`,
        },
      }]];
    } catch (error) {
      return [[{
        json: {
          success: false,
          orderId,
          error: error instanceof Error ? error.message : 'Unknown error',
          broker,
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }
}
