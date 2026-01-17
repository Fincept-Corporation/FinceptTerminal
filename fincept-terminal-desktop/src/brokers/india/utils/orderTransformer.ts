/**
 * Order transformation utilities for Indian brokers
 * Handles conversion between standardized order format and broker-specific formats
 */

import type {
  OrderParams,
  Order,
  OrderStatus,
  OrderType,
  ProductType,
  TransactionType,
  OrderValidity,
  Position,
  Holding,
  FundsData,
} from '../types';

// ============================================================================
// Order Status Mapping
// ============================================================================

/**
 * Standardize order status from various broker formats
 */
export function standardizeOrderStatus(
  status: string,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): OrderStatus {
  const statusLower = status.toLowerCase().trim();

  // Common status mappings
  const statusMap: Record<string, OrderStatus> = {
    // Completed
    complete: 'COMPLETE',
    completed: 'COMPLETE',
    filled: 'COMPLETE',
    traded: 'COMPLETE',

    // Pending/Open
    pending: 'PENDING',
    open: 'OPEN',
    'open pending': 'OPEN',
    'after market order req received': 'AMO_SUBMITTED',
    'amo req received': 'AMO_SUBMITTED',
    amo: 'AMO_SUBMITTED',

    // Cancelled
    cancelled: 'CANCELLED',
    canceled: 'CANCELLED',

    // Rejected
    rejected: 'REJECTED',
    failed: 'REJECTED',

    // Trigger pending (for SL orders)
    'trigger pending': 'TRIGGER_PENDING',
    'sl pending': 'TRIGGER_PENDING',

    // Modified
    modified: 'MODIFIED',
    'modify pending': 'MODIFIED',
  };

  return statusMap[statusLower] || 'PENDING';
}

/**
 * Convert standardized order status to broker format
 */
export function toBrokerOrderStatus(
  status: OrderStatus,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): string {
  // Zerodha format
  if (brokerFormat === 'zerodha') {
    const zerodhaMap: Record<OrderStatus, string> = {
      PENDING: 'PENDING',
      OPEN: 'OPEN',
      COMPLETE: 'COMPLETE',
      CANCELLED: 'CANCELLED',
      REJECTED: 'REJECTED',
      MODIFIED: 'MODIFIED',
      TRIGGER_PENDING: 'TRIGGER PENDING',
      AMO_SUBMITTED: 'AMO REQ RECEIVED',
    };
    return zerodhaMap[status] || status;
  }

  return status;
}

// ============================================================================
// Order Type Mapping
// ============================================================================

export function standardizeOrderType(
  orderType: string,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): OrderType {
  const typeUpper = orderType.toUpperCase().trim();

  const typeMap: Record<string, OrderType> = {
    MARKET: 'MARKET',
    MKT: 'MARKET',
    LIMIT: 'LIMIT',
    LMT: 'LIMIT',
    SL: 'SL',
    'SL-L': 'SL',
    'STOPLOSS_LIMIT': 'SL',
    'SL-M': 'SL-M',
    'STOPLOSS_MARKET': 'SL-M',
    SLM: 'SL-M',
  };

  return typeMap[typeUpper] || 'MARKET';
}

export function toBrokerOrderType(
  orderType: OrderType,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): string {
  if (brokerFormat === 'zerodha') {
    return orderType; // Zerodha uses same format
  }

  if (brokerFormat === 'angel') {
    const angelMap: Record<OrderType, string> = {
      MARKET: 'MARKET',
      LIMIT: 'LIMIT',
      SL: 'STOPLOSS_LIMIT',
      'SL-M': 'STOPLOSS_MARKET',
    };
    return angelMap[orderType] || orderType;
  }

  return orderType;
}

// ============================================================================
// Product Type Mapping
// ============================================================================

