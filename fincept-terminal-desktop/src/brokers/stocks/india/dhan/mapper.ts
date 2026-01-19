/**
 * Dhan Data Mappers
 *
 * Transform data between Dhan API format and internal format
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
  OrderType,
} from '../../types';
import {
  DHAN_EXCHANGE_MAP,
  DHAN_EXCHANGE_REVERSE_MAP,
  DHAN_PRODUCT_MAP,
  DHAN_ORDER_TYPE_MAP,
} from './constants';

/**
 * Convert internal exchange to Dhan segment
 */
export function toDhanExchange(exchange: StockExchange): string {
  return DHAN_EXCHANGE_MAP[exchange] || exchange;
}

/**
 * Convert Dhan segment to internal exchange
 */
export function fromDhanExchange(segment: string): StockExchange {
  return DHAN_EXCHANGE_REVERSE_MAP[segment] || (segment as StockExchange);
}

/**
 * Convert internal product type to Dhan product
 */
export function toDhanProduct(product: string): string {
  return DHAN_PRODUCT_MAP[product] || 'INTRADAY';
}

/**
 * Convert Dhan product to internal product type
 * Returns unified ProductType ('CASH', 'INTRADAY', 'MARGIN')
 */
export function fromDhanProduct(product: string): ProductType {
  switch (product) {
    case 'CNC':
      return 'CASH';
    case 'INTRADAY':
      return 'INTRADAY';
    case 'MARGIN':
      return 'MARGIN';
    default:
      return 'INTRADAY';
  }
}

/**
 * Convert internal order type to Dhan order type
 */
export function toDhanOrderType(orderType: string): string {
  return DHAN_ORDER_TYPE_MAP[orderType] || 'MARKET';
}

/**
 * Convert order params to Dhan format
 */
export function toDhanOrderParams(
  clientId: string,
  securityId: string,
  params: OrderParams
): Record<string, unknown> {
  const order: Record<string, unknown> = {
    dhanClientId: clientId,
    transactionType: params.side,
    exchangeSegment: toDhanExchange(params.exchange),
    productType: toDhanProduct(params.productType),
    orderType: toDhanOrderType(params.orderType),
    validity: 'DAY',
    securityId,
    quantity: params.quantity,
  };

  if (params.price && params.orderType !== 'MARKET') {
    order.price = params.price;
  }

  if (params.triggerPrice) {
    order.triggerPrice = params.triggerPrice;
  }

  return order;
}

/**
 * Convert modify order params to Dhan format
 */
export function toDhanModifyParams(
  clientId: string,
  orderId: string,
  params: ModifyOrderParams
): Record<string, unknown> {
  const result: Record<string, unknown> = {
    dhanClientId: clientId,
    orderId,
    legName: 'ENTRY_LEG',
    validity: 'DAY',
  };

  if (params.quantity !== undefined) {
    result.quantity = params.quantity;
  }
  if (params.price !== undefined) {
    result.price = params.price;
  }
  if (params.triggerPrice !== undefined) {
    result.triggerPrice = params.triggerPrice;
  }
  if (params.orderType !== undefined) {
    result.orderType = toDhanOrderType(params.orderType);
  }

  return result;
}

/**
 * Map Dhan order type to internal order type
 */
function fromDhanOrderType(orderType: string): OrderType {
  const typeMap: Record<string, OrderType> = {
    MARKET: 'MARKET',
    LIMIT: 'LIMIT',
    STOP_LOSS: 'STOP',
    STOP_LOSS_MARKET: 'STOP',
    SL: 'STOP',
    'SL-M': 'STOP',
  };
  return typeMap[orderType.toUpperCase()] || 'MARKET';
}

/**
 * Convert Dhan order to internal format
 */
export function fromDhanOrder(data: Record<string, unknown>): Order {
  const exchange = String(data.exchangeSegment || 'NSE_EQ');
  const product = String(data.productType || 'INTRADAY');

  return {
    orderId: String(data.orderId || ''),
    symbol: String(data.tradingSymbol || ''),
    exchange: fromDhanExchange(exchange),
    side: String(data.transactionType || 'BUY') as 'BUY' | 'SELL',
    quantity: Number(data.quantity || 0),
    filledQuantity: Number(data.filledQty || 0),
    pendingQuantity: Number(data.remainingQuantity || data.quantity || 0) - Number(data.filledQty || 0),
    price: Number(data.price || 0),
    averagePrice: Number(data.tradedPrice || data.averagePrice || 0),
    orderType: fromDhanOrderType(String(data.orderType || 'MARKET')),
    productType: fromDhanProduct(product),
    validity: 'DAY',
    status: mapOrderStatus(String(data.orderStatus || '')),
    placedAt: data.createTime ? new Date(String(data.createTime)) : new Date(),
    statusMessage: String(data.omsErrorDescription || ''),
  };
}

/**
 * Map Dhan order status to internal status
 */
