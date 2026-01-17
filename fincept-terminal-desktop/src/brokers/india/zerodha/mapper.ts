/**
 * Zerodha data transformation and mapping utilities
 */

import type {
  Order,
  Position,
  Holding,
  FundsData,
  Quote,
  OHLCV,
  MarketDepth,
  MarketDepthLevel,
  OrderParams,
  ModifyOrderParams,
  SymbolToken,
  IndianExchange,
  TickData,
  SubscriptionMode,
} from '../types';
import {
  standardizeOrderStatus,
  standardizeOrderType,
  standardizeProductType,
  standardizeTransactionType,
} from '../utils/orderTransformer';
import { parseExpiry } from '../utils/symbolMapper';
import { ZERODHA_EXCHANGE_MAP, ZERODHA_TIMEFRAME_MAP } from './constants';

// ============================================================================
// Request Transformers
// ============================================================================

/**
 * Transform order params to Zerodha format
 */
export function toZerodhaOrderParams(params: OrderParams): Record<string, unknown> {
  return {
    tradingsymbol: params.symbol,
    exchange: ZERODHA_EXCHANGE_MAP[params.exchange] || params.exchange,
    transaction_type: params.transactionType,
    order_type: params.orderType,
    product: params.productType,
    quantity: params.quantity,
    price: params.price || 0,
    trigger_price: params.triggerPrice || 0,
    disclosed_quantity: params.disclosedQuantity || 0,
    validity: params.validity || 'DAY',
    tag: params.tag || '',
  };
}

/**
 * Transform modify order params to Zerodha format
 */
export function toZerodhaModifyParams(params: ModifyOrderParams): Record<string, unknown> {
  const zerodhaParams: Record<string, unknown> = {};

  if (params.orderType) zerodhaParams.order_type = params.orderType;
  if (params.quantity) zerodhaParams.quantity = params.quantity;
  if (params.price !== undefined) zerodhaParams.price = params.price;
  if (params.triggerPrice !== undefined) zerodhaParams.trigger_price = params.triggerPrice;
  if (params.validity) zerodhaParams.validity = params.validity;

  return zerodhaParams;
}

/**
 * Get Zerodha interval string from timeframe
 */
export function toZerodhaInterval(timeframe: string): string {
  return ZERODHA_TIMEFRAME_MAP[timeframe] || 'day';
}

// ============================================================================
// Response Transformers
// ============================================================================

/**
 * Transform Zerodha order response to standard format
 */
export function fromZerodhaOrder(order: Record<string, unknown>): Order {
  return {
    orderId: String(order.order_id || ''),
    exchangeOrderId: String(order.exchange_order_id || ''),
    symbol: String(order.tradingsymbol || ''),
    exchange: mapZerodhaExchange(String(order.exchange || 'NSE')),
    transactionType: standardizeTransactionType(
      String(order.transaction_type || 'BUY'),
      'zerodha'
    ),
    orderType: standardizeOrderType(String(order.order_type || 'MARKET'), 'zerodha'),
    productType: standardizeProductType(String(order.product || 'CNC'), 'zerodha'),
    quantity: Number(order.quantity || 0),
    filledQuantity: Number(order.filled_quantity || 0),
    pendingQuantity: Number(order.pending_quantity || 0),
    price: Number(order.price || 0),
    averagePrice: Number(order.average_price || 0),
    triggerPrice: Number(order.trigger_price || 0),
    status: standardizeOrderStatus(String(order.status || 'PENDING'), 'zerodha'),
    statusMessage: String(order.status_message || ''),
    orderTimestamp: order.order_timestamp
      ? new Date(order.order_timestamp as string).getTime()
      : Date.now(),
    exchangeTimestamp: order.exchange_timestamp
      ? new Date(order.exchange_timestamp as string).getTime()
      : undefined,
    tag: String(order.tag || ''),
  };
}

/**
 * Transform Zerodha position to standard format
 */
