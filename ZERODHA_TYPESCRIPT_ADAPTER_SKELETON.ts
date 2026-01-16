/**
 * Zerodha Broker TypeScript Adapter - Skeleton Implementation
 * Based on OpenAlgo Zerodha Broker Plugin
 *
 * This file provides a complete TypeScript adapter for integrating with
 * Zerodha's Kite Connect v3 API. Use this as a reference for your
 * Fincept Terminal implementation.
 */

import axios, { AxiosInstance, AxiosError } from 'axios';
import crypto from 'crypto';

// ============================================================================
// TYPES & INTERFACES
// ============================================================================

interface ZerodhaConfig {
  apiKey: string;
  apiSecret: string;
  apiBaseUrl: string;
  requestTimeout: number;
  maxRetries: number;
  rateLimitDelay: number;
}

// Authentication Types
interface AuthenticationRequest {
  apiKey: string;
  requestToken: string;
  checksum: string;
}

interface AuthenticationResponse {
  status: 'success' | 'error';
  data?: {
    access_token: string;
    user_id: string;
    user_name: string;
    email: string;
    user_type: 'individual' | 'broker';
    broker: string;
    exchanges: string[];
    products: string[];
    order_types: string[];
    public_token: string;
    login_time: string;
  };
  error_type?: string;
  message?: string;
}

// Order Types
enum OrderType {
  MARKET = 'MARKET',
  LIMIT = 'LIMIT',
  SL = 'SL',
  SL_M = 'SL-M',
}

enum TransactionType {
  BUY = 'BUY',
  SELL = 'SELL',
}

enum Product {
  CNC = 'CNC',
  NRML = 'NRML',
  MIS = 'MIS',
}

enum Exchange {
  NSE = 'NSE',
  BSE = 'BSE',
  NFO = 'NFO',
  CDS = 'CDS',
  MCX = 'MCX',
  BCD = 'BCD',
}

enum OrderStatus {
  OPEN = 'OPEN',
  COMPLETE = 'COMPLETE',
  REJECTED = 'REJECTED',
  CANCELLED = 'CANCELLED',
  TRIGGER_PENDING = 'TRIGGER PENDING',
}

interface PlaceOrderRequest {
  tradingsymbol: string;
  exchange: string;
  transaction_type: TransactionType;
  order_type: OrderType;
  quantity: number | string;
  product: Product;
  price?: number | string;
  trigger_price?: number | string;
  disclosed_quantity?: number | string;
  validity?: 'DAY' | 'IOC';
  tag?: string;
}

interface Order {
  order_id: string;
  exchange: string;
  tradingsymbol: string;
  transaction_type: TransactionType;
  order_type: OrderType;
  quantity: number;
  price: number;
  product: Product;
  status: OrderStatus;
  filled_quantity: number;
  pending_quantity: number;
  average_price: number;
  order_timestamp: string;
  trigger_price: number;
}

interface Trade {
  trade_id: string;
  order_id: string;
  exchange: string;
  tradingsymbol: string;
  transaction_type: TransactionType;
  quantity: number;
  average_price: number;
  product: Product;
  order_timestamp: string;
  filled_quantity: number;
}

interface Position {
  exchange: string;
  tradingsymbol: string;
  quantity: number;
  product: Product;
  average_price: number;
  last_price: number;
  pnl: number;
  m2m: number;
  unrealised: number;
  realised: number;
}

interface Holding {
  exchange: string;
  tradingsymbol: string;
  quantity: number;
  product: Product;
  average_price: number;
  last_price: number;
  pnl: number;
  pnl_percent: number;
}

interface Quote {
  instrument_token: number;
  exchange_token: number;
  tradingsymbol: string;
  last_price: number;
  last_quantity: number;
  bid: number;
  ask: number;
  volume: number;
  average_price: number;
  oi: number;
  net_change: number;
  net_change_pct: number;
  ohlc: {
    open: number;
    high: number;
    low: number;
    close: number;
  };
  depth: {
    buy: MarketLevel[];
    sell: MarketLevel[];
  };
}

