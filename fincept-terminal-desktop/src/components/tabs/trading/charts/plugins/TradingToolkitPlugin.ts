/**
 * Trading Toolkit Plugin - All-in-One Professional Drawing & Analysis Tools
 *
 * A comprehensive plugin that replicates TradingView's complete toolkit including:
 * - Drawing Tools: Trend lines, channels, patterns, rectangles, ellipses
 * - Fibonacci Tools: Retracement, extension, fans, arcs, circles
 * - Position Markers: Long/Short entries, exits, stop-loss, take-profit
 * - Technical Patterns: Head & Shoulders, triangles, flags, wedges
 * - Annotations: Text, arrows, labels, callouts
 * - Measurement Tools: Price range, date range, angle measurements
 */

import type {
  ISeriesPrimitive,
  SeriesAttachedParameter,
  Time,
  IPrimitivePaneView,
  Coordinate,
  IChartApi,
  ISeriesApi,
  SeriesType
} from 'lightweight-charts';
import { BasePlugin } from './BasePlugin';
import { PluginCategory, PluginOptions, ChartPoint } from './types';

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

interface DrawingObject {
  id: string;
  type: DrawingTool;
  points: ChartPoint<Time>[];
  style: DrawingStyle;
  locked?: boolean;
  visible?: boolean;
  data?: any; // Additional tool-specific data
}

interface DrawingStyle {
  color: string;
  lineWidth: number;
  lineStyle: 'solid' | 'dashed' | 'dotted';
  fillColor?: string;
  fillOpacity?: number;
  textColor?: string;
  fontSize?: number;
  showLabels?: boolean;
}

interface FibonacciLevel {
  level: number;
  label: string;
  color: string;
  showLabel: boolean;
}

interface PositionMarker {
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

interface ToolkitOptions extends PluginOptions {
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

// ==================== PANE VIEW ====================

class ToolkitPaneView implements IPrimitivePaneView {
  private _drawings: DrawingObject[] = [];
  private _positions: PositionMarker[] = [];
  private _options: ToolkitOptions;
  private _chart: IChartApi | null = null;
  private _series: ISeriesApi<SeriesType, Time> | null = null;
  private _selectedDrawingId: string | null = null;
  private _hoveredDrawingId: string | null = null;

  constructor(options: ToolkitOptions) {
    this._options = options;
  }

  public setSelectedDrawing(id: string | null): void {
    this._selectedDrawingId = id;
  }

  public setHoveredDrawing(id: string | null): void {
    this._hoveredDrawingId = id;
  }

  update(
    drawings: DrawingObject[],
    positions: PositionMarker[],
    chart: IChartApi,
    series: ISeriesApi<SeriesType, Time>
  ): void {
    this._drawings = drawings;
    this._positions = positions;
    this._chart = chart;
    this._series = series;
  }

  renderer(): any {
    return {
      draw: (target: any) => {
        if (!this._chart || !this._series) return;

        // Removed rendering logs - they cause severe lag on every frame

        target.useBitmapCoordinateSpace((scope: any) => {
          const ctx = scope.context;

          // Apply scaling for high DPI displays
          const scaleX = scope.horizontalPixelRatio || 1;
          const scaleY = scope.verticalPixelRatio || 1;

          ctx.save();
          ctx.scale(scaleX, scaleY);

          // Calculate logical dimensions (unscaled)
          const logicalWidth = scope.bitmapSize.width / scaleX;
          const logicalHeight = scope.bitmapSize.height / scaleY;

          // Draw all objects
          this._drawings.forEach(drawing => {
            if (drawing.visible !== false) {
              this.drawObject(ctx, drawing, logicalWidth, logicalHeight);
            }
          });

          // Draw position markers
          this._positions.forEach(position => {
            this.drawPosition(ctx, position);
          });

          ctx.restore();
        });
      }
    };
  }