export function fromZerodhaPosition(position: Record<string, unknown>): Position {
  const avgPrice = Number(position.average_price || 0);
  const lastPrice = Number(position.last_price || 0);
  const quantity = Number(position.quantity || 0);
  const pnl = Number(position.pnl || position.unrealised || 0);
  const pnlPercentage = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;

  return {
    symbol: String(position.tradingsymbol || ''),
    exchange: mapZerodhaExchange(String(position.exchange || 'NSE')),
    productType: standardizeProductType(String(position.product || 'MIS'), 'zerodha'),
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    pnl,
    pnlPercentage,
    dayPnl: Number(position.day_m2m || 0),
    buyQuantity: Number(position.day_buy_quantity || 0),
    sellQuantity: Number(position.day_sell_quantity || 0),
    buyValue: Number(position.day_buy_value || 0),
    sellValue: Number(position.day_sell_value || 0),
    multiplier: Number(position.multiplier || 1),
  };
}

/**
 * Transform Zerodha holding to standard format
 */
export function fromZerodhaHolding(holding: Record<string, unknown>): Holding {
  const avgPrice = Number(holding.average_price || 0);
  const lastPrice = Number(holding.last_price || 0);
  const quantity = Number(holding.quantity || 0);
  const pnl = (lastPrice - avgPrice) * quantity;
  const pnlPercentage = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;
  const close = Number(holding.close_price || lastPrice);
  const dayChange = lastPrice - close;
  const dayChangePercentage = close > 0 ? (dayChange / close) * 100 : 0;

  return {
    symbol: String(holding.tradingsymbol || ''),
    exchange: mapZerodhaExchange(String(holding.exchange || 'NSE')),
    isin: String(holding.isin || ''),
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    pnl,
    pnlPercentage,
    dayChange,
    dayChangePercentage,
    pledgedQuantity: Number(holding.pledged_quantity || 0),
    collateralQuantity: Number(holding.collateral_quantity || 0),
  };
}

/**
 * Transform Zerodha funds/margins to standard format
 */
export function fromZerodhaFunds(data: Record<string, unknown>): FundsData {
  const equity = (data.equity as Record<string, unknown>) || {};
  const commodity = (data.commodity as Record<string, unknown>) || {};

  const equityNet = Number(equity.net || 0);
  const commodityNet = Number(commodity.net || 0);

  const equityUtilised = (equity.utilised as Record<string, number>) || {};
  const commodityUtilised = (commodity.utilised as Record<string, number>) || {};

  const equityAvailable = (equity.available as Record<string, number>) || {};
  const commodityAvailable = (commodity.available as Record<string, number>) || {};

  return {
    availableCash: equityNet + commodityNet,
    usedMargin: (equityUtilised.debits || 0) + (commodityUtilised.debits || 0),
    availableMargin: equityNet + commodityNet,
    collateral: (equityAvailable.collateral || 0) + (commodityAvailable.collateral || 0),
    unrealizedMtm:
      (equityUtilised.m2m_unrealised || 0) + (commodityUtilised.m2m_unrealised || 0),
    realizedMtm: (equityUtilised.m2m_realised || 0) + (commodityUtilised.m2m_realised || 0),
  };
}

/**
 * Transform Zerodha quote to standard format
 */
export function fromZerodhaQuote(
  quote: Record<string, unknown>,
  symbol: string,
  exchange: IndianExchange
): Quote {
  const ohlc = (quote.ohlc as Record<string, number>) || {};

  return {
    symbol,
    exchange,
    lastPrice: Number(quote.last_price || 0),
    open: Number(ohlc.open || 0),
    high: Number(ohlc.high || 0),
    low: Number(ohlc.low || 0),
    close: Number(ohlc.close || 0),
    volume: Number(quote.volume || 0),
    change: Number(quote.change || 0),
    changePercent: Number(quote.change_percent || 0),
    averagePrice: Number(quote.average_price || 0),
    lastTradedQuantity: Number(quote.last_quantity || 0),
    totalBuyQuantity: Number(quote.buy_quantity || 0),
    totalSellQuantity: Number(quote.sell_quantity || 0),
    openInterest: Number(quote.oi || 0),
    timestamp: quote.last_trade_time
      ? new Date(quote.last_trade_time as string).getTime()
      : Date.now(),
  };
}

/**
 * Transform Zerodha historical data to OHLCV format
 */