interface MarketLevel {
  quantity: number;
  price: number;
  orders: number;
}

interface Candle {
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  oi: number;
}

interface MarginData {
  availablecash: string;
  collateral: string;
  m2munrealized: string;
  m2mrealized: string;
  utiliseddebits: string;
}

interface MarginPosition {
  exchange: string;
  tradingsymbol: string;
  transaction_type: TransactionType;
  variety: string;
  product: Product;
  order_type: OrderType;
  quantity: number;
  price: number;
  trigger_price: number;
}

interface MarginResponse {
  status: 'success' | 'error';
  data?: {
    total_margin_required: number;
    span_margin: number;
    exposure_margin: number;
  };
  message?: string;
}

// ============================================================================
// CUSTOM ERROR CLASSES
// ============================================================================

class ZerodhaAPIError extends Error {
  constructor(
    message: string,
    public errorType?: string,
    public statusCode?: number
  ) {
    super(message);
    this.name = 'ZerodhaAPIError';
  }
}

class ZerodhaPermissionError extends ZerodhaAPIError {
  constructor(message: string) {
    super(message, 'PermissionException');
    this.name = 'ZerodhaPermissionError';
  }
}

class ZerodhaAuthenticationError extends ZerodhaAPIError {
  constructor(message: string) {
    super(message, 'InvalidCredentials');
    this.name = 'ZerodhaAuthenticationError';
  }
}

// ============================================================================
// ZERODHA ADAPTER CLASS
// ============================================================================

export class ZerodhaAdapter {
  private config: ZerodhaConfig;
  private client: AxiosInstance;
  private authToken: string | null = null;

  constructor(config: Partial<ZerodhaConfig> = {}) {
    this.config = {
      apiKey: process.env.BROKER_API_KEY || '',
      apiSecret: process.env.BROKER_API_SECRET || '',
      apiBaseUrl: 'https://api.kite.trade',
      requestTimeout: 30000,
      maxRetries: 3,
      rateLimitDelay: 1000,
      ...config,
    };

    this.client = axios.create({
      baseURL: this.config.apiBaseUrl,
      timeout: this.config.requestTimeout,
      headers: {
        'X-Kite-Version': '3',
      },
      httpAgent: { keepAlive: true },
      httpsAgent: { keepAlive: true },
    });
  }

  // ========================================================================
  // AUTHENTICATION
  // ========================================================================

  /**
   * Generate checksum for authentication
   */
  private generateChecksum(requestToken: string): string {
    const checksumInput = `${this.config.apiKey}${requestToken}${this.config.apiSecret}`;
    return crypto.createHash('sha256').update(checksumInput).digest('hex');
  }

