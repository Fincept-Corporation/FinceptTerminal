/**
 * Angel One Data Mappers
 *
 * Transform between Angel One API format and standard stock broker format
 * Based on OpenAlgo broker integration patterns
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
  ANGELONE_EXCHANGE_MAP,
  ANGELONE_PRODUCT_MAP,
  ANGELONE_PRODUCT_REVERSE_MAP,
  ANGELONE_ORDER_TYPE_MAP,
  ANGELONE_ORDER_TYPE_REVERSE_MAP,
  ANGELONE_VARIETY_MAP,
  ANGELONE_STATUS_MAP,
  ANGELONE_VALIDITY_MAP,
  ANGELONE_INTERVAL_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to Angel One format
 */
export function toAngelOneOrderParams(
  params: OrderParams,
  symbolToken: string
): Record<string, unknown> {
  const orderType = params.orderType || 'MARKET';
  const variety = ANGELONE_VARIETY_MAP[orderType] || 'NORMAL';

  return {
    variety: variety,
    tradingsymbol: params.symbol,
    symboltoken: symbolToken,
    transactiontype: params.side,
    exchange: ANGELONE_EXCHANGE_MAP[params.exchange] || params.exchange,
    ordertype: ANGELONE_ORDER_TYPE_MAP[orderType] || 'MARKET',
    producttype: ANGELONE_PRODUCT_MAP[params.productType] || 'INTRADAY',
    duration: ANGELONE_VALIDITY_MAP[params.validity || 'DAY'] || 'DAY',
    price: String(params.price || 0),
    triggerprice: String(params.triggerPrice || 0),
    squareoff: '0',
    stoploss: '0',
    quantity: String(params.quantity),
    disclosedquantity: String(params.disclosedQuantity || 0),
  };
}

/**
 * Convert modify order params to Angel One format
 */
export function toAngelOneModifyParams(
  orderId: string,
  params: ModifyOrderParams,
  originalOrder: {
    symbol: string;
    exchange: string;
    symbolToken: string;
  }
): Record<string, unknown> {
  const orderType = params.orderType || 'LIMIT';
  const variety = ANGELONE_VARIETY_MAP[orderType] || 'NORMAL';

  return {
    variety: variety,
    orderid: orderId,
    ordertype: params.orderType ? ANGELONE_ORDER_TYPE_MAP[params.orderType] || 'LIMIT' : 'LIMIT',
    producttype: ANGELONE_PRODUCT_MAP['INTRADAY'], // Will be set from original order
    duration: ANGELONE_VALIDITY_MAP[params.validity || 'DAY'] || 'DAY',
    price: String(params.price || 0),
    quantity: String(params.quantity || 0),
    tradingsymbol: originalOrder.symbol,
    symboltoken: originalOrder.symbolToken,
    exchange: originalOrder.exchange,
    disclosedquantity: '0',
    stoploss: String(params.triggerPrice || 0),
  };
}

/**
 * Convert Angel One order to standard format
 */
