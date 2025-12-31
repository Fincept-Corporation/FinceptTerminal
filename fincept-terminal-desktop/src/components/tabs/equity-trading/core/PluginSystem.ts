/**
 * Plugin System Architecture
 * Extensible framework for future integrations
 * - Paper Trading
 * - Alpha Arena
 * - AI Chat
 * - Excel Export
 * - Rust Performance Hooks
 */

import { UnifiedOrder, OrderRequest, UnifiedPosition, UnifiedHolding, BrokerType } from './types';

// ==================== PLUGIN TYPES ====================

export enum PluginType {
  PRE_ORDER = 'PRE_ORDER',           // Before order placement
  POST_ORDER = 'POST_ORDER',         // After order placement
  ORDER_UPDATE = 'ORDER_UPDATE',     // On order status change
  POSITION_UPDATE = 'POSITION_UPDATE', // On position change
  DATA_FEED = 'DATA_FEED',          // Market data stream
  ANALYTICS = 'ANALYTICS',           // Custom analytics
  EXPORT = 'EXPORT',                 // Data export
  AI_AGENT = 'AI_AGENT',            // AI integration
  PERFORMANCE = 'PERFORMANCE',       // Performance optimization
}

export interface Plugin {
  id: string;
  name: string;
  type: PluginType;
  version: string;
  enabled: boolean;
  execute: (context: PluginContext) => Promise<PluginResult>;
  onEnable?: () => Promise<void>;
  onDisable?: () => Promise<void>;
}

export interface PluginContext {
  type: PluginType;
  data: any;
  metadata?: Record<string, any>;
  cancel?: () => void;
  modify?: (updates: any) => void;
}

export interface PluginResult {
  success: boolean;
  data?: any;
  error?: string;
  modified?: boolean;
}

// ==================== PLUGIN MANAGER ====================

export class PluginManager {
  private static instance: PluginManager;
  private plugins: Map<string, Plugin> = new Map();
  private hooks: Map<PluginType, Set<string>> = new Map();

  private constructor() {
    // Initialize hook sets
    Object.values(PluginType).forEach(type => {
      this.hooks.set(type as PluginType, new Set());
    });
  }

  static getInstance(): PluginManager {
    if (!PluginManager.instance) {
      PluginManager.instance = new PluginManager();
    }
    return PluginManager.instance;
  }

  /**
   * Register a plugin
   */
  async registerPlugin(plugin: Plugin): Promise<void> {
    if (this.plugins.has(plugin.id)) {
      throw new Error(`Plugin ${plugin.id} already registered`);
    }

    this.plugins.set(plugin.id, plugin);
    this.hooks.get(plugin.type)?.add(plugin.id);

    console.log(`[PluginManager] Registered plugin: ${plugin.name} (${plugin.type})`);

    if (plugin.enabled && plugin.onEnable) {
      await plugin.onEnable();
    }
  }

  /**
   * Unregister a plugin
   */
  async unregisterPlugin(pluginId: string): Promise<void> {
    const plugin = this.plugins.get(pluginId);
    if (!plugin) return;

    if (plugin.enabled && plugin.onDisable) {
      await plugin.onDisable();
    }

    this.hooks.get(plugin.type)?.delete(pluginId);
    this.plugins.delete(pluginId);

    console.log(`[PluginManager] Unregistered plugin: ${plugin.name}`);
  }

  /**
   * Enable a plugin
   */
  async enablePlugin(pluginId: string): Promise<void> {
    const plugin = this.plugins.get(pluginId);
    if (!plugin || plugin.enabled) return;

    plugin.enabled = true;

    if (plugin.onEnable) {
      await plugin.onEnable();
    }

    console.log(`[PluginManager] Enabled plugin: ${plugin.name}`);
  }

