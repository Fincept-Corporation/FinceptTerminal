// File: src/services/websocket/EventBus.ts
// Lightweight pub/sub event bus for distributing WebSocket messages

import { EventBusEvent, MessageCallback, NormalizedMessage } from './types';

/**
 * Topic subscription metadata
 */
interface TopicSubscription {
  id: string;
  topic: string;
  pattern: RegExp;
  callback: MessageCallback;
  subscribedAt: number;
}

/**
 * EventBus - Centralized message distribution using pub/sub pattern
 *
 * Features:
 * - Topic-based subscriptions with wildcard support
 * - Memory-efficient using WeakMap where possible
 * - Topic pattern matching (exact, wildcard, regex)
 * - Message history (optional, for debugging)
 * - Performance monitoring
 */
export class EventBus {
  private subscriptions: Map<string, TopicSubscription[]> = new Map();
  private messageHistory: EventBusEvent[] = [];
  private maxHistorySize: number = 100;
  private enableHistory: boolean = false;
  private enableLogging: boolean = false;

  // Performance metrics
  private messageCount: number = 0;
  private lastMessageTime: number = 0;
  private messageRate: number = 0;

  constructor(options?: {
    enableHistory?: boolean;
    maxHistorySize?: number;
    enableLogging?: boolean;
  }) {
    this.enableHistory = options?.enableHistory ?? false;
    this.maxHistorySize = options?.maxHistorySize ?? 100;
    this.enableLogging = options?.enableLogging ?? false;

    if (this.enableLogging) {
      console.log('[EventBus] Initialized with history:', this.enableHistory);
    }
  }

  /**
   * Subscribe to a topic with optional wildcard support
   *
   * Examples:
   * - "kraken.ticker.BTC/USD" - exact match
   * - "kraken.ticker.*" - all tickers from Kraken
   * - "kraken.*" - all messages from Kraken
   * - "*.ticker.BTC/USD" - BTC/USD ticker from any provider
   * - "*" - all messages (use sparingly)
   *
   * @returns Unsubscribe function
   */
  subscribe(topic: string, callback: MessageCallback): () => void {
    const subscriptionId = this.generateSubscriptionId();
    const pattern = this.topicToRegex(topic);

    const subscription: TopicSubscription = {
      id: subscriptionId,
      topic,
      pattern,
      callback,
      subscribedAt: Date.now()
    };

    // Store by exact topic for efficient lookup
    const key = this.getSubscriptionKey(topic);
    if (!this.subscriptions.has(key)) {
      this.subscriptions.set(key, []);
    }
    this.subscriptions.get(key)!.push(subscription);

    if (this.enableLogging) {
      console.log(`[EventBus] Subscribed to topic: ${topic} (ID: ${subscriptionId})`);
    }

    // Return unsubscribe function
    return () => this.unsubscribe(subscriptionId, topic);
  }

  /**
   * Unsubscribe from a topic
   */
  private unsubscribe(subscriptionId: string, topic: string): void {
    const key = this.getSubscriptionKey(topic);
    const subs = this.subscriptions.get(key);

    if (subs) {
      const index = subs.findIndex(s => s.id === subscriptionId);
      if (index !== -1) {
        subs.splice(index, 1);

        // Clean up empty subscription arrays
        if (subs.length === 0) {
          this.subscriptions.delete(key);
        }

        if (this.enableLogging) {
          console.log(`[EventBus] Unsubscribed from topic: ${topic} (ID: ${subscriptionId})`);
        }
      }
    }
  }

  /**
   * Publish a message to all matching subscribers
   */
  publish(topic: string, message: NormalizedMessage): void {
    const event: EventBusEvent = {
      topic,
      message,
      timestamp: Date.now()
    };

    // Update metrics
    this.updateMetrics();

    // Add to history if enabled
    if (this.enableHistory) {
      this.addToHistory(event);
    }

    // Find all matching subscriptions
    const matchingSubscriptions = this.findMatchingSubscriptions(topic);

    // Only log chart/candle related messages
    if (this.enableLogging && matchingSubscriptions.length > 0 && (topic.includes('ohlc') || topic.includes('candle'))) {
      console.log(`[EventBus] 📊 Publishing chart data to ${matchingSubscriptions.length} subscribers: ${topic}`);
    }

    // Deliver message to all matching subscribers
    matchingSubscriptions.forEach(sub => {
      try {
        sub.callback(message);
      } catch (error) {
        console.error(`[EventBus] Error in subscription callback for ${topic}:`, error);
      }
    });
  }

