// Kite Connect Type Definitions
// All TypeScript interfaces and types for Zerodha Kite Connect API v3

export interface KiteConfig {
  apiKey: string;
  apiSecret: string;
  accessToken?: string;
  baseUrl?: string;
  timeout?: number;
}

export interface SessionData {
  user_type: string;
  email: string;
  user_name: string;
  user_shortname: string;
  broker: string;
  exchanges: string[];
  products: string[];
  order_types: string[];
  avatar_url: string;
  user_id: string;
  api_key: string;
  access_token: string;
  public_token: string;
  refresh_token: string;
  silo: string;
  login_time: string;
}

export interface OrderParams {
  tradingsymbol: string;
  exchange: string;
  transaction_type: 'BUY' | 'SELL';
  order_type: 'MARKET' | 'LIMIT' | 'SL' | 'SL-M';
  quantity: number;
  product: 'CNC' | 'MIS' | 'NRML' | 'MTF';
  validity?: 'DAY' | 'IOC' | 'TTL';
  validity_ttl?: number;
  price?: number;
  trigger_price?: number;
  disclosed_quantity?: number;
  variety?: 'regular' | 'amo' | 'co' | 'iceberg' | 'auction';
  tag?: string;
  iceberg_legs?: number;
  iceberg_quantity?: number;
  auction_number?: string;
  market_protection?: number;
  autoslice?: boolean;
}

export interface OrderResponse {
  order_id: string;
}

export interface Order {
  order_id: string;
  parent_order_id?: string;
  exchange_order_id?: string;
  placed_by: string;
  variety: string;
  status: string;
  status_message?: string;
  order_timestamp: string;
  exchange_timestamp?: string;
  exchange_update_timestamp?: string;
  tradingsymbol: string;
  exchange: string;
  instrument_token: number;
  transaction_type: string;
  order_type: string;
  product: string;
  validity: string;
  price: number;
  quantity: number;
  trigger_price: number;
  average_price: number;
  pending_quantity: number;
  filled_quantity: number;
  disclosed_quantity: number;
  cancelled_quantity: number;
  market_protection: number;
  meta: Record<string, any>;
  tag?: string;
  guid: string;
  modified: boolean;
}

export interface Trade {
  trade_id: string;
  order_id: string;
  exchange_order_id?: string;
  tradingsymbol: string;
  exchange: string;
  instrument_token: number;
  transaction_type: string;
  product: string;
  average_price: number;
  quantity: number;
  fill_timestamp: string;
  exchange_timestamp: string;
  order_timestamp: string;
}

export interface Quote {
  instrument_token: number;
  timestamp?: string;
  last_trade_time?: string;
  last_price: number;
  last_quantity?: number;
  buy_quantity?: number;
  sell_quantity?: number;
  volume?: number;
  average_price?: number;
  oi?: number;
  oi_day_high?: number;
  oi_day_low?: number;
  net_change?: number;
  lower_circuit_limit?: number;
  upper_circuit_limit?: number;
  ohlc?: {
    open: number;
    high: number;
    low: number;
    close: number;
  };
  depth?: {
    buy: Array<{
      price: number;
      quantity: number;
      orders: number;
    }>;
    sell: Array<{
      price: number;
      quantity: number;
      orders: number;
    }>;
  };
}

export interface Holding {
  tradingsymbol: string;
  exchange: string;
  instrument_token: number;
  isin: string;
  product: string;
  price: number;
  quantity: number;
  used_quantity: number;
  t1_quantity: number;
  realised_quantity: number;
  authorised_quantity: number;
  authorised_date: string;
  opening_quantity: number;
  collateral_quantity: number;
  collateral_type: string;
  discrepancy: boolean;
  average_price: number;
  last_price: number;
  close_price: number;
  pnl: number;
  day_change: number;
  day_change_percentage: number;
}

export interface Position {
  tradingsymbol: string;
  exchange: string;
  instrument_token: number;
  product: string;
  quantity: number;
  overnight_quantity: number;
  multiplier: number;
  average_price: number;
  close_price: number;
  last_price: number;
  value: number;
  pnl: number;
  m2m: number;
  unrealised: number;
  realised: number;
  buy_quantity: number;
  buy_price: number;
  buy_value: number;
  buy_m2m: number;
  sell_quantity: number;
  sell_price: number;
  sell_value: number;
  sell_m2m: number;
  day_buy_quantity: number;
  day_buy_price: number;
  day_buy_value: number;
  day_sell_quantity: number;
  day_sell_price: number;
  day_sell_value: number;
}

export interface HistoricalParams {
  instrument_token: string;
  interval: 'minute' | 'day' | '3minute' | '5minute' | '10minute' | '15minute' | '30minute' | '60minute';
  from: string;
  to: string;
  continuous?: boolean;
  oi?: boolean;
}

export interface CandleData {
  timestamp: string;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  oi?: number;
}

export interface GTTParams {
  type: 'single' | 'two-leg';
  condition: {
    exchange: string;
    tradingsymbol: string;
    trigger_values: number[];
    last_price: number;
    instrument_token?: number;
  };
  orders: Array<{
    exchange: string;
    tradingsymbol: string;
    transaction_type: 'BUY' | 'SELL';
    quantity: number;
    order_type: 'LIMIT' | 'MARKET';
    product: string;
    price?: number;
  }>;
}

