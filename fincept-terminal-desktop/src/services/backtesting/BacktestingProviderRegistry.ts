/**
 * Backtesting Provider Registry
 * Central registry for managing all backtesting providers
 * Enables easy provider switching and discovery
 */

import { IBacktestingProvider } from './interfaces/IBacktestingProvider';
import { ProviderInfo, ProviderConfig } from './interfaces/types';

/**
 * BacktestingProviderRegistry
 *
 * Singleton registry that manages all available backtesting providers.
 * Provides provider registration, activation, and discovery.
 */
export class BacktestingProviderRegistry {
  private providers: Map<string, IBacktestingProvider> = new Map();
  private activeProviderId: string | null = null;
  private configs: Map<string, ProviderConfig> = new Map();
  private listeners: Set<RegistryListener> = new Set();

  // ============================================================================
  // Provider Registration
  // ============================================================================

  /**
   * Register a new provider
   *
   * @param provider Provider instance implementing IBacktestingProvider
   * @throws Error if provider with same name already exists
   */
  registerProvider(provider: IBacktestingProvider): void {
    if (this.providers.has(provider.name)) {
      throw new Error(`Provider "${provider.name}" is already registered`);
    }

    this.providers.set(provider.name, provider);
    this.notifyListeners('provider:registered', { provider: provider.name });

    console.log(`✓ Registered backtesting provider: ${provider.name}`);
  }

  /**
   * Unregister a provider
   *
   * @param name Provider name
   * @returns true if provider was found and removed
   */
  unregisterProvider(name: string): boolean {
    const provider = this.providers.get(name);
    if (!provider) {
      return false;
    }

    // Disconnect provider before removing
    provider.disconnect().catch(console.error);

    this.providers.delete(name);
    this.configs.delete(name);

    // If this was the active provider, clear it
    if (this.activeProviderId === name) {
      this.activeProviderId = null;
    }

    this.notifyListeners('provider:unregistered', { provider: name });

    console.log(`✓ Unregistered backtesting provider: ${name}`);
    return true;
  }

  // ============================================================================
  // Provider Selection
  // ============================================================================

  /**
   * Set active provider
   *
   * @param name Provider name
   * @throws Error if provider not found
   */
  async setActiveProvider(name: string): Promise<void> {
    const provider = this.providers.get(name);
    if (!provider) {
      throw new Error(`Provider "${name}" not found`);
    }

    // Test connection before activating
    const testResult = await provider.testConnection();
    if (!testResult.success) {
      throw new Error(
        `Cannot activate provider "${name}": ${testResult.error || 'Connection test failed'}`
      );
    }

    const previousProvider = this.activeProviderId;
    this.activeProviderId = name;

    this.notifyListeners('provider:activated', {
      provider: name,
      previous: previousProvider,
    });

    console.log(`✓ Activated backtesting provider: ${name}`);
  }

  /**
   * Get active provider
   *
   * @returns Active provider instance or null if none active
   */
  getActiveProvider(): IBacktestingProvider | null {
    if (!this.activeProviderId) {
      return null;
    }
    return this.providers.get(this.activeProviderId) || null;
  }

  /**
   * Get active provider name
   *
   * @returns Active provider name or null
   */
  getActiveProviderName(): string | null {
    return this.activeProviderId;
  }

  /**
   * Check if a provider is active
   *
   * @param name Provider name
   * @returns true if this provider is active
   */
  isActive(name: string): boolean {
    return this.activeProviderId === name;
  }

  // ============================================================================
  // Provider Discovery
  // ============================================================================

  /**
   * Get provider by name
   *
   * @param name Provider name
   * @returns Provider instance or null if not found
   */
  getProvider(name: string): IBacktestingProvider | null {
    return this.providers.get(name) || null;
  }

  /**
   * List all registered providers
   *
   * @returns Array of provider info
   */
  listProviders(): ProviderInfo[] {
    return Array.from(this.providers.values()).map((provider) => ({
      name: provider.name,
      version: provider.version,
      capabilities: provider.capabilities,
      description: provider.description || '',
      isActive: this.activeProviderId === provider.name,
      isEnabled: true,
      config: this.configs.get(provider.name),
    }));
  }

  /**
   * Get providers by capability
   *
   * @param capability Capability to filter by
   * @returns Array of provider info that support this capability
   */
  getProvidersByCapability(
    capability: keyof ProviderInfo['capabilities']
  ): ProviderInfo[] {
    return this.listProviders().filter((info) => info.capabilities[capability]);
  }

  /**
   * Get providers supporting specific asset class
   *
   * @param assetClass Asset class (stocks, options, crypto, etc.)
   * @returns Array of provider info
   */
  getProvidersByAssetClass(assetClass: string): ProviderInfo[] {
    return this.listProviders().filter((info) =>
      info.capabilities.multiAsset.includes(assetClass as any)
    );
  }

