/**
 * Get Balance Node
 * Fetches account balance and buying power from broker
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

export class GetBalanceNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Account Balance',
    name: 'getBalance',
    icon: 'file:balance.svg',
    group: ['trading'],
    version: 1,
    subtitle: '={{$parameter["broker"]}} balance',
    description: 'Fetches account balance and buying power',
    defaults: {
      name: 'Account Balance',
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
        description: 'Broker to fetch balance from',
      },
      {
        displayName: 'Include Margin Details',
        name: 'includeMargin',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include margin and leverage details',
      },
      {
        displayName: 'Include P&L Summary',
        name: 'includePnl',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include profit/loss summary',
      },
      {
        displayName: 'Currency',
        name: 'currency',
        type: NodePropertyType.Options,
        default: 'USD',
        options: [
          { name: 'US Dollar (USD)', value: 'USD' },
          { name: 'Indian Rupee (INR)', value: 'INR' },
          { name: 'Euro (EUR)', value: 'EUR' },
          { name: 'British Pound (GBP)', value: 'GBP' },
          { name: 'Bitcoin (BTC)', value: 'BTC' },
          { name: 'Ethereum (ETH)', value: 'ETH' },
        ],
        description: 'Display currency',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const broker = this.getNodeParameter('broker', 0) as string;
    const includeMargin = this.getNodeParameter('includeMargin', 0) as boolean;
    const includePnl = this.getNodeParameter('includePnl', 0) as boolean;
    const currency = this.getNodeParameter('currency', 0) as string;

    const isPaper = broker === 'paper';

    try {
      const balance = await TradingBridge.getBalance(broker as any, currency);

      const result: Record<string, any> = {
        broker,
        paperTrading: isPaper,
        currency,

        // Core balance info
        available: balance.available,
        used: balance.used,
        total: balance.total,

        fetchedAt: new Date().toISOString(),
      };

      // Add margin details if requested
      if (includeMargin) {
        result.margin = {
          usedMargin: balance.marginUsed || 0,
          availableMargin: balance.margin || balance.available,
          marginLevel: 100,
          leverage: 1,
          maintenanceMargin: 0,
          marginCallLevel: 50,
          isMarginCall: false,
        };
      }

      // Add P&L summary if requested
      if (includePnl) {
        result.pnl = {
          unrealizedPnl: 0,
          realizedPnlToday: 0,
          realizedPnlWeek: 0,
          realizedPnlMonth: 0,
          realizedPnlYear: 0,
          returnToday: '0%',
          returnWeek: '0%',
          returnMonth: '0%',
          returnYear: '0%',
        };
      }

      // Add crypto-specific fields if applicable
      if (['binance', 'kraken', 'coinbase'].includes(broker)) {
        result.cryptoBalances = {};
      }

      return [[{ json: result }]];
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
