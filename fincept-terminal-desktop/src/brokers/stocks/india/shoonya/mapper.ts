/**
 * Shoonya (Finvasia) Data Mappers
 *
 * Transform between Shoonya API format and standard stock broker format
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
} from '../../types';

import {
  SHOONYA_EXCHANGE_MAP,
  SHOONYA_PRODUCT_MAP,
  SHOONYA_PRODUCT_REVERSE_MAP,
  SHOONYA_ORDER_TYPE_MAP,
  SHOONYA_ORDER_TYPE_REVERSE_MAP,
  SHOONYA_SIDE_MAP,
  SHOONYA_SIDE_REVERSE_MAP,
  SHOONYA_STATUS_MAP,
  SHOONYA_VALIDITY_MAP,
  SHOONYA_RESOLUTION_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to Shoonya format
 */
export function toShoonyaOrderParams(params: OrderParams): Record<string, unknown> {
  const orderType = SHOONYA_ORDER_TYPE_MAP[params.orderType] || 'MKT';
  const side = SHOONYA_SIDE_MAP[params.side] || 'B';
  const product = SHOONYA_PRODUCT_MAP[params.productType] || 'C';

  return {
    exch: SHOONYA_EXCHANGE_MAP[params.exchange] || params.exchange,
    tsym: params.symbol,  // Trading symbol in broker format
    qty: params.quantity.toString(),
    prc: (params.price || 0).toString(),
    trgprc: (params.triggerPrice || 0).toString(),
    dscqty: (params.disclosedQuantity || 0).toString(),
    prd: product,
    trantype: side,
    prctyp: orderType,
    ret: SHOONYA_VALIDITY_MAP[params.validity || 'DAY'] || 'DAY',
    mkt_protection: '0',
    ordersource: 'API',
  };
}

/**
 * Convert modify order params to Shoonya format
 */
export function toShoonyaModifyParams(orderId: string, params: ModifyOrderParams): Record<string, unknown> {
  const shoonyaParams: Record<string, unknown> = {
    norenordno: orderId,
  };

  if (params.quantity !== undefined) {
    shoonyaParams.qty = params.quantity.toString();
  }
  if (params.price !== undefined) {
    shoonyaParams.prc = params.price.toString();
  }
  if (params.triggerPrice !== undefined) {
    shoonyaParams.trgprc = params.triggerPrice.toString();
  }
  if (params.orderType !== undefined) {
    shoonyaParams.prctyp = SHOONYA_ORDER_TYPE_MAP[params.orderType] || 'MKT';
  }

  return shoonyaParams;
}

/**
 * Convert Shoonya order to standard format
 */
