/**
 * Alpaca-specific Types
 *
 * Type definitions for Alpaca Trading API responses and requests
 */

// ============================================================================
// ALPACA API RESPONSE TYPES
// ============================================================================

export interface AlpacaAccount {
  id: string;
  account_number: string;
  status: string;
  crypto_status?: string;
  currency: string;
  buying_power: string;
  regt_buying_power: string;
  daytrading_buying_power: string;
  non_marginable_buying_power: string;
  cash: string;
  accrued_fees: string;
  pending_transfer_out: string;
  pending_transfer_in: string;
  portfolio_value: string;
  pattern_day_trader: boolean;
  trading_blocked: boolean;
  transfers_blocked: boolean;
  account_blocked: boolean;
  created_at: string;
  trade_suspended_by_user: boolean;
  multiplier: string;
  shorting_enabled: boolean;
  equity: string;
  last_equity: string;
  long_market_value: string;
  short_market_value: string;
  initial_margin: string;
  maintenance_margin: string;
  last_maintenance_margin: string;
  sma: string;
  daytrade_count: number;
}

export interface AlpacaOrder {
  id: string;
  client_order_id: string;
  created_at: string;
  updated_at: string;
  submitted_at: string;
  filled_at: string | null;
  expired_at: string | null;
  canceled_at: string | null;
  failed_at: string | null;
  replaced_at: string | null;
  replaced_by: string | null;
  replaces: string | null;
  asset_id: string;
  symbol: string;
  asset_class: string;
  notional: string | null;
  qty: string | null;
  filled_qty: string;
  filled_avg_price: string | null;
  order_class: string;
  order_type: AlpacaOrderType;
  type: AlpacaOrderType;
  side: AlpacaSide;
  time_in_force: AlpacaTimeInForce;
  limit_price: string | null;
  stop_price: string | null;
  status: AlpacaOrderStatus;
  extended_hours: boolean;
  legs: AlpacaOrder[] | null;
  trail_percent: string | null;
  trail_price: string | null;
  hwm: string | null;
}

export type AlpacaOrderType = 'market' | 'limit' | 'stop' | 'stop_limit' | 'trailing_stop';
export type AlpacaSide = 'buy' | 'sell';
export type AlpacaTimeInForce = 'day' | 'gtc' | 'opg' | 'cls' | 'ioc' | 'fok';
export type AlpacaOrderStatus =
  | 'new'
  | 'partially_filled'
  | 'filled'
  | 'done_for_day'
  | 'canceled'
  | 'expired'
  | 'replaced'
  | 'pending_cancel'
  | 'pending_replace'
  | 'pending_new'
  | 'accepted'
  | 'stopped'
  | 'rejected'
  | 'suspended'
  | 'calculated';

export interface AlpacaPosition {
  asset_id: string;
  symbol: string;
  exchange: string;
  asset_class: string;
  asset_marginable: boolean;
  qty: string;
  avg_entry_price: string;
  side: 'long' | 'short';
  market_value: string;
  cost_basis: string;
  unrealized_pl: string;
  unrealized_plpc: string;
  unrealized_intraday_pl: string;
  unrealized_intraday_plpc: string;
  current_price: string;
  lastday_price: string;
  change_today: string;
  qty_available: string;
}

export interface AlpacaAsset {
  id: string;
  class: string;
  exchange: string;
  symbol: string;
  name: string;
  status: string;
  tradable: boolean;
  marginable: boolean;
  maintenance_margin_requirement: number;
  shortable: boolean;
  easy_to_borrow: boolean;
  fractionable: boolean;
  min_order_size: string;
  min_trade_increment: string;
  price_increment: string;
}

export interface AlpacaBar {
  t: string;  // timestamp
  o: number;  // open
  h: number;  // high
  l: number;  // low
  c: number;  // close
  v: number;  // volume
  n: number;  // number of trades
  vw: number; // volume weighted average price
}

export interface AlpacaQuote {
  t: string;  // timestamp
  ax: string; // ask exchange
  ap: number; // ask price
  as: number; // ask size
  bx: string; // bid exchange
  bp: number; // bid price
  bs: number; // bid size
  c: string[];// conditions
}

export interface AlpacaTrade {
  t: string;  // timestamp
  x: string;  // exchange
  p: number;  // price
  s: number;  // size
  c: string[];// conditions
  i: number;  // trade id
  z: string;  // tape
}

export interface AlpacaSnapshot {
  latestTrade: AlpacaTrade;
  latestQuote: AlpacaQuote;
  minuteBar: AlpacaBar;
  dailyBar: AlpacaBar;
  prevDailyBar: AlpacaBar;
}

// ============================================================================
// ALPACA REQUEST TYPES
// ============================================================================

export interface AlpacaOrderRequest {
  symbol: string;
  qty?: string;
  notional?: string;
  side: AlpacaSide;
  type: AlpacaOrderType;
  time_in_force: AlpacaTimeInForce;
  limit_price?: string;
  stop_price?: string;
  trail_price?: string;
  trail_percent?: string;
  extended_hours?: boolean;
  client_order_id?: string;
  order_class?: 'simple' | 'bracket' | 'oco' | 'oto';
  take_profit?: {
    limit_price: string;
  };
  stop_loss?: {
    stop_price: string;
    limit_price?: string;
  };
}

export interface AlpacaModifyOrderRequest {
  qty?: string;
  time_in_force?: AlpacaTimeInForce;
  limit_price?: string;
  stop_price?: string;
  trail?: string;
  client_order_id?: string;
}

// ============================================================================
// WEBSOCKET MESSAGE TYPES
// ============================================================================

export interface AlpacaWSAuthMessage {
  action: 'auth';
  key: string;
  secret: string;
}

export interface AlpacaWSSubscribeMessage {
  action: 'subscribe';
  trades?: string[];
  quotes?: string[];
  bars?: string[];
}

export interface AlpacaWSUnsubscribeMessage {
  action: 'unsubscribe';
  trades?: string[];
  quotes?: string[];
  bars?: string[];
}

export interface AlpacaWSTradeMessage {
  T: 't';      // type
  S: string;   // symbol
  i: number;   // trade id
  x: string;   // exchange
  p: number;   // price
  s: number;   // size
  t: string;   // timestamp
  c: string[]; // conditions
  z: string;   // tape
}

export interface AlpacaWSQuoteMessage {
  T: 'q';      // type
  S: string;   // symbol
  ax: string;  // ask exchange
  ap: number;  // ask price
  as: number;  // ask size
  bx: string;  // bid exchange
  bp: number;  // bid price
  bs: number;  // bid size
  t: string;   // timestamp
  c: string[]; // conditions
  z: string;   // tape
}

export interface AlpacaWSBarMessage {
  T: 'b';      // type
  S: string;   // symbol
  o: number;   // open
  h: number;   // high
  l: number;   // low
  c: number;   // close
  v: number;   // volume
  t: string;   // timestamp
  n: number;   // trade count
  vw: number;  // vwap
}

// Trade updates WebSocket
export interface AlpacaTradeUpdate {
  event: 'new' | 'fill' | 'partial_fill' | 'canceled' | 'expired' | 'done_for_day' | 'replaced' | 'rejected' | 'pending_new' | 'stopped' | 'pending_cancel' | 'pending_replace' | 'calculated' | 'suspended' | 'order_replace_rejected' | 'order_cancel_rejected';
  timestamp: string;
  order: AlpacaOrder;
  position_qty?: string;
  price?: string;
  qty?: string;
}
