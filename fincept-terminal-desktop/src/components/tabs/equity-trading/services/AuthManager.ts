/**
 * Auth Manager
 * Handles credential storage, encryption, and broker authentication
 * Provides seamless background authentication
 */

import { sqliteService } from '../../../../services/sqliteService';
import { BrokerType, BrokerCredentials, AuthStatus, BrokerStatus } from '../core/types';
import { BaseBrokerAdapter } from '../adapters/BaseBrokerAdapter';
import { tokenRefreshService } from './TokenRefreshService';

interface StoredCredential {
  service_name: string;
  username: string;
  api_key: string;
  api_secret: string;
  password?: string;
  additional_data?: string;
}

export class AuthManager {
  private static instance: AuthManager;
  private adapters: Map<BrokerType, BaseBrokerAdapter> = new Map();
  private authStatuses: Map<BrokerType, AuthStatus> = new Map();
  private statusChangeListeners: Set<(brokerId: BrokerType, status: AuthStatus) => void> = new Set();

  private constructor() {
    // Singleton pattern
  }

  static getInstance(): AuthManager {
    if (!AuthManager.instance) {
      AuthManager.instance = new AuthManager();
    }
    return AuthManager.instance;
  }

  /**
   * Register a broker adapter
   */
  registerAdapter(brokerId: BrokerType, adapter: BaseBrokerAdapter): void {
    this.adapters.set(brokerId, adapter);
    console.log(`[AuthManager] Registered adapter for ${brokerId}`);
  }

  /**
   * Save broker credentials to encrypted storage
   */
  async saveCredentials(credentials: BrokerCredentials): Promise<void> {
    const { brokerId, apiKey, apiSecret, accessToken, additionalData } = credentials;

    const stored: StoredCredential = {
      service_name: brokerId,
      username: `${brokerId}_user`,
      api_key: apiKey,
      api_secret: apiSecret,
      password: accessToken,
      additional_data: additionalData ? JSON.stringify(additionalData) : undefined,
    };

    try {
      // Note: sqliteService.saveCredential expects the parameter to be named 'cred'
      // The function signature is: saveCredential(cred: Omit<Credential, 'created_at' | 'updated_at'>)
      const result = await sqliteService.saveCredential(stored);
      console.log(`[AuthManager] Credentials saved for ${brokerId}:`, result);
    } catch (error) {
      console.error(`[AuthManager] Failed to save credentials for ${brokerId}:`, error);
      throw error;
    }
  }

  /**
   * Load broker credentials from storage
   */
  async loadCredentials(brokerId: BrokerType): Promise<BrokerCredentials | null> {
    try {
      const stored = await sqliteService.getCredentialByService(brokerId);

      if (!stored || !stored.api_key || !stored.api_secret) {
        console.log(`[AuthManager] No credentials found for ${brokerId}`);
        return null;
      }

      const credentials: BrokerCredentials = {
        brokerId,
        apiKey: stored.api_key,
        apiSecret: stored.api_secret,
        accessToken: stored.password,
        additionalData: stored.additional_data ? JSON.parse(stored.additional_data) : {},
      };

      return credentials;
    } catch (error) {
      console.error(`[AuthManager] Failed to load credentials for ${brokerId}:`, error);
      return null;
    }
  }

  /**
   * Delete broker credentials
   */
  async deleteCredentials(brokerId: BrokerType): Promise<void> {
    try {
      // SQLite service doesn't have a delete method, so we'll clear the fields
      await sqliteService.saveCredential({
        service_name: brokerId,
        username: '',
        api_key: '',
        api_secret: '',
        password: '',
        additional_data: '',
      });
      console.log(`[AuthManager] Credentials deleted for ${brokerId}`);
    } catch (error) {
      console.error(`[AuthManager] Failed to delete credentials for ${brokerId}:`, error);
      throw error;
    }
  }

  /**
   * Initialize and authenticate a broker
   * This is the main entry point for setting up a broker
   */
  async initializeBroker(brokerId: BrokerType): Promise<AuthStatus> {
    console.log(`[AuthManager] Initializing broker: ${brokerId}`);

    const adapter = this.adapters.get(brokerId);
    if (!adapter) {
      const error = `Adapter not registered for ${brokerId}`;
      console.error(`[AuthManager] ${error}`);
      return this.createErrorStatus(brokerId, error);
    }

    // Load stored credentials
    const credentials = await this.loadCredentials(brokerId);
    if (!credentials) {
      // Not an error - just not configured yet
      console.log(`[AuthManager] No credentials found for ${brokerId}, skipping initialization`);
      return this.createErrorStatus(brokerId, 'Not configured');
    }

    try {
      // Initialize adapter with credentials
      const initialized = await adapter.initialize(credentials);
      if (!initialized) {
        throw new Error('Adapter initialization failed');
      }

      // Authenticate
      const authStatus = await adapter.authenticate();

      // Store auth status
      this.authStatuses.set(brokerId, authStatus);

      // Notify listeners of auth status change
      this.notifyStatusChangeListeners(brokerId, authStatus);

      if (authStatus.authenticated) {
        // Start automatic token refresh
        tokenRefreshService.registerBroker(brokerId, adapter);
        await tokenRefreshService.startAutoRefresh(brokerId);

        console.log(`[AuthManager] ${brokerId} initialized and authenticated successfully`);
      } else {
        console.error(`[AuthManager] ${brokerId} authentication failed:`, authStatus.error);
      }

      return authStatus;
    } catch (error: any) {
      const errorMsg = error.message || 'Authentication failed';
      console.error(`[AuthManager] ${brokerId} initialization error:`, error);
      return this.createErrorStatus(brokerId, errorMsg);
    }
  }

