/**
 * 5Paisa Data Mappers
 *
 * Transform between 5Paisa API format and standard stock broker format
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
} from '../../types';

import {
  FIVEPAISA_EXCHANGE_MAP,
  FIVEPAISA_EXCHANGE_TYPE_MAP,
  FIVEPAISA_EXCHANGE_REVERSE_MAP,
  FIVEPAISA_PRODUCT_MAP,
  FIVEPAISA_ORDER_SIDE_MAP,
  FIVEPAISA_ORDER_SIDE_REVERSE_MAP,
  FIVEPAISA_ORDER_TYPE_REVERSE_MAP,
  FIVEPAISA_STATUS_MAP,
  FIVEPAISA_INTERVAL_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPERS
// ============================================================================

/**
 * Convert standard order params to 5Paisa format
 */
export function toFivePaisaOrderParams(
  params: OrderParams,
  clientId: string,
  scripCode: number
): Record<string, unknown> {
  return {
    ClientCode: clientId,
    OrderType: FIVEPAISA_ORDER_SIDE_MAP[params.side] || 'B',
    Exchange: FIVEPAISA_EXCHANGE_MAP[params.exchange] || 'N',
    ExchangeType: FIVEPAISA_EXCHANGE_TYPE_MAP[params.exchange] || 'C',
    ScripCode: scripCode,
    Price: params.price || 0,
    Qty: params.quantity,
    StopLossPrice: params.triggerPrice || 0,
    DisQty: params.disclosedQuantity || 0,
    IsIntraday: FIVEPAISA_PRODUCT_MAP[params.productType] ?? false,
    AHPlaced: params.amo ? 'Y' : 'N',
    RemoteOrderID: params.tag || 'FinceptTerminal',
  };
}

/**
 * Convert modify order params to 5Paisa format
 */
export function toFivePaisaModifyParams(params: ModifyOrderParams): Record<string, unknown> {
  const fivePaisaParams: Record<string, unknown> = {
    ExchOrderID: params.orderId,
  };

  if (params.quantity !== undefined) {
    fivePaisaParams.Qty = params.quantity;
  }
  if (params.price !== undefined) {
    fivePaisaParams.Price = params.price;
  }
  if (params.triggerPrice !== undefined) {
    fivePaisaParams.StopLossPrice = params.triggerPrice;
  }

  return fivePaisaParams;
}

/**
 * Convert 5Paisa order to standard format
 */
export function fromFivePaisaOrder(order: Record<string, unknown>): Order {
  const statusKey = String(order.OrderStatus || order.Status || 'Pending');
  const exchCode = String(order.Exch || 'N');
  const isIntraday = Boolean(order.DelvIntra === 'I' || order.IsIntraday);

  return {
    orderId: String(order.ExchOrderID || order.BrokerOrderId || ''),
    symbol: String(order.ScripName || order.TradingSymbol || ''),
    exchange: (FIVEPAISA_EXCHANGE_REVERSE_MAP[exchCode] || 'NSE') as StockExchange,
    side: (FIVEPAISA_ORDER_SIDE_REVERSE_MAP[String(order.BuySell || 'B')] || 'BUY') as OrderSide,
    quantity: Number(order.Qty || 0),
    filledQuantity: Number(order.TradedQty || 0),
    pendingQuantity: Number(order.Qty || 0) - Number(order.TradedQty || 0),
    price: Number(order.Rate || order.Price || 0),
    averagePrice: Number(order.AvgRate || order.AveragePrice || 0),
    triggerPrice: order.SLTriggerRate ? Number(order.SLTriggerRate) : undefined,
    orderType: (FIVEPAISA_ORDER_TYPE_REVERSE_MAP[String(order.AtMarket === 'N' ? 'L' : 'N')] || 'LIMIT') as OrderType,
    productType: (isIntraday ? 'INTRADAY' : 'CASH') as ProductType,
    validity: 'DAY',
    status: (FIVEPAISA_STATUS_MAP[statusKey] || 'PENDING') as OrderStatus,
    statusMessage: order.Reason ? String(order.Reason) : undefined,
    placedAt: new Date(order.OrderDateTime as string || Date.now()),
    updatedAt: order.ExchOrderTime ? new Date(order.ExchOrderTime as string) : undefined,
    exchangeOrderId: order.ExchOrderID ? String(order.ExchOrderID) : undefined,
    tag: order.RemoteOrderID ? String(order.RemoteOrderID) : undefined,
  };
}

// ============================================================================
// TRADE MAPPERS
// ============================================================================

