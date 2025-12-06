// kite-connect-adapter.ts
// Comprehensive TypeScript adapter for Zerodha Kite Connect API v3
// Modular design for plug-and-play integration

import axios, { AxiosInstance, AxiosResponse } from 'axios';
import crypto from 'crypto';

// ================================
// TYPE DEFINITIONS
// ================================

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
  from: string; // 'yyyy-mm-dd hh:mm:ss'
  to: string;   // 'yyyy-mm-dd hh:mm:ss'
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
  basket?: string; // JSON string for ATO alerts
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

// ================================
// MAIN ADAPTER CLASS
// ================================

export class KiteConnectAdapter {
  private config: KiteConfig;
  private client: AxiosInstance;
  private readonly baseUrl: string;

  constructor(config: KiteConfig) {
    this.config = config;
    this.baseUrl = config.baseUrl || 'https://api.kite.trade';

    this.client = axios.create({
      baseURL: this.baseUrl,
      timeout: config.timeout || 10000,
      headers: {
        'X-Kite-Version': '3',
        'Content-Type': 'application/x-www-form-urlencoded',
      },
    });

    this.setupInterceptors();
  }

  private setupInterceptors(): void {
    // Request interceptor to add auth token
    this.client.interceptors.request.use(
      (config) => {
        if (this.config.accessToken) {
          config.headers.Authorization = `token ${this.config.apiKey}:${this.config.accessToken}`;
        }
        return config;
      },
      (error) => Promise.reject(error)
    );

    // Response interceptor for error handling
    this.client.interceptors.response.use(
      (response: AxiosResponse) => response,
      (error) => {
        if (error.response?.status === 403) {
          throw new Error('Token expired or invalid. Please re-authenticate.');
        }
        throw error;
      }
    );
  }

  // ================================
  // AUTHENTICATION METHODS
  // ================================

  /**
   * Generate login URL for manual authentication
   * User must complete this step manually
   */
  generateLoginUrl(redirectUrl?: string): string {
    const url = new URL('https://kite.zerodha.com/connect/login');
    url.searchParams.set('api_key', this.config.apiKey);
    if (redirectUrl) {
      url.searchParams.set('redirect_url', redirectUrl);
    }
    return url.toString();
  }

  /**
   * Generate session after manual login
   * Call this with the request_token received from redirect
   */
  async generateSession(requestToken: string): Promise<SessionData> {
    const checksum = crypto
      .createHash('sha256')
      .update(this.config.apiKey + requestToken + this.config.apiSecret)
      .digest('hex');

    const data = new URLSearchParams({
      api_key: this.config.apiKey,
      request_token: requestToken,
      checksum: checksum,
    });

    const response = await this.client.post('/session/token', data);
    const sessionData = response.data.data;

    // Store access token for future requests
    this.config.accessToken = sessionData.access_token;

    return sessionData;
  }

  /**
   * Invalidate session
   */
  async invalidateSession(): Promise<void> {
    await this.client.delete('/session/token');
    this.config.accessToken = undefined;
  }

  /**
   * Get user profile
   */
  async getProfile(): Promise<any> {
    const response = await this.client.get('/user/profile');
    return response.data.data;
  }

  /**
   * Get user margins
   */
  async getMargins(segment?: string): Promise<any> {
    const url = segment ? `/user/margins/${segment}` : '/user/margins';
    const response = await this.client.get(url);
    return response.data.data;
  }

  // ================================
  // ORDER MANAGEMENT
  // ================================

