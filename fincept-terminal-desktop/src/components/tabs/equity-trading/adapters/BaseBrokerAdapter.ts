/**
 * Base Broker Adapter Interface
 * All broker implementations must extend this abstract class
 * Ensures consistent interface across all brokers
 */

import {
  BrokerType,
  BrokerStatus,
  UnifiedOrder,
  OrderRequest,
  OrderResponse,
  UnifiedPosition,
  UnifiedHolding,
  UnifiedQuote,
  UnifiedMarketDepth,
  Candle,
  TimeInterval,
  UnifiedFunds,
  UnifiedProfile,
  BrokerCredentials,
  AuthStatus,
} from '../core/types';

export abstract class BaseBrokerAdapter {
  protected brokerId: BrokerType;
  protected status: BrokerStatus = BrokerStatus.DISCONNECTED;
  protected credentials?: BrokerCredentials;

  constructor(brokerId: BrokerType) {
    this.brokerId = brokerId;
  }

  // ==================== BROKER CAPABILITIES ====================

  /**
   * Get list of exchanges supported by this broker
   * Must be implemented by each broker adapter
   */
  abstract getSupportedExchanges(): string[];

  /**
   * Check if broker supports a specific exchange
   */
  supportsExchange(exchange: string): boolean {
    const supported = this.getSupportedExchanges();
    return supported.includes(exchange.toUpperCase());
  }

  // ==================== AUTHENTICATION ====================

  /**
   * Initialize broker connection with credentials
   */
  abstract initialize(credentials: BrokerCredentials): Promise<boolean>;

  /**
   * Authenticate and get access token
   * Should handle background token refresh automatically
   */
  abstract authenticate(): Promise<AuthStatus>;

  /**
   * Check if currently authenticated
   */
  abstract isAuthenticated(): boolean;

  /**
   * Get current authentication status
   */
  abstract getAuthStatus(): AuthStatus;

  /**
   * Refresh access token (called automatically by background service)
   */
  abstract refreshToken(): Promise<boolean>;

  /**
   * Disconnect and cleanup
   */
  abstract disconnect(): Promise<void>;

  // ==================== TRADING OPERATIONS ====================

  /**
   * Place a new order
   */
  abstract placeOrder(order: OrderRequest): Promise<OrderResponse>;

  /**
   * Modify an existing order
   */
  abstract modifyOrder(orderId: string, updates: Partial<OrderRequest>): Promise<OrderResponse>;

  /**
   * Cancel an order
   */
  abstract cancelOrder(orderId: string): Promise<OrderResponse>;

  /**
   * Get all orders for the day
   */
  abstract getOrders(): Promise<UnifiedOrder[]>;

  /**
   * Get specific order by ID
   */
  abstract getOrder(orderId: string): Promise<UnifiedOrder>;

  /**
   * Get order history
   */
  abstract getOrderHistory(orderId: string): Promise<UnifiedOrder[]>;

  // ==================== POSITIONS & HOLDINGS ====================

  /**
   * Get all open positions
   */
  abstract getPositions(): Promise<UnifiedPosition[]>;

  /**
   * Get all holdings
   */
  abstract getHoldings(): Promise<UnifiedHolding[]>;

  /**
   * Get account funds/margin info
   */
  abstract getFunds(): Promise<UnifiedFunds>;

  /**
   * Get user profile
   */
  abstract getProfile(): Promise<UnifiedProfile>;

  // ==================== MARKET DATA ====================

  /**
   * Get quote for single symbol
   */
  abstract getQuote(symbol: string, exchange: string): Promise<UnifiedQuote>;

  /**
   * Get quotes for multiple symbols
   */
  abstract getQuotes(symbols: Array<{ symbol: string; exchange: string }>): Promise<UnifiedQuote[]>;

  /**
   * Get market depth (order book)
   */
  abstract getMarketDepth(symbol: string, exchange: string): Promise<UnifiedMarketDepth>;

  /**
   * Get historical candles
   */
  abstract getHistoricalData(
    symbol: string,
    exchange: string,
    interval: TimeInterval,
    from: Date,
    to: Date
  ): Promise<Candle[]>;

  // ==================== WEBSOCKET OPERATIONS ====================

  /**
   * Connect to real-time data stream
   */
  abstract connectWebSocket(): Promise<void>;

  /**
   * Subscribe to order updates
   */
  abstract subscribeOrders(callback: (order: UnifiedOrder) => void): void;

  /**
   * Subscribe to position updates
   */
  abstract subscribePositions(callback: (position: UnifiedPosition) => void): void;

  /**
   * Subscribe to quote updates
   */
  abstract subscribeQuotes(
    symbols: Array<{ symbol: string; exchange: string }>,
    callback: (quote: UnifiedQuote) => void
  ): void;

  /**
   * Subscribe to market depth updates
   */
  abstract subscribeMarketDepth(
    symbol: string,
    exchange: string,
    callback: (depth: UnifiedMarketDepth) => void
  ): void;

  /**
   * Unsubscribe from quotes
   */
  abstract unsubscribeQuotes(symbols: Array<{ symbol: string; exchange: string }>): void;

  /**
   * Disconnect WebSocket
   */
  abstract disconnectWebSocket(): Promise<void>;

  // ==================== UTILITY METHODS ====================

  /**
   * Get broker ID
   */
  getBrokerId(): BrokerType {
    return this.brokerId;
  }

  /**
   * Get current status
   */
  getStatus(): BrokerStatus {
    return this.status;
  }

  /**
   * Set status
   */
  protected setStatus(status: BrokerStatus): void {
    this.status = status;
  }

  /**
   * Calculate latency (override for specific implementations)
   */
  async measureLatency(): Promise<number> {
    const start = Date.now();
    try {
      await this.getProfile();
      return Date.now() - start;
    } catch {
      return -1;
    }
  }

  // ==================== MARGIN CALCULATION ====================

  /**
   * Calculate margin required for an order
   */
  abstract calculateMargin(order: OrderRequest): Promise<{
    required: number;
    available: number;
    leverage?: number;
  }>;

  // ==================== SYMBOL SEARCH ====================

  /**
   * Search for symbols/instruments
   */
  abstract searchSymbols(query: string, exchange?: string): Promise<Array<{
    symbol: string;
    exchange: string;
    name: string;
    instrumentType?: string;
  }>>;
}
