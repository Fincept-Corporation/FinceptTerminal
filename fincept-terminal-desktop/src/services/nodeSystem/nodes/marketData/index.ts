/**
 * Market Data Nodes Index
 * All market data fetching nodes
 */

export * from './GetQuoteNode';
export * from './GetMarketDepthNode';
export * from './GetFundamentalsNode';
export * from './GetTickerStatsNode';

import { GetQuoteNode } from './GetQuoteNode';
import { GetMarketDepthNode } from './GetMarketDepthNode';
import { GetFundamentalsNode } from './GetFundamentalsNode';
import { GetTickerStatsNode } from './GetTickerStatsNode';

export const MarketDataNodes = {
  GetQuoteNode,
  GetMarketDepthNode,
  GetFundamentalsNode,
  GetTickerStatsNode,
};

export const MarketDataNodeTypes = [
  'getQuote',
  'getMarketDepth',
  'getFundamentals',
  'getTickerStats',
] as const;

export type MarketDataNodeType = typeof MarketDataNodeTypes[number];
