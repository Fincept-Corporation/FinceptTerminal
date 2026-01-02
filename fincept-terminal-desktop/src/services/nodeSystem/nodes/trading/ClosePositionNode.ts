/**
 * Close Position Node
 * Closes open positions partially or fully
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { TradingBridge, OrderType } from '../../adapters/TradingBridge';
import { RiskManager } from '../../safety/RiskManager';
import { AuditLogger } from '../../safety/AuditLogger';
import { ConfirmationService } from '../../safety/ConfirmationService';

export class ClosePositionNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Close Position',
    name: 'closePosition',
    icon: 'file:close-position.svg',
    group: ['trading'],
    version: 1,
    subtitle: 'Close {{$parameter["symbol"]}}',
    description: 'Closes open positions partially or fully',
    defaults: {
      name: 'Close Position',
      color: '#ef4444',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'AAPL',
        description: 'Symbol of position to close',
      },
      {
        displayName: 'Use Input Symbol',
        name: 'useInputSymbol',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbol from input data',
      },
      {
        displayName: 'Close Amount',
        name: 'closeAmount',
        type: NodePropertyType.Options,
        default: 'full',
        options: [
          { name: 'Close Entire Position', value: 'full' },
          { name: 'Close Percentage', value: 'percent' },
          { name: 'Close Specific Quantity', value: 'quantity' },
          { name: 'Close Dollar Amount', value: 'dollars' },
        ],
        description: 'How much of the position to close',
      },
      {
        displayName: 'Percentage',
        name: 'percentage',
        type: NodePropertyType.Number,
        default: 50,
        description: 'Percentage of position to close',
        displayOptions: {
          show: { closeAmount: ['percent'] },
        },
      },
      {
        displayName: 'Quantity',
        name: 'quantity',
        type: NodePropertyType.Number,
        default: 1,
        description: 'Number of shares to close',
        displayOptions: {
          show: { closeAmount: ['quantity'] },
        },
      },
      {
        displayName: 'Dollar Amount',
        name: 'dollarAmount',
        type: NodePropertyType.Number,
        default: 1000,
        description: 'Dollar amount to close',
        displayOptions: {
          show: { closeAmount: ['dollars'] },
        },
      },
      {
        displayName: 'Order Type',
        name: 'orderType',
        type: NodePropertyType.Options,
        default: 'MARKET',
        options: [
          { name: 'Market', value: 'MARKET' },
          { name: 'Limit', value: 'LIMIT' },
        ],
        description: 'Order type for closing',
      },
      {
        displayName: 'Limit Price',
        name: 'limitPrice',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Limit price for closing order',
        displayOptions: {
          show: { orderType: ['LIMIT'] },
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
        description: 'Broker to execute through',
      },
      {
        displayName: 'Require Confirmation',
        name: 'requireConfirmation',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Require user confirmation before closing',
      },
      {
        displayName: 'Close All Positions',
        name: 'closeAll',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Close all open positions (ignores symbol)',
      },
      {
        displayName: 'Only if Profitable',
        name: 'onlyProfitable',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Only close if position is profitable',
      },
      {
        displayName: 'Only if Loss Exceeds',
        name: 'onlyIfLoss',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Only close if loss exceeds threshold',
      },
      {
        displayName: 'Loss Threshold %',
        name: 'lossThreshold',
        type: NodePropertyType.Number,
        default: 5,
        description: 'Loss percentage threshold',
        displayOptions: {
          show: { onlyIfLoss: [true] },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputSymbol = this.getNodeParameter('useInputSymbol', 0) as boolean;
    const closeAmount = this.getNodeParameter('closeAmount', 0) as string;
    const orderType = this.getNodeParameter('orderType', 0) as OrderType;
    const broker = this.getNodeParameter('broker', 0) as string;
    const requireConfirmation = this.getNodeParameter('requireConfirmation', 0) as boolean;
    const closeAll = this.getNodeParameter('closeAll', 0) as boolean;
    const onlyProfitable = this.getNodeParameter('onlyProfitable', 0) as boolean;
    const onlyIfLoss = this.getNodeParameter('onlyIfLoss', 0) as boolean;

    const tradingBridge = new TradingBridge();
    const isPaper = broker === 'paper';

    try {
      // Get current positions
      const positions = await tradingBridge.getPositions(broker, isPaper);

      // Determine which positions to close
      let positionsToClose = positions;

      if (!closeAll) {
        let symbol: string;
        if (useInputSymbol) {
          const inputData = this.getInputData();
          symbol = inputData[0]?.json?.symbol as string || '';
        } else {
          symbol = this.getNodeParameter('symbol', 0) as string;
        }

        if (!symbol) {
          return [[{
            json: {
              success: false,
              error: 'No symbol provided',
              timestamp: new Date().toISOString(),
            },
          }]];
        }

        positionsToClose = positions.filter(p => p.symbol.toUpperCase() === symbol.toUpperCase());
      }

      // Filter by profit condition
      if (onlyProfitable) {
        positionsToClose = positionsToClose.filter(p => p.pnl > 0);
      }

      // Filter by loss condition
      if (onlyIfLoss) {
        const lossThreshold = this.getNodeParameter('lossThreshold', 0) as number;
        positionsToClose = positionsToClose.filter(p => {
          const pnlPercent = p.averagePrice > 0 ? ((p.lastPrice - p.averagePrice) / p.averagePrice) * 100 : 0;
          return pnlPercent < -lossThreshold;
        });
      }

      if (positionsToClose.length === 0) {
        return [[{
          json: {
            success: true,
            message: 'No positions to close matching criteria',
            broker,
            paperTrading: isPaper,
            timestamp: new Date().toISOString(),
          },
        }]];
      }

      const results: Array<{ json: Record<string, unknown> }> = [];

      for (const position of positionsToClose) {
        // Calculate quantity to close
        let closeQuantity: number;
        const positionQty = Math.abs(position.quantity);

        switch (closeAmount) {
          case 'percent': {
            const percentage = this.getNodeParameter('percentage', 0) as number;
            closeQuantity = Math.floor(positionQty * (percentage / 100));
            break;
          }
          case 'quantity': {
            const qty = this.getNodeParameter('quantity', 0) as number;
            closeQuantity = Math.min(qty, positionQty);
            break;
          }
          case 'dollars': {
            const dollarAmount = this.getNodeParameter('dollarAmount', 0) as number;
            closeQuantity = Math.floor(dollarAmount / position.lastPrice);
            closeQuantity = Math.min(closeQuantity, positionQty);
            break;
          }
          case 'full':
          default:
            closeQuantity = positionQty;
        }

        if (closeQuantity <= 0) {
          results.push({
            json: {
              success: false,
              symbol: position.symbol,
              error: 'Calculated close quantity is zero',
              timestamp: new Date().toISOString(),
            },
          });
          continue;
        }

        // Determine close side (opposite of position)
        const closeSide = position.quantity > 0 ? 'SELL' : 'BUY';
        const estimatedValue = closeQuantity * position.lastPrice;

        // Request confirmation if enabled
        if (requireConfirmation && !isPaper) {
          const confirmationService = new ConfirmationService();
          const confirmed = await confirmationService.requestTradeConfirmation({
            symbol: position.symbol,
            side: closeSide,
            quantity: closeQuantity,
            price: position.lastPrice,
            broker,
            estimatedValue,
            isClosing: true,
          });

          if (!confirmed) {
            results.push({
              json: {
                success: false,
                symbol: position.symbol,
                cancelled: true,
                reason: 'User cancelled',
                timestamp: new Date().toISOString(),
              },
            });
            continue;
          }
        }

        // Execute close order
        const limitPrice = orderType === 'LIMIT'
          ? this.getNodeParameter('limitPrice', 0) as number
          : undefined;

        const result = await tradingBridge.closePosition(
          position.symbol,
          closeQuantity,
          broker,
          isPaper
        );

        // Log the close
        const auditLogger = new AuditLogger();
        await auditLogger.logOrder({
          orderId: result.orderId,
          symbol: position.symbol,
          side: closeSide as any,
          quantity: closeQuantity,
          price: position.lastPrice,
          status: result.status,
          broker,
          paperTrading: isPaper,
          action: 'close_position',
        });

        // Calculate realized P&L
        const realizedPnl = (position.lastPrice - position.averagePrice) * closeQuantity * (position.quantity > 0 ? 1 : -1);

        results.push({
          json: {
            success: true,
            orderId: result.orderId,
            symbol: position.symbol,
            side: closeSide,
            quantityClosed: closeQuantity,
            remainingQuantity: positionQty - closeQuantity,
            price: position.lastPrice,
            realizedPnl,
            realizedPnlPercent: ((realizedPnl / (closeQuantity * position.averagePrice)) * 100).toFixed(2) + '%',
            status: result.status,
            broker,
            paperTrading: isPaper,
            timestamp: new Date().toISOString(),
          },
        });
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
}
