// File: src/services/websocket/WebSocketManager.ts
// Central WebSocket manager - orchestrates all WebSocket connections

import {
  IWebSocketAdapter,
  ConnectionStatus,
  ProviderConfig,
  Subscription,
  NormalizedMessage,
  MessageCallback,
  WebSocketManagerConfig,
  DEFAULT_MANAGER_CONFIG,
  parseTopic
} from './types';
import { EventBus, globalEventBus } from './EventBus';
import { WebSocketRegistry } from './WebSocketRegistry';
import { createAdapter, isProviderAvailable } from './adapters';
import { generateId } from './utils';

/**
 * WebSocketManager - Central orchestrator for all WebSocket connections
 *
 * Features:
 * - Automatic connection management
 * - Connection pooling (single connection per provider)
 * - Topic-based subscriptions with pub/sub
 * - Metrics and health monitoring
 * - Configuration management
 *
 * Usage:
 * ```typescript
 * const manager = new WebSocketManager();
 *
 * // Subscribe to a topic
 * const subscription = await manager.subscribe('kraken.ticker.BTC/USD', (message) => {
 *   console.log('Price update:', message.data);
 * });
 *
 * // Unsubscribe
 * subscription.unsubscribe();
 * ```
 */
export class WebSocketManager {
  private eventBus: EventBus;
  private registry: WebSocketRegistry;
  private config: WebSocketManagerConfig;
  private providerConfigs: Map<string, ProviderConfig> = new Map();

  // Track subscriptions for cleanup
  private activeSubscriptions: Map<string, Subscription> = new Map();

  // Track ongoing connections to prevent duplicates
  private connectingProviders: Set<string> = new Set();

  constructor(config?: Partial<WebSocketManagerConfig>) {
    this.config = { ...DEFAULT_MANAGER_CONFIG, ...config };
    this.eventBus = globalEventBus;
    this.registry = new WebSocketRegistry(this.eventBus, {
      enableLogging: this.config.enableLogging
    });

    // Configure event bus
    this.eventBus.setLogging(this.config.enableLogging);
    this.eventBus.setHistory(this.config.enableMetrics, 100);

    this.log('WebSocketManager initialized');
  }

  /**
   * Subscribe to a topic
   * @param topic - Format: "provider.channel.symbol" e.g., "kraken.ticker.BTC/USD"
   * @param callback - Message handler callback
   * @param params - Optional subscription parameters
   */
  async subscribe(
    topic: string,
    callback: MessageCallback,
    params?: Record<string, any>
  ): Promise<Subscription> {
    try {
      const parsed = parseTopic(topic);
      const { provider, channel, symbol } = parsed;

      // Ensure adapter exists and is connected
      await this.ensureConnection(provider);

      // Get adapter
      const adapter = this.registry.get(provider);
      if (!adapter) {
        throw new Error(`Adapter not found for provider: ${provider}`);
      }

      // Subscribe via adapter (only if not already subscribed)
      const adapterTopic = symbol ? `${channel}.${symbol}` : channel;
      const existingSubscriptions = adapter.getSubscriptions();

      if (!existingSubscriptions.includes(adapterTopic)) {
        await adapter.subscribe(adapterTopic, params);
      }

      // Create event bus subscription
      const eventBusUnsubscribe = this.eventBus.subscribe(topic, callback);

      // Track reference
      this.registry.addRef(provider);

      // Create subscription object
      const subscriptionId = generateId('sub');
      const subscription: Subscription = {
        id: subscriptionId,
        topic,
        callback,
        unsubscribe: () => this.unsubscribe(subscriptionId, topic, provider, adapterTopic, eventBusUnsubscribe)
      };

      this.activeSubscriptions.set(subscriptionId, subscription);

      this.log(`Subscribed to: ${topic} (ID: ${subscriptionId})`);

      return subscription;
    } catch (error) {
      // Only log non-connection errors (connection errors are expected during reconnects)
      const errorMsg = error instanceof Error ? error.message : String(error);
      if (!errorMsg.includes('not connected') && !errorMsg.includes('not open')) {
        console.error(`[WebSocketManager] Subscribe failed for ${topic}:`, error);
      } else {
        console.debug(`[WebSocketManager] Subscribe pending connection for ${topic}`);
      }
      throw error;
    }
  }

