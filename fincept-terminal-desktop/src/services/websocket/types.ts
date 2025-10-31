// File: src/services/websocket/types.ts
// Core TypeScript types and interfaces for WebSocket management system

/**
 * Connection status for WebSocket providers
 */
export enum ConnectionStatus {
  DISCONNECTED = 'disconnected',
  CONNECTING = 'connecting',
  CONNECTED = 'connected',
  RECONNECTING = 'reconnecting',
  ERROR = 'error',
  RATE_LIMITED = 'rate_limited'
}

/**
 * Standard message format across all providers
 * Providers normalize their messages to this format
 */
export interface NormalizedMessage {
  provider: string;
  channel: string;
  type: 'ticker' | 'trade' | 'orderbook' | 'candle' | 'status' | 'error' | 'custom';
  symbol?: string;
  timestamp: number;
  data: any;
  raw?: any; // Original message for debugging
}

/**
 * Subscription topic structure
 * Example: "kraken.ticker.BTC/USD" or "fyers.orderbook.NSE:RELIANCE-EQ"
 */
export interface SubscriptionTopic {
  provider: string;
  channel: string;
  symbol?: string;
  params?: Record<string, any>;
}

/**
 * Callback function for message subscriptions
 */
export type MessageCallback = (message: NormalizedMessage) => void;

/**
 * Subscription handle returned to subscribers
 */
export interface Subscription {
  id: string;
  topic: string;
  callback: MessageCallback;
  unsubscribe: () => void;
}

/**
 * Provider configuration stored in SQLite
 */
export interface ProviderConfig {
  id?: number;
  provider_name: string;
  enabled: boolean;
  api_key?: string | null;
  api_secret?: string | null;
  endpoint?: string | null;
  config_data?: string | null; // JSON string for additional config
  created_at?: string;
  updated_at?: string;
}

/**
 * Connection health metrics
 */
export interface ConnectionMetrics {
  provider: string;
  status: ConnectionStatus;
  connectedAt?: number;
  lastMessageAt?: number;
  uptime?: number; // milliseconds
  latency?: number; // milliseconds
  messagesReceived: number;
  messagesPerSecond: number;
  reconnectionAttempts: number;
  errors: ConnectionError[];
  subscriptions: number;
}

/**
 * Connection error tracking
 */
export interface ConnectionError {
  timestamp: number;
  error: string;
  code?: string | number;
  details?: any;
}

/**
 * Rate limiting configuration
 */
export interface RateLimitConfig {
  maxMessagesPerSecond: number;
  maxSubscriptions: number;
  throttleMs: number;
  enabled: boolean;
}

/**
 * Reconnection policy configuration
 */
export interface ReconnectionPolicy {
  enabled: boolean;
  maxAttempts: number;
  initialDelayMs: number;
  maxDelayMs: number;
  backoffMultiplier: number;
  resetAfterMs: number; // Reset attempt counter after successful connection
}

/**
 * Abstract WebSocket adapter interface
 * All provider adapters must implement this
 */
export interface IWebSocketAdapter {
  readonly provider: string;
  readonly status: ConnectionStatus;
  readonly metrics: ConnectionMetrics;

  /**
   * Initialize connection with provider-specific config
   */
  connect(config: ProviderConfig): Promise<void>;

  /**
   * Gracefully disconnect
   */
  disconnect(): Promise<void>;

  /**
   * Subscribe to a channel/topic
   * @param topic - Topic string (e.g., "ticker.BTC/USD")
   * @param params - Additional subscription parameters
   */
  subscribe(topic: string, params?: Record<string, any>): Promise<void>;

  /**
   * Unsubscribe from a channel/topic
   */
  unsubscribe(topic: string): Promise<void>;

  /**
   * Send raw message to provider (for advanced use)
   */
  send(message: any): Promise<void>;

  /**
   * Register message handler
   */
  onMessage(callback: (message: NormalizedMessage) => void): void;

  /**
   * Register error handler
   */
  onError(callback: (error: ConnectionError) => void): void;

  /**
   * Register status change handler
   */
  onStatusChange(callback: (status: ConnectionStatus) => void): void;

  /**
   * Ping/health check
   */
  ping(): Promise<number>; // Returns latency in ms

  /**
   * Get current subscriptions
   */
  getSubscriptions(): string[];

  /**
   * Cleanup resources
   */
  destroy(): void;
}

/**
 * WebSocket Manager configuration
 */
export interface WebSocketManagerConfig {
  maxConnectionsPerProvider: number;
  globalRateLimit: RateLimitConfig;
  defaultReconnectionPolicy: ReconnectionPolicy;
  enableMetrics: boolean;
  enableLogging: boolean;
  logLevel: 'debug' | 'info' | 'warn' | 'error';
}

/**
 * Event bus event structure
 */
export interface EventBusEvent {
  topic: string;
  message: NormalizedMessage;
  timestamp: number;
}

/**
 * Provider adapter constructor signature
 */
export type AdapterConstructor = new () => IWebSocketAdapter;

/**
 * Registry of available adapters
 */
export interface AdapterRegistry {
  [provider: string]: AdapterConstructor;
}

/**
 * Health check result
 */
export interface HealthCheckResult {
  provider: string;
  healthy: boolean;
  latency?: number;
  error?: string;
  timestamp: number;
}

/**
 * Topic parser result
 */
export interface ParsedTopic {
  provider: string;
  channel: string;
  symbol?: string;
  params?: Record<string, any>;
  raw: string;
}

/**
 * Message queue item for backpressure handling
 */
export interface QueuedMessage {
  message: NormalizedMessage;
  timestamp: number;
  retries: number;
}

/**
 * Default configurations
 */
export const DEFAULT_RATE_LIMIT: RateLimitConfig = {
  maxMessagesPerSecond: 100,
  maxSubscriptions: 50,
  throttleMs: 10,
  enabled: true
};

export const DEFAULT_RECONNECTION_POLICY: ReconnectionPolicy = {
  enabled: true,
  maxAttempts: 10,
  initialDelayMs: 1000,
  maxDelayMs: 30000,
  backoffMultiplier: 2,
  resetAfterMs: 60000
};

export const DEFAULT_MANAGER_CONFIG: WebSocketManagerConfig = {
  maxConnectionsPerProvider: 10,
  globalRateLimit: DEFAULT_RATE_LIMIT,
  defaultReconnectionPolicy: DEFAULT_RECONNECTION_POLICY,
  enableMetrics: true,
  enableLogging: true,
  logLevel: 'info'
};

/**
 * Topic format: "provider.channel.symbol"
 * Examples:
 * - "kraken.ticker.BTC/USD"
 * - "fyers.orderbook.NSE:RELIANCE-EQ"
 * - "polygon.trades.AAPL"
 * - "binance.candles.BTCUSDT.1m"
 */
export function parseTopic(topic: string): ParsedTopic {
  const parts = topic.split('.');

  if (parts.length < 2) {
    throw new Error(`Invalid topic format: ${topic}. Expected "provider.channel[.symbol]"`);
  }

  const [provider, channel, symbol, ...paramParts] = parts;

  return {
    provider,
    channel,
    symbol,
    params: paramParts.length > 0 ? { extra: paramParts.join('.') } : undefined,
    raw: topic
  };
}

/**
 * Build topic string from components
 */
export function buildTopic(provider: string, channel: string, symbol?: string, params?: string[]): string {
  const parts = [provider, channel];
  if (symbol) parts.push(symbol);
  if (params && params.length > 0) parts.push(...params);
  return parts.join('.');
}
