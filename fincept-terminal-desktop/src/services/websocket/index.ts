// File: src/services/websocket/index.ts
// Main entry point for WebSocket management system

// Core Manager
export { WebSocketManager, getWebSocketManager, resetWebSocketManager } from './WebSocketManager';

// Event Bus
export { EventBus, globalEventBus } from './EventBus';

// Registry
export { WebSocketRegistry } from './WebSocketRegistry';

// Types
export type {
  IWebSocketAdapter,
  ConnectionMetrics,
  ConnectionError,
  ProviderConfig,
  NormalizedMessage,
  Subscription,
  SubscriptionTopic,
  MessageCallback,
  RateLimitConfig,
  ReconnectionPolicy,
  WebSocketManagerConfig,
  EventBusEvent,
  AdapterRegistry,
  HealthCheckResult,
  ParsedTopic,
  QueuedMessage
} from './types';

export {
  ConnectionStatus, // enum (both type and value)
  DEFAULT_RATE_LIMIT,
  DEFAULT_RECONNECTION_POLICY,
  DEFAULT_MANAGER_CONFIG,
  parseTopic,
  buildTopic
} from './types';

// Utilities
export {
  ReconnectionManager,
  RateLimiter,
  CircuitBreaker,
  MessageThrottler,
  HeartbeatMonitor,
  PerformanceMetrics,
  retryWithBackoff,
  generateId,
  safeJsonParse,
  debounce
} from './utils';

// Adapters
export { BaseAdapter } from './adapters/BaseAdapter';
export { KrakenAdapter } from './adapters/KrakenAdapter';
export {
  adapters,
  getAvailableProviders,
  isProviderAvailable,
  createAdapter
} from './adapters';

/**
 * Quick start guide:
 *
 * 1. Set up provider configuration:
 * ```typescript
 * import { getWebSocketManager } from '@/services/websocket';
 *
 * const manager = getWebSocketManager();
 *
 * manager.setProviderConfig('kraken', {
 *   provider_name: 'kraken',
 *   enabled: true,
 *   endpoint: 'wss://ws.kraken.com/v2'
 * });
 * ```
 *
 * 2. Subscribe to topics:
 * ```typescript
 * const subscription = await manager.subscribe(
 *   'kraken.ticker.BTC/USD',
 *   (message) => {
 *     console.log('Price update:', message.data);
 *   }
 * );
 * ```
 *
 * 3. Use in React components:
 * ```typescript
 * import { useWebSocket } from '@/hooks/useWebSocket';
 *
 * function PriceDisplay() {
 *   const { message, status } = useWebSocket('kraken.ticker.BTC/USD');
 *
 *   return (
 *     <div>
 *       <p>Status: {status}</p>
 *       <p>Price: {message?.data.last}</p>
 *     </div>
 *   );
 * }
 * ```
 *
 * 4. Monitor connections in Settings:
 * See useWebSocketManager hook in hooks/useWebSocket.ts for connection monitoring
 */
