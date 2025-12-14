/**
 * Plugin Manager
 *
 * Manages all chart plugins, including registration, lifecycle, and coordination.
 */

import type { IChartApi, ISeriesApi, SeriesType, Time } from 'lightweight-charts';
import { IPluginManager, IChartPlugin, PluginCategory } from './types';

export class PluginManager implements IPluginManager {
  private plugins: Map<string, IChartPlugin> = new Map();
  private chart: IChartApi | null = null;
  private series: ISeriesApi<SeriesType, Time> | null = null;

  constructor(chart?: IChartApi, series?: ISeriesApi<SeriesType, Time>) {
    if (chart) this.chart = chart;
    if (series) this.series = series;
  }

  /**
   * Set the chart and series context
   */
  public setContext(chart: IChartApi, series: ISeriesApi<SeriesType, Time>): void {
    this.chart = chart;
    this.series = series;

    // Initialize all existing plugins with new context
    this.plugins.forEach(plugin => {
      if (this.chart && this.series) {
        plugin.initialize(this.chart, this.series);
      }
    });
  }

  /**
   * Register a plugin
   */
  public register(plugin: IChartPlugin): void {
    const pluginId = plugin.metadata.id;

    if (this.plugins.has(pluginId)) {
      console.warn(`Plugin with id "${pluginId}" is already registered. Skipping.`);
      return;
    }

    // Initialize plugin if context is available
    if (this.chart && this.series) {
      plugin.initialize(this.chart, this.series);
    }

    this.plugins.set(pluginId, plugin);
    console.log(`Plugin "${plugin.metadata.name}" registered successfully.`);
  }

  /**
   * Unregister a plugin by id
   */
  public unregister(pluginId: string): void {
    const plugin = this.plugins.get(pluginId);
    if (!plugin) {
      console.warn(`Plugin with id "${pluginId}" not found.`);
      return;
    }

    plugin.destroy();
    this.plugins.delete(pluginId);
    console.log(`Plugin "${plugin.metadata.name}" unregistered.`);
  }

  /**
   * Get all registered plugins
   */
  public getPlugins(): IChartPlugin[] {
    return Array.from(this.plugins.values());
  }

  /**
   * Get plugins by category
   */
  public getPluginsByCategory(category: PluginCategory): IChartPlugin[] {
    return this.getPlugins().filter(
      plugin => plugin.metadata.category === category
    );
  }

  /**
   * Toggle plugin enabled state
   */
  public togglePlugin(pluginId: string, enabled: boolean): void {
    const plugin = this.plugins.get(pluginId);
    if (!plugin) {
      console.warn(`Plugin with id "${pluginId}" not found.`);
      return;
    }

    plugin.setEnabled(enabled);
    console.log(`Plugin "${plugin.metadata.name}" ${enabled ? 'enabled' : 'disabled'}.`);
  }

  /**
   * Get plugin by id
   */
  public getPlugin(pluginId: string): IChartPlugin | undefined {
    return this.plugins.get(pluginId);
  }

  /**
   * Enable all plugins
   */
  public enableAll(): void {
    this.plugins.forEach(plugin => plugin.setEnabled(true));
  }

  /**
   * Disable all plugins
   */
  public disableAll(): void {
    this.plugins.forEach(plugin => plugin.setEnabled(false));
  }

  /**
   * Clear all plugins
   */
  public clear(): void {
    this.plugins.forEach(plugin => plugin.destroy());
    this.plugins.clear();
  }

  /**
   * Get enabled plugins count
   */
  public getEnabledCount(): number {
    return this.getPlugins().filter(p => p.metadata.enabled).length;
  }

  /**
   * Get plugin statistics
   */
  public getStats(): {
    total: number;
    enabled: number;
    byCategory: Record<string, number>;
  } {
    const plugins = this.getPlugins();
    const byCategory: Record<string, number> = {};

    plugins.forEach(plugin => {
      const category = plugin.metadata.category;
      byCategory[category] = (byCategory[category] || 0) + 1;
    });

    return {
      total: plugins.length,
      enabled: this.getEnabledCount(),
      byCategory,
    };
  }

  /**
   * Destroy manager and all plugins
   */
  public destroy(): void {
    this.clear();
    this.chart = null;
    this.series = null;
  }
}
