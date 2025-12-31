/**
 * Integration Manager
 * Central hub for all external integrations
 */

import { pluginManager } from '../core/PluginSystem';
import { paperTradingIntegration } from './PaperTradingIntegration';

export class IntegrationManager {
  private static instance: IntegrationManager;
  private initialized = false;

  private constructor() {}

  static getInstance(): IntegrationManager {
    if (!IntegrationManager.instance) {
      IntegrationManager.instance = new IntegrationManager();
    }
    return IntegrationManager.instance;
  }

  /**
   * Initialize all available integrations
   */
  async initialize(): Promise<void> {
    if (this.initialized) return;

    console.log('[IntegrationManager] Initializing integrations...');

    try {
      // Register Paper Trading
      await pluginManager.registerPlugin(paperTradingIntegration.createPlugin());

      // TODO: Register other integrations
      // - Alpha Arena
      // - AI Chat
      // - Excel Export
      // - Rust Performance

      this.initialized = true;
      console.log('[IntegrationManager] All integrations registered');
    } catch (error) {
      console.error('[IntegrationManager] Initialization failed:', error);
      throw error;
    }
  }

  /**
   * Enable paper trading mode
   */
  async enablePaperTrading(): Promise<void> {
    await pluginManager.enablePlugin('paper-trading');
  }

  /**
   * Disable paper trading mode
   */
  async disablePaperTrading(): Promise<void> {
    await pluginManager.disablePlugin('paper-trading');
  }

  /**
   * Check if paper trading is enabled
   */
  isPaperTradingEnabled(): boolean {
    const plugin = pluginManager.getPlugin('paper-trading');
    return plugin?.enabled || false;
  }

  /**
   * Get paper trading balance
   */
  async getPaperBalance(): Promise<number> {
    const balance = await paperTradingIntegration.getBalance();
    console.log('[IntegrationManager] getPaperBalance returning:', balance);
    return balance;
  }

  /**
   * Get paper trading positions
   */
  async getPaperPositions(): Promise<any[]> {
    return await paperTradingIntegration.getPositions();
  }

  /**
   * Get paper trading orders
   */
  async getPaperOrders(): Promise<any[]> {
    return await paperTradingIntegration.getOrders();
  }

  /**
   * Get paper trading statistics
   */
  async getPaperStatistics(): Promise<any> {
    return await paperTradingIntegration.getStatistics();
  }

  /**
   * Reset paper trading account
   */
  async resetPaperAccount(initialBalance?: number): Promise<void> {
    await paperTradingIntegration.reset(initialBalance);
  }

  /**
   * Check if paper trading is initialized
   */
  isPaperTradingInitialized(): boolean {
    return paperTradingIntegration.isInitialized();
  }

  /**
   * Alpha Arena Integration Points
   */
  async connectAlphaArena(): Promise<void> {
    // TODO: Implement Alpha Arena connection
    console.log('[IntegrationManager] Alpha Arena integration - Coming soon');
  }

  /**
   * AI Chat Integration Points
   */
  async initializeAIChat(): Promise<void> {
    // TODO: Implement AI Chat integration
    console.log('[IntegrationManager] AI Chat integration - Coming soon');
  }

  /**
   * Excel Export Integration Points
   */
  async exportToExcel(data: any, type: 'orders' | 'positions' | 'holdings'): Promise<void> {
    // TODO: Implement Excel export
    console.log('[IntegrationManager] Excel export integration - Coming soon');
  }

  /**
   * Rust Performance Hooks
   */
  async initializeRustHooks(): Promise<void> {
    // TODO: Implement Rust performance hooks for:
    // - hftbacktest
    // - barter-rs
    console.log('[IntegrationManager] Rust performance hooks - Coming soon');
  }

  /**
   * Get all active integrations
   */
  getActiveIntegrations(): Array<{ id: string; name: string; enabled: boolean }> {
    return pluginManager.getPlugins().map(p => ({
      id: p.id,
      name: p.name,
      enabled: p.enabled,
    }));
  }
}

export const integrationManager = IntegrationManager.getInstance();