  private drawObject(
    ctx: CanvasRenderingContext2D,
    drawing: DrawingObject,
    canvasWidth: number,
    canvasHeight: number
  ): void {
    const coords = this.getCoordinates(drawing.points);
    // Reduced logging - only log when coordinates are null (error case)

    if (coords.some(c => c === null)) {
      console.warn('[TradingToolkit] Skipping drawing - null coordinates:', { type: drawing.type, points: drawing.points });
      return;
    }

    // Check if drawing is selected or hovered
    const isSelected = drawing.id === this._selectedDrawingId;
    const isHovered = drawing.id === this._hoveredDrawingId;

    // Apply selection/hover styling
    if (isSelected) {
      this.applyStyle(ctx, {
        ...drawing.style,
        color: '#FF8800', // Bloomberg orange for selection
        lineWidth: (drawing.style.lineWidth || 2) + 1, // Thicker when selected
      });
    } else if (isHovered) {
      this.applyStyle(ctx, {
        ...drawing.style,
        color: '#00D66F', // Green for hover
      });
    } else {
      this.applyStyle(ctx, drawing.style);
    }

    switch (drawing.type) {
      case DrawingTool.TREND_LINE:
        this.drawTrendLine(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.HORIZONTAL_LINE:
        this.drawHorizontalLine(ctx, coords as Coordinate[][], drawing, canvasWidth);
        break;
      case DrawingTool.VERTICAL_LINE:
        this.drawVerticalLine(ctx, coords as Coordinate[][], drawing, canvasHeight);
        break;
      case DrawingTool.RAY:
        this.drawRay(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.PARALLEL_CHANNEL:
        this.drawChannel(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.REGRESSION_CHANNEL:
        this.drawRegressionChannel(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.PITCHFORK:
        this.drawPitchfork(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.RECTANGLE:
        this.drawRectangle(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.ELLIPSE:
        this.drawEllipse(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.TRIANGLE:
        this.drawTriangle(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.FIBO_RETRACEMENT:
        this.drawFibonacci(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.LONG_POSITION:
      case DrawingTool.SHORT_POSITION:
        this.drawPositionBox(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.TEXT:
        this.drawText(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.CALLOUT:
        this.drawCallout(ctx, coords as Coordinate[][], drawing);
        break;
      case DrawingTool.ARROW:
        this.drawArrow(ctx, coords as Coordinate[][], drawing);
        break;
      default:
        console.warn('[TradingToolkit] No renderer for tool type:', drawing.type);
    }
  }

  private drawTrendLine(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 2) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();

    // Draw anchor points
    this.drawAnchor(ctx, x1, y1, drawing.style.color);
    this.drawAnchor(ctx, x2, y2, drawing.style.color);
  }

  private drawHorizontalLine(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject,
    canvasWidth: number
  ): void {
    if (coords.length < 1) return;

    const [x, y] = coords[0];

    // Draw line across entire chart width
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(canvasWidth, y);
    ctx.stroke();

    // Draw anchor point
    this.drawAnchor(ctx, x, y, drawing.style.color);
  }

  private drawVerticalLine(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject,
    canvasHeight: number
  ): void {
    if (coords.length < 1) return;

    const [x, y] = coords[0];

    // Draw line across entire chart height
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, canvasHeight);
    ctx.stroke();

    // Draw anchor point
    this.drawAnchor(ctx, x, y, drawing.style.color);
  }

  private drawRay(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 2) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    // Calculate direction and extend the line to canvas edge
    const dx = x2 - x1;
    const dy = y2 - y1;
    const angle = Math.atan2(dy, dx);

    // Extend line to a very large distance in the direction
    const extendDistance = 10000;
    const x3 = x1 + Math.cos(angle) * extendDistance;
    const y3 = y1 + Math.sin(angle) * extendDistance;

    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x3, y3);
    ctx.stroke();

    // Draw anchor points
    this.drawAnchor(ctx, x1, y1, drawing.style.color);
    this.drawAnchor(ctx, x2, y2, drawing.style.color);
  }

  private drawTriangle(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 3) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];
    const [x3, y3] = coords[2];

    // Fill
    if (drawing.style.fillColor) {
      ctx.fillStyle = drawing.style.fillColor;
      ctx.globalAlpha = drawing.style.fillOpacity || 0.2;
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.lineTo(x3, y3);
      ctx.closePath();
      ctx.fill();
      ctx.globalAlpha = 1;
    }

    // Border
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.lineTo(x3, y3);
    ctx.closePath();
    ctx.stroke();

    // Anchors
    this.drawAnchor(ctx, x1, y1, drawing.style.color);
    this.drawAnchor(ctx, x2, y2, drawing.style.color);
    this.drawAnchor(ctx, x3, y3, drawing.style.color);
  }

  private drawChannel(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 3) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];
    const [x3, y3] = coords[2];

    // Calculate parallel offset
    const dx = x2 - x1;
    const dy = y2 - y1;
    const offsetY = y3 - y1;

    // Main trend line
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();

    // Parallel line
    ctx.beginPath();
    ctx.moveTo(x1, y1 + offsetY);
    ctx.lineTo(x2, y2 + offsetY);
    ctx.stroke();

    // Fill channel if specified
    if (drawing.style.fillColor) {
      ctx.fillStyle = drawing.style.fillColor;
      ctx.globalAlpha = drawing.style.fillOpacity || 0.1;
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.lineTo(x2, y2 + offsetY);
      ctx.lineTo(x1, y1 + offsetY);
      ctx.closePath();
      ctx.fill();
      ctx.globalAlpha = 1;
    }

    // Anchor points
    this.drawAnchor(ctx, x1, y1, drawing.style.color);
    this.drawAnchor(ctx, x2, y2, drawing.style.color);
    this.drawAnchor(ctx, x3, y3, drawing.style.color);
  }

  private drawRegressionChannel(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 3) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];
    const [x3, y3] = coords[2];

    // Calculate regression line (simplified - just uses two points)
    const slope = (y2 - y1) / (x2 - x1);

    // Calculate channel width based on third point
    const channelWidth = Math.abs(y3 - y1);

    // Draw center regression line
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();

    // Draw upper channel line
    ctx.beginPath();
    ctx.moveTo(x1, y1 - channelWidth);
    ctx.lineTo(x2, y2 - channelWidth);
    ctx.stroke();

    // Draw lower channel line
    ctx.beginPath();
    ctx.moveTo(x1, y1 + channelWidth);
    ctx.lineTo(x2, y2 + channelWidth);
    ctx.stroke();

    // Fill channel
    if (drawing.style.fillColor) {
      ctx.fillStyle = drawing.style.fillColor;
      ctx.globalAlpha = drawing.style.fillOpacity || 0.1;

      // Upper half
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.lineTo(x2, y2 - channelWidth);
      ctx.lineTo(x1, y1 - channelWidth);
      ctx.closePath();
      ctx.fill();

      // Lower half
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.lineTo(x2, y2 + channelWidth);
      ctx.lineTo(x1, y1 + channelWidth);
      ctx.closePath();
      ctx.fill();

      ctx.globalAlpha = 1;
    }

    // Anchor points
    this.drawAnchor(ctx, x1, y1, drawing.style.color);
    this.drawAnchor(ctx, x2, y2, drawing.style.color);
    this.drawAnchor(ctx, x3, y3, drawing.style.color);
  }

  private drawPitchfork(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 3) return;

    // Andrew's Pitchfork: 3 points define the fork
    // Point 1: Starting point (handle)
    // Point 2: First prong end
    // Point 3: Second prong end
    const [x1, y1] = coords[0]; // Handle/pivot point
    const [x2, y2] = coords[1]; // First peak/trough
    const [x3, y3] = coords[2]; // Second peak/trough

    // Calculate median line (from pivot to midpoint of peaks)
    const midX = (x2 + x3) / 2;
    const midY = (y2 + y3) / 2;

    // Extend median line
    const medianDx = midX - x1;
    const medianDy = midY - y1;
    const extendFactor = 2;
    const medianEndX = x1 + medianDx * extendFactor;
    const medianEndY = y1 + medianDy * extendFactor;

    // Draw median line (center prong)
    ctx.setLineDash([5, 5]); // Dashed for median
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(medianEndX, medianEndY);
    ctx.stroke();
    ctx.setLineDash([]); // Reset

    // Draw upper prong (from pivot through first peak)
    const upperDx = x2 - x1;
    const upperDy = y2 - y1;
    const upperEndX = x1 + upperDx * extendFactor;
    const upperEndY = y1 + upperDy * extendFactor;

    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(upperEndX, upperEndY);
    ctx.stroke();

    // Draw lower prong (from pivot through second peak)
    const lowerDx = x3 - x1;
    const lowerDy = y3 - y1;
    const lowerEndX = x1 + lowerDx * extendFactor;
    const lowerEndY = y1 + lowerDy * extendFactor;

    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(lowerEndX, lowerEndY);
    ctx.stroke();

    // Fill the fork area
    if (drawing.style.fillColor) {
      ctx.fillStyle = drawing.style.fillColor;
      ctx.globalAlpha = drawing.style.fillOpacity || 0.05;
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(upperEndX, upperEndY);
      ctx.lineTo(lowerEndX, lowerEndY);
      ctx.closePath();
      ctx.fill();
      ctx.globalAlpha = 1;
    }

    // Anchor points
    this.drawAnchor(ctx, x1, y1, drawing.style.color);
    this.drawAnchor(ctx, x2, y2, drawing.style.color);
    this.drawAnchor(ctx, x3, y3, drawing.style.color);
  }

