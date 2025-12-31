/**
 * WebSocket Manager
 * Manages real-time data streams from all brokers
 */

import { BrokerType, UnifiedQuote, UnifiedOrder, UnifiedPosition, WebSocketEvent, WebSocketMessage } from '../core/types';
import { BaseBrokerAdapter } from '../adapters/BaseBrokerAdapter';

type WebSocketCallback<T = any> = (message: WebSocketMessage<T>) => void;

interface Subscription {
  brokerId: BrokerType;
  symbols: Array<{ symbol: string; exchange: string }>;
  callbacks: Set<WebSocketCallback>;
}

export class WebSocketManager {
  private static instance: WebSocketManager;
  private adapters: Map<BrokerType, BaseBrokerAdapter> = new Map();
  private subscriptions: Map<string, Subscription> = new Map();
  private globalCallbacks: Map<WebSocketEvent, Set<WebSocketCallback>> = new Map();
  private isConnected: Map<BrokerType, boolean> = new Map();

  private constructor() {
    // Initialize callback maps
    const events: WebSocketEvent[] = [
      'order_update',
      'trade_update',
      'position_update',
      'quote_update',
      'depth_update',
      'connection_status',
    ];
    events.forEach(event => this.globalCallbacks.set(event, new Set()));
  }

  static getInstance(): WebSocketManager {
    if (!WebSocketManager.instance) {
      WebSocketManager.instance = new WebSocketManager();
    }
    return WebSocketManager.instance;
  }

  /**
   * Register broker adapter
   */
  registerAdapter(brokerId: BrokerType, adapter: BaseBrokerAdapter): void {
    this.adapters.set(brokerId, adapter);
    this.isConnected.set(brokerId, false);
    console.log(`[WebSocketManager] Registered adapter: ${brokerId}`);
  }

  /**
   * Connect to broker WebSocket
   */
  async connect(brokerId: BrokerType): Promise<void> {
    const adapter = this.adapters.get(brokerId);
    if (!adapter) {
      throw new Error(`Adapter not found for ${brokerId}`);
    }

    if (this.isConnected.get(brokerId)) {
      console.log(`[WebSocketManager] ${brokerId} already connected`);
      return;
    }

    try {
      await adapter.connectWebSocket();
      this.isConnected.set(brokerId, true);

      // Subscribe to order updates
      adapter.subscribeOrders((order) => {
        this.broadcast('order_update', brokerId, order);
      });

      // Subscribe to position updates
      adapter.subscribePositions((position) => {
        this.broadcast('position_update', brokerId, position);
      });

      this.broadcast('connection_status', brokerId, { connected: true });
      console.log(`[WebSocketManager] ${brokerId} connected`);
    } catch (error) {
      console.error(`[WebSocketManager] ${brokerId} connection failed:`, error);
      this.broadcast('connection_status', brokerId, { connected: false, error });
      throw error;
    }
  }

  /**
   * Disconnect from broker WebSocket
   */
  async disconnect(brokerId: BrokerType): Promise<void> {
    const adapter = this.adapters.get(brokerId);
    if (!adapter) return;

    try {
      await adapter.disconnectWebSocket();
      this.isConnected.set(brokerId, false);
      this.broadcast('connection_status', brokerId, { connected: false });
      console.log(`[WebSocketManager] ${brokerId} disconnected`);
    } catch (error) {
      console.error(`[WebSocketManager] ${brokerId} disconnect failed:`, error);
    }
  }

  /**
   * Connect all registered brokers
   */
  async connectAll(): Promise<void> {
    const promises = Array.from(this.adapters.keys()).map(brokerId =>
      this.connect(brokerId).catch(err => {
        console.error(`[WebSocketManager] Failed to connect ${brokerId}:`, err);
      })
    );
    await Promise.all(promises);
  }

