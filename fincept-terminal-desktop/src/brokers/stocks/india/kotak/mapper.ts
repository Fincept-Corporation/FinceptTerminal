/**
 * Kotak Neo Data Mappers
 *
 * Transform data between Kotak Neo API format and internal format
 * Based on OpenAlgo Kotak implementation
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
  KOTAK_EXCHANGE_MAP,
  KOTAK_EXCHANGE_REVERSE_MAP,
  KOTAK_PRODUCT_MAP,
  KOTAK_PRODUCT_REVERSE_MAP,
  KOTAK_ORDER_TYPE_MAP,
  KOTAK_ORDER_TYPE_REVERSE_MAP,
} from './constants';

/**
 * Convert internal exchange to Kotak exchange format
 */
export function toKotakExchange(exchange: StockExchange): string {
  return KOTAK_EXCHANGE_MAP[exchange] || exchange.toLowerCase();
}

/**
 * Convert Kotak exchange to internal exchange format
 */
export function fromKotakExchange(exchange: string): StockExchange {
  return KOTAK_EXCHANGE_REVERSE_MAP[exchange.toLowerCase()] || (exchange.toUpperCase() as StockExchange);
}

/**
 * Convert internal product type to Kotak product
 */
export function toKotakProduct(product: string): string {
  return KOTAK_PRODUCT_MAP[product.toUpperCase()] || 'MIS';
}

/**
 * Convert Kotak product to internal product type
 */
export function fromKotakProduct(product: string): ProductType {
  const mapped = KOTAK_PRODUCT_REVERSE_MAP[product.toUpperCase()];
  if (mapped === 'CASH') return 'CASH';
  if (mapped === 'MARGIN') return 'MARGIN';
  return 'INTRADAY';
}

/**
 * Convert internal order type to Kotak order type
 */
export function toKotakOrderType(orderType: string): string {
  return KOTAK_ORDER_TYPE_MAP[orderType.toUpperCase()] || 'MKT';
}

/**
 * Convert Kotak order type to internal order type
 */
export function fromKotakOrderType(orderType: string): OrderType {
  const mapped = KOTAK_ORDER_TYPE_REVERSE_MAP[orderType.toUpperCase()];
  if (mapped === 'LIMIT') return 'LIMIT';
  if (mapped === 'STOP') return 'STOP';
  if (mapped === 'STOP_LIMIT') return 'STOP_LIMIT';
  return 'MARKET';
}

/**
 * Convert order params to Kotak format
 */
export function toKotakOrderParams(
  token: string,
  params: OrderParams
): Record<string, unknown> {
  const order: Record<string, unknown> = {
    am: 'NO', // After market
    dq: '0', // Disclosed quantity
    es: toKotakExchange(params.exchange),
    mp: '0', // Market protection
    pc: toKotakProduct(params.productType),
    pf: 'N', // Price fill
    pr: params.price?.toString() || '0',
    pt: toKotakOrderType(params.orderType),
    qt: params.quantity.toString(),
    rt: 'DAY', // Retention type
    tp: params.triggerPrice?.toString() || '0',
    ts: params.symbol,
    tt: params.side,
    tk: token, // Token (pSymbol)
  };

  return order;
}

/**
 * Convert modify order params to Kotak format
 */
export function toKotakModifyParams(
  orderId: string,
  params: ModifyOrderParams
): Record<string, unknown> {
  const result: Record<string, unknown> = {
    no: orderId, // Neo order ID
  };

  if (params.quantity !== undefined) {
    result.qt = params.quantity.toString();
  }
  if (params.price !== undefined) {
    result.pr = params.price.toString();
  }
  if (params.triggerPrice !== undefined) {
    result.tp = params.triggerPrice.toString();
  }
  if (params.orderType !== undefined) {
    result.pt = toKotakOrderType(params.orderType);
  }

  return result;
}

/**
 * Convert Kotak order to internal format
 */
export function fromKotakOrder(data: Record<string, unknown>): Order {
  const exchange = String(data.es || data.exchange || 'nse_cm');
  const product = String(data.pc || data.product || 'MIS');
  const quantity = Number(data.qt || data.quantity || 0);
  const filledQty = Number(data.fq || data.filledQuantity || 0);

  return {
    orderId: String(data.no || data.orderId || ''),
    symbol: String(data.ts || data.tradingSymbol || ''),
    exchange: fromKotakExchange(exchange),
    side: String(data.tt || data.transactionType || 'BUY') as 'BUY' | 'SELL',
    quantity,
    filledQuantity: filledQty,
    pendingQuantity: quantity - filledQty,
    price: Number(data.pr || data.price || 0),
    averagePrice: Number(data.ap || data.averagePrice || 0),
    orderType: fromKotakOrderType(String(data.pt || data.orderType || 'MKT')),
    productType: fromKotakProduct(product),
    validity: 'DAY',
    status: mapOrderStatus(String(data.st || data.status || '')),
    placedAt: data.od ? new Date(String(data.od)) : new Date(),
    statusMessage: String(data.rm || data.rejectionReason || ''),
  };
}

/**
 * Map Kotak order status to internal status
 */
