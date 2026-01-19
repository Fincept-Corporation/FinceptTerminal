/**
 * Saxo Bank Mapper
 *
 * Transforms Saxo Bank API responses to unified broker types
 */

import type {
  Order,
  Position,
  Holding,
  Trade,
  Quote,
  OHLCV,
  MarketDepth,
  Funds,
  Instrument,
  TickData,
  OrderSide,
  OrderType,
  OrderStatus,
  OrderValidity,
  ProductType,
  InstrumentType,
  Currency,
} from '../../types';

import type {
  SaxoAccountBalance,
  SaxoOrder,
  SaxoPosition,
  SaxoNetPosition,
  SaxoTrade,
  SaxoQuote,
  SaxoInfoPrice,
  SaxoOHLC,
  SaxoInstrument,
  SaxoInstrumentDetails,
  SaxoWSQuoteUpdate,
  SaxoBuySell,
  SaxoOrderType,
  SaxoOrderStatus,
  SaxoAssetType,
} from './types';

import {
  SAXO_ORDER_TYPE_REVERSE_MAP,
  SAXO_STATUS_MAP,
  SAXO_SIDE_REVERSE_MAP,
  SAXO_EXCHANGE_MAP,
} from './constants';

// ============================================================================
// ORDER MAPPING
// ============================================================================

export function mapSaxoOrderToOrder(saxoOrder: SaxoOrder): Order {
  return {
    orderId: saxoOrder.OrderId,
    symbol: saxoOrder.DisplayAndFormat?.Symbol || `UIC:${saxoOrder.Uic}`,
    exchange: saxoOrder.Exchange?.ExchangeId || 'SAXO',
    side: mapSaxoSideToOrderSide(saxoOrder.BuySell),
    orderType: mapSaxoOrderTypeToOrderType(saxoOrder.OpenOrderType),
    productType: mapSaxoAssetTypeToProductType(saxoOrder.AssetType),
    status: mapSaxoStatusToOrderStatus(saxoOrder.Status),
    quantity: saxoOrder.Amount,
    filledQuantity: saxoOrder.FilledAmount,
    pendingQuantity: saxoOrder.Amount - saxoOrder.FilledAmount,
    price: saxoOrder.Price || 0,
    averagePrice: saxoOrder.MarketPrice || 0,
    triggerPrice: undefined,
    placedAt: new Date(saxoOrder.OrderTime),
    updatedAt: new Date(saxoOrder.OrderTime),
    validity: mapSaxoDurationToValidity(saxoOrder.Duration?.DurationType),
    exchangeOrderId: saxoOrder.OrderId,
    tag: saxoOrder.CorrelationKey,
    statusMessage: undefined,
  };
}

export function mapSaxoSideToOrderSide(side: SaxoBuySell): OrderSide {
  return SAXO_SIDE_REVERSE_MAP[side] as OrderSide || 'BUY';
}

export function mapSaxoOrderTypeToOrderType(orderType: SaxoOrderType): OrderType {
  return (SAXO_ORDER_TYPE_REVERSE_MAP[orderType] as OrderType) || 'MARKET';
}

export function mapSaxoStatusToOrderStatus(status: SaxoOrderStatus): OrderStatus {
  return (SAXO_STATUS_MAP[status] as OrderStatus) || 'PENDING';
}

export function mapSaxoDurationToValidity(durationType?: string): OrderValidity {
  const mapping: Record<string, OrderValidity> = {
    'DayOrder': 'DAY',
    'GoodTillCancel': 'GTC',
    'GoodTillDate': 'GTD',
    'ImmediateOrCancel': 'IOC',
    'FillOrKill': 'IOC',
    'AtTheOpening': 'DAY',
    'AtTheClose': 'DAY',
  };
  return mapping[durationType || ''] || 'DAY';
}

export function mapSaxoAssetTypeToProductType(assetType: SaxoAssetType): ProductType {
  // CFD and Forex typically intraday, everything else cash/delivery
  const intradayTypes = ['CfdOnStock', 'CfdOnIndex', 'CfdOnFutures', 'FxSpot', 'FxForwards'];
  const marginTypes = ['ContractFutures', 'StockOption', 'StockIndexOption', 'FuturesOption'];

  if (intradayTypes.includes(assetType)) return 'INTRADAY';
  if (marginTypes.includes(assetType)) return 'MARGIN';
  return 'CASH';
}

