// Fyers Broker Type Definitions
// All TypeScript interfaces and types for Fyers API v3

export interface FyersCredentials {
  appId: string;
  appType?: string;
  redirectUrl: string;
  accessToken: string;
}

export interface FyersAuthConfig {
  clientId: string;
  pin: string;
  appId: string;
  appType: string;
  appSecret: string;
  totpKey: string;
}

export interface FyersAuthResult {
  accessToken: string;
  connectionString: string;
}

export interface FyersProfile {
  name: string;
  image: string;
  display_name: string;
  email_id: string;
  PAN: string;
}

export interface FyersFund {
  id: number;
  title: string;
  equityAmount: number;
  commodityAmount: number;
}

export interface FyersHolding {
  costPrice: number;
  id: number;
  fyToken: string;
  symbol: string;
  isin: string;
  quantity: number;
  exchange: number;
  segment: number;
  qty_t1: number;
  remainingQuantity: number;
  collateralQuantity: number;
  remainingPledgeQuantity: number;
  pl: number;
  ltp: number;
  marketVal: number;
  holdingType: string;
}

export interface FyersHoldingsResponse {
  overall: {
    count_total: number;
    pnl_perc: number;
    total_current_value: number;
    total_investment: number;
    total_pl: number;
  };
  holdings: FyersHolding[];
}

export interface FyersPosition {
  id: string;
  symbol: string;
  fyToken: string;
  side: number;
  segment: number;
  qty: number;
  buyQty: number;
  sellQty: number;
  buyAvg: number;
  sellAvg: number;
  netQty: number;
  netAvg: number;
  ltp: number;
  pl: number;
  realized_profit: number;
  unrealized_profit: number;
  productType: string;
}

export interface PlaceOrderRequest {
  symbol: string;
  qty: number;
  type: number;
  side: number;
  productType: string;
  limitPrice: number;
  stopPrice: number;
  disclosedQty: number;
  validity: string;
  offlineOrder: boolean;
  stopLoss: number;
  takeProfit: number;
  orderTag?: string;
}

export interface FyersOrder {
  clientId: string;
  id: string;
  exchOrdId: string;
  qty: number;
  filledQty: number;
  limitPrice: number;
  type: number;
  fyToken: string;
  exchange: number;
  segment: number;
  symbol: string;
  orderDateTime: string;
  orderValidity: string;
  productType: string;
  side: number;
  status: number;
  ex_sym: string;
  description: string;
}

export interface FyersMarketStatus {
  exchange: number;
  market_type: string;
  segment: number;
  status: string;
}

export interface FyersQuote {
  n: string;
  s?: string;
  lp?: number;
  ltp?: number;
  ch?: number;
  chp?: number;
  high?: number;
  low?: number;
  open?: number;
  volume?: number;
  v?: number | {
    ch: number;
    chp: number;
    lp: number;
    spread: number;
    ask: number;
    bid: number;
    open_price: number;
    high_price: number;
    low_price: number;
    prev_close_price: number;
    volume: number;
  };
  o?: number;
  h?: number;
  l?: number;
}

export interface FyersMarketDepth {
  totalbuyqty: number;
  totalsellqty: number;
  bids: Array<{ price: number; volume: number; ord: number }>;
  ask: Array<{ price: number; volume: number; ord: number }>;
  o: number;
  h: number;
  l: number;
  c: number;
  chp: number;
  ch: number;
  ltp: number;
  v: number;
  atp: number;
  lower_ckt: number;
  upper_ckt: number;
}

export interface FyersHistoryCandle {
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface SymbolMasterEntry {
  fytoken?: string;
  fyToken?: string;
  symbol: string;
  description: string;
  exchange: string;
  segment: string;
  isin?: string;
  lotsize?: number;
  lotSize?: number;
  tick_size?: number;
  tickSize?: number;
  shortName?: string;
  displayName?: string;
}

export interface FyersOrderUpdate {
  clientId: string;
  id: string;
  exchOrdId: string;
  qty: number;
  filledQty: number;
  limitPrice: number;
  type: number;
  fyToken: string;
  exchange: number;
  segment: number;
  symbol: string;
  orderDateTime: string;
  orderValidity: string;
  productType: string;
  side: number;
  status: number;
  ex_sym: string;
  description: string;
}

export interface FyersTradeUpdate {
  id: string;
  orderId: string;
  symbol: string;
  side: number;
  qty: number;
  price: number;
  tradeDateTime: string;
}

export interface FyersPositionUpdate {
  symbol: string;
  netQty: number;
  avgPrice: number;
  ltp: number;
  pl: number;
  side: number;
}

export type OrderCallback = (order: FyersOrderUpdate) => void;
export type TradeCallback = (trade: FyersTradeUpdate) => void;
export type PositionCallback = (position: FyersPositionUpdate) => void;
export type GeneralCallback = (data: any) => void;
export type ErrorCallback = (error: any) => void;