  private drawRectangle(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 2) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    const x = Math.min(x1, x2);
    const y = Math.min(y1, y2);
    const width = Math.abs(x2 - x1);
    const height = Math.abs(y2 - y1);

    // Fill
    if (drawing.style.fillColor) {
      ctx.fillStyle = drawing.style.fillColor;
      ctx.globalAlpha = drawing.style.fillOpacity || 0.2;
      ctx.fillRect(x, y, width, height);
      ctx.globalAlpha = 1;
    }

    // Border
    ctx.strokeRect(x, y, width, height);

    // Anchors
    this.drawAnchor(ctx, x1, y1, drawing.style.color);
    this.drawAnchor(ctx, x2, y2, drawing.style.color);
  }

  private drawEllipse(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 2) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    const centerX = (x1 + x2) / 2;
    const centerY = (y1 + y2) / 2;
    const radiusX = Math.abs(x2 - x1) / 2;
    const radiusY = Math.abs(y2 - y1) / 2;

    ctx.beginPath();
    ctx.ellipse(centerX, centerY, radiusX, radiusY, 0, 0, 2 * Math.PI);

    if (drawing.style.fillColor) {
      ctx.fillStyle = drawing.style.fillColor;
      ctx.globalAlpha = drawing.style.fillOpacity || 0.2;
      ctx.fill();
      ctx.globalAlpha = 1;
    }

    ctx.stroke();
  }

  private drawFibonacci(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 2) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    const levels = this._options.fibonacciLevels || [
      { level: 0, label: '0%', color: '#787878', showLabel: true },
      { level: 0.236, label: '23.6%', color: '#f23645', showLabel: true },
      { level: 0.382, label: '38.2%', color: '#ff9800', showLabel: true },
      { level: 0.5, label: '50%', color: '#fbbf24', showLabel: true },
      { level: 0.618, label: '61.8%', color: '#4caf50', showLabel: true },
      { level: 0.786, label: '78.6%', color: '#2196f3', showLabel: true },
      { level: 1, label: '100%', color: '#787878', showLabel: true },
    ];

    const priceRange = Math.abs(y2 - y1);
    const timeRange = Math.abs(x2 - x1);

    levels.forEach(level => {
      const y = y1 + (y2 - y1) * level.level;

      // Line
      ctx.strokeStyle = level.color;
      ctx.setLineDash([5, 5]);
      ctx.beginPath();
      ctx.moveTo(x1, y);
      ctx.lineTo(x2, y);
      ctx.stroke();
      ctx.setLineDash([]);

      // Label
      if (level.showLabel) {
        ctx.fillStyle = level.color;
        ctx.font = '11px monospace';
        ctx.fillText(level.label, x2 + 5, y + 4);
      }
    });

