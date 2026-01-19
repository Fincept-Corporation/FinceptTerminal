/**
 * Alice Blue Data Mappers
 *
 * Transform between Alice Blue API format and standard stock broker format
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
} from '../../types';

import {
  ALICEBLUE_EXCHANGE_MAP,
  ALICEBLUE_PRODUCT_MAP,
  ALICEBLUE_PRODUCT_REVERSE_MAP,
  ALICEBLUE_ORDER_TYPE_MAP,
  ALICEBLUE_ORDER_TYPE_REVERSE_MAP,
  ALICEBLUE_STATUS_MAP,
  ALICEBLUE_VALIDITY_MAP,
  ALICEBLUE_INTERVAL_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to Alice Blue format
 */
export function toAliceBlueOrderParams(params: OrderParams): Record<string, unknown> {
  return {
    complexty: 'regular',
    discqty: params.disclosedQuantity || 0,
    exch: ALICEBLUE_EXCHANGE_MAP[params.exchange] || params.exchange,
    pCode: ALICEBLUE_PRODUCT_MAP[params.productType] || params.productType,
    prctyp: ALICEBLUE_ORDER_TYPE_MAP[params.orderType] || params.orderType,
    price: params.price || 0,
    qty: params.quantity,
    ret: ALICEBLUE_VALIDITY_MAP[params.validity || 'DAY'] || 'DAY',
    symbol_id: (params as any).token || '',
    trading_symbol: params.symbol,
    transtype: params.side,
    trigPrice: params.triggerPrice || 0,
  };
}

/**
 * Convert modify order params to Alice Blue format
 */
export function toAliceBlueModifyParams(params: ModifyOrderParams): Record<string, unknown> {
  const aliceBlueParams: Record<string, unknown> = {
    nestOrderNumber: params.orderId,
  };

  if (params.quantity !== undefined) {
    aliceBlueParams.qty = params.quantity;
  }
  if (params.price !== undefined) {
    aliceBlueParams.price = params.price;
  }
  if (params.triggerPrice !== undefined) {
    aliceBlueParams.trigPrice = params.triggerPrice;
  }
  if (params.orderType !== undefined) {
    aliceBlueParams.prctyp = ALICEBLUE_ORDER_TYPE_MAP[params.orderType] || params.orderType;
  }
  if (params.validity !== undefined) {
    aliceBlueParams.ret = ALICEBLUE_VALIDITY_MAP[params.validity] || params.validity;
  }

  return aliceBlueParams;
}

/**
 * Convert Alice Blue order to standard format
 */
