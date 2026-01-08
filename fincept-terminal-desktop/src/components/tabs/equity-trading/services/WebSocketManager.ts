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
  adapterCallback?: (quote: UnifiedQuote) => void; // Store adapter callback for cleanup
}

// Reconnection configuration
interface ReconnectConfig {
  maxAttempts: number;
  baseDelayMs: number;
  maxDelayMs: number;
}

const DEFAULT_RECONNECT_CONFIG: ReconnectConfig = {
  maxAttempts: 5,
  baseDelayMs: 1000,
  maxDelayMs: 30000,
};

export class WebSocketManager {
  private static instance: WebSocketManager;
  private adapters: Map<BrokerType, BaseBrokerAdapter> = new Map();
  private subscriptions: Map<string, Subscription> = new Map();
  private globalCallbacks: Map<WebSocketEvent, Set<WebSocketCallback>> = new Map();
  private isConnected: Map<BrokerType, boolean> = new Map();
  private isConnecting: Map<BrokerType, boolean> = new Map();
  private reconnectAttempts: Map<BrokerType, number> = new Map();
  private reconnectConfig: ReconnectConfig = DEFAULT_RECONNECT_CONFIG;

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
   * Set reconnection configuration
   */
  setReconnectConfig(config: Partial<ReconnectConfig>): void {
    this.reconnectConfig = { ...this.reconnectConfig, ...config };
  }

  /**
   * Register broker adapter
   */
  registerAdapter(brokerId: BrokerType, adapter: BaseBrokerAdapter): void {
    this.adapters.set(brokerId, adapter);
    this.isConnected.set(brokerId, false);
    this.isConnecting.set(brokerId, false);
    this.reconnectAttempts.set(brokerId, 0);
    console.log(`[WebSocketManager] Registered adapter: ${brokerId}`);
  }

  /**
   * Connect to broker WebSocket with reconnection support
   */
  async connect(brokerId: BrokerType): Promise<void> {
    const adapter = this.adapters.get(brokerId);
    if (!adapter) {
      throw new Error(`Adapter not found for ${brokerId}`);
    }

    // Prevent concurrent connection attempts
    if (this.isConnecting.get(brokerId)) {
      console.log(`[WebSocketManager] ${brokerId} connection already in progress`);
      return;
    }

    if (this.isConnected.get(brokerId)) {
      console.log(`[WebSocketManager] ${brokerId} already connected`);
      return;
    }

    this.isConnecting.set(brokerId, true);

    try {
      await adapter.connectWebSocket();
      this.isConnected.set(brokerId, true);
      this.isConnecting.set(brokerId, false);
      this.reconnectAttempts.set(brokerId, 0); // Reset on successful connection

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
      this.isConnecting.set(brokerId, false);
      this.isConnected.set(brokerId, false);
      console.error(`[WebSocketManager] ${brokerId} connection failed:`, error);
      this.broadcast('connection_status', brokerId, { connected: false, error });
      throw error;
    }
  }

  /**
   * Attempt to reconnect with exponential backoff
   */
  async reconnect(brokerId: BrokerType): Promise<void> {
    const currentAttempt = this.reconnectAttempts.get(brokerId) || 0;

    if (currentAttempt >= this.reconnectConfig.maxAttempts) {
      console.error(`[WebSocketManager] ${brokerId} max reconnection attempts reached`);
      this.broadcast('connection_status', brokerId, {
        connected: false,
        error: `Max reconnection attempts (${this.reconnectConfig.maxAttempts}) reached`
      });
      return;
    }

    // Calculate delay with exponential backoff
    const delay = Math.min(
      this.reconnectConfig.baseDelayMs * Math.pow(2, currentAttempt),
      this.reconnectConfig.maxDelayMs
    );

    console.log(`[WebSocketManager] ${brokerId} reconnecting in ${delay}ms (attempt ${currentAttempt + 1}/${this.reconnectConfig.maxAttempts})`);
    this.broadcast('connection_status', brokerId, {
      connected: false,
      reconnecting: true,
      attempt: currentAttempt + 1,
      maxAttempts: this.reconnectConfig.maxAttempts
    });

    await new Promise(resolve => setTimeout(resolve, delay));

    this.reconnectAttempts.set(brokerId, currentAttempt + 1);

    try {
      // Disconnect first to clean up
      await this.disconnect(brokerId);

      // Reconnect
      await this.connect(brokerId);

      // Restore subscriptions
      await this.restoreSubscriptions(brokerId);

      console.log(`[WebSocketManager] ${brokerId} reconnected successfully`);
    } catch (error) {
      console.error(`[WebSocketManager] ${brokerId} reconnection failed:`, error);
      // Try again
      await this.reconnect(brokerId);
    }
  }