    // Main lines
    ctx.strokeStyle = drawing.style.color;
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y1);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(x1, y2);
    ctx.lineTo(x2, y2);
    ctx.stroke();
  }

  private drawPositionBox(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 2) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    const isLong = drawing.type === DrawingTool.LONG_POSITION;
    const color = isLong ? (this._options.longColor || '#10b981') : (this._options.shortColor || '#ef4444');

    // Position rectangle
    ctx.fillStyle = color + '20';
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;

    const x = Math.min(x1, x2);
    const y = Math.min(y1, y2);
    const width = Math.abs(x2 - x1);
    const height = Math.abs(y2 - y1);

    ctx.fillRect(x, y, width, height);
    ctx.strokeRect(x, y, width, height);

    // Label
    ctx.fillStyle = color;
    ctx.font = 'bold 12px monospace';
    ctx.fillText(isLong ? 'LONG' : 'SHORT', x + 5, y + 15);

    // PnL if available
    if (drawing.data?.pnl !== undefined) {
      const pnlColor = drawing.data.pnl >= 0 ? this._options.profitColor || '#10b981' : this._options.lossColor || '#ef4444';
      ctx.fillStyle = pnlColor;
      ctx.fillText(`${drawing.data.pnl >= 0 ? '+' : ''}${drawing.data.pnl.toFixed(2)}%`, x + 5, y + 30);
    }
  }

  private drawText(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 1 || !drawing.data?.text) return;

    const [x, y] = coords[0];

    ctx.fillStyle = drawing.style.textColor || drawing.style.color;
    ctx.font = `bold ${drawing.style.fontSize || 14}px monospace`;
    ctx.fillText(drawing.data.text, x, y);

    // Draw anchor point
    this.drawAnchor(ctx, x, y, drawing.style.color);
  }

  private drawCallout(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 1 || !drawing.data?.text) return;

    const [x, y] = coords[0];
    const text = drawing.data.text;

    // Measure text
    ctx.font = `${drawing.style.fontSize || 14}px monospace`;
    const metrics = ctx.measureText(text);
    const textWidth = metrics.width;
    const textHeight = drawing.style.fontSize || 14;

    // Callout box dimensions with padding
    const padding = 8;
    const boxWidth = textWidth + padding * 2;
    const boxHeight = textHeight + padding * 2;

    // Draw callout box background
    ctx.fillStyle = 'rgba(0, 0, 0, 0.8)';
    ctx.fillRect(x, y - boxHeight, boxWidth, boxHeight);

    // Draw box border
    ctx.strokeStyle = drawing.style.color;
    ctx.lineWidth = 2;
    ctx.strokeRect(x, y - boxHeight, boxWidth, boxHeight);

    // Draw pointer line from box to anchor point
    const pointerY = y + 15;
    ctx.beginPath();
    ctx.moveTo(x + boxWidth / 2, y);
    ctx.lineTo(x + boxWidth / 2, pointerY);
    ctx.stroke();

    // Draw text inside box
    ctx.fillStyle = drawing.style.textColor || '#FFFFFF';
    ctx.fillText(text, x + padding, y - padding - 2);

    // Draw anchor point at the bottom
    this.drawAnchor(ctx, x + boxWidth / 2, pointerY, drawing.style.color);
  }

  private drawArrow(
    ctx: CanvasRenderingContext2D,
    coords: Coordinate[][],
    drawing: DrawingObject
  ): void {
    if (coords.length < 2) return;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    // Arrow line
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();

    // Arrowhead
    const angle = Math.atan2(y2 - y1, x2 - x1);
    const headLength = 15;

    ctx.beginPath();
    ctx.moveTo(x2, y2);
    ctx.lineTo(
      x2 - headLength * Math.cos(angle - Math.PI / 6),
      y2 - headLength * Math.sin(angle - Math.PI / 6)
    );
    ctx.moveTo(x2, y2);
    ctx.lineTo(
      x2 - headLength * Math.cos(angle + Math.PI / 6),
      y2 - headLength * Math.sin(angle + Math.PI / 6)
    );
    ctx.stroke();
  }

  private drawPosition(
    ctx: CanvasRenderingContext2D,
    position: PositionMarker
  ): void {
    if (!this._chart || !this._series) return;

    const x = this._chart.timeScale().timeToCoordinate(position.time);
    const y = this._series.priceToCoordinate(position.price);

    if (x === null || y === null) return;

    const isLong = position.type === PositionType.LONG;
    const size = 12;

    // Triangle marker
    ctx.fillStyle = position.color;
    ctx.beginPath();

    if (isLong) {
      // Up triangle
      ctx.moveTo(x, y - size);
      ctx.lineTo(x - size / 2, y);
      ctx.lineTo(x + size / 2, y);
    } else {
      // Down triangle
      ctx.moveTo(x, y + size);
      ctx.lineTo(x - size / 2, y);
      ctx.lineTo(x + size / 2, y);
    }

    ctx.closePath();
    ctx.fill();

    // Border
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 1;
    ctx.stroke();

    // Label
    if (position.label) {
      ctx.fillStyle = '#ffffff';
      ctx.font = '10px monospace';
      ctx.fillText(position.label, x + 8, y + 4);
    }
  }

  private getCoordinates(points: ChartPoint<Time>[]): (Coordinate[] | null)[] {
    if (!this._chart || !this._series) return points.map(() => null);

    return points.map(point => {
      const x = this._chart!.timeScale().timeToCoordinate(point.time);
      const y = this._series!.priceToCoordinate(point.price);
      return x !== null && y !== null ? [x, y] : null;
    });
  }

  private applyStyle(ctx: CanvasRenderingContext2D, style: DrawingStyle): void {
    ctx.strokeStyle = style.color;
    ctx.lineWidth = style.lineWidth;

    switch (style.lineStyle) {
      case 'dashed':
        ctx.setLineDash([5, 5]);
        break;
      case 'dotted':
        ctx.setLineDash([2, 2]);
        break;
      default:
        ctx.setLineDash([]);
    }
  }

  private drawAnchor(ctx: CanvasRenderingContext2D, x: number, y: number, color: string): void {
    ctx.fillStyle = color;
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 2;

    ctx.beginPath();
    ctx.arc(x, y, 5, 0, 2 * Math.PI);
    ctx.fill();
    ctx.stroke();
  }

  // ==================== HIT DETECTION ====================

  public findDrawingAtPoint(x: number, y: number): string | null {
    const hitThreshold = 10; // Pixels

    // Check drawings in reverse order (most recent on top)
    for (let i = this._drawings.length - 1; i >= 0; i--) {
      const drawing = this._drawings[i];
      const coords = this.getCoordinates(drawing.points);

      if (coords.some(c => c === null)) continue;

      // Check based on drawing type
      switch (drawing.type) {
        case DrawingTool.TREND_LINE:
        case DrawingTool.RAY:
        case DrawingTool.HORIZONTAL_LINE:
        case DrawingTool.VERTICAL_LINE:
          if (this.hitTestLine(x, y, coords as Coordinate[][], hitThreshold)) {
            return drawing.id;
          }
          break;

        case DrawingTool.RECTANGLE:
        case DrawingTool.PARALLEL_CHANNEL:
        case DrawingTool.REGRESSION_CHANNEL:
          if (this.hitTestRectangle(x, y, coords as Coordinate[][], hitThreshold)) {
            return drawing.id;
          }
          break;

        case DrawingTool.ELLIPSE:
          if (this.hitTestEllipse(x, y, coords as Coordinate[][], hitThreshold)) {
            return drawing.id;
          }
          break;

        case DrawingTool.TEXT:
        case DrawingTool.CALLOUT:
          if (this.hitTestPoint(x, y, coords as Coordinate[][], hitThreshold)) {
            return drawing.id;
          }
          break;

        default:
          // For other types, check if near any anchor point
          if (this.hitTestAnyAnchor(x, y, coords as Coordinate[][], hitThreshold)) {
            return drawing.id;
          }
      }
    }

    return null;
  }

  public findAnchorAtPoint(drawingId: string, x: number, y: number): number {
    const drawing = this._drawings.find(d => d.id === drawingId);
    if (!drawing) return -1;

    const coords = this.getCoordinates(drawing.points);
    if (coords.some(c => c === null)) return -1;

    const hitThreshold = 12; // Slightly larger for anchors

    for (let i = 0; i < coords.length; i++) {
      const coord = coords[i];
      if (coord === null) continue;
      const [cx, cy] = coord;
      const distance = Math.sqrt((x - cx) ** 2 + (y - cy) ** 2);
      if (distance <= hitThreshold) {
        return i;
      }
    }

    return -1;
  }

  private hitTestLine(x: number, y: number, coords: Coordinate[][], threshold: number): boolean {
    if (coords.length < 2) return false;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    // Calculate distance from point to line segment
    const distance = this.pointToLineDistance(x, y, x1, y1, x2, y2);
    return distance <= threshold;
  }

  private hitTestRectangle(x: number, y: number, coords: Coordinate[][], threshold: number): boolean {
    if (coords.length < 2) return false;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    const minX = Math.min(x1, x2);
    const maxX = Math.max(x1, x2);
    const minY = Math.min(y1, y2);
    const maxY = Math.max(y1, y2);

    // Check if point is inside or near the rectangle edges
    if (x >= minX - threshold && x <= maxX + threshold &&
        y >= minY - threshold && y <= maxY + threshold) {
      // Inside bounding box
      const nearLeft = Math.abs(x - minX) <= threshold;
      const nearRight = Math.abs(x - maxX) <= threshold;
      const nearTop = Math.abs(y - minY) <= threshold;
      const nearBottom = Math.abs(y - maxY) <= threshold;

      // On an edge or inside
      return nearLeft || nearRight || nearTop || nearBottom ||
             (x >= minX && x <= maxX && y >= minY && y <= maxY);
    }

    return false;
  }

  private hitTestEllipse(x: number, y: number, coords: Coordinate[][], threshold: number): boolean {
    if (coords.length < 2) return false;

    const [x1, y1] = coords[0];
    const [x2, y2] = coords[1];

    const centerX = (x1 + x2) / 2;
    const centerY = (y1 + y2) / 2;
    const radiusX = Math.abs(x2 - x1) / 2;
    const radiusY = Math.abs(y2 - y1) / 2;

    // Ellipse equation: ((x-cx)/rx)^2 + ((y-cy)/ry)^2 = 1
    const normalized = ((x - centerX) / radiusX) ** 2 + ((y - centerY) / radiusY) ** 2;

    // Check if on or near the ellipse boundary
    return Math.abs(normalized - 1) * Math.min(radiusX, radiusY) <= threshold;
  }

  private hitTestPoint(x: number, y: number, coords: Coordinate[][], threshold: number): boolean {
    if (coords.length < 1) return false;
    const [cx, cy] = coords[0];
    const distance = Math.sqrt((x - cx) ** 2 + (y - cy) ** 2);
    return distance <= threshold * 2; // Larger hit area for text
  }

  private hitTestAnyAnchor(x: number, y: number, coords: Coordinate[][], threshold: number): boolean {
    for (const [cx, cy] of coords) {
      const distance = Math.sqrt((x - cx) ** 2 + (y - cy) ** 2);
      if (distance <= threshold) {
        return true;
      }
    }
    return false;
  }

  private pointToLineDistance(px: number, py: number, x1: number, y1: number, x2: number, y2: number): number {
    const A = px - x1;
    const B = py - y1;
    const C = x2 - x1;
    const D = y2 - y1;

    const dot = A * C + B * D;
    const lenSq = C * C + D * D;
    let param = -1;

    if (lenSq !== 0) {
      param = dot / lenSq;
    }

    let xx, yy;

    if (param < 0) {
      xx = x1;
      yy = y1;
    } else if (param > 1) {
      xx = x2;
      yy = y2;
    } else {
      xx = x1 + param * C;
      yy = y1 + param * D;
    }

    const dx = px - xx;
    const dy = py - yy;
    return Math.sqrt(dx * dx + dy * dy);
  }
}

