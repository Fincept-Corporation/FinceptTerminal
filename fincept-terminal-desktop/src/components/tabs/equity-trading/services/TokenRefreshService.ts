/**
 * Background Token Refresh Service
 * Automatically refreshes broker access tokens before expiration
 * Zero user intervention required
 */

import { BrokerType, AuthStatus, BrokerStatus } from '../core/types';
import { BaseBrokerAdapter } from '../adapters/BaseBrokerAdapter';

interface RefreshConfig {
  brokerId: BrokerType;
  adapter: BaseBrokerAdapter;
  refreshInterval: number; // milliseconds
  retryAttempts: number;
  retryDelay: number; // milliseconds
}

interface RefreshSchedule {
  brokerId: BrokerType;
  nextRefresh: Date;
  intervalId?: NodeJS.Timeout;
  isRunning: boolean;
}

export class TokenRefreshService {
  private static instance: TokenRefreshService;
  private configs: Map<BrokerType, RefreshConfig> = new Map();
  private schedules: Map<BrokerType, RefreshSchedule> = new Map();
  private listeners: Set<(status: AuthStatus) => void> = new Set();

  // Default refresh intervals (in milliseconds)
  private readonly DEFAULT_INTERVALS = {
    fyers: 6 * 60 * 60 * 1000,  // 6 hours (token valid for 24h, refresh at 25%)
    kite: 12 * 60 * 60 * 1000,  // 12 hours (token valid for 1 day, refresh at 50%)
  };

  private readonly DEFAULT_RETRY_ATTEMPTS = 3;
  private readonly DEFAULT_RETRY_DELAY = 5 * 60 * 1000; // 5 minutes

  private constructor() {
    // Singleton pattern
  }

  static getInstance(): TokenRefreshService {
    if (!TokenRefreshService.instance) {
      TokenRefreshService.instance = new TokenRefreshService();
    }
    return TokenRefreshService.instance;
  }

  /**
   * Register a broker for automatic token refresh
   */
  registerBroker(
    brokerId: BrokerType,
    adapter: BaseBrokerAdapter,
    customInterval?: number
  ): void {
    const refreshInterval = customInterval || this.DEFAULT_INTERVALS[brokerId] || 6 * 60 * 60 * 1000;

    const config: RefreshConfig = {
      brokerId,
      adapter,
      refreshInterval,
      retryAttempts: this.DEFAULT_RETRY_ATTEMPTS,
      retryDelay: this.DEFAULT_RETRY_DELAY,
    };

    this.configs.set(brokerId, config);
    this.log(`[TokenRefresh] Registered broker: ${brokerId}`);
  }

  /**
   * Start automatic refresh for a broker
   */
  async startAutoRefresh(brokerId: BrokerType): Promise<void> {
    const config = this.configs.get(brokerId);
    if (!config) {
      throw new Error(`Broker ${brokerId} not registered`);
    }

    // Stop existing schedule if any
    this.stopAutoRefresh(brokerId);

    // Initial auth check
    const isAuthenticated = config.adapter.isAuthenticated();
    if (!isAuthenticated) {
      this.log(`[TokenRefresh] ${brokerId} not authenticated, attempting initial auth...`);
      const authStatus = await config.adapter.authenticate();
      this.notifyListeners(authStatus);

      if (!authStatus.authenticated) {
        this.log(`[TokenRefresh] ${brokerId} initial auth failed`, 'error');
        return;
      }
    }

    // Schedule periodic refresh
    const intervalId = setInterval(async () => {
      await this.performRefresh(brokerId);
    }, config.refreshInterval);

    const schedule: RefreshSchedule = {
      brokerId,
      nextRefresh: new Date(Date.now() + config.refreshInterval),
      intervalId,
      isRunning: true,
    };

    this.schedules.set(brokerId, schedule);
    this.log(`[TokenRefresh] Auto-refresh started for ${brokerId}, next refresh at ${schedule.nextRefresh.toLocaleString()}`);
  }

