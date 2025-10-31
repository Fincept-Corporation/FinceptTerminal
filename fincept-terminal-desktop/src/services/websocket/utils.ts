// File: src/services/websocket/utils.ts
// Utility functions for WebSocket management (reconnection, rate limiting, etc.)

import { ReconnectionPolicy, RateLimitConfig } from './types';

/**
 * Exponential backoff calculator for reconnection delays
 */
export class ReconnectionManager {
  private attempts: number = 0;
  private lastAttemptTime: number = 0;
  private lastSuccessTime: number = 0;
  private policy: ReconnectionPolicy;

  constructor(policy: ReconnectionPolicy) {
    this.policy = policy;
  }

  /**
   * Calculate next reconnection delay using exponential backoff
   */
  getNextDelay(): number {
    if (!this.policy.enabled) {
      return 0;
    }

    // Reset attempts if enough time has passed since last success
    if (
      this.lastSuccessTime > 0 &&
      Date.now() - this.lastSuccessTime > this.policy.resetAfterMs
    ) {
      this.reset();
    }

    const delay = Math.min(
      this.policy.initialDelayMs * Math.pow(this.policy.backoffMultiplier, this.attempts),
      this.policy.maxDelayMs
    );

    return delay;
  }

  /**
   * Record a reconnection attempt
   */
  recordAttempt(): void {
    this.attempts++;
    this.lastAttemptTime = Date.now();
  }

  /**
   * Record successful connection
   */
  recordSuccess(): void {
    this.lastSuccessTime = Date.now();
    this.reset();
  }

  /**
   * Check if should attempt reconnection
   */
  shouldAttempt(): boolean {
    return this.policy.enabled && this.attempts < this.policy.maxAttempts;
  }

  /**
   * Get current attempt count
   */
  getAttempts(): number {
    return this.attempts;
  }

  /**
   * Reset attempt counter
   */
  reset(): void {
    this.attempts = 0;
  }

  /**
   * Update policy at runtime
   */
  updatePolicy(policy: Partial<ReconnectionPolicy>): void {
    this.policy = { ...this.policy, ...policy };
  }
}

/**
 * Rate limiter using token bucket algorithm
 */
export class RateLimiter {
  private tokens: number;
  private maxTokens: number;
  private refillRate: number; // tokens per second
  private lastRefill: number;
  private config: RateLimitConfig;
  private messageQueue: Array<() => void> = [];

  constructor(config: RateLimitConfig) {
    this.config = config;
    this.maxTokens = config.maxMessagesPerSecond;
    this.tokens = this.maxTokens;
    this.refillRate = config.maxMessagesPerSecond;
    this.lastRefill = Date.now();
  }

  /**
   * Check if action is allowed (consumes token if available)
   */
  tryConsume(): boolean {
    if (!this.config.enabled) {
      return true;
    }

    this.refill();

    if (this.tokens >= 1) {
      this.tokens -= 1;
      return true;
    }

    return false;
  }

  /**
   * Refill tokens based on time elapsed
   */
  private refill(): void {
    const now = Date.now();
    const timePassed = (now - this.lastRefill) / 1000; // seconds
    const tokensToAdd = timePassed * this.refillRate;

    this.tokens = Math.min(this.maxTokens, this.tokens + tokensToAdd);
    this.lastRefill = now;
  }

  /**
   * Wait for available token (returns delay in ms)
   */
  getWaitTime(): number {
    if (!this.config.enabled) {
      return 0;
    }

    this.refill();

    if (this.tokens >= 1) {
      return 0;
    }

    // Calculate time until next token is available
    const tokensNeeded = 1 - this.tokens;
    const waitTime = (tokensNeeded / this.refillRate) * 1000;

    return Math.max(waitTime, this.config.throttleMs);
  }

  /**
   * Execute action with rate limiting
   */
  async execute<T>(action: () => Promise<T>): Promise<T> {
    if (!this.config.enabled) {
      return action();
    }

    const waitTime = this.getWaitTime();

    if (waitTime > 0) {
      await this.sleep(waitTime);
    }

    this.tryConsume();
    return action();
  }

  /**
   * Queue action for later execution when tokens available
   */
  queue(action: () => void): void {
    this.messageQueue.push(action);
    this.processQueue();
  }

  /**
   * Process queued actions
   */
  private async processQueue(): Promise<void> {
    while (this.messageQueue.length > 0) {
      if (this.tryConsume()) {
        const action = this.messageQueue.shift();
        if (action) {
          try {
            action();
          } catch (error) {
            console.error('[RateLimiter] Error executing queued action:', error);
          }
        }
      } else {
        // Wait for next token
        const waitTime = this.getWaitTime();
        await this.sleep(waitTime);
      }
    }
  }

