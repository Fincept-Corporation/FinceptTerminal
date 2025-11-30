// File: src/types/hyperliquid.d.ts
// HyperLiquid exchange TypeScript type definitions
// Documentation: https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api

/**
 * HyperLiquid WebSocket subscription types
 */
export type HyperLiquidSubscriptionType =
  | 'allMids'
  | 'notification'
  | 'webData2'
  | 'webData3'
  | 'candle'
  | 'l2Book'
  | 'trades'
  | 'orderUpdates'
  | 'userEvents'
  | 'userFills'
  | 'userFundings'
  | 'userNonFundingLedgerUpdates'
  | 'userTwapSliceFills'
  | 'userTwapHistory'
  | 'activeAssetCtx'
  | 'activeAssetData'
  | 'twapStates'
  | 'clearinghouseState'
  | 'openOrders'
  | 'bbo';

/**
 * Candle intervals supported by HyperLiquid
 */
export type HyperLiquidCandleInterval =
  | '1m' | '3m' | '5m' | '15m' | '30m'
  | '1h' | '2h' | '4h' | '8h' | '12h'
  | '1d' | '3d' | '1w' | '1M';

/**
 * HyperLiquid WebSocket base message structure
 */
export interface HyperLiquidMessage {
  channel: string;
  data: any;
}

/**
 * HyperLiquid subscription request
 */
export interface HyperLiquidSubscriptionRequest {
  method: 'subscribe' | 'unsubscribe';
  subscription: {
    type: HyperLiquidSubscriptionType;
    coin?: string;
    user?: string;
    interval?: HyperLiquidCandleInterval;
    nSigFigs?: number; // 2-5 for l2Book
    mantissa?: 1 | 2 | 5; // Only with nSigFigs=5
    aggregateByTime?: boolean; // For userFills
  };
}

/**
 * HyperLiquid subscription response
 */
export interface HyperLiquidSubscriptionResponse {
  channel: 'subscriptionResponse';
  data: {
    method: 'subscribe' | 'unsubscribe';
    subscription: any;
  };
}

/**
 * All Mids (mid-price for all coins)
 */
export interface HyperLiquidAllMidsData {
  mids: Record<string, string>; // Coin -> mid price
}

/**
 * Level 2 Order Book
 */
export interface HyperLiquidL2BookLevel {
  px: string; // Price
  sz: string; // Size
  n: number;  // Number of orders
}

export interface HyperLiquidL2BookData {
  coin: string;
  time: number;
  levels: [HyperLiquidL2BookLevel[], HyperLiquidL2BookLevel[]]; // [bids, asks]
}

/**
 * Trade data
 */
export interface HyperLiquidTrade {
  coin: string;
  side: 'A' | 'B'; // Ask (sell) or Bid (buy)
  px: string;      // Price
  sz: string;      // Size
  time: number;    // Timestamp
  tid: number;     // Trade ID
  hash?: string;   // Transaction hash
}

export interface HyperLiquidTradesData {
  coin?: string;
  trades: HyperLiquidTrade[];
}

/**
 * Candle (OHLCV) data
 */
export interface HyperLiquidCandle {
  t: number;  // Timestamp (open time)
  T: number;  // Close timestamp
  s: string;  // Symbol
  i: HyperLiquidCandleInterval; // Interval
  o: string;  // Open
  c: string;  // Close
  h: string;  // High
  l: string;  // Low
  v: string;  // Volume
  n: number;  // Number of trades
}

export interface HyperLiquidCandleData {
  candle?: HyperLiquidCandle;
}

/**
 * BBO (Best Bid Offer)
 */
export interface HyperLiquidBBO {
  coin: string;
  bid: string;  // Best bid price
  ask: string;  // Best ask price
  time: number; // Timestamp
}

export interface HyperLiquidBBOData {
  bbo?: HyperLiquidBBO;
}

/**
 * User Fills
 */