  /**
   * Place a new order
   */
  async placeOrder(params: OrderParams): Promise<OrderResponse> {
    const variety = params.variety || 'regular';
    const data = new URLSearchParams();

    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined && key !== 'variety') {
        data.append(key, value.toString());
      }
    });

    const response = await this.client.post(`/orders/${variety}`, data);
    return response.data.data;
  }

  /**
   * Modify an existing order
   */
  async modifyOrder(
    orderId: string,
    params: Partial<OrderParams>,
    variety: string = 'regular'
  ): Promise<OrderResponse> {
    const data = new URLSearchParams();

    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined) {
        data.append(key, value.toString());
      }
    });

    const response = await this.client.put(`/orders/${variety}/${orderId}`, data);
    return response.data.data;
  }

  /**
   * Cancel an order
   */
  async cancelOrder(orderId: string, variety: string = 'regular'): Promise<OrderResponse> {
    const response = await this.client.delete(`/orders/${variety}/${orderId}`);
    return response.data.data;
  }

  /**
   * Get all orders for the day
   */
  async getOrders(): Promise<Order[]> {
    const response = await this.client.get('/orders');
    return response.data.data;
  }

  /**
   * Get order history for a specific order
   */
  async getOrderHistory(orderId: string): Promise<Order[]> {
    const response = await this.client.get(`/orders/${orderId}`);
    return response.data.data;
  }

  /**
   * Get all trades for the day
   */
  async getTrades(): Promise<Trade[]> {
    const response = await this.client.get('/trades');
    return response.data.data;
  }

  /**
   * Get trades for a specific order
   */
  async getOrderTrades(orderId: string): Promise<Trade[]> {
    const response = await this.client.get(`/orders/${orderId}/trades`);
    return response.data.data;
  }

  // ================================
  // MARKET DATA
  // ================================

  /**
   * Get full market quotes for instruments
   * Max 500 instruments per request
   */
  async getQuotes(instruments: string[]): Promise<Record<string, Quote>> {
    if (instruments.length > 500) {
      throw new Error('Maximum 500 instruments allowed per request');
    }

    const params = new URLSearchParams();
    instruments.forEach(instrument => params.append('i', instrument));

    const response = await this.client.get(`/quote?${params.toString()}`);
    return response.data.data;
  }

  /**
   * Get OHLC quotes for instruments
   * Max 1000 instruments per request
   */
  async getOHLC(instruments: string[]): Promise<Record<string, any>> {
    if (instruments.length > 1000) {
      throw new Error('Maximum 1000 instruments allowed per request');
    }

    const params = new URLSearchParams();
    instruments.forEach(instrument => params.append('i', instrument));

    const response = await this.client.get(`/quote/ohlc?${params.toString()}`);
    return response.data.data;
  }

  /**
   * Get LTP for instruments
   * Max 1000 instruments per request
   */
  async getLTP(instruments: string[]): Promise<Record<string, any>> {
    if (instruments.length > 1000) {
      throw new Error('Maximum 1000 instruments allowed per request');
    }

    const params = new URLSearchParams();
    instruments.forEach(instrument => params.append('i', instrument));

    const response = await this.client.get(`/quote/ltp?${params.toString()}`);
    return response.data.data;
  }

  /**
   * Get historical data
   */
  async getHistoricalData(params: HistoricalParams): Promise<CandleData[]> {
    const queryParams = new URLSearchParams({
      from: params.from,
      to: params.to,
    });

    if (params.continuous) queryParams.append('continuous', '1');
    if (params.oi) queryParams.append('oi', '1');

    const response = await this.client.get(
      `/instruments/historical/${params.instrument_token}/${params.interval}?${queryParams.toString()}`
    );

    // Transform array format to object format
    return response.data.data.candles.map((candle: any[]) => ({
      timestamp: candle[0],
      open: candle[1],
      high: candle[2],
      low: candle[3],
      close: candle[4],
      volume: candle[5],
      oi: candle[6] || undefined,
    }));
  }

  /**
   * Get instruments list for exchange
   */
  async getInstruments(exchange?: string): Promise<string> {
    const url = exchange ? `/instruments/${exchange}` : '/instruments';
    const response = await this.client.get(url);
    return response.data; // Returns CSV data
  }

  // ================================
  // PORTFOLIO
  // ================================

  /**
   * Get holdings
   */
  async getHoldings(): Promise<Holding[]> {
    const response = await this.client.get('/portfolio/holdings');
    return response.data.data;
  }

  /**
   * Get positions
   */
  async getPositions(): Promise<{ net: Position[]; day: Position[] }> {
    const response = await this.client.get('/portfolio/positions');
    return response.data.data;
  }

  /**
   * Convert position
   */
  async convertPosition(params: {
    tradingsymbol: string;
    exchange: string;
    transaction_type: 'BUY' | 'SELL';
    position_type: 'overnight' | 'day';
    quantity: number;
    old_product: string;
    new_product: string;
  }): Promise<boolean> {
    const data = new URLSearchParams();
    Object.entries(params).forEach(([key, value]) => {
      data.append(key, value.toString());
    });

    const response = await this.client.put('/portfolio/positions', data);
    return response.data.data;
  }

  /**
   * Get holdings auction list
   */
  async getHoldingsAuctions(): Promise<any[]> {
    const response = await this.client.get('/portfolio/holdings/auctions');
    return response.data.data;
  }

  /**
   * Initiate holdings authorisation
   */
  async initiateHoldingsAuth(holdings?: Array<{ isin: string; quantity: number }>): Promise<{ request_id: string }> {
    const data = new URLSearchParams();

    if (holdings) {
      holdings.forEach(holding => {
        data.append('isin', holding.isin);
        data.append('quantity', holding.quantity.toString());
      });
    }

    const response = await this.client.post('/portfolio/holdings/authorise', data);
    return response.data.data;
  }

  // ================================
  // GTT (Good Till Triggered)
  // ================================

  /**
   * Place GTT order
   */
  async placeGTT(params: GTTParams): Promise<GTTResponse> {
    const data = new URLSearchParams({
      type: params.type,
      condition: JSON.stringify(params.condition),
      orders: JSON.stringify(params.orders),
    });

    const response = await this.client.post('/gtt/triggers', data);
    return response.data.data;
  }

  /**
   * Get all GTT orders
   */
  async getGTTs(): Promise<GTT[]> {
    const response = await this.client.get('/gtt/triggers');
    return response.data.data;
  }

  /**
   * Get specific GTT order
   */
  async getGTT(triggerId: number): Promise<GTT> {
    const response = await this.client.get(`/gtt/triggers/${triggerId}`);
    return response.data.data;
  }

  /**
   * Modify GTT order
   */
  async modifyGTT(triggerId: number, params: GTTParams): Promise<GTTResponse> {
    const data = new URLSearchParams({
      type: params.type,
      condition: JSON.stringify(params.condition),
      orders: JSON.stringify(params.orders),
    });

    const response = await this.client.put(`/gtt/triggers/${triggerId}`, data);
    return response.data.data;
  }

  /**
   * Cancel GTT order
   */
  async cancelGTT(triggerId: number): Promise<GTTResponse> {
    const response = await this.client.delete(`/gtt/triggers/${triggerId}`);
    return response.data.data;
  }

  // ================================
  // ALERTS
  // ================================

  /**
   * Create alert
   */
  async createAlert(params: AlertParams): Promise<Alert> {
    const data = new URLSearchParams();
    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined) {
        data.append(key, value.toString());
      }
    });

    const response = await this.client.post('/alerts', data);
    return response.data.data;
  }

  /**
   * Get all alerts
   */
  async getAlerts(): Promise<Alert[]> {
    const response = await this.client.get('/alerts');
    return response.data.data;
  }

  /**
   * Get specific alert
   */
  async getAlert(alertUuid: string): Promise<Alert> {
    const response = await this.client.get(`/alerts/${alertUuid}`);
    return response.data.data;
  }

  /**
   * Modify alert
   */
  async modifyAlert(alertUuid: string, params: Partial<AlertParams>): Promise<Alert> {
    const data = new URLSearchParams();
    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined) {
        data.append(key, value.toString());
      }
    });

    const response = await this.client.put(`/alerts/${alertUuid}`, data);
    return response.data.data;
  }

  /**
   * Delete alerts
   */
  async deleteAlerts(alertUuids: string[]): Promise<void> {
    const params = new URLSearchParams();
    alertUuids.forEach(uuid => params.append('uuid', uuid));

    await this.client.delete(`/alerts?${params.toString()}`);
  }

  /**
   * Get alert history
   */
  async getAlertHistory(alertUuid: string): Promise<any[]> {
    const response = await this.client.get(`/alerts/${alertUuid}/history`);
    return response.data.data;
  }

  // ================================
  // MARGINS
  // ================================

  /**
   * Calculate order margins
   */
  async calculateOrderMargins(orders: MarginOrder[], mode?: 'compact'): Promise<MarginResponse[]> {
    const params = new URLSearchParams();
    if (mode) params.append('mode', mode);

    const response = await this.client.post(
      `/margins/orders?${params.toString()}`,
      JSON.stringify(orders),
      {
        headers: { 'Content-Type': 'application/json' }
      }
    );
    return response.data.data;
  }

  /**
   * Calculate basket margins
   */
  async calculateBasketMargins(orders: MarginOrder[]): Promise<any> {
    const response = await this.client.post(
      '/margins/basket',
      JSON.stringify(orders),
      {
        headers: { 'Content-Type': 'application/json' }
      }
    );
    return response.data.data;
  }

  /**
   * Calculate order charges
   */
  async calculateOrderCharges(orders: MarginOrder[]): Promise<any> {
    const response = await this.client.post(
      '/charges/orders',
      JSON.stringify(orders),
      {
        headers: { 'Content-Type': 'application/json' }
      }
    );
    return response.data.data;
  }

  // ================================
  // MUTUAL FUNDS
  // ================================

  /**
   * Get mutual fund orders
   */
  async getMFOrders(): Promise<MFOrder[]> {
    const response = await this.client.get('/mf/orders');
    return response.data.data;
  }

  /**
   * Get specific mutual fund order
   */
  async getMFOrder(orderId: string): Promise<MFOrder> {
    const response = await this.client.get(`/mf/orders/${orderId}`);
    return response.data.data;
  }

  /**
   * Get mutual fund holdings
   */
  async getMFHoldings(): Promise<MFHolding[]> {
    const response = await this.client.get('/mf/holdings');
    return response.data.data;
  }

  /**
   * Get all SIPs
   */
  async getMFSips(): Promise<MFSip[]> {
    const response = await this.client.get('/mf/sips');
    return response.data.data;
  }

  /**
   * Get mutual fund instruments
   */
  async getMFInstruments(): Promise<string> {
    const response = await this.client.get('/mf/instruments');
    return response.data; // Returns CSV data
  }

  // ================================
  // UTILITY METHODS
  // ================================

  /**
   * Validate postback checksum
   */
  validatePostbackChecksum(orderId: string, orderTimestamp: string, receivedChecksum: string): boolean {
    const expectedChecksum = crypto
      .createHash('sha256')
      .update(orderId + orderTimestamp + this.config.apiSecret)
      .digest('hex');

    return expectedChecksum === receivedChecksum;
  }

  /**
   * Set access token (for session persistence)
   */
  setAccessToken(accessToken: string): void {
    this.config.accessToken = accessToken;
  }

  /**
   * Get current access token
   */
  getAccessToken(): string | undefined {
    return this.config.accessToken;
  }

  /**
   * Check if authenticated
   */
  isAuthenticated(): boolean {
    return !!this.config.accessToken;
  }
}