// ============================================================================
// POSITION MAPPING
// ============================================================================

export function mapSaxoPositionToPosition(saxoPosition: SaxoPosition): Position {
  const base = saxoPosition.PositionBase;
  const view = saxoPosition.PositionView;

  const quantity = Math.abs(base.Amount);
  const avgPrice = base.OpenPrice;
  const lastPrice = view.CurrentPrice;
  const pnl = view.ProfitLossOnTrade;
  const pnlPercent = quantity !== 0 && avgPrice !== 0
    ? (pnl / (quantity * avgPrice)) * 100
    : 0;

  return {
    symbol: saxoPosition.DisplayAndFormat?.Symbol || `UIC:${base.Uic}`,
    exchange: saxoPosition.Exchange?.ExchangeId || 'SAXO',
    productType: mapSaxoAssetTypeToProductType(base.AssetType),
    quantity: base.Amount, // Positive for long, negative for short
    buyQuantity: base.Amount > 0 ? quantity : 0,
    sellQuantity: base.Amount < 0 ? quantity : 0,
    buyValue: base.Amount > 0 ? quantity * avgPrice : 0,
    sellValue: base.Amount < 0 ? quantity * avgPrice : 0,
    averagePrice: avgPrice,
    lastPrice: lastPrice,
    pnl: pnl,
    pnlPercent: pnlPercent,
    dayPnl: pnl, // Saxo doesn't separate day PnL in this response
    overnight: false,
  };
}

export function mapSaxoNetPositionToPosition(netPosition: SaxoNetPosition): Position {
  const base = netPosition.NetPositionBase;
  const view = netPosition.NetPositionView;

  const quantity = Math.abs(base.Amount);
  const avgPrice = base.AverageOpenPrice;
  const lastPrice = view.CurrentPrice;
  const pnl = view.ProfitLossOnTrade;
  const pnlPercent = quantity !== 0 && avgPrice !== 0
    ? (pnl / (quantity * avgPrice)) * 100
    : 0;

  return {
    symbol: netPosition.DisplayAndFormat?.Symbol || `UIC:${base.Uic}`,
    exchange: netPosition.Exchange?.ExchangeId || 'SAXO',
    productType: mapSaxoAssetTypeToProductType(base.AssetType),
    quantity: base.Amount,
    buyQuantity: base.Amount > 0 ? quantity : 0,
    sellQuantity: base.Amount < 0 ? quantity : 0,
    buyValue: base.Amount > 0 ? quantity * avgPrice : 0,
    sellValue: base.Amount < 0 ? quantity * avgPrice : 0,
    averagePrice: avgPrice,
    lastPrice: lastPrice,
    pnl: pnl,
    pnlPercent: pnlPercent,
    dayPnl: pnl,
    overnight: false,
  };
}

// ============================================================================
// HOLDING MAPPING (Long-term positions / Portfolio)
// ============================================================================

export function mapSaxoPositionToHolding(saxoPosition: SaxoPosition): Holding {
  const base = saxoPosition.PositionBase;
  const view = saxoPosition.PositionView;

  const quantity = Math.abs(base.Amount);
  const avgPrice = base.OpenPrice;
  const lastPrice = view.CurrentPrice;
  const pnl = view.ProfitLossOnTrade;
  const pnlPercent = quantity !== 0 && avgPrice !== 0
    ? (pnl / (quantity * avgPrice)) * 100
    : 0;

  return {
    symbol: saxoPosition.DisplayAndFormat?.Symbol || `UIC:${base.Uic}`,
    exchange: saxoPosition.Exchange?.ExchangeId || 'SAXO',
    quantity: quantity,
    averagePrice: avgPrice,
    lastPrice: lastPrice,
    investedValue: quantity * avgPrice,
    currentValue: view.MarketValue,
    pnl: pnl,
    pnlPercent: pnlPercent,
    isin: undefined,
    pledgedQuantity: 0,
    collateralQuantity: 0,
    t1Quantity: quantity,
  };
}

// ============================================================================
// TRADE MAPPING
// ============================================================================

