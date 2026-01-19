/**
 * Motilal Oswal Data Mappers
 *
 * Transform between Motilal Oswal API format and standard stock broker format
 * Reference: https://openapi.motilaloswal.com
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
  MOTILAL_EXCHANGE_MAP,
  MOTILAL_EXCHANGE_REVERSE_MAP,
  MOTILAL_PRODUCT_MAP,
  MOTILAL_PRODUCT_REVERSE_MAP,
  MOTILAL_ORDER_SIDE_MAP,
  MOTILAL_ORDER_SIDE_REVERSE_MAP,
  MOTILAL_ORDER_TYPE_MAP,
  MOTILAL_ORDER_TYPE_REVERSE_MAP,
  MOTILAL_STATUS_MAP,
} from './constants';

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Determine if exchange is cash segment
 */
function isCashSegment(exchange: string): boolean {
  return ['NSE', 'BSE'].includes(exchange);
}

/**
 * Get product type mapping based on exchange segment
 */
function getProductMapping(product: string, exchange: string): string {
  const segment = isCashSegment(exchange) ? 'CASH' : 'FO';
  return MOTILAL_PRODUCT_MAP[segment]?.[product] || 'NORMAL';
}

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to Motilal format
 */
export function toMotilalOrderParams(
  params: OrderParams,
  token: number
): Record<string, unknown> {
  const motilalExchange = MOTILAL_EXCHANGE_MAP[params.exchange] || 'NSE';

  return {
    exchange: motilalExchange,
    symboltoken: token,
    buyorsell: MOTILAL_ORDER_SIDE_MAP[params.side] || 'BUY',
    ordertype: MOTILAL_ORDER_TYPE_MAP[params.orderType] || 'MARKET',
    producttype: getProductMapping(params.productType, params.exchange),
    orderduration: params.validity || 'DAY',
    price: params.price || 0,
    triggerprice: params.triggerPrice || 0,
    quantityinlot: params.quantity,
    disclosedquantity: params.disclosedQuantity || 0,
    amoorder: 'N',
  };
}

/**
 * Convert modify order params to Motilal format
 */
export function toMotilalModifyParams(
  params: ModifyOrderParams,
  lastModifiedTime: string,
  qtyTradedToday: number
): Record<string, unknown> {
  return {
    uniqueorderid: params.orderId,
    newordertype: params.orderType ? MOTILAL_ORDER_TYPE_MAP[params.orderType] || 'LIMIT' : undefined,
    neworderduration: params.validity || 'DAY',
    newprice: params.price || 0,
    newtriggerprice: params.triggerPrice || 0,
    newquantityinlot: params.quantity,
    newdisclosedquantity: 0,
    newgoodtilldate: '',
    lastmodifiedtime: lastModifiedTime,
    qtytradedtoday: qtyTradedToday,
  };
}

/**
 * Convert Motilal order to standard format
 */