// ==================== PRIMITIVE ====================

class ToolkitPrimitive implements ISeriesPrimitive<Time> {
  private _paneViews: ToolkitPaneView[];
  private _drawings: DrawingObject[] = [];
  private _positions: PositionMarker[] = [];
  private _options: ToolkitOptions;
  private _param: SeriesAttachedParameter<Time> | null = null;

  constructor(options: ToolkitOptions) {
    this._options = options;
    this._paneViews = [new ToolkitPaneView(options)];
  }

  attached(param: SeriesAttachedParameter<Time>): void {
    this._param = param;
    this.updateAllViews();
  }

  updateAllViews(): void {
    if (!this._param) return;
    this._paneViews.forEach(view =>
      view.update(this._drawings, this._positions, this._param!.chart, this._param!.series)
    );
    this._param.requestUpdate();
  }

  paneViews(): readonly IPrimitivePaneView[] {
    return this._paneViews;
  }

  addDrawing(drawing: DrawingObject): void {
    this._drawings.push(drawing);
    this.updateAllViews();
  }

  removeDrawing(id: string): void {
    this._drawings = this._drawings.filter(d => d.id !== id);
    this.updateAllViews();
  }

  addPosition(position: PositionMarker): void {
    this._positions.push(position);
    this.updateAllViews();
  }