export function mapSaxoTradeToTrade(saxoTrade: SaxoTrade): Trade {
  return {
    tradeId: saxoTrade.TradeId,
    orderId: saxoTrade.OrderId,
    symbol: saxoTrade.DisplayAndFormat?.Symbol || `UIC:${saxoTrade.Uic}`,
    exchange: saxoTrade.Exchange?.ExchangeId || 'SAXO',
    side: mapSaxoSideToOrderSide(saxoTrade.BuySell),
    quantity: saxoTrade.Amount,
    price: saxoTrade.Price,
    productType: mapSaxoAssetTypeToProductType(saxoTrade.AssetType),
    tradedAt: new Date(saxoTrade.ExecutionTime),
    exchangeTradeId: saxoTrade.TradeId,
  };
}

// ============================================================================
// QUOTE MAPPING
// ============================================================================

export function mapSaxoQuoteToQuote(
  saxoQuote: SaxoQuote,
  symbol: string,
  exchange: string
): Quote {
  const quote = saxoQuote.Quote;
  const lastPrice = quote.Mid || ((quote.Ask + quote.Bid) / 2);

  return {
    symbol,
    exchange,
    lastPrice: lastPrice,
    open: quote.Open,
    high: quote.High,
    low: quote.Low,
    close: lastPrice,
    previousClose: 0,
    change: 0,
    changePercent: 0,
    volume: 0,
    bid: quote.Bid,
    bidQty: 0,
    ask: quote.Ask,
    askQty: 0,
    timestamp: new Date(saxoQuote.LastUpdated).getTime(),
  };
}

export function mapSaxoInfoPriceToQuote(
  infoPrice: SaxoInfoPrice,
  symbol: string,
  exchange: string
): Quote {
  const quote = infoPrice.Quote;
  const details = infoPrice.InstrumentPriceDetails;
  const lastPrice = details?.LastTraded || quote.Mid || ((quote.Ask + quote.Bid) / 2);
  const previousClose = details?.LastClose || 0;
  const change = previousClose !== 0 ? lastPrice - previousClose : 0;
  const changePercent = previousClose !== 0 ? (change / previousClose) * 100 : 0;

  return {
    symbol,
    exchange,
    lastPrice: lastPrice,
    open: details?.Open || quote.Open,
    high: quote.High,
    low: quote.Low,
    close: lastPrice,
    previousClose: previousClose,
    change: change,
    changePercent: changePercent,
    volume: details?.Volume || 0,
    bid: quote.Bid,
    bidQty: details?.BidSize || 0,
    ask: quote.Ask,
    askQty: details?.AskSize || 0,
    timestamp: new Date(infoPrice.LastUpdated).getTime(),
  };
}

// ============================================================================
// OHLCV MAPPING
// ============================================================================

export function mapSaxoOHLCToOHLCV(saxoOHLC: SaxoOHLC): OHLCV {
  return {
    timestamp: new Date(saxoOHLC.Time).getTime(),
    open: (saxoOHLC.OpenAsk + saxoOHLC.OpenBid) / 2,
    high: (saxoOHLC.HighAsk + saxoOHLC.HighBid) / 2,
    low: (saxoOHLC.LowAsk + saxoOHLC.LowBid) / 2,
    close: (saxoOHLC.CloseAsk + saxoOHLC.CloseBid) / 2,
    volume: saxoOHLC.Volume || 0,
  };
}

export function mapSaxoChartDataToOHLCV(data: SaxoOHLC[]): OHLCV[] {
  return data.map(mapSaxoOHLCToOHLCV);
}

// ============================================================================
// MARKET DEPTH MAPPING
// ============================================================================

export function mapSaxoToMarketDepth(
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  marketDepthData: any
): MarketDepth {
  const bids: Array<{ price: number; quantity: number; orders?: number }> = [];
  const asks: Array<{ price: number; quantity: number; orders?: number }> = [];

  if (marketDepthData?.Bids) {
    for (const bid of marketDepthData.Bids) {
      bids.push({
        price: bid.Price || 0,
        quantity: bid.Amount || 0,
        orders: bid.NumberOfOrders || 1,
      });
    }
  }

  if (marketDepthData?.Asks) {
    for (const ask of marketDepthData.Asks) {
      asks.push({
        price: ask.Price || 0,
        quantity: ask.Amount || 0,
        orders: ask.NumberOfOrders || 1,
      });
    }
  }

  return {
    bids,
    asks,
    timestamp: Date.now(),
  };
}

