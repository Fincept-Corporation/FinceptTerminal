// File: src/services/websocket/adapters/BaseAdapter.ts
// Abstract base class for WebSocket adapters - provides common functionality

import {
  IWebSocketAdapter,
  ConnectionStatus,
  ConnectionMetrics,
  ConnectionError,
  ProviderConfig,
  NormalizedMessage,
  DEFAULT_RECONNECTION_POLICY,
  DEFAULT_RATE_LIMIT
} from '../types';
import {
  ReconnectionManager,
  RateLimiter,
  CircuitBreaker,
  HeartbeatMonitor,
  PerformanceMetrics,
  generateId
} from '../utils';

/**
 * Base WebSocket Adapter
 * Provides common functionality for all provider adapters:
 * - Connection management
 * - Automatic reconnection
 * - Rate limiting
 * - Health monitoring
 * - Error handling
 * - Metrics collection
 */
export abstract class BaseAdapter implements IWebSocketAdapter {
  abstract readonly provider: string;

  protected ws: WebSocket | null = null;
  protected _status: ConnectionStatus = ConnectionStatus.DISCONNECTED;
  protected config: ProviderConfig | null = null;

  // Utilities
  protected reconnectionManager: ReconnectionManager;
  protected rateLimiter: RateLimiter;
  protected circuitBreaker: CircuitBreaker;
  protected heartbeatMonitor: HeartbeatMonitor;
  protected performanceMetrics: PerformanceMetrics;

  // Callbacks
  private messageCallbacks: Array<(message: NormalizedMessage) => void> = [];
  private errorCallbacks: Array<(error: ConnectionError) => void> = [];
  private statusCallbacks: Array<(status: ConnectionStatus) => void> = [];

  // Subscriptions tracking
  protected subscriptions: Set<string> = new Set();

  // Metrics
  protected _metrics: ConnectionMetrics;

  // Logging
  protected enableLogging: boolean = false;

  constructor() {
    // Initialize metrics with empty provider (will be set by getter)
    this._metrics = {
      provider: '',
      status: ConnectionStatus.DISCONNECTED,
      messagesReceived: 0,
      messagesPerSecond: 0,
      reconnectionAttempts: 0,
      errors: [],
      subscriptions: 0
    };

    this.reconnectionManager = new ReconnectionManager(DEFAULT_RECONNECTION_POLICY);
    this.rateLimiter = new RateLimiter(DEFAULT_RATE_LIMIT);
    this.circuitBreaker = new CircuitBreaker(5, 60000, 2);
    this.heartbeatMonitor = new HeartbeatMonitor(30000, 60000);
    this.performanceMetrics = new PerformanceMetrics();
  }

  /**
   * Getter for current status
   */
  get status(): ConnectionStatus {
    return this._status;
  }

  /**
   * Getter for metrics
   */
  get metrics(): ConnectionMetrics {
    const perfMetrics = this.performanceMetrics.getMetrics();

    return {
      ...this._metrics,
      provider: this.provider,
      status: this._status,
      messagesReceived: perfMetrics.messageCount,
      messagesPerSecond: perfMetrics.messagesPerSecond,
      reconnectionAttempts: this.reconnectionManager.getAttempts(),
      subscriptions: this.subscriptions.size
    };
  }

