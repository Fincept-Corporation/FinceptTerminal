/**
 * Groww Data Mappers
 *
 * Transform between Groww API format and standard stock broker format
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
  GROWW_EXCHANGE_MAP,
  GROWW_SEGMENT_MAP,
  GROWW_PRODUCT_MAP,
  GROWW_PRODUCT_REVERSE_MAP,
  GROWW_ORDER_TYPE_MAP,
  GROWW_ORDER_TYPE_REVERSE_MAP,
  GROWW_STATUS_MAP,
  GROWW_VALIDITY_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to Groww format
 */
export function toGrowwOrderParams(params: OrderParams): Record<string, unknown> {
  const exchange = GROWW_EXCHANGE_MAP[params.exchange] || params.exchange;
  const segment = GROWW_SEGMENT_MAP[params.exchange] || 'CASH';

  return {
    trading_symbol: params.symbol,
    exchange,
    segment,
    transaction_type: params.side,
    quantity: params.quantity,
    order_type: GROWW_ORDER_TYPE_MAP[params.orderType] || params.orderType,
    product: GROWW_PRODUCT_MAP[params.productType] || params.productType,
    price: params.price || 0,
    trigger_price: params.triggerPrice || 0,
    validity: GROWW_VALIDITY_MAP[params.validity || 'DAY'] || 'DAY',
  };
}

/**
 * Convert modify order params to Groww format
 */
export function toGrowwModifyParams(params: ModifyOrderParams): Record<string, unknown> {
  const growwParams: Record<string, unknown> = {};

  if (params.quantity !== undefined) {
    growwParams.quantity = params.quantity;
  }
  if (params.price !== undefined) {
    growwParams.price = params.price;
  }
  if (params.triggerPrice !== undefined) {
    growwParams.trigger_price = params.triggerPrice;
  }
  if (params.orderType !== undefined) {
    growwParams.order_type = GROWW_ORDER_TYPE_MAP[params.orderType] || params.orderType;
  }
  if (params.validity !== undefined) {
    growwParams.validity = GROWW_VALIDITY_MAP[params.validity] || params.validity;
  }

  return growwParams;
}

/**
 * Convert Groww order to standard format
 */