export function fromMotilalOrder(order: Record<string, unknown>): Order {
  const motilalExchange = String(order.exchange || 'NSE');
  const standardExchange = MOTILAL_EXCHANGE_REVERSE_MAP[motilalExchange] || 'NSE';
  const productType = String(order.producttype || 'NORMAL');

  // Map order type
  let orderType = String(order.ordertype || 'Market');
  if (orderType === 'Stoploss' || orderType === 'STOPLOSS') {
    const triggerPrice = Number(order.triggerprice || 0);
    orderType = triggerPrice > 0 ? 'SL' : 'SL-M';
  }

  // Map order status
  let status = String(order.orderstatus || 'Open');
  if (status === 'Traded' || status === 'Complete') {
    status = 'FILLED';
  } else if (['Confirm', 'Sent', 'Open'].includes(status)) {
    status = 'OPEN';
  } else if (['Rejected', 'Error'].includes(status)) {
    status = 'REJECTED';
  } else if (status === 'Cancel') {
    status = 'CANCELLED';
  }

  // Determine price to use
  const avgPrice = Number(order.averageprice || 0);
  const orderPrice = Number(order.price || 0);
  const displayPrice = avgPrice > 0 ? avgPrice : orderPrice;

  return {
    orderId: String(order.uniqueorderid || ''),
    symbol: String(order.symbol || ''),
    exchange: standardExchange as StockExchange,
    side: (MOTILAL_ORDER_SIDE_REVERSE_MAP[String(order.buyorsell || 'BUY')] || 'BUY') as OrderSide,
    quantity: Number(order.orderqty || 0),
    filledQuantity: Number(order.qtytradedtoday || 0),
    pendingQuantity: Number(order.orderqty || 0) - Number(order.qtytradedtoday || 0),
    price: displayPrice,
    averagePrice: avgPrice,
    triggerPrice: order.triggerprice ? Number(order.triggerprice) : undefined,
    orderType: (MOTILAL_ORDER_TYPE_REVERSE_MAP[orderType] || 'MARKET') as OrderType,
    productType: (MOTILAL_PRODUCT_REVERSE_MAP[productType] || 'INTRADAY') as ProductType,
    validity: 'DAY' as OrderValidity,
    status: status as OrderStatus,
    statusMessage: order.message ? String(order.message) : undefined,
    placedAt: new Date(order.lastmodifiedtime as string || Date.now()),
    updatedAt: order.lastmodifiedtime ? new Date(order.lastmodifiedtime as string) : undefined,
    exchangeOrderId: order.exchangeorderid ? String(order.exchangeorderid) : undefined,
    tag: order.tag ? String(order.tag) : undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert Motilal trade to standard format
 */
export function fromMotilalTrade(trade: Record<string, unknown>): Trade {
  const motilalExchange = String(trade.exchange || 'NSE');
  const standardExchange = MOTILAL_EXCHANGE_REVERSE_MAP[motilalExchange] || 'NSE';
  const productType = String(trade.producttype || 'NORMAL');

  return {
    tradeId: String(trade.tradetime || trade.uniqueorderid || ''),
    orderId: String(trade.uniqueorderid || ''),
    symbol: String(trade.symbol || ''),
    exchange: standardExchange as StockExchange,
    side: (MOTILAL_ORDER_SIDE_REVERSE_MAP[String(trade.buyorsell || 'BUY')] || 'BUY') as OrderSide,
    quantity: Number(trade.tradeqty || 0),
    price: Number(trade.tradeprice || 0),
    productType: (MOTILAL_PRODUCT_REVERSE_MAP[productType] || 'INTRADAY') as ProductType,
    tradedAt: new Date(trade.tradetime as string || Date.now()),
    exchangeTradeId: trade.exchangetradeid ? String(trade.exchangetradeid) : undefined,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert Motilal position to standard format
 */
export function fromMotilalPosition(position: Record<string, unknown>): Position {
  const buyQty = Number(position.buyquantity || 0);
  const sellQty = Number(position.sellquantity || 0);
  const netQty = buyQty - sellQty;
  const buyAmt = Number(position.buyamount || 0);
  const sellAmt = Number(position.sellamount || 0);

  // Calculate average price
  let avgPrice = 0;
  if (netQty > 0 && buyQty > 0) {
    avgPrice = buyAmt / buyQty;
  } else if (netQty < 0 && sellQty > 0) {
    avgPrice = sellAmt / sellQty;
  }

  const lastPrice = Number(position.LTP || position.ltp || 0);
  const pnl = Number(position.marktomarket || 0) + Number(position.bookedprofitloss || 0);
  const pnlPercent = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;
  const dayPnl = Number(position.marktomarket || 0);

  const motilalExchange = String(position.exchange || 'NSE');
  const standardExchange = MOTILAL_EXCHANGE_REVERSE_MAP[motilalExchange] || 'NSE';
  // Motilal uses 'productname' for positions
  const productType = String(position.productname || 'NORMAL');

  return {
    symbol: String(position.symbol || ''),
    exchange: standardExchange as StockExchange,
    productType: (MOTILAL_PRODUCT_REVERSE_MAP[productType] || 'INTRADAY') as ProductType,
    quantity: netQty,
    buyQuantity: buyQty,
    sellQuantity: sellQty,
    buyValue: buyAmt,
    sellValue: sellAmt,
    averagePrice: avgPrice,
    lastPrice,
    pnl,
    pnlPercent,
    dayPnl,
    overnight: Boolean(position.bodquantity && Number(position.bodquantity) > 0),
  };
}

// ============================================================================
// HOLDING MAPPERS
// ============================================================================

/**
 * Convert Motilal holding to standard format
 */
export function fromMotilalHolding(holding: Record<string, unknown>): Holding {
  const quantity = Number(holding.dpquantity || holding.quantity || 0);
  const avgPrice = Number(holding.buyavgprice || holding.averageprice || 0);
  const lastPrice = Number(holding.currentprice || holding.ltp || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  // Determine exchange from available token
  let exchange: StockExchange = 'NSE';
  if (holding.nsesymboltoken && Number(holding.nsesymboltoken) > 0) {
    exchange = 'NSE';
  } else if (holding.bsescripcode && Number(holding.bsescripcode) > 0) {
    exchange = 'BSE';
  }

  return {
    symbol: String(holding.symbol || holding.scripname || ''),
    exchange,
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    investedValue,
    currentValue,
    pnl,
    pnlPercent,
    isin: holding.isin ? String(holding.isin) : undefined,
    pledgedQuantity: holding.pledgedquantity ? Number(holding.pledgedquantity) : undefined,
    collateralQuantity: holding.collateralquantity ? Number(holding.collateralquantity) : undefined,
    t1Quantity: holding.t1quantity ? Number(holding.t1quantity) : undefined,
  };
}

// ============================================================================
// FUNDS MAPPERS
// ============================================================================

/**
 * Convert Motilal margin data to standard funds format
 */
export function fromMotilalFunds(funds: Record<string, unknown>): Funds {
  const availableCash = Number(funds.availablecash || 0);
  const usedMargin = Number(funds.utiliseddebits || 0);
  const collateral = Number(funds.collateral || 0);
  const unrealizedPnl = Number(funds.m2munrealized || 0);
  const realizedPnl = Number(funds.m2mrealized || 0);

  return {
    availableCash,
    usedMargin,
    availableMargin: availableCash - usedMargin,
    totalBalance: availableCash + usedMargin,
    currency: 'INR',
    equityAvailable: availableCash,
    commodityAvailable: 0,
    collateral,
    unrealizedPnl,
    realizedPnl,
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert Motilal quote to standard format
 * Note: Motilal returns values in paisa, need to divide by 100
 */
export function fromMotilalQuote(
  quote: Record<string, unknown>,
  symbol: string,
  exchange: StockExchange
): Quote {
  // Motilal returns prices in paisa
  const convertPaisa = (value: unknown): number => {
    const num = Number(value || 0);
    return num / 100;
  };

  const lastPrice = convertPaisa(quote.ltp);
  const previousClose = convertPaisa(quote.close);
  const change = lastPrice - previousClose;
  const changePercent = previousClose > 0 ? (change / previousClose) * 100 : 0;

  return {
    symbol,
    exchange,
    lastPrice,
    open: convertPaisa(quote.open),
    high: convertPaisa(quote.high),
    low: convertPaisa(quote.low),
    close: convertPaisa(quote.close),
    previousClose,
    change,
    changePercent,
    volume: Number(quote.volume || 0),
    value: undefined,
    averagePrice: undefined,
    bid: convertPaisa(quote.bid),
    bidQty: Number(quote.bidQty || 0),
    ask: convertPaisa(quote.ask),
    askQty: Number(quote.askQty || 0),
    timestamp: Date.now(),
    openInterest: undefined, // Not provided in LTP API
    upperCircuit: undefined,
    lowerCircuit: undefined,
  };
}

// ============================================================================
// OHLCV MAPPERS
// ============================================================================

/**
 * Convert Motilal OHLCV candle to standard format
 * Note: Motilal doesn't support historical data API
 */
export function fromMotilalOHLCV(candle: Record<string, unknown>): OHLCV {
  return {
    timestamp: Number(candle.timestamp || 0) * 1000,
    open: Number(candle.open || 0),
    high: Number(candle.high || 0),
    low: Number(candle.low || 0),
    close: Number(candle.close || 0),
    volume: Number(candle.volume || 0),
  };
}

/**
 * Convert timeframe to Motilal interval
 * Note: Motilal doesn't support historical data
 */
export function toMotilalInterval(_timeframe: TimeFrame): string {
  return '1'; // Not applicable
}

// ============================================================================
// DEPTH MAPPERS
// ============================================================================

/**
 * Convert Motilal depth to standard format
 */
export function fromMotilalDepth(depth: Record<string, unknown>): MarketDepth {
  const bidsArray = depth.bids as Array<Record<string, unknown>> || [];
  const bids: DepthLevel[] = bidsArray.map((bid) => ({
    price: Number(bid.price || 0),
    quantity: Number(bid.quantity || 0),
    orders: Number(bid.orders || 1),
  }));

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
 * Get Motilal exchange string
 */
export function getMotilalExchange(exchange: string): string {
  return MOTILAL_EXCHANGE_MAP[exchange] || 'NSE';
}

/**
 * Convert Motilal exchange to standard exchange
 */
export function toStandardExchange(motilalExchange: string): StockExchange {
  return (MOTILAL_EXCHANGE_REVERSE_MAP[motilalExchange] || 'NSE') as StockExchange;
}