  /**
   * Get current token count
   */
  getAvailableTokens(): number {
    this.refill();
    return Math.floor(this.tokens);
  }

  /**
   * Reset rate limiter
   */
  reset(): void {
    this.tokens = this.maxTokens;
    this.lastRefill = Date.now();
    this.messageQueue = [];
  }

  /**
   * Update configuration at runtime
   */
  updateConfig(config: Partial<RateLimitConfig>): void {
    this.config = { ...this.config, ...config };
    this.maxTokens = this.config.maxMessagesPerSecond;
    this.refillRate = this.config.maxMessagesPerSecond;
  }

  private sleep(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}

/**
 * Circuit breaker for handling repeated failures
 */
export class CircuitBreaker {
  private failures: number = 0;
  private lastFailureTime: number = 0;
  private state: 'closed' | 'open' | 'half-open' = 'closed';
  private successCount: number = 0;

  constructor(
    private failureThreshold: number = 5,
    private timeout: number = 60000, // 1 minute
    private successThreshold: number = 2
  ) {}

  /**
   * Record successful operation
   */
  recordSuccess(): void {
    this.failures = 0;
    this.lastFailureTime = 0;

    if (this.state === 'half-open') {
      this.successCount++;
      if (this.successCount >= this.successThreshold) {
        this.state = 'closed';
        this.successCount = 0;
      }
    }
  }

  /**
   * Record failed operation
   */
  recordFailure(): void {
    this.failures++;
    this.lastFailureTime = Date.now();

    if (this.failures >= this.failureThreshold) {
      this.state = 'open';
    }

    if (this.state === 'half-open') {
      this.state = 'open';
      this.successCount = 0;
    }
  }

  /**
   * Check if operation is allowed
   */
  isAllowed(): boolean {
    if (this.state === 'closed') {
      return true;
    }

    if (this.state === 'open') {
      // Check if timeout has passed
      if (Date.now() - this.lastFailureTime >= this.timeout) {
        this.state = 'half-open';
        this.successCount = 0;
        return true;
      }
      return false;
    }

    // half-open state
    return true;
  }

  /**
   * Get current state
   */
  getState(): 'closed' | 'open' | 'half-open' {
    return this.state;
  }

  /**
   * Reset circuit breaker
   */
  reset(): void {
    this.failures = 0;
    this.lastFailureTime = 0;
    this.state = 'closed';
    this.successCount = 0;
  }
}

/**
 * Message throttler for preventing UI overload
 */
export class MessageThrottler {
  private lastExecutionTime: number = 0;
  private pendingMessage: any = null;
  private timeoutId: NodeJS.Timeout | null = null;

  constructor(private throttleMs: number = 100) {}

  /**
   * Throttle message delivery
   */
  throttle<T>(message: T, callback: (message: T) => void): void {
    const now = Date.now();
    const timeSinceLastExecution = now - this.lastExecutionTime;

    if (timeSinceLastExecution >= this.throttleMs) {
      // Execute immediately
      callback(message);
      this.lastExecutionTime = now;
      this.pendingMessage = null;

      // Clear any pending timeout
      if (this.timeoutId) {
        clearTimeout(this.timeoutId);
        this.timeoutId = null;
      }
    } else {
      // Store message and schedule execution
      this.pendingMessage = message;

      if (!this.timeoutId) {
        const delay = this.throttleMs - timeSinceLastExecution;
        this.timeoutId = setTimeout(() => {
          if (this.pendingMessage) {
            callback(this.pendingMessage);
            this.lastExecutionTime = Date.now();
            this.pendingMessage = null;
          }
          this.timeoutId = null;
        }, delay);
      }
    }
  }

  /**
   * Flush any pending message immediately
   */
  flush<T>(callback: (message: T) => void): void {
    if (this.pendingMessage) {
      callback(this.pendingMessage);
      this.pendingMessage = null;
      this.lastExecutionTime = Date.now();
    }

    if (this.timeoutId) {
      clearTimeout(this.timeoutId);
      this.timeoutId = null;
    }
  }

  /**
   * Clear throttler
   */
  clear(): void {
    this.pendingMessage = null;
    if (this.timeoutId) {
      clearTimeout(this.timeoutId);
      this.timeoutId = null;
    }
  }
}

/**
 * Heartbeat monitor for detecting stale connections
 */
export class HeartbeatMonitor {
  private lastHeartbeat: number = Date.now();
  private timeoutId: NodeJS.Timeout | null = null;
  private onTimeout?: () => void;

  constructor(
    private intervalMs: number = 30000, // 30 seconds
    private timeoutMs: number = 60000  // 1 minute
  ) {}

