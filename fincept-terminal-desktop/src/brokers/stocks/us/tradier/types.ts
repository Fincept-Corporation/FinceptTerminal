/**
 * Tradier-specific Types
 *
 * Type definitions for Tradier Brokerage API responses and requests
 */

// ============================================================================
// TRADIER API RESPONSE TYPES
// ============================================================================

export interface TradierProfile {
  profile: {
    account: TradierAccount | TradierAccount[];
    id: string;
    name: string;
  };
}

export interface TradierAccount {
  account_number: string;
  classification: 'individual' | 'entity' | 'joint_survivor' | 'traditional_ira' | 'roth_ira' | 'rollover_ira' | 'sep_ira';
  date_created: string;
  day_trader: boolean;
  option_level: number;
  status: 'active' | 'closed';
  type: 'cash' | 'margin' | 'pdt';
  last_update_date: string;
}

export interface TradierBalances {
  balances: {
    option_short_value: number;
    total_equity: number;
    account_number: string;
    account_type: 'cash' | 'margin' | 'pdt';
    close_pl: number;
    current_requirement: number;
    equity: number;
    long_market_value: number;
    market_value: number;
    open_pl: number;
    option_long_value: number;
    option_requirement: number;
    pending_orders_count: number;
    short_market_value: number;
    stock_long_value: number;
    total_cash: number;
    uncleared_funds: number;
    pending_cash: number;
    // Margin-specific fields
    margin?: {
      fed_call: number;
      maintenance_call: number;
      option_buying_power: number;
      stock_buying_power: number;
      stock_short_value: number;
      sweep: number;
    };
    // Cash-specific fields
    cash?: {
      cash_available: number;
      sweep: number;
      unsettled_funds: number;
    };
    // PDT-specific fields
    pdt?: {
      fed_call: number;
      maintenance_call: number;
      option_buying_power: number;
      stock_buying_power: number;
      stock_short_value: number;
    };
  };
}

export interface TradierPosition {
  cost_basis: number;
  date_acquired: string;
  id: number;
  quantity: number;
  symbol: string;
}

export interface TradierPositions {
  positions: {
    position: TradierPosition | TradierPosition[] | null;
  };
}

export interface TradierOrder {
  id: number;
  type: TradierOrderType;
  symbol: string;
  side: TradierSide;
  quantity: number;
  status: TradierOrderStatus;
  duration: TradierDuration;
  price?: number;
  avg_fill_price?: number;
  exec_quantity?: number;
  last_fill_price?: number;
  last_fill_quantity?: number;
  remaining_quantity?: number;
  create_date: string;
  transaction_date: string;
  class: 'equity' | 'option' | 'multileg' | 'combo';
  option_symbol?: string;
  stop_price?: number;
  tag?: string;
  // For multileg orders
  num_legs?: number;
  strategy?: string;
  leg?: TradierOrderLeg[];
}

export interface TradierOrderLeg {
  id: number;
  type: TradierOrderType;
  symbol: string;
  side: TradierSide;
  quantity: number;
  status: TradierOrderStatus;
  duration: TradierDuration;
  price?: number;
  avg_fill_price?: number;
  exec_quantity?: number;
  last_fill_price?: number;
  last_fill_quantity?: number;
  remaining_quantity?: number;
  create_date: string;
  transaction_date: string;
  class: string;
  option_symbol?: string;
}

export interface TradierOrders {
  orders: {
    order: TradierOrder | TradierOrder[] | null;
  };
}

export type TradierOrderType = 'market' | 'limit' | 'stop' | 'stop_limit' | 'debit' | 'credit' | 'even';
export type TradierSide = 'buy' | 'sell' | 'buy_to_open' | 'sell_to_open' | 'buy_to_close' | 'sell_to_close';
export type TradierDuration = 'day' | 'gtc' | 'pre' | 'post';
export type TradierOrderStatus =
  | 'open'
  | 'partially_filled'
  | 'filled'
  | 'expired'
  | 'canceled'
  | 'pending'
  | 'rejected'
  | 'error';

export interface TradierGainLoss {
  gainloss: {
    closed_position: TradierClosedPosition | TradierClosedPosition[] | null;
  };
}

export interface TradierClosedPosition {
  close_date: string;
  cost: number;
  gain_loss: number;
  gain_loss_percent: number;
  open_date: string;
  proceeds: number;
  quantity: number;
  symbol: string;
  term: number;
}

export interface TradierHistory {
  history: {
    event: TradierHistoryEvent | TradierHistoryEvent[] | null;
  };
}

export interface TradierHistoryEvent {
  amount: number;
  date: string;
  type: string;
  trade?: {
    commission: number;
    description: string;
    price: number;
    quantity: number;
    symbol: string;
    trade_type: string;
  };
  adjustment?: {
    description: string;
    quantity: number;
  };
}

// ============================================================================
// MARKET DATA TYPES
// ============================================================================

export interface TradierQuote {
  symbol: string;
  description: string;
  exch: string;
  type: string;
  last?: number;
  change?: number;
  volume?: number;
  open?: number;
  high?: number;
  low?: number;
  close?: number;
  bid?: number;
  ask?: number;
  bidsize?: number;
  asksize?: number;
  bidexch?: string;
  askexch?: string;
  change_percentage?: number;
  average_volume?: number;
  last_volume?: number;
  trade_date?: number;
  prevclose?: number;
  week_52_high?: number;
  week_52_low?: number;
  biddate?: number;
  askdate?: number;
  root_symbols?: string;
  underlying?: string;
  strike?: number;
  contract_size?: number;
  expiration_date?: string;
  expiration_type?: string;
  option_type?: string;
  root_symbol?: string;
  greeks?: TradierGreeks;
}

