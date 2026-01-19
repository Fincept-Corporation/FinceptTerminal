/**
 * Interactive Brokers (IBKR) Data Mappers
 *
 * Functions to transform between standard types and IBKR API formats
 */

import type {
  OrderParams,
  Order,
  Position,
  Quote,
  OHLCV,
  OrderSide,
  OrderType,
  OrderStatus,
  ProductType,
  StockExchange,
} from '../../types';

import type {
  IBKROrder,
  IBKRPosition,
  IBKRMarketDataSnapshot,
  IBKRHistoricalBar,
  IBKROrderRequest,
  IBKROrderType,
  IBKRTimeInForce,
  IBKRSide,
} from './types';

// ============================================================================
// ORDER MAPPING
// ============================================================================

/**
 * Map standard order params to IBKR order request format
 */
export function toIBKROrderRequest(
  params: OrderParams,
  accountId: string,
  conid: number
): IBKROrderRequest {
  return {
    acctId: accountId,
    conid: conid,
    orderType: mapOrderTypeToIBKR(params.orderType) as IBKROrderType,
    side: params.side as IBKRSide,
    quantity: params.quantity,
    price: params.price,
    auxPrice: params.triggerPrice,
    tif: mapValidityToIBKR(params.validity) as IBKRTimeInForce,
    outsideRTH: false,
  };
}

/**
 * Map IBKR order to standard Order format
 */
export function fromIBKROrder(ibkrOrder: IBKROrder): Order {
  return {
    orderId: ibkrOrder.orderId?.toString() || '',
    symbol: ibkrOrder.ticker || '',
    exchange: (ibkrOrder.listingExchange || 'SMART') as StockExchange,
    side: mapIBKRSideToOrderSide(ibkrOrder.side),
    quantity: ibkrOrder.totalSize || 0,
    filledQuantity: ibkrOrder.filledQuantity || 0,
    pendingQuantity: (ibkrOrder.totalSize || 0) - (ibkrOrder.filledQuantity || 0),
    price: ibkrOrder.price || 0,
    averagePrice: ibkrOrder.avgPrice || 0,
    orderType: mapIBKROrderType(ibkrOrder.orderType),
    productType: 'CASH' as ProductType,
    validity: mapIBKRValidity(ibkrOrder.timeInForce),
    status: mapIBKRStatus(ibkrOrder.status),
    placedAt: new Date(ibkrOrder.lastExecutionTime_r || Date.now()),
  };
}

// ============================================================================
// POSITION MAPPING
// ============================================================================

/**
 * Map IBKR position to standard Position format
 */
export function fromIBKRPosition(ibkrPos: IBKRPosition): Position {
  const quantity = ibkrPos.position || 0;
  const avgPrice = ibkrPos.avgCost || 0;
  const lastPrice = ibkrPos.mktPrice || 0;
  const pnl = ibkrPos.unrealizedPnl || 0;
  const pnlPercent = avgPrice !== 0 ? (pnl / (Math.abs(quantity) * avgPrice)) * 100 : 0;

  return {
    symbol: ibkrPos.contractDesc || '',
    exchange: 'SMART' as StockExchange,
    productType: 'CASH' as ProductType,
    quantity: quantity,
    buyQuantity: quantity > 0 ? Math.abs(quantity) : 0,
    sellQuantity: quantity < 0 ? Math.abs(quantity) : 0,
    buyValue: quantity > 0 ? Math.abs(quantity) * avgPrice : 0,
    sellValue: quantity < 0 ? Math.abs(quantity) * avgPrice : 0,
    averagePrice: avgPrice,
    lastPrice: lastPrice,
    pnl: pnl,
    pnlPercent: pnlPercent,
    dayPnl: ibkrPos.realizedPnl || 0,
  };
}

// ============================================================================
// MARKET DATA MAPPING
// ============================================================================

/**
 * Map IBKR snapshot to standard Quote format
 */
export function fromIBKRSnapshot(
  conid: string,
  snapshot: IBKRMarketDataSnapshot[string]
): Quote {
  // IBKR field mappings
  // 31 = Last Price, 84 = Bid, 86 = Ask, 85 = Ask Size, 88 = Bid Size
  // 82 = Change, 83 = Change %, 87 = Volume
  // 70 = High, 71 = Low, 7294 = Open, 7762 = Prior Close
  return {
    symbol: snapshot['55'] || conid,
    exchange: 'SMART' as StockExchange,
    lastPrice: parseFloat(snapshot['31'] || '0'),
    open: parseFloat(snapshot['7294'] || '0'),
    high: parseFloat(snapshot['70'] || '0'),
    low: parseFloat(snapshot['71'] || '0'),
    close: parseFloat(snapshot['31'] || '0'),
    previousClose: parseFloat(snapshot['7762'] || '0'),
    change: parseFloat(snapshot['82'] || '0'),
    changePercent: parseFloat(snapshot['83'] || '0'),
    volume: parseInt(snapshot['87'] || '0'),
    bid: parseFloat(snapshot['84'] || '0'),
    bidQty: parseInt(snapshot['88'] || '0'),
    ask: parseFloat(snapshot['86'] || '0'),
    askQty: parseInt(snapshot['85'] || '0'),
    timestamp: Date.now(),
  };
}