/**
 * Convert 5Paisa trade to standard format
 */
export function fromFivePaisaTrade(trade: Record<string, unknown>): Trade {
  const exchCode = String(trade.Exch || 'N');
  const isIntraday = Boolean(trade.DelvIntra === 'I');

  return {
    tradeId: String(trade.ExchOrderID || ''),
    orderId: String(trade.ExchOrderID || ''),
    symbol: String(trade.ScripName || ''),
    exchange: (FIVEPAISA_EXCHANGE_REVERSE_MAP[exchCode] || 'NSE') as StockExchange,
    side: (FIVEPAISA_ORDER_SIDE_REVERSE_MAP[String(trade.BuySell || 'B')] || 'BUY') as OrderSide,
    quantity: Number(trade.Qty || 0),
    price: Number(trade.Rate || 0),
    productType: (isIntraday ? 'INTRADAY' : 'CASH') as ProductType,
    tradedAt: new Date(trade.ExchOrderTime as string || Date.now()),
    exchangeTradeId: trade.ExchOrderID ? String(trade.ExchOrderID) : undefined,
  };
}

// ============================================================================
// POSITION MAPPERS
// ============================================================================

/**
 * Convert 5Paisa position to standard format
 */
export function fromFivePaisaPosition(position: Record<string, unknown>): Position {
  const netQty = Number(position.NetQty || 0);
  const buyQty = Number(position.BuyQty || 0);
  const sellQty = Number(position.SellQty || 0);
  const buyValue = Number(position.BuyValue || position.BuyAmt || 0);
  const sellValue = Number(position.SellValue || position.SellAmt || 0);
  const lastPrice = Number(position.LTP || 0);
  const avgPrice = Number(position.NetRate || position.AvgRate || 0);

  // Calculate P&L
  const pnl = Number(position.BookedPL || 0) + Number(position.MTOM || 0);
  const pnlPercent = avgPrice > 0 ? ((lastPrice - avgPrice) / avgPrice) * 100 : 0;
  const dayPnl = Number(position.MTOM || 0);

  const exchCode = String(position.Exch || 'N');

  return {
    symbol: String(position.ScripName || ''),
    exchange: (FIVEPAISA_EXCHANGE_REVERSE_MAP[exchCode] || 'NSE') as StockExchange,
    productType: 'INTRADAY' as ProductType,
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
    overnight: Boolean(position.BodQty && Number(position.BodQty) > 0),
  };
}

// ============================================================================
// HOLDING MAPPERS
// ============================================================================

/**
 * Convert 5Paisa holding to standard format
 */
export function fromFivePaisaHolding(holding: Record<string, unknown>): Holding {
  const quantity = Number(holding.Quantity || holding.Qty || 0);
  const avgPrice = Number(holding.BuyAvgRate || holding.AvgRate || 0);
  const lastPrice = Number(holding.CurrentPrice || holding.LTP || 0);
  const investedValue = quantity * avgPrice;
  const currentValue = quantity * lastPrice;
  const pnl = currentValue - investedValue;
  const pnlPercent = investedValue > 0 ? (pnl / investedValue) * 100 : 0;

  const exchCode = String(holding.Exch || 'N');

  return {
    symbol: String(holding.ScripName || holding.TradingSymbol || ''),
    exchange: (FIVEPAISA_EXCHANGE_REVERSE_MAP[exchCode] || 'NSE') as StockExchange,
    quantity,
    averagePrice: avgPrice,
    lastPrice,
    investedValue,
    currentValue,
    pnl,
    pnlPercent,
    isin: holding.ISIN ? String(holding.ISIN) : undefined,
    pledgedQuantity: holding.PledgedQty ? Number(holding.PledgedQty) : undefined,
    collateralQuantity: holding.CollateralQty ? Number(holding.CollateralQty) : undefined,
    t1Quantity: holding.T1Qty ? Number(holding.T1Qty) : undefined,
  };
}

// ============================================================================
// FUNDS MAPPERS
// ============================================================================

/**
 * Convert 5Paisa margin to standard funds format
 */
export function fromFivePaisaFunds(funds: Record<string, unknown>): Funds {
  const availableCash = Number(funds.AvailableMargin || funds.NetAvailableMargin || 0);
  const usedMargin = Number(funds.UsedMargin || funds.MarginUsed || 0);
  const collateral = Number(funds.CollateralValue || 0);

  return {
    availableCash,
    usedMargin,
    availableMargin: availableCash - usedMargin,
    totalBalance: availableCash + usedMargin,
    currency: 'INR',
    equityAvailable: availableCash,
    commodityAvailable: Number(funds.CommodityMargin || 0),
    collateral,
    unrealizedPnl: Number(funds.UnrealizedMTOM || 0),
    realizedPnl: Number(funds.RealizedMTOM || 0),
  };
}