  /**
   * Restore subscriptions after reconnection
   */
  private async restoreSubscriptions(brokerId: BrokerType): Promise<void> {
    const adapter = this.adapters.get(brokerId);
    if (!adapter) return;

    for (const [subscriptionId, subscription] of this.subscriptions.entries()) {
      if (subscription.brokerId === brokerId) {
        console.log(`[WebSocketManager] Restoring subscription: ${subscriptionId}`);

        // Create new callback for adapter
        const adapterCallback = (quote: UnifiedQuote) => {
          this.broadcast('quote_update', brokerId, quote);
          subscription.callbacks.forEach(cb => {
            try {
              cb({ event: 'quote_update', brokerId, data: quote, timestamp: new Date() });
            } catch (err) {
              console.error('[WebSocketManager] Callback error during restore:', err);
            }
          });
        };

        subscription.adapterCallback = adapterCallback;
        adapter.subscribeQuotes(subscription.symbols, adapterCallback);
      }
    }
  }

  /**
   * Disconnect from broker WebSocket
   */
  async disconnect(brokerId: BrokerType): Promise<void> {
    const adapter = this.adapters.get(brokerId);
    if (!adapter) return;

    try {
      // Clean up subscriptions for this broker (but don't delete - needed for reconnection)
      for (const [subscriptionId, subscription] of this.subscriptions.entries()) {
        if (subscription.brokerId === brokerId) {
          // Clear the adapter callback reference (will be recreated on reconnect)
          subscription.adapterCallback = undefined;
        }
      }

      await adapter.disconnectWebSocket();
      this.isConnected.set(brokerId, false);
      this.isConnecting.set(brokerId, false);
      this.broadcast('connection_status', brokerId, { connected: false });
      console.log(`[WebSocketManager] ${brokerId} disconnected`);
    } catch (error) {
      console.error(`[WebSocketManager] ${brokerId} disconnect failed:`, error);
    }
  }

  /**
   * Fully cleanup a broker (disconnect and remove all subscriptions)
   */
  async cleanup(brokerId: BrokerType): Promise<void> {
    // Remove all subscriptions for this broker
    for (const [subscriptionId, subscription] of this.subscriptions.entries()) {
      if (subscription.brokerId === brokerId) {
        subscription.callbacks.clear();
        this.subscriptions.delete(subscriptionId);
      }
    }

    await this.disconnect(brokerId);
    this.reconnectAttempts.set(brokerId, 0);
    console.log(`[WebSocketManager] ${brokerId} fully cleaned up`);
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

    // Check if subscription already exists - just add callback if so
    const existingSubscription = this.subscriptions.get(subscriptionId);
    if (existingSubscription) {
      existingSubscription.callbacks.add(callback as any);
      console.log(`[WebSocketManager] Added callback to existing subscription: ${subscriptionId}`);
      return subscriptionId;
    }

    // Create adapter callback that broadcasts to all registered callbacks
    const adapterCallback = (quote: UnifiedQuote) => {
      this.broadcast('quote_update', brokerId, quote);
      const sub = this.subscriptions.get(subscriptionId);
      if (sub) {
        sub.callbacks.forEach(cb => {
          try {
            cb({ event: 'quote_update', brokerId, data: quote, timestamp: new Date() } as any);
          } catch (err) {
            console.error('[WebSocketManager] Callback error:', err);
          }
        });
      }
    };

    // Subscribe via adapter
    adapter.subscribeQuotes(symbols, adapterCallback);

    // Store subscription with adapter callback for cleanup
    const subscription: Subscription = {
      brokerId,
      symbols,
      callbacks: new Set([callback as any]),
      adapterCallback,
    };
    this.subscriptions.set(subscriptionId, subscription);

    console.log(`[WebSocketManager] Subscribed to quotes: ${subscriptionId}`);
    return subscriptionId;
  }

  /**
   * Unsubscribe from quotes (removes entire subscription)
   */
  unsubscribeQuotes(subscriptionId: string): void {
    const subscription = this.subscriptions.get(subscriptionId);
    if (!subscription) return;

    const adapter = this.adapters.get(subscription.brokerId);
    if (adapter) {
      adapter.unsubscribeQuotes(subscription.symbols);
    }

    // Clear all callbacks
    subscription.callbacks.clear();
    this.subscriptions.delete(subscriptionId);
    console.log(`[WebSocketManager] Unsubscribed: ${subscriptionId}`);
  }

  /**
   * Remove a specific callback from a subscription
   * If no callbacks remain, unsubscribes entirely
   */
  removeCallback(subscriptionId: string, callback: (quote: UnifiedQuote) => void): void {
    const subscription = this.subscriptions.get(subscriptionId);
    if (!subscription) return;

    subscription.callbacks.delete(callback as any);
    console.log(`[WebSocketManager] Removed callback from: ${subscriptionId}, remaining: ${subscription.callbacks.size}`);

    // If no callbacks remain, unsubscribe from adapter
    if (subscription.callbacks.size === 0) {
      this.unsubscribeQuotes(subscriptionId);
    }
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
