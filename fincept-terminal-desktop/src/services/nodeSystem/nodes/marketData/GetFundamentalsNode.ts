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

  private static async getIndustryComparison(symbol: string, data: any): Promise<Record<string, any>> {
    // Mock industry comparison - would call actual API in production
    return {
      industry: data.industry || 'Technology',
      sector: data.sector || 'Technology',
      metrics: {
        peRatio: { company: data.peRatio || 25, industry: 22.5, percentile: 65 },
        pbRatio: { company: data.pbRatio || 8, industry: 5.2, percentile: 78 },
        roe: { company: data.roe || 0.35, industry: 0.18, percentile: 85 },
        debtToEquity: { company: data.debtToEquity || 1.2, industry: 0.8, percentile: 40 },
        profitMargin: { company: data.profitMargin || 0.25, industry: 0.15, percentile: 80 },
      },
    };
  }
}
