/**
 * Get Fundamentals Node
 * Fetches company fundamental data (financials, ratios, metrics)
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { MarketDataBridge } from '../../adapters/MarketDataBridge';

export class GetFundamentalsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Company Data',
    name: 'getFundamentals',
    icon: 'file:fundamentals.svg',
    group: ['marketData'],
    version: 1,
    subtitle: '={{$parameter["symbol"]}} {{$parameter["dataType"]}}',
    description: 'Fetches company fundamental data and financial metrics',
    defaults: {
      name: 'Company Data',
      color: '#10b981',
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
        description: 'Stock symbol to fetch fundamentals for',
      },
      {
        displayName: 'Use Input Symbol',
        name: 'useInputSymbol',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbol from input data',
      },
      {
        displayName: 'Data Type',
        name: 'dataType',
        type: NodePropertyType.Options,
        default: 'overview',
        options: [
          { name: 'Company Overview', value: 'overview', description: 'Basic company info and key metrics' },
          { name: 'Income Statement', value: 'income', description: 'Revenue, expenses, profit' },
          { name: 'Balance Sheet', value: 'balance', description: 'Assets, liabilities, equity' },
          { name: 'Cash Flow', value: 'cashflow', description: 'Operating, investing, financing' },
          { name: 'Key Ratios', value: 'ratios', description: 'P/E, P/B, ROE, etc.' },
          { name: 'Earnings', value: 'earnings', description: 'Historical and estimated earnings' },
          { name: 'Dividends', value: 'dividends', description: 'Dividend history and yield' },
          { name: 'Analyst Ratings', value: 'ratings', description: 'Analyst recommendations' },
          { name: 'Insider Transactions', value: 'insiders', description: 'Insider buying/selling' },
          { name: 'Institutional Holders', value: 'institutional', description: 'Major institutional holders' },
          { name: 'SEC Filings', value: 'filings', description: 'Recent SEC filings' },
          { name: 'All Data', value: 'all', description: 'Comprehensive fundamental data' },
        ],
        description: 'Type of fundamental data to fetch',
      },
      {
        displayName: 'Period',
        name: 'period',
        type: NodePropertyType.Options,
        default: 'annual',
        options: [
          { name: 'Annual', value: 'annual' },
          { name: 'Quarterly', value: 'quarterly' },
          { name: 'TTM (Trailing Twelve Months)', value: 'ttm' },
        ],
        description: 'Reporting period for financial data',
        displayOptions: {
          show: { dataType: ['income', 'balance', 'cashflow', 'ratios', 'earnings'] },
        },
      },
      {
        displayName: 'Limit',
        name: 'limit',
        type: NodePropertyType.Number,
        default: 4,
        description: 'Number of historical periods to fetch',
        displayOptions: {
          show: { dataType: ['income', 'balance', 'cashflow', 'earnings', 'dividends', 'filings'] },
        },
      },
      {
        displayName: 'Data Provider',
        name: 'provider',
        type: NodePropertyType.Options,
        default: 'yahoo',
        options: [
          { name: 'Yahoo Finance', value: 'yahoo' },
          { name: 'Alpha Vantage', value: 'alphavantage' },
          { name: 'Financial Modeling Prep', value: 'fmp' },
          { name: 'IEX Cloud', value: 'iex' },
          { name: 'Finnhub', value: 'finnhub' },
          { name: 'SEC EDGAR', value: 'sec' },
        ],
        description: 'Fundamental data provider',
      },
      {
        displayName: 'Include Growth Rates',
        name: 'includeGrowth',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Calculate YoY and QoQ growth rates',
        displayOptions: {
          show: { dataType: ['income', 'balance', 'cashflow', 'ratios'] },
        },
      },
      {
        displayName: 'Include Industry Comparison',
        name: 'includeIndustry',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Compare metrics against industry averages',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputSymbol = this.getNodeParameter('useInputSymbol', 0) as boolean;
    const dataType = this.getNodeParameter('dataType', 0) as string;
    const provider = this.getNodeParameter('provider', 0) as string;
    const includeIndustry = this.getNodeParameter('includeIndustry', 0) as boolean;

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
          error: 'No symbol provided',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    

    try {
      const fundamentals = await MarketDataBridge.getFundamentals(symbol);

      const result: Record<string, any> = {
        dataType,
        provider,
        ...fundamentals,
        fetchedAt: new Date().toISOString(),
      };

      // Add industry comparison if requested
      if (includeIndustry && fundamentals.industry) {
        result.industryComparison = await GetFundamentalsNode.getIndustryComparison(symbol, fundamentals);
      }

      return [[{ json: result }]];
    } catch (error) {
      return [[{
        json: {
          symbol,
          dataType,
          error: error instanceof Error ? error.message : 'Unknown error',
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }

  /**
   * Compare company metrics against sector peers using real data.
   * Fetches fundamentals for well-known sector peers and computes
   * the company's percentile rank for key ratios.
   */
  private static async getIndustryComparison(symbol: string, data: any): Promise<Record<string, any>> {
    // Select a set of sector peers to build a comparison baseline
    const sectorPeers: Record<string, string[]> = {
      'Technology': ['AAPL', 'MSFT', 'GOOGL', 'META', 'NVDA', 'CRM', 'ADBE', 'INTC', 'ORCL'],
      'Healthcare': ['JNJ', 'UNH', 'PFE', 'ABBV', 'MRK', 'TMO', 'ABT', 'LLY'],
      'Financial Services': ['JPM', 'BAC', 'WFC', 'GS', 'MS', 'C', 'BLK', 'SCHW'],
      'Consumer Cyclical': ['AMZN', 'TSLA', 'HD', 'NKE', 'MCD', 'SBUX', 'TGT', 'LOW'],
      'Energy': ['XOM', 'CVX', 'COP', 'SLB', 'EOG', 'OXY', 'PSX', 'VLO'],
    };

    const sector = data.sector || 'Technology';
    const peers = (sectorPeers[sector] || sectorPeers['Technology']).filter(s => s !== symbol.toUpperCase());

    // Fetch peer fundamentals in parallel (best-effort)
    const peerResults = await Promise.allSettled(
      peers.slice(0, 6).map(p => MarketDataBridge.getFundamentals(p))
    );

    const peerData = peerResults
      .filter((r): r is PromiseFulfilledResult<any> => r.status === 'fulfilled')
      .map(r => r.value);

    // Helper: compute percentile rank of value in an array
    const percentile = (value: number | undefined, arr: (number | undefined)[]): number | null => {
      if (value == null) return null;
      const valid = arr.filter((v): v is number => v != null);
      if (valid.length === 0) return null;
      const below = valid.filter(v => v < value).length;
      return Math.round((below / valid.length) * 100);
    };

    const peerPEs = peerData.map(p => p.peRatio);
    const peerDivYields = peerData.map(p => p.dividendYield);
    const peerBetas = peerData.map(p => p.beta);

    const avg = (arr: (number | undefined)[]): number | null => {
      const valid = arr.filter((v): v is number => v != null);
      return valid.length > 0 ? valid.reduce((a, b) => a + b, 0) / valid.length : null;
    };

    return {
      industry: data.industry || 'Unknown',
      sector,
      peerCount: peerData.length,
      peers: peerData.map(p => p.symbol),
      metrics: {
        peRatio: {
          company: data.peRatio ?? null,
          sectorAvg: avg(peerPEs),
          percentile: percentile(data.peRatio, peerPEs),
        },
        dividendYield: {
          company: data.dividendYield ?? null,
          sectorAvg: avg(peerDivYields),
          percentile: percentile(data.dividendYield, peerDivYields),
        },
        beta: {
          company: data.beta ?? null,
          sectorAvg: avg(peerBetas),
          percentile: percentile(data.beta, peerBetas),
        },
        marketCap: {
          company: data.marketCap ?? null,
        },
      },
    };
  }
}
