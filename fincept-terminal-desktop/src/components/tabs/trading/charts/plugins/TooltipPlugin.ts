/**
 * Tooltip Plugin
 *
 * Displays a customizable tooltip with price and time information when hovering over the chart.
 * Based on https://tradingview.github.io/lightweight-charts/plugin-examples/tooltip
 */

import type {
  ISeriesPrimitive,
  Time,
  IPrimitivePaneView
} from 'lightweight-charts';
import { BasePlugin } from './BasePlugin';
import { PluginCategory, PluginOptions } from './types';

interface TooltipOptions extends PluginOptions {
  backgroundColor?: string;
  textColor?: string;
  borderColor?: string;
  borderWidth?: number;
  borderRadius?: number;
  padding?: number;
  fontSize?: number;
  fontFamily?: string;
  followCursor?: boolean;
  offsetX?: number;
  offsetY?: number;
  showTime?: boolean;
  showPrice?: boolean;
  showOHLC?: boolean;
  showVolume?: boolean;
  customFormatter?: (data: any) => string;
}

class TooltipPaneView implements IPrimitivePaneView {
  private _data: any = null;
  private _options: TooltipOptions;

  constructor(options: TooltipOptions) {
    this._options = options;
  }

  update(data: any): void {
    this._data = data;
  }

  renderer(): any {
    return {
      draw: (target: any) => {
        if (!this._data || !this._data.visible) return;

        target.useBitmapCoordinateSpace((scope: any) => {
          const ctx = scope.context;
          const x = this._data.x + (this._options.offsetX || 10);
          const y = this._data.y + (this._options.offsetY || 10);

        // Prepare text
        const lines: string[] = [];

        if (this._options.customFormatter) {
          lines.push(this._options.customFormatter(this._data));
        } else {
          if (this._options.showTime && this._data.time) {
            lines.push(`Time: ${this.formatTime(this._data.time)}`);
          }

          if (this._options.showOHLC && this._data.ohlc) {
            lines.push(`O: ${this._data.ohlc.open.toFixed(2)}`);
            lines.push(`H: ${this._data.ohlc.high.toFixed(2)}`);
            lines.push(`L: ${this._data.ohlc.low.toFixed(2)}`);
            lines.push(`C: ${this._data.ohlc.close.toFixed(2)}`);
          } else if (this._options.showPrice && this._data.price !== undefined) {
            lines.push(`Price: ${this._data.price.toFixed(2)}`);
          }

          if (this._options.showVolume && this._data.volume !== undefined) {
            lines.push(`Vol: ${this.formatVolume(this._data.volume)}`);
          }
        }

        if (lines.length === 0) return;

        // Style
        const fontSize = this._options.fontSize || 12;
        const fontFamily = this._options.fontFamily || 'monospace';
        const padding = this._options.padding || 8;
        const lineHeight = fontSize * 1.4;

        ctx.font = `${fontSize}px ${fontFamily}`;

        // Measure text
        const maxWidth = Math.max(...lines.map(line => ctx.measureText(line).width));
        const tooltipWidth = maxWidth + padding * 2;
        const tooltipHeight = lines.length * lineHeight + padding * 2;

        // Draw background
        ctx.fillStyle = this._options.backgroundColor || 'rgba(0, 0, 0, 0.8)';
        ctx.strokeStyle = this._options.borderColor || 'rgba(255, 255, 255, 0.2)';
        ctx.lineWidth = this._options.borderWidth || 1;

        const radius = this._options.borderRadius || 4;
        this.roundRect(ctx, x, y, tooltipWidth, tooltipHeight, radius);
        ctx.fill();
        ctx.stroke();

          // Draw text
          ctx.fillStyle = this._options.textColor || '#ffffff';
          lines.forEach((line, index) => {
            ctx.fillText(line, x + padding, y + padding + (index + 1) * lineHeight - 4);
          });
        });
      }
    };
  }

