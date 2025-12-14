/**
 * Vertical Line Plugin
 *
 * Draws vertical lines at specific time points for marking important events.
 * Based on https://tradingview.github.io/lightweight-charts/plugin-examples/vertical-line
 */

import type {
  ISeriesPrimitive,
  SeriesAttachedParameter,
  Time,
  IPrimitivePaneView,
  Coordinate,
  IChartApi
} from 'lightweight-charts';
import { BasePlugin } from './BasePlugin';
import { PluginCategory, PluginOptions } from './types';

interface VerticalLineOptions extends PluginOptions {
  showLabel?: boolean;
  labelBackgroundColor?: string;
  labelTextColor?: string;
  dashArray?: number[];
}

interface VerticalLineMarker {
  id: string;
  time: Time;
  color: string;
  lineWidth: number;
  label?: string;
  labelPosition?: 'top' | 'bottom';
}

class VerticalLinePaneView implements IPrimitivePaneView {
  private _markers: VerticalLineMarker[] = [];
  private _options: VerticalLineOptions;
  private _chart: IChartApi | null = null;

  constructor(options: VerticalLineOptions) {
    this._options = options;
  }

  update(markers: VerticalLineMarker[], chart: IChartApi): void {
    this._markers = markers;
    this._chart = chart;
  }

  renderer(): any {
    return {
      draw: (target: any) => {
        if (!this._chart) return;

        target.useBitmapCoordinateSpace((scope: any) => {
          const ctx = scope.context;
          const height = scope.bitmapSize.height;

          this._markers.forEach(marker => {
            const x = this._chart!.timeScale().timeToCoordinate(marker.time);
            if (x === null) return;

            // Draw vertical line
            ctx.strokeStyle = marker.color;
            ctx.lineWidth = marker.lineWidth;

            if (this._options.dashArray) {
              ctx.setLineDash(this._options.dashArray);
            } else {
              ctx.setLineDash([]);
            }

            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
            ctx.stroke();

            // Draw label if exists
            if (marker.label && this._options.showLabel) {
              this.drawLabel(ctx, x, marker, height);
            }
          });
        });
      }
    };
  }

  private drawLabel(
    ctx: CanvasRenderingContext2D,
    x: number,
    marker: VerticalLineMarker,
    height: number
  ): void {
    const fontSize = 11;
    const padding = 6;
    const labelY = marker.labelPosition === 'bottom' ? height - 25 : 10;

    ctx.font = `${fontSize}px monospace`;
    const textWidth = ctx.measureText(marker.label!).width;

    // Background
    ctx.fillStyle = this._options.labelBackgroundColor || 'rgba(0, 0, 0, 0.8)';
    ctx.fillRect(x - textWidth / 2 - padding, labelY, textWidth + padding * 2, fontSize + padding * 2);

    // Border
    ctx.strokeStyle = marker.color;
    ctx.lineWidth = 1;
    ctx.strokeRect(x - textWidth / 2 - padding, labelY, textWidth + padding * 2, fontSize + padding * 2);

    // Text
    ctx.fillStyle = this._options.labelTextColor || '#ffffff';
    ctx.textAlign = 'center';
    ctx.fillText(marker.label!, x, labelY + fontSize + padding - 2);
  }
}

class VerticalLinePrimitive implements ISeriesPrimitive<Time> {
  private _paneViews: VerticalLinePaneView[];
  private _markers: VerticalLineMarker[] = [];
  private _options: VerticalLineOptions;
  private _param: SeriesAttachedParameter<Time> | null = null;

  constructor(options: VerticalLineOptions) {
    this._options = options;
    this._paneViews = [new VerticalLinePaneView(options)];
  }

  attached(param: SeriesAttachedParameter<Time>): void {
    this._param = param;
    this.updateAllViews();
  }

  updateAllViews(): void {
    if (!this._param) return;
    this._paneViews.forEach(view => view.update(this._markers, this._param!.chart));
    this._param.requestUpdate();
  }

  paneViews(): readonly IPrimitivePaneView[] {
    return this._paneViews;
  }

  addMarker(marker: VerticalLineMarker): void {
    this._markers.push(marker);
    this.updateAllViews();
  }

  removeMarker(markerId: string): void {
    this._markers = this._markers.filter(m => m.id !== markerId);
    this.updateAllViews();
  }

  getMarkers(): VerticalLineMarker[] {
    return [...this._markers];
  }

  clearMarkers(): void {
    this._markers = [];
    this.updateAllViews();
  }
}

export class VerticalLinePlugin extends BasePlugin<Time> {
  private verticalLinePrimitive: VerticalLinePrimitive | null = null;
  private markerIdCounter: number = 0;

  constructor(options: Partial<VerticalLineOptions> = {}) {
    const defaultOptions: VerticalLineOptions = {
      visible: true,
      color: '#3f51b5',
      lineWidth: 1,
      showLabel: true,
      labelBackgroundColor: 'rgba(0, 0, 0, 0.85)',
      labelTextColor: '#ffffff',
      dashArray: [5, 5],
      ...options
    };

    super(
      {
        id: 'verticalline',
        name: 'Vertical Line',
        description: 'Mark specific time points with vertical lines',
        category: PluginCategory.ANNOTATION,
        version: '1.0.0',
        author: 'Fincept Terminal',
        enabled: true
      },
      defaultOptions
    );
  }

  protected createPrimitive(): ISeriesPrimitive<Time> | null {
    this.verticalLinePrimitive = new VerticalLinePrimitive(this.options as VerticalLineOptions);
    return this.verticalLinePrimitive;
  }

  /**
   * Add a vertical line marker at a specific time
   */
  public addMarker(
    time: Time,
    label?: string,
    color?: string,
    labelPosition?: 'top' | 'bottom'
  ): string {
    if (!this.verticalLinePrimitive) return '';

    const markerId = `vline-${++this.markerIdCounter}`;
    const marker: VerticalLineMarker = {
      id: markerId,
      time,
      color: color || this.options.color || '#3f51b5',
      lineWidth: this.options.lineWidth || 1,
      label,
      labelPosition: labelPosition || 'top',
    };

    this.verticalLinePrimitive.addMarker(marker);
    this.chart?.timeScale().fitContent();

    return markerId;
  }

  /**
   * Remove a specific marker
   */
  public removeMarker(markerId: string): void {
    if (this.verticalLinePrimitive) {
      this.verticalLinePrimitive.removeMarker(markerId);
      this.chart?.timeScale().fitContent();
    }
  }

  /**
   * Get all markers
   */
  public getMarkers(): VerticalLineMarker[] {
    return this.verticalLinePrimitive?.getMarkers() || [];
  }

  /**
   * Clear all markers
   */
  public clear(): void {
    if (this.verticalLinePrimitive) {
      this.verticalLinePrimitive.clearMarkers();
      this.chart?.timeScale().fitContent();
    }
  }

  /**
   * Mark current time
   */
  public markNow(label: string = 'Now'): string {
    const now = Math.floor(Date.now() / 1000) as Time;
    return this.addMarker(now, label, '#ff9800');
  }
}