  /**
   * Check if any providers are registered
   *
   * @returns true if at least one provider is registered
   */
  hasProviders(): boolean {
    return this.providers.size > 0;
  }

  /**
   * Get total number of registered providers
   *
   * @returns Number of providers
   */
  getProviderCount(): number {
    return this.providers.size;
  }

  // ============================================================================
  // Configuration Management
  // ============================================================================

  /**
   * Save provider configuration
   *
   * @param name Provider name
   * @param config Provider configuration
   */
  saveProviderConfig(name: string, config: ProviderConfig): void {
    this.configs.set(name, config);
    this.notifyListeners('config:saved', { provider: name });
  }

  /**
   * Load provider configuration
   *
   * @param name Provider name
   * @returns Configuration or null if not found
   */
  loadProviderConfig(name: string): ProviderConfig | null {
    return this.configs.get(name) || null;
  }

  /**
   * Delete provider configuration
   *
   * @param name Provider name
   * @returns true if config was found and deleted
   */
  deleteProviderConfig(name: string): boolean {
    const deleted = this.configs.delete(name);
    if (deleted) {
      this.notifyListeners('config:deleted', { provider: name });
    }
    return deleted;
  }

  // ============================================================================
  // Provider Switching
  // ============================================================================

  /**
   * Switch from one provider to another
   *
   * @param fromProvider Current provider name (optional)
   * @param toProvider Target provider name
   */
  async switchProvider(
    fromProvider: string | null,
    toProvider: string
  ): Promise<void> {
    // Validate target provider exists
    if (!this.providers.has(toProvider)) {
      throw new Error(`Target provider "${toProvider}" not found`);
    }

    // If fromProvider specified, validate it's currently active
    if (fromProvider && this.activeProviderId !== fromProvider) {
      console.warn(
        `Warning: fromProvider "${fromProvider}" is not active. Current: ${this.activeProviderId}`
      );
    }

    // Disconnect from current provider
    if (this.activeProviderId) {
      const currentProvider = this.providers.get(this.activeProviderId);
      if (currentProvider) {
        await currentProvider.disconnect();
      }
    }

    // Activate new provider
    await this.setActiveProvider(toProvider);

    console.log(`✓ Switched provider: ${fromProvider} → ${toProvider}`);
  }

  // ============================================================================
  // Event System
  // ============================================================================

  /**
   * Add event listener
   *
   * @param listener Listener function
   */
  addListener(listener: RegistryListener): void {
    this.listeners.add(listener);
  }

  /**
   * Remove event listener
   *
   * @param listener Listener function
   */
  removeListener(listener: RegistryListener): void {
    this.listeners.delete(listener);
  }

  /**
   * Notify all listeners of an event
   *
   * @param event Event type
   * @param data Event data
   */
  private notifyListeners(event: RegistryEvent, data: any): void {
    this.listeners.forEach((listener) => {
      try {
        listener(event, data);
      } catch (error) {
        console.error('Error in registry listener:', error);
      }
    });
  }

  // ============================================================================
  // Utility Methods
  // ============================================================================

  /**
   * Clear all providers and reset registry
   * WARNING: This disconnects all providers and clears all data
   */
  async clearAll(): Promise<void> {
    // Disconnect all providers
    await Promise.all(
      Array.from(this.providers.values()).map((p) =>
        p.disconnect().catch(console.error)
      )
    );

    this.providers.clear();
    this.configs.clear();
    this.activeProviderId = null;
    this.listeners.clear();

    console.log('✓ Cleared all backtesting providers');
  }

  /**
   * Get registry statistics
   *
   * @returns Registry stats
   */
  getStats(): RegistryStats {
    return {
      totalProviders: this.providers.size,
      activeProvider: this.activeProviderId,
      providersWithOptimization: this.getProvidersByCapability('optimization')
        .length,
      providersWithLiveTrading: this.getProvidersByCapability('liveTrading')
        .length,
      providersWithResearch: this.getProvidersByCapability('research').length,
    };
  }
}

// ============================================================================
// Types
// ============================================================================

export type RegistryEvent =
  | 'provider:registered'
  | 'provider:unregistered'
  | 'provider:activated'
  | 'config:saved'
  | 'config:deleted';

export type RegistryListener = (event: RegistryEvent, data: any) => void;

export interface RegistryStats {
  totalProviders: number;
  activeProvider: string | null;
  providersWithOptimization: number;
  providersWithLiveTrading: number;
  providersWithResearch: number;
}

// ============================================================================
// Singleton Export
// ============================================================================

/**
 * Global singleton instance of the backtesting provider registry
 */
export const backtestingRegistry = new BacktestingProviderRegistry();