export function standardizeProductType(
  productType: string,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): ProductType {
  const typeUpper = productType.toUpperCase().trim();

  const typeMap: Record<string, ProductType> = {
    // Delivery
    CNC: 'CNC',
    DELIVERY: 'CNC',
    C: 'CNC',

    // Intraday
    MIS: 'MIS',
    INTRADAY: 'MIS',
    I: 'MIS',

    // Normal (F&O)
    NRML: 'NRML',
    NORMAL: 'NRML',
    M: 'NRML',
    MARGIN: 'NRML',

    // Cover Order
    CO: 'CO',

    // Bracket Order
    BO: 'BO',
  };

  return typeMap[typeUpper] || 'CNC';
}

export function toBrokerProductType(
  productType: ProductType,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): string {
  if (brokerFormat === 'zerodha') {
    return productType;
  }

  if (brokerFormat === 'angel') {
    const angelMap: Record<ProductType, string> = {
      CNC: 'DELIVERY',
      MIS: 'INTRADAY',
      NRML: 'CARRYFORWARD',
      CO: 'CO',
      BO: 'BO',
    };
    return angelMap[productType] || productType;
  }

  if (brokerFormat === 'dhan') {
    const dhanMap: Record<ProductType, string> = {
      CNC: 'CNC',
      MIS: 'INTRADAY',
      NRML: 'MARGIN',
      CO: 'CO',
      BO: 'BO',
    };
    return dhanMap[productType] || productType;
  }

  return productType;
}

// ============================================================================
// Transaction Type Mapping
// ============================================================================

export function standardizeTransactionType(
  txnType: string,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): TransactionType {
  const typeUpper = txnType.toUpperCase().trim();

  if (['BUY', 'B', 'LONG'].includes(typeUpper)) {
    return 'BUY';
  }

  if (['SELL', 'S', 'SHORT'].includes(typeUpper)) {
    return 'SELL';
  }

  return 'BUY';
}

export function toBrokerTransactionType(
  txnType: TransactionType,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): string {
  // Most brokers use BUY/SELL
  return txnType;
}

// ============================================================================
// Validity Mapping
// ============================================================================

export function standardizeValidity(
  validity: string,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): OrderValidity {
  const validityUpper = validity.toUpperCase().trim();

  const validityMap: Record<string, OrderValidity> = {
    DAY: 'DAY',
    IOC: 'IOC',
    GTC: 'GTC',
    GTD: 'GTD',
  };

  return validityMap[validityUpper] || 'DAY';
}

// ============================================================================
// Transform Functions
// ============================================================================

/**
 * Transform broker order response to standardized Order
 */
export function transformOrder(
  brokerOrder: Record<string, unknown>,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): Order {
  if (brokerFormat === 'zerodha') {
    return transformZerodhaOrder(brokerOrder);
  }

  // Default transformation
  return {
    orderId: String(brokerOrder.order_id || brokerOrder.orderId || ''),
    exchangeOrderId: String(brokerOrder.exchange_order_id || ''),
    symbol: String(brokerOrder.tradingsymbol || brokerOrder.symbol || ''),
    exchange: (brokerOrder.exchange as string) as any,
    transactionType: standardizeTransactionType(
      String(brokerOrder.transaction_type || brokerOrder.transactionType || 'BUY'),
      brokerFormat
    ),
    orderType: standardizeOrderType(
      String(brokerOrder.order_type || brokerOrder.orderType || 'MARKET'),
      brokerFormat
    ),
    productType: standardizeProductType(
      String(brokerOrder.product || brokerOrder.productType || 'CNC'),
      brokerFormat
    ),
    quantity: Number(brokerOrder.quantity || 0),
    filledQuantity: Number(brokerOrder.filled_quantity || brokerOrder.filledQuantity || 0),
    pendingQuantity: Number(brokerOrder.pending_quantity || brokerOrder.pendingQuantity || 0),
    price: Number(brokerOrder.price || 0),
    averagePrice: Number(brokerOrder.average_price || brokerOrder.averagePrice || 0),
    triggerPrice: Number(brokerOrder.trigger_price || brokerOrder.triggerPrice || 0),
    status: standardizeOrderStatus(
      String(brokerOrder.status || 'PENDING'),
      brokerFormat
    ),
    statusMessage: String(brokerOrder.status_message || brokerOrder.statusMessage || ''),
    orderTimestamp: Number(brokerOrder.order_timestamp || Date.now()),
    exchangeTimestamp: Number(brokerOrder.exchange_timestamp || 0),
    tag: String(brokerOrder.tag || ''),
  };
}

