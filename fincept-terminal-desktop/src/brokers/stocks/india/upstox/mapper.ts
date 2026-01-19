/**
 * Upstox Data Mappers
 *
 * Transform data between Upstox API format and internal format
 */

import type {
  OrderParams,
  ModifyOrderParams,
  Order,
  Trade,
  Position,
  Holding,
  Funds,
  Quote,
  StockExchange,
  ProductType,
} from '../../types';
import {
  UPSTOX_EXCHANGE_MAP,
  UPSTOX_EXCHANGE_REVERSE_MAP,
  UPSTOX_PRODUCT_MAP,
  UPSTOX_PRODUCT_REVERSE_MAP,
  UPSTOX_ORDER_TYPE_MAP,
} from './constants';

/**
 * Convert internal exchange to Upstox segment
 */
export function toUpstoxExchange(exchange: StockExchange): string {
  return UPSTOX_EXCHANGE_MAP[exchange] || exchange;
}

/**
 * Convert Upstox segment to internal exchange
 */
export function fromUpstoxExchange(segment: string): StockExchange {
  return UPSTOX_EXCHANGE_REVERSE_MAP[segment] || (segment as StockExchange);
}

/**
 * Convert internal product type to Upstox product
 */
export function toUpstoxProduct(product: string): string {
  return UPSTOX_PRODUCT_MAP[product] || 'I';
}

/**
 * Convert Upstox product to internal product type
 * Returns unified ProductType ('CASH', 'INTRADAY', 'MARGIN')
 */
export function fromUpstoxProduct(product: string, exchange: string): ProductType {
  // Map broker-specific product codes to unified ProductType
  // Upstox uses: D (Delivery), I (Intraday)
  const internalExchange = fromUpstoxExchange(exchange);

  if (product === 'D') {
    // Delivery - CASH for equity, MARGIN for F&O
    if (internalExchange === 'NFO' || internalExchange === 'BFO' || internalExchange === 'MCX' || internalExchange === 'CDS') {
      return 'MARGIN';
    }
    return 'CASH';
  }

  // I = Intraday
  return 'INTRADAY';
}

/**
 * Convert internal order type to Upstox order type
 */
export function toUpstoxOrderType(orderType: string): string {
  return UPSTOX_ORDER_TYPE_MAP[orderType] || 'MARKET';
}

/**
 * Convert order params to Upstox format
 */
export function toUpstoxOrderParams(params: OrderParams): Record<string, unknown> {
  return {
    instrument_token: params.symbol, // Will be replaced with actual token
    quantity: params.quantity,
    transaction_type: params.side,
    order_type: toUpstoxOrderType(params.orderType),
    product: toUpstoxProduct(params.productType),
    price: params.price || 0,
    trigger_price: params.triggerPrice || 0,
    disclosed_quantity: 0,
    is_amo: false,
  };
}

/**
 * Convert modify order params to Upstox format
 */
export function toUpstoxModifyParams(orderId: string, params: ModifyOrderParams): Record<string, unknown> {
  const result: Record<string, unknown> = {
    order_id: orderId,
    validity: 'DAY',
  };

  if (params.quantity !== undefined) {
    result.quantity = params.quantity;
  }
  if (params.price !== undefined) {
    result.price = params.price;
  }
  if (params.triggerPrice !== undefined) {
    result.trigger_price = params.triggerPrice;
  }
  if (params.orderType !== undefined) {
    result.order_type = toUpstoxOrderType(params.orderType);
  }

  return result;
}

/**
 * Convert Upstox order to internal format
 */
export function fromUpstoxOrder(data: Record<string, unknown>): Order {
  const exchange = String(data.exchange || 'NSE');
  const product = String(data.product || 'I');

  return {
    orderId: String(data.order_id || ''),
    symbol: String(data.trading_symbol || data.tradingsymbol || ''),
    exchange: fromUpstoxExchange(exchange),
    side: String(data.transaction_type || 'BUY') as 'BUY' | 'SELL',
    quantity: Number(data.quantity || 0),
    filledQuantity: Number(data.filled_quantity || 0),
    pendingQuantity: Number(data.pending_quantity || data.quantity || 0) - Number(data.filled_quantity || 0),
    price: Number(data.price || 0),
    averagePrice: Number(data.average_price || 0),
    orderType: String(data.order_type || 'MARKET') as any,
    productType: fromUpstoxProduct(product, exchange),
    validity: 'DAY',
    status: mapOrderStatus(String(data.status || '')),
    placedAt: data.order_timestamp ? new Date(String(data.order_timestamp)) : new Date(),
    statusMessage: String(data.status_message || ''),
  };
}

/**
 * Map Upstox order status to internal status
 */
