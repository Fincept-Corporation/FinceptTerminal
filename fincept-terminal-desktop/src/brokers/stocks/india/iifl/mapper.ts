/**
 * IIFL Data Mappers
 *
 * Transform between IIFL XTS API format and standard stock broker format
 * Reference: https://symphonyfintech.com/xts-trading-front-end-api/
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
  OrderValidity,
  ProductType,
  StockExchange,
} from '../../types';

import {
  IIFL_EXCHANGE_MAP,
  IIFL_EXCHANGE_REVERSE_MAP,
  IIFL_EXCHANGE_SEGMENT_ID,
  IIFL_PRODUCT_MAP,
  IIFL_PRODUCT_REVERSE_MAP,
  IIFL_ORDER_SIDE_MAP,
  IIFL_ORDER_SIDE_REVERSE_MAP,
  IIFL_ORDER_TYPE_MAP,
  IIFL_ORDER_TYPE_REVERSE_MAP,
  IIFL_STATUS_MAP,
  IIFL_INTERVAL_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to IIFL XTS format
 */
export function toIIFLOrderParams(
  params: OrderParams,
  token: number
): Record<string, unknown> {
  return {
    exchangeSegment: IIFL_EXCHANGE_MAP[params.exchange] || 'NSECM',
    exchangeInstrumentID: token,
    productType: IIFL_PRODUCT_MAP[params.productType] || 'MIS',
    orderType: IIFL_ORDER_TYPE_MAP[params.orderType] || 'MARKET',
    orderSide: IIFL_ORDER_SIDE_MAP[params.side] || 'BUY',
    timeInForce: params.validity || 'DAY',
    disclosedQuantity: String(params.disclosedQuantity || 0),
    orderQuantity: params.quantity,
    limitPrice: params.price || 0,
    stopPrice: params.triggerPrice || 0,
    orderUniqueIdentifier: params.tag || 'fincept',
  };
}

/**
 * Convert modify order params to IIFL XTS format
 */
export function toIIFLModifyParams(params: ModifyOrderParams): Record<string, unknown> {
  return {
    appOrderID: params.orderId,
    modifiedOrderType: params.orderType ? IIFL_ORDER_TYPE_MAP[params.orderType] || 'LIMIT' : undefined,
    modifiedOrderQuantity: params.quantity,
    modifiedDisclosedQuantity: '0',
    modifiedLimitPrice: params.price || 0,
    modifiedStopPrice: params.triggerPrice || 0,
    modifiedTimeInForce: params.validity || 'DAY',
    orderUniqueIdentifier: 'fincept',
  };
}

/**
 * Convert IIFL order to standard format
 */
