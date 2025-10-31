// File: src/services/websocket/WebSocketRegistry.ts
// Connection registry for managing WebSocket adapter instances and lifecycle

import {
  IWebSocketAdapter,
  ConnectionStatus,
  ConnectionMetrics,
  ProviderConfig
} from './types';
import { EventBus } from './EventBus';

/**
 * Registry entry for a WebSocket adapter
 */
interface RegistryEntry {
  adapter: IWebSocketAdapter;
  config: ProviderConfig;
  createdAt: number;
  refCount: number; // Track number of active subscribers
}

/**
 * WebSocketRegistry - Manages lifecycle and pooling of WebSocket adapters
 *
 * Features:
 * - Single instance per provider (connection pooling)
 * - Reference counting for automatic cleanup
 * - Health monitoring
 * - Automatic reconnection
 * - Connection metrics tracking
 */
export class WebSocketRegistry {
  private registry: Map<string, RegistryEntry> = new Map();
  private eventBus: EventBus;
  private enableLogging: boolean = false;

  // Cleanup settings
  private cleanupIntervalMs: number = 60000; // 1 minute
  private cleanupTimeoutMs: number = 300000; // 5 minutes of inactivity
  private cleanupTimer: NodeJS.Timeout | null = null;

  constructor(eventBus: EventBus, options?: {
    enableLogging?: boolean;
    cleanupIntervalMs?: number;
    cleanupTimeoutMs?: number;
  }) {
    this.eventBus = eventBus;
    this.enableLogging = options?.enableLogging ?? false;
    this.cleanupIntervalMs = options?.cleanupIntervalMs ?? 60000;
    this.cleanupTimeoutMs = options?.cleanupTimeoutMs ?? 300000;

    this.startCleanupTimer();

    if (this.enableLogging) {
      console.log('[WebSocketRegistry] Initialized');
    }
  }

  /**
   * Register a new adapter instance
   */
  register(provider: string, adapter: IWebSocketAdapter, config: ProviderConfig): void {
    if (this.registry.has(provider)) {
      throw new Error(`Provider ${provider} is already registered`);
    }

    const entry: RegistryEntry = {
      adapter,
      config,
      createdAt: Date.now(),
      refCount: 0
    };

    this.registry.set(provider, entry);

    if (this.enableLogging) {
      console.log(`[WebSocketRegistry] Registered provider: ${provider}`);
    }
  }

  /**
   * Unregister an adapter and cleanup
   */
  async unregister(provider: string): Promise<void> {
    const entry = this.registry.get(provider);

    if (!entry) {
      console.warn(`[WebSocketRegistry] Provider not found: ${provider}`);
      return;
    }

    // Disconnect and cleanup
    try {
      await entry.adapter.disconnect();
      entry.adapter.destroy();
    } catch (error) {
      console.error(`[WebSocketRegistry] Error cleaning up ${provider}:`, error);
    }

    this.registry.delete(provider);

    if (this.enableLogging) {
      console.log(`[WebSocketRegistry] Unregistered provider: ${provider}`);
    }
  }

  /**
   * Get adapter instance (creates if doesn't exist)
   */
  get(provider: string): IWebSocketAdapter | null {
    const entry = this.registry.get(provider);
    return entry ? entry.adapter : null;
  }

  /**
   * Check if provider is registered
   */
  has(provider: string): boolean {
    return this.registry.has(provider);
  }

  /**
   * Get all registered providers
   */
  getProviders(): string[] {
    return Array.from(this.registry.keys());
  }

  /**
   * Increment reference count (track active subscribers)
   */
  addRef(provider: string): void {
    const entry = this.registry.get(provider);
    if (entry) {
      entry.refCount++;

      if (this.enableLogging) {
        console.log(`[WebSocketRegistry] ${provider} refCount: ${entry.refCount}`);
      }
    }
  }

  /**
   * Decrement reference count
   */
  release(provider: string): void {
    const entry = this.registry.get(provider);
    if (entry) {
      entry.refCount = Math.max(0, entry.refCount - 1);

      if (this.enableLogging) {
        console.log(`[WebSocketRegistry] ${provider} refCount: ${entry.refCount}`);
      }

      // Schedule cleanup if no active references
      if (entry.refCount === 0) {
        this.scheduleCleanup(provider);
      }
    }
  }

  /**
   * Get reference count for provider
   */
  getRefCount(provider: string): number {
    const entry = this.registry.get(provider);
    return entry ? entry.refCount : 0;
  }

  /**
   * Get metrics for a provider
   */
  getMetrics(provider: string): ConnectionMetrics | null {
    const entry = this.registry.get(provider);
    return entry ? entry.adapter.metrics : null;
  }

  /**
   * Get metrics for all providers
   */
  getAllMetrics(): Map<string, ConnectionMetrics> {
    const metrics = new Map<string, ConnectionMetrics>();

    this.registry.forEach((entry, provider) => {
      metrics.set(provider, entry.adapter.metrics);
    });

    return metrics;
  }

  /**
   * Get connection status for a provider
   */
  getStatus(provider: string): ConnectionStatus | null {
    const entry = this.registry.get(provider);
    return entry ? entry.adapter.status : null;
  }