  removePosition(id: string): void {
    this._positions = this._positions.filter(p => p.id !== id);
    this.updateAllViews();
  }

  getDrawings(): DrawingObject[] {
    return [...this._drawings];
  }

  getPositions(): PositionMarker[] {
    return [...this._positions];
  }

  clearAll(): void {
    this._drawings = [];
    this._positions = [];
    this.updateAllViews();
  }

  // Selection methods
  setSelectedDrawing(id: string | null): void {
    this._paneViews.forEach(view => view.setSelectedDrawing(id));
    this.updateAllViews();
  }

  setHoveredDrawing(id: string | null): void {
    this._paneViews.forEach(view => view.setHoveredDrawing(id));
    this.updateAllViews();
  }

  findDrawingAtPoint(x: number, y: number): string | null {
    return this._paneViews[0]?.findDrawingAtPoint(x, y) || null;
  }

  findAnchorAtPoint(drawingId: string, x: number, y: number): number {
    return this._paneViews[0]?.findAnchorAtPoint(drawingId, x, y) || -1;
  }

  // Update drawing coordinates (for dragging)
  updateDrawingPoint(drawingId: string, pointIndex: number, newPoint: ChartPoint<Time>): void {
    const drawing = this._drawings.find(d => d.id === drawingId);
    if (drawing && pointIndex >= 0 && pointIndex < drawing.points.length) {
      drawing.points[pointIndex] = newPoint;
      this.updateAllViews();
    }
  }

  // Move entire drawing
  moveDrawing(drawingId: string, deltaTime: number, deltaPrice: number): void {
    const drawing = this._drawings.find(d => d.id === drawingId);
    if (drawing) {
      drawing.points = drawing.points.map(point => ({
        time: (point.time as number + deltaTime) as Time,
        price: point.price + deltaPrice,
      }));
      this.updateAllViews();
    }
  }
}

// ==================== MAIN PLUGIN ====================

export class TradingToolkitPlugin extends BasePlugin<Time> {
  private toolkitPrimitive: ToolkitPrimitive | null = null;
  private currentTool: DrawingTool | null = null;
  private tempPoints: ChartPoint<Time>[] = [];
  private drawingIdCounter: number = 0;
  private positionIdCounter: number = 0;
  private onToolDeactivated?: () => void;

