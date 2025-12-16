/**
 * Professional Chart with Complete Trading Toolkit
 *
 * Ready-to-use chart component with full TradingView-style functionality
 */

import React, { useEffect, useRef, useState, useCallback } from 'react';
import { createChart, ColorType, CandlestickSeries, HistogramSeries } from 'lightweight-charts';
import type { IChartApi, CandlestickData, Time, ISeriesApi, MouseEventParams } from 'lightweight-charts';
import {
  PluginManager,
  TooltipPlugin,
  TradingToolkitPlugin,
  DrawingTool,
  PositionType,
  ChartPoint
} from './plugins';
import { ToolkitToolbar } from './ToolkitToolbar';

interface ProChartProps {
  data: Array<{
    time: number;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
  }>;
  symbol?: string;
  height?: number;
  showVolume?: boolean;
  showToolbar?: boolean;
  showHeader?: boolean; // Option to hide the symbol/OHLC header
  onToolChange?: (tool: DrawingTool | null) => void;
}

export function ProChartWithToolkit({
  data,
  symbol = 'CHART',
  height = 600,
  showVolume = true,
  showToolbar = true,
  showHeader = true,
  onToolChange,
}: ProChartProps) {
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const candlestickSeriesRef = useRef<ISeriesApi<'Candlestick', Time> | null>(null);
  const volumeSeriesRef = useRef<any>(null);
  const pluginManagerRef = useRef<PluginManager | null>(null);
  const toolkitRef = useRef<TradingToolkitPlugin | null>(null);
  const activeToolRef = useRef<DrawingTool | null>(null); // Ref to avoid stale closures

  const [activeTool, setActiveTool] = useState<DrawingTool | null>(null);
  const [isReady, setIsReady] = useState(false);
  const [cursorPosition, setCursorPosition] = useState<{ time: Time | null; price: number | null } | null>(null);
  const [cursorStyle, setCursorStyle] = useState<string>('default');

  // Initialize chart
  useEffect(() => {
    if (!chartContainerRef.current) return;

    const chart = createChart(chartContainerRef.current, {
      width: chartContainerRef.current.clientWidth,
      height: height,
      layout: {
        background: { type: ColorType.Solid, color: '#0a0a0a' },
        textColor: '#d1d5db',
        fontSize: 11,
      },
      handleScroll: {
        vertTouchDrag: true,
      },
      handleScale: {
        axisPressedMouseMove: {
          time: true,
          price: true,
        },
      },
      grid: {
        vertLines: { color: '#1f1f1f' },
        horzLines: { color: '#1f1f1f' },
      },
      crosshair: {
        mode: 0, // Normal mode (0) allows free cursor movement, Magnet mode (1) snaps to candles
        vertLine: {
          width: 1,
          color: '#758696',
          style: 0,
        },
        horzLine: {
          width: 1,
          color: '#758696',
          style: 0,
        },
      },
      rightPriceScale: {
        borderColor: '#2a2a2a',
        scaleMargins: {
          top: 0.1,
          bottom: showVolume ? 0.25 : 0.1,
        },
      },
      timeScale: {
        borderColor: '#2a2a2a',
        timeVisible: true,
        secondsVisible: false,
      },
    });

    chartRef.current = chart;

    // Create candlestick series
    const candlestickSeries = chart.addSeries(CandlestickSeries, {
      upColor: '#10b981',
      downColor: '#ef4444',
      borderUpColor: '#10b981',
      borderDownColor: '#ef4444',
      wickUpColor: '#10b981',
      wickDownColor: '#ef4444',
    });

    candlestickSeriesRef.current = candlestickSeries as any;

    // Create volume series
    if (showVolume) {
      const volumeSeries = chart.addSeries(HistogramSeries, {
        color: '#6b7280',
        priceFormat: {
          type: 'volume',
        },
        priceScaleId: '',
      });

      volumeSeries.priceScale().applyOptions({
        scaleMargins: {
          top: 0.8,
          bottom: 0,
        },
      });

      volumeSeriesRef.current = volumeSeries;
    }

    // Initialize plugin system
    const pluginManager = new PluginManager(chart, candlestickSeries as any);
    pluginManagerRef.current = pluginManager;

    // Add tooltip
    const tooltip = new TooltipPlugin({
      showOHLC: true,
      showVolume: showVolume,
    });
    pluginManager.register(tooltip);

    // Add trading toolkit
    const toolkit = new TradingToolkitPlugin();
    pluginManager.register(toolkit);
    toolkitRef.current = toolkit;

    // Listen for tool deactivation from the plugin
    toolkit.setOnToolDeactivated(() => {
      setActiveTool(null);
      activeToolRef.current = null;
      onToolChange?.(null);
    });

    // Chart initialized with plugins

    // Handle chart clicks for drawing
    chart.subscribeClick((param: MouseEventParams) => {
      handleChartClick(param);
    });

    // Handle mouse move for cursor tracking, hover, and dragging
    chart.subscribeCrosshairMove((param: MouseEventParams) => {
      handleMouseMove(param);
    });

    // Handle keyboard shortcuts
    const handleKeyDown = (e: KeyboardEvent) => {
      // Delete: Remove last drawing or selected drawing
      if (e.key === 'Delete' || e.key === 'Backspace') {
        if (toolkitRef.current && !activeTool) {
          e.preventDefault();
          const selectedId = toolkitRef.current.getSelectedDrawingId();
          if (selectedId) {
            // Delete selected drawing
            const drawings = toolkitRef.current.getDrawings();
            const drawing = drawings.find(d => d.id === selectedId);
            if (drawing) {
              toolkitRef.current.removeDrawing(selectedId);
              toolkitRef.current.selectDrawing(null);
            }
          } else {
            // Delete last drawing
            toolkitRef.current.deleteLastDrawing();
          }
        }
      }
      // Escape: Deactivate current tool or deselect drawing
      if (e.key === 'Escape') {
        e.preventDefault();
        if (activeTool) {
          handleToolSelect(null);
        } else if (toolkitRef.current) {
          toolkitRef.current.selectDrawing(null);
        }
      }
    };

    // Handle mouse up to end dragging
    const handleMouseUp = () => {
      if (toolkitRef.current) {
        toolkitRef.current.handleMouseUp();
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('mouseup', handleMouseUp);

    // Handle resize
    const handleResize = () => {
      if (chartContainerRef.current && chartRef.current) {
        chartRef.current.resize(chartContainerRef.current.clientWidth, height, true);
      }
    };

    window.addEventListener('resize', handleResize);

    const resizeObserver = new ResizeObserver(handleResize);
    if (chartContainerRef.current) {
      resizeObserver.observe(chartContainerRef.current);
    }

    setIsReady(true);

    return () => {
      window.removeEventListener('resize', handleResize);
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('mouseup', handleMouseUp);
      resizeObserver.disconnect();
      pluginManager.destroy();
      chart.remove();
    };
  }, [height, showVolume]);

  // Update chart data
  useEffect(() => {
    if (!isReady || !candlestickSeriesRef.current || data.length === 0) return;

    const candleData: CandlestickData[] = data.map((candle) => {
      let timestamp = candle.time;
      if (timestamp > 10000000000) {
        timestamp = Math.floor(timestamp / 1000);
      }

      return {
        time: timestamp as Time,
        open: candle.open,
        high: candle.high,
        low: candle.low,
        close: candle.close,
      };
    });

    candlestickSeriesRef.current.setData(candleData);

    if (showVolume && volumeSeriesRef.current) {
      const volumeData = data.map((candle) => {
        let timestamp = candle.time;
        if (timestamp > 10000000000) {
          timestamp = Math.floor(timestamp / 1000);
        }

        return {
          time: timestamp as Time,
          value: candle.volume,
          color: candle.close >= candle.open ? '#10b98144' : '#ef444444',
        };
      });

      volumeSeriesRef.current.setData(volumeData);
    }

    chartRef.current?.timeScale().fitContent();
  }, [data, isReady, showVolume]);

  // Handle mouse move for cursor tracking, hover, and dragging
  const handleMouseMove = useCallback((param: MouseEventParams) => {
    if (!candlestickSeriesRef.current || !toolkitRef.current) return;

    // Update cursor position for UI display
    if (param.time && param.point) {
      const price = candlestickSeriesRef.current.coordinateToPrice(param.point.y);
      if (price !== undefined && price !== null) {
        setCursorPosition({ time: param.time as Time, price });

        // Handle dragging and hover
        const point: ChartPoint<Time> = {
          time: param.time as Time,
          price: price,
        };
        toolkitRef.current.handleMouseMove(point, param.point.x, param.point.y);

        // Update cursor based on context
        if (activeToolRef.current) {
          setCursorStyle('crosshair');
        } else {
          // Check if hovering over a drawing
          const hoveredDrawing = toolkitRef.current.getDrawings().find(d =>
            d.id === (toolkitRef.current as any)._hoveredDrawingId
          );

          if (hoveredDrawing) {
            setCursorStyle('move');
          } else {
            setCursorStyle('default');
          }
        }
      }
    } else {
      setCursorPosition(null);
    }
  }, []);

  // Handle chart click for drawing and selection
  const handleChartClick = useCallback(
    (param: MouseEventParams) => {
      if (!toolkitRef.current) return;

      // Try to get coordinates from the click
      let time: Time | null = null;
      let price: number | null = null;
      let pixelX = 0;
      let pixelY = 0;

      // Method 1: Use param.time and param.point if available
      if (param.time && param.point) {
        time = param.time as Time;
        price = candlestickSeriesRef.current?.coordinateToPrice(param.point.y) ?? null;
        pixelX = param.point.x;
        pixelY = param.point.y;
      }
      // Method 2: Fallback - use logical coordinates directly from the chart
      else if (param.point && chartRef.current && candlestickSeriesRef.current) {
        time = chartRef.current.timeScale().coordinateToTime(param.point.x) as Time | null;
        price = candlestickSeriesRef.current.coordinateToPrice(param.point.y);
        pixelX = param.point.x;
        pixelY = param.point.y;
      }

      if (time === null || price === null || price === undefined) {
        return;
      }

      const point: ChartPoint<Time> = { time, price };
      const currentActiveTool = activeToolRef.current;

      // If a drawing tool is active, create drawing
      if (currentActiveTool && toolkitRef.current.getCurrentTool()) {
        toolkitRef.current.handleClick(point);

        // Force chart update
        if (chartRef.current) {
          chartRef.current.timeScale().scrollToPosition(0, false);
        }
      } else {
        // No tool active - handle selection/dragging
        const handled = toolkitRef.current.handleMouseDown(point, pixelX, pixelY);

        if (!handled) {
          // Click on empty space - finish any drag operation
          toolkitRef.current.handleMouseUp();
        }
      }
    },
    []
  );

  // Tool selection handler
  const handleToolSelect = useCallback(
    (tool: DrawingTool | null) => {
      // Update both state and ref to avoid stale closures
      setActiveTool(tool);
      activeToolRef.current = tool;

      if (tool && toolkitRef.current) {
        toolkitRef.current.activateTool(tool);
      } else if (!tool && toolkitRef.current) {
        toolkitRef.current.deactivateTool();
      }

      onToolChange?.(tool);
    },
    [onToolChange, activeTool]
  );

  // Public API for adding positions programmatically
  const addLongPosition = useCallback(
    (entryTime: Time, entryPrice: number, label?: string) => {
      if (toolkitRef.current) {
        return toolkitRef.current.addLongPosition(entryTime, entryPrice, undefined, undefined, {
          label,
        });
      }
      return '';
    },
    []
  );

  const addShortPosition = useCallback(
    (entryTime: Time, entryPrice: number, label?: string) => {
      if (toolkitRef.current) {
        return toolkitRef.current.addShortPosition(entryTime, entryPrice, undefined, undefined, {
          label,
        });
      }
      return '';
    },
    []
  );

  // TODO: Expose API via ref using forwardRef if needed
  // For now, the component is self-contained with toolbar UI

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        width: '100%',
        backgroundColor: '#0a0a0a',
        position: 'relative',
      }}
    >
      {/* Header with symbol and OHLC data - Bloomberg style */}
      {showHeader && (
        <div
          style={{
            padding: '4px 12px',
            backgroundColor: '#000000',
            borderBottom: '1px solid #2A2A2A',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            height: '28px',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            <span
              style={{
                fontSize: '11px',
                fontWeight: 600,
                color: '#FF8800',
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
              }}
            >
              {symbol}
            </span>
            {data.length > 0 && (
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px', fontSize: '10px', fontFamily: 'monospace' }}>
                <span style={{ color: '#787878' }}>O</span>
                <span style={{ color: '#FFFFFF' }}>{data[data.length - 1].open.toFixed(2)}</span>
                <span style={{ color: '#787878' }}>H</span>
                <span style={{ color: '#FFFFFF' }}>{data[data.length - 1].high.toFixed(2)}</span>
                <span style={{ color: '#787878' }}>L</span>
                <span style={{ color: '#FFFFFF' }}>{data[data.length - 1].low.toFixed(2)}</span>
                <span style={{ color: '#787878' }}>C</span>
                <span
                  style={{
                    color:
                      data[data.length - 1].close >= data[data.length - 1].open
                        ? '#00D66F'
                        : '#FF3B3B',
                    fontWeight: 600,
                  }}
                >
                  {data[data.length - 1].close.toFixed(2)}
                </span>
              </div>
            )}
          </div>

          <div
            style={{
              fontSize: '9px',
              color: '#787878',
              fontFamily: 'monospace',
            }}
          >
            {data.length} BARS
          </div>
        </div>
      )}

      {/* Toolbar */}
      {showToolbar && (
        <ToolkitToolbar
          toolkit={toolkitRef.current}
          onToolSelect={handleToolSelect}
          activeTool={activeTool}
        />
      )}

      {/* Chart container */}
      <div
        style={{
          flex: 1,
          position: 'relative',
          minHeight: 0,
          overflow: 'hidden',
        }}
      >
        <div
          ref={chartContainerRef}
          style={{
            width: '100%',
            height: '100%',
            position: 'relative',
            cursor: cursorStyle,
          }}
        />

        {/* Help text when tool is active */}
        {activeTool && (
          <div
            style={{
              position: 'absolute',
              bottom: '10px',
              left: '50%',
              transform: 'translateX(-50%)',
              backgroundColor: 'rgba(0, 0, 0, 0.95)',
              border: '1px solid #2A2A2A',
              borderRadius: '4px',
              padding: '6px 14px',
              fontSize: '10px',
              color: '#FFFFFF',
              zIndex: 100,
              pointerEvents: 'none',
              fontFamily: 'monospace',
            }}
          >
            Click on the chart to draw {activeTool.replace(/_/g, ' ')}
          </div>
        )}
      </div>
    </div>
  );
}