  /**
   * Authenticate and get access token
   */
  async authenticate(requestToken: string): Promise<AuthenticationResponse> {
    try {
      const checksum = this.generateChecksum(requestToken);

      const response = await this.client.post<AuthenticationResponse>(
        '/session/token',
        {
          api_key: this.config.apiKey,
          request_token: requestToken,
          checksum,
        }
      );

      if (response.data.status === 'success' && response.data.data?.access_token) {
        this.authToken = response.data.data.access_token;
        this.setAuthHeader(this.authToken);
      }

      return response.data;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Set authentication header for subsequent requests
   */
  private setAuthHeader(token: string): void {
    this.client.defaults.headers.common['Authorization'] = `token ${token}`;
  }

  /**
   * Set auth token directly (useful if token is stored)
   */
  public setAuthToken(token: string): void {
    this.authToken = token;
    this.setAuthHeader(token);
  }

  // ========================================================================
  // ORDER MANAGEMENT
  // ========================================================================

  /**
   * Place a regular order
   */
  async placeOrder(order: PlaceOrderRequest): Promise<{ order_id: string }> {
    try {
      const params = new URLSearchParams({
        tradingsymbol: order.tradingsymbol,
        exchange: order.exchange,
        transaction_type: order.transaction_type,
        order_type: order.order_type,
        quantity: String(order.quantity),
        product: order.product,
        price: String(order.price || 0),
        trigger_price: String(order.trigger_price || 0),
        disclosed_quantity: String(order.disclosed_quantity || 0),
        validity: order.validity || 'DAY',
        tag: order.tag || 'openalgo',
      });

      const response = await this.client.post(
        '/orders/regular',
        params,
        {
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        }
      );

      if (response.data.status === 'success') {
        return response.data.data;
      }

      throw new ZerodhaAPIError(response.data.message || 'Failed to place order');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Modify an existing order
   */
  async modifyOrder(
    orderId: string,
    updates: {
      order_type?: OrderType;
      quantity?: number;
      price?: number;
      trigger_price?: number;
      disclosed_quantity?: number;
      validity?: 'DAY' | 'IOC';
    }
  ): Promise<{ order_id: string }> {
    try {
      const params = new URLSearchParams();
      if (updates.order_type) params.append('order_type', updates.order_type);
      if (updates.quantity) params.append('quantity', String(updates.quantity));
      if (updates.price) params.append('price', String(updates.price));
      if (updates.trigger_price) params.append('trigger_price', String(updates.trigger_price));
      if (updates.disclosed_quantity) params.append('disclosed_quantity', String(updates.disclosed_quantity));
      if (updates.validity) params.append('validity', updates.validity);

      const response = await this.client.put(
        `/orders/regular/${orderId}`,
        params,
        {
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        }
      );

      if (response.data.status === 'success' || response.data.message === 'SUCCESS') {
        return response.data.data;
      }

      throw new ZerodhaAPIError(response.data.message || 'Failed to modify order');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Cancel an order
   */
  async cancelOrder(orderId: string): Promise<{ order_id: string }> {
    try {
      const response = await this.client.delete(`/orders/regular/${orderId}`);

      if (response.data.status === 'success' || response.data.status === true) {
        return response.data.data || { order_id: orderId };
      }

      throw new ZerodhaAPIError(response.data.message || 'Failed to cancel order');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get order book
   */
  async getOrderBook(): Promise<Order[]> {
    try {
      const response = await this.client.get('/orders');
      return response.data.data || [];
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get trade book
   */
  async getTradeBook(): Promise<Trade[]> {
    try {
      const response = await this.client.get('/trades');
      return response.data.data || [];
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Cancel all open orders
   */
  async cancelAllOrders(): Promise<{ cancelled: string[]; failed: string[] }> {
    try {
      const orders = await this.getOrderBook();
      const openOrders = orders.filter(o =>
        o.status === OrderStatus.OPEN || o.status === OrderStatus.TRIGGER_PENDING
      );

      const cancelled: string[] = [];
      const failed: string[] = [];

      for (const order of openOrders) {
        try {
          await this.cancelOrder(order.order_id);
          cancelled.push(order.order_id);
        } catch {
          failed.push(order.order_id);
        }
      }

      return { cancelled, failed };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ========================================================================
  // PORTFOLIO MANAGEMENT
  // ========================================================================

  /**
   * Get positions (intraday + day)
   */
  async getPositions(): Promise<{ net: Position[]; day: Position[] }> {
    try {
      const response = await this.client.get('/portfolio/positions');
      return response.data.data || { net: [], day: [] };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get holdings (deliveries)
   */
  async getHoldings(): Promise<Holding[]> {
    try {
      const response = await this.client.get('/portfolio/holdings');
      return response.data.data || [];
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Close all open positions
   */
  async closeAllPositions(): Promise<{ status: string; message: string }> {
    try {
      const positions = await this.getPositions();
      const netPositions = positions.net.filter(p => p.quantity !== 0);

      for (const position of netPositions) {
        const order: PlaceOrderRequest = {
          tradingsymbol: position.tradingsymbol,
          exchange: position.exchange,
          transaction_type: position.quantity > 0 ? TransactionType.SELL : TransactionType.BUY,
          order_type: OrderType.MARKET,
          quantity: Math.abs(position.quantity),
          product: position.product,
          price: 0,
          trigger_price: 0,
        };

        await this.placeOrder(order);
      }

      return { status: 'success', message: 'All positions closed' };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ========================================================================
  // MARKET DATA
  // ========================================================================

  /**
   * Get real-time quote for a single symbol
   */
  async getQuote(exchange: string, symbol: string): Promise<Quote> {
    try {
      const instrument = `${exchange}:${symbol}`;
      const response = await this.client.get(`/quote?i=${encodeURIComponent(instrument)}`);

      const quote = response.data.data[instrument];
      if (!quote) {
        throw new ZerodhaAPIError('No quote data found');
      }

      return quote;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get quotes for multiple symbols with batching
   */
  async getMultipleQuotes(
    symbols: Array<{ exchange: string; symbol: string }>
  ): Promise<Quote[]> {
    try {
      const BATCH_SIZE = 500;
      const results: Quote[] = [];

      for (let i = 0; i < symbols.length; i += BATCH_SIZE) {
        const batch = symbols.slice(i, i + BATCH_SIZE);

        const queryParams = batch
          .map(s => `i=${encodeURIComponent(`${s.exchange}:${s.symbol}`)}`)
          .join('&');

        const response = await this.client.get(`/quote?${queryParams}`);

        if (response.data.data) {
          Object.values(response.data.data).forEach(quote => {
            results.push(quote as Quote);
          });
        }

        // Rate limiting between batches
        if (i + BATCH_SIZE < symbols.length) {
          await this.sleep(this.config.rateLimitDelay);
        }
      }

      return results;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get historical candle data
   */
  async getHistoricalData(
    instrumentToken: string,
    resolution: string,
    fromDate: string,
    toDate: string
  ): Promise<Candle[]> {
    try {
      const endpoint =
        `/instruments/historical/${instrumentToken}/${resolution}` +
        `?from=${encodeURIComponent(fromDate)}&to=${encodeURIComponent(toDate)}&oi=1`;

      const response = await this.client.get(endpoint);

      const candles = response.data.data?.candles || [];

      return candles.map((candle: any[]) => ({
        timestamp: new Date(candle[0]).getTime() / 1000,
        open: candle[1],
        high: candle[2],
        low: candle[3],
        close: candle[4],
        volume: candle[5],
        oi: candle[6],
      }));
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get market depth
   */
  async getMarketDepth(exchange: string, symbol: string): Promise<Quote> {
    try {
      return await this.getQuote(exchange, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ========================================================================
  // MARGINS & FUNDS
  // ========================================================================

  /**
   * Get margin data
   */
  async getMarginData(): Promise<MarginData> {
    try {
      const response = await this.client.get('/user/margins');

      const data = response.data.data;
      if (!data) {
        throw new ZerodhaAPIError('Failed to fetch margin data');
      }

      const equity = data.equity;
      const commodity = data.commodity;

      return {
        availablecash: (equity.net + commodity.net).toFixed(2),
        collateral: (equity.available.collateral + commodity.available.collateral).toFixed(2),
        m2munrealized: (
          equity.utilised.m2m_unrealised +
          commodity.utilised.m2m_unrealised
        ).toFixed(2),
        m2mrealized: (
          equity.utilised.m2m_realised +
          commodity.utilised.m2m_realised
        ).toFixed(2),
        utiliseddebits: (
          equity.utilised.debits +
          commodity.utilised.debits
        ).toFixed(2),
      };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Calculate margin requirement for positions
   */
  async calculateMargin(positions: MarginPosition[]): Promise<MarginResponse> {
    try {
      if (positions.length === 0) {
        throw new ZerodhaAPIError('No positions to calculate margin');
      }

      const endpoint =
        positions.length > 1
          ? '/margins/basket?consider_positions=true'
          : '/margins/orders';

      const response = await this.client.post(endpoint, positions, {
        headers: { 'Content-Type': 'application/json' },
      });

      if (response.data.status !== 'success') {
        throw new ZerodhaAPIError(response.data.message || 'Failed to calculate margin');
      }

      const data = response.data.data;

      return {
        status: 'success',
        data: {
          total_margin_required: data.initial?.total || 0,
          span_margin: data.final?.span || 0,
          exposure_margin: data.final?.exposure || 0,
        },
      };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ========================================================================
  // UTILITY METHODS
  // ========================================================================

  /**
   * Handle API errors
   */
  private handleError(error: any): Error {
    if (error instanceof ZerodhaAPIError) {
      return error;
    }

    if (axios.isAxiosError(error)) {
      const status = error.response?.status;
      const data = error.response?.data;

      if (status === 401 || data?.error_type === 'InvalidCredentials') {
        return new ZerodhaAuthenticationError(
          data?.message || 'Authentication failed'
        );
      }

      if (data?.error_type === 'PermissionException') {
        return new ZerodhaPermissionError(data?.message || 'Permission denied');
      }

      return new ZerodhaAPIError(
        data?.message || error.message,
        data?.error_type,
        status
      );
    }

    return new ZerodhaAPIError(
      error instanceof Error ? error.message : 'Unknown error'
    );
  }

  /**
   * Sleep utility
   */
  private sleep(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Get API base URL
   */
  getBaseURL(): string {
    return this.config.apiBaseUrl;
  }

  /**
   * Check if authenticated
   */
  isAuthenticated(): boolean {
    return this.authToken !== null;
  }
}

// ============================================================================
// HELPER FUNCTIONS FOR DATA TRANSFORMATION
// ============================================================================

export class OrderTransformer {
  /**
   * Transform OpenAlgo order to Zerodha format
   */
  static toZerodha(openalgoOrder: any): PlaceOrderRequest {
    return {
      tradingsymbol: openalgoOrder.symbol,
      exchange: openalgoOrder.exchange,
      transaction_type: openalgoOrder.action as TransactionType,
      order_type: openalgoOrder.pricetype as OrderType,
      quantity: openalgoOrder.quantity,
      product: openalgoOrder.product as Product,
      price: openalgoOrder.price || 0,
      trigger_price: openalgoOrder.trigger_price || 0,
      disclosed_quantity: openalgoOrder.disclosed_quantity || 0,
      validity: 'DAY',
      tag: openalgoOrder.tag || 'openalgo',
    };
  }

  /**
   * Transform Zerodha order to OpenAlgo format
   */
  static toOpenAlgo(zerodhaOrder: Order): any {
    const statusMap: Record<OrderStatus, string> = {
      [OrderStatus.OPEN]: 'open',
      [OrderStatus.COMPLETE]: 'complete',
      [OrderStatus.REJECTED]: 'rejected',
      [OrderStatus.CANCELLED]: 'cancelled',
      [OrderStatus.TRIGGER_PENDING]: 'trigger pending',
    };

    return {
      symbol: zerodhaOrder.tradingsymbol,
      exchange: zerodhaOrder.exchange,
      action: zerodhaOrder.transaction_type,
      quantity: zerodhaOrder.quantity,
      price: zerodhaOrder.price,
      trigger_price: zerodhaOrder.trigger_price,
      pricetype: zerodhaOrder.order_type,
      product: zerodhaOrder.product,
      orderid: zerodhaOrder.order_id,
      order_status: statusMap[zerodhaOrder.status] || 'open',
      timestamp: zerodhaOrder.order_timestamp,
    };
  }
}

export class PositionTransformer {
  /**
   * Transform Zerodha position to OpenAlgo format
   */
  static toOpenAlgo(zerodhaPosition: Position): any {
    return {
      symbol: zerodhaPosition.tradingsymbol,
      exchange: zerodhaPosition.exchange,
      product: zerodhaPosition.product,
      quantity: String(zerodhaPosition.quantity),
      pnl: Number(zerodhaPosition.pnl.toFixed(2)),
      average_price: zerodhaPosition.average_price.toFixed(2),
      ltp: Number(zerodhaPosition.last_price.toFixed(2)),
    };
  }
}

// ============================================================================
// EXPORT
// ============================================================================

export default ZerodhaAdapter;
