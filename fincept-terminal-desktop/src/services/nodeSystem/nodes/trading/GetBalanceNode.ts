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
    displayName: 'Get Balance',
    name: 'getBalance',
    icon: 'file:balance.svg',
    group: ['trading'],
    version: 1,
    subtitle: '={{$parameter["broker"]}} balance',
    description: 'Fetches account balance and buying power',
    defaults: {
      name: 'Get Balance',
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

    const tradingBridge = new TradingBridge();
    const isPaper = broker === 'paper';

    try {
      const balance = await tradingBridge.getBalance(broker, isPaper);

      const result: Record<string, unknown> = {
        broker,
        paperTrading: isPaper,
        currency,

        // Core balance info
        cashBalance: balance.cashBalance,
        availableBalance: balance.availableBalance,
        buyingPower: balance.buyingPower,

        // Portfolio value
        portfolioValue: balance.portfolioValue,
        totalEquity: balance.totalEquity,

        fetchedAt: new Date().toISOString(),
      };

      // Add margin details if requested
      if (includeMargin) {
        result.margin = {
          usedMargin: balance.usedMargin || 0,
          availableMargin: balance.availableMargin || balance.availableBalance,
          marginLevel: balance.marginLevel || 100,
          leverage: balance.leverage || 1,
          maintenanceMargin: balance.maintenanceMargin || 0,
          marginCallLevel: balance.marginCallLevel || 50,
          isMarginCall: balance.marginLevel ? balance.marginLevel < (balance.marginCallLevel || 50) : false,
        };
      }

      // Add P&L summary if requested
      if (includePnl) {
        result.pnl = {
          unrealizedPnl: balance.unrealizedPnl || 0,
          realizedPnlToday: balance.realizedPnlToday || 0,
          realizedPnlWeek: balance.realizedPnlWeek || 0,
          realizedPnlMonth: balance.realizedPnlMonth || 0,
          realizedPnlYear: balance.realizedPnlYear || 0,
          returnToday: balance.returnToday || '0%',
          returnWeek: balance.returnWeek || '0%',
          returnMonth: balance.returnMonth || '0%',
          returnYear: balance.returnYear || '0%',
        };
      }

      // Add crypto-specific fields if applicable
      if (['binance', 'kraken', 'coinbase'].includes(broker)) {
        result.cryptoBalances = balance.cryptoBalances || {};
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
