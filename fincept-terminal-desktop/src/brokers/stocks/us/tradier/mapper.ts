/**
 * Tradier Data Mappers
 *
 * Functions to transform between standard types and Tradier API formats
 */

import type {
  OrderParams,
  ModifyOrderParams,
  Order,
  Trade,
  Position,
  Holding,
  Quote,
  OHLCV,
  Funds,
  OrderSide,
  OrderType,
  OrderStatus,
  ProductType,
  StockExchange,
  TimeFrame,
  TickData,
  Instrument,
  InstrumentType,
} from '../../types';

import type {
  TradierOrder,
  TradierPosition,
  TradierQuote,
  TradierHistoricalBar,
  TradierBalances,
  TradierOrderRequest,
  TradierModifyOrderRequest,
  TradierSecurity,
  TradierStreamTrade,
  TradierStreamQuote,
  TradierDuration,
  TradierOrderType,
  TradierSide,
} from './types';

import {
  TRADIER_ORDER_TYPE_MAP,
  TRADIER_ORDER_TYPE_REVERSE_MAP,
  TRADIER_STATUS_MAP,
  TRADIER_SIDE_MAP,
  TRADIER_SIDE_REVERSE_MAP,
  TRADIER_VALIDITY_MAP,
  TRADIER_VALIDITY_REVERSE_MAP,
  TRADIER_EXCHANGE_REVERSE_MAP,
  TRADIER_INTERVAL_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPING
// ============================================================================

/**
 * Map standard order params to Tradier order request format
 */
export function toTradierOrderRequest(params: OrderParams): TradierOrderRequest {
  return {
    class: 'equity',
    symbol: params.symbol,
    side: (TRADIER_SIDE_MAP[params.side] || 'buy') as TradierSide,
    quantity: params.quantity,
    type: (TRADIER_ORDER_TYPE_MAP[params.orderType] || 'market') as TradierOrderType,
    duration: (TRADIER_VALIDITY_MAP[params.validity || 'DAY'] || 'day') as TradierDuration,
    price: params.orderType === 'LIMIT' || params.orderType === 'STOP_LIMIT' ? params.price : undefined,
    stop: params.orderType === 'STOP' || params.orderType === 'STOP_LIMIT' ? params.triggerPrice : undefined,
    tag: params.tag,
  };
}

/**
 * Map standard modify order params to Tradier format
 */
export function toTradierModifyRequest(params: ModifyOrderParams): TradierModifyOrderRequest {
  const request: TradierModifyOrderRequest = {};

  if (params.orderType) {
    request.type = (TRADIER_ORDER_TYPE_MAP[params.orderType] || 'market') as TradierOrderType;
  }
  if (params.validity) {
    request.duration = (TRADIER_VALIDITY_MAP[params.validity] || 'day') as TradierDuration;
  }
  if (params.price !== undefined) {
    request.price = params.price;
  }
  if (params.triggerPrice !== undefined) {
    request.stop = params.triggerPrice;
  }

  return request;
}

/**
 * Map Tradier order to standard Order format
 */
export function fromTradierOrder(tradierOrder: TradierOrder): Order {
  const side = TRADIER_SIDE_REVERSE_MAP[tradierOrder.side] || 'BUY';
  const status = TRADIER_STATUS_MAP[tradierOrder.status] || 'PENDING';
  const orderType = TRADIER_ORDER_TYPE_REVERSE_MAP[tradierOrder.type] || 'MARKET';
  const validity = TRADIER_VALIDITY_REVERSE_MAP[tradierOrder.duration] || 'DAY';

  return {
    orderId: tradierOrder.id.toString(),
    symbol: tradierOrder.symbol,
    exchange: 'NASDAQ' as StockExchange, // Tradier doesn't return exchange, default to NASDAQ
    side: side as OrderSide,
    quantity: tradierOrder.quantity,
    filledQuantity: tradierOrder.exec_quantity || 0,
    pendingQuantity: tradierOrder.remaining_quantity || (tradierOrder.quantity - (tradierOrder.exec_quantity || 0)),
    price: tradierOrder.price || 0,
    triggerPrice: tradierOrder.stop_price,
    averagePrice: tradierOrder.avg_fill_price || 0,
    orderType: orderType as OrderType,
    productType: 'CASH' as ProductType,
    validity: validity as 'DAY' | 'GTC' | 'IOC' | 'GTD',
    status: status as OrderStatus,
    placedAt: new Date(tradierOrder.create_date),
    updatedAt: new Date(tradierOrder.transaction_date),
    tag: tradierOrder.tag,
  };
}

/**
 * Map Tradier order to Trade format (for filled orders)
 */
export function fromTradierOrderToTrade(tradierOrder: TradierOrder): Trade | null {
  // Only convert filled orders to trades
  if (tradierOrder.status !== 'filled' && tradierOrder.status !== 'partially_filled') {
    return null;
  }

  if (!tradierOrder.exec_quantity || tradierOrder.exec_quantity === 0) {
    return null;
  }

  const side = TRADIER_SIDE_REVERSE_MAP[tradierOrder.side] || 'BUY';

  return {
    tradeId: `${tradierOrder.id}-trade`,
    orderId: tradierOrder.id.toString(),
    symbol: tradierOrder.symbol,
    exchange: 'NASDAQ' as StockExchange,
    side: side as OrderSide,
    quantity: tradierOrder.exec_quantity,
    price: tradierOrder.avg_fill_price || tradierOrder.last_fill_price || 0,
    productType: 'CASH' as ProductType,
    tradedAt: new Date(tradierOrder.transaction_date),
  };
}

// ============================================================================
// POSITION MAPPING
// ============================================================================

/**
 * Map Tradier position to standard Position format
 */
export function fromTradierPosition(tradierPos: TradierPosition, quote?: TradierQuote): Position {
  const quantity = tradierPos.quantity;
  const avgPrice = tradierPos.cost_basis / Math.abs(quantity);
  const lastPrice = quote?.last || avgPrice;
  const pnl = (lastPrice - avgPrice) * quantity;
  const pnlPercent = avgPrice !== 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;

  return {
    symbol: tradierPos.symbol,
    exchange: 'NASDAQ' as StockExchange,
    productType: 'CASH' as ProductType,
    quantity: quantity,
    buyQuantity: quantity > 0 ? Math.abs(quantity) : 0,
    sellQuantity: quantity < 0 ? Math.abs(quantity) : 0,
    buyValue: quantity > 0 ? tradierPos.cost_basis : 0,
    sellValue: quantity < 0 ? tradierPos.cost_basis : 0,
    averagePrice: avgPrice,
    lastPrice: lastPrice,
    pnl: pnl,
    pnlPercent: pnlPercent,
    dayPnl: quote?.change ? quote.change * quantity : 0,
  };
}

/**
 * Map Tradier position to Holding format
 */
export function fromTradierPositionToHolding(tradierPos: TradierPosition, quote?: TradierQuote): Holding {
  const quantity = Math.abs(tradierPos.quantity);
  const avgPrice = tradierPos.cost_basis / quantity;
  const lastPrice = quote?.last || avgPrice;
  const investedValue = tradierPos.cost_basis;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue !== 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol: tradierPos.symbol,
    exchange: 'NASDAQ' as StockExchange,
    quantity: quantity,
    averagePrice: avgPrice,
    lastPrice: lastPrice,
    investedValue: investedValue,
    currentValue: currentValue,
    pnl: pnl,
    pnlPercent: pnlPercent,
  };
}

