/**
 * Get Positions Node
 * Fetches current open positions from broker
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

export class GetPositionsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'My Positions',
    name: 'getPositions',
    icon: 'file:positions.svg',
    group: ['trading'],
    version: 1,
    subtitle: '={{$parameter["broker"]}} positions',
    description: 'Fetches current open positions from broker',
    defaults: {
      name: 'My Positions',
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
        description: 'Broker to fetch positions from',
      },
      {
        displayName: 'Filter by Symbol',
        name: 'filterSymbol',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'AAPL, MSFT',
        description: 'Filter positions by symbols (comma-separated, empty for all)',
      },
      {
        displayName: 'Position Type',
        name: 'positionType',
        type: NodePropertyType.Options,
        default: 'all',
        options: [
          { name: 'All Positions', value: 'all' },
          { name: 'Long Only', value: 'long' },
          { name: 'Short Only', value: 'short' },
          { name: 'In Profit', value: 'profit' },
          { name: 'In Loss', value: 'loss' },
        ],
        description: 'Filter positions by type',
      },
      {
        displayName: 'Include Closed',
        name: 'includeClosed',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Include positions closed today',
      },
      {
        displayName: 'Include Market Value',
        name: 'includeMarketValue',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Calculate current market value',
      },
      {
        displayName: 'Include Greeks',
        name: 'includeGreeks',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Include options Greeks for options positions',
      },
      {
        displayName: 'Output Mode',
        name: 'outputMode',
        type: NodePropertyType.Options,
        default: 'array',
        options: [
          { name: 'Array of Positions', value: 'array' },
          { name: 'Summary Object', value: 'summary' },
          { name: 'Both', value: 'both' },
        ],
        description: 'Output format',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const broker = this.getNodeParameter('broker', 0) as string;
    const filterSymbol = this.getNodeParameter('filterSymbol', 0) as string;
    const positionType = this.getNodeParameter('positionType', 0) as string;
    const includeClosed = this.getNodeParameter('includeClosed', 0) as boolean;
    const includeMarketValue = this.getNodeParameter('includeMarketValue', 0) as boolean;
    const outputMode = this.getNodeParameter('outputMode', 0) as string;

    const isPaper = broker === 'paper';

    try{
      let positions = await TradingBridge.getPositions(broker as any);

      // Filter by symbol if specified
      if (filterSymbol) {
        const symbols = filterSymbol.split(',').map(s => s.trim().toUpperCase());
        positions = positions.filter(p => symbols.includes(p.symbol.toUpperCase()));
      }

      // Filter by position type
      switch (positionType) {
        case 'long':
          positions = positions.filter(p => p.quantity > 0);
          break;
        case 'short':
          positions = positions.filter(p => p.quantity < 0);
          break;
        case 'profit':
          positions = positions.filter(p => (p.pnl || 0) > 0);
          break;
        case 'loss':
          positions = positions.filter(p => (p.pnl || 0) < 0);
          break;
      }

      // Filter closed positions
      if (!includeClosed) {
        positions = positions.filter(p => Math.abs(p.quantity) > 0);
      }

      // Calculate market values if requested
      if (includeMarketValue) {
        for (const pos of positions) {
          (pos as any).marketValue = pos.quantity * (pos.currentPrice || pos.averagePrice);
          (pos as any).costBasis = pos.quantity * pos.averagePrice;
          (pos as any).unrealizedPnl = (pos as any).marketValue - (pos as any).costBasis;
          (pos as any).unrealizedPnlPercent = (pos as any).costBasis !== 0
            ? (((pos as any).unrealizedPnl / (pos as any).costBasis) * 100).toFixed(2) + '%'
            : '0%';
        }
      }

      // Build summary
      const summary = {
        totalPositions: positions.length,
        longPositions: positions.filter(p => p.quantity > 0).length,
        shortPositions: positions.filter(p => p.quantity < 0).length,
        totalMarketValue: positions.reduce((sum, p) => sum + ((p as any).marketValue || 0), 0),
        totalCostBasis: positions.reduce((sum, p) => sum + ((p as any).costBasis || 0), 0),
        totalUnrealizedPnl: positions.reduce((sum, p) => sum + ((p as any).unrealizedPnl || 0), 0),
        profitablePositions: positions.filter(p => ((p as any).unrealizedPnl || 0) > 0).length,
        losingPositions: positions.filter(p => ((p as any).unrealizedPnl || 0) < 0).length,
        largestPosition: positions.reduce((max, p) =>
          Math.abs((p as any).marketValue || 0) > Math.abs((max as any)?.marketValue || 0) ? p : max, positions[0]) as any,
        broker,
        paperTrading: isPaper,
        fetchedAt: new Date().toISOString(),
      };

      // Return based on output mode
      switch (outputMode) {
        case 'summary':
          return [[{ json: summary }]];

        case 'both':
          return [[
            { json: { type: 'summary', ...summary } },
            ...positions.map(p => ({ json: { type: 'position', ...p } })),
          ]];

        case 'array':
        default:
          if (positions.length === 0) {
            return [[{
              json: {
                message: 'No positions found',
                broker,
                paperTrading: isPaper,
                filters: { symbol: filterSymbol, type: positionType },
                fetchedAt: new Date().toISOString(),
              },
            }]];
          }
          return [positions.map(p => ({ json: p as any }))];
      }
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