  /**
   * Start heartbeat monitoring
   */
  start(onTimeout: () => void): void {
    this.onTimeout = onTimeout;
    this.lastHeartbeat = Date.now();
    this.scheduleCheck();
  }

  /**
   * Record heartbeat (call this when receiving messages)
   */
  beat(): void {
    this.lastHeartbeat = Date.now();
  }

  /**
   * Stop monitoring
   */
  stop(): void {
    if (this.timeoutId) {
      clearTimeout(this.timeoutId);
      this.timeoutId = null;
    }
    this.onTimeout = undefined;
  }

  /**
   * Schedule next heartbeat check
   */
  private scheduleCheck(): void {
    if (this.timeoutId) {
      clearTimeout(this.timeoutId);
    }

    this.timeoutId = setTimeout(() => {
      const timeSinceLastBeat = Date.now() - this.lastHeartbeat;

      if (timeSinceLastBeat > this.timeoutMs) {
        // Connection appears stale
        if (this.onTimeout) {
          this.onTimeout();
        }
      } else {
        // Schedule next check
        this.scheduleCheck();
      }
    }, this.intervalMs);
  }

  /**
   * Get time since last heartbeat
   */
  getTimeSinceLastBeat(): number {
    return Date.now() - this.lastHeartbeat;
  }

  /**
   * Check if connection appears healthy
   */
  isHealthy(): boolean {
    return this.getTimeSinceLastBeat() < this.timeoutMs;
  }
}

/**
 * Performance metrics collector
 */
export class PerformanceMetrics {
  private messageCount: number = 0;
  private errorCount: number = 0;
  private startTime: number = Date.now();
  private messageSizes: number[] = [];
  private latencies: number[] = [];
  private maxSamples: number = 100;

  /**
   * Record message received
   */
  recordMessage(size?: number): void {
    this.messageCount++;

    if (size !== undefined) {
      this.messageSizes.push(size);
      if (this.messageSizes.length > this.maxSamples) {
        this.messageSizes.shift();
      }
    }
  }

  /**
   * Record error
   */
  recordError(): void {
    this.errorCount++;
  }

  /**
   * Record latency measurement
   */
  recordLatency(latencyMs: number): void {
    this.latencies.push(latencyMs);
    if (this.latencies.length > this.maxSamples) {
      this.latencies.shift();
    }
  }

  /**
   * Get metrics summary
   */
  getMetrics() {
    const uptime = Date.now() - this.startTime;
    const uptimeSeconds = uptime / 1000;

    return {
      messageCount: this.messageCount,
      errorCount: this.errorCount,
      messagesPerSecond: uptimeSeconds > 0 ? this.messageCount / uptimeSeconds : 0,
      errorRate: this.messageCount > 0 ? this.errorCount / this.messageCount : 0,
      averageMessageSize: this.average(this.messageSizes),
      averageLatency: this.average(this.latencies),
      uptime,
      uptimeHours: uptime / (1000 * 60 * 60)
    };
  }

  /**
   * Reset metrics
   */
  reset(): void {
    this.messageCount = 0;
    this.errorCount = 0;
    this.startTime = Date.now();
    this.messageSizes = [];
    this.latencies = [];
  }

  private average(arr: number[]): number {
    if (arr.length === 0) return 0;
    return arr.reduce((a, b) => a + b, 0) / arr.length;
  }
}

/**
 * Retry helper with exponential backoff
 */
export async function retryWithBackoff<T>(
  fn: () => Promise<T>,
  maxRetries: number = 3,
  initialDelay: number = 1000,
  maxDelay: number = 10000,
  multiplier: number = 2
): Promise<T> {
  let lastError: Error | undefined;

  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    try {
      return await fn();
    } catch (error) {
      lastError = error instanceof Error ? error : new Error(String(error));

      if (attempt < maxRetries) {
        const delay = Math.min(initialDelay * Math.pow(multiplier, attempt), maxDelay);
        await new Promise(resolve => setTimeout(resolve, delay));
      }
    }
  }

  throw lastError || new Error('Max retries exceeded');
}

/**
 * Generate unique ID
 */
export function generateId(prefix: string = 'ws'): string {
  return `${prefix}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
}

/**
 * Safe JSON parse with fallback
 */
export function safeJsonParse<T = any>(json: string, fallback: T): T {
  try {
    return JSON.parse(json);
  } catch {
    return fallback;
  }
}

/**
 * Debounce function
 */
export function debounce<T extends (...args: any[]) => any>(
  func: T,
  wait: number
): (...args: Parameters<T>) => void {
  let timeout: NodeJS.Timeout | null = null;

  return function (this: any, ...args: Parameters<T>) {
    const context = this;

    if (timeout) {
      clearTimeout(timeout);
    }

    timeout = setTimeout(() => {
      func.apply(context, args);
    }, wait);
  };
}
