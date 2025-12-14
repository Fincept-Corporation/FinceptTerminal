/**
 * Trend Line Plugin
 *
 * Interactive trend line drawing tool for technical analysis.
 * Users can draw trend lines by clicking two points on the chart.
 * Based on https://tradingview.github.io/lightweight-charts/plugin-examples/trend-line
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
import { PluginCategory, PluginOptions, IDrawingPlugin, ChartPoint } from './types';

interface TrendLineOptions extends PluginOptions {
  extendLeft?: boolean;
  extendRight?: boolean;
  showPrice?: boolean;
  dashArray?: number[];
}

interface TrendLine {
  id: string;
  point1: ChartPoint<Time>;
  point2: ChartPoint<Time>;
  color: string;
  lineWidth: number;
  extendLeft: boolean;
  extendRight: boolean;
}

class TrendLinePaneView implements IPrimitivePaneView {
  private _lines: TrendLine[] = [];
  private _options: TrendLineOptions;
  private _chart: IChartApi | null = null;
  private _series: ISeriesApi<SeriesType, Time> | null = null;

  constructor(options: TrendLineOptions) {
    this._options = options;
  }

  update(lines: TrendLine[], chart: IChartApi, series: ISeriesApi<SeriesType, Time>): void {
    this._lines = lines;
    this._chart = chart;
    this._series = series;
  }

  renderer(): any {
    return {
      draw: (target: any) => {
        if (!this._chart || !this._series) return;

        target.useBitmapCoordinateSpace((scope: any) => {
          const ctx = scope.context;

          this._lines.forEach(line => {
            const x1 = this._chart!.timeScale().timeToCoordinate(line.point1.time);
            const y1 = this._series!.priceToCoordinate(line.point1.price);
            const x2 = this._chart!.timeScale().timeToCoordinate(line.point2.time);
            const y2 = this._series!.priceToCoordinate(line.point2.price);

            if (x1 === null || y1 === null || x2 === null || y2 === null) return;

            // Calculate slope
            const slope = (y2 - y1) / (x2 - x1);

            // Determine start and end points (as numbers, not Coordinates)
            let startX: number = x1 as number;
            let startY: number = y1 as number;
            let endX: number = x2 as number;
            let endY: number = y2 as number;

            if (line.extendLeft) {
              startX = 0;
              startY = (y1 as number) - slope * (x1 as number);
            }

            if (line.extendRight) {
              endX = scope.bitmapSize.width;
              endY = (y1 as number) + slope * (endX - (x1 as number));
            }

            // Draw line
            ctx.strokeStyle = line.color;
            ctx.lineWidth = line.lineWidth;

            if (this._options.dashArray) {
              ctx.setLineDash(this._options.dashArray);
            } else {
              ctx.setLineDash([]);
            }

            ctx.beginPath();
            ctx.moveTo(startX, startY);
            ctx.lineTo(endX, endY);
            ctx.stroke();

            // Draw anchor points
            this.drawAnchor(ctx, x1, y1, line.color);
            this.drawAnchor(ctx, x2, y2, line.color);
          });
        });
      }
    };
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
}

class TrendLinePrimitive implements ISeriesPrimitive<Time> {
  private _paneViews: TrendLinePaneView[];
  private _lines: TrendLine[] = [];
  private _options: TrendLineOptions;
  private _param: SeriesAttachedParameter<Time> | null = null;

  constructor(options: TrendLineOptions) {
    this._options = options;
    this._paneViews = [new TrendLinePaneView(options)];
  }

  attached(param: SeriesAttachedParameter<Time>): void {
    this._param = param;
    this.updateAllViews();
  }

  updateAllViews(): void {
    if (!this._param) return;
    this._paneViews.forEach(view => view.update(this._lines, this._param!.chart, this._param!.series));
    this._param.requestUpdate();
  }

  paneViews(): readonly IPrimitivePaneView[] {
    return this._paneViews;
  }

  addLine(line: TrendLine): void {
    this._lines.push(line);
    this.updateAllViews();
  }

  removeLine(lineId: string): void {
    this._lines = this._lines.filter(l => l.id !== lineId);
    this.updateAllViews();
  }

  getLines(): TrendLine[] {
    return [...this._lines];
  }

  clearLines(): void {
    this._lines = [];
    this.updateAllViews();
  }
}

export class TrendLinePlugin extends BasePlugin<Time> implements IDrawingPlugin<Time> {
  private trendLinePrimitive: TrendLinePrimitive | null = null;
  private isDrawingMode: boolean = false;
  private tempPoint1: ChartPoint<Time> | null = null;
  private lineIdCounter: number = 0;

  constructor(options: Partial<TrendLineOptions> = {}) {
    const defaultOptions: TrendLineOptions = {
      visible: true,
      interactive: true,
      color: '#2196F3',
      lineWidth: 2,
      extendLeft: false,
      extendRight: true,
      showPrice: true,
      dashArray: undefined,
      ...options
    };

    super(
      {
        id: 'trendline',
        name: 'Trend Line',
        description: 'Draw trend lines for technical analysis',
        category: PluginCategory.DRAWING,
        version: '1.0.0',
        author: 'Fincept Terminal',
        enabled: true
      },
      defaultOptions
    );
  }

  protected createPrimitive(): ISeriesPrimitive<Time> | null {
    this.trendLinePrimitive = new TrendLinePrimitive(this.options as TrendLineOptions);
    return this.trendLinePrimitive;
  }

  /**
   * Start drawing mode
   */
  public startDrawing(): void {
    this.isDrawingMode = true;
    this.tempPoint1 = null;
    console.log('Trend line drawing mode activated');
  }

  /**
   * Stop drawing mode
   */
  public stopDrawing(): void {
    this.isDrawingMode = false;
    this.tempPoint1 = null;
    console.log('Trend line drawing mode deactivated');
  }

  /**
   * Check if in drawing mode
   */
  public isDrawing(): boolean {
    return this.isDrawingMode;
  }

  /**
   * Handle click to add point
   */
  public handleClick(point: ChartPoint<Time>): void {
    if (!this.isDrawingMode || !this.trendLinePrimitive) return;

    if (!this.tempPoint1) {
      // First point
      this.tempPoint1 = point;
      console.log('First point set:', point);
    } else {
      // Second point - complete the line
      this.addLine(this.tempPoint1, point);
      this.tempPoint1 = null;

      // Optionally stop drawing after one line (can be configured)
      // this.stopDrawing();
    }
  }

  /**
   * Add a trend line
   */
  public addLine(point1: ChartPoint<Time>, point2: ChartPoint<Time>): void {
    if (!this.trendLinePrimitive) return;

    const line: TrendLine = {
      id: `trendline-${++this.lineIdCounter}`,
      point1,
      point2,
      color: this.options.color || '#2196F3',
      lineWidth: this.options.lineWidth || 2,
      extendLeft: (this.options as TrendLineOptions).extendLeft || false,
      extendRight: (this.options as TrendLineOptions).extendRight || true,
    };

    this.trendLinePrimitive.addLine(line);

    // Request update
    if (this.series) {
      this.chart?.timeScale().fitContent();
    }

    console.log('Trend line added:', line);
  }

  /**
   * Remove a specific line
   */
  public removeLine(lineId: string): void {
    if (this.trendLinePrimitive) {
      this.trendLinePrimitive.removeLine(lineId);
      this.chart?.timeScale().fitContent();
    }
  }

  /**
   * Get all lines
   */
  public getLines(): TrendLine[] {
    return this.trendLinePrimitive?.getLines() || [];
  }

  /**
   * Clear all lines
   */
  public clear(): void {
    if (this.trendLinePrimitive) {
      this.trendLinePrimitive.clearLines();
      this.chart?.timeScale().fitContent();
      console.log('All trend lines cleared');
    }
  }

  protected onOptionsChanged(): void {
    // Update existing lines with new options
    if (this.trendLinePrimitive) {
      const lines = this.trendLinePrimitive.getLines();
      lines.forEach(line => {
        line.color = this.options.color || line.color;
        line.lineWidth = this.options.lineWidth || line.lineWidth;
      });
    }
    super.onOptionsChanged();
  }
}