export function fromZerodhaOHLCV(candle: unknown[]): OHLCV {
  // Zerodha candle format: [datetime, open, high, low, close, volume]
  return {
    timestamp: new Date(candle[0] as string).getTime(),
    open: Number(candle[1]),
    high: Number(candle[2]),
    low: Number(candle[3]),
    close: Number(candle[4]),
    volume: Number(candle[5]),
  };
}

/**
 * Transform Zerodha market depth to standard format
 */
export function fromZerodhaDepth(depth: Record<string, unknown>): MarketDepth {
  const buy = (depth.buy as unknown[]) || [];
  const sell = (depth.sell as unknown[]) || [];

  return {
    buy: buy.map((level) => fromZerodhaDepthLevel(level as Record<string, unknown>)),
    sell: sell.map((level) => fromZerodhaDepthLevel(level as Record<string, unknown>)),
  };
}

function fromZerodhaDepthLevel(level: Record<string, unknown>): MarketDepthLevel {
  return {
    price: Number(level.price || 0),
    quantity: Number(level.quantity || 0),
    orders: Number(level.orders || 0),
  };
}

// ============================================================================
// Master Contract Transformers
// ============================================================================

/**
 * Transform Zerodha instrument to SymbolToken
 */
export function fromZerodhaInstrument(instrument: Record<string, unknown>): SymbolToken {
  const exchange = String(instrument.exchange || 'NSE');
  const segment = String(instrument.segment || '');

  // Determine the mapped exchange (handle indices)
  let mappedExchange: IndianExchange = exchange as IndianExchange;
  if (segment === 'INDICES') {
    if (exchange === 'NSE') mappedExchange = 'NSE_INDEX';
    else if (exchange === 'BSE') mappedExchange = 'BSE_INDEX';
    else if (exchange === 'MCX') mappedExchange = 'MCX_INDEX';
    else if (exchange === 'CDS') mappedExchange = 'CDS_INDEX';
  }

  // Build token combining instrument_token and exchange_token
  const instrumentToken = String(instrument.instrument_token || '');
  const exchangeToken = String(instrument.exchange_token || '');
  const token = `${instrumentToken}::::${exchangeToken}`;

  return {
    symbol: buildStandardSymbol(instrument),
    brokerSymbol: String(instrument.tradingsymbol || ''),
    name: String(instrument.name || ''),
    exchange: mappedExchange,
    brokerExchange: exchange,
    token,
    expiry: parseExpiry(instrument.expiry as string | null),
    strike: instrument.strike ? Number(instrument.strike) : undefined,
    lotSize: Number(instrument.lot_size || 1),
    instrumentType: mapInstrumentType(String(instrument.instrument_type || 'EQ')),
    tickSize: Number(instrument.tick_size || 0.05),
  };
}

/**
 * Build standardized symbol from Zerodha instrument
 */
function buildStandardSymbol(instrument: Record<string, unknown>): string {
  const tradingsymbol = String(instrument.tradingsymbol || '');
  const instrumentType = String(instrument.instrument_type || 'EQ');
  const name = String(instrument.name || tradingsymbol);

  // For equity, use tradingsymbol as-is
  if (instrumentType === 'EQ') {
    return tradingsymbol;
  }

  // For futures
  if (instrumentType === 'FUT') {
    const expiry = parseExpiry(instrument.expiry as string).replace(/-/g, '');
    return `${name}${expiry}FUT`;
  }

  // For options
  if (instrumentType === 'CE' || instrumentType === 'PE') {
    const expiry = parseExpiry(instrument.expiry as string).replace(/-/g, '');
    const strike = String(Math.floor(Number(instrument.strike || 0)));
    return `${name}${expiry}${strike}${instrumentType}`;
  }

  // Default
  return tradingsymbol;
}

/**
 * Map Zerodha instrument type to standard type
 */
function mapInstrumentType(type: string): import('../types').InstrumentType {
  const typeMap: Record<string, import('../types').InstrumentType> = {
    EQ: 'EQ',
    FUT: 'FUT',
    CE: 'CE',
    PE: 'PE',
    FUTIDX: 'FUTIDX',
    OPTIDX: 'OPTIDX',
    FUTSTK: 'FUTSTK',
    OPTSTK: 'OPTSTK',
  };
  return typeMap[type] || 'EQ';
}