export interface HyperLiquidUserFill {
  coin: string;
  px: string;      // Fill price
  sz: string;      // Fill size
  side: 'A' | 'B'; // Ask or Bid
  time: number;    // Timestamp
  startPosition: string;
  dir: string;     // Direction
  closedPnl: string;
  hash: string;
  oid: number;     // Order ID
  crossed: boolean;
  fee: string;
  tid: number;     // Trade ID
  builderFee?: string;
}

export interface HyperLiquidUserFillsData {
  isSnapshot: boolean;
  user: string;
  fills: HyperLiquidUserFill[];
}

/**
 * Open Orders
 */
export interface HyperLiquidOpenOrder {
  coin: string;
  side: 'A' | 'B';
  limitPx: string;
  sz: string;
  oid: number;
  timestamp: number;
  origSz: string;
  cloid?: string; // Client order ID
}

export interface HyperLiquidOpenOrdersData {
  user?: string;
  orders?: HyperLiquidOpenOrder[];
}

/**
 * User Fundings
 */
export interface HyperLiquidUserFunding {
  time: number;
  coin: string;
  usdc: string;
  szi: string;
  fundingRate: string;
}

export interface HyperLiquidUserFundingsData {
  isSnapshot: boolean;
  user: string;
  fundings: HyperLiquidUserFunding[];
}

/**
 * Clearinghouse State (User Position State)
 */
export interface HyperLiquidAssetPosition {
  type: 'oneWay';
  position: {
    coin: string;
    szi: string;          // Signed position size
    leverage: {
      type: 'cross' | 'isolated';
      value: number;
      rawUsd?: string;
    };
    entryPx: string | null;
    positionValue: string;
    unrealizedPnl: string;
    returnOnEquity: string;
    liquidationPx: string | null;
    marginUsed: string;
    maxTrade: string;
    cumFunding: {
      allTime: string;
      sinceOpen: string;
      sinceChange: string;
    };
  };
}

export interface HyperLiquidClearinghouseState {
  assetPositions: HyperLiquidAssetPosition[];
  crossMarginSummary: {
    accountValue: string;
    totalNtlPos: string;
    totalRawUsd: string;
    totalMarginUsed: string;
    withdrawable: string;
  };
  marginSummary: {
    accountValue: string;
    totalNtlPos: string;
    totalRawUsd: string;
    totalMarginUsed: string;
  };
  time: number;
}

/**
 * REST API Response Types
 */

export interface HyperLiquidCandleSnapshotRequest {
  type: 'candleSnapshot';
  req: {
    coin: string;
    interval: HyperLiquidCandleInterval;
    startTime: number;  // Milliseconds
    endTime?: number;   // Milliseconds
  };
}

export interface HyperLiquidAllMidsRequest {
  type: 'allMids';
  dex?: string; // Optional, defaults to first perp dex
}

export interface HyperLiquidL2BookSnapshotRequest {
  type: 'l2Book';
  coin: string;
  nSigFigs?: number;  // 2-5
  mantissa?: 1 | 2 | 5;
}

export interface HyperLiquidUserStateRequest {
  type: 'clearinghouseState';
  user: string; // Wallet address
}

export interface HyperLiquidOpenOrdersRequest {
  type: 'openOrders';
  user: string;
  dex?: string;
}

export interface HyperLiquidUserFillsRequest {
  type: 'userFills';
  user: string;
  aggregateByTime?: boolean;
}

export interface HyperLiquidUserFillsByTimeRequest {
  type: 'userFillsByTime';
  user: string;
  startTime: number;
  endTime?: number;
}

/**
 * Meta info for available assets
 */
export interface HyperLiquidAssetInfo {
  name: string;
  szDecimals: number;
  maxLeverage: number;
  onlyIsolated: boolean;
}

export interface HyperLiquidMetaRequest {
  type: 'meta';
}

export interface HyperLiquidMetaResponse {
  universe: HyperLiquidAssetInfo[];
}