export function fromIIFLOrder(order: Record<string, unknown>): Order {
  const statusKey = String(order.OrderStatus || 'New');
  const exchangeSegment = String(order.ExchangeSegment || 'NSECM');
  const productType = String(order.ProductType || 'MIS');

  return {
    orderId: String(order.AppOrderID || ''),
    symbol: String(order.TradingSymbol || order.ScripName || ''),
    exchange: (IIFL_EXCHANGE_REVERSE_MAP[exchangeSegment] || 'NSE') as StockExchange,
    side: (IIFL_ORDER_SIDE_REVERSE_MAP[String(order.OrderSide || 'BUY')] || 'BUY') as OrderSide,
    quantity: Number(order.OrderQuantity || 0),
    filledQuantity: Number(order.CumulativeQuantity || order.TradedQty || 0),
    pendingQuantity: Number(order.LeavesQuantity || 0),
    price: Number(order.OrderPrice || order.LimitPrice || 0),
    averagePrice: Number(order.OrderAverageTradedPrice || 0),
    triggerPrice: order.OrderStopPrice ? Number(order.OrderStopPrice) : undefined,
    orderType: (IIFL_ORDER_TYPE_REVERSE_MAP[String(order.OrderType || 'Market')] || 'MARKET') as OrderType,
    productType: (IIFL_PRODUCT_REVERSE_MAP[productType] || 'INTRADAY') as ProductType,
    validity: (String(order.TimeInForce || 'DAY') as OrderValidity),
    status: (IIFL_STATUS_MAP[statusKey] || 'OPEN') as OrderStatus,
    statusMessage: order.CancelRejectReason ? String(order.CancelRejectReason) : undefined,
    placedAt: new Date(order.OrderGeneratedDateTime as string || order.ExchangeTransactTime as string || Date.now()),
    updatedAt: order.LastUpdateDateTime ? new Date(order.LastUpdateDateTime as string) : undefined,
    exchangeOrderId: order.ExchangeOrderID ? String(order.ExchangeOrderID) : undefined,
    tag: order.OrderUniqueIdentifier ? String(order.OrderUniqueIdentifier) : undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert IIFL trade to standard format
 */
export function fromIIFLTrade(trade: Record<string, unknown>): Trade {
  const exchangeSegment = String(trade.ExchangeSegment || 'NSECM');
  const productType = String(trade.ProductType || 'MIS');

  return {
    tradeId: String(trade.ExchangeTransactTime || trade.AppOrderID || ''),
    orderId: String(trade.AppOrderID || ''),
    symbol: String(trade.TradingSymbol || ''),
    exchange: (IIFL_EXCHANGE_REVERSE_MAP[exchangeSegment] || 'NSE') as StockExchange,
    side: (IIFL_ORDER_SIDE_REVERSE_MAP[String(trade.OrderSide || 'BUY')] || 'BUY') as OrderSide,
    quantity: Number(trade.OrderQuantity || trade.CumulativeQuantity || 0),
    price: Number(trade.OrderAverageTradedPrice || trade.LastTradedPrice || 0),
    productType: (IIFL_PRODUCT_REVERSE_MAP[productType] || 'INTRADAY') as ProductType,
    tradedAt: new Date(trade.OrderGeneratedDateTime as string || trade.ExchangeTransactTime as string || Date.now()),
    exchangeTradeId: trade.ExchangeOrderID ? String(trade.ExchangeOrderID) : undefined,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert IIFL position to standard format
 */
export function fromIIFLPosition(position: Record<string, unknown>): Position {
  const netQty = Number(position.Quantity || 0);
  const buyQty = Number(position.TotalBuyQuantity || position.OpenBuyQuantity || 0);
  const sellQty = Number(position.TotalSellQuantity || position.OpenSellQuantity || 0);
  const buyValue = Number(position.BuyAmount || 0);
  const sellValue = Number(position.SellAmount || 0);
  const lastPrice = Number(position.ltp || position.LastTradedPrice || 0);
  const avgPrice = netQty > 0 ? Number(position.BuyAveragePrice || 0) : Number(position.SellAveragePrice || 0);

  // Calculate P&L
  const pnl = Number(position.RealizedMTM || 0) + Number(position.UnrealizedMTM || 0);
  const pnlPercent = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;
  const dayPnl = Number(position.MTM || position.UnrealizedMTM || 0);

  const exchangeSegment = String(position.ExchangeSegment || 'NSECM');
  const productType = String(position.ProductType || 'MIS');

  return {
    symbol: String(position.TradingSymbol || ''),
    exchange: (IIFL_EXCHANGE_REVERSE_MAP[exchangeSegment] || 'NSE') as StockExchange,
    productType: (IIFL_PRODUCT_REVERSE_MAP[productType] || 'INTRADAY') as ProductType,
    quantity: netQty,
    buyQuantity: buyQty,
    sellQuantity: sellQty,
    buyValue,
    sellValue,
    averagePrice: avgPrice,
    lastPrice,
    pnl,
    pnlPercent,
    dayPnl,
    overnight: Boolean(position.BodQuantity && Number(position.BodQuantity) > 0),
  };
}

// ============================================================================
// HOLDING MAPPERS
// ============================================================================

/**
 * Convert IIFL holding to standard format
 */
export function fromIIFLHolding(holding: Record<string, unknown>): Holding {
  const quantity = Number(holding.HoldingQuantity || holding.Quantity || 0);
  const avgPrice = Number(holding.BuyAvgPrice || holding.AveragePrice || 0);
  const lastPrice = Number(holding.CurrentPrice || holding.LTP || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol: String(holding.TradingSymbol || holding.Symbol || ''),
    exchange: 'NSE' as StockExchange, // Holdings are typically NSE/BSE CNC
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    investedValue,
    currentValue,
    pnl,
    pnlPercent,
    isin: holding.ISIN ? String(holding.ISIN) : undefined,
    pledgedQuantity: holding.PledgedQuantity ? Number(holding.PledgedQuantity) : undefined,
    collateralQuantity: holding.CollateralQuantity ? Number(holding.CollateralQuantity) : undefined,
    t1Quantity: holding.T1Quantity ? Number(holding.T1Quantity) : undefined,
  };
}

// ============================================================================
// FUNDS MAPPERS
// ============================================================================

/**
 * Convert IIFL balance to standard funds format
 */
export function fromIIFLFunds(funds: Record<string, unknown>): Funds {
  // Handle direct funds data or nested structure
  const availableCash = Number(funds.availablecash || funds.netMarginAvailable || 0);
  const usedMargin = Number(funds.utiliseddebits || funds.marginUtilized || 0);
  const collateral = Number(funds.collateral || 0);
  const unrealizedPnl = Number(funds.m2munrealized || funds.UnrealizedMTM || 0);
  const realizedPnl = Number(funds.m2mrealized || funds.RealizedMTM || 0);

  return {
    availableCash,
    usedMargin,
    availableMargin: availableCash - usedMargin,
    totalBalance: availableCash + usedMargin,
    currency: 'INR',
    equityAvailable: availableCash,
    commodityAvailable: 0, // Not directly provided
    collateral,
    unrealizedPnl,
    realizedPnl,
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert IIFL quote to standard format
 */
export function fromIIFLQuote(
  quote: Record<string, unknown>,
  symbol: string,
  exchange: StockExchange
): Quote {
  const lastPrice = Number(quote.ltp || quote.LastTradedPrice || 0);
  const previousClose = Number(quote.prev_close || quote.Close || 0);
  const change = lastPrice - previousClose;
  const changePercent = previousClose > 0 ? (change / previousClose) * 100 : 0;

  return {
    symbol,
    exchange,
    lastPrice,
    open: Number(quote.open || quote.Open || 0),
    high: Number(quote.high || quote.High || 0),
    low: Number(quote.low || quote.Low || 0),
    close: Number(quote.close || quote.Close || 0),
    previousClose,
    change,
    changePercent,
    volume: Number(quote.volume || quote.TotalTradedQuantity || 0),
    value: quote.TotalValue ? Number(quote.TotalValue) : undefined,
    averagePrice: quote.AvgRate ? Number(quote.AvgRate) : undefined,
    bid: Number(quote.bid || 0),
    bidQty: Number(quote.bidQty || 0),
    ask: Number(quote.ask || 0),
    askQty: Number(quote.askQty || 0),
    timestamp: Date.now(),
    openInterest: quote.oi ? Number(quote.oi) : undefined,
    upperCircuit: quote.UpperCktLimit ? Number(quote.UpperCktLimit) : undefined,
    lowerCircuit: quote.LowerCktLimit ? Number(quote.LowerCktLimit) : undefined,
  };
}

// ============================================================================
// OHLCV MAPPERS
// ============================================================================

/**
 * Convert IIFL OHLCV candle to standard format
 */
export function fromIIFLOHLCV(candle: Record<string, unknown>): OHLCV {
  // IIFL returns timestamp as Unix timestamp (seconds)
  const timestamp = Number(candle.timestamp || 0) * 1000; // Convert to milliseconds

  return {
    timestamp,
    open: Number(candle.open || 0),
    high: Number(candle.high || 0),
    low: Number(candle.low || 0),
    close: Number(candle.close || 0),
    volume: Number(candle.volume || 0),
  };
}

/**
 * Convert timeframe to IIFL interval (compression value)
 */
export function toIIFLInterval(timeframe: TimeFrame): string {
  return IIFL_INTERVAL_MAP[timeframe] || '60';
}

// ============================================================================
// DEPTH MAPPERS
// ============================================================================

/**
 * Convert IIFL depth to standard format
 */
export function fromIIFLDepth(depth: Record<string, unknown>): MarketDepth {
  // Handle bids array
  const bidsArray = depth.bids as Array<Record<string, unknown>> || [];
  const bids: DepthLevel[] = bidsArray.map((bid) => ({
    price: Number(bid.price || 0),
    quantity: Number(bid.quantity || 0),
    orders: Number(bid.orders || 1),
  }));

  // Handle asks array
  const asksArray = depth.asks as Array<Record<string, unknown>> || [];
  const asks: DepthLevel[] = asksArray.map((ask) => ({
    price: Number(ask.price || 0),
    quantity: Number(ask.quantity || 0),
    orders: Number(ask.orders || 1),
  }));

  return { bids, asks };
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Get exchange segment ID for market data API
 */
export function getExchangeSegmentId(exchange: string): number {
  return IIFL_EXCHANGE_SEGMENT_ID[exchange] || 1;
}

/**
 * Get exchange segment string for order API
 */
export function getExchangeSegment(exchange: string): string {
  return IIFL_EXCHANGE_MAP[exchange] || 'NSECM';
}

/**
 * Convert standard exchange to IIFL exchange for reverse lookup
 */
export function toStandardExchange(iiflExchange: string): StockExchange {
  return (IIFL_EXCHANGE_REVERSE_MAP[iiflExchange] || 'NSE') as StockExchange;
}