  private roundRect(
    ctx: CanvasRenderingContext2D,
    x: number,
    y: number,
    width: number,
    height: number,
    radius: number
  ): void {
    ctx.beginPath();
    ctx.moveTo(x + radius, y);
    ctx.lineTo(x + width - radius, y);
    ctx.quadraticCurveTo(x + width, y, x + width, y + radius);
    ctx.lineTo(x + width, y + height - radius);
    ctx.quadraticCurveTo(x + width, y + height, x + width - radius, y + height);
    ctx.lineTo(x + radius, y + height);
    ctx.quadraticCurveTo(x, y + height, x, y + height - radius);
    ctx.lineTo(x, y + radius);
    ctx.quadraticCurveTo(x, y, x + radius, y);
    ctx.closePath();
  }

  private formatTime(time: Time): string {
    if (typeof time === 'number') {
      return new Date(time * 1000).toLocaleString();
    }
    if (typeof time === 'object' && 'year' in time) {
      return `${time.year}-${String(time.month).padStart(2, '0')}-${String(time.day).padStart(2, '0')}`;
    }
    return String(time);
  }

  private formatVolume(volume: number): string {
    if (volume >= 1e9) return (volume / 1e9).toFixed(2) + 'B';
    if (volume >= 1e6) return (volume / 1e6).toFixed(2) + 'M';
    if (volume >= 1e3) return (volume / 1e3).toFixed(2) + 'K';
    return volume.toFixed(0);
  }
}

class TooltipPrimitive implements ISeriesPrimitive<Time> {
  private _paneViews: TooltipPaneView[];
  private _options: TooltipOptions;

  constructor(options: TooltipOptions) {
    this._options = options;
    this._paneViews = [new TooltipPaneView(options)];
  }

  updateAllViews(): void {
    this._paneViews.forEach(view => view.update(null));
  }

  paneViews(): readonly IPrimitivePaneView[] {
    return this._paneViews;
  }

  update(data: any): void {
    this._paneViews[0].update(data);
  }

  updateOptions(options: TooltipOptions): void {
    this._options = { ...this._options, ...options };
  }
}

export class TooltipPlugin extends BasePlugin<Time> {
  private tooltipPrimitive: TooltipPrimitive | null = null;

  constructor(options: Partial<TooltipOptions> = {}) {
    const defaultOptions: TooltipOptions = {
      visible: true,
      backgroundColor: 'rgba(0, 0, 0, 0.85)',
      textColor: '#ffffff',
      borderColor: 'rgba(255, 255, 255, 0.2)',
      borderWidth: 1,
      borderRadius: 6,
      padding: 10,
      fontSize: 11,
      fontFamily: 'monospace',
      followCursor: true,
      offsetX: 15,
      offsetY: 15,
      showTime: true,
      showPrice: false,
      showOHLC: true,
      showVolume: true,
      ...options
    };

    super(
      {
        id: 'tooltip',
        name: 'Tooltip',
        description: 'Displays price and time information on hover',
        category: PluginCategory.INTERACTIVE,
        version: '1.0.0',
        author: 'Fincept Terminal',
        enabled: true
      },
      defaultOptions
    );
  }

  protected getDefaultOptions(): PluginOptions {
    return {
      visible: true,
      showTime: true,
      showOHLC: true,
      showVolume: true,
    };
  }

  protected createPrimitive(): ISeriesPrimitive<Time> | null {
    this.tooltipPrimitive = new TooltipPrimitive(this.options as TooltipOptions);
    return this.tooltipPrimitive;
  }

  /**
   * Update tooltip with crosshair data
   */
  public updateTooltip(data: any): void {
    if (this.tooltipPrimitive) {
      this.tooltipPrimitive.update(data);
    }
  }

  protected onOptionsChanged(): void {
    if (this.tooltipPrimitive) {
      this.tooltipPrimitive.updateOptions(this.options as TooltipOptions);
    }
    super.onOptionsChanged();
  }
}