// ================================
// FACTORY FUNCTION
// ================================

export function createKiteAdapter(config: KiteConfig): KiteConnectAdapter {
  return new KiteConnectAdapter(config);
}

// ================================
// RATE LIMITER UTILITY
// ================================

export class RateLimiter {
  private requests: number[] = [];
  private readonly limit: number;
  private readonly window: number;

  constructor(limit: number = 100, windowMs: number = 60000) {
    this.limit = limit;
    this.window = windowMs;
  }

  async checkLimit(): Promise<void> {
    const now = Date.now();
    this.requests = this.requests.filter(time => now - time < this.window);

    if (this.requests.length >= this.limit) {
      const oldestRequest = Math.min(...this.requests);
      const waitTime = this.window - (now - oldestRequest);
      await new Promise(resolve => setTimeout(resolve, waitTime));
      return this.checkLimit();
    }

    this.requests.push(now);
  }
}

// ================================
// WEBHOOK HANDLER UTILITY
// ================================

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

export class WebhookHandler {
  constructor(private apiSecret: string) {}

  /**
   * Process postback webhook payload
   */
  processPostback(payload: PostbackPayload): {
    isValid: boolean;
    order: PostbackPayload | null;
  } {
    // Validate checksum
    const expectedChecksum = crypto
      .createHash('sha256')
      .update(payload.order_id + payload.order_timestamp + this.apiSecret)
      .digest('hex');

    const isValid = expectedChecksum === payload.checksum;

    return {
      isValid,
      order: isValid ? payload : null
    };
  }

