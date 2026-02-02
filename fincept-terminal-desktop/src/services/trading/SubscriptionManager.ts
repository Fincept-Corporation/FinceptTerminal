/**
 * SubscriptionManager - Reference-counted WebSocket subscription manager
 *
 * Prevents duplicate subscriptions when multiple components subscribe to the same symbol.
 * Implements proper cleanup when all subscribers unsubscribe.
 *
 * Production patterns:
 * - Reference counting for subscriptions
 * - Automatic cleanup on last unsubscribe
 * - Subscription state tracking
 * - Error recovery with auto-resubscribe
 */

import { websocketBridge } from './websocketBridge';

// ============================================================================
// Types
// ============================================================================

export interface SubscriptionKey {
  provider: string;
  symbol: string;
  channel: string;
}

export interface SubscriptionState {
  key: SubscriptionKey;
  refCount: number;
  status: 'pending' | 'active' | 'error' | 'unsubscribing';
  subscribedAt: number | null;
  lastError: string | null;
  subscribers: Set<string>; // Track subscriber IDs for debugging
}

export interface SubscribeOptions {
  subscriberId: string; // Unique ID for the subscribing component
  params?: Record<string, unknown>;
  onError?: (error: Error) => void;
}

export interface UnsubscribeOptions {
  subscriberId: string;
  force?: boolean; // Force unsubscribe even if refCount > 0
}

// ============================================================================
// Subscription Manager
// ============================================================================

class SubscriptionManager {
  private subscriptions: Map<string, SubscriptionState> = new Map();
  private pendingOperations: Map<string, Promise<void>> = new Map();

  /**
   * Generate a unique key for subscription lookup
   */
  private getKey(provider: string, symbol: string, channel: string): string {
    return `${provider}:${symbol}:${channel}`;
  }

  /**
   * Parse key back to components
   */
  private parseKey(key: string): SubscriptionKey {
    const [provider, symbol, channel] = key.split(':');
    return { provider, symbol, channel };
  }

  /**
   * Subscribe to a symbol/channel with reference counting
   *
   * If already subscribed, increments ref count.
   * If new subscription, creates actual WebSocket subscription.
   */
  async subscribe(
    provider: string,
    symbol: string,
    channel: string,
    options: SubscribeOptions
  ): Promise<void> {
    const key = this.getKey(provider, symbol, channel);
    const { subscriberId, params, onError } = options;

    // Check if already subscribed
    const existing = this.subscriptions.get(key);
    if (existing) {
      // Increment ref count
      existing.refCount++;
      existing.subscribers.add(subscriberId);
      console.log(
        `[SubscriptionManager] Reused subscription: ${key}, refCount: ${existing.refCount}`
      );
      return;
    }

    // Wait for any pending operation on this key
    const pending = this.pendingOperations.get(key);
    if (pending) {
      await pending;
      // Check again after pending completes
      const existingAfterPending = this.subscriptions.get(key);
      if (existingAfterPending) {
        existingAfterPending.refCount++;
        existingAfterPending.subscribers.add(subscriberId);
        return;
      }
    }

    // Create new subscription state
    const state: SubscriptionState = {
      key: { provider, symbol, channel },
      refCount: 1,
      status: 'pending',
      subscribedAt: null,
      lastError: null,
      subscribers: new Set([subscriberId]),
    };
    this.subscriptions.set(key, state);

    // Create actual subscription
    const subscribePromise = (async () => {
      try {
        await websocketBridge.subscribe(provider, symbol, channel, params);
        state.status = 'active';
        state.subscribedAt = Date.now();
        state.lastError = null;
        console.log(`[SubscriptionManager] Subscribed: ${key}`);
      } catch (error) {
        state.status = 'error';
        state.lastError = error instanceof Error ? error.message : String(error);
        console.error(`[SubscriptionManager] Subscribe failed: ${key}`, error);

        // Notify subscriber of error
        onError?.(error instanceof Error ? error : new Error(String(error)));

        // Clean up failed subscription if no other refs were added
        if (state.refCount === 1) {
          this.subscriptions.delete(key);
        }
      } finally {
        this.pendingOperations.delete(key);
      }
    })();

    this.pendingOperations.set(key, subscribePromise);
    await subscribePromise;
  }

  /**
   * Unsubscribe from a symbol/channel with reference counting
   *
   * Decrements ref count. Only actually unsubscribes when refCount reaches 0.
   */
  async unsubscribe(
    provider: string,
    symbol: string,
    channel: string,
    options: UnsubscribeOptions
  ): Promise<void> {
    const key = this.getKey(provider, symbol, channel);
    const { subscriberId, force = false } = options;

    const state = this.subscriptions.get(key);
    if (!state) {
      console.warn(`[SubscriptionManager] No subscription to unsubscribe: ${key}`);
      return;
    }

    // Remove this subscriber
    state.subscribers.delete(subscriberId);
    state.refCount = Math.max(0, state.refCount - 1);

    console.log(
      `[SubscriptionManager] Unsubscribe requested: ${key}, refCount: ${state.refCount}`
    );

    // Only unsubscribe if no more refs (or forced)
    if (state.refCount === 0 || force) {
      // Wait for any pending operation
      const pending = this.pendingOperations.get(key);
      if (pending) {
        await pending;
      }

      state.status = 'unsubscribing';

      const unsubscribePromise = (async () => {
        try {
          await websocketBridge.unsubscribe(provider, symbol, channel);
          this.subscriptions.delete(key);
          console.log(`[SubscriptionManager] Unsubscribed: ${key}`);
        } catch (error) {
          console.error(`[SubscriptionManager] Unsubscribe failed: ${key}`, error);
          // Still remove from tracking - WebSocket will clean up on reconnect
          this.subscriptions.delete(key);
        } finally {
          this.pendingOperations.delete(key);
        }
      })();

      this.pendingOperations.set(key, unsubscribePromise);
      await unsubscribePromise;
    }
  }

