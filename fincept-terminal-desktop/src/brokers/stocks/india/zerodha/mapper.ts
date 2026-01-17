/**
 * Zerodha Data Mappers
 *
 * Transform between Zerodha API format and standard stock broker format
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
  ZERODHA_EXCHANGE_MAP,
  ZERODHA_PRODUCT_MAP,
  ZERODHA_PRODUCT_REVERSE_MAP,
  ZERODHA_ORDER_TYPE_MAP,
  ZERODHA_ORDER_TYPE_REVERSE_MAP,
  ZERODHA_STATUS_MAP,
  ZERODHA_VALIDITY_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to Zerodha format
 */
export function toZerodhaOrderParams(params: OrderParams): Record<string, unknown> {
  return {
    tradingsymbol: params.symbol,
    exchange: ZERODHA_EXCHANGE_MAP[params.exchange] || params.exchange,
    transaction_type: params.side,
    quantity: params.quantity,
    order_type: ZERODHA_ORDER_TYPE_MAP[params.orderType] || params.orderType,
    product: ZERODHA_PRODUCT_MAP[params.productType] || params.productType,
    price: params.price || 0,
    trigger_price: params.triggerPrice || 0,
    validity: ZERODHA_VALIDITY_MAP[params.validity || 'DAY'] || 'DAY',
    disclosed_quantity: params.disclosedQuantity || 0,
    tag: params.tag || '',
  };
}

/**
 * Convert modify order params to Zerodha format
 */
export function toZerodhaModifyParams(params: ModifyOrderParams): Record<string, unknown> {
  const zerodhaParams: Record<string, unknown> = {};

  if (params.quantity !== undefined) {
    zerodhaParams.quantity = params.quantity;
  }
  if (params.price !== undefined) {
    zerodhaParams.price = params.price;
  }
  if (params.triggerPrice !== undefined) {
    zerodhaParams.trigger_price = params.triggerPrice;
  }
  if (params.orderType !== undefined) {
    zerodhaParams.order_type = ZERODHA_ORDER_TYPE_MAP[params.orderType] || params.orderType;
  }
  if (params.validity !== undefined) {
    zerodhaParams.validity = ZERODHA_VALIDITY_MAP[params.validity] || params.validity;
  }

  return zerodhaParams;
}

/**
 * Convert Zerodha order to standard format
 */