  /**
   * Find all subscriptions matching a topic
   */
  private findMatchingSubscriptions(topic: string): TopicSubscription[] {
    const matches: TopicSubscription[] = [];

    // Check all subscription patterns
    this.subscriptions.forEach(subs => {
      subs.forEach(sub => {
        if (sub.pattern.test(topic)) {
          matches.push(sub);
        }
      });
    });

    return matches;
  }

  /**
   * Convert topic pattern to RegExp
   * Supports wildcards: * (any segment), ** (any number of segments)
   */
  private topicToRegex(topic: string): RegExp {
    // Escape special regex characters except *
    const escaped = topic.replace(/[.+?^${}()|[\]\\]/g, '\\$&');

    // Replace wildcards
    const pattern = escaped
      .replace(/\\\*\\\*/g, '.*') // ** matches anything
      .replace(/\\\*/g, '[^.]+');  // * matches one segment

    return new RegExp(`^${pattern}$`);
  }

  /**
   * Get subscription key for efficient storage
   * Groups similar patterns together
   */
  private getSubscriptionKey(topic: string): string {
    // Use the first non-wildcard segment as key
    const parts = topic.split('.');
    const firstPart = parts[0];
    return firstPart === '*' ? '__wildcard__' : firstPart;
  }

  /**
   * Generate unique subscription ID
   */
  private generateSubscriptionId(): string {
    return `sub_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  /**
   * Add event to message history
   */
  private addToHistory(event: EventBusEvent): void {
    this.messageHistory.push(event);

    // Trim history if it exceeds max size
    if (this.messageHistory.length > this.maxHistorySize) {
      this.messageHistory = this.messageHistory.slice(-this.maxHistorySize);
    }
  }

  /**
   * Update performance metrics
   */
  private updateMetrics(): void {
    this.messageCount++;
    const now = Date.now();

    if (this.lastMessageTime > 0) {
      const timeDiff = (now - this.lastMessageTime) / 1000; // seconds
      this.messageRate = 1 / timeDiff;
    }

    this.lastMessageTime = now;
  }

  /**
   * Get message history
   */
  getHistory(limit?: number): EventBusEvent[] {
    if (!this.enableHistory) {
      console.warn('[EventBus] History is disabled');
      return [];
    }

    if (limit) {
      return this.messageHistory.slice(-limit);
    }

    return [...this.messageHistory];
  }

  /**
   * Get history for a specific topic
   */
  getHistoryForTopic(topic: string, limit?: number): EventBusEvent[] {
    const pattern = this.topicToRegex(topic);
    const matching = this.messageHistory.filter(event => pattern.test(event.topic));

    if (limit) {
      return matching.slice(-limit);
    }

    return matching;
  }

  /**
   * Clear message history
   */
  clearHistory(): void {
    this.messageHistory = [];
    if (this.enableLogging) {
      console.log('[EventBus] History cleared');
    }
  }

  /**
   * Get current subscription count
   */
  getSubscriptionCount(): number {
    let count = 0;
    this.subscriptions.forEach(subs => {
      count += subs.length;
    });
    return count;
  }

  /**
   * Get all active subscriptions
   */
  getSubscriptions(): Map<string, TopicSubscription[]> {
    return new Map(this.subscriptions);
  }

  /**
   * Get metrics
   */
  getMetrics() {
    return {
      totalMessages: this.messageCount,
      messagesPerSecond: this.messageRate,
      subscriptions: this.getSubscriptionCount(),
      historySize: this.messageHistory.length,
      lastMessageAt: this.lastMessageTime
    };
  }

  /**
   * Clear all subscriptions (useful for cleanup)
   */
  clear(): void {
    this.subscriptions.clear();
    this.clearHistory();
    this.messageCount = 0;
    this.lastMessageTime = 0;
    this.messageRate = 0;

    if (this.enableLogging) {
      console.log('[EventBus] All subscriptions cleared');
    }
  }

  /**
   * Enable/disable logging at runtime
   */
  setLogging(enabled: boolean): void {
    this.enableLogging = enabled;
  }

  /**
   * Enable/disable history at runtime
   */
  setHistory(enabled: boolean, maxSize?: number): void {
    this.enableHistory = enabled;
    if (maxSize !== undefined) {
      this.maxHistorySize = maxSize;
    }
    if (!enabled) {
      this.clearHistory();
    }
  }
}

// Singleton instance for global use
export const globalEventBus = new EventBus({
  enableHistory: false,
  enableLogging: false
});