  /**
   * Disconnect all brokers
   */
  async disconnectAll(): Promise<void> {
    const promises = Array.from(this.adapters.keys()).map(brokerId =>
      this.disconnect(brokerId)
    );
    await Promise.all(promises);
  }

  /**
   * Subscribe to quote updates
   */
  subscribeQuotes(
    brokerId: BrokerType,
    symbols: Array<{ symbol: string; exchange: string }>,
    callback: (quote: UnifiedQuote) => void
  ): string {
    const adapter = this.adapters.get(brokerId);
    if (!adapter) {
      throw new Error(`Adapter not found for ${brokerId}`);
    }

    const subscriptionId = `${brokerId}_${symbols.map(s => `${s.exchange}:${s.symbol}`).join('_')}`;

    // Subscribe via adapter
    adapter.subscribeQuotes(symbols, (quote) => {
      this.broadcast('quote_update', brokerId, quote);
      callback(quote);
    });

    // Store subscription
    const subscription: Subscription = {
      brokerId,
      symbols,
      callbacks: new Set([callback as any]),
    };
    this.subscriptions.set(subscriptionId, subscription);

    console.log(`[WebSocketManager] Subscribed to quotes: ${subscriptionId}`);
    return subscriptionId;
  }

  /**
   * Unsubscribe from quotes
   */
  unsubscribeQuotes(subscriptionId: string): void {
    const subscription = this.subscriptions.get(subscriptionId);
    if (!subscription) return;

    const adapter = this.adapters.get(subscription.brokerId);
    if (adapter) {
      adapter.unsubscribeQuotes(subscription.symbols);
    }

    this.subscriptions.delete(subscriptionId);
    console.log(`[WebSocketManager] Unsubscribed: ${subscriptionId}`);
  }

  /**
   * Subscribe to market depth updates
   */
  subscribeMarketDepth(
    brokerId: BrokerType,
    symbol: string,
    exchange: string,
    callback: WebSocketCallback
  ): string {
    const adapter = this.adapters.get(brokerId);
    if (!adapter) {
      throw new Error(`Adapter not found for ${brokerId}`);
    }

    const subscriptionId = `${brokerId}_depth_${exchange}:${symbol}`;

    adapter.subscribeMarketDepth(symbol, exchange, (depth) => {
      this.broadcast('depth_update', brokerId, depth);
      callback({
        event: 'depth_update',
        brokerId,
        data: depth,
        timestamp: new Date(),
      });
    });

    console.log(`[WebSocketManager] Subscribed to market depth: ${subscriptionId}`);
    return subscriptionId;
  }

  /**
   * Subscribe to global events (across all brokers)
   */
  on(event: WebSocketEvent, callback: WebSocketCallback): () => void {
    const callbacks = this.globalCallbacks.get(event);
    if (callbacks) {
      callbacks.add(callback);
    }

    // Return unsubscribe function
    return () => {
      callbacks?.delete(callback);
    };
  }

  /**
   * Broadcast message to all subscribers
   */
  private broadcast(event: WebSocketEvent, brokerId: BrokerType, data: any): void {
    const message: WebSocketMessage = {
      event,
      brokerId,
      data,
      timestamp: new Date(),
    };

    const callbacks = this.globalCallbacks.get(event);
    if (callbacks) {
      callbacks.forEach(callback => {
        try {
          callback(message);
        } catch (error) {
          console.error(`[WebSocketManager] Callback error:`, error);
        }
      });
    }
  }

  /**
   * Check connection status
   */
  isConnectedTo(brokerId: BrokerType): boolean {
    return this.isConnected.get(brokerId) || false;
  }

  /**
   * Get all connection statuses
   */
  getConnectionStatuses(): Map<BrokerType, boolean> {
    return new Map(this.isConnected);
  }

  /**
   * Get active subscriptions count
   */
  getSubscriptionCount(): number {
    return this.subscriptions.size;
  }
}

export const webSocketManager = WebSocketManager.getInstance();