  // Selection and drag state
  private selectedDrawingId: string | null = null;
  private isDragging: boolean = false;
  private draggedAnchorIndex: number = -1;
  private dragStartPoint: ChartPoint<Time> | null = null;

  constructor(options: Partial<ToolkitOptions> = {}) {
    const defaultOptions: ToolkitOptions = {
      visible: true,
      interactive: true,
      defaultLineColor: '#2196F3',
      defaultFillColor: '#2196F320',
      defaultTextColor: '#ffffff',
      defaultLineWidth: 2,
      longColor: '#10b981',
      shortColor: '#ef4444',
      profitColor: '#10b981',
      lossColor: '#ef4444',
      snapToPrice: true,
      snapToTime: false,
      showMagnetMode: true,
      ...options
    };

    super(
      {
        id: 'trading-toolkit',
        name: 'Trading Toolkit',
        description: 'Complete TradingView-style drawing and analysis tools',
        category: PluginCategory.DRAWING,
        version: '1.0.0',
        author: 'Fincept Terminal',
        enabled: true
      },
      defaultOptions
    );
  }

  protected createPrimitive(): ISeriesPrimitive<Time> | null {
    this.toolkitPrimitive = new ToolkitPrimitive(this.options as ToolkitOptions);
    return this.toolkitPrimitive;
  }

  // ==================== TOOL ACTIVATION ====================

  public activateTool(tool: DrawingTool): void {
    this.currentTool = tool;
    this.tempPoints = [];
    // Tool activated: ${tool}
  }

  public deactivateTool(): void {
    this.currentTool = null;
    this.tempPoints = [];

    // Notify parent component that tool was deactivated
    if (this.onToolDeactivated) {
      this.onToolDeactivated();
    }
  }

  public getCurrentTool(): DrawingTool | null {
    return this.currentTool;
  }

  public setOnToolDeactivated(callback: () => void): void {
    this.onToolDeactivated = callback;
  }

  // ==================== SELECTION & DRAGGING ====================

  public handleMouseDown(point: ChartPoint<Time>, pixelX: number, pixelY: number): boolean {
    if (!this.toolkitPrimitive) return false;

    // If a drawing tool is active, let the normal click handler work
    if (this.currentTool) return false;

    // Check if clicking on an existing drawing
    const hitDrawingId = this.toolkitPrimitive.findDrawingAtPoint(pixelX, pixelY);

    if (hitDrawingId) {
      this.selectedDrawingId = hitDrawingId;
      this.toolkitPrimitive.setSelectedDrawing(hitDrawingId);

      // Check if clicking on an anchor point
      const anchorIndex = this.toolkitPrimitive.findAnchorAtPoint(hitDrawingId, pixelX, pixelY);

      if (anchorIndex >= 0) {
        // Start dragging anchor
        this.isDragging = true;
        this.draggedAnchorIndex = anchorIndex;
        this.dragStartPoint = point;
        return true; // Handled
      } else {
        // Start dragging entire drawing
        this.isDragging = true;
        this.draggedAnchorIndex = -1;
        this.dragStartPoint = point;
        return true; // Handled
      }
    } else {
      // Clicked empty space - deselect
      this.selectedDrawingId = null;
      this.toolkitPrimitive.setSelectedDrawing(null);
    }

    return false;
  }

  public handleMouseMove(point: ChartPoint<Time>, pixelX: number, pixelY: number): void {
    if (!this.toolkitPrimitive) return;

    // Handle dragging
    if (this.isDragging && this.dragStartPoint && this.selectedDrawingId) {
      const deltaTime = (point.time as number) - (this.dragStartPoint.time as number);
      const deltaPrice = point.price - this.dragStartPoint.price;

      if (this.draggedAnchorIndex >= 0) {
        // Dragging a specific anchor point
        this.toolkitPrimitive.updateDrawingPoint(
          this.selectedDrawingId,
          this.draggedAnchorIndex,
          point
        );
      } else {
        // Dragging entire drawing
        this.toolkitPrimitive.moveDrawing(this.selectedDrawingId, deltaTime, deltaPrice);
      }

      // Update drag start point for next move
      this.dragStartPoint = point;
      return;
    }

    // Handle hover (when not dragging and no tool active)
    if (!this.currentTool && !this.isDragging) {
      const hoveredDrawingId = this.toolkitPrimitive.findDrawingAtPoint(pixelX, pixelY);
      this.toolkitPrimitive.setHoveredDrawing(hoveredDrawingId);
    }
  }

  public handleMouseUp(): void {
    this.isDragging = false;
    this.draggedAnchorIndex = -1;
    this.dragStartPoint = null;
  }

  public selectDrawing(id: string | null): void {
    this.selectedDrawingId = id;
    if (this.toolkitPrimitive) {
      this.toolkitPrimitive.setSelectedDrawing(id);
    }
  }

  public getSelectedDrawingId(): string | null {
    return this.selectedDrawingId;
  }

  // ==================== DRAWING ====================

  public handleClick(point: ChartPoint<Time>): void {
    if (!this.currentTool || !this.toolkitPrimitive) return;

    this.tempPoints.push(point);

    const requiredPoints = this.getRequiredPoints(this.currentTool);

    if (this.tempPoints.length >= requiredPoints) {
      this.completeDrawing();
    }
  }