export function fromShoonyaOrder(order: Record<string, unknown>): Order {
  const exchange = String(order.exch || 'NSE');
  const symbol = String(order.tsym || order.symbol || '');

  // Map status
  const rawStatus = String(order.status || order.stat || 'PENDING').toUpperCase();
  const status = SHOONYA_STATUS_MAP[rawStatus] || rawStatus;

  // Map side
  const sideCode = String(order.trantype || 'B');
  const side = SHOONYA_SIDE_REVERSE_MAP[sideCode] || 'BUY';

  // Map order type
  const typeCode = String(order.prctyp || 'MKT');
  const orderType = SHOONYA_ORDER_TYPE_REVERSE_MAP[typeCode] || 'MARKET';

  // Parse quantities
  const qty = Number(order.qty || 0);
  const filledQty = Number(order.fillshares || order.flqty || 0);

  return {
    orderId: String(order.norenordno || order.orderno || ''),
    symbol: extractSymbolName(symbol),
    exchange: exchange as StockExchange,
    side: side as OrderSide,
    quantity: qty,
    filledQuantity: filledQty,
    pendingQuantity: qty - filledQty,
    price: Number(order.prc || 0),
    averagePrice: Number(order.avgprc || order.flprc || order.prc || 0),
    triggerPrice: order.trgprc ? Number(order.trgprc) : undefined,
    orderType: orderType as OrderType,
    productType: (SHOONYA_PRODUCT_REVERSE_MAP[String(order.prd)] || 'CASH') as ProductType,
    validity: String(order.ret || 'DAY') as 'DAY' | 'IOC' | 'GTC' | 'GTD',
    status: status as OrderStatus,
    statusMessage: order.rejreason ? String(order.rejreason) : undefined,
    placedAt: order.norentm ? parseDateTime(String(order.norentm)) : new Date(),
    updatedAt: order.exch_tm ? parseDateTime(String(order.exch_tm)) : undefined,
    exchangeOrderId: order.exchordid ? String(order.exchordid) : undefined,
    tag: order.remarks ? String(order.remarks) : undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert Shoonya trade to standard format
 */
export function fromShoonyaTrade(trade: Record<string, unknown>): Trade {
  const exchange = String(trade.exch || 'NSE');
  const symbol = String(trade.tsym || trade.symbol || '');

  // Map side
  const sideCode = String(trade.trantype || 'B');
  const side = SHOONYA_SIDE_REVERSE_MAP[sideCode] || 'BUY';

  return {
    tradeId: String(trade.flid || trade.tradeno || ''),
    orderId: String(trade.norenordno || trade.orderno || ''),
    symbol: extractSymbolName(symbol),
    exchange: exchange as StockExchange,
    side: side as OrderSide,
    quantity: Number(trade.flqty || trade.qty || 0),
    price: Number(trade.flprc || trade.prc || 0),
    productType: (SHOONYA_PRODUCT_REVERSE_MAP[String(trade.prd)] || 'CASH') as ProductType,
    tradedAt: trade.fltm ? parseDateTime(String(trade.fltm)) : new Date(),
    exchangeTradeId: trade.exchordid ? String(trade.exchordid) : undefined,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert Shoonya position to standard format
 */
export function fromShoonyaPosition(position: Record<string, unknown>): Position {
  const exchange = String(position.exch || 'NSE');
  const symbol = String(position.tsym || position.symbol || '');

  // Parse quantities
  const daybuyqty = Number(position.daybuyqty || 0);
  const daysellqty = Number(position.daysellqty || 0);
  const cfbuyqty = Number(position.cfbuyqty || 0);
  const cfsellqty = Number(position.cfsellqty || 0);

  // Net position
  const netQty = Number(position.netqty || ((daybuyqty + cfbuyqty) - (daysellqty + cfsellqty)));

  // Average prices
  const daybuyavgprc = Number(position.daybuyavgprc || 0);
  const daysellavgprc = Number(position.daysellavgprc || 0);

  // Calculate average price based on position direction
  const avgPrice = netQty > 0 ? daybuyavgprc : daysellavgprc;

  // LTP and P&L
  const ltp = Number(position.lp || position.ltp || 0);
  const pnl = Number(position.rpnl || 0) + Number(position.urmtom || 0);
  const pnlPercent = avgPrice > 0 ? ((ltp - avgPrice) / avgPrice) * 100 : 0;

  return {
    symbol: extractSymbolName(symbol),
    exchange: exchange as StockExchange,
    productType: (SHOONYA_PRODUCT_REVERSE_MAP[String(position.prd)] || 'INTRADAY') as ProductType,
    quantity: netQty,
    buyQuantity: daybuyqty + cfbuyqty,
    sellQuantity: daysellqty + cfsellqty,
    buyValue: (daybuyqty * daybuyavgprc) + (cfbuyqty * Number(position.cfbuyavgprc || daybuyavgprc)),
    sellValue: (daysellqty * daysellavgprc) + (cfsellqty * Number(position.cfsellavgprc || daysellavgprc)),
    averagePrice: avgPrice,
    lastPrice: ltp,
    pnl,
    pnlPercent,
    dayPnl: Number(position.rpnl || 0),
    overnight: cfbuyqty > 0 || cfsellqty > 0,
  };
}

// ============================================================================
// HOLDING MAPPERS
// ============================================================================

/**
 * Convert Shoonya holding to standard format
 */
export function fromShoonyaHolding(holding: Record<string, unknown>): Holding {
  const exchange = String(holding.exch || 'NSE');
  const symbol = String(holding.tsym || holding.symbol || '');

  // Parse values
  const quantity = Number(holding.holdqty || holding.qty || 0);
  const avgPrice = Number(holding.upldprc || holding.avgprc || 0);
  const ltp = Number(holding.lp || holding.ltp || 0);

  const investedValue = quantity * avgPrice;
  const currentValue = quantity * ltp;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol: extractSymbolName(symbol),
    exchange: exchange as StockExchange,
    quantity,
    averagePrice: avgPrice,
    lastPrice: ltp,
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
 * Convert Shoonya funds to standard format
 * Shoonya returns: cash, payin, marginused, brkcollamt, rpnl, urmtom
 */
export function fromShoonyaFunds(funds: Record<string, unknown>): Funds {
  // Parse fund values
  const cash = Number(funds.cash || funds.availablecash || 0);
  const payin = Number(funds.payin || 0);
  const marginUsed = Number(funds.marginused || funds.utiliseddebits || 0);
  const collateral = Number(funds.brkcollamt || funds.collateral || 0);
  const rpnl = Number(funds.rpnl || funds.m2mrealized || 0);
  const urmtom = Number(funds.urmtom || funds.m2munrealized || 0);

  // Total available (OpenAlgo formula)
  const availableCash = cash + payin - marginUsed;

  return {
    availableCash,
    usedMargin: marginUsed,
    availableMargin: availableCash,
    totalBalance: cash + payin + collateral,
    currency: 'INR',
    collateral,
    unrealizedPnl: urmtom,
    realizedPnl: -rpnl,  // Negate as per OpenAlgo pattern
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert Shoonya quote to standard format
 */
export function fromShoonyaQuote(quote: Record<string, unknown>): Quote {
  const exchange = String(quote.exch || 'NSE');
  const symbol = String(quote.tsym || quote.symbol || '');

  const ltp = Number(quote.lp || quote.ltp || 0);
  const prevClose = Number(quote.c || quote.prev_close || 0);
  const change = ltp - prevClose;
  const changePercent = prevClose > 0 ? (change / prevClose) * 100 : 0;

  return {
    symbol: extractSymbolName(symbol),
    exchange: exchange as StockExchange,
    lastPrice: ltp,
    open: Number(quote.o || quote.open || 0),
    high: Number(quote.h || quote.high || 0),
    low: Number(quote.l || quote.low || 0),
    close: ltp,
    previousClose: prevClose,
    change,
    changePercent,
    volume: Number(quote.v || quote.volume || 0),
    averagePrice: Number(quote.ap || quote.avgprc || 0),
    bid: Number(quote.bp1 || quote.bid || 0),
    bidQty: Number(quote.bq1 || quote.bidqty || 0),
    ask: Number(quote.sp1 || quote.ask || 0),
    askQty: Number(quote.sq1 || quote.askqty || 0),
    timestamp: Date.now(),
    openInterest: Number(quote.oi || 0),
  };
}

// ============================================================================
// TICK DATA MAPPERS
// ============================================================================

/**
 * Convert Shoonya tick data to standard format
 */
export function fromShoonyaTickData(tick: Record<string, unknown>): TickData {
  const exchange = String(tick.e || tick.exch || 'NSE');
  const symbol = String(tick.tk || tick.tsym || '');

  return {
    symbol: extractSymbolName(symbol),
    exchange: exchange as StockExchange,
    mode: 'full',
    lastPrice: Number(tick.lp || 0),
    timestamp: tick.ft ? Number(tick.ft) * 1000 : Date.now(),
    volume: Number(tick.v || 0),
    open: Number(tick.o || 0),
    high: Number(tick.h || 0),
    low: Number(tick.l || 0),
    close: Number(tick.c || 0),
    change: Number(tick.lp || 0) - Number(tick.c || 0),
    changePercent: Number(tick.pc || 0),
    bid: Number(tick.bp1 || 0),
    bidQty: Number(tick.bq1 || 0),
    ask: Number(tick.sp1 || 0),
    askQty: Number(tick.sq1 || 0),
    openInterest: Number(tick.oi || 0),
  };
}

// ============================================================================
// MARKET DEPTH MAPPERS
// ============================================================================

/**
 * Convert Shoonya market depth to standard format
 */
export function fromShoonyaDepth(depth: Record<string, unknown>): MarketDepth {
  const bids: DepthLevel[] = [];
  const asks: DepthLevel[] = [];

  // Shoonya provides bp1-bp5, bq1-bq5 for bids and sp1-sp5, sq1-sq5 for asks
  for (let i = 1; i <= 5; i++) {
    const bidPrice = Number(depth[`bp${i}`] || 0);
    const bidQty = Number(depth[`bq${i}`] || 0);
    const askPrice = Number(depth[`sp${i}`] || 0);
    const askQty = Number(depth[`sq${i}`] || 0);

    if (bidPrice > 0 || bidQty > 0) {
      bids.push({
        price: bidPrice,
        quantity: bidQty,
        orders: Number(depth[`bo${i}`] || 0),
      });
    }

    if (askPrice > 0 || askQty > 0) {
      asks.push({
        price: askPrice,
        quantity: askQty,
        orders: Number(depth[`so${i}`] || 0),
      });
    }
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
 * Convert symbol to Shoonya format
 * Shoonya uses trading symbol directly (e.g., RELIANCE-EQ, NIFTY26JAN25FUT)
 */
export function toShoonyaSymbol(symbol: string, exchange: StockExchange): string {
  // For equity, add -EQ suffix if not present
  if ((exchange === 'NSE' || exchange === 'BSE') && !symbol.includes('-')) {
    return `${symbol}-EQ`;
  }
  return symbol;
}

/**
 * Extract base symbol name from trading symbol
 * e.g., RELIANCE-EQ -> RELIANCE, NIFTY26JAN25FUT -> NIFTY26JAN25FUT
 */
export function extractSymbolName(tradingSymbol: string): string {
  // Remove -EQ suffix for equity
  if (tradingSymbol.endsWith('-EQ')) {
    return tradingSymbol.replace('-EQ', '');
  }
  // Remove -BE suffix
  if (tradingSymbol.endsWith('-BE')) {
    return tradingSymbol.replace('-BE', '');
  }
  return tradingSymbol;
}

/**
 * Convert standard TimeFrame to Shoonya resolution format
 */
export function toShoonyaResolution(timeframe: TimeFrame): string {
  return SHOONYA_RESOLUTION_MAP[timeframe] || '5';
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Parse Shoonya date/time format to Date object
 * Formats: "DD-MM-YYYY HH:MM:SS" or Unix timestamp
 */
function parseDateTime(dateStr: string): Date {
  // Try Unix timestamp first
  if (/^\d+$/.test(dateStr)) {
    return new Date(Number(dateStr) * 1000);
  }

  // Try DD-MM-YYYY HH:MM:SS format
  const match = dateStr.match(/(\d{2})-(\d{2})-(\d{4})\s+(\d{2}):(\d{2}):(\d{2})/);
  if (match) {
    const [, day, month, year, hour, min, sec] = match;
    return new Date(
      Number(year),
      Number(month) - 1,
      Number(day),
      Number(hour),
      Number(min),
      Number(sec)
    );
  }

  // Fallback to Date constructor
  return new Date(dateStr);
}