function transformZerodhaOrder(order: Record<string, unknown>): Order {
  return {
    orderId: String(order.order_id || ''),
    exchangeOrderId: String(order.exchange_order_id || ''),
    symbol: String(order.tradingsymbol || ''),
    exchange: (order.exchange as string) as any,
    transactionType: standardizeTransactionType(String(order.transaction_type || 'BUY'), 'zerodha'),
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
      : 0,
    tag: String(order.tag || ''),
  };
}

/**
 * Transform broker position to standardized Position
 */
export function transformPosition(
  brokerPosition: Record<string, unknown>,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): Position {
  if (brokerFormat === 'zerodha') {
    return transformZerodhaPosition(brokerPosition);
  }

  return {
    symbol: String(brokerPosition.tradingsymbol || brokerPosition.symbol || ''),
    exchange: (brokerPosition.exchange as string) as any,
    productType: standardizeProductType(
      String(brokerPosition.product || brokerPosition.productType || 'MIS'),
      brokerFormat
    ),
    quantity: Number(brokerPosition.quantity || brokerPosition.netQuantity || 0),
    averagePrice: Number(brokerPosition.average_price || brokerPosition.averagePrice || 0),
    lastPrice: Number(brokerPosition.last_price || brokerPosition.ltp || 0),
    pnl: Number(brokerPosition.pnl || brokerPosition.unrealised || 0),
    pnlPercentage: 0, // Calculate if needed
    dayPnl: Number(brokerPosition.day_pnl || brokerPosition.dayPnl || 0),
    buyQuantity: Number(brokerPosition.buy_quantity || brokerPosition.buyQuantity || 0),
    sellQuantity: Number(brokerPosition.sell_quantity || brokerPosition.sellQuantity || 0),
    buyValue: Number(brokerPosition.buy_value || brokerPosition.buyValue || 0),
    sellValue: Number(brokerPosition.sell_value || brokerPosition.sellValue || 0),
    multiplier: Number(brokerPosition.multiplier || 1),
  };
}