// ============================================================================
// FUNDS / BALANCE MAPPING
// ============================================================================

export function mapSaxoBalanceToFunds(balance: SaxoAccountBalance, currency: Currency = 'EUR'): Funds {
  return {
    availableCash: balance.CashAvailableForTrading,
    usedMargin: balance.MarginUsedByCurrentPositions,
    availableMargin: balance.MarginAvailableForTrading,
    totalBalance: balance.TotalValue,
    currency: currency,
    collateral: balance.CollateralAvailable,
    unrealizedPnl: balance.UnrealizedMarginProfitLoss,
    realizedPnl: 0,
  };
}

// ============================================================================
// INSTRUMENT MAPPING
// ============================================================================

export function mapSaxoInstrumentToInstrument(
  saxoInstrument: SaxoInstrument,
  currency: Currency = 'EUR'
): Instrument {
  return {
    symbol: saxoInstrument.Symbol,
    name: saxoInstrument.Description,
    exchange: saxoInstrument.ExchangeId,
    instrumentType: mapSaxoAssetTypeToInstrumentType(saxoInstrument.AssetType),
    currency: currency,
    token: saxoInstrument.Uic.toString(),
    isin: saxoInstrument.Identifier || undefined,
    lotSize: 1,
    tickSize: 0.01,
  };
}

export function mapSaxoInstrumentDetailsToInstrument(
  details: SaxoInstrumentDetails,
  currency: Currency = 'EUR'
): Instrument {
  return {
    symbol: details.Symbol,
    name: details.Description,
    exchange: details.Exchange?.ExchangeId || 'SAXO',
    instrumentType: mapSaxoAssetTypeToInstrumentType(details.AssetType),
    currency: currency,
    token: details.Uic.toString(),
    lotSize: details.LotSize || 1,
    tickSize: details.TickSize || 0.01,
  };
}

function mapSaxoAssetTypeToInstrumentType(assetType: SaxoAssetType): InstrumentType {
  const mapping: Record<string, InstrumentType> = {
    Stock: 'EQUITY',
    Etf: 'ETF',
    Etn: 'ETF',
    Etc: 'ETF',
    Fund: 'MUTUAL_FUND',
    MutualFund: 'MUTUAL_FUND',
    Bond: 'BOND',
    StockOption: 'OPTION',
    StockIndexOption: 'OPTION',
    FuturesOption: 'OPTION',
    ContractFutures: 'FUTURE',
  };
  return mapping[assetType] || 'EQUITY';
}

// ============================================================================
// WEBSOCKET TICK DATA MAPPING
// ============================================================================

export function mapSaxoWSQuoteToTickData(
  wsQuote: SaxoWSQuoteUpdate,
  symbol: string,
  exchange: string
): TickData {
  const quote = wsQuote.Quote;
  const lastPrice = quote.Mid || ((quote.Ask + quote.Bid) / 2);

  return {
    symbol,
    exchange,
    mode: 'full',
    lastPrice: lastPrice,
    timestamp: new Date(wsQuote.LastUpdated).getTime(),
    open: quote.Open || 0,
    high: quote.High || 0,
    low: quote.Low || 0,
    close: lastPrice,
    volume: 0,
    bid: quote.Bid,
    bidQty: 0,
    ask: quote.Ask,
    askQty: 0,
    change: 0,
    changePercent: 0,
  };
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

export function getExchangeName(exchangeId: string): string {
  return SAXO_EXCHANGE_MAP[exchangeId] || exchangeId;
}

export function parseSymbolExchange(symbolWithExchange: string): {
  symbol: string;
  exchange: string;
} {
  const parts = symbolWithExchange.split(':');
  if (parts.length === 2) {
    return { symbol: parts[0], exchange: parts[1] };
  }
  return { symbol: symbolWithExchange, exchange: 'SAXO' };
}

export function formatSymbolForSaxo(symbol: string, exchange: string): string {
  return `${symbol}:${exchange}`;
}