  /**
   * Authenticate all registered brokers
   */
  async initializeAllBrokers(): Promise<Map<BrokerType, AuthStatus>> {
    const results = new Map<BrokerType, AuthStatus>();

    for (const brokerId of this.adapters.keys()) {
      const status = await this.initializeBroker(brokerId);
      results.set(brokerId, status);
    }

    return results;
  }

  /**
   * Get authentication status for a broker
   */
  getAuthStatus(brokerId: BrokerType): AuthStatus | null {
    const cached = this.authStatuses.get(brokerId);
    if (cached) return cached;

    const adapter = this.adapters.get(brokerId);
    if (adapter) {
      return adapter.getAuthStatus();
    }

    return null;
  }

  /**
   * Get all authentication statuses
   */
  getAllAuthStatuses(): Map<BrokerType, AuthStatus> {
    const statuses = new Map<BrokerType, AuthStatus>();

    for (const [brokerId, adapter] of this.adapters) {
      const status = this.authStatuses.get(brokerId) || adapter.getAuthStatus();
      statuses.set(brokerId, status);
    }

    return statuses;
  }

  /**
   * Check if broker is authenticated
   */
  isAuthenticated(brokerId: BrokerType): boolean {
    const adapter = this.adapters.get(brokerId);
    return adapter?.isAuthenticated() || false;
  }

  /**
   * Disconnect a broker
   */
  async disconnectBroker(brokerId: BrokerType): Promise<void> {
    const adapter = this.adapters.get(brokerId);
    if (!adapter) {
      console.warn(`[AuthManager] No adapter found for ${brokerId}`);
      return;
    }

    try {
      // Stop auto-refresh
      tokenRefreshService.stopAutoRefresh(brokerId);

      // Disconnect adapter
      await adapter.disconnect();

      // Update status
      const disconnectedStatus: AuthStatus = {
        brokerId,
        status: BrokerStatus.DISCONNECTED,
        authenticated: false,
      };
      this.authStatuses.set(brokerId, disconnectedStatus);

      // Notify listeners
      this.notifyStatusChangeListeners(brokerId, disconnectedStatus);

      console.log(`[AuthManager] ${brokerId} disconnected`);
    } catch (error) {
      console.error(`[AuthManager] Failed to disconnect ${brokerId}:`, error);
      throw error;
    }
  }

  /**
   * Disconnect all brokers
   */
  async disconnectAll(): Promise<void> {
    const promises = Array.from(this.adapters.keys()).map(brokerId =>
      this.disconnectBroker(brokerId)
    );
    await Promise.allSettled(promises);
  }

  /**
   * Get broker adapter
   */
  getAdapter(brokerId: BrokerType): BaseBrokerAdapter | undefined {
    return this.adapters.get(brokerId);
  }

  /**
   * Create error status object
   */
  private createErrorStatus(brokerId: BrokerType, error: string): AuthStatus {
    return {
      brokerId,
      status: BrokerStatus.ERROR,
      authenticated: false,
      error,
    };
  }

  /**
   * Subscribe to auth status changes across all brokers
   */
  onAuthStatusChange(callback: (brokerId: BrokerType, status: AuthStatus) => void): () => void {
    // Add to our own listeners
    this.statusChangeListeners.add(callback);

    // Also subscribe to token refresh service updates
    const unsubscribeTokenRefresh = tokenRefreshService.onAuthStatusChange((status) => {
      this.authStatuses.set(status.brokerId, status);
      callback(status.brokerId, status);
    });

    // Return unsubscribe function
    return () => {
      this.statusChangeListeners.delete(callback);
      unsubscribeTokenRefresh();
    };
  }

  /**
   * Notify all listeners of auth status change
   */
  private notifyStatusChangeListeners(brokerId: BrokerType, status: AuthStatus): void {
    this.statusChangeListeners.forEach(listener => {
      try {
        listener(brokerId, status);
      } catch (error) {
        console.error(`[AuthManager] Listener notification error:`, error);
      }
    });
  }
}

// Export singleton instance
export const authManager = AuthManager.getInstance();
