/**
 * Alpaca Data Mappers
 *
 * Transform between Alpaca API format and standard stock broker format
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

import type {
  AlpacaOrder,
  AlpacaPosition,
  AlpacaAccount,
  AlpacaBar,
  AlpacaQuote,
  AlpacaSnapshot,
  AlpacaOrderRequest,
  AlpacaModifyOrderRequest,
  AlpacaWSTradeMessage,
  AlpacaWSQuoteMessage,
  AlpacaWSBarMessage,
} from './types';

import {
  ALPACA_ORDER_TYPE_MAP,
  ALPACA_ORDER_TYPE_REVERSE_MAP,
  ALPACA_STATUS_MAP,
  ALPACA_SIDE_MAP,
  ALPACA_SIDE_REVERSE_MAP,
  ALPACA_VALIDITY_MAP,
  ALPACA_VALIDITY_REVERSE_MAP,
  ALPACA_PRODUCT_MAP,
  ALPACA_PRODUCT_REVERSE_MAP,
  ALPACA_TIMEFRAME_MAP,
  ALPACA_EXCHANGE_REVERSE_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to Alpaca format
 */
export function toAlpacaOrderParams(params: OrderParams): AlpacaOrderRequest {
  const alpacaParams: AlpacaOrderRequest = {
    symbol: params.symbol,
    side: (ALPACA_SIDE_MAP[params.side] || 'buy') as 'buy' | 'sell',
    type: (ALPACA_ORDER_TYPE_MAP[params.orderType] || 'market') as AlpacaOrderRequest['type'],
    time_in_force: (ALPACA_VALIDITY_MAP[params.validity || 'DAY'] || 'day') as AlpacaOrderRequest['time_in_force'],
  };

  // Quantity (Alpaca uses string)
  alpacaParams.qty = String(params.quantity);

  // Price for limit orders
  if (params.price && (params.orderType === 'LIMIT' || params.orderType === 'STOP_LIMIT')) {
    alpacaParams.limit_price = String(params.price);
  }

  // Trigger price for stop orders
  if (params.triggerPrice && (params.orderType === 'STOP' || params.orderType === 'STOP_LIMIT')) {
    alpacaParams.stop_price = String(params.triggerPrice);
  }

  // Extended hours trading (pre/post market)
  if (params.amo) {
    alpacaParams.extended_hours = true;
  }

  // Bracket order support
  if (params.squareOff && params.stopLoss) {
    alpacaParams.order_class = 'bracket';
    alpacaParams.take_profit = {
      limit_price: String(params.squareOff),
    };
    alpacaParams.stop_loss = {
      stop_price: String(params.stopLoss),
    };
  }

  return alpacaParams;
}

/**
 * Convert modify order params to Alpaca format
 */
export function toAlpacaModifyParams(params: ModifyOrderParams): AlpacaModifyOrderRequest {
  const alpacaParams: AlpacaModifyOrderRequest = {};

  if (params.quantity !== undefined) {
    alpacaParams.qty = String(params.quantity);
  }

  if (params.price !== undefined) {
    alpacaParams.limit_price = String(params.price);
  }

  if (params.triggerPrice !== undefined) {
    alpacaParams.stop_price = String(params.triggerPrice);
  }

  if (params.validity !== undefined) {
    alpacaParams.time_in_force = ALPACA_VALIDITY_MAP[params.validity] as AlpacaModifyOrderRequest['time_in_force'];
  }

  return alpacaParams;
}

/**
 * Convert Alpaca order to standard format
 */