/**
 * Map IBKR historical bars to OHLCV format
 */
export function fromIBKRHistoricalBars(bars: IBKRHistoricalBar[]): OHLCV[] {
  return bars.map(bar => ({
    timestamp: bar.t * 1000,
    open: bar.o,
    high: bar.h,
    low: bar.l,
    close: bar.c,
    volume: bar.v,
  }));
}

/**
 * Map IBKR contract search to instrument format
 */
export function fromIBKRContract(contract: {
  conid: number;
  symbol: string;
  companyName?: string;
  exchange?: string;
  secType?: string;
}) {
  return {
    symbol: contract.symbol,
    name: contract.companyName || contract.symbol,
    exchange: contract.exchange || 'SMART',
    type: contract.secType || 'STK',
    token: contract.conid.toString(),
  };
}

/**
 * Map IBKR account summary
 */
export function fromIBKRAccountSummary(summary: Record<string, { amount: number; currency: string }>) {
  return {
    buyingPower: summary.buyingpower?.amount || 0,
    cash: summary.totalcashvalue?.amount || 0,
    equity: summary.netliquidation?.amount || 0,
    portfolioValue: summary.netliquidation?.amount || 0,
    marginUsed: summary.initmarginreq?.amount || 0,
    marginAvailable: summary.availablefunds?.amount || 0,
    dayTradesRemaining: summary.daytradesremaining?.amount || 999,
    unrealizedPnl: summary.unrealizedpnl?.amount || 0,
    realizedPnl: summary.realizedpnl?.amount || 0,
  };
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Check if account is paper trading
 */
export function isPaperAccount(accountId: string): boolean {
  return accountId.startsWith('DU');
}

/**
 * Map timeframe to IBKR bar size
 */
export function toIBKRBarSize(interval: string): string {
  const mapping: Record<string, string> = {
    '1m': '1min',
    '5m': '5mins',
    '15m': '15mins',
    '30m': '30mins',
    '1h': '1hour',
    '4h': '4hours',
    '1d': '1day',
    '1w': '1week',
    '1M': '1month',
  };
  return mapping[interval] || '1day';
}

/**
 * Map duration string to IBKR format
 */
export function toIBKRDuration(duration: string): string {
  const mapping: Record<string, string> = {
    '1d': '1 D',
    '1w': '1 W',
    '1M': '1 M',
    '3M': '3 M',
    '6M': '6 M',
    '1Y': '1 Y',
  };
  return mapping[duration] || '1 Y';
}

/**
 * Build fields parameter for market data request
 */
export function buildFieldsParam(fields: number[]): string {
  return fields.join(',');
}

// ============================================================================
// PRIVATE MAPPING FUNCTIONS
// ============================================================================

function mapOrderTypeToIBKR(orderType: OrderType): string {
  const mapping: Record<OrderType, string> = {
    'MARKET': 'MKT',
    'LIMIT': 'LMT',
    'STOP': 'STP',
    'STOP_LIMIT': 'STP LMT',
    'TRAILING_STOP': 'TRAIL',
  };
  return mapping[orderType] || 'MKT';
}

function mapIBKROrderType(ibkrType?: string): OrderType {
  const mapping: Record<string, OrderType> = {
    'MKT': 'MARKET',
    'LMT': 'LIMIT',
    'STP': 'STOP',
    'STP LMT': 'STOP_LIMIT',
    'TRAIL': 'TRAILING_STOP',
  };
  return mapping[ibkrType || ''] || 'MARKET';
}

function mapValidityToIBKR(validity?: string): string {
  const mapping: Record<string, string> = {
    'DAY': 'DAY',
    'GTC': 'GTC',
    'IOC': 'IOC',
    'GTD': 'GTD',
  };
  return mapping[validity || 'DAY'] || 'DAY';
}

function mapIBKRValidity(tif?: string): 'DAY' | 'GTC' | 'IOC' | 'GTD' {
  const mapping: Record<string, 'DAY' | 'GTC' | 'IOC' | 'GTD'> = {
    'DAY': 'DAY',
    'GTC': 'GTC',
    'IOC': 'IOC',
    'GTD': 'GTD',
  };
  return mapping[tif || ''] || 'DAY';
}

function mapIBKRSideToOrderSide(side?: IBKRSide): OrderSide {
  return side === 'BUY' ? 'BUY' : 'SELL';
}

function mapIBKRStatus(status?: string): OrderStatus {
  const mapping: Record<string, OrderStatus> = {
    'PreSubmitted': 'PENDING',
    'Submitted': 'OPEN',
    'Filled': 'FILLED',
    'Cancelled': 'CANCELLED',
    'Inactive': 'CANCELLED',
    'PendingSubmit': 'PENDING',
    'PendingCancel': 'PENDING',
  };
  return mapping[status || ''] || 'PENDING';
}