  private completeDrawing(): void {
    if (!this.currentTool || !this.toolkitPrimitive) return;

    const drawing: DrawingObject = {
      id: `drawing-${++this.drawingIdCounter}`,
      type: this.currentTool,
      points: [...this.tempPoints],
      style: {
        color: (this.options as ToolkitOptions).defaultLineColor || '#2196F3',
        lineWidth: (this.options as ToolkitOptions).defaultLineWidth || 2,
        lineStyle: 'solid',
        fillColor: (this.options as ToolkitOptions).defaultFillColor,
        fillOpacity: 0.2,
        textColor: (this.options as ToolkitOptions).defaultTextColor || '#FFFFFF',
        fontSize: 14,
        showLabels: true,
      },
      visible: true,
      locked: false,
    };

    // For text-based annotations, prompt for text input
    if (this.currentTool === DrawingTool.TEXT || this.currentTool === DrawingTool.CALLOUT) {
      const text = prompt('Enter annotation text:');
      if (text) {
        drawing.data = { text };
      } else {
        // User cancelled, don't create the drawing
        this.tempPoints = [];
        return;
      }
    }

    // Drawing completed
    this.toolkitPrimitive.addDrawing(drawing);
    this.tempPoints = [];

    // Auto-deselect tool after completing drawing (TradingView behavior)
    this.deactivateTool();

    // Force redraw
    if (this.chart) {
      this.chart.timeScale().scrollToPosition(0, false);
    }
  }

  private getRequiredPoints(tool: DrawingTool): number {
    switch (tool) {
      case DrawingTool.HORIZONTAL_LINE:
      case DrawingTool.VERTICAL_LINE:
      case DrawingTool.TEXT:
      case DrawingTool.CALLOUT:
        return 1;
      case DrawingTool.TREND_LINE:
      case DrawingTool.RAY:
      case DrawingTool.RECTANGLE:
      case DrawingTool.ELLIPSE:
      case DrawingTool.ARROW:
      case DrawingTool.FIBO_RETRACEMENT:
      case DrawingTool.FIBO_EXTENSION:
      case DrawingTool.LONG_POSITION:
      case DrawingTool.SHORT_POSITION:
        return 2;
      case DrawingTool.PARALLEL_CHANNEL:
      case DrawingTool.REGRESSION_CHANNEL:
      case DrawingTool.TRIANGLE:
      case DrawingTool.PITCHFORK:
        return 3;
      default:
        return 2;
    }
  }

  // ==================== POSITIONS ====================

  public addLongPosition(
    entryTime: Time,
    entryPrice: number,
    exitTime?: Time,
    exitPrice?: number,
    options?: Partial<PositionMarker>
  ): string {
    return this.addPosition(PositionType.LONG, OrderType.ENTRY, entryTime, entryPrice, options);
  }

  public addShortPosition(
    entryTime: Time,
    entryPrice: number,
    exitTime?: Time,
    exitPrice?: number,
    options?: Partial<PositionMarker>
  ): string {
    return this.addPosition(PositionType.SHORT, OrderType.ENTRY, entryTime, entryPrice, options);
  }

  public addPosition(
    type: PositionType,
    orderType: OrderType,
    time: Time,
    price: number,
    options?: Partial<PositionMarker>
  ): string {
    if (!this.toolkitPrimitive) return '';

    const color = type === PositionType.LONG
      ? ((this.options as ToolkitOptions).longColor || '#10b981')
      : ((this.options as ToolkitOptions).shortColor || '#ef4444');

    const position: PositionMarker = {
      id: `position-${++this.positionIdCounter}`,
      type,
      orderType,
      time,
      price,
      color,
      ...options,
    };

    this.toolkitPrimitive.addPosition(position);
    this.chart?.timeScale().fitContent();

    return position.id;
  }

  // ==================== MANAGEMENT ====================

  public getDrawings(): DrawingObject[] {
    return this.toolkitPrimitive?.getDrawings() || [];
  }

  public getPositions(): PositionMarker[] {
    return this.toolkitPrimitive?.getPositions() || [];
  }

  public removeDrawing(id: string): void {
    if (this.toolkitPrimitive) {
      this.toolkitPrimitive.removeDrawing(id);
      this.chart?.timeScale().fitContent();
    }
  }

  public removePosition(id: string): void {
    if (this.toolkitPrimitive) {
      this.toolkitPrimitive.removePosition(id);
      this.chart?.timeScale().fitContent();
    }
  }

  public clearAll(): void {
    if (this.toolkitPrimitive) {
      this.toolkitPrimitive.clearAll();
      this.chart?.timeScale().fitContent();
    }
  }

  public clearDrawings(): void {
    if (this.toolkitPrimitive) {
      this.getDrawings().forEach(d => this.removeDrawing(d.id));
    }
  }

  public deleteLastDrawing(): void {
    const drawings = this.getDrawings();
    if (drawings.length > 0) {
      this.removeDrawing(drawings[drawings.length - 1].id);
    }
  }

  public clearPositions(): void {
    if (this.toolkitPrimitive) {
      this.getPositions().forEach(p => this.removePosition(p.id));
    }
  }
}

// Export types
export type { DrawingObject, DrawingStyle, PositionMarker, ToolkitOptions };