function mapOrderStatus(status: string): Order['status'] {
  const statusMap: Record<string, Order['status']> = {
    PENDING: 'OPEN',
    TRANSIT: 'PENDING',
    TRADED: 'FILLED',
    REJECTED: 'REJECTED',
    CANCELLED: 'CANCELLED',
    EXPIRED: 'EXPIRED',
    PART_TRADED: 'PARTIALLY_FILLED',
    TRIGGER_PENDING: 'TRIGGER_PENDING',
  };
  return statusMap[status.toUpperCase()] || 'PENDING';
}

/**
 * Convert Dhan trade to internal format
 */
export function fromDhanTrade(data: Record<string, unknown>): Trade {
  const exchange = String(data.exchangeSegment || 'NSE_EQ');
  const product = String(data.productType || 'INTRADAY');

  return {
    tradeId: String(data.tradeId || data.orderId || ''),
    orderId: String(data.orderId || ''),
    symbol: String(data.tradingSymbol || ''),
    exchange: fromDhanExchange(exchange),
    side: String(data.transactionType || 'BUY') as 'BUY' | 'SELL',
    quantity: Number(data.tradedQuantity || data.quantity || 0),
    price: Number(data.tradedPrice || data.price || 0),
    productType: fromDhanProduct(product),
    tradedAt: data.exchangeTime ? new Date(String(data.exchangeTime)) : new Date(),
  };
}

/**
 * Convert Dhan position to internal format
 */
export function fromDhanPosition(data: Record<string, unknown>): Position {
  const exchange = String(data.exchangeSegment || 'NSE_EQ');
  const product = String(data.productType || 'INTRADAY');
  const netQty = Number(data.netQty || 0);

  return {
    symbol: String(data.tradingSymbol || ''),
    exchange: fromDhanExchange(exchange),
    productType: fromDhanProduct(product),
    quantity: netQty,
    buyQuantity: Number(data.dayBuyQty || 0),
    sellQuantity: Number(data.daySellQty || 0),
    buyValue: Number(data.dayBuyValue || 0),
    sellValue: Number(data.daySellValue || 0),
    averagePrice: Number(data.costPrice || 0),
    lastPrice: Number(data.lastPrice || 0),
    pnl: Number(data.realizedProfit || 0) + Number(data.unrealizedProfit || 0),
    pnlPercent: 0,
    dayPnl: Number(data.unrealizedProfit || 0),
  };
}

/**
 * Convert Dhan holding to internal format
 */
export function fromDhanHolding(data: Record<string, unknown>): Holding {
  const quantity = Number(data.totalQty || data.quantity || 0);
  const avgPrice = Number(data.avgCostPrice || 0);
  const lastPrice = Number(data.lastPrice || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol: String(data.tradingSymbol || ''),
    exchange: fromDhanExchange(String(data.exchangeSegment || 'NSE_EQ')),
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
 * Convert Dhan funds to internal format
 * Note: Dhan API has a typo "availabelBalance" (not "availableBalance")
 */
export function fromDhanFunds(data: Record<string, unknown>): Funds {
  const availableBalance = Number(data.availabelBalance || data.availableBalance || 0);
  const utilizedAmount = Number(data.utilizedAmount || 0);
  const collateral = Number(data.collateralAmount || 0);

  return {
    availableCash: availableBalance,
    usedMargin: utilizedAmount,
    availableMargin: availableBalance,
    totalBalance: availableBalance + utilizedAmount,
    currency: 'INR',
    collateral,
  };
}

/**
 * Convert Dhan quote to internal format
 */
export function fromDhanQuote(data: Record<string, unknown>, symbol: string, exchange: StockExchange): Quote {
  const ohlc = (data.ohlc as Record<string, unknown>) || {};
  const depth = (data.depth as Record<string, unknown>) || {};
  const buyOrders = (depth.buy as Array<Record<string, unknown>>) || [];
  const sellOrders = (depth.sell as Array<Record<string, unknown>>) || [];

  const lastPrice = Number(data.last_price || data.lastPrice || 0);
  const previousClose = Number(ohlc.close || 0);
  const change = previousClose > 0 ? lastPrice - previousClose : 0;
  const changePercent = previousClose > 0 ? (change / previousClose) * 100 : 0;

  return {
    symbol,
    exchange,
    lastPrice,
    open: Number(ohlc.open || 0),
    high: Number(ohlc.high || 0),
    low: Number(ohlc.low || 0),
    close: Number(ohlc.close || 0),
    previousClose,
    change,
    changePercent,
    volume: Number(data.volume || 0),
    bid: buyOrders.length > 0 ? Number(buyOrders[0].price || 0) : 0,
    bidQty: buyOrders.length > 0 ? Number(buyOrders[0].quantity || 0) : 0,
    ask: sellOrders.length > 0 ? Number(sellOrders[0].price || 0) : 0,
    askQty: sellOrders.length > 0 ? Number(sellOrders[0].quantity || 0) : 0,
    timestamp: Date.now(),
  };
}