  /**
   * Check if provider is connected
   */
  isConnected(provider: string): boolean {
    const status = this.getStatus(provider);
    return status === ConnectionStatus.CONNECTED;
  }

  /**
   * Get all connection statuses
   */
  getAllStatuses(): Map<string, ConnectionStatus> {
    const statuses = new Map<string, ConnectionStatus>();

    this.registry.forEach((entry, provider) => {
      statuses.set(provider, entry.adapter.status);
    });

    return statuses;
  }

  /**
   * Schedule cleanup for idle connections
   */
  private scheduleCleanup(provider: string): void {
    // Don't cleanup if there are active references
    const entry = this.registry.get(provider);
    if (!entry || entry.refCount > 0) {
      return;
    }

    // Cleanup will happen in the cleanup timer if still idle
  }

  /**
   * Start periodic cleanup timer
   */
  private startCleanupTimer(): void {
    this.cleanupTimer = setInterval(() => {
      this.performCleanup();
    }, this.cleanupIntervalMs);
  }

  /**
   * Perform cleanup of idle connections
   */
  private async performCleanup(): Promise<void> {
    const now = Date.now();
    const toCleanup: string[] = [];

    this.registry.forEach((entry, provider) => {
      // Skip if has active references
      if (entry.refCount > 0) {
        return;
      }

      // Check if connection has been idle too long
      const metrics = entry.adapter.metrics;
      const lastActivity = metrics.lastMessageAt || entry.createdAt;
      const idleTime = now - lastActivity;

      if (idleTime > this.cleanupTimeoutMs) {
        toCleanup.push(provider);
      }
    });

    // Cleanup idle connections
    for (const provider of toCleanup) {
      if (this.enableLogging) {
        console.log(`[WebSocketRegistry] Cleaning up idle connection: ${provider}`);
      }
      await this.unregister(provider);
    }
  }

  /**
   * Force cleanup of all connections
   */
  async cleanup(): Promise<void> {
    const providers = this.getProviders();

    for (const provider of providers) {
      await this.unregister(provider);
    }

    if (this.cleanupTimer) {
      clearInterval(this.cleanupTimer);
      this.cleanupTimer = null;
    }

    if (this.enableLogging) {
      console.log('[WebSocketRegistry] All connections cleaned up');
    }
  }

  /**
   * Get registry statistics
   */
  getStats() {
    const stats = {
      totalProviders: this.registry.size,
      connectedProviders: 0,
      totalSubscriptions: 0,
      totalMessages: 0,
      providers: new Map<string, {
        status: ConnectionStatus;
        refCount: number;
        uptime: number;
        subscriptions: number;
      }>()
    };

    this.registry.forEach((entry, provider) => {
      const metrics = entry.adapter.metrics;

      if (entry.adapter.status === ConnectionStatus.CONNECTED) {
        stats.connectedProviders++;
      }

      stats.totalSubscriptions += metrics.subscriptions;
      stats.totalMessages += metrics.messagesReceived;

      stats.providers.set(provider, {
        status: entry.adapter.status,
        refCount: entry.refCount,
        uptime: metrics.uptime || 0,
        subscriptions: metrics.subscriptions
      });
    });

    return stats;
  }

  /**
   * Reconnect a provider
   */
  async reconnect(provider: string): Promise<void> {
    const entry = this.registry.get(provider);

    if (!entry) {
      throw new Error(`Provider not found: ${provider}`);
    }

    if (this.enableLogging) {
      console.log(`[WebSocketRegistry] Reconnecting provider: ${provider}`);
    }

    try {
      await entry.adapter.disconnect();
      await entry.adapter.connect(entry.config);
    } catch (error) {
      console.error(`[WebSocketRegistry] Failed to reconnect ${provider}:`, error);
      throw error;
    }
  }

  /**
   * Reconnect all providers
   */
  async reconnectAll(): Promise<void> {
    const providers = this.getProviders();

    for (const provider of providers) {
      try {
        await this.reconnect(provider);
      } catch (error) {
        console.error(`[WebSocketRegistry] Failed to reconnect ${provider}:`, error);
      }
    }
  }

  /**
   * Ping all connected providers
   */
  async pingAll(): Promise<Map<string, number>> {
    const results = new Map<string, number>();

    for (const [provider, entry] of this.registry.entries()) {
      if (entry.adapter.status === ConnectionStatus.CONNECTED) {
        try {
          const latency = await entry.adapter.ping();
          results.set(provider, latency);
        } catch (error) {
          console.error(`[WebSocketRegistry] Ping failed for ${provider}:`, error);
          results.set(provider, -1);
        }
      }
    }

    return results;
  }

  /**
   * Enable/disable logging
   */
  setLogging(enabled: boolean): void {
    this.enableLogging = enabled;
  }

  /**
   * Destroy registry and cleanup all resources
   */
  async destroy(): Promise<void> {
    await this.cleanup();

    if (this.enableLogging) {
      console.log('[WebSocketRegistry] Destroyed');
    }
  }
}