function mapOrderStatus(status: string): Order['status'] {
  const statusMap: Record<string, Order['status']> = {
    pending: 'PENDING',
    open: 'OPEN',
    complete: 'FILLED',
    traded: 'FILLED',
    rejected: 'REJECTED',
    cancelled: 'CANCELLED',
    expired: 'EXPIRED',
    'part traded': 'PARTIALLY_FILLED',
    'trigger pending': 'TRIGGER_PENDING',
    // Kotak-specific statuses
    placed: 'PENDING',
    modified: 'OPEN',
    queued: 'PENDING',
  };
  return statusMap[status.toLowerCase()] || 'PENDING';
}

/**
 * Convert Kotak trade to internal format
 */
export function fromKotakTrade(data: Record<string, unknown>): Trade {
  const exchange = String(data.es || data.exchange || 'nse_cm');
  const product = String(data.pc || data.product || 'MIS');

  return {
    tradeId: String(data.tid || data.tradeId || data.no || ''),
    orderId: String(data.no || data.orderId || ''),
    symbol: String(data.ts || data.tradingSymbol || ''),
    exchange: fromKotakExchange(exchange),
    side: String(data.tt || data.transactionType || 'BUY') as 'BUY' | 'SELL',
    quantity: Number(data.fq || data.filledQuantity || 0),
    price: Number(data.fp || data.filledPrice || data.pr || 0),
    productType: fromKotakProduct(product),
    tradedAt: data.ft ? new Date(String(data.ft)) : new Date(),
  };
}

/**
 * Convert Kotak position to internal format
 */
export function fromKotakPosition(data: Record<string, unknown>): Position {
  const exchange = String(data.es || data.exchange || 'nse_cm');
  const product = String(data.pc || data.product || 'MIS');
  const netQty = Number(data.nq || data.netQty || 0);
  const avgPrice = Number(data.ap || data.averagePrice || 0);
  const lastPrice = Number(data.lp || data.lastPrice || 0);

  const buyQty = Number(data.bq || data.buyQty || 0);
  const sellQty = Number(data.sq || data.sellQty || 0);
  const buyValue = Number(data.bv || data.buyValue || 0);
  const sellValue = Number(data.sv || data.sellValue || 0);

  const mtm = Number(data.mtm || data.mtmLoss || 0);

  return {
    symbol: String(data.ts || data.tradingSymbol || ''),
    exchange: fromKotakExchange(exchange),
    productType: fromKotakProduct(product),
    quantity: netQty,
    buyQuantity: buyQty,
    sellQuantity: sellQty,
    buyValue,
    sellValue,
    averagePrice: avgPrice,
    lastPrice,
    pnl: mtm,
    pnlPercent: avgPrice > 0 ? (mtm / (avgPrice * Math.abs(netQty))) * 100 : 0,
    dayPnl: mtm,
  };
}

/**
 * Convert Kotak holding to internal format
 */
export function fromKotakHolding(data: Record<string, unknown>): Holding {
  const quantity = Number(data.hq || data.holdingQty || data.qt || 0);
  const avgPrice = Number(data.ap || data.averagePrice || 0);
  const lastPrice = Number(data.lp || data.lastPrice || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  return {
    symbol: String(data.ts || data.tradingSymbol || ''),
    exchange: fromKotakExchange(String(data.es || data.exchange || 'nse_cm')),
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
 * Convert Kotak funds to internal format
 */
export function fromKotakFunds(data: Record<string, unknown>): Funds {
  // Kotak returns nested structure
  const fundsData = (data.Cash as Record<string, unknown>) || data;

  const availableCash = Number(
    fundsData.net || fundsData.availableBalance || fundsData.cash || 0
  );
  const usedMargin = Number(
    fundsData.utilized || fundsData.usedMargin || fundsData.marginUsed || 0
  );
  const collateral = Number(
    fundsData.collateral || fundsData.payin || 0
  );

  return {
    availableCash,
    usedMargin,
    availableMargin: availableCash,
    totalBalance: availableCash + usedMargin,
    currency: 'INR',
    collateral,
  };
}

/**
 * Convert Kotak quote to internal format
 */
export function fromKotakQuote(data: Record<string, unknown>, symbol: string, exchange: StockExchange): Quote {
  // Kotak quote structure
  const lastPrice = Number(data.ltp || data.lastPrice || 0);
  const previousClose = Number(data.cp || data.closePrice || 0);
  const change = Number(data.ch || data.change || (previousClose > 0 ? lastPrice - previousClose : 0));
  const changePercent = Number(data.chp || data.changePercent || (previousClose > 0 ? (change / previousClose) * 100 : 0));

  // Depth data (if available)
  const buyDepth = (data.bd as Array<Record<string, unknown>>) || [];
  const sellDepth = (data.sd as Array<Record<string, unknown>>) || [];

  return {
    symbol,
    exchange,
    lastPrice,
    open: Number(data.op || data.openPrice || 0),
    high: Number(data.hp || data.highPrice || 0),
    low: Number(data.lop || data.lowPrice || 0),
    close: previousClose,
    previousClose,
    change,
    changePercent,
    volume: Number(data.v || data.volume || 0),
    bid: buyDepth.length > 0 ? Number(buyDepth[0].bp || buyDepth[0].price || 0) : 0,
    bidQty: buyDepth.length > 0 ? Number(buyDepth[0].bq || buyDepth[0].quantity || 0) : 0,
    ask: sellDepth.length > 0 ? Number(sellDepth[0].sp || sellDepth[0].price || 0) : 0,
    askQty: sellDepth.length > 0 ? Number(sellDepth[0].sq || sellDepth[0].quantity || 0) : 0,
    timestamp: Date.now(),
  };
}