function transformZerodhaPosition(position: Record<string, unknown>): Position {
  const avgPrice = Number(position.average_price || 0);
  const lastPrice = Number(position.last_price || 0);
  const quantity = Number(position.quantity || 0);
  const pnl = Number(position.pnl || position.unrealised || 0);
  const pnlPercentage = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;

  return {
    symbol: String(position.tradingsymbol || ''),
    exchange: (position.exchange as string) as any,
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
 * Transform broker holding to standardized Holding
 */
export function transformHolding(
  brokerHolding: Record<string, unknown>,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): Holding {
  const avgPrice = Number(
    brokerHolding.average_price || brokerHolding.averagePrice || brokerHolding.avgPrice || 0
  );
  const lastPrice = Number(brokerHolding.last_price || brokerHolding.ltp || 0);
  const quantity = Number(brokerHolding.quantity || brokerHolding.holdingQuantity || 0);
  const pnl = (lastPrice - avgPrice) * quantity;
  const pnlPercentage = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;
  const close = Number(brokerHolding.close_price || brokerHolding.previousClose || lastPrice);
  const dayChange = lastPrice - close;
  const dayChangePercentage = close > 0 ? (dayChange / close) * 100 : 0;

  return {
    symbol: String(
      brokerHolding.tradingsymbol || brokerHolding.symbol || brokerHolding.tradingSymbol || ''
    ),
    exchange: (brokerHolding.exchange as string || 'NSE') as any,
    isin: String(brokerHolding.isin || ''),
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    pnl,
    pnlPercentage,
    dayChange,
    dayChangePercentage,
    pledgedQuantity: Number(brokerHolding.pledged_quantity || brokerHolding.pledgedQuantity || 0),
    collateralQuantity: Number(
      brokerHolding.collateral_quantity || brokerHolding.collateralQuantity || 0
    ),
  };
}

/**
 * Transform broker funds data to standardized FundsData
 */
export function transformFunds(
  brokerFunds: Record<string, unknown>,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): FundsData {
  if (brokerFormat === 'zerodha') {
    return transformZerodhaFunds(brokerFunds);
  }

  return {
    availableCash: Number(brokerFunds.available_cash || brokerFunds.availableCash || 0),
    usedMargin: Number(brokerFunds.used_margin || brokerFunds.usedMargin || 0),
    availableMargin: Number(brokerFunds.available_margin || brokerFunds.availableMargin || 0),
    collateral: Number(brokerFunds.collateral || 0),
    unrealizedMtm: Number(brokerFunds.unrealized_mtm || brokerFunds.unrealizedMtm || 0),
    realizedMtm: Number(brokerFunds.realized_mtm || brokerFunds.realizedMtm || 0),
  };
}

function transformZerodhaFunds(funds: Record<string, unknown>): FundsData {
  // Zerodha returns separate equity and commodity margins
  const equity = (funds.equity as Record<string, unknown>) || {};
  const commodity = (funds.commodity as Record<string, unknown>) || {};

  const equityNet = Number(equity.net || 0);
  const commodityNet = Number(commodity.net || 0);

  const equityUtilised = (equity.utilised as Record<string, unknown>) || {};
  const commodityUtilised = (commodity.utilised as Record<string, unknown>) || {};

  const equityAvailable = (equity.available as Record<string, unknown>) || {};
  const commodityAvailable = (commodity.available as Record<string, unknown>) || {};

  const availableCash = equityNet + commodityNet;
  const usedMargin =
    Number(equityUtilised.debits || 0) + Number(commodityUtilised.debits || 0);
  const collateral =
    Number(equityAvailable.collateral || 0) + Number(commodityAvailable.collateral || 0);
  const unrealizedMtm =
    Number(equityUtilised.m2m_unrealised || 0) + Number(commodityUtilised.m2m_unrealised || 0);
  const realizedMtm =
    Number(equityUtilised.m2m_realised || 0) + Number(commodityUtilised.m2m_realised || 0);

  return {
    availableCash,
    usedMargin,
    availableMargin: availableCash - usedMargin,
    collateral,
    unrealizedMtm,
    realizedMtm,
  };
}

/**
 * Build order params for broker API
 */
export function buildBrokerOrderParams(
  params: OrderParams,
  brokerFormat: 'zerodha' | 'angel' | 'dhan' | 'fyers' | 'upstox'
): Record<string, unknown> {
  if (brokerFormat === 'zerodha') {
    return {
      tradingsymbol: params.symbol,
      exchange: params.exchange,
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

  if (brokerFormat === 'angel') {
    return {
      tradingsymbol: params.symbol,
      exchange: params.exchange,
      transactiontype: params.transactionType,
      ordertype: toBrokerOrderType(params.orderType, 'angel'),
      producttype: toBrokerProductType(params.productType, 'angel'),
      quantity: params.quantity,
      price: params.price || 0,
      triggerprice: params.triggerPrice || 0,
      disclosedquantity: params.disclosedQuantity || 0,
      duration: params.validity || 'DAY',
      ordertag: params.tag || '',
    };
  }

  // Default format
  return {
    symbol: params.symbol,
    exchange: params.exchange,
    transactionType: params.transactionType,
    orderType: params.orderType,
    productType: params.productType,
    quantity: params.quantity,
    price: params.price || 0,
    triggerPrice: params.triggerPrice || 0,
    validity: params.validity || 'DAY',
    tag: params.tag || '',
  };
}
