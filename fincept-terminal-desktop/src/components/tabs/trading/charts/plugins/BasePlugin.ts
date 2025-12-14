/**
 * Base Plugin Class
 *
 * Abstract base class that provides common functionality for all chart plugins.
 * Implements the IChartPlugin interface with shared utilities.
 */

import type {
  ISeriesPrimitive,
  Time,
  SeriesType,
  ISeriesApi,
  IChartApi
} from 'lightweight-charts';
import {
  IChartPlugin,
  PluginMetadata,
  PluginOptions,
  PluginEventType,
  PluginEvent,
  PluginEventListener
} from './types';

export abstract class BasePlugin<HorzScaleItem = Time> implements IChartPlugin<HorzScaleItem> {
  public metadata: PluginMetadata;
  public options: PluginOptions;

  protected chart: IChartApi | null = null;
  protected series: ISeriesApi<SeriesType, HorzScaleItem> | null = null;
  protected primitive: ISeriesPrimitive<HorzScaleItem> | null = null;
  protected eventListeners: Map<PluginEventType, PluginEventListener[]> = new Map();

  constructor(metadata: PluginMetadata, options: PluginOptions = {}) {
    this.metadata = { ...metadata };
    this.options = { ...this.getDefaultOptions(), ...options };
  }

  /**
   * Get default options for the plugin (can be overridden)
   */
  protected getDefaultOptions(): PluginOptions {
    return {
      visible: true,
      interactive: false,
      color: '#2196F3',
      lineWidth: 2,
      lineStyle: 0, // Solid
    };
  }

  /**
   * Initialize the plugin with chart and series
   */
  public initialize(chart: IChartApi, series: ISeriesApi<SeriesType, HorzScaleItem>): void {
    this.chart = chart;
    this.series = series;

    // Create and attach primitive
    this.primitive = this.createPrimitive();
    if (this.primitive && this.metadata.enabled) {
      this.attachPrimitive();
    }
  }

  /**
   * Create the primitive (must be implemented by subclasses)
   */
  protected abstract createPrimitive(): ISeriesPrimitive<HorzScaleItem> | null;

  /**
   * Attach primitive to series
   */
  protected attachPrimitive(): void {
    if (this.primitive && this.series) {
      this.series.attachPrimitive(this.primitive);
    }
  }

  /**
   * Detach primitive from series
   */
  protected detachPrimitive(): void {
    if (this.primitive && this.series) {
      this.series.detachPrimitive(this.primitive);
    }
  }

  /**
   * Apply new options to the plugin
   */
  public applyOptions(newOptions: Partial<PluginOptions>): void {
    this.options = { ...this.options, ...newOptions };
    this.onOptionsChanged();
  }

  /**
   * Called when options change (can be overridden)
   */
  protected onOptionsChanged(): void {
    // Trigger re-render
    if (this.chart) {
      this.chart.timeScale().fitContent();
    }
  }

  /**
   * Get the primitive
   */
  public getPrimitive(): ISeriesPrimitive<HorzScaleItem> | null {
    return this.primitive;
  }

  /**
   * Enable/disable the plugin
   */
  public setEnabled(enabled: boolean): void {
    if (this.metadata.enabled === enabled) return;

    this.metadata.enabled = enabled;

    if (enabled) {
      this.attachPrimitive();
    } else {
      this.detachPrimitive();
    }
  }

  /**
   * Destroy the plugin and cleanup
   */
  public destroy(): void {
    this.detachPrimitive();
    this.eventListeners.clear();
    this.chart = null;
    this.series = null;
    this.primitive = null;
  }

  /**
   * Add event listener
   */
  public addEventListener(type: PluginEventType, listener: PluginEventListener): void {
    if (!this.eventListeners.has(type)) {
      this.eventListeners.set(type, []);
    }
    this.eventListeners.get(type)!.push(listener);
  }

  /**
   * Remove event listener
   */
  public removeEventListener(type: PluginEventType, listener: PluginEventListener): void {
    const listeners = this.eventListeners.get(type);
    if (listeners) {
      const index = listeners.indexOf(listener);
      if (index > -1) {
        listeners.splice(index, 1);
      }
    }
  }

  /**
   * Emit event to listeners
   */
  protected emitEvent(event: PluginEvent): void {
    const listeners = this.eventListeners.get(event.type);
    if (listeners) {
      listeners.forEach(listener => listener(event));
    }
  }
}
