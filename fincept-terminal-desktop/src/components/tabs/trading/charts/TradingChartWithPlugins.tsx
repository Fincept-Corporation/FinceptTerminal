/**
 * TradingChartWithPlugins - Enhanced TradingChart with plugin system
 * Extends the base TradingChart with support for lightweight-charts plugins
 */

import React, { useEffect, useRef, useState, useCallback } from 'react';
import { createChart, ColorType, CandlestickSeries, HistogramSeries, CrosshairMode } from 'lightweight-charts';
import type { IChartApi, CandlestickData, Time, ISeriesApi, SeriesType } from 'lightweight-charts';
import { useOHLCV } from '../hooks/useMarketData';
import type { Timeframe, OHLCV } from '../types';
import { PluginManager, TooltipPlugin, TrendLinePlugin, VerticalLinePlugin, IChartPlugin, PluginCategory } from './plugins';
import { Settings, TrendingUp, Minus, Maximize2 } from 'lucide-react';

interface TradingChartWithPluginsProps {
  symbol: string;
  timeframe?: Timeframe;
  height?: number;
  showVolume?: boolean;
  enablePlugins?: boolean;
}

export function TradingChartWithPlugins({
  symbol,
  timeframe = '1h',
  height = 500,
  showVolume = true,
  enablePlugins = true
}: TradingChartWithPluginsProps) {
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const candlestickSeriesRef = useRef<ISeriesApi<'Candlestick', Time> | null>(null);
  const volumeSeriesRef = useRef<any>(null);
  const pluginManagerRef = useRef<PluginManager | null>(null);

  const [selectedTimeframe, setSelectedTimeframe] = useState<Timeframe>(timeframe);
  const [showPluginPanel, setShowPluginPanel] = useState(false);
  const [activeDrawingTool, setActiveDrawingTool] = useState<string | null>(null);

  const { ohlcv, isLoading, error } = useOHLCV(symbol, selectedTimeframe, 200);

  // Initialize chart and plugins
  useEffect(() => {
    if (!chartContainerRef.current) return;

    const containerHeight = chartContainerRef.current.clientHeight || 400;
    const isIntradayTimeframe = !['1d', '1w', '1M'].includes(selectedTimeframe);
    const showSeconds = ['1m', '3m', '5m'].includes(selectedTimeframe);

    const chart = createChart(chartContainerRef.current, {
      width: chartContainerRef.current.clientWidth,
      height: containerHeight,
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
        mode: CrosshairMode.Normal, // Free movement crosshair
      },
      rightPriceScale: {
        borderColor: '#2a2a2a',
        scaleMargins: {
          top: 0.1,
          bottom: 0.1,
        },
      },
      timeScale: {
        borderColor: '#2a2a2a',
        timeVisible: isIntradayTimeframe,
        secondsVisible: showSeconds,
        visible: true,
        borderVisible: true,
        ticksVisible: true,
        rightOffset: 5,
      },
      overlayPriceScales: {
        scaleMargins: {
          top: 0.7,
          bottom: 0,
        },
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
    if (enablePlugins) {
      const pluginManager = new PluginManager(chart, candlestickSeries as any);
      pluginManagerRef.current = pluginManager;

      // Register default plugins
      const tooltipPlugin = new TooltipPlugin();
      const trendLinePlugin = new TrendLinePlugin();
      const verticalLinePlugin = new VerticalLinePlugin();

      pluginManager.register(tooltipPlugin);
      pluginManager.register(trendLinePlugin);
      pluginManager.register(verticalLinePlugin);

      console.log('[TradingChart] Plugin system initialized:', pluginManager.getStats());
    }

    // Handle resize
    const handleResize = () => {
      if (chartContainerRef.current && chartRef.current) {
        const containerHeight = chartContainerRef.current.clientHeight || 400;
        chartRef.current.resize(
          chartContainerRef.current.clientWidth,
          containerHeight,
          true
        );
      }
    };

    window.addEventListener('resize', handleResize);

    const resizeObserver = new ResizeObserver(() => {
      handleResize();
    });

    if (chartContainerRef.current) {
      resizeObserver.observe(chartContainerRef.current);
    }

    return () => {
      window.removeEventListener('resize', handleResize);
      resizeObserver.disconnect();

      // Cleanup plugins
      if (pluginManagerRef.current) {
        pluginManagerRef.current.destroy();
      }

      chart.remove();
    };
  }, [height, showVolume, enablePlugins, selectedTimeframe]);

  // Update chart data
  useEffect(() => {
    if (!ohlcv.length || !candlestickSeriesRef.current) return;

    const candleData: CandlestickData[] = ohlcv.map((candle: OHLCV) => {
      let timestamp = candle.timestamp;
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
      const volumeData = ohlcv.map((candle: OHLCV) => {
        let timestamp = candle.timestamp;
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
  }, [ohlcv, showVolume]);

  // Plugin controls
  const togglePlugin = useCallback((pluginId: string) => {
    if (!pluginManagerRef.current) return;

    const plugin = pluginManagerRef.current.getPlugin(pluginId);
    if (plugin) {
      pluginManagerRef.current.togglePlugin(pluginId, !plugin.metadata.enabled);
    }
  }, []);

  const activateDrawingTool = useCallback((toolName: string) => {
    if (!pluginManagerRef.current) return;

    // Deactivate previous tool
    if (activeDrawingTool) {
      const prevTool = pluginManagerRef.current.getPlugin(activeDrawingTool);
      if (prevTool && 'stopDrawing' in prevTool) {
        (prevTool as any).stopDrawing();
      }
    }

    // Activate new tool
    if (toolName === activeDrawingTool) {
      setActiveDrawingTool(null);
    } else {
      const tool = pluginManagerRef.current.getPlugin(toolName);
      if (tool && 'startDrawing' in tool) {
        (tool as any).startDrawing();
        setActiveDrawingTool(toolName);
      }
    }
  }, [activeDrawingTool]);

  const timeframes: Timeframe[] = ['1m', '5m', '15m', '1h', '4h', '1d'];

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      width: '100%',
      position: 'relative'
    }}>
      {/* Toolbar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        gap: '8px',
        padding: '8px 16px',
        flexShrink: 0,
        backgroundColor: '#0a0a0a',
        borderBottom: '1px solid #1f1f1f'
      }}>
        {/* Timeframe selector */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ fontSize: '11px', color: '#9ca3af', fontWeight: 600 }}>TIMEFRAME:</span>
          <div style={{ display: 'flex', gap: '4px' }}>
            {timeframes.map((tf) => (
              <button
                key={tf}
                onClick={() => setSelectedTimeframe(tf)}
                style={{
                  padding: '4px 8px',
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  borderRadius: '2px',
                  border: 'none',
                  cursor: 'pointer',
                  backgroundColor: selectedTimeframe === tf ? '#2563eb' : '#1f2937',
                  color: selectedTimeframe === tf ? '#ffffff' : '#9ca3af',
                  transition: 'all 0.2s'
                }}
              >
                {tf}
              </button>
            ))}
          </div>
        </div>

        {/* Plugin toolbar */}
        {enablePlugins && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <button
              onClick={() => activateDrawingTool('trendline')}
              title="Trend Line"
              style={{
                padding: '6px 10px',
                fontSize: '11px',
                borderRadius: '3px',
                border: 'none',
                cursor: 'pointer',
                backgroundColor: activeDrawingTool === 'trendline' ? '#10b981' : '#1f2937',
                color: '#ffffff',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <TrendingUp size={14} />
              <span>Trend</span>
            </button>

            <button
              onClick={() => {
                const plugin = pluginManagerRef.current?.getPlugin('verticalline');
                if (plugin && 'markNow' in plugin) {
                  (plugin as any).markNow();
                }
              }}
              title="Mark Current Time"
              style={{
                padding: '6px 10px',
                fontSize: '11px',
                borderRadius: '3px',
                border: 'none',
                cursor: 'pointer',
                backgroundColor: '#1f2937',
                color: '#ffffff',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Minus size={14} />
              <span>Mark</span>
            </button>

            <button
              onClick={() => setShowPluginPanel(!showPluginPanel)}
              title="Plugin Settings"
              style={{
                padding: '6px 10px',
                fontSize: '11px',
                borderRadius: '3px',
                border: 'none',
                cursor: 'pointer',
                backgroundColor: showPluginPanel ? '#2563eb' : '#1f2937',
                color: '#ffffff',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Settings size={14} />
            </button>
          </div>
        )}
      </div>

      {/* Plugin panel */}
      {enablePlugins && showPluginPanel && (
        <div style={{
          padding: '12px 16px',
          backgroundColor: '#0f0f0f',
          borderBottom: '1px solid #1f1f1f',
          fontSize: '11px'
        }}>
          <div style={{ color: '#9ca3af', marginBottom: '8px', fontWeight: 600 }}>PLUGINS</div>
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
            {pluginManagerRef.current?.getPlugins().map((plugin) => (
              <label
                key={plugin.metadata.id}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  padding: '4px 8px',
                  backgroundColor: '#1f2937',
                  borderRadius: '3px',
                  cursor: 'pointer',
                  color: plugin.metadata.enabled ? '#ffffff' : '#6b7280'
                }}
              >
                <input
                  type="checkbox"
                  checked={plugin.metadata.enabled}
                  onChange={() => togglePlugin(plugin.metadata.id)}
                  style={{ cursor: 'pointer' }}
                />
                <span>{plugin.metadata.name}</span>
                <span style={{
                  fontSize: '9px',
                  color: '#6b7280',
                  textTransform: 'uppercase'
                }}>
                  {plugin.metadata.category}
                </span>
              </label>
            ))}
          </div>
        </div>
      )}

      {/* Chart container */}
      <div style={{
        flex: 1,
        position: 'relative',
        minHeight: 0,
        overflow: 'hidden'
      }}>
        {isLoading && (
          <div style={{
            position: 'absolute',
            inset: 0,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: 'rgba(0, 0, 0, 0.5)',
            zIndex: 10
          }}>
            <div style={{
              width: '32px',
              height: '32px',
              border: '2px solid transparent',
              borderTopColor: '#3b82f6',
              borderRadius: '50%',
              animation: 'spin 1s linear infinite'
            }} />
          </div>
        )}

        {error && (
          <div style={{
            position: 'absolute',
            inset: 0,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: 'rgba(0, 0, 0, 0.5)',
            zIndex: 10
          }}>
            <div style={{ color: '#ef4444', fontSize: '14px' }}>
              Failed to load chart data: {error.message}
            </div>
          </div>
        )}

        <div
          ref={chartContainerRef}
          style={{
            width: '100%',
            height: '100%',
            position: 'relative'
          }}
        />
      </div>
    </div>
  );
}
