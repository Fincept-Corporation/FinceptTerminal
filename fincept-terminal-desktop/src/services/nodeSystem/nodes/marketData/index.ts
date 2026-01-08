/**
 * Market Data Nodes Index
 * All market data fetching and streaming nodes
 */

export * from './GetQuoteNode';
export * from './GetHistoricalDataNode';
export * from './GetMarketDepthNode';
export * from './StreamQuotesNode';
export * from './GetFundamentalsNode';
export * from './GetTickerStatsNode';
export * from './YFinanceNode';

// Re-export for convenience
import { GetQuoteNode } from './GetQuoteNode';
import { GetHistoricalDataNode } from './GetHistoricalDataNode';
import { GetMarketDepthNode } from './GetMarketDepthNode';
import { StreamQuotesNode } from './StreamQuotesNode';
import { GetFundamentalsNode } from './GetFundamentalsNode';
import { GetTickerStatsNode } from './GetTickerStatsNode';
import { YFinanceNode } from './YFinanceNode';

export const MarketDataNodes = {
  GetQuoteNode,
  GetHistoricalDataNode,
  GetMarketDepthNode,
  StreamQuotesNode,
  GetFundamentalsNode,
  GetTickerStatsNode,
  YFinanceNode,
};

export const MarketDataNodeTypes = [
  'getQuote',
  'getHistoricalData',
  'getMarketDepth',
  'streamQuotes',
  'getFundamentals',
  'getTickerStats',
  'yfinance',
] as const;

export type MarketDataNodeType = typeof MarketDataNodeTypes[number];
