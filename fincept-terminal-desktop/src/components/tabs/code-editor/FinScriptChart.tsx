import React, { useEffect, useRef } from 'react';
import { createChart, IChartApi, ColorType, CandlestickSeries, LineSeries, HistogramSeries } from 'lightweight-charts';

interface PlotPoint {
  timestamp: number;
  value?: number;
  open?: number;
  high?: number;
  low?: number;
  close?: number;
  volume?: number;
}

interface PlotData {
  plot_type: string;
  label: string;
  data: PlotPoint[];
  color?: string;
}

interface FinScriptChartProps {
  plot: PlotData;
  height?: number;
}

export const FinScriptChart: React.FC<FinScriptChartProps> = ({ plot, height = 250 }) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);

  useEffect(() => {
    if (!containerRef.current || plot.data.length === 0) return;

    // Clean up existing chart
    if (chartRef.current) {
      chartRef.current.remove();
      chartRef.current = null;
    }

    const chart = createChart(containerRef.current, {
      width: containerRef.current.clientWidth,
      height,
      layout: {
        background: { type: ColorType.Solid, color: '#0a0a0a' },
        textColor: '#a0a0a0',
        fontSize: 11,
      },
      grid: {
        vertLines: { color: '#1a1a1a' },
        horzLines: { color: '#1a1a1a' },
      },
      crosshair: {
        mode: 0,
      },
      rightPriceScale: {
        borderColor: '#2a2a2a',
      },
      timeScale: {
        borderColor: '#2a2a2a',
        timeVisible: true,
      },
    });

    chartRef.current = chart;

    if (plot.plot_type === 'candlestick') {
      const series = chart.addSeries(CandlestickSeries, {
        upColor: '#26a69a',
        downColor: '#ef5350',
        borderDownColor: '#ef5350',
        borderUpColor: '#26a69a',
        wickDownColor: '#ef5350',
        wickUpColor: '#26a69a',
      });

      const candleData = plot.data
        .filter(p => p.open != null && p.high != null && p.low != null && p.close != null)
        .map(p => ({
          time: p.timestamp as any,
          open: p.open!,
          high: p.high!,
          low: p.low!,
          close: p.close!,
        }));

      if (candleData.length > 0) {
        series.setData(candleData);
      }
    } else if (plot.plot_type === 'histogram') {
      const series = chart.addSeries(HistogramSeries, {
        color: plot.color || '#26a69a',
      });

      const histData = plot.data
        .filter(p => p.value != null && !isNaN(p.value!))
        .map(p => ({
          time: p.timestamp as any,
          value: p.value!,
          color: p.value! >= 0 ? (plot.color || '#26a69a') : '#ef5350',
        }));

      if (histData.length > 0) {
        series.setData(histData);
      }
    } else {
      // Line chart
      const series = chart.addSeries(LineSeries, {
        color: plot.color || '#2196F3',
        lineWidth: 2,
      });

      const lineData = plot.data
        .filter(p => p.value != null && !isNaN(p.value!))
        .map(p => ({
          time: p.timestamp as any,
          value: p.value!,
        }));

      if (lineData.length > 0) {
        series.setData(lineData);
      }
    }

    chart.timeScale().fitContent();

    // Handle resize
    const resizeObserver = new ResizeObserver(entries => {
      for (const entry of entries) {
        if (chartRef.current) {
          chartRef.current.applyOptions({
            width: entry.contentRect.width,
          });
        }
      }
    });

    resizeObserver.observe(containerRef.current);

    return () => {
      resizeObserver.disconnect();
      if (chartRef.current) {
        chartRef.current.remove();
        chartRef.current = null;
      }
    };
  }, [plot, height]);

  return (
    <div style={{ marginBottom: '12px' }}>
      <div style={{
        color: '#a0a0a0',
        fontSize: '11px',
        fontWeight: 'bold',
        marginBottom: '4px',
        fontFamily: 'Consolas, monospace',
      }}>
        {plot.label} ({plot.plot_type === 'candlestick' ? 'OHLC' : plot.plot_type === 'histogram' ? 'Histogram' : 'Line'} - {plot.data.length} points)
      </div>
      <div
        ref={containerRef}
        style={{
          width: '100%',
          border: '1px solid #2a2a2a',
          borderRadius: '2px',
        }}
      />
    </div>
  );
};