function mapOrderStatus(status: string): Order['status'] {
  const statusMap: Record<string, Order['status']> = {
    open: 'OPEN',
    complete: 'FILLED',
    rejected: 'REJECTED',
    cancelled: 'CANCELLED',
    'trigger pending': 'TRIGGER_PENDING',
    'not modified': 'OPEN',
    'modify pending': 'PENDING',
    'cancel pending': 'PENDING',
    'put order req received': 'PENDING',
  };
  return statusMap[status.toLowerCase()] || 'PENDING';
}

/**
 * Convert Upstox trade to internal format
 */
export function fromUpstoxTrade(data: Record<string, unknown>): Trade {
  const exchange = String(data.exchange || 'NSE');
  const product = String(data.product || 'I');

  return {
    tradeId: String(data.trade_id || ''),
    orderId: String(data.order_id || ''),
    symbol: String(data.trading_symbol || data.tradingsymbol || ''),
    exchange: fromUpstoxExchange(exchange),
    side: String(data.transaction_type || 'BUY') as 'BUY' | 'SELL',
    quantity: Number(data.quantity || 0),
    price: Number(data.average_price || data.price || 0),
    productType: fromUpstoxProduct(product, exchange),
    tradedAt: data.order_timestamp ? new Date(String(data.order_timestamp)) : new Date(),
  };
}

/**
 * Convert Upstox position to internal format
 */
export function fromUpstoxPosition(data: Record<string, unknown>): Position {
  const exchange = String(data.exchange || 'NSE');
  const product = String(data.product || 'I');
  const quantity = Number(data.quantity || 0);

  // Handle null average_price
  let avgPrice = data.average_price;
  if (avgPrice === null || avgPrice === undefined || avgPrice === 0) {
    if (quantity > 0) {
      avgPrice = data.buy_price || data.day_buy_price || 0;
    } else if (quantity < 0) {
      avgPrice = data.sell_price || data.day_sell_price || 0;
    }
  }

  return {
    symbol: String(data.trading_symbol || data.tradingsymbol || ''),
    exchange: fromUpstoxExchange(exchange),
    productType: fromUpstoxProduct(product, exchange),
    quantity,
    buyQuantity: Number(data.day_buy_quantity || 0),
    sellQuantity: Number(data.day_sell_quantity || 0),
    buyValue: Number(data.day_buy_value || 0),
    sellValue: Number(data.day_sell_value || 0),
    averagePrice: Number(avgPrice || 0),
    lastPrice: Number(data.last_price || 0),
    pnl: Number(data.pnl || 0),
    pnlPercent: 0, // Calculate if needed
    dayPnl: Number(data.day_pnl || data.pnl || 0),
  };
}

/**
 * Convert Upstox holding to internal format
 */
export function fromUpstoxHolding(data: Record<string, unknown>): Holding {
  const quantity = Number(data.quantity || 0);
  const avgPrice = Number(data.average_price || 0);
  const lastPrice = Number(data.last_price || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol: String(data.trading_symbol || data.tradingsymbol || ''),
    exchange: fromUpstoxExchange(String(data.exchange || 'NSE')),
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    investedValue,
    currentValue,
    pnl,
    pnlPercent,
  };
}

/**
 * Convert Upstox funds to internal format
 */
export function fromUpstoxFunds(data: Record<string, unknown>): Funds {
  const equity = data.equity as Record<string, unknown> || {};
  const commodity = data.commodity as Record<string, unknown> || {};

  const availableMargin = Number(equity.available_margin || 0) + Number(commodity.available_margin || 0);
  const usedMargin = Number(equity.used_margin || 0) + Number(commodity.used_margin || 0);

  return {
    availableCash: availableMargin,
    usedMargin,
    availableMargin,
    totalBalance: availableMargin + usedMargin,
    currency: 'INR',
  };
}

/**
 * Convert Upstox quote to internal format
 */
export function fromUpstoxQuote(data: Record<string, unknown>, symbol: string, exchange: StockExchange): Quote {
  const ohlc = data.ohlc as Record<string, unknown> || {};

  return {
    symbol,
    exchange,
    lastPrice: Number(data.last_price || ohlc.close || 0),
    open: Number(ohlc.open || 0),
    high: Number(ohlc.high || 0),
    low: Number(ohlc.low || 0),
    close: Number(ohlc.close || 0),
    previousClose: Number(data.prev_close || ohlc.close || 0),
    change: Number(data.net_change || 0),
    changePercent: Number(data.percent_change || 0),
    volume: Number(data.volume || 0),
    bid: Number(data.bid_price || 0),
    bidQty: Number(data.bid_qty || 0),
    ask: Number(data.ask_price || 0),
    askQty: Number(data.ask_qty || 0),
    timestamp: Date.now(),
  };
}