  /**
   * Subscribe to multiple channels for a symbol
   */
  async subscribeSymbol(
    provider: string,
    symbol: string,
    channels: string[],
    options: SubscribeOptions
  ): Promise<void> {
    await Promise.all(
      channels.map((channel) => this.subscribe(provider, symbol, channel, options))
    );
  }

  /**
   * Unsubscribe from multiple channels for a symbol
   */
  async unsubscribeSymbol(
    provider: string,
    symbol: string,
    channels: string[],
    options: UnsubscribeOptions
  ): Promise<void> {
    await Promise.all(
      channels.map((channel) => this.unsubscribe(provider, symbol, channel, options))
    );
  }

  /**
   * Get current subscription state
   */
  getSubscription(
    provider: string,
    symbol: string,
    channel: string
  ): SubscriptionState | undefined {
    const key = this.getKey(provider, symbol, channel);
    return this.subscriptions.get(key);
  }

  /**
   * Check if subscribed to a symbol/channel
   */
  isSubscribed(provider: string, symbol: string, channel: string): boolean {
    const state = this.getSubscription(provider, symbol, channel);
    return state?.status === 'active';
  }

  /**
   * Get all active subscriptions
   */
  getActiveSubscriptions(): SubscriptionState[] {
    return Array.from(this.subscriptions.values()).filter(
      (s) => s.status === 'active'
    );
  }

  /**
   * Get subscription stats
   */
  getStats(): {
    total: number;
    active: number;
    pending: number;
    error: number;
    totalRefCount: number;
  } {
    let active = 0;
    let pending = 0;
    let error = 0;
    let totalRefCount = 0;

    for (const state of this.subscriptions.values()) {
      totalRefCount += state.refCount;
      switch (state.status) {
        case 'active':
          active++;
          break;
        case 'pending':
          pending++;
          break;
        case 'error':
          error++;
          break;
      }
    }

    return {
      total: this.subscriptions.size,
      active,
      pending,
      error,
      totalRefCount,
    };
  }

  /**
   * Force cleanup all subscriptions (use on app shutdown)
   */
  async cleanup(): Promise<void> {
    console.log('[SubscriptionManager] Cleaning up all subscriptions...');

    const unsubscribePromises: Promise<void>[] = [];

    for (const [key, state] of this.subscriptions) {
      const { provider, symbol, channel } = this.parseKey(key);
      unsubscribePromises.push(
        websocketBridge.unsubscribe(provider, symbol, channel).catch((err) => {
          console.warn(`[SubscriptionManager] Cleanup unsubscribe failed: ${key}`, err);
        })
      );
    }

    await Promise.all(unsubscribePromises);
    this.subscriptions.clear();
    this.pendingOperations.clear();

    console.log('[SubscriptionManager] Cleanup complete');
  }

  /**
   * Resubscribe all active subscriptions (use after reconnect)
   */
  async resubscribeAll(): Promise<void> {
    console.log('[SubscriptionManager] Resubscribing all active subscriptions...');

    const resubscribePromises: Promise<void>[] = [];

    for (const [key, state] of this.subscriptions) {
      if (state.status === 'active' || state.status === 'error') {
        const { provider, symbol, channel } = state.key;
        state.status = 'pending';

        resubscribePromises.push(
          (async () => {
            try {
              await websocketBridge.subscribe(provider, symbol, channel);
              state.status = 'active';
              state.subscribedAt = Date.now();
              state.lastError = null;
            } catch (error) {
              state.status = 'error';
              state.lastError = error instanceof Error ? error.message : String(error);
              console.error(`[SubscriptionManager] Resubscribe failed: ${key}`, error);
            }
          })()
        );
      }
    }

    await Promise.all(resubscribePromises);
    console.log('[SubscriptionManager] Resubscribe complete');
  }
}

// ============================================================================
// Singleton Export
// ============================================================================

export const subscriptionManager = new SubscriptionManager();
export default subscriptionManager;

// ============================================================================
// React Hook Helper
// ============================================================================

/**
 * Generate a unique subscriber ID for React components
 * Use this with useRef to maintain consistent ID across renders
 */
export function generateSubscriberId(): string {
  return `sub_${Date.now()}_${Math.random().toString(36).substring(2, 9)}`;
}