export interface TradierGreeks {
  delta: number;
  gamma: number;
  theta: number;
  vega: number;
  rho: number;
  phi: number;
  bid_iv: number;
  mid_iv: number;
  ask_iv: number;
  smv_vol: number;
  updated_at: string;
}

export interface TradierQuotes {
  quotes: {
    quote: TradierQuote | TradierQuote[] | 'null';
    unmatched_symbols?: {
      symbol: string | string[];
    };
  };
}

export interface TradierHistoricalData {
  history: {
    day: TradierHistoricalBar | TradierHistoricalBar[] | null;
  } | null;
}

export interface TradierHistoricalBar {
  date: string;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface TradierTimeSales {
  series: {
    data: TradierTimeSale | TradierTimeSale[] | null;
  } | null;
}

export interface TradierTimeSale {
  time: string;
  timestamp: number;
  price: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  vwap: number;
}

export interface TradierOptionChain {
  options: {
    option: TradierOption | TradierOption[] | null;
  } | null;
}

export interface TradierOption {
  symbol: string;
  description: string;
  exch: string;
  type: string;
  last: number;
  change: number;
  volume: number;
  open: number;
  high: number;
  low: number;
  close: number;
  bid: number;
  ask: number;
  underlying: string;
  strike: number;
  change_percentage: number;
  average_volume: number;
  last_volume: number;
  trade_date: number;
  prevclose: number;
  week_52_high: number;
  week_52_low: number;
  bidsize: number;
  bidexch: string;
  bid_date: number;
  asksize: number;
  askexch: string;
  ask_date: number;
  open_interest: number;
  contract_size: number;
  expiration_date: string;
  expiration_type: string;
  option_type: string;
  root_symbol: string;
  greeks?: TradierGreeks;
}

export interface TradierExpirations {
  expirations: {
    date: string | string[] | null;
  } | null;
}

export interface TradierStrikes {
  strikes: {
    strike: number | number[] | null;
  } | null;
}

export interface TradierOptionSymbols {
  symbols: string[] | null;
}

// ============================================================================
// STREAMING TYPES
// ============================================================================

export interface TradierStreamSession {
  stream: {
    sessionid: string;
    url: string;
  };
}

export interface TradierStreamQuote {
  type: 'quote';
  symbol: string;
  bid: number;
  bidsz: number;
  bidexch: string;
  biddate: string;
  ask: number;
  asksz: number;
  askexch: string;
  askdate: string;
}

export interface TradierStreamTrade {
  type: 'trade';
  symbol: string;
  exch: string;
  price: number;
  size: number;
  cvol: number;
  date: string;
  last: number;
}

export interface TradierStreamSummary {
  type: 'summary';
  symbol: string;
  open: number;
  high: number;
  low: number;
  prevClose: number;
}

export interface TradierStreamTimesale {
  type: 'timesale';
  symbol: string;
  exch: string;
  bid: number;
  ask: number;
  last: number;
  size: number;
  date: string;
  seq: number;
  flag: string;
  cancel: boolean;
  correction: boolean;
  session: string;
}

export type TradierStreamMessage =
  | TradierStreamQuote
  | TradierStreamTrade
  | TradierStreamSummary
  | TradierStreamTimesale;

// ============================================================================
// TRADING/CALENDAR TYPES
// ============================================================================

export interface TradierClock {
  clock: {
    date: string;
    description: string;
    state: 'premarket' | 'open' | 'postmarket' | 'closed';
    timestamp: number;
    next_change: string;
    next_state: string;
  };
}

export interface TradierCalendar {
  calendar: {
    month: number;
    year: number;
    days: {
      day: TradierCalendarDay | TradierCalendarDay[];
    };
  };
}

export interface TradierCalendarDay {
  date: string;
  status: 'open' | 'closed';
  description: string;
  premarket: {
    start: string;
    end: string;
  };
  open: {
    start: string;
    end: string;
  };
  postmarket: {
    start: string;
    end: string;
  };
}

export interface TradierSymbolLookup {
  securities: {
    security: TradierSecurity | TradierSecurity[] | null;
  } | null;
}

export interface TradierSecurity {
  symbol: string;
  exchange: string;
  type: string;
  description: string;
}

// ============================================================================
// REQUEST TYPES
// ============================================================================

export interface TradierOrderRequest {
  class: 'equity' | 'option' | 'multileg' | 'combo';
  symbol: string;
  side: TradierSide;
  quantity: number;
  type: TradierOrderType;
  duration: TradierDuration;
  price?: number;
  stop?: number;
  tag?: string;
  // For options
  option_symbol?: string;
  // For preview
  preview?: boolean;
}

export interface TradierModifyOrderRequest {
  type?: TradierOrderType;
  duration?: TradierDuration;
  price?: number;
  stop?: number;
}

// ============================================================================
// ERROR TYPES
// ============================================================================

export interface TradierError {
  fault?: {
    faultstring: string;
    detail: {
      errorcode: string;
    };
  };
  errors?: {
    error: string | string[];
  };
}
