/**
 * Lightweight Charts Plugin System - Type Definitions
 *
 * This file defines the plugin architecture for extending lightweight-charts
 * with custom primitives, drawing tools, indicators, and interactive features.
 */

import type {
  ISeriesPrimitive,
  SeriesAttachedParameter,
  IPrimitivePaneView,
  Time,
  SeriesType,
  ISeriesApi,
  IChartApi
} from 'lightweight-charts';

/**
 * Plugin categories for organization
 */
export enum PluginCategory {
  DRAWING = 'drawing',
  INDICATOR = 'indicator',
  ANNOTATION = 'annotation',
  INTERACTIVE = 'interactive',
  OVERLAY = 'overlay',
}

/**
 * Plugin metadata
 */
export interface PluginMetadata {
  id: string;
  name: string;
  description: string;
  category: PluginCategory;
  version: string;
  author?: string;
  enabled: boolean;
}

/**
 * Plugin configuration options
 */
export interface PluginOptions {
  color?: string;
  lineWidth?: number;
  lineStyle?: number;
  visible?: boolean;
  interactive?: boolean;
  [key: string]: any;
}

/**
 * Base plugin interface that all plugins must implement
 */
export interface IChartPlugin<HorzScaleItem = Time> {
  metadata: PluginMetadata;
  options: PluginOptions;

  /**
   * Initialize the plugin with chart and series context
   */
  initialize(chart: IChartApi, series: ISeriesApi<SeriesType, HorzScaleItem>): void;

  /**
   * Update plugin options
   */
  applyOptions(options: Partial<PluginOptions>): void;

  /**
   * Get the primitive to attach to the series
   */
  getPrimitive(): ISeriesPrimitive<HorzScaleItem> | null;

  /**
   * Cleanup and remove the plugin
   */
  destroy(): void;

  /**
   * Enable/disable the plugin
   */
  setEnabled(enabled: boolean): void;
}

/**
 * Drawing plugin interface for interactive drawing tools
 */
export interface IDrawingPlugin<HorzScaleItem = Time> extends IChartPlugin<HorzScaleItem> {
  /**
   * Start drawing mode
   */
  startDrawing(): void;

  /**
   * Stop drawing mode
   */
  stopDrawing(): void;

  /**
   * Check if currently in drawing mode
   */
  isDrawing(): boolean;

  /**
   * Clear all drawn objects
   */
  clear(): void;
}

/**
 * Point in chart coordinates
 */
export interface ChartPoint<HorzScaleItem = Time> {
  time: HorzScaleItem;
  price: number;
}

/**
 * Mouse event data for plugins
 */
export interface PluginMouseEvent {
  x: number;
  y: number;
  time?: Time;
  price?: number;
  ctrlKey: boolean;
  shiftKey: boolean;
  altKey: boolean;
}

/**
 * Plugin manager interface
 */
export interface IPluginManager {
  /**
   * Register a plugin
   */
  register(plugin: IChartPlugin): void;

  /**
   * Unregister a plugin by id
   */
  unregister(pluginId: string): void;

  /**
   * Get all registered plugins
   */
  getPlugins(): IChartPlugin[];

  /**
   * Get plugins by category
   */
  getPluginsByCategory(category: PluginCategory): IChartPlugin[];

  /**
   * Enable/disable a plugin
   */
  togglePlugin(pluginId: string, enabled: boolean): void;

  /**
   * Get plugin by id
   */
  getPlugin(pluginId: string): IChartPlugin | undefined;
}

/**
 * Plugin event types
 */
export enum PluginEventType {
  CLICK = 'click',
  DOUBLE_CLICK = 'doubleClick',
  MOUSE_DOWN = 'mouseDown',
  MOUSE_UP = 'mouseUp',
  MOUSE_MOVE = 'mouseMove',
  DRAWING_COMPLETE = 'drawingComplete',
  DRAWING_CANCEL = 'drawingCancel',
}

/**
 * Plugin event
 */
export interface PluginEvent {
  type: PluginEventType;
  data: any;
  timestamp: number;
}

/**
 * Plugin event listener
 */
export type PluginEventListener = (event: PluginEvent) => void;
