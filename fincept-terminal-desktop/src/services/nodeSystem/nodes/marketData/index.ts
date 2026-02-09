/**
 * Market Data Nodes Index
 * All market data fetching and streaming nodes
 */

export * from './GetQuoteNode';
export * from './GetMarketDepthNode';
export * from './StreamQuotesNode';
export * from './GetFundamentalsNode';
export * from './GetTickerStatsNode';

// Re-export for convenience
import { GetQuoteNode } from './GetQuoteNode';
import { GetMarketDepthNode } from './GetMarketDepthNode';
import { StreamQuotesNode } from './StreamQuotesNode';
import { GetFundamentalsNode } from './GetFundamentalsNode';
import { GetTickerStatsNode } from './GetTickerStatsNode';

export const MarketDataNodes = {
  GetQuoteNode,
  GetMarketDepthNode,
  StreamQuotesNode,
  GetFundamentalsNode,
  GetTickerStatsNode,
};

export const MarketDataNodeTypes = [
  'getQuote',
  'getMarketDepth',
  'streamQuotes',
  'getFundamentals',
  'getTickerStats',
] as const;

export type MarketDataNodeType = typeof MarketDataNodeTypes[number];