  /**
   * Unsubscribe from a topic
   */
  private async unsubscribe(
    subscriptionId: string,
    topic: string,
    provider: string,
    adapterTopic: string,
    eventBusUnsubscribe: () => void
  ): Promise<void> {
    try {
      // Remove from active subscriptions first to avoid recursion
      const subscription = this.activeSubscriptions.get(subscriptionId);
      if (!subscription) {
        return; // Already unsubscribed
      }

      this.activeSubscriptions.delete(subscriptionId);

      // Unsubscribe from event bus
      eventBusUnsubscribe();

      // Release reference
      this.registry.release(provider);

      // Check if topic has other subscribers
      const hasOtherSubscribers = Array.from(this.activeSubscriptions.values())
        .some(sub => sub.topic === topic);

      // Unsubscribe from adapter if no other subscribers
      if (!hasOtherSubscribers) {
        const adapter = this.registry.get(provider);
        if (adapter) {
          await adapter.unsubscribe(adapterTopic);
        }
      }

      this.log(`Unsubscribed from: ${topic} (ID: ${subscriptionId})`);
    } catch (error) {
      console.error(`[WebSocketManager] Unsubscribe failed:`, error);
    }
  }

  /**
   * Ensure connection to provider exists and is healthy
   */
  private async ensureConnection(provider: string): Promise<void> {
    // Check if adapter exists
    if (!isProviderAvailable(provider)) {
      throw new Error(`Provider not supported: ${provider}`);
    }

    // Check if already connected
    if (this.registry.has(provider) && this.registry.isConnected(provider)) {
      return;
    }

    // Check if already connecting - PREVENT DUPLICATE CONNECTIONS
    if (this.connectingProviders.has(provider)) {
      this.log(`Already connecting to ${provider}, waiting...`);
      // Wait for existing connection to complete
      while (this.connectingProviders.has(provider)) {
        await new Promise(resolve => setTimeout(resolve, 100));
      }
      return;
    }

    // Mark as connecting
    this.connectingProviders.add(provider);

    try {
      // Get provider config
      const config = this.providerConfigs.get(provider);
      if (!config) {
        throw new Error(`No configuration found for provider: ${provider}. Use setProviderConfig() first.`);
      }

      // Create adapter if doesn't exist
      if (!this.registry.has(provider)) {
        const adapter = createAdapter(provider);

        // Setup message handler
        adapter.onMessage((message: NormalizedMessage) => {
          // Publish to event bus with full topic path
          const fullTopic = this.buildFullTopic(message);
          this.eventBus.publish(fullTopic, message);
        });

        // Setup error handler
        adapter.onError((error) => {
          this.log(`Error from ${provider}:`, error);
        });

        // Setup status change handler
        adapter.onStatusChange((status) => {
          this.log(`${provider} status changed to: ${status}`);
        });

        // Register adapter
        this.registry.register(provider, adapter, config);
      }

      // Connect
      const adapter = this.registry.get(provider);
      if (adapter && adapter.status !== ConnectionStatus.CONNECTED) {
        await adapter.connect(config);
      }
    } finally {
      // Always remove from connecting set
      this.connectingProviders.delete(provider);
    }
  }

  /**
   * Build full topic path from message
   */
  private buildFullTopic(message: NormalizedMessage): string {
    const parts = [message.provider, message.channel];

    if (message.symbol) {
      parts.push(message.symbol);
    }

    return parts.join('.');
  }

  /**
   * Set provider configuration
   */
  setProviderConfig(provider: string, config: ProviderConfig): void {
    this.providerConfigs.set(provider, config);
    this.log(`Configuration set for provider: ${provider}`);
  }

  /**
   * Get provider configuration
   */
  getProviderConfig(provider: string): ProviderConfig | undefined {
    return this.providerConfigs.get(provider);
  }

  /**
   * Connect to a provider manually
   */
  async connect(provider: string): Promise<void> {
    await this.ensureConnection(provider);
  }

  /**
   * Disconnect from a provider
   */
  async disconnect(provider: string): Promise<void> {
    if (this.registry.has(provider)) {
      await this.registry.unregister(provider);
      this.log(`Disconnected from provider: ${provider}`);
    }
  }

  /**
   * Get connection status for a provider
   */
  getStatus(provider: string): ConnectionStatus | null {
    return this.registry.getStatus(provider);
  }

  /**
   * Get all connection statuses
   */
  getAllStatuses(): Map<string, ConnectionStatus> {
    return this.registry.getAllStatuses();
  }

  /**
   * Get metrics for a provider
   */
  getMetrics(provider: string) {
    return this.registry.getMetrics(provider);
  }

  /**
   * Get all metrics
   */
  getAllMetrics() {
    return this.registry.getAllMetrics();
  }

  /**
   * Get registry statistics
   */
  getStats() {
    return {
      registry: this.registry.getStats(),
      eventBus: this.eventBus.getMetrics(),
      activeSubscriptions: this.activeSubscriptions.size,
      configuredProviders: this.providerConfigs.size
    };
  }