  /**
   * Stop automatic refresh for a broker
   */
  stopAutoRefresh(brokerId: BrokerType): void {
    const schedule = this.schedules.get(brokerId);
    if (schedule?.intervalId) {
      clearInterval(schedule.intervalId);
      schedule.isRunning = false;
      this.log(`[TokenRefresh] Auto-refresh stopped for ${brokerId}`);
    }
  }

  /**
   * Manually trigger token refresh for a broker
   */
  async manualRefresh(brokerId: BrokerType): Promise<boolean> {
    return await this.performRefresh(brokerId);
  }

  /**
   * Perform token refresh with retry logic
   */
  private async performRefresh(brokerId: BrokerType, attempt: number = 1): Promise<boolean> {
    const config = this.configs.get(brokerId);
    if (!config) {
      this.log(`[TokenRefresh] Broker ${brokerId} not found`, 'error');
      return false;
    }

    this.log(`[TokenRefresh] Refreshing token for ${brokerId} (attempt ${attempt}/${config.retryAttempts})`);

    try {
      const success = await config.adapter.refreshToken();

      if (success) {
        this.log(`[TokenRefresh] ${brokerId} token refreshed successfully`);
        const authStatus = config.adapter.getAuthStatus();
        this.notifyListeners(authStatus);

        // Update next refresh time
        const schedule = this.schedules.get(brokerId);
        if (schedule) {
          schedule.nextRefresh = new Date(Date.now() + config.refreshInterval);
        }

        return true;
      } else {
        throw new Error('Refresh returned false');
      }
    } catch (error) {
      this.log(`[TokenRefresh] ${brokerId} refresh failed: ${error}`, 'error');

      // Retry logic
      if (attempt < config.retryAttempts) {
        this.log(`[TokenRefresh] Retrying ${brokerId} in ${config.retryDelay / 1000}s...`);

        await this.sleep(config.retryDelay);
        return await this.performRefresh(brokerId, attempt + 1);
      } else {
        this.log(`[TokenRefresh] ${brokerId} refresh failed after ${config.retryAttempts} attempts`, 'error');

        // Notify listeners of auth failure
        const failedStatus: AuthStatus = {
          brokerId,
          status: BrokerStatus.ERROR,
          authenticated: false,
          error: 'Token refresh failed',
        };
        this.notifyListeners(failedStatus);

        return false;
      }
    }
  }

  /**
   * Subscribe to auth status changes
   */
  onAuthStatusChange(callback: (status: AuthStatus) => void): () => void {
    this.listeners.add(callback);

    // Return unsubscribe function
    return () => {
      this.listeners.delete(callback);
    };
  }

  /**
   * Notify all listeners of auth status change
   */
  private notifyListeners(status: AuthStatus): void {
    this.listeners.forEach(listener => {
      try {
        listener(status);
      } catch (error) {
        this.log(`[TokenRefresh] Listener error: ${error}`, 'error');
      }
    });
  }

  /**
   * Get refresh status for a broker
   */
  getRefreshStatus(brokerId: BrokerType): RefreshSchedule | undefined {
    return this.schedules.get(brokerId);
  }

  /**
   * Get all refresh statuses
   */
  getAllRefreshStatuses(): Map<BrokerType, RefreshSchedule> {
    return new Map(this.schedules);
  }

  /**
   * Stop all auto-refresh schedules
   */
  stopAll(): void {
    this.schedules.forEach((_, brokerId) => {
      this.stopAutoRefresh(brokerId);
    });
  }

  /**
   * Start all registered brokers
   */
  async startAll(): Promise<void> {
    const promises = Array.from(this.configs.keys()).map(brokerId =>
      this.startAutoRefresh(brokerId)
    );
    await Promise.allSettled(promises);
  }

  /**
   * Utility: Sleep function
   */
  private sleep(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Logging utility
   */
  private log(message: string, level: 'info' | 'error' = 'info'): void {
    const timestamp = new Date().toISOString();
    if (level === 'error') {
      console.error(`[${timestamp}] ${message}`);
    } else {
      console.log(`[${timestamp}] ${message}`);
    }
  }
}

// Export singleton instance
export const tokenRefreshService = TokenRefreshService.getInstance();
