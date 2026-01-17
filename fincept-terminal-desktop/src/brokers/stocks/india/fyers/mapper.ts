/**
 * Fyers Data Mappers
 *
 * Transform between Fyers API format and standard stock broker format
 */

import type {
  Order,
  Trade,
  Position,
  Holding,
  Funds,
  Quote,
  OHLCV,
  MarketDepth,
  DepthLevel,
  OrderParams,
  ModifyOrderParams,
  TimeFrame,
  OrderStatus,
  OrderSide,
  OrderType,
  ProductType,
  StockExchange,
  TickData,
  SubscriptionMode,
} from '../../types';

import {
  FYERS_EXCHANGE_MAP,
  FYERS_PRODUCT_MAP,
  FYERS_PRODUCT_REVERSE_MAP,
  FYERS_ORDER_TYPE_MAP,
  FYERS_ORDER_TYPE_REVERSE_MAP,
  FYERS_SIDE_MAP,
  FYERS_SIDE_REVERSE_MAP,
  FYERS_STATUS_MAP,
  FYERS_VALIDITY_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to Fyers format
 */
export function toFyersOrderParams(params: OrderParams): Record<string, unknown> {
  const orderType = FYERS_ORDER_TYPE_MAP[params.orderType] || 2; // Default to Market
  const side = FYERS_SIDE_MAP[params.side] || 1; // Default to BUY

  // Build Fyers symbol format: EXCHANGE:SYMBOL-INSTRUMENTTYPE
  // Use toFyersSymbol() to ensure correct format (e.g., NSE:RELIANCE-EQ)
  const fyersSymbol = toFyersSymbol(params.symbol, params.exchange);

  return {
    symbol: fyersSymbol,
    qty: params.quantity,
    type: orderType,
    side: side,
    productType: FYERS_PRODUCT_MAP[params.productType] || 'CNC',
    limitPrice: params.price || 0,
    stopPrice: params.triggerPrice || 0,
    validity: FYERS_VALIDITY_MAP[params.validity || 'DAY'] || 'DAY',
    disclosedQty: params.disclosedQuantity || 0,
    offlineOrder: false,
  };
}

/**
 * Convert modify order params to Fyers format
 */
export function toFyersModifyParams(orderId: string, params: ModifyOrderParams): Record<string, unknown> {
  const fyersParams: Record<string, unknown> = {
    id: orderId,
  };

  if (params.quantity !== undefined) {
    fyersParams.qty = params.quantity;
  }
  if (params.price !== undefined) {
    fyersParams.limitPrice = params.price;
  }
  if (params.triggerPrice !== undefined) {
    fyersParams.stopPrice = params.triggerPrice;
  }
  if (params.orderType !== undefined) {
    fyersParams.type = FYERS_ORDER_TYPE_MAP[params.orderType] || 2;
  }

  return fyersParams;
}

/**
 * Convert Fyers order to standard format
 */
export function fromFyersOrder(order: Record<string, unknown>): Order {
  // Parse symbol (format: EXCHANGE:SYMBOL)
  const symbolStr = String(order.symbol || '');
  const [exchange, symbol] = symbolStr.includes(':') ? symbolStr.split(':') : ['NSE', symbolStr];

  // Map status code to string
  const statusCode = Number(order.status || 6);
  const status = FYERS_STATUS_MAP[statusCode] || 'PENDING';

  // Map side (-1 = Sell, 1 = Buy)
  const sideCode = Number(order.side || 1);
  const side = FYERS_SIDE_REVERSE_MAP[sideCode] || 'BUY';

  // Map order type
  const typeCode = Number(order.type || 2);
  const orderType = FYERS_ORDER_TYPE_REVERSE_MAP[typeCode] || 'MARKET';

  return {
    orderId: String(order.id || ''),
    symbol,
    exchange: exchange as StockExchange,
    side: side as OrderSide,
    quantity: Number(order.qty || 0),
    filledQuantity: Number(order.filledQty || 0),
    pendingQuantity: Number(order.qty || 0) - Number(order.filledQty || 0),
    price: Number(order.limitPrice || 0),
    averagePrice: Number(order.tradedPrice || order.limitPrice || 0),
    triggerPrice: order.stopPrice ? Number(order.stopPrice) : undefined,
    orderType: orderType as OrderType,
    productType: (FYERS_PRODUCT_REVERSE_MAP[String(order.productType)] || 'CASH') as ProductType,
    validity: String(order.validity || 'DAY') as 'DAY' | 'IOC' | 'GTC' | 'GTD',
    status: status as OrderStatus,
    statusMessage: order.message ? String(order.message) : undefined,
    placedAt: order.orderDateTime ? new Date(order.orderDateTime as string) : new Date(),
    updatedAt: order.orderDateTime ? new Date(order.orderDateTime as string) : undefined,
    exchangeOrderId: order.exchOrdId ? String(order.exchOrdId) : undefined,
    tag: order.clientId ? String(order.clientId) : undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert Fyers trade to standard format
 */
export function fromFyersTrade(trade: Record<string, unknown>): Trade {
  // Parse symbol (format: EXCHANGE:SYMBOL)
  const symbolStr = String(trade.symbol || '');
  const [exchange, symbol] = symbolStr.includes(':') ? symbolStr.split(':') : ['NSE', symbolStr];

  // Map side
  const sideCode = Number(trade.side || 1);
  const side = FYERS_SIDE_REVERSE_MAP[sideCode] || 'BUY';

  return {
    tradeId: String(trade.tradeNumber || trade.id || ''),
    orderId: String(trade.id || ''),
    symbol,
    exchange: exchange as StockExchange,
    side: side as OrderSide,
    quantity: Number(trade.tradedQty || trade.qty || 0),
    price: Number(trade.tradedPrice || 0),
    productType: (FYERS_PRODUCT_REVERSE_MAP[String(trade.productType)] || 'CASH') as ProductType,
    tradedAt: trade.orderDateTime ? new Date(trade.orderDateTime as string) : new Date(),
    exchangeTradeId: trade.exchOrdId ? String(trade.exchOrdId) : undefined,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert Fyers position to standard format
 */
export function fromFyersPosition(position: Record<string, unknown>): Position {
  // Parse symbol
  const symbolStr = String(position.symbol || '');
  const [exchange, symbol] = symbolStr.includes(':') ? symbolStr.split(':') : ['NSE', symbolStr];

  const netQty = Number(position.netQty || 0);
  const buyQty = Number(position.buyQty || 0);
  const sellQty = Number(position.sellQty || 0);
  const buyAvg = Number(position.buyAvg || 0);
  const sellAvg = Number(position.sellAvg || 0);
  const ltp = Number(position.ltp || position.marketVal || 0);

  // Calculate average price
  const avgPrice = netQty > 0 ? buyAvg : sellAvg;

  // Calculate P&L
  const pnl = Number(position.pl || position.realizedProfit || 0);
  const pnlPercent = avgPrice > 0 ? ((ltp - avgPrice) / avgPrice) * 100 : 0;

  return {
    symbol,
    exchange: exchange as StockExchange,
    productType: (FYERS_PRODUCT_REVERSE_MAP[String(position.productType)] || 'INTRADAY') as ProductType,
    quantity: netQty,
    buyQuantity: buyQty,
    sellQuantity: sellQty,
    buyValue: buyQty * buyAvg,
    sellValue: sellQty * sellAvg,
    averagePrice: avgPrice,
    lastPrice: ltp,
    pnl,
    pnlPercent,
    dayPnl: pnl,
    overnight: false,
  };
}

// ============================================================================
// HOLDING MAPPERS
// ============================================================================

/**
 * Convert Fyers holding to standard format
 */
export function fromFyersHolding(holding: Record<string, unknown>): Holding {
  // Parse symbol
  const symbolStr = String(holding.symbol || '');
  const [exchange, symbol] = symbolStr.includes(':') ? symbolStr.split(':') : ['NSE', symbolStr];

  const quantity = Number(holding.quantity || holding.holdingQty || 0);
  const avgPrice = Number(holding.buyAvg || holding.costPrice || 0);
  const lastPrice = Number(holding.ltp || holding.marketVal || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = Number(holding.pl || (currentValue - investedValue));
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol,
    exchange: exchange as StockExchange,
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    investedValue,
    currentValue,
    pnl,
    pnlPercent,
    isin: holding.isin ? String(holding.isin) : undefined,
  };
}

// ============================================================================
// FUNDS MAPPERS
// ============================================================================

/**
 * Convert Fyers funds to standard format
 * Processes both equity (id=10) and commodity (id=11) segments
 */
export function fromFyersFunds(funds: Record<string, unknown>): Funds {
  // Fyers fund structure
  const fundLimit = (funds.fund_limit as Array<Record<string, unknown>>) || [];

  // Find equity segment data (id=10)
  const equityFund = fundLimit.find(f => f.id === 10) || {};

  // Find commodity segment data (id=11)
  const commodityFund = fundLimit.find(f => f.id === 11) || {};

  const equityMargin = Number(equityFund.equityMargin || 0);
  const equityUsed = Number(equityFund.utilizedMargin || 0);
  const equityAvailable = equityMargin - equityUsed;

  const commodityMargin = Number(commodityFund.equityMargin || 0);
  const commodityUsed = Number(commodityFund.utilizedMargin || 0);
  const commodityAvailable = commodityMargin - commodityUsed;

  // Total calculations (equity + commodity)
  const totalMargin = equityMargin + commodityMargin;
  const totalUsed = equityUsed + commodityUsed;
  const totalAvailable = equityAvailable + commodityAvailable;

  return {
    availableCash: totalAvailable,
    usedMargin: totalUsed,
    availableMargin: totalAvailable,
    totalBalance: totalMargin,
    currency: 'INR',
    equityAvailable,
    commodityAvailable,
    collateral: Number(equityFund.collateralAmount || 0) + Number(commodityFund.collateralAmount || 0),
    unrealizedPnl: Number(equityFund.unmrealizedMtomPrsnt || 0) + Number(commodityFund.unmrealizedMtomPrsnt || 0),
    realizedPnl: Number(equityFund.realizedMtomPrsnt || 0) + Number(commodityFund.realizedMtomPrsnt || 0),
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert Fyers quote to standard format
 */
export function fromFyersQuote(quote: Record<string, unknown>): Quote {
  // Parse symbol
  const symbolStr = String(quote.n || quote.symbol || '');
  const [exchange, symbol] = symbolStr.includes(':') ? symbolStr.split(':') : ['NSE', symbolStr];

  const v = quote.v as Record<string, unknown> || quote;

  const lastPrice = Number(v.lp || quote.ltp || 0);
  const prevClose = Number(v.prev_close_price || quote.prev_close_price || 0);
  const change = Number(v.ch || quote.ch || (lastPrice - prevClose));
  const changePercent = prevClose > 0 ? (change / prevClose) * 100 : 0;

  return {
    symbol,
    exchange: exchange as StockExchange,
    lastPrice,
    open: Number(v.open_price || quote.open_price || 0),
    high: Number(v.high_price || quote.high_price || 0),
    low: Number(v.low_price || quote.low_price || 0),
    close: lastPrice,
    previousClose: prevClose,
    change,
    changePercent: Number(v.chp || quote.chp || changePercent),
    volume: Number(v.volume || v.vol_traded_today || quote.volume || 0),
    averagePrice: Number(v.avg_trade_price || quote.avg_trade_price || 0),
    bid: Number(v.bid || v.bid_price || quote.best_bid_price || 0),
    bidQty: Number(v.bid_size || quote.best_bid_qty || 0),
    ask: Number(v.ask || v.ask_price || quote.best_ask_price || 0),
    askQty: Number(v.ask_size || quote.best_ask_qty || 0),
    timestamp: Date.now(),
    openInterest: Number(v.oi || quote.oi || 0),
  };
}

// ============================================================================
// TICK DATA MAPPERS
// ============================================================================

/**
 * Convert Fyers tick data to standard format
 */
export function fromFyersTickData(tick: Record<string, unknown>): TickData {
  // Parse symbol
  const symbolStr = String(tick.symbol || tick.ticker || '');
  const [exchange, symbol] = symbolStr.includes(':') ? symbolStr.split(':') : ['NSE', symbolStr];

  return {
    symbol,
    exchange: exchange as StockExchange,
    mode: 'full',
    lastPrice: Number(tick.ltp || tick.last_price || 0),
    timestamp: tick.feed_time ? Number(tick.feed_time) : Date.now(),
    volume: Number(tick.volume || tick.vol_traded_today || 0),
    open: Number(tick.open_price || 0),
    high: Number(tick.high_price || 0),
    low: Number(tick.low_price || 0),
    close: Number(tick.prev_close_price || 0),
    change: Number(tick.ch || 0),
    changePercent: Number(tick.chp || 0),
    bid: Number(tick.bid_price || tick.best_bid_price || 0),
    bidQty: Number(tick.bid_size || tick.best_bid_qty || 0),
    ask: Number(tick.ask_price || tick.best_ask_price || 0),
    askQty: Number(tick.ask_size || tick.best_ask_qty || 0),
    openInterest: Number(tick.oi || 0),
  };
}

// ============================================================================
// MARKET DEPTH MAPPERS
// ============================================================================

/**
 * Convert Fyers market depth to standard format
 */
export function fromFyersDepth(depth: Record<string, unknown>): MarketDepth {
  const bids: DepthLevel[] = [];
  const asks: DepthLevel[] = [];

  // Fyers provides depth data in asks and bids arrays
  const asksData = (depth.asks as Array<Record<string, unknown>>) || [];
  const bidsData = (depth.bids as Array<Record<string, unknown>>) || [];

  // Convert asks
  for (const ask of asksData.slice(0, 5)) {
    asks.push({
      price: Number(ask.price || 0),
      quantity: Number(ask.qty || ask.volume || 0),
      orders: Number(ask.nord || ask.orders || 0),
    });
  }

  // Convert bids
  for (const bid of bidsData.slice(0, 5)) {
    bids.push({
      price: Number(bid.price || 0),
      quantity: Number(bid.qty || bid.volume || 0),
      orders: Number(bid.nord || bid.orders || 0),
    });
  }

  return {
    bids,
    asks,
  };
}

// ============================================================================
// SYMBOL MAPPERS
// ============================================================================

/**
 * Convert symbol to Fyers format (EXCHANGE:SYMBOL-INSTRUMENTTYPE)
 *
 * Fyers requires instrument type suffix:
 * - NSE/BSE equity: -EQ (e.g., NSE:RELIANCE-EQ)
 * - Futures: -FUT
 * - Options: -OPT
 *
 * @param symbol - The stock symbol (e.g., "RELIANCE", "SBIN")
 * @param exchange - The exchange (NSE, BSE, etc.)
 * @param instrumentType - Optional instrument type suffix (defaults to "EQ" for equity)
 */
export function toFyersSymbol(
  symbol: string,
  exchange: StockExchange,
  instrumentType: string = 'EQ'
): string {
  const fyersExchange = FYERS_EXCHANGE_MAP[exchange] || exchange;

  // Fyers format: EXCHANGE:SYMBOL-INSTRUMENTTYPE
  // Example: NSE:RELIANCE-EQ, BSE:SBIN-EQ
  return `${fyersExchange}:${symbol}-${instrumentType}`;
}

/**
 * Parse Fyers symbol format to separate exchange and symbol
 *
 * Parses format: EXCHANGE:SYMBOL-INSTRUMENTTYPE
 * Example: NSE:RELIANCE-EQ -> { exchange: "NSE", symbol: "RELIANCE" }
 */
export function fromFyersSymbol(fyersSymbol: string): { exchange: StockExchange; symbol: string } {
  const [exchange, symbolWithSuffix] = fyersSymbol.includes(':')
    ? fyersSymbol.split(':')
    : ['NSE', fyersSymbol];

  // Remove instrument type suffix (e.g., -EQ, -FUT, -OPT)
  const symbol = symbolWithSuffix.includes('-')
    ? symbolWithSuffix.split('-')[0]
    : symbolWithSuffix;

  return {
    exchange: exchange as StockExchange,
    symbol,
  };
}

/**
 * Convert standard TimeFrame to Fyers API resolution format
 *
 * Fyers API expects: 1, 2, 3, 5, 10, 15, 20, 30, 60, 120, 240, 1D
 * Standard format: 1m, 3m, 5m, 10m, 15m, 30m, 1h, 2h, 4h, 1d, 1w, 1M
 */
export function toFyersInterval(timeframe: TimeFrame): string {
  switch (timeframe) {
    case '1m': return '1';
    case '3m': return '3';
    case '5m': return '5';
    case '10m': return '10';
    case '15m': return '15';
    case '30m': return '30';
    case '1h': return '60';
    case '2h': return '120';
    case '4h': return '240';
    case '1d': return '1D';
    case '1w': return '1D'; // Fyers doesn't support weekly, use daily
    case '1M': return '1D'; // Fyers doesn't support monthly, use daily
    default: return '5'; // Default to 5 minutes
  }
}