export function fromAngelOneOrder(order: Record<string, unknown>): Order {
  const status = String(order.status || 'pending').toLowerCase();
  const orderType = String(order.ordertype || 'MARKET');

  // Map Angel One order type back to standard
  let mappedOrderType = ANGELONE_ORDER_TYPE_REVERSE_MAP[orderType] || 'LIMIT';
  if (orderType === 'STOPLOSS_LIMIT') mappedOrderType = 'STOP_LIMIT';
  if (orderType === 'STOPLOSS_MARKET') mappedOrderType = 'STOP_MARKET';

  // Map product type
  const productType = String(order.producttype || 'INTRADAY');
  const mappedProductType = ANGELONE_PRODUCT_REVERSE_MAP[productType] || 'INTRADAY';

  return {
    orderId: String(order.orderid || ''),
    symbol: String(order.tradingsymbol || ''),
    exchange: String(order.exchange || 'NSE') as StockExchange,
    side: String(order.transactiontype || 'BUY').toUpperCase() as OrderSide,
    quantity: Number(order.quantity || 0),
    filledQuantity: Number(order.filledshares || 0),
    pendingQuantity: Number(order.unfilledshares || Number(order.quantity || 0) - Number(order.filledshares || 0)),
    price: Number(order.price || 0),
    averagePrice: Number(order.averageprice || 0),
    triggerPrice: order.triggerprice ? Number(order.triggerprice) : undefined,
    orderType: mappedOrderType as OrderType,
    productType: mappedProductType as ProductType,
    validity: String(order.duration || 'DAY') as 'DAY' | 'IOC' | 'GTC' | 'GTD',
    status: (ANGELONE_STATUS_MAP[status] || 'PENDING') as OrderStatus,
    statusMessage: order.text ? String(order.text) : undefined,
    placedAt: order.orderentryTime
      ? new Date(order.orderentryTime as string)
      : new Date(),
    updatedAt: order.updatetime
      ? new Date(order.updatetime as string)
      : undefined,
    exchangeOrderId: order.exchorderId ? String(order.exchorderId) : undefined,
    tag: order.tag ? String(order.tag) : undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert Angel One trade to standard format
 */
export function fromAngelOneTrade(trade: Record<string, unknown>): Trade {
  const productType = String(trade.producttype || 'INTRADAY');
  const mappedProductType = ANGELONE_PRODUCT_REVERSE_MAP[productType] || 'INTRADAY';

  return {
    tradeId: String(trade.tradeid || trade.fillid || ''),
    orderId: String(trade.orderid || ''),
    symbol: String(trade.tradingsymbol || ''),
    exchange: String(trade.exchange || 'NSE') as StockExchange,
    side: String(trade.transactiontype || 'BUY').toUpperCase() as OrderSide,
    quantity: Number(trade.fillquantity || trade.quantity || 0),
    price: Number(trade.fillprice || trade.price || 0),
    productType: mappedProductType as ProductType,
    tradedAt: trade.filltime
      ? new Date(trade.filltime as string)
      : new Date(),
    exchangeTradeId: trade.exchtradenumber ? String(trade.exchtradenumber) : undefined,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert Angel One position to standard format
 */
export function fromAngelOnePosition(position: Record<string, unknown>): Position {
  const netQty = Number(position.netqty || 0);
  const buyQty = Number(position.buyqty || 0);
  const sellQty = Number(position.sellqty || 0);
  const buyValue = Number(position.totalbuyvalue || 0);
  const sellValue = Number(position.totalsellvalue || 0);
  const avgPrice = Number(position.avgnetprice || position.netprice || 0);
  const lastPrice = Number(position.ltp || 0);
  const pnl = Number(position.pnl || position.unrealised || 0);
  const pnlPercent = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;

  // Map product type
  const productType = String(position.producttype || 'INTRADAY');
  const mappedProductType = ANGELONE_PRODUCT_REVERSE_MAP[productType] || 'INTRADAY';

  return {
    symbol: String(position.tradingsymbol || ''),
    exchange: String(position.exchange || 'NSE') as StockExchange,
    productType: mappedProductType as ProductType,
    quantity: netQty,
    buyQuantity: buyQty,
    sellQuantity: sellQty,
    buyValue,
    sellValue,
    averagePrice: avgPrice,
    lastPrice,
    pnl,
    pnlPercent,
    dayPnl: Number(position.realised || 0),
    overnight: Boolean(position.cfbuyqty || position.cfsellqty),
  };
}

// ============================================================================
// HOLDING MAPPERS
// ============================================================================

/**
 * Convert Angel One holding to standard format
 */
export function fromAngelOneHolding(holding: Record<string, unknown>): Holding {
  const quantity = Number(holding.quantity || 0);
  const avgPrice = Number(holding.averageprice || 0);
  const lastPrice = Number(holding.ltp || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = Number(holding.profitandloss || (currentValue - investedValue));
  const pnlPercent = Number(holding.pnlpercentage || (investedValue > 0 ? (pnl / investedValue) * 100 : 0));

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
    pledgedQuantity: holding.pledgedquantity ? Number(holding.pledgedquantity) : undefined,
    collateralQuantity: holding.collateralqty ? Number(holding.collateralqty) : undefined,
    t1Quantity: holding.t1quantity ? Number(holding.t1quantity) : undefined,
  };
}

// ============================================================================
// FUNDS MAPPERS
// ============================================================================

/**
 * Convert Angel One RMS (Risk Management System) data to standard funds format
 */
export function fromAngelOneFunds(funds: Record<string, unknown>): Funds {
  // Angel One provides funds via getRMS endpoint
  const availableCash = Number(funds.availablecash || funds.net || 0);
  const utilisedDebits = Number(funds.utiliseddebits || 0);
  const utilisedPayout = Number(funds.utilisedpayout || 0);

  // Calculate collateral as availablecash - utilisedpayout (as per OpenAlgo)
  const collateral = availableCash - utilisedPayout;

  const m2mRealized = Number(funds.m2mrealized || 0);
  const m2mUnrealized = Number(funds.m2munrealized || 0);

  return {
    availableCash,
    usedMargin: utilisedDebits,
    availableMargin: availableCash - utilisedDebits,
    totalBalance: availableCash,
    currency: 'INR',
    equityAvailable: availableCash,
    commodityAvailable: 0, // Angel One provides combined margin
    collateral,
    unrealizedPnl: m2mUnrealized,
    realizedPnl: m2mRealized,
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert Angel One quote to standard format
 */
export function fromAngelOneQuote(
  quote: Record<string, unknown>,
  symbol: string,
  exchange: StockExchange
): Quote {
  const depth = quote.depth as Record<string, unknown[]> | undefined;
  const buyDepth = depth?.buy as Record<string, number>[] || [];
  const sellDepth = depth?.sell as Record<string, number>[] || [];

  const bid = buyDepth.length > 0 ? Number(buyDepth[0].price || 0) : 0;
  const bidQty = buyDepth.length > 0 ? Number(buyDepth[0].quantity || 0) : 0;
  const ask = sellDepth.length > 0 ? Number(sellDepth[0].price || 0) : 0;
  const askQty = sellDepth.length > 0 ? Number(sellDepth[0].quantity || 0) : 0;

  const lastPrice = Number(quote.ltp || 0);
  const previousClose = Number(quote.close || 0);
  const change = lastPrice - previousClose;
  const changePercent = previousClose > 0 ? (change / previousClose) * 100 : 0;

  return {
    symbol,
    exchange,
    lastPrice,
    open: Number(quote.open || 0),
    high: Number(quote.high || 0),
    low: Number(quote.low || 0),
    close: lastPrice,
    previousClose,
    change,
    changePercent,
    volume: Number(quote.tradeVolume || quote.volume || 0),
    averagePrice: quote.avgTradedPrice ? Number(quote.avgTradedPrice) : undefined,
    bid,
    bidQty,
    ask,
    askQty,
    timestamp: Date.now(),
    openInterest: quote.opnInterest ? Number(quote.opnInterest) : undefined,
  };
}

// ============================================================================
// OHLCV MAPPERS
// ============================================================================

/**
 * Convert Angel One candle data to standard OHLCV format
 * Angel One returns: [timestamp, open, high, low, close, volume]
 */
export function fromAngelOneOHLCV(candle: unknown[]): OHLCV {
  // Angel One returns timestamp as ISO string
  const timestamp = typeof candle[0] === 'string'
    ? new Date(candle[0]).getTime()
    : Number(candle[0]);

  return {
    timestamp,
    open: Number(candle[1] || 0),
    high: Number(candle[2] || 0),
    low: Number(candle[3] || 0),
    close: Number(candle[4] || 0),
    volume: Number(candle[5] || 0),
  };
}

/**
 * Convert timeframe to Angel One interval
 */
export function toAngelOneInterval(timeframe: TimeFrame): string {
  return ANGELONE_INTERVAL_MAP[timeframe] || 'ONE_DAY';
}

// ============================================================================
// DEPTH MAPPERS
// ============================================================================

/**
 * Convert Angel One depth to standard format
 */
export function fromAngelOneDepth(depth: Record<string, unknown>): MarketDepth {
  const buyLevels = (depth.buy as Record<string, unknown>[]) || [];
  const sellLevels = (depth.sell as Record<string, unknown>[]) || [];

  const bids: DepthLevel[] = [];
  const asks: DepthLevel[] = [];

  // Ensure exactly 5 levels for bids
  for (let i = 0; i < 5; i++) {
    if (i < buyLevels.length) {
      bids.push({
        price: Number(buyLevels[i].price || 0),
        quantity: Number(buyLevels[i].quantity || 0),
        orders: buyLevels[i].orders ? Number(buyLevels[i].orders) : undefined,
      });
    } else {
      bids.push({ price: 0, quantity: 0 });
    }
  }

  // Ensure exactly 5 levels for asks
  for (let i = 0; i < 5; i++) {
    if (i < sellLevels.length) {
      asks.push({
        price: Number(sellLevels[i].price || 0),
        quantity: Number(sellLevels[i].quantity || 0),
        orders: sellLevels[i].orders ? Number(sellLevels[i].orders) : undefined,
      });
    } else {
      asks.push({ price: 0, quantity: 0 });
    }
  }

  return { bids, asks };
}

// ============================================================================
// WEBSOCKET MAPPERS
// ============================================================================

/**
 * Convert Angel One tick to standard format
 */
export function fromAngelOneTick(
  tick: Record<string, unknown>,
  symbolInfo: { symbol: string; exchange: StockExchange },
  mode: SubscriptionMode
): TickData {
  const baseTick: TickData = {
    symbol: symbolInfo.symbol,
    exchange: symbolInfo.exchange,
    token: String(tick.token || ''),
    mode,
    lastPrice: Number(tick.ltp || tick.last_traded_price || 0),
    timestamp: Number(tick.exchange_timestamp || tick.timestamp || Date.now()),
  };

  if (mode === 'quote' || mode === 'full') {
    baseTick.open = Number(tick.open_price_of_the_day || tick.open || 0);
    baseTick.high = Number(tick.high_price_of_the_day || tick.high || 0);
    baseTick.low = Number(tick.low_price_of_the_day || tick.low || 0);
    baseTick.close = Number(tick.closed_price || tick.close || 0);
    baseTick.volume = Number(tick.volume_trade_for_the_day || tick.volume || 0);

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
    baseTick.openInterest = tick.open_interest ? Number(tick.open_interest) : undefined;
  }

  return baseTick;
}

/**
 * Convert subscription mode to Angel One WS mode
 */
export function toAngelOneWSMode(mode: SubscriptionMode): number {
  const modeMap: Record<SubscriptionMode, number> = {
    ltp: 1,     // LTP mode
    quote: 2,   // Quote mode
    full: 3,    // Snap Quote mode (full depth)
  };
  return modeMap[mode] || 2;
}

// ============================================================================
// SYMBOL MAPPERS
// ============================================================================

/**
 * Map order data for database lookup (OpenAlgo compatible)
 * Converts Angel One response to standardized format
 */
export function mapOrderData(orderData: Record<string, unknown>): Record<string, unknown>[] {
  if (!orderData || !orderData.data || orderData.data === null) {
    return [];
  }

  const orders = orderData.data as Record<string, unknown>[];

  return orders.map(order => {
    const productType = String(order.producttype || 'INTRADAY');
    const exchange = String(order.exchange || 'NSE');

    // Map product type based on exchange
    let mappedProduct = productType;
    if ((exchange === 'NSE' || exchange === 'BSE') && productType === 'DELIVERY') {
      mappedProduct = 'CNC';
    } else if (productType === 'INTRADAY') {
      mappedProduct = 'MIS';
    } else if (['NFO', 'MCX', 'BFO', 'CDS'].includes(exchange) && productType === 'CARRYFORWARD') {
      mappedProduct = 'NRML';
    }

    return {
      ...order,
      producttype: mappedProduct,
    };
  });
}

/**
 * Transform order data to standard OpenAlgo format
 */
export function transformOrderData(orders: Record<string, unknown>[]): Record<string, unknown>[] {
  return orders.map(order => {
    let orderType = String(order.ordertype || 'MARKET');
    if (orderType === 'STOPLOSS_LIMIT') orderType = 'SL';
    if (orderType === 'STOPLOSS_MARKET') orderType = 'SL-M';

    return {
      symbol: order.tradingsymbol || '',
      exchange: order.exchange || '',
      action: order.transactiontype || '',
      quantity: order.quantity || 0,
      price: order.averageprice || 0,
      trigger_price: order.triggerprice || 0,
      pricetype: orderType,
      product: order.producttype || '',
      orderid: order.orderid || '',
      order_status: order.status || '',
      timestamp: order.updatetime || '',
    };
  });
}