export function fromAliceBlueOrder(order: Record<string, unknown>): Order {
  const statusKey = String(order.Status || order.status || 'pending').toLowerCase();

  return {
    orderId: String(order.Nstordno || order.nestOrderNumber || ''),
    symbol: String(order.Trsym || order.trading_symbol || ''),
    exchange: String(order.Exchange || order.exch || 'NSE') as StockExchange,
    side: String(order.Trantype || order.transtype || 'BUY').toUpperCase() as OrderSide,
    quantity: Number(order.Qty || order.qty || 0),
    filledQuantity: Number(order.Fillshares || order.filledQty || 0),
    pendingQuantity: Number(order.Unfilledsize || order.pendingQty || 0),
    price: Number(order.Prc || order.price || 0),
    averagePrice: Number(order.Avgprc || order.avgPrice || 0),
    triggerPrice: order.Trgprc ? Number(order.Trgprc) : undefined,
    orderType: (ALICEBLUE_ORDER_TYPE_REVERSE_MAP[String(order.Prctype || order.prctyp)] || 'LIMIT') as OrderType,
    productType: (ALICEBLUE_PRODUCT_REVERSE_MAP[String(order.Pcode || order.pCode)] || 'CASH') as ProductType,
    validity: String(order.Validity || order.ret || 'DAY') as 'DAY' | 'IOC' | 'GTC' | 'GTD',
    status: (ALICEBLUE_STATUS_MAP[statusKey] || 'PENDING') as OrderStatus,
    statusMessage: order.RejReason ? String(order.RejReason) : undefined,
    placedAt: new Date(order.OrderedTime as string || Date.now()),
    updatedAt: order.ExchConfrmtime
      ? new Date(order.ExchConfrmtime as string)
      : undefined,
    exchangeOrderId: order.ExchOrdID ? String(order.ExchOrdID) : undefined,
    tag: order.remarks ? String(order.remarks) : undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert Alice Blue trade to standard format
 */
export function fromAliceBlueTrade(trade: Record<string, unknown>): Trade {
  return {
    tradeId: String(trade.FillId || trade.fillId || ''),
    orderId: String(trade.Nstordno || trade.nestOrderNumber || ''),
    symbol: String(trade.Trsym || trade.trading_symbol || ''),
    exchange: String(trade.Exchange || trade.exch || 'NSE') as StockExchange,
    side: String(trade.Trantype || trade.transtype || 'BUY').toUpperCase() as OrderSide,
    quantity: Number(trade.Fillshares || trade.filledQty || 0),
    price: Number(trade.Fillprc || trade.fillPrice || 0),
    productType: (ALICEBLUE_PRODUCT_REVERSE_MAP[String(trade.Pcode || trade.pCode)] || 'CASH') as ProductType,
    tradedAt: new Date(trade.FillTime as string || trade.ExchConfrmtime as string || Date.now()),
    exchangeTradeId: trade.ExchOrdID ? String(trade.ExchOrdID) : undefined,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert Alice Blue position to standard format
 */
export function fromAliceBluePosition(position: Record<string, unknown>): Position {
  const netQty = Number(position.Netqty || position.netQty || 0);
  const buyQty = Number(position.Bqty || position.buyQty || 0);
  const sellQty = Number(position.Sqty || position.sellQty || 0);
  const buyValue = Number(position.BuyAmt || position.buyValue || 0);
  const sellValue = Number(position.SellAmt || position.sellValue || 0);
  const lastPrice = Number(position.LTP || position.ltp || 0);
  const avgPrice = Number(position.Netavgprc || position.avgPrice || 0);

  // Calculate P&L
  const pnl = Number(position.MtoM || position.pnl || 0);
  const pnlPercent = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;
  const dayPnl = Number(position.realisedprofitloss || position.dayPnl || 0);

  return {
    symbol: String(position.Trsym || position.trading_symbol || ''),
    exchange: String(position.Exchange || position.exch || 'NSE') as StockExchange,
    productType: (ALICEBLUE_PRODUCT_REVERSE_MAP[String(position.Pcode || position.pCode)] || 'INTRADAY') as ProductType,
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
    overnight: Boolean(position.CFBuyQty || position.cfBuyQty),
  };
}

// ============================================================================
// HOLDING MAPPERS
// ============================================================================

/**
 * Convert Alice Blue holding to standard format
 */
export function fromAliceBlueHolding(holding: Record<string, unknown>): Holding {
  const quantity = Number(holding.Holdqty || holding.holdQty || 0);
  const avgPrice = Number(holding.Price || holding.avgPrice || 0);
  const lastPrice = Number(holding.LTP || holding.ltp || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol: String(holding.Trsym || holding.trading_symbol || ''),
    exchange: String(holding.Exchange || holding.exch || 'NSE') as StockExchange,
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    investedValue,
    currentValue,
    pnl,
    pnlPercent,
    isin: holding.Isin ? String(holding.Isin) : undefined,
    pledgedQuantity: holding.Colqty ? Number(holding.Colqty) : undefined,
    collateralQuantity: holding.Btst ? Number(holding.Btst) : undefined,
    t1Quantity: holding.Btstqty ? Number(holding.Btstqty) : undefined,
  };
}

// ============================================================================
// FUNDS MAPPERS
// ============================================================================

/**
 * Convert Alice Blue margins to standard funds format
 */
export function fromAliceBlueFunds(funds: Record<string, unknown>): Funds {
  // Alice Blue returns segment-wise data
  const availableCash = Number(funds.net || funds.cash || 0);
  const usedMargin = Number(funds.utilised || funds.marginUsed || 0);
  const payin = Number(funds.payin || 0);
  const payout = Number(funds.payout || 0);
  const collateral = Number(funds.collateral || 0);
  const spanMargin = Number(funds.span || 0);
  const exposureMargin = Number(funds.expo || 0);

  return {
    availableCash,
    usedMargin,
    availableMargin: availableCash - usedMargin,
    totalBalance: availableCash + usedMargin,
    currency: 'INR',
    equityAvailable: availableCash,
    commodityAvailable: Number(funds.commodity || 0),
    collateral,
    unrealizedPnl: Number(funds.unrealised_m2m || 0),
    realizedPnl: Number(funds.realised_m2m || 0),
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert Alice Blue quote to standard format
 */
export function fromAliceBlueQuote(quote: Record<string, unknown>, symbol: string, exchange: StockExchange): Quote {
  const lastPrice = Number(quote.lp || quote.LTP || 0);
  const previousClose = Number(quote.c || quote.close || 0);
  const change = lastPrice - previousClose;
  const changePercent = previousClose > 0 ? (change / previousClose) * 100 : 0;

  return {
    symbol,
    exchange,
    lastPrice,
    open: Number(quote.o || quote.open || 0),
    high: Number(quote.h || quote.high || 0),
    low: Number(quote.l || quote.low || 0),
    close: Number(quote.c || quote.close || 0),
    previousClose,
    change,
    changePercent,
    volume: Number(quote.v || quote.volume || 0),
    value: quote.value ? Number(quote.value) : undefined,
    averagePrice: quote.ap ? Number(quote.ap) : undefined,
    bid: Number(quote.bp1 || quote.bid || 0),
    bidQty: Number(quote.bq1 || quote.bidQty || 0),
    ask: Number(quote.sp1 || quote.ask || 0),
    askQty: Number(quote.sq1 || quote.askQty || 0),
    timestamp: Number(quote.ltt || Date.now()),
    openInterest: quote.oi ? Number(quote.oi) : undefined,
    oiChange: quote.poi ? Number(quote.poi) : undefined,
    upperCircuit: quote.uc ? Number(quote.uc) : undefined,
    lowerCircuit: quote.lc ? Number(quote.lc) : undefined,
  };
}

// ============================================================================
// OHLCV MAPPERS
// ============================================================================

/**
 * Convert Alice Blue OHLCV candle to standard format
 */
export function fromAliceBlueOHLCV(candle: unknown[]): OHLCV {
  return {
    timestamp: new Date(candle[0] as string).getTime(),
    open: Number(candle[1] || 0),
    high: Number(candle[2] || 0),
    low: Number(candle[3] || 0),
    close: Number(candle[4] || 0),
    volume: Number(candle[5] || 0),
  };
}

/**
 * Convert timeframe to Alice Blue interval
 */
export function toAliceBlueInterval(timeframe: TimeFrame): string {
  const intervalMap: Record<TimeFrame, string> = {
    '1m': '1',
    '3m': '3',
    '5m': '5',
    '10m': '10',
    '15m': '15',
    '30m': '30',
    '1h': '60',
    '2h': '120',
    '4h': '240',
    '1d': 'D',
    '1w': 'W',
    '1M': 'M',
  };
  return intervalMap[timeframe] || 'D';
}

// ============================================================================
// DEPTH MAPPERS
// ============================================================================

/**
 * Convert Alice Blue depth to standard format
 */
export function fromAliceBlueDepth(quote: Record<string, unknown>): MarketDepth {
  const bids: DepthLevel[] = [];
  const asks: DepthLevel[] = [];

  // Alice Blue uses bp1-bp5 for bid prices, bq1-bq5 for bid quantities
  for (let i = 1; i <= 5; i++) {
    const bidPrice = Number(quote[`bp${i}`] || 0);
    const bidQty = Number(quote[`bq${i}`] || 0);
    const bidOrders = Number(quote[`bo${i}`] || 1);

    if (bidPrice > 0) {
      bids.push({
        price: bidPrice,
        quantity: bidQty,
        orders: bidOrders,
      });
    }

    const askPrice = Number(quote[`sp${i}`] || 0);
    const askQty = Number(quote[`sq${i}`] || 0);
    const askOrders = Number(quote[`so${i}`] || 1);

    if (askPrice > 0) {
      asks.push({
        price: askPrice,
        quantity: askQty,
        orders: askOrders,
      });
    }
  }

  return { bids, asks };
}

// ============================================================================
// MARGIN MAPPERS
// ============================================================================

/**
 * Convert order params to Alice Blue margin request format
 */
export function toAliceBlueMarginParams(params: OrderParams): Record<string, unknown> {
  return {
    exch: ALICEBLUE_EXCHANGE_MAP[params.exchange] || params.exchange,
    trading_symbol: params.symbol,
    transtype: params.side,
    pCode: ALICEBLUE_PRODUCT_MAP[params.productType] || params.productType,
    prctyp: ALICEBLUE_ORDER_TYPE_MAP[params.orderType] || params.orderType,
    qty: params.quantity,
    price: params.price || 0,
    trigPrice: params.triggerPrice || 0,
  };
}