  /**
   * Disable a plugin
   */
  async disablePlugin(pluginId: string): Promise<void> {
    const plugin = this.plugins.get(pluginId);
    if (!plugin || !plugin.enabled) return;

    plugin.enabled = false;

    if (plugin.onDisable) {
      await plugin.onDisable();
    }

    console.log(`[PluginManager] Disabled plugin: ${plugin.name}`);
  }

  /**
   * Execute all plugins for a specific hook
   */
  async executeHook(type: PluginType, context: PluginContext): Promise<PluginResult[]> {
    const pluginIds = this.hooks.get(type);
    if (!pluginIds || pluginIds.size === 0) return [];

    const results: PluginResult[] = [];

    for (const pluginId of pluginIds) {
      const plugin = this.plugins.get(pluginId);
      if (!plugin || !plugin.enabled) continue;

      try {
        const result = await plugin.execute(context);
        results.push(result);

        if (!result.success) {
          console.error(`[PluginManager] Plugin ${plugin.name} failed:`, result.error);
        }
      } catch (error: any) {
        console.error(`[PluginManager] Plugin ${plugin.name} error:`, error);
        results.push({
          success: false,
          error: error.message || 'Plugin execution failed',
        });
      }
    }

    return results;
  }

  /**
   * Get all registered plugins
   */
  getPlugins(): Plugin[] {
    return Array.from(this.plugins.values());
  }

  /**
   * Get plugins by type
   */
  getPluginsByType(type: PluginType): Plugin[] {
    return this.getPlugins().filter(p => p.type === type);
  }

  /**
   * Get plugin by ID
   */
  getPlugin(pluginId: string): Plugin | undefined {
    return this.plugins.get(pluginId);
  }
}

// ==================== INTEGRATION POINT INTERFACES ====================

/**
 * Paper Trading Integration
 */
export interface PaperTradingHook {
  onOrderPlaced: (order: OrderRequest, brokerId: BrokerType) => Promise<void>;
  onOrderFilled: (order: UnifiedOrder) => Promise<void>;
  getVirtualBalance: () => Promise<number>;
  getVirtualPositions: () => Promise<UnifiedPosition[]>;
}

/**
 * Alpha Arena Integration
 */
export interface AlphaArenaHook {
  onStrategySignal: (signal: { symbol: string; action: 'BUY' | 'SELL'; confidence: number }) => Promise<void>;
  submitPerformance: (metrics: { pnl: number; sharpe: number; drawdown: number }) => Promise<void>;
  getLeaderboard: () => Promise<any[]>;
}

/**
 * AI Chat Integration
 */
export interface AIChatHook {
  onTradeExecuted: (order: UnifiedOrder) => Promise<string>; // Returns AI commentary
  analyzePosition: (position: UnifiedPosition) => Promise<string>;
  suggestTrade: (symbol: string) => Promise<OrderRequest | null>;
  chatQuery: (query: string, context: any) => Promise<string>;
}

/**
 * Excel Integration
 */
export interface ExcelHook {
  exportOrders: (orders: UnifiedOrder[]) => Promise<Blob>;
  exportPositions: (positions: UnifiedPosition[]) => Promise<Blob>;
  exportHoldings: (holdings: UnifiedHolding[]) => Promise<Blob>;
  importStrategy: (file: File) => Promise<OrderRequest[]>;
}

/**
 * Rust Performance Hook
 * For high-frequency operations (hftbacktest, barter-rs)
 */
export interface RustPerformanceHook {
  processTick: (symbol: string, price: number, volume: number) => Promise<void>;
  runBacktest: (strategy: any, data: any) => Promise<any>;
  optimizeExecution: (order: OrderRequest) => Promise<OrderRequest>;
  calculateRisk: (positions: UnifiedPosition[]) => Promise<{ var: number; sharpe: number }>;
}

// ==================== EXPORT ====================

export const pluginManager = PluginManager.getInstance();

// Helper to create plugin
export function createPlugin(config: Omit<Plugin, 'enabled'>): Plugin {
  return {
    ...config,
    enabled: false,
  };
}
