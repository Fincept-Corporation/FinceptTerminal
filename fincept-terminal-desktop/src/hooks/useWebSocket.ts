// File: src/hooks/useWebSocket.ts
// React hook for WebSocket subscriptions

import { useState, useEffect, useCallback, useRef } from 'react';
import {
  getWebSocketManager,
  Subscription,
  NormalizedMessage,
  ConnectionStatus,
  MessageCallback
} from '@/services/websocket';

/**
 * Hook options
 */
export interface UseWebSocketOptions {
  /**
   * Auto-connect on mount (default: true)
   */
  autoConnect?: boolean;

  /**
   * Auto-subscribe on mount (default: true)
   */
  autoSubscribe?: boolean;

  /**
   * Optional subscription parameters
   */
  params?: Record<string, any>;

  /**
   * Custom error handler
   */
  onError?: (error: Error) => void;

  /**
   * Connection status change handler
   */
  onStatusChange?: (status: ConnectionStatus) => void;
}

/**
 * Hook return value
 */
export interface UseWebSocketReturn {
  /**
   * Latest message received
   */
  message: NormalizedMessage | null;

  /**
   * All messages received (up to maxMessages)
   */
  messages: NormalizedMessage[];

  /**
   * Connection status
   */
  status: ConnectionStatus | null;

  /**
   * Loading state (connecting or subscribing)
   */
  isLoading: boolean;

  /**
   * Error state
   */
  error: Error | null;

  /**
   * Subscribe to topic manually
   */
  subscribe: (topic: string, params?: Record<string, any>) => Promise<void>;

  /**
   * Unsubscribe from current topic
   */
  unsubscribe: () => void;

  /**
   * Send message (for two-way communication if supported)
   */
  send: (data: any) => Promise<void>;

  /**
   * Reconnect to provider
   */
  reconnect: () => Promise<void>;

  /**
   * Clear message history
   */
  clearMessages: () => void;
}

/**
 * useWebSocket - React hook for WebSocket subscriptions
 *
 * @param topic - Topic to subscribe to (format: "provider.channel.symbol")
 * @param callback - Optional message callback (alternative to using message state)
 * @param options - Hook options
 *
 * @example
 * ```typescript
 * // Simple usage with state
 * const { message, status } = useWebSocket('kraken.ticker.BTC/USD');
 *
 * // With callback
 * useWebSocket('kraken.ticker.BTC/USD', (msg) => {
 *   console.log('Price:', msg.data.last);
 * });
 *
 * // Manual subscription control
 * const { subscribe, unsubscribe, messages } = useWebSocket(
 *   'kraken.ticker.BTC/USD',
 *   null,
 *   { autoSubscribe: false }
 * );
 * ```
 */