  /**
   * Express.js middleware for handling postbacks
   */
  expressMiddleware() {
    return (req: any, res: any, next: any) => {
      try {
        const result = this.processPostback(req.body);
        req.kitePostback = result;
        next();
      } catch (error) {
        res.status(400).json({ error: 'Invalid postback payload' });
      }
    };
  }
}

// ================================
// BATCH OPERATIONS UTILITY
// ================================

export class BatchOperations {
  constructor(private adapter: KiteConnectAdapter, private rateLimiter?: RateLimiter) {}

  /**
   * Get quotes for large number of instruments (handles rate limits)
   */
  async getBatchQuotes(instruments: string[]): Promise<Record<string, Quote>> {
    const batches: string[][] = [];
    const batchSize = 500; // API limit

    for (let i = 0; i < instruments.length; i += batchSize) {
      batches.push(instruments.slice(i, i + batchSize));
    }

    const results: Record<string, Quote> = {};

    for (const batch of batches) {
      if (this.rateLimiter) {
        await this.rateLimiter.checkLimit();
      }

      const batchResult = await this.adapter.getQuotes(batch);
      Object.assign(results, batchResult);

      // Small delay between batches to be respectful to the API
      if (batches.length > 1) {
        await new Promise(resolve => setTimeout(resolve, 100));
      }
    }

    return results;
  }