  /**
   * Ping a provider
   */
  async ping(provider: string): Promise<number> {
    const adapter = this.registry.get(provider);

    if (!adapter) {
      throw new Error(`Provider not connected: ${provider}`);
    }

    return adapter.ping();
  }

  /**
   * Ping all connected providers
   */
  async pingAll(): Promise<Map<string, number>> {
    return this.registry.pingAll();
  }

  /**
   * Reconnect a provider and restore subscriptions
   */
  async reconnect(provider: string): Promise<void> {
    // Get active subscriptions for this provider before reconnection
    const providerSubs = this.getProviderSubscriptions(provider);

    // Store subscription details
    const subsToRestore = providerSubs.map(sub => ({
      topic: sub.topic,
      callback: sub.callback,
      // Extract params from topic if needed
      params: {} // TODO: Store params in Subscription if needed
    }));

    // Reconnect the adapter
    await this.registry.reconnect(provider);

    // Resubscribe to all topics
    const adapter = this.registry.get(provider);
    if (adapter && subsToRestore.length > 0) {
      this.log(`Resubscribing to ${subsToRestore.length} topics for ${provider}`);

      for (const subInfo of subsToRestore) {
        try {
          const parsed = parseTopic(subInfo.topic);
          const adapterTopic = parsed.symbol ? `${parsed.channel}.${parsed.symbol}` : parsed.channel;
          await adapter.subscribe(adapterTopic, subInfo.params);
          this.log(`Resubscribed to: ${subInfo.topic}`);
        } catch (error) {
          console.error(`[WebSocketManager] Failed to resubscribe to ${subInfo.topic}:`, error);
        }
      }
    }
  }

  /**
   * Reconnect all providers and restore subscriptions
   */
  async reconnectAll(): Promise<void> {
    const providers = this.registry.getProviders();

    for (const provider of providers) {
      try {
        await this.reconnect(provider);
      } catch (error) {
        console.error(`[WebSocketManager] Failed to reconnect ${provider}:`, error);
      }
    }
  }

  /**
   * Get message history from event bus
   */
  getMessageHistory(topic?: string, limit?: number) {
    if (topic) {
      return this.eventBus.getHistoryForTopic(topic, limit);
    }
    return this.eventBus.getHistory(limit);
  }

  /**
   * Clear message history
   */
  clearHistory(): void {
    this.eventBus.clearHistory();
  }

  /**
   * Get active subscriptions
   */
  getSubscriptions(): Subscription[] {
    return Array.from(this.activeSubscriptions.values());
  }

  /**
   * Get subscriptions for a specific provider
   */
  getProviderSubscriptions(provider: string): Subscription[] {
    return Array.from(this.activeSubscriptions.values())
      .filter(sub => sub.topic.startsWith(`${provider}.`));
  }

  /**
   * Enable/disable logging
   */
  setLogging(enabled: boolean): void {
    this.config.enableLogging = enabled;
    this.eventBus.setLogging(enabled);
    this.registry.setLogging(enabled);
  }

  /**
   * Update configuration
   */
  updateConfig(config: Partial<WebSocketManagerConfig>): void {
    this.config = { ...this.config, ...config };
  }

  /**
   * Cleanup all resources
   */
  async destroy(): Promise<void> {
    this.log('Destroying WebSocketManager...');

    // Unsubscribe all
    const subscriptionIds = Array.from(this.activeSubscriptions.keys());
    for (const id of subscriptionIds) {
      const sub = this.activeSubscriptions.get(id);
      if (sub) {
        sub.unsubscribe();
      }
    }

    // Cleanup registry
    await this.registry.destroy();

    // Clear event bus
    this.eventBus.clear();

    this.log('WebSocketManager destroyed');
  }

  /**
   * Log helper
   */
  private log(...args: any[]): void {
    if (this.config.enableLogging) {
      console.log('[WebSocketManager]', ...args);
    }
  }
}

// Global singleton instance
let globalManagerInstance: WebSocketManager | null = null;

/**
 * Get global WebSocketManager instance (singleton)
 */
export function getWebSocketManager(): WebSocketManager {
  if (!globalManagerInstance) {
    globalManagerInstance = new WebSocketManager({
      enableLogging: true, // ENABLED FOR DEBUGGING
      enableMetrics: true
    });
  }
  return globalManagerInstance;
}

/**
 * Reset global instance (useful for testing)
 */
export function resetWebSocketManager(): void {
  if (globalManagerInstance) {
    globalManagerInstance.destroy();
    globalManagerInstance = null;
  }
}
