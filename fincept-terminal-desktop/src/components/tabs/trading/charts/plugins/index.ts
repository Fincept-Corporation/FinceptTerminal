/**
 * Lightweight Charts Plugin System
 *
 * Export all plugins and core plugin infrastructure
 */

// Core infrastructure
export * from './types';
export { BasePlugin } from './BasePlugin';
export { PluginManager } from './PluginManager';

// Built-in plugins
export { TooltipPlugin } from './TooltipPlugin';
export { TrendLinePlugin } from './TrendLinePlugin';
export { VerticalLinePlugin } from './VerticalLinePlugin';
export { TradingToolkitPlugin, DrawingTool, PositionType, OrderType } from './TradingToolkitPlugin';
export type { DrawingObject, DrawingStyle, PositionMarker, ToolkitOptions } from './TradingToolkitPlugin';

// Plugin factory for easy instantiation
import { TooltipPlugin } from './TooltipPlugin';
import { TrendLinePlugin } from './TrendLinePlugin';
import { VerticalLinePlugin } from './VerticalLinePlugin';
import { TradingToolkitPlugin } from './TradingToolkitPlugin';
import { IChartPlugin, PluginCategory } from './types';

export interface PluginRegistry {
  [key: string]: () => IChartPlugin;
}

/**
 * Registry of all available plugins
 */
export const PLUGIN_REGISTRY: PluginRegistry = {
  tooltip: () => new TooltipPlugin(),
  trendline: () => new TrendLinePlugin(),
  verticalline: () => new VerticalLinePlugin(),
  tradingtoolkit: () => new TradingToolkitPlugin(),
};

/**
 * Create a plugin by name
 */
export function createPlugin(name: string, options?: any): IChartPlugin | null {
  const factory = PLUGIN_REGISTRY[name.toLowerCase()];
  if (!factory) {
    console.warn(`Plugin "${name}" not found in registry`);
    return null;
  }
  return factory();
}

/**
 * Get all available plugin names
 */
export function getAvailablePlugins(): string[] {
  return Object.keys(PLUGIN_REGISTRY);
}

/**
 * Get plugin metadata by name
 */
export function getPluginInfo(name: string): { name: string; category: string; description: string } | null {
  const plugin = createPlugin(name);
  if (!plugin) return null;

  const info = {
    name: plugin.metadata.name,
    category: plugin.metadata.category,
    description: plugin.metadata.description,
  };

  plugin.destroy();
  return info;
}

/**
 * Get all plugins grouped by category
 */
export function getPluginsByCategory(): Record<PluginCategory, string[]> {
  const grouped: Record<string, string[]> = {
    [PluginCategory.DRAWING]: [],
    [PluginCategory.INDICATOR]: [],
    [PluginCategory.ANNOTATION]: [],
    [PluginCategory.INTERACTIVE]: [],
    [PluginCategory.OVERLAY]: [],
  };

  Object.keys(PLUGIN_REGISTRY).forEach(name => {
    const info = getPluginInfo(name);
    if (info) {
      grouped[info.category].push(name);
    }
  });

  return grouped as Record<PluginCategory, string[]>;
}