/**
 * Map Zerodha exchange to standard exchange
 */
function mapZerodhaExchange(exchange: string): IndianExchange {
  // Reverse mapping - Zerodha doesn't use _INDEX suffixes
  return exchange as IndianExchange;
}

// ============================================================================
// WebSocket Tick Transformers
// ============================================================================

/**
 * Transform Zerodha WebSocket tick to standard format
 */
export function fromZerodhaTick(
  tick: Record<string, unknown>,
  symbolInfo: { symbol: string; exchange: IndianExchange },
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

  // Add quote mode fields
  if (mode === 'quote' || mode === 'full') {
    const ohlc = tick.ohlc as Record<string, number> | undefined;
    baseTick.open = Number(ohlc?.open || tick.open_price || 0);
    baseTick.high = Number(ohlc?.high || tick.high_price || 0);
    baseTick.low = Number(ohlc?.low || tick.low_price || 0);
    baseTick.close = Number(ohlc?.close || tick.close_price || 0);
    baseTick.volume = Number(tick.volume_traded || tick.volume || 0);
    baseTick.averagePrice = Number(tick.average_traded_price || tick.average_price || 0);
    baseTick.lastTradedQuantity = Number(tick.last_traded_quantity || 0);
    baseTick.totalBuyQuantity = Number(tick.total_buy_quantity || 0);
    baseTick.totalSellQuantity = Number(tick.total_sell_quantity || 0);
    baseTick.openInterest = Number(tick.oi || tick.open_interest || 0);
  }

  // Add depth for full mode
  if (mode === 'full' && tick.depth) {
    baseTick.depth = fromZerodhaDepth(tick.depth as Record<string, unknown>);
  }

  return baseTick;
}

/**
 * Get Zerodha WebSocket mode string
 */
export function toZerodhaWSMode(mode: SubscriptionMode): string {
  const modeMap: Record<SubscriptionMode, string> = {
    ltp: 'ltp',
    quote: 'quote',
    full: 'full',
  };
  return modeMap[mode] || 'quote';
}

/**
 * Parse Zerodha WebSocket mode string
 */
export function fromZerodhaWSMode(mode: string): SubscriptionMode {
  const modeMap: Record<string, SubscriptionMode> = {
    ltp: 'ltp',
    quote: 'quote',
    full: 'full',
  };
  return modeMap[mode] || 'quote';
}

// ============================================================================
// Trade Book Transformers
// ============================================================================

/**
 * Transform Zerodha trade to standard format
 */
export function fromZerodhaTrade(trade: Record<string, unknown>): import('../types').Trade {
  return {
    tradeId: String(trade.trade_id || ''),
    orderId: String(trade.order_id || ''),
    symbol: String(trade.tradingsymbol || ''),
    exchange: mapZerodhaExchange(String(trade.exchange || 'NSE')),
    transactionType: String(trade.transaction_type || 'BUY').toUpperCase() as import('../types').TransactionType,
    quantity: Number(trade.quantity || 0),
    price: Number(trade.average_price || trade.price || 0),
    product: String(trade.product || 'CNC') as import('../types').ProductType,
    tradeTime: new Date(trade.fill_timestamp as string || trade.order_timestamp as string || Date.now()),
    exchangeOrderId: trade.exchange_order_id ? String(trade.exchange_order_id) : undefined,
    exchangeTradeId: trade.exchange_trade_id ? String(trade.exchange_trade_id) : undefined,
  };
}

// ============================================================================
// Margin Calculator Transformers
// ============================================================================

/**
 * Transform margin order params to Zerodha format
 */
export function toZerodhaMarginParams(params: import('../types').MarginOrderParams): Record<string, unknown> {
  return {
    exchange: params.exchange,
    tradingsymbol: params.symbol,
    transaction_type: params.transactionType,
    variety: 'regular',
    product: params.product,
    order_type: params.orderType,
    quantity: params.quantity,
    price: params.price || 0,
    trigger_price: params.triggerPrice || 0,
  };
}