export function useWebSocket(
  topic: string | null,
  callback?: MessageCallback | null,
  options: UseWebSocketOptions = {}
): UseWebSocketReturn {
  const {
    autoConnect = true,
    autoSubscribe = true,
    params,
    onError,
    onStatusChange
  } = options;

  // State
  const [message, setMessage] = useState<NormalizedMessage | null>(null);
  const [messages, setMessages] = useState<NormalizedMessage[]>([]);
  const [status, setStatus] = useState<ConnectionStatus | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);

  // Refs
  const subscriptionRef = useRef<Subscription | null>(null);
  const callbackRef = useRef<MessageCallback | null>(callback || null);
  const maxMessages = 100; // Keep last 100 messages

  // Update callback ref when it changes
  useEffect(() => {
    callbackRef.current = callback || null;
  }, [callback]);

  /**
   * Internal message handler
   */
  const handleMessage = useCallback((msg: NormalizedMessage) => {
    // Only log chart/candle messages
    if (msg.channel === 'ohlc' || msg.type === 'candle') {
      console.log(`[useWebSocket] 📊 Chart message:`, {
        provider: msg.provider,
        channel: msg.channel,
        type: msg.type,
        symbol: msg.symbol,
        dataKeys: Object.keys(msg.data || {})
      });
    }

    setMessage(msg);
    setMessages(prev => {
      const updated = [...prev, msg];
      return updated.slice(-maxMessages);
    });

    // Call user callback if provided
    if (callbackRef.current) {
      callbackRef.current(msg);
    }
  }, []);

  /**
   * Subscribe to topic
   */
  const subscribe = useCallback(async (
    subscribeTopic: string,
    subscribeParams?: Record<string, any>
  ) => {
    console.log(`[useWebSocket] 📡 Attempting to subscribe to: ${subscribeTopic}`);
    console.log(`[useWebSocket] Params:`, subscribeParams || params);

    try {
      setIsLoading(true);
      setError(null);

      const manager = getWebSocketManager();

      // Parse provider from topic
      const provider = subscribeTopic.split('.')[0];
      console.log(`[useWebSocket] Provider: ${provider}`);

      // Check WebSocket status BEFORE subscribing
      const wsStatus = manager.getStatus(provider);
      console.log(`[useWebSocket] WebSocket status for ${provider}: ${wsStatus}`);

      // Get or create provider config if needed
      // Note: Config should be set via setProviderConfig before subscribing
      // This is handled in SettingsTab

      // Subscribe
      console.log(`[useWebSocket] Calling manager.subscribe()...`);
      const sub = await manager.subscribe(
        subscribeTopic,
        handleMessage,
        subscribeParams || params
      );

      subscriptionRef.current = sub;
      console.log(`[useWebSocket] ✓ Subscription created: ${sub.id}`);

      // Update status
      const providerStatus = manager.getStatus(provider);
      setStatus(providerStatus);
      console.log(`[useWebSocket] Final status: ${providerStatus}`);

      if (onStatusChange && providerStatus) {
        onStatusChange(providerStatus);
      }
    } catch (err) {
      const error = err instanceof Error ? err : new Error(String(err));
      setError(error);

      console.error(`[useWebSocket] ✗ Subscribe FAILED for ${subscribeTopic}:`, error.message);

      if (onError) {
        onError(error);
      }

      // Only log errors that aren't connection-related (those are logged elsewhere)
      if (!error.message.includes('not connected') && !error.message.includes('not open')) {
        console.error('[useWebSocket] Subscribe error:', error);
      }
    } finally {
      setIsLoading(false);
    }
  }, [handleMessage, params, onError, onStatusChange]);

  /**
   * Unsubscribe
   */
  const unsubscribe = useCallback(() => {
    if (subscriptionRef.current) {
      subscriptionRef.current.unsubscribe();
      subscriptionRef.current = null;
      setStatus(ConnectionStatus.DISCONNECTED);
    }
  }, []);

  /**
   * Send message
   */
  const send = useCallback(async (data: any) => {
    if (!topic) {
      throw new Error('No topic specified');
    }

    const manager = getWebSocketManager();
    const provider = topic.split('.')[0];
    const adapter = (manager as any).registry.get(provider);

    if (!adapter) {
      throw new Error(`Provider ${provider} not connected`);
    }

    await adapter.send(data);
  }, [topic]);

  /**
   * Reconnect
   */
  const reconnect = useCallback(async () => {
    if (!topic) {
      throw new Error('No topic specified');
    }

    try {
      setIsLoading(true);
      setError(null);

      const manager = getWebSocketManager();
      const provider = topic.split('.')[0];

      await manager.reconnect(provider);

      // Resubscribe
      if (subscriptionRef.current) {
        unsubscribe();
        await subscribe(topic, params);
      }
    } catch (err) {
      const error = err instanceof Error ? err : new Error(String(err));
      setError(error);

      if (onError) {
        onError(error);
      }
    } finally {
      setIsLoading(false);
    }
  }, [topic, params, onError, subscribe, unsubscribe]);

  /**
   * Clear messages
   */
  const clearMessages = useCallback(() => {
    setMessages([]);
    setMessage(null);
  }, []);

  /**
   * Auto-subscribe on mount if enabled
   */
  useEffect(() => {
    let isMounted = true;

    if (topic && autoSubscribe && autoConnect && isMounted) {
      subscribe(topic, params);
    }

    // Cleanup on unmount
    return () => {
      isMounted = false;
      unsubscribe();
    };
  }, [topic, autoSubscribe, autoConnect]); // Intentionally limited dependencies

  /**
   * Monitor connection status and resubscribe if connection is restored
   */
  useEffect(() => {
    if (!topic || !autoSubscribe) return;

    const manager = getWebSocketManager();
    const provider = topic.split('.')[0];
    let isMounted = true;
    let isResubscribing = false;
    let consecutiveFailures = 0;

    const checkInterval = setInterval(async () => {
      if (!isMounted || isResubscribing) return; // Guard against unmounted component or concurrent resubscribes

      const providerStatus = manager.getStatus(provider);

      // If connected but no active subscription, resubscribe
      if (providerStatus === ConnectionStatus.CONNECTED && !subscriptionRef.current) {
        // Exponential backoff if we're having repeated failures
        if (consecutiveFailures > 3) {
          console.debug(`[useWebSocket] Too many consecutive failures for ${topic}, pausing auto-resubscribe`);
          return;
        }

        isResubscribing = true;
        try {
          console.log(`[useWebSocket] Auto-resubscribing to ${topic} after reconnection`);
          await subscribe(topic, params);
          consecutiveFailures = 0; // Reset on success
        } catch (error) {
          consecutiveFailures++;
          // Silently fail - subscription will retry on next interval
          const errorMsg = error instanceof Error ? error.message : String(error);
          if (!errorMsg.includes('not connected') && !errorMsg.includes('not open')) {
            console.debug(`[useWebSocket] Auto-resubscribe failed (attempt ${consecutiveFailures}):`, error);
          }
        } finally {
          isResubscribing = false;
        }
      } else if (subscriptionRef.current) {
        // Reset failure counter when subscription is active
        consecutiveFailures = 0;
      }
    }, 5000); // Increased to 5 seconds to reduce noise

    return () => {
      isMounted = false;
      clearInterval(checkInterval);
    };
  }, [topic, autoSubscribe, params, subscribe]);

  return {
    message,
    messages,
    status,
    isLoading,
    error,
    subscribe,
    unsubscribe,
    send,
    reconnect,
    clearMessages
  };
}