  /**
   * Connect to WebSocket
   */
  async connect(config: ProviderConfig): Promise<void> {
    console.log(`[${this.provider}] 🔌 Connecting to WebSocket...`);

    if (!this.circuitBreaker.isAllowed()) {
      throw new Error(`Circuit breaker is open for ${this.provider}`);
    }

    this.config = config;
    this.setStatus(ConnectionStatus.CONNECTING);

    try {
      const url = this.getWebSocketUrl(config);
      console.log(`[${this.provider}] WebSocket URL: ${url}`);

      this.ws = new WebSocket(url);
      console.log(`[${this.provider}] WebSocket object created, readyState: ${this.ws.readyState}`);

      this.setupWebSocketHandlers();

      // Wait for connection
      console.log(`[${this.provider}] Waiting for WebSocket to open...`);
      await this.waitForConnection();
      console.log(`[${this.provider}] ✓ WebSocket opened! readyState: ${this.ws?.readyState}`);

      this.reconnectionManager.recordSuccess();
      this.circuitBreaker.recordSuccess();
      this.setStatus(ConnectionStatus.CONNECTED);
      this._metrics.connectedAt = Date.now();

      // Start heartbeat monitoring
      this.heartbeatMonitor.start(() => this.handleStaleConnection());
      console.log(`[${this.provider}] Heartbeat monitoring started`);

      // Perform provider-specific initialization
      await this.onConnected();

      console.log(`[${this.provider}] ✓ Connection complete!`);
      this.log('Connected successfully');
    } catch (error) {
      console.error(`[${this.provider}] ✗ Connection failed:`, error);

      this.circuitBreaker.recordFailure();
      this.reconnectionManager.recordAttempt();

      const errorObj: ConnectionError = {
        timestamp: Date.now(),
        error: error instanceof Error ? error.message : String(error)
      };
      this.addError(errorObj);
      this.notifyError(errorObj);

      this.setStatus(ConnectionStatus.ERROR);

      // Attempt reconnection if policy allows
      if (this.reconnectionManager.shouldAttempt()) {
        console.log(`[${this.provider}] Will retry connection...`);
        await this.scheduleReconnection();
      } else {
        throw error;
      }
    }
  }

  /**
   * Disconnect from WebSocket
   */
  async disconnect(): Promise<void> {
    this.log('Disconnecting...');

    this.heartbeatMonitor.stop();

    if (this.ws) {
      // Remove event listeners to prevent reconnection
      this.ws.onclose = null;
      this.ws.onerror = null;

      if (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING) {
        this.ws.close();
      }

      this.ws = null;
    }

    this.setStatus(ConnectionStatus.DISCONNECTED);
    this.subscriptions.clear();

    await this.onDisconnected();
  }

  /**
   * Subscribe to a topic
   */
  async subscribe(topic: string, params?: Record<string, any>): Promise<void> {
    console.log(`[${this.provider}] Subscribe requested: ${topic}`);
    console.log(`[${this.provider}] - WebSocket exists: ${!!this.ws}`);
    console.log(`[${this.provider}] - readyState: ${this.ws?.readyState} (OPEN=${WebSocket.OPEN})`);
    console.log(`[${this.provider}] - Status: ${this._status}`);

    // Check WebSocket is actually open FIRST (most accurate check)
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      throw new Error(`Cannot subscribe: WebSocket is not open (readyState: ${this.ws?.readyState || 'null'})`);
    }

    // Then check connection status
    if (this._status !== ConnectionStatus.CONNECTED) {
      throw new Error(`Cannot subscribe: connection status is ${this._status}`);
    }

    // Check rate limit
    if (!this.rateLimiter.tryConsume()) {
      const waitTime = this.rateLimiter.getWaitTime();
      this.log(`Rate limited, waiting ${waitTime}ms before subscribing to ${topic}`);
      await new Promise(resolve => setTimeout(resolve, waitTime));
    }

    await this.performSubscribe(topic, params);
    this.subscriptions.add(topic);

