/**
 * Trading Toolkit Plugin — Types & Enums
 *
 * Enums, interfaces, and type definitions extracted from TradingToolkitPlugin.ts.
 */

import type { Time } from 'lightweight-charts';
import type { PluginOptions, ChartPoint } from './types';

// ==================== TOOL TYPES ====================

export enum DrawingTool {
  // Basic Drawing
  TREND_LINE = 'trendline',
  HORIZONTAL_LINE = 'horizontal',
  VERTICAL_LINE = 'vertical',
  RAY = 'ray',
  EXTENDED_LINE = 'extended',

  // Channels & Patterns
  PARALLEL_CHANNEL = 'channel',
  REGRESSION_CHANNEL = 'regression',
  PITCHFORK = 'pitchfork',

  // Shapes
  RECTANGLE = 'rectangle',
  ELLIPSE = 'ellipse',
  TRIANGLE = 'triangle',
  ARROW = 'arrow',

  // Fibonacci Tools
  FIBO_RETRACEMENT = 'fibo_retracement',
  FIBO_EXTENSION = 'fibo_extension',
  FIBO_FAN = 'fibo_fan',
  FIBO_CIRCLE = 'fibo_circle',
  FIBO_SPIRAL = 'fibo_spiral',

  // Gann Tools
  GANN_FAN = 'gann_fan',
  GANN_BOX = 'gann_box',

  // Positions & Orders
  LONG_POSITION = 'long_position',
  SHORT_POSITION = 'short_position',
  STOP_LOSS = 'stop_loss',
  TAKE_PROFIT = 'take_profit',
  ENTRY_ORDER = 'entry_order',

  // Annotations
  TEXT = 'text',
  CALLOUT = 'callout',
  LABEL = 'label',
  PRICE_NOTE = 'price_note',

  // Measurement
  MEASURE = 'measure',
  ANGLE = 'angle',
}

export enum PositionType {
  LONG = 'long',
  SHORT = 'short',
}

export enum OrderType {
  ENTRY = 'entry',
  EXIT = 'exit',
  STOP_LOSS = 'stop_loss',
  TAKE_PROFIT = 'take_profit',
}

// ==================== DATA STRUCTURES ====================

export interface DrawingObject {
  id: string;
  type: DrawingTool;
  points: ChartPoint<Time>[];
  style: DrawingStyle;
  locked?: boolean;
  visible?: boolean;
  data?: any; // Additional tool-specific data
}

export interface DrawingStyle {
  color: string;
  lineWidth: number;
  lineStyle: 'solid' | 'dashed' | 'dotted';
  fillColor?: string;
  fillOpacity?: number;
  textColor?: string;
  fontSize?: number;
  showLabels?: boolean;
}

export interface FibonacciLevel {
  level: number;
  label: string;
  color: string;
  showLabel: boolean;
}

export interface PositionMarker {
  id: string;
  type: PositionType;
  orderType: OrderType;
  time: Time;
  price: number;
  quantity?: number;
  label?: string;
  pnl?: number;
  color: string;
}

export interface ToolkitOptions extends PluginOptions {
  // Default styles
  defaultLineColor?: string;
  defaultFillColor?: string;
  defaultTextColor?: string;
  defaultLineWidth?: number;

  // Fibonacci levels
  fibonacciLevels?: FibonacciLevel[];

  // Position colors
  longColor?: string;
  shortColor?: string;
  profitColor?: string;
  lossColor?: string;

  // Behavior
  snapToPrice?: boolean;
  snapToTime?: boolean;
  showMagnetMode?: boolean;
}