  /**
   * Get OHLC for large number of instruments
   */
  async getBatchOHLC(instruments: string[]): Promise<Record<string, any>> {
    const batches: string[][] = [];
    const batchSize = 1000; // API limit

    for (let i = 0; i < instruments.length; i += batchSize) {
      batches.push(instruments.slice(i, i + batchSize));
    }

    const results: Record<string, any> = {};

    for (const batch of batches) {
      if (this.rateLimiter) {
        await this.rateLimiter.checkLimit();
      }

      const batchResult = await this.adapter.getOHLC(batch);
      Object.assign(results, batchResult);

      if (batches.length > 1) {
        await new Promise(resolve => setTimeout(resolve, 100));
      }
    }

    return results;
  }

  /**
   * Place multiple orders with rate limiting
   */
  async placeMultipleOrders(orders: OrderParams[]): Promise<OrderResponse[]> {
    const results: OrderResponse[] = [];

    for (const order of orders) {
      if (this.rateLimiter) {
        await this.rateLimiter.checkLimit();
      }

      try {
        const result = await this.adapter.placeOrder(order);
        results.push(result);
      } catch (error) {
        // You might want to handle errors differently based on your use case
        console.error(`Failed to place order for ${order.tradingsymbol}:`, error);
        throw error;
      }

      // Small delay between orders
      await new Promise(resolve => setTimeout(resolve, 50));
    }

    return results;
  }
}

// ================================
// SESSION MANAGER UTILITY
// ================================

export interface SessionStorage {
  save(key: string, data: any): Promise<void>;
  load(key: string): Promise<any>;
  remove(key: string): Promise<void>;
}

export class FileSessionStorage implements SessionStorage {
  constructor(private basePath: string = './sessions') {
    const fs = require('fs');
    if (!fs.existsSync(this.basePath)) {
      fs.mkdirSync(this.basePath, { recursive: true });
    }
  }

  async save(key: string, data: any): Promise<void> {
    const fs = require('fs').promises;
    const path = require('path');
    const filePath = path.join(this.basePath, `${key}.json`);
    await fs.writeFile(filePath, JSON.stringify(data, null, 2));
  }

  async load(key: string): Promise<any> {
    const fs = require('fs').promises;
    const path = require('path');
    const filePath = path.join(this.basePath, `${key}.json`);

    try {
      const data = await fs.readFile(filePath, 'utf8');
      return JSON.parse(data);
    } catch (error) {
      return null;
    }
  }