export interface GTTResponse {
  trigger_id: number;
}

export interface GTT {
  id: number;
  user_id: string;
  parent_trigger?: number;
  type: string;
  created_at: string;
  updated_at: string;
  expires_at: string;
  status: string;
  condition: Record<string, any>;
  orders: Array<Record<string, any>>;
  meta: Record<string, any>;
}

export interface AlertParams {
  name: string;
  type: 'simple' | 'ato';
  lhs_exchange: string;
  lhs_tradingsymbol: string;
  lhs_attribute: 'LastTradedPrice';
  operator: '<=' | '>=' | '<' | '>' | '==';
  rhs_type: 'constant' | 'instrument';
  rhs_constant?: number;
  rhs_exchange?: string;
  rhs_tradingsymbol?: string;
  rhs_attribute?: string;
  basket?: string;
}

export interface Alert {
  type: string;
  user_id: string;
  uuid: string;
  name: string;
  status: string;
  disabled_reason: string;
  lhs_attribute: string;
  lhs_exchange: string;
  lhs_tradingsymbol: string;
  operator: string;
  rhs_type: string;
  rhs_attribute: string;
  rhs_exchange: string;
  rhs_tradingsymbol: string;
  rhs_constant: number;
  alert_count: number;
  created_at: string;
  updated_at: string;
  basket?: Record<string, any>;
}

export interface MarginOrder {
  exchange: string;
  tradingsymbol: string;
  transaction_type: 'BUY' | 'SELL';
  variety: string;
  product: string;
  order_type: string;
  quantity: number;
  price: number;
  trigger_price: number;
}

export interface MarginResponse {
  type: string;
  tradingsymbol: string;
  exchange: string;
  span: number;
  exposure: number;
  option_premium: number;
  additional: number;
  bo: number;
  cash: number;
  var: number;
  pnl: {
    realised: number;
    unrealised: number;
  };
  leverage: number;
  charges: {
    transaction_tax: number;
    transaction_tax_type: string;
    exchange_turnover_charge: number;
    sebi_turnover_charge: number;
    brokerage: number;
    stamp_duty: number;
    gst: {
      igst: number;
      cgst: number;
      sgst: number;
      total: number;
    };
    total: number;
  };
  total: number;
}

export interface MFOrder {
  order_id: string;
  exchange_order_id?: string;
  tradingsymbol: string;
  status: string;
  status_message?: string;
  folio?: string;
  fund: string;
  order_timestamp: string;
  exchange_timestamp?: string;
  settlement_id?: string;
  transaction_type: 'BUY' | 'SELL';
  amount: number;
  variety: string;
  purchase_type?: string;
  quantity: number;
  price: number;
  last_price: number;
  average_price: number;
  placed_by: string;
  last_price_date: string;
  tag?: string;
}

export interface MFHolding {
  folio?: string;
  fund: string;
  tradingsymbol: string;
  average_price: number;
  last_price: number;
  pnl: number;
  last_price_date: string;
  quantity: number;
  pledged_quantity: number;
}

export interface MFSip {
  sip_id: string;
  tradingsymbol: string;
  fund: string;
  dividend_type: string;
  transaction_type: 'BUY' | 'SELL';
  status: string;
  created: string;
  frequency: string;
  next_instalment: string;
  instalment_amount: number;
  instalments: number;
  last_instalment: string;
  pending_instalments: number;
  instalment_day: number;
  completed_instalments: number;
  tag?: string;
  sip_reg_num?: string;
  step_up?: Record<string, number>;
}

export interface PostbackPayload {
  user_id: string;
  unfilled_quantity: number;
  app_id: number;
  checksum: string;
  placed_by: string;
  order_id: string;
  exchange_order_id?: string;
  parent_order_id?: string;
  status: string;
  status_message?: string;
  status_message_raw?: string;
  order_timestamp: string;
  exchange_update_timestamp?: string;
  exchange_timestamp?: string;
  variety: string;
  exchange: string;
  tradingsymbol: string;
  instrument_token: number;
  order_type: string;
  transaction_type: string;
  validity: string;
  product: string;
  quantity: number;
  disclosed_quantity: number;
  price: number;
  trigger_price: number;
  average_price: number;
  filled_quantity: number;
  pending_quantity: number;
  cancelled_quantity: number;
  market_protection: number;
  meta: Record<string, any>;
  tag?: string;
  guid: string;
}

export class KiteError extends Error {
  constructor(
    message: string,
    public code?: string,
    public statusCode?: number
  ) {
    super(message);
    this.name = 'KiteError';
  }
}

export class KiteAuthError extends KiteError {
  constructor(message: string = 'Authentication required') {
    super(message, 'AUTH_ERROR', 403);
    this.name = 'KiteAuthError';
  }
}

export class KiteRateLimitError extends KiteError {
  constructor(message: string = 'Rate limit exceeded') {
    super(message, 'RATE_LIMIT', 429);
    this.name = 'KiteRateLimitError';
  }
}