export function fromZerodhaOrder(order: Record<string, unknown>): Order {
  const status = String(order.status || 'PENDING').toUpperCase();

  return {
    orderId: String(order.order_id || ''),
    symbol: String(order.tradingsymbol || ''),
    exchange: String(order.exchange || 'NSE') as StockExchange,
    side: String(order.transaction_type || 'BUY').toUpperCase() as OrderSide,
    quantity: Number(order.quantity || 0),
    filledQuantity: Number(order.filled_quantity || 0),
    pendingQuantity: Number(order.pending_quantity || 0),
    price: Number(order.price || 0),
    averagePrice: Number(order.average_price || 0),
    triggerPrice: order.trigger_price ? Number(order.trigger_price) : undefined,
    orderType: (ZERODHA_ORDER_TYPE_REVERSE_MAP[String(order.order_type)] || 'LIMIT') as OrderType,
    productType: (ZERODHA_PRODUCT_REVERSE_MAP[String(order.product)] || 'CASH') as ProductType,
    validity: String(order.validity || 'DAY') as 'DAY' | 'IOC' | 'GTC' | 'GTD',
    status: (ZERODHA_STATUS_MAP[status] || 'PENDING') as OrderStatus,
    statusMessage: order.status_message ? String(order.status_message) : undefined,
    placedAt: new Date(order.order_timestamp as string || Date.now()),
    updatedAt: order.exchange_update_timestamp
      ? new Date(order.exchange_update_timestamp as string)
      : undefined,
    exchangeOrderId: order.exchange_order_id ? String(order.exchange_order_id) : undefined,
    tag: order.tag ? String(order.tag) : undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert Zerodha trade to standard format
 */
export function fromZerodhaTrade(trade: Record<string, unknown>): Trade {
  return {
    tradeId: String(trade.trade_id || ''),
    orderId: String(trade.order_id || ''),
    symbol: String(trade.tradingsymbol || ''),
    exchange: String(trade.exchange || 'NSE') as StockExchange,
    side: String(trade.transaction_type || 'BUY').toUpperCase() as OrderSide,
    quantity: Number(trade.quantity || 0),
    price: Number(trade.average_price || trade.price || 0),
    productType: (ZERODHA_PRODUCT_REVERSE_MAP[String(trade.product)] || 'CASH') as ProductType,
    tradedAt: new Date(trade.fill_timestamp as string || trade.order_timestamp as string || Date.now()),
    exchangeTradeId: trade.exchange_trade_id ? String(trade.exchange_trade_id) : undefined,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert Zerodha position to standard format
 */
export function fromZerodhaPosition(position: Record<string, unknown>): Position {
  const buyQty = Number(position.buy_quantity || 0);
  const sellQty = Number(position.sell_quantity || 0);
  const buyValue = Number(position.buy_value || 0);
  const sellValue = Number(position.sell_value || 0);
  const netQty = Number(position.quantity || 0);
  const lastPrice = Number(position.last_price || 0);
  const avgPrice = Number(position.average_price || 0);

  // Calculate P&L
  const pnl = Number(position.pnl || position.unrealised || 0);
  const pnlPercent = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;
  const dayPnl = Number(position.day_m2m || position.m2m || 0);

  return {
    symbol: String(position.tradingsymbol || ''),
    exchange: String(position.exchange || 'NSE') as StockExchange,
    productType: (ZERODHA_PRODUCT_REVERSE_MAP[String(position.product)] || 'INTRADAY') as ProductType,
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
    overnight: Boolean(position.overnight_quantity),
  };
}

// ============================================================================
// HOLDING MAPPERS
// ============================================================================

/**
 * Convert Zerodha holding to standard format
 */
export function fromZerodhaHolding(holding: Record<string, unknown>): Holding {
  const quantity = Number(holding.quantity || 0);
  const avgPrice = Number(holding.average_price || 0);
  const lastPrice = Number(holding.last_price || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol: String(holding.tradingsymbol || ''),
    exchange: String(holding.exchange || 'NSE') as StockExchange,
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    investedValue,
    currentValue,
    pnl,
    pnlPercent,
    isin: holding.isin ? String(holding.isin) : undefined,
    pledgedQuantity: holding.pledged_quantity ? Number(holding.pledged_quantity) : undefined,
    collateralQuantity: holding.collateral_quantity ? Number(holding.collateral_quantity) : undefined,
    t1Quantity: holding.t1_quantity ? Number(holding.t1_quantity) : undefined,
  };
}

// ============================================================================
// FUNDS MAPPERS
// ============================================================================

/**
 * Convert Zerodha margins to standard funds format
 */
export function fromZerodhaFunds(funds: Record<string, unknown>): Funds {
  // Handle both combined and segment-wise data
  const equity = (funds.equity as Record<string, unknown>) || {};
  const commodity = (funds.commodity as Record<string, unknown>) || {};

  const equityAvailableObj = (equity.available as Record<string, unknown>) || {};
  const commodityAvailableObj = (commodity.available as Record<string, unknown>) || {};
  const equityUtilisedObj = (equity.utilised as Record<string, unknown>) || {};
  const commodityUtilisedObj = (commodity.utilised as Record<string, unknown>) || {};

  const equityAvailable = Number(equityAvailableObj.cash || equity.net || 0);
  const commodityAvailable = Number(commodityAvailableObj.cash || commodity.net || 0);

  const equityUsed = Number(equityUtilisedObj.debits || 0);
  const commodityUsed = Number(commodityUtilisedObj.debits || 0);

  return {
    availableCash: equityAvailable + commodityAvailable,
    usedMargin: equityUsed + commodityUsed,
    availableMargin: equityAvailable + commodityAvailable,
    totalBalance: equityAvailable + commodityAvailable + equityUsed + commodityUsed,
    currency: 'INR',
    equityAvailable,
    commodityAvailable,
    collateral: Number(equityAvailableObj.collateral || 0) + Number(commodityAvailableObj.collateral || 0),
    unrealizedPnl: Number(equityUtilisedObj.m2m_unrealised || 0) + Number(commodityUtilisedObj.m2m_unrealised || 0),
    realizedPnl: Number(equityUtilisedObj.m2m_realised || 0) + Number(commodityUtilisedObj.m2m_realised || 0),
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert Zerodha quote to standard format
 */
export function fromZerodhaQuote(quote: Record<string, unknown>, symbol: string, exchange: StockExchange): Quote {
  const ohlc = (quote.ohlc as Record<string, number>) || {};
  const depth = quote.depth as Record<string, unknown[]> | undefined;

  let bid = 0, bidQty = 0, ask = 0, askQty = 0;

  if (depth) {
    const buyDepth = depth.buy as Record<string, number>[] || [];
    const sellDepth = depth.sell as Record<string, number>[] || [];

    if (buyDepth.length > 0) {
      bid = Number(buyDepth[0].price || 0);
      bidQty = Number(buyDepth[0].quantity || 0);
    }
    if (sellDepth.length > 0) {
      ask = Number(sellDepth[0].price || 0);
      askQty = Number(sellDepth[0].quantity || 0);
    }
  }

  const lastPrice = Number(quote.last_price || 0);
  const previousClose = Number(ohlc.close || quote.close || 0);
  const change = lastPrice - previousClose;
  const changePercent = previousClose > 0 ? (change / previousClose) * 100 : 0;

  return {
    symbol,
    exchange,
    lastPrice,
    open: Number(ohlc.open || quote.open || 0),
    high: Number(ohlc.high || quote.high || 0),
    low: Number(ohlc.low || quote.low || 0),
    close: Number(ohlc.close || quote.close || 0),
    previousClose,
    change,
    changePercent,
    volume: Number(quote.volume || quote.volume_traded || 0),
    value: quote.traded_value ? Number(quote.traded_value) : undefined,
    averagePrice: quote.average_price ? Number(quote.average_price) : undefined,
    bid,
    bidQty,
    ask,
    askQty,
    timestamp: Number(quote.timestamp || Date.now()),
    openInterest: quote.oi ? Number(quote.oi) : undefined,
    oiChange: quote.oi_day_change ? Number(quote.oi_day_change) : undefined,
    upperCircuit: quote.upper_circuit_limit ? Number(quote.upper_circuit_limit) : undefined,
    lowerCircuit: quote.lower_circuit_limit ? Number(quote.lower_circuit_limit) : undefined,
  };
}

// ============================================================================
// OHLCV MAPPERS
// ============================================================================

/**
 * Convert Zerodha OHLCV candle to standard format
 */
export function fromZerodhaOHLCV(candle: unknown[]): OHLCV {
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
 * Convert timeframe to Zerodha interval
 */
export function toZerodhaInterval(timeframe: TimeFrame): string {
  const intervalMap: Record<TimeFrame, string> = {
    '1m': 'minute',
    '3m': '3minute',
    '5m': '5minute',
    '10m': '10minute',
    '15m': '15minute',
    '30m': '30minute',
    '1h': '60minute',
    '2h': '2hour',
    '4h': '4hour',
    '1d': 'day',
    '1w': 'week',
    '1M': 'month',
  };
  return intervalMap[timeframe] || 'day';
}

// ============================================================================
// DEPTH MAPPERS
// ============================================================================

/**
 * Convert Zerodha depth to standard format
 */
export function fromZerodhaDepth(depth: Record<string, unknown>): MarketDepth {
  const buyLevels = (depth.buy as Record<string, unknown>[]) || [];
  const sellLevels = (depth.sell as Record<string, unknown>[]) || [];

  const bids: DepthLevel[] = buyLevels.map(level => ({
    price: Number(level.price || 0),
    quantity: Number(level.quantity || 0),
    orders: level.orders ? Number(level.orders) : undefined,
  }));

  const asks: DepthLevel[] = sellLevels.map(level => ({
    price: Number(level.price || 0),
    quantity: Number(level.quantity || 0),
    orders: level.orders ? Number(level.orders) : undefined,
  }));

  return { bids, asks };
}

// ============================================================================
// WEBSOCKET MAPPERS
// ============================================================================

/**
 * Convert Zerodha tick to standard format
 */
export function fromZerodhaTick(
  tick: Record<string, unknown>,
  symbolInfo: { symbol: string; exchange: StockExchange },
  mode: SubscriptionMode
): TickData {
  const baseTick: TickData = {
    symbol: symbolInfo.symbol,
    exchange: symbolInfo.exchange,
    token: String(tick.instrument_token || ''),
    mode,
    lastPrice: Number(tick.last_traded_price || tick.last_price || 0),
    timestamp: Number(tick.timestamp || Date.now()),
  };

  if (mode === 'quote' || mode === 'full') {
    const ohlc = tick.ohlc as Record<string, number> | undefined;
    baseTick.open = Number(ohlc?.open || tick.open_price || 0);
    baseTick.high = Number(ohlc?.high || tick.high_price || 0);
    baseTick.low = Number(ohlc?.low || tick.low_price || 0);
    baseTick.close = Number(ohlc?.close || tick.close_price || 0);
    baseTick.volume = Number(tick.volume_traded || tick.volume || 0);

    const lastPrice = baseTick.lastPrice;
    const close = baseTick.close || 0;
    baseTick.change = lastPrice - close;
    baseTick.changePercent = close > 0 ? ((lastPrice - close) / close) * 100 : 0;
  }

  if (mode === 'full') {
    baseTick.bid = Number(tick.best_bid_price || 0);
    baseTick.bidQty = Number(tick.best_bid_quantity || 0);
    baseTick.ask = Number(tick.best_ask_price || 0);
    baseTick.askQty = Number(tick.best_ask_quantity || 0);
    baseTick.openInterest = tick.oi ? Number(tick.oi) : undefined;

    if (tick.depth) {
      baseTick.depth = fromZerodhaDepth(tick.depth as Record<string, unknown>);
    }
  }

  return baseTick;
}

/**
 * Convert subscription mode to Zerodha WS mode
 */
export function toZerodhaWSMode(mode: SubscriptionMode): string {
  const modeMap: Record<SubscriptionMode, string> = {
    ltp: 'ltp',
    quote: 'quote',
    full: 'full',
  };
  return modeMap[mode] || 'quote';
}

// ============================================================================
// MARGIN MAPPERS
// ============================================================================

/**
 * Convert order params to Zerodha margin request format
 */
export function toZerodhaMarginParams(params: OrderParams): Record<string, unknown> {
  return {
    exchange: ZERODHA_EXCHANGE_MAP[params.exchange] || params.exchange,
    tradingsymbol: params.symbol,
    transaction_type: params.side,
    variety: 'regular',
    product: ZERODHA_PRODUCT_MAP[params.productType] || params.productType,
    order_type: ZERODHA_ORDER_TYPE_MAP[params.orderType] || params.orderType,
    quantity: params.quantity,
    price: params.price || 0,
    trigger_price: params.triggerPrice || 0,
  };
}