  async remove(key: string): Promise<void> {
    const fs = require('fs').promises;
    const path = require('path');
    const filePath = path.join(this.basePath, `${key}.json`);

    try {
      await fs.unlink(filePath);
    } catch (error) {
      // File doesn't exist, ignore
    }
  }
}

export class SessionManager {
  constructor(
    private adapter: KiteConnectAdapter,
    private storage: SessionStorage,
    private sessionKey: string = 'kite_session'
  ) {}

  /**
   * Save current session
   */
  async saveSession(): Promise<void> {
    const sessionData = {
      accessToken: this.adapter.getAccessToken(),
      timestamp: Date.now()
    };
    await this.storage.save(this.sessionKey, sessionData);
  }

  /**
   * Load saved session
   */
  async loadSession(): Promise<boolean> {
    const sessionData = await this.storage.load(this.sessionKey);

    if (!sessionData || !sessionData.accessToken) {
      return false;
    }

    // Check if session is less than 24 hours old (Kite tokens expire daily)
    const sessionAge = Date.now() - sessionData.timestamp;
    const maxAge = 24 * 60 * 60 * 1000; // 24 hours

    if (sessionAge > maxAge) {
      await this.clearSession();
      return false;
    }

    this.adapter.setAccessToken(sessionData.accessToken);
    return true;
  }

  /**
   * Clear saved session
   */
  async clearSession(): Promise<void> {
    await this.storage.remove(this.sessionKey);
  }

  /**
   * Generate login URL and handle the full auth flow
   */
  async initiateAuth(redirectUrl?: string): Promise<string> {
    await this.clearSession();
    return this.adapter.generateLoginUrl(redirectUrl);
  }

  /**
   * Complete authentication with request token
   */
  async completeAuth(requestToken: string): Promise<SessionData> {
    const sessionData = await this.adapter.generateSession(requestToken);
    await this.saveSession();
    return sessionData;
  }
}

// ================================
// PORTFOLIO TRACKER UTILITY
// ================================

export class PortfolioTracker {
  constructor(private adapter: KiteConnectAdapter) {}

  /**
   * Get complete portfolio summary
   */
  async getPortfolioSummary(): Promise<{
    holdings: Holding[];
    positions: { net: Position[]; day: Position[] };
    totalPnL: number;
    totalValue: number;
    dayChange: number;
  }> {
    const [holdings, positions] = await Promise.all([
      this.adapter.getHoldings(),
      this.adapter.getPositions()
    ]);

    const totalPnL = holdings.reduce((sum, holding) => sum + holding.pnl, 0) +
                     positions.net.reduce((sum, pos) => sum + pos.pnl, 0);

    const totalValue = holdings.reduce((sum, holding) =>
      sum + (holding.last_price * holding.quantity), 0
    );

    const dayChange = holdings.reduce((sum, holding) => sum + holding.day_change, 0);

    return {
      holdings,
      positions,
      totalPnL,
      totalValue,
      dayChange
    };
  }

  /**
   * Calculate portfolio risk metrics
   */
  async getRiskMetrics(): Promise<{
    concentration: Array<{ symbol: string; percentage: number }>;
    sectorExposure: Record<string, number>;
    largestPosition: number;
  }> {
    const holdings = await this.adapter.getHoldings();
    const totalValue = holdings.reduce((sum, holding) =>
      sum + (holding.last_price * holding.quantity), 0
    );

    const concentration = holdings
      .map(holding => ({
        symbol: holding.tradingsymbol,
        percentage: ((holding.last_price * holding.quantity) / totalValue) * 100
      }))
      .sort((a, b) => b.percentage - a.percentage);

    const largestPosition = Math.max(...concentration.map(c => c.percentage));

    // Note: Sector classification would require additional data mapping
    const sectorExposure: Record<string, number> = {};

    return {
      concentration,
      sectorExposure,
      largestPosition
    };
  }
}

// ================================
// ERROR CLASSES
// ================================

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

// ================================
// EXPORT ALL
// ================================

export default KiteConnectAdapter;