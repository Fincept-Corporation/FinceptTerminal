/**
 * Get Holdings Node
 * Fetches portfolio holdings (long-term positions) from broker
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

export class GetHoldingsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Get Holdings',
    name: 'getHoldings',
    icon: 'file:holdings.svg',
    group: ['trading'],
    version: 1,
    subtitle: '={{$parameter["broker"]}} holdings',
    description: 'Fetches portfolio holdings from broker',
    defaults: {
      name: 'Get Holdings',
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
          { name: 'All Connected', value: 'all' },
        ],
        description: 'Broker to fetch holdings from',
      },
      {
        displayName: 'Filter by Symbol',
        name: 'filterSymbol',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'AAPL, MSFT',
        description: 'Filter holdings by symbols (comma-separated, empty for all)',
      },
      {
        displayName: 'Include Market Value',
        name: 'includeMarketValue',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Calculate current market value',
      },
      {
        displayName: 'Include Sector Data',
        name: 'includeSector',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include sector and industry classification',
      },
      {
        displayName: 'Include Dividends',
        name: 'includeDividends',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Include dividend information',
      },
      {
        displayName: 'Sort By',
        name: 'sortBy',
        type: NodePropertyType.Options,
        default: 'value',
        options: [
          { name: 'Market Value (High to Low)', value: 'value' },
          { name: 'P&L (High to Low)', value: 'pnl' },
          { name: 'P&L % (High to Low)', value: 'pnlPercent' },
          { name: 'Symbol (A-Z)', value: 'symbol' },
          { name: 'Quantity (High to Low)', value: 'quantity' },
        ],
        description: 'How to sort the holdings',
      },
      {
        displayName: 'Output Mode',
        name: 'outputMode',
        type: NodePropertyType.Options,
        default: 'array',
        options: [
          { name: 'Array of Holdings', value: 'array' },
          { name: 'Portfolio Summary', value: 'summary' },
          { name: 'Both', value: 'both' },
        ],
        description: 'Output format',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const broker = this.getNodeParameter('broker', 0) as string;
    const filterSymbol = this.getNodeParameter('filterSymbol', 0) as string;
    const includeMarketValue = this.getNodeParameter('includeMarketValue', 0) as boolean;
    const includeSector = this.getNodeParameter('includeSector', 0) as boolean;
    const includeDividends = this.getNodeParameter('includeDividends', 0) as boolean;
    const sortBy = this.getNodeParameter('sortBy', 0) as string;
    const outputMode = this.getNodeParameter('outputMode', 0) as string;

    const isPaper = broker === 'paper';

    try {
      let holdings = await TradingBridge.getHoldings(broker as any);

      // Filter by symbol if specified
      if (filterSymbol) {
        const symbols = filterSymbol.split(',').map(s => s.trim().toUpperCase());
        holdings = holdings.filter(h => symbols.includes(h.symbol.toUpperCase()));
      }

      // Calculate market values if requested
      if (includeMarketValue) {
        for (const holding of holdings) {
          (holding as any).marketValue = holding.quantity * (holding.currentPrice || holding.averagePrice);
          (holding as any).costBasis = holding.quantity * holding.averagePrice;
          (holding as any).unrealizedPnl = (holding as any).marketValue - (holding as any).costBasis;
          (holding as any).unrealizedPnlPercent = (holding as any).costBasis !== 0
            ? (((holding as any).unrealizedPnl / (holding as any).costBasis) * 100).toFixed(2)
            : '0';
        }
      }

      // Add mock sector data if requested
      if (includeSector) {
        const sectorMap: Record<string, { sector: string; industry: string }> = {
          AAPL: { sector: 'Technology', industry: 'Consumer Electronics' },
          MSFT: { sector: 'Technology', industry: 'Software' },
          GOOGL: { sector: 'Technology', industry: 'Internet Content' },
          AMZN: { sector: 'Consumer Cyclical', industry: 'E-Commerce' },
          TSLA: { sector: 'Consumer Cyclical', industry: 'Auto Manufacturers' },
          JPM: { sector: 'Financial Services', industry: 'Banks' },
          JNJ: { sector: 'Healthcare', industry: 'Drug Manufacturers' },
        };

        for (const holding of holdings) {
          const sectorInfo = sectorMap[holding.symbol] || { sector: 'Unknown', industry: 'Unknown' };
          (holding as any).sector = sectorInfo.sector;
          (holding as any).industry = sectorInfo.industry;
        }
      }

      // Add mock dividend data if requested
      if (includeDividends) {
        for (const holding of holdings) {
          (holding as any).annualDividend = ((holding.currentPrice || holding.averagePrice) * 0.02).toFixed(2); // Mock 2% yield
          (holding as any).dividendYield = '2.00%';
          (holding as any).exDividendDate = null;
        }
      }

      // Sort holdings
      switch (sortBy) {
        case 'value':
          holdings.sort((a, b) => (b.currentValue || 0) - (a.currentValue || 0));
          break;
        case 'pnl':
          holdings.sort((a, b) => (b.pnl || 0) - (a.pnl || 0));
          break;
        case 'pnlPercent':
          holdings.sort((a, b) => (b.pnlPercent || 0) - (a.pnlPercent || 0));
          break;
        case 'symbol':
          holdings.sort((a, b) => a.symbol.localeCompare(b.symbol));
          break;
        case 'quantity':
          holdings.sort((a, b) => b.quantity - a.quantity);
          break;
      }

      // Calculate portfolio summary
      const totalValue = holdings.reduce((sum, h) => sum + (h.currentValue || 0), 0);
      const totalCost = holdings.reduce((sum, h) => sum + (h.investedValue || 0), 0);
      const totalPnl = totalValue - totalCost;

      // Calculate sector allocation
      const sectorAllocation: Record<string, number> = {};
      if (includeSector) {
        for (const holding of holdings) {
          const sector = (holding as any).sector || 'Unknown';
          sectorAllocation[sector] = (sectorAllocation[sector] || 0) + (holding.currentValue || 0);
        }
        // Convert to percentages
        for (const sector of Object.keys(sectorAllocation)) {
          sectorAllocation[sector] = parseFloat(((sectorAllocation[sector] / totalValue) * 100).toFixed(2));
        }
      }

      const summary = {
        totalHoldings: holdings.length,
        totalMarketValue: totalValue,
        totalCostBasis: totalCost,
        totalUnrealizedPnl: totalPnl,
        totalUnrealizedPnlPercent: totalCost !== 0 ? ((totalPnl / totalCost) * 100).toFixed(2) + '%' : '0%',
        profitableHoldings: holdings.filter(h => (h.pnl || 0) > 0).length,
        losingHoldings: holdings.filter(h => (h.pnl || 0) < 0).length,
        topHolding: holdings[0] ? { symbol: holdings[0].symbol, weight: ((holdings[0].currentValue || 0) / totalValue * 100).toFixed(2) + '%' } : null,
        sectorAllocation: includeSector ? sectorAllocation : undefined,
        broker,
        paperTrading: isPaper,
        fetchedAt: new Date().toISOString(),
      };

      // Calculate weights for each holding
      for (const holding of holdings) {
        (holding as any).portfolioWeight = totalValue > 0
          ? ((holding.currentValue || 0) / totalValue * 100).toFixed(2) + '%'
          : '0%';
      }

      // Return based on output mode
      switch (outputMode) {
        case 'summary':
          return [[{ json: summary }]];

        case 'both':
          return [[
            { json: { type: 'summary', ...summary } },
            ...holdings.map(h => ({ json: { type: 'holding', ...h } })),
          ]];

        case 'array':
        default:
          if (holdings.length === 0) {
            return [[{
              json: {
                message: 'No holdings found',
                broker,
                paperTrading: isPaper,
                fetchedAt: new Date().toISOString(),
              },
            }]];
          }
          return [holdings.map(h => ({ json: h as any }))];
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