// ============================================================================
// ACCOUNT/FUNDS MAPPING
// ============================================================================

/**
 * Map Tradier balances to standard Funds format
 */
export function fromTradierBalances(balances: TradierBalances['balances']): Funds {
  const isMargin = balances.account_type === 'margin' || balances.account_type === 'pdt';

  return {
    availableCash: balances.total_cash || 0,
    usedMargin: isMargin ? (balances.margin?.stock_buying_power ? balances.equity - balances.margin.stock_buying_power : 0) : 0,
    availableMargin: isMargin ? (balances.margin?.stock_buying_power || 0) : (balances.cash?.cash_available || balances.total_cash || 0),
    totalBalance: balances.total_equity || 0,
    currency: 'USD',
    unrealizedPnl: balances.open_pl || 0,
    realizedPnl: balances.close_pl || 0,
  };
}

// ============================================================================
// MARKET DATA MAPPING
// ============================================================================

/**
 * Map Tradier quote to standard Quote format
 */
export function fromTradierQuote(tradierQuote: TradierQuote): Quote {
  const exchange = TRADIER_EXCHANGE_REVERSE_MAP[tradierQuote.exch] || 'NASDAQ';

  return {
    symbol: tradierQuote.symbol,
    exchange: exchange as StockExchange,
    lastPrice: tradierQuote.last || 0,
    open: tradierQuote.open || 0,
    high: tradierQuote.high || 0,
    low: tradierQuote.low || 0,
    close: tradierQuote.close || tradierQuote.last || 0,
    previousClose: tradierQuote.prevclose || 0,
    change: tradierQuote.change || 0,
    changePercent: tradierQuote.change_percentage || 0,
    volume: tradierQuote.volume || 0,
    bid: tradierQuote.bid || 0,
    bidQty: tradierQuote.bidsize || 0,
    ask: tradierQuote.ask || 0,
    askQty: tradierQuote.asksize || 0,
    timestamp: tradierQuote.trade_date ? tradierQuote.trade_date : Date.now(),
  };
}