/**
 * useWebSocketManager - Hook for accessing WebSocketManager directly
 */
export function useWebSocketManager() {
  const manager = getWebSocketManager();
  const [stats, setStats] = useState(manager.getStats());
  const [statuses, setStatuses] = useState(manager.getAllStatuses());
  const [metrics, setMetrics] = useState(manager.getAllMetrics());

  useEffect(() => {
    let isMounted = true;

    // Update stats, statuses, and metrics periodically
    const interval = setInterval(() => {
      if (!isMounted) return; // Guard against unmounted component

      setStats(manager.getStats());
      setStatuses(manager.getAllStatuses());
      setMetrics(manager.getAllMetrics());
    }, 1000);

    return () => {
      isMounted = false;
      clearInterval(interval);
    };
  }, [manager]);

  return {
    manager,
    stats,
    statuses,
    metrics
  };
}

/**
 * useWebSocketConnection - Hook for managing provider connection
 */
export function useWebSocketConnection(provider: string) {
  const manager = getWebSocketManager();
  const [status, setStatus] = useState<ConnectionStatus | null>(
    manager.getStatus(provider)
  );
  const [metrics, setMetrics] = useState(manager.getMetrics(provider));

  useEffect(() => {
    let isMounted = true;

    // Update status and metrics periodically
    const interval = setInterval(() => {
      if (!isMounted) return; // Guard against unmounted component

      setStatus(manager.getStatus(provider));
      setMetrics(manager.getMetrics(provider));
    }, 1000);

    return () => {
      isMounted = false;
      clearInterval(interval);
    };
  }, [manager, provider]);

  const connect = useCallback(async () => {
    await manager.connect(provider);
  }, [manager, provider]);

  const disconnect = useCallback(async () => {
    await manager.disconnect(provider);
  }, [manager, provider]);

  const reconnect = useCallback(async () => {
    await manager.reconnect(provider);
  }, [manager, provider]);

  const ping = useCallback(async () => {
    return manager.ping(provider);
  }, [manager, provider]);

  return {
    status,
    metrics,
    connect,
    disconnect,
    reconnect,
    ping
  };
}