export function fromGrowwOrder(order: Record<string, unknown>): Order {
  const status = String(order.order_status || order.status || 'NEW').toUpperCase();

  // Determine exchange based on segment
  const segment = String(order.segment || 'CASH');
  let exchange = String(order.exchange || 'NSE');
  if (segment === 'FNO' && exchange === 'NSE') {
    exchange = 'NFO';
  } else if (segment === 'FNO' && exchange === 'BSE') {
    exchange = 'BFO';
  }

  return {
    orderId: String(order.groww_order_id || order.orderid || order.order_id || ''),
    symbol: String(order.trading_symbol || order.tradingsymbol || ''),
    exchange: exchange as StockExchange,
    side: String(order.transaction_type || 'BUY').toUpperCase() as OrderSide,
    quantity: Number(order.quantity || 0),
    filledQuantity: Number(order.filled_quantity || 0),
    pendingQuantity: Number(order.quantity || 0) - Number(order.filled_quantity || 0),
    price: Number(order.price || 0),
    averagePrice: Number(order.average_price || order.price || 0),
    triggerPrice: order.trigger_price ? Number(order.trigger_price) : undefined,
    orderType: (GROWW_ORDER_TYPE_REVERSE_MAP[String(order.order_type)] || 'LIMIT') as OrderType,
    productType: (GROWW_PRODUCT_REVERSE_MAP[String(order.product)] || 'CASH') as ProductType,
    validity: String(order.validity || 'DAY') as 'DAY' | 'IOC' | 'GTC' | 'GTD',
    status: (GROWW_STATUS_MAP[status] || 'PENDING') as OrderStatus,
    statusMessage: order.remark ? String(order.remark) : undefined,
    placedAt: new Date(order.created_at as string || Date.now()),
    updatedAt: order.updated_at
      ? new Date(order.updated_at as string)
      : undefined,
    exchangeOrderId: order.exchange_order_id ? String(order.exchange_order_id) : undefined,
    tag: order.order_reference_id ? String(order.order_reference_id) : undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert Groww trade to standard format
 */
export function fromGrowwTrade(trade: Record<string, unknown>): Trade {
  // Determine exchange based on segment
  const segment = String(trade.segment || 'CASH');
  let exchange = String(trade.exchange || 'NSE');
  if (segment === 'FNO' && exchange === 'NSE') {
    exchange = 'NFO';
  }

  return {
    tradeId: String(trade.trade_id || trade.tradeId || ''),
    orderId: String(trade.order_id || trade.orderId || trade.groww_order_id || ''),
    symbol: String(trade.trading_symbol || trade.tradingSymbol || trade.symbol || ''),
    exchange: exchange as StockExchange,
    side: String(trade.transaction_type || trade.transactionType || 'BUY').toUpperCase() as OrderSide,
    quantity: Number(trade.quantity || trade.tradedQuantity || 0),
    price: Number(trade.price || trade.tradedPrice || 0),
    productType: (GROWW_PRODUCT_REVERSE_MAP[String(trade.product || trade.productType)] || 'CASH') as ProductType,
    tradedAt: new Date(trade.trade_date_time as string || trade.updateTime as string || Date.now()),
    exchangeTradeId: trade.exchange_trade_id ? String(trade.exchange_trade_id) : undefined,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert Groww position to standard format
 */
export function fromGrowwPosition(position: Record<string, unknown>): Position {
  const buyQty = Number(position.buy_quantity || position.buyQty || 0);
  const sellQty = Number(position.sell_quantity || position.sellQty || 0);
  const buyValue = Number(position.buy_value || position.buyValue || 0);
  const sellValue = Number(position.sell_value || position.sellValue || 0);
  const netQty = Number(position.quantity || position.net_quantity || 0);
  const lastPrice = Number(position.last_price || position.lastPrice || 0);
  const avgPrice = Number(position.average_price || position.avgPrice || 0);

  // Calculate P&L
  const pnl = Number(position.pnl || position.unrealised || 0);
  const pnlPercent = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;
  const dayPnl = Number(position.day_pnl || position.realised || 0);

  // Determine exchange based on segment
  const segment = String(position.segment || 'CASH');
  let exchange = String(position.exchange || 'NSE');
  if (segment === 'FNO' && exchange === 'NSE') {
    exchange = 'NFO';
  }

  return {
    symbol: String(position.trading_symbol || position.tradingsymbol || ''),
    exchange: exchange as StockExchange,
    productType: (GROWW_PRODUCT_REVERSE_MAP[String(position.product)] || 'INTRADAY') as ProductType,
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
    overnight: false,  // Groww doesn't provide this info directly
  };
}

// ============================================================================
// HOLDING MAPPERS
// ============================================================================

/**
 * Convert Groww holding to standard format
 */
export function fromGrowwHolding(holding: Record<string, unknown>): Holding {
  const quantity = Number(holding.quantity || holding.totalQty || 0);
  const avgPrice = Number(holding.average_price || holding.avgPrice || 0);
  const lastPrice = Number(holding.last_price || holding.ltp || avgPrice);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol: String(holding.symbol || holding.trading_symbol || ''),
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
    t1Quantity: holding.t1_quantity ? Number(holding.t1_quantity) : undefined,
  };
}

// ============================================================================
// FUNDS MAPPERS
// ============================================================================

/**
 * Convert Groww margins to standard funds format
 */
export function fromGrowwFunds(funds: Record<string, unknown>): Funds {
  const availableCash = Number(funds.clear_cash || funds.availablecash || 0);
  const collateral = Number(funds.collateral_available || funds.collateral || 0);
  const usedMargin = Number(funds.net_margin_used || funds.utiliseddebits || 0);
  const unrealizedPnl = Number(funds.m2munrealized || 0);
  const realizedPnl = Number(funds.m2mrealized || 0);

  // Extract equity-specific balances
  const equityMarginDetails = (funds.equity_margin_details as Record<string, unknown>) || {};
  const equityAvailable = Number(equityMarginDetails.cnc_balance_available || 0) +
                          Number(equityMarginDetails.mis_balance_available || 0);

  return {
    availableCash,
    usedMargin,
    availableMargin: availableCash + collateral,
    totalBalance: availableCash + collateral + usedMargin,
    currency: 'INR',
    equityAvailable: equityAvailable || availableCash,
    collateral,
    unrealizedPnl,
    realizedPnl,
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert Groww quote to standard format
 */
export function fromGrowwQuote(quote: Record<string, unknown>, symbol: string, exchange: StockExchange): Quote {
  const ohlc = (quote.ohlc as Record<string, number>) || {};
  const depth = quote.depth as Record<string, unknown[]> | undefined;

  let bid = 0, bidQty = 0, ask = 0, askQty = 0;

  // Get bid/ask from depth or direct fields
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
  } else {
    bid = Number(quote.bid_price || quote.bid || 0);
    bidQty = Number(quote.bid_quantity || quote.bidQty || 0);
    ask = Number(quote.offer_price || quote.ask || 0);
    askQty = Number(quote.offer_quantity || quote.askQty || 0);
  }

  const lastPrice = Number(quote.last_price || quote.ltp || 0);
  const previousClose = Number(ohlc.close || quote.close || quote.prev_close || 0);
  const change = Number(quote.day_change || quote.change || (lastPrice - previousClose));
  const changePercent = Number(quote.day_change_perc || quote.change_percent ||
    (previousClose > 0 ? (change / previousClose) * 100 : 0));

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
    volume: Number(quote.volume || 0),
    bid,
    bidQty,
    ask,
    askQty,
    timestamp: Number(quote.timestamp || Date.now()),
    openInterest: quote.open_interest ? Number(quote.open_interest) : undefined,
  };
}

// ============================================================================
// OHLCV MAPPERS
// ============================================================================

/**
 * Convert Groww OHLCV candle to standard format
 */
export function fromGrowwOHLCV(candle: unknown[] | Record<string, unknown>): OHLCV {
  if (Array.isArray(candle)) {
    // Array format: [timestamp, open, high, low, close, volume]
    let timestamp = Number(candle[0] || 0);
    // Convert from milliseconds if needed
    if (timestamp > 4102444800) {
      timestamp = Math.floor(timestamp / 1000);
    }

    return {
      timestamp: timestamp * 1000, // Convert to milliseconds for Date
      open: Number(candle[1] || 0),
      high: Number(candle[2] || 0),
      low: Number(candle[3] || 0),
      close: Number(candle[4] || 0),
      volume: Number(candle[5] || 0),
    };
  } else {
    // Object format
    return {
      timestamp: Number(candle.timestamp || 0),
      open: Number(candle.open || 0),
      high: Number(candle.high || 0),
      low: Number(candle.low || 0),
      close: Number(candle.close || 0),
      volume: Number(candle.volume || 0),
    };
  }
}

/**
 * Convert timeframe to Groww interval (minutes)
 */
export function toGrowwInterval(timeframe: TimeFrame): string {
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
    '1d': '1440',
    '1w': '10080',
    '1M': '43200',
  };
  return intervalMap[timeframe] || '1440';
}

// ============================================================================
// DEPTH MAPPERS
// ============================================================================

/**
 * Convert Groww depth to standard format
 */
export function fromGrowwDepth(depth: Record<string, unknown>): MarketDepth {
  const buyLevels = (depth.buy as Record<string, unknown>[]) || [];
  const sellLevels = (depth.sell as Record<string, unknown>[]) || [];

  const bids: DepthLevel[] = buyLevels.slice(0, 5).map(level => ({
    price: Number(level.price || 0),
    quantity: Number(level.quantity || 0),
    orders: level.orders ? Number(level.orders) : undefined,
  }));

  const asks: DepthLevel[] = sellLevels.slice(0, 5).map(level => ({
    price: Number(level.price || 0),
    quantity: Number(level.quantity || 0),
    orders: level.orders ? Number(level.orders) : undefined,
  }));

  return { bids, asks };
}

// ============================================================================
// TICK MAPPERS (for REST API polling)
// ============================================================================

/**
 * Convert Groww quote to tick data format
 */
export function fromGrowwTick(
  quote: Record<string, unknown>,
  symbolInfo: { symbol: string; exchange: StockExchange },
  mode: SubscriptionMode
): TickData {
  const ohlc = (quote.ohlc as Record<string, number>) || {};

  const baseTick: TickData = {
    symbol: symbolInfo.symbol,
    exchange: symbolInfo.exchange,
    mode,
    lastPrice: Number(quote.last_price || quote.ltp || 0),
    timestamp: Number(quote.timestamp || Date.now()),
  };

  if (mode === 'quote' || mode === 'full') {
    baseTick.open = Number(ohlc.open || quote.open || 0);
    baseTick.high = Number(ohlc.high || quote.high || 0);
    baseTick.low = Number(ohlc.low || quote.low || 0);
    baseTick.close = Number(ohlc.close || quote.close || 0);
    baseTick.volume = Number(quote.volume || 0);

    const lastPrice = baseTick.lastPrice;
    const close = baseTick.close || 0;
    baseTick.change = lastPrice - close;
    baseTick.changePercent = close > 0 ? ((lastPrice - close) / close) * 100 : 0;
  }

  if (mode === 'full') {
    baseTick.bid = Number(quote.bid_price || 0);
    baseTick.bidQty = Number(quote.bid_quantity || 0);
    baseTick.ask = Number(quote.offer_price || 0);
    baseTick.askQty = Number(quote.offer_quantity || 0);
    baseTick.openInterest = quote.open_interest ? Number(quote.open_interest) : undefined;

    if (quote.depth) {
      baseTick.depth = fromGrowwDepth(quote.depth as Record<string, unknown>);
    }
  }

  return baseTick;
}