// ============================================================================
// QUOTE MAPPERS
// ============================================================================

/**
 * Convert 5Paisa market depth to standard quote format
 */
export function fromFivePaisaQuote(
  quote: Record<string, unknown>,
  symbol: string,
  exchange: StockExchange
): Quote {
  const lastPrice = Number(quote.LastRate || quote.LTP || 0);
  const previousClose = Number(quote.PClose || quote.Close || 0);
  const change = lastPrice - previousClose;
  const changePercent = previousClose > 0 ? (change / previousClose) * 100 : 0;

  return {
    symbol,
    exchange,
    lastPrice,
    open: Number(quote.Open || 0),
    high: Number(quote.High || 0),
    low: Number(quote.Low || 0),
    close: Number(quote.Close || 0),
    previousClose,
    change,
    changePercent,
    volume: Number(quote.TotalQty || quote.Volume || 0),
    value: quote.TotalValue ? Number(quote.TotalValue) : undefined,
    averagePrice: quote.AvgRate ? Number(quote.AvgRate) : undefined,
    bid: Number(quote.BuyRate || quote.BidPrice || 0),
    bidQty: Number(quote.BuyQty || quote.BidQty || 0),
    ask: Number(quote.SellRate || quote.AskPrice || 0),
    askQty: Number(quote.SellQty || quote.AskQty || 0),
    timestamp: Date.now(),
    openInterest: quote.OI ? Number(quote.OI) : undefined,
    upperCircuit: quote.UpperCktLmt ? Number(quote.UpperCktLmt) : undefined,
    lowerCircuit: quote.LowerCktLmt ? Number(quote.LowerCktLmt) : undefined,
  };
}

// ============================================================================
// OHLCV MAPPERS
// ============================================================================

/**
 * Convert 5Paisa OHLCV candle to standard format
 */
export function fromFivePaisaOHLCV(candle: Record<string, unknown>): OHLCV {
  return {
    timestamp: new Date(candle.Datetime as string || candle.Time as string).getTime(),
    open: Number(candle.Open || 0),
    high: Number(candle.High || 0),
    low: Number(candle.Low || 0),
    close: Number(candle.Close || 0),
    volume: Number(candle.Volume || candle.TotalQty || 0),
  };
}

/**
 * Convert timeframe to 5Paisa interval
 */
export function toFivePaisaInterval(timeframe: TimeFrame): string {
  return FIVEPAISA_INTERVAL_MAP[timeframe] || '1d';
}

// ============================================================================
// DEPTH MAPPERS
// ============================================================================

/**
 * Convert 5Paisa depth to standard format
 */
export function fromFivePaisaDepth(quote: Record<string, unknown>): MarketDepth {
  const bids: DepthLevel[] = [];
  const asks: DepthLevel[] = [];

  // 5Paisa uses BuyRate1-5 for bid prices, BuyQty1-5 for bid quantities
  for (let i = 1; i <= 5; i++) {
    const bidPrice = Number(quote[`BuyRate${i}`] || 0);
    const bidQty = Number(quote[`BuyQty${i}`] || 0);

    if (bidPrice > 0) {
      bids.push({
        price: bidPrice,
        quantity: bidQty,
        orders: 1,
      });
    }

    const askPrice = Number(quote[`SellRate${i}`] || 0);
    const askQty = Number(quote[`SellQty${i}`] || 0);

    if (askPrice > 0) {
      asks.push({
        price: askPrice,
        quantity: askQty,
        orders: 1,
      });
    }
  }

  return { bids, asks };
}

// ============================================================================
// MARGIN MAPPERS
// ============================================================================

/**
 * Convert order params to 5Paisa margin request format
 */
export function toFivePaisaMarginParams(
  params: OrderParams,
  clientId: string,
  scripCode: number
): Record<string, unknown> {
  return {
    ClientCode: clientId,
    Exchange: FIVEPAISA_EXCHANGE_MAP[params.exchange] || 'N',
    ExchangeType: FIVEPAISA_EXCHANGE_TYPE_MAP[params.exchange] || 'C',
    ScripCode: scripCode,
    TransactionType: FIVEPAISA_ORDER_SIDE_MAP[params.side] || 'B',
    Qty: params.quantity,
    Price: params.price || 0,
  };
}