export function fromAlpacaOrder(order: AlpacaOrder): Order {
  const status = (ALPACA_STATUS_MAP[order.status] || 'PENDING') as OrderStatus;
  const orderType = order.order_type || order.type;

  return {
    orderId: order.id,
    symbol: order.symbol,
    exchange: 'NASDAQ' as StockExchange, // Default, Alpaca doesn't always provide exchange
    side: (ALPACA_SIDE_REVERSE_MAP[order.side] || 'BUY') as OrderSide,
    quantity: Number(order.qty || 0),
    filledQuantity: Number(order.filled_qty || 0),
    pendingQuantity: Number(order.qty || 0) - Number(order.filled_qty || 0),
    price: Number(order.limit_price || 0),
    averagePrice: Number(order.filled_avg_price || 0),
    triggerPrice: order.stop_price ? Number(order.stop_price) : undefined,
    orderType: (ALPACA_ORDER_TYPE_REVERSE_MAP[orderType] || 'MARKET') as OrderType,
    productType: 'CASH' as ProductType, // US doesn't use product types
    validity: (ALPACA_VALIDITY_REVERSE_MAP[order.time_in_force] || 'DAY') as 'DAY' | 'IOC' | 'GTC' | 'GTD',
    status,
    statusMessage: undefined,
    placedAt: new Date(order.submitted_at || order.created_at),
    updatedAt: order.updated_at ? new Date(order.updated_at) : undefined,
    exchangeOrderId: order.client_order_id || undefined,
    tag: order.client_order_id || undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert Alpaca filled order to standard trade format
 * Note: Alpaca doesn't have a separate trades endpoint, trades are derived from filled orders
 */
export function fromAlpacaOrderToTrade(order: AlpacaOrder): Trade | null {
  if (order.status !== 'filled' && order.status !== 'partially_filled') {
    return null;
  }

  return {
    tradeId: `${order.id}-fill`,
    orderId: order.id,
    symbol: order.symbol,
    exchange: 'NASDAQ' as StockExchange,
    side: (ALPACA_SIDE_REVERSE_MAP[order.side] || 'BUY') as OrderSide,
    quantity: Number(order.filled_qty || 0),
    price: Number(order.filled_avg_price || 0),
    productType: 'CASH' as ProductType,
    tradedAt: new Date(order.filled_at || order.updated_at || Date.now()),
    exchangeTradeId: order.id,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert Alpaca position to standard format
 */
export function fromAlpacaPosition(position: AlpacaPosition): Position {
  const qty = Number(position.qty);
  const avgPrice = Number(position.avg_entry_price);
  const currentPrice = Number(position.current_price);
  const pnl = Number(position.unrealized_pl);
  const pnlPercent = Number(position.unrealized_plpc) * 100;
  const dayPnl = Number(position.unrealized_intraday_pl);

  // Determine side (long = positive qty, short = negative)
  const isShort = position.side === 'short';
  const quantity = isShort ? -Math.abs(qty) : Math.abs(qty);

  return {
    symbol: position.symbol,
    exchange: (ALPACA_EXCHANGE_REVERSE_MAP[position.exchange] || 'NASDAQ') as StockExchange,
    productType: 'CASH' as ProductType,
    quantity,
    buyQuantity: isShort ? 0 : Math.abs(qty),
    sellQuantity: isShort ? Math.abs(qty) : 0,
    buyValue: isShort ? 0 : Number(position.cost_basis),
    sellValue: isShort ? Number(position.cost_basis) : 0,
    averagePrice: avgPrice,
    lastPrice: currentPrice,
    pnl,
    pnlPercent,
    dayPnl,
    overnight: true, // Alpaca positions persist
  };
}

// ============================================================================
// HOLDINGS MAPPERS (same as positions for US)
// ============================================================================

/**
 * Convert Alpaca position to standard holding format
 * In US, positions and holdings are essentially the same
 */
export function fromAlpacaPositionToHolding(position: AlpacaPosition): Holding {
  const qty = Number(position.qty);
  const avgPrice = Number(position.avg_entry_price);
  const currentPrice = Number(position.current_price);
  const investedValue = Number(position.cost_basis);
  const currentValue = Number(position.market_value);
  const pnl = Number(position.unrealized_pl);
  const pnlPercent = Number(position.unrealized_plpc) * 100;

  return {
    symbol: position.symbol,
    exchange: (ALPACA_EXCHANGE_REVERSE_MAP[position.exchange] || 'NASDAQ') as StockExchange,
    quantity: Math.abs(qty),
    averagePrice: avgPrice,
    lastPrice: currentPrice,
    investedValue,
    currentValue,
    pnl,
    pnlPercent,
    isin: undefined, // Alpaca uses asset_id instead
  };
}

// ============================================================================
// FUNDS MAPPERS
// ============================================================================

/**
 * Convert Alpaca account to standard funds format
 */
export function fromAlpacaAccount(account: AlpacaAccount): Funds {
  return {
    availableCash: Number(account.cash),
    usedMargin: Number(account.initial_margin) + Number(account.maintenance_margin),
    availableMargin: Number(account.buying_power),
    totalBalance: Number(account.equity),
    currency: 'USD',
    equityAvailable: Number(account.non_marginable_buying_power),
    collateral: 0,
    unrealizedPnl: Number(account.equity) - Number(account.last_equity),
    realizedPnl: 0,
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert Alpaca snapshot to standard quote format
 */
export function fromAlpacaSnapshot(snapshot: AlpacaSnapshot, symbol: string, exchange: StockExchange): Quote {
  const trade = snapshot.latestTrade;
  const quote = snapshot.latestQuote;
  const dailyBar = snapshot.dailyBar;
  const prevBar = snapshot.prevDailyBar;

  const lastPrice = trade?.p || 0;
  const previousClose = prevBar?.c || 0;
  const change = lastPrice - previousClose;
  const changePercent = previousClose > 0 ? (change / previousClose) * 100 : 0;

  return {
    symbol,
    exchange,
    lastPrice,
    open: dailyBar?.o || 0,
    high: dailyBar?.h || 0,
    low: dailyBar?.l || 0,
    close: dailyBar?.c || lastPrice,
    previousClose,
    change,
    changePercent,
    volume: dailyBar?.v || 0,
    value: undefined,
    averagePrice: dailyBar?.vw || undefined,
    bid: quote?.bp || 0,
    bidQty: quote?.bs || 0,
    ask: quote?.ap || 0,
    askQty: quote?.as || 0,
    timestamp: Date.now(),
  };
}

/**
 * Convert Alpaca quote to standard format
 */
export function fromAlpacaQuote(quote: AlpacaQuote, symbol: string, exchange: StockExchange): Quote {
  return {
    symbol,
    exchange,
    lastPrice: (quote.bp + quote.ap) / 2, // Mid price as last
    open: 0,
    high: 0,
    low: 0,
    close: 0,
    previousClose: 0,
    change: 0,
    changePercent: 0,
    volume: 0,
    bid: quote.bp,
    bidQty: quote.bs,
    ask: quote.ap,
    askQty: quote.as,
    timestamp: new Date(quote.t).getTime(),
  };
}

// ============================================================================
// OHLCV MAPPERS
// ============================================================================

/**
 * Convert Alpaca bar to standard OHLCV format
 */
export function fromAlpacaBar(bar: AlpacaBar): OHLCV {
  return {
    timestamp: new Date(bar.t).getTime(),
    open: bar.o,
    high: bar.h,
    low: bar.l,
    close: bar.c,
    volume: bar.v,
  };
}

/**
 * Convert timeframe to Alpaca interval
 */
export function toAlpacaTimeframe(timeframe: TimeFrame): string {
  return ALPACA_TIMEFRAME_MAP[timeframe] || '1Day';
}

// ============================================================================
// DEPTH MAPPERS (Alpaca doesn't provide full depth, only top of book)
// ============================================================================

/**
 * Create market depth from Alpaca quote (top of book only)
 */
export function fromAlpacaQuoteToDepth(quote: AlpacaQuote): MarketDepth {
  const bids: DepthLevel[] = [{
    price: quote.bp,
    quantity: quote.bs,
    orders: 1,
  }];

  const asks: DepthLevel[] = [{
    price: quote.ap,
    quantity: quote.as,
    orders: 1,
  }];

  return { bids, asks };
}

// ============================================================================
// WEBSOCKET MAPPERS
// ============================================================================

/**
 * Convert Alpaca WebSocket trade to tick data
 */
export function fromAlpacaWSTrade(trade: AlpacaWSTradeMessage): TickData {
  return {
    symbol: trade.S,
    exchange: (ALPACA_EXCHANGE_REVERSE_MAP[trade.x] || 'NASDAQ') as StockExchange,
    mode: 'ltp',
    lastPrice: trade.p,
    timestamp: new Date(trade.t).getTime(),
    volume: trade.s,
  };
}

/**
 * Convert Alpaca WebSocket quote to tick data
 */
export function fromAlpacaWSQuote(quote: AlpacaWSQuoteMessage): TickData {
  return {
    symbol: quote.S,
    exchange: (ALPACA_EXCHANGE_REVERSE_MAP[quote.bx] || 'NASDAQ') as StockExchange,
    mode: 'quote',
    lastPrice: (quote.bp + quote.ap) / 2,
    timestamp: new Date(quote.t).getTime(),
    bid: quote.bp,
    bidQty: quote.bs,
    ask: quote.ap,
    askQty: quote.as,
  };
}

/**
 * Convert Alpaca WebSocket bar to tick data
 */
export function fromAlpacaWSBar(bar: AlpacaWSBarMessage): TickData {
  return {
    symbol: bar.S,
    exchange: 'NASDAQ' as StockExchange,
    mode: 'full',
    lastPrice: bar.c,
    timestamp: new Date(bar.t).getTime(),
    open: bar.o,
    high: bar.h,
    low: bar.l,
    close: bar.c,
    volume: bar.v,
  };
}

// ============================================================================
// MARGIN MAPPERS
// ============================================================================

/**
 * Convert order params to Alpaca margin request format
 * Note: Alpaca calculates margin automatically, this is for display purposes
 */
export function toAlpacaMarginParams(params: OrderParams): Record<string, unknown> {
  return {
    symbol: params.symbol,
    qty: String(params.quantity),
    side: ALPACA_SIDE_MAP[params.side] || 'buy',
    type: ALPACA_ORDER_TYPE_MAP[params.orderType] || 'market',
    time_in_force: ALPACA_VALIDITY_MAP[params.validity || 'DAY'] || 'day',
    limit_price: params.price ? String(params.price) : undefined,
    stop_price: params.triggerPrice ? String(params.triggerPrice) : undefined,
  };
}