/**
 * Map Tradier historical bar to OHLCV format
 */
export function fromTradierBar(bar: TradierHistoricalBar): OHLCV {
  return {
    timestamp: new Date(bar.date).getTime(),
    open: bar.open,
    high: bar.high,
    low: bar.low,
    close: bar.close,
    volume: bar.volume,
  };
}

/**
 * Map standard timeframe to Tradier interval
 */
export function toTradierInterval(timeframe: TimeFrame): string {
  return TRADIER_INTERVAL_MAP[timeframe] || 'daily';
}

// ============================================================================
// STREAMING DATA MAPPING
// ============================================================================

/**
 * Map Tradier stream trade to TickData
 */
export function fromTradierStreamTrade(trade: TradierStreamTrade): TickData {
  return {
    symbol: trade.symbol,
    exchange: (TRADIER_EXCHANGE_REVERSE_MAP[trade.exch] || 'NASDAQ') as StockExchange,
    mode: 'ltp',
    lastPrice: trade.last || trade.price,
    volume: trade.size,
    timestamp: new Date(trade.date).getTime(),
  };
}

/**
 * Map Tradier stream quote to TickData
 */
export function fromTradierStreamQuote(quote: TradierStreamQuote): TickData {
  return {
    symbol: quote.symbol,
    exchange: (TRADIER_EXCHANGE_REVERSE_MAP[quote.bidexch] || 'NASDAQ') as StockExchange,
    mode: 'quote',
    lastPrice: (quote.bid + quote.ask) / 2, // Mid price
    bid: quote.bid,
    bidQty: quote.bidsz,
    ask: quote.ask,
    askQty: quote.asksz,
    timestamp: new Date(quote.biddate).getTime(),
  };
}

// ============================================================================
// SYMBOL/INSTRUMENT MAPPING
// ============================================================================

/**
 * Map Tradier security to Instrument
 */
export function fromTradierSecurity(security: TradierSecurity): Instrument {
  const exchange = TRADIER_EXCHANGE_REVERSE_MAP[security.exchange] || 'NASDAQ';

  let instrumentType: InstrumentType = 'EQUITY';
  if (security.type === 'etf') {
    instrumentType = 'ETF';
  } else if (security.type === 'option') {
    instrumentType = 'OPTION';
  } else if (security.type === 'index') {
    instrumentType = 'INDEX';
  }

  return {
    symbol: security.symbol,
    name: security.description,
    exchange: exchange as StockExchange,
    instrumentType: instrumentType,
    currency: 'USD',
    token: security.symbol,
    lotSize: 1,
    tickSize: 0.01,
  };
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Normalize Tradier API response arrays
 * Tradier returns single items as objects, multiple as arrays
 */
export function normalizeArray<T>(item: T | T[] | null | undefined): T[] {
  if (item === null || item === undefined) {
    return [];
  }
  return Array.isArray(item) ? item : [item];
}

/**
 * Check if account is paper trading based on API base URL
 */
export function isPaperAccount(apiBase: string): boolean {
  return apiBase.includes('sandbox');
}