    this.log(`Subscribed to: ${topic}`);
  }

  /**
   * Unsubscribe from a topic
   */
  async unsubscribe(topic: string): Promise<void> {
    if (this._status !== ConnectionStatus.CONNECTED) {
      this.log(`Cannot unsubscribe: connection status is ${this._status}`);
      return;
    }

    await this.performUnsubscribe(topic);
    this.subscriptions.delete(topic);

    this.log(`Unsubscribed from: ${topic}`);
  }

  /**
   * Send raw message
   */
  async send(message: any): Promise<void> {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      throw new Error('WebSocket is not connected');
    }

    const data = typeof message === 'string' ? message : JSON.stringify(message);
    this.ws.send(data);
  }

  /**
   * Ping/health check
   */
  async ping(): Promise<number> {
    const start = Date.now();
    await this.performPing();
    const latency = Date.now() - start;

    this._metrics.latency = latency;
    this.performanceMetrics.recordLatency(latency);

    return latency;
  }

  /**
   * Get current subscriptions
   */
  getSubscriptions(): string[] {
    return Array.from(this.subscriptions);
  }

  /**
   * Register message callback
   */
  onMessage(callback: (message: NormalizedMessage) => void): void {
    this.messageCallbacks.push(callback);
  }

  /**
   * Register error callback
   */
  onError(callback: (error: ConnectionError) => void): void {
    this.errorCallbacks.push(callback);
  }

  /**
   * Register status change callback
   */
  onStatusChange(callback: (status: ConnectionStatus) => void): void {
    this.statusCallbacks.push(callback);
  }

  /**
   * Cleanup resources
   */
  destroy(): void {
    this.heartbeatMonitor.stop();
    this.messageCallbacks = [];
    this.errorCallbacks = [];
    this.statusCallbacks = [];
    this.subscriptions.clear();
    this.ws = null;
  }

  // ==================== Abstract Methods (Provider-Specific) ====================

  /**
   * Get WebSocket URL for provider
   */
  protected abstract getWebSocketUrl(config: ProviderConfig): string;

  /**
   * Handle incoming message (parse and normalize)
   */
  protected abstract handleMessage(event: MessageEvent): void;

  /**
   * Perform subscription (provider-specific)
   */
  protected abstract performSubscribe(topic: string, params?: Record<string, any>): Promise<void>;

  /**
   * Perform unsubscription (provider-specific)
   */
  protected abstract performUnsubscribe(topic: string): Promise<void>;

  /**
   * Perform ping (provider-specific)
   */
  protected abstract performPing(): Promise<void>;

  /**
   * Called after successful connection (optional hook)
   */
  protected async onConnected(): Promise<void> {
    // Override in subclass if needed
  }

  /**
   * Called after disconnection (optional hook)
   */
  protected async onDisconnected(): Promise<void> {
    // Override in subclass if needed
  }

  // ==================== Protected Helper Methods ====================

  /**
   * Setup WebSocket event handlers
   */
  protected setupWebSocketHandlers(): void {
    if (!this.ws) return;

    this.ws.onopen = () => {
      this.log('WebSocket connection opened');
    };

    this.ws.onmessage = (event: MessageEvent) => {
      this.heartbeatMonitor.beat();
      this._metrics.lastMessageAt = Date.now();
      this.performanceMetrics.recordMessage(event.data?.length);

      try {
        this.handleMessage(event);
      } catch (error) {
        console.error(`[${this.provider}] Error handling message:`, error);
        this.addError({
          timestamp: Date.now(),
          error: `Message handling error: ${error}`
        });
      }
    };

    this.ws.onerror = (event: Event) => {
      this.log('WebSocket error', event);
      this.addError({
        timestamp: Date.now(),
        error: 'WebSocket error occurred'
      });
      this.setStatus(ConnectionStatus.ERROR);
    };

    this.ws.onclose = (event: CloseEvent) => {
      this.log(`WebSocket closed: ${event.code} - ${event.reason}`);

      if (this._status !== ConnectionStatus.DISCONNECTED) {
        // Unexpected close, attempt reconnection
        this.handleUnexpectedClose(event);
      }
    };
  }

  /**
   * Wait for WebSocket connection to open
   */
  protected waitForConnection(timeout: number = 10000): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.ws) {
        reject(new Error('WebSocket is null'));
        return;
      }

      const timer = setTimeout(() => {
        reject(new Error('Connection timeout'));
      }, timeout);

      this.ws.addEventListener('open', () => {
        clearTimeout(timer);
        resolve();
      }, { once: true });

      this.ws.addEventListener('error', (error) => {
        clearTimeout(timer);
        reject(error);
      }, { once: true });
    });
  }

  /**
   * Handle unexpected connection close
   */
  protected async handleUnexpectedClose(event: CloseEvent): Promise<void> {
    this.log(`Unexpected close: ${event.code} - ${event.reason}`);

    if (this.reconnectionManager.shouldAttempt()) {
      await this.scheduleReconnection();
    } else {
      this.setStatus(ConnectionStatus.ERROR);
      this.addError({
        timestamp: Date.now(),
        error: 'Max reconnection attempts reached',
        code: event.code
      });
    }
  }

  /**
   * Schedule reconnection attempt
   */
  protected async scheduleReconnection(): Promise<void> {
    const delay = this.reconnectionManager.getNextDelay();
    this.setStatus(ConnectionStatus.RECONNECTING);

    this.log(`Scheduling reconnection in ${delay}ms (attempt ${this.reconnectionManager.getAttempts() + 1})`);

    await new Promise(resolve => setTimeout(resolve, delay));

    if (this.config) {
      try {
        await this.connect(this.config);

        // Resubscribe to previous topics
        await this.resubscribe();
      } catch (error) {
        console.error(`[${this.provider}] Reconnection failed:`, error);
      }
    }
  }

  /**
   * Resubscribe to all topics after reconnection
   */
  protected async resubscribe(): Promise<void> {
    const topics = Array.from(this.subscriptions);
    this.subscriptions.clear();

    for (const topic of topics) {
      try {
        await this.subscribe(topic);
      } catch (error) {
        console.error(`[${this.provider}] Failed to resubscribe to ${topic}:`, error);
      }
    }
  }

  /**
   * Handle stale connection (heartbeat timeout)
   */
  protected async handleStaleConnection(): Promise<void> {
    // Prevent simultaneous reconnection attempts
    if (this._status === ConnectionStatus.RECONNECTING || this._status === ConnectionStatus.CONNECTING) {
      this.log('Already reconnecting, skipping stale connection handler');
      return;
    }

    this.log('Connection appears stale, forcing reconnect');

    await this.disconnect();

    if (this.config && this.reconnectionManager.shouldAttempt()) {
      await this.scheduleReconnection();
    }
  }

  /**
   * Notify message to all callbacks
   */
  protected notifyMessage(message: NormalizedMessage): void {
    this.messageCallbacks.forEach(callback => {
      try {
        callback(message);
      } catch (error) {
        console.error(`[${this.provider}] Error in message callback:`, error);
      }
    });
  }

  /**
   * Notify error to all callbacks
   */
  protected notifyError(error: ConnectionError): void {
    this.errorCallbacks.forEach(callback => {
      try {
        callback(error);
      } catch (err) {
        console.error(`[${this.provider}] Error in error callback:`, err);
      }
    });
  }

  /**
   * Set connection status and notify listeners
   */
  protected setStatus(status: ConnectionStatus): void {
    if (this._status !== status) {
      this._status = status;
      this._metrics.status = status;

      this.statusCallbacks.forEach(callback => {
        try {
          callback(status);
        } catch (error) {
          console.error(`[${this.provider}] Error in status callback:`, error);
        }
      });

      this.log(`Status changed to: ${status}`);
    }
  }

  /**
   * Add error to metrics
   */
  protected addError(error: ConnectionError): void {
    this._metrics.errors.push(error);
    this.performanceMetrics.recordError();

    // Keep only last 50 errors
    if (this._metrics.errors.length > 50) {
      this._metrics.errors = this._metrics.errors.slice(-50);
    }
  }

  /**
   * Log message (if logging enabled)
   */
  protected log(...args: any[]): void {
    if (this.enableLogging) {
      console.log(`[${this.provider}]`, ...args);
    }
  }

  /**
   * Enable/disable logging
   */
  setLogging(enabled: boolean): void {
    this.enableLogging = enabled;
  }
}
