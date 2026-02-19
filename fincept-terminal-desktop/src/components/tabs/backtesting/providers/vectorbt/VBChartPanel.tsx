/**
 * VBChartPanel - Lightweight Charts equity curve with trade markers.
 * Fix: SVG placeholder over empty chart, opacity transition on data arrival.
 */

import React, { useRef, useEffect, useMemo, useState } from 'react';
import { createChart, createSeriesMarkers, LineSeries, type IChartApi, type ISeriesApi, type ISeriesMarkersPluginApi, type Time } from 'lightweight-charts';
import { FINCEPT, TYPOGRAPHY } from '../../../portfolio-tab/finceptStyles';
import type { BacktestingState } from '../../types';

interface VBChartPanelProps {
  state: BacktestingState;
}

export const VBChartPanel: React.FC<VBChartPanelProps> = ({ state }) => {
  const { result, isRunning } = state;
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const equitySeriesRef = useRef<ISeriesApi<'Line'> | null>(null);
  const benchmarkSeriesRef = useRef<ISeriesApi<'Line'> | null>(null);
  const markersPluginRef = useRef<ISeriesMarkersPluginApi<Time> | null>(null);
  const [chartReady, setChartReady] = useState(false);

  const equityData = result?.data?.equity;
  const trades = result?.data?.trades;
  const benchmark = result?.data?.benchmark;

  // Normalize any date string to yyyy-mm-dd
  const toDateStr = (raw: string): string => raw.split('T')[0].split(' ')[0].substring(0, 10);

  // Parse equity curve data
  const equityPoints = useMemo(() => {
    if (!equityData) return [];
    if (Array.isArray(equityData)) {
      return equityData.map((pt: any) => {
        if (pt.time && pt.value !== undefined) return { time: toDateStr(pt.time), value: pt.value };
        if (pt.date && pt.equity !== undefined) return { time: toDateStr(pt.date), value: pt.equity };
        if (pt.date && pt.value !== undefined) return { time: toDateStr(pt.date), value: pt.value };
        if (pt.timestamp && pt.equity !== undefined)
          return { time: toDateStr(new Date(pt.timestamp).toISOString()), value: pt.equity };
        return null;
      }).filter(Boolean);
    }
    if (typeof equityData === 'object') {
      return Object.entries(equityData).map(([date, value]) => ({
        time: toDateStr(date),
        value: Number(value),
      })).sort((a, b) => a.time.localeCompare(b.time));
    }
    return [];
  }, [equityData]);

  // Parse benchmark data
  const benchmarkPoints = useMemo(() => {
    if (!benchmark) return [];
    if (Array.isArray(benchmark)) {
      return benchmark.map((pt: any) => {
        if (pt.time && pt.value !== undefined) return { time: toDateStr(pt.time), value: pt.value };
        if (pt.date && pt.equity !== undefined) return { time: toDateStr(pt.date), value: pt.equity };
        if (pt.date && pt.value !== undefined) return { time: toDateStr(pt.date), value: pt.value };
        return null;
      }).filter(Boolean);
    }
    if (typeof benchmark === 'object') {
      return Object.entries(benchmark).map(([date, value]) => ({
        time: toDateStr(date),
        value: Number(value),
      })).sort((a, b) => a.time.localeCompare(b.time));
    }
    return [];
  }, [benchmark]);

  // Parse trade markers
  const markers = useMemo(() => {
    if (!trades || !Array.isArray(trades)) return [];
    const m: any[] = [];
    trades.forEach((trade: any) => {
      if (trade.entryDate || trade.entry_date) {
        m.push({
          time: toDateStr(trade.entryDate || trade.entry_date),
          position: 'belowBar' as const,
          color: '#00D66F',
          shape: 'arrowUp' as const,
          text: 'BUY',
        });
      }
      if (trade.exitDate || trade.exit_date) {
        m.push({
          time: toDateStr(trade.exitDate || trade.exit_date),
          position: 'aboveBar' as const,
          color: '#FF3B3B',
          shape: 'arrowDown' as const,
          text: 'SELL',
        });
      }
    });
    return m.sort((a, b) => a.time.localeCompare(b.time));
  }, [trades]);

  // Create chart
  useEffect(() => {
    if (!containerRef.current) return;

    const chart = createChart(containerRef.current, {
      layout: {
        background: { color: '#0a0a0a' },
        textColor: '#787878',
        fontFamily: 'IBM Plex Mono, monospace',
        fontSize: 10,
      },
      grid: {
        vertLines: { color: '#1a1a1a' },
        horzLines: { color: '#1a1a1a' },
      },
      crosshair: {
        vertLine: { color: '#FF880060', labelBackgroundColor: '#FF8800' },
        horzLine: { color: '#FF880060', labelBackgroundColor: '#FF8800' },
      },
      rightPriceScale: {
        borderColor: '#2A2A2A',
      },
      timeScale: {
        borderColor: '#2A2A2A',
        timeVisible: false,
      },
    });

    chartRef.current = chart;

    // Equity series
    const equitySeries = chart.addSeries(LineSeries, {
      color: '#00D66F',
      lineWidth: 2,
      priceLineVisible: false,
      lastValueVisible: true,
    });
    equitySeriesRef.current = equitySeries;

    // Markers plugin for trade entry/exit markers
    const markersPlugin = createSeriesMarkers(equitySeries, []);
    markersPluginRef.current = markersPlugin;

    // Benchmark series
    const bmSeries = chart.addSeries(LineSeries, {
      color: '#4A4A4A',
      lineWidth: 1,
      lineStyle: 2, // dashed
      priceLineVisible: false,
      lastValueVisible: false,
    });
    benchmarkSeriesRef.current = bmSeries;

    // ResizeObserver
    const observer = new ResizeObserver(entries => {
      const { width, height } = entries[0].contentRect;
      chart.resize(width, height);
    });
    observer.observe(containerRef.current);

    return () => {
      observer.disconnect();
      markersPlugin.detach();
      chart.remove();
      chartRef.current = null;
      markersPluginRef.current = null;
    };
  }, []);

  // Update data
  useEffect(() => {
    if (!equitySeriesRef.current) return;

    if (equityPoints.length > 0) {
      equitySeriesRef.current.setData(equityPoints as any);
      if (markersPluginRef.current && markers.length > 0) {
        markersPluginRef.current.setMarkers(markers);
      }
      chartRef.current?.timeScale().fitContent();
      setChartReady(true);
    } else {
      equitySeriesRef.current.setData([]);
      if (markersPluginRef.current) {
        markersPluginRef.current.setMarkers([]);
      }
      setChartReady(false);
    }
  }, [equityPoints, markers]);

  // Update benchmark
  useEffect(() => {
    if (!benchmarkSeriesRef.current) return;
    if (benchmarkPoints.length > 0) {
      benchmarkSeriesRef.current.setData(benchmarkPoints as any);
    } else {
      benchmarkSeriesRef.current.setData([]);
    }
  }, [benchmarkPoints]);

  const hasData = equityPoints.length > 0;

  return (
    <div style={{
      flex: 1,
      minHeight: '280px',
      position: 'relative',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
    }}>
      {/* Header label */}
      <div style={{
        position: 'absolute',
        top: '10px',
        left: '14px',
        zIndex: 10,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
      }}>
        <span style={{
          fontSize: '10px',
          fontWeight: 700,
          color: FINCEPT.ORANGE,
          letterSpacing: '0.5px',
        }}>
          EQUITY CURVE
        </span>
        {hasData && (
          <span style={{
            fontSize: '9px',
            color: FINCEPT.GRAY,
            fontFamily: TYPOGRAPHY.MONO,
          }}>
            {equityPoints.length} points
          </span>
        )}
      </div>

      {/* Chart container - fades in when data arrives */}
      <div
        ref={containerRef}
        style={{
          width: '100%',
          height: '100%',
          opacity: chartReady ? 1 : 0,
          transition: 'opacity 0.4s ease',
        }}
      />

      {/* SVG Placeholder (visible when no data and not running) */}
      {!hasData && !isRunning && (
        <div style={{
          position: 'absolute',
          inset: 0,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          pointerEvents: 'none',
          backgroundColor: '#0a0a0a',
        }}>
          {/* Fake chart SVG */}
          <svg
            width="200"
            height="80"
            viewBox="0 0 200 80"
            fill="none"
            style={{ marginBottom: '12px', opacity: 0.4 }}
          >
            {/* Grid lines */}
            <line x1="0" y1="20" x2="200" y2="20" stroke="#1a1a1a" strokeWidth="1" />
            <line x1="0" y1="40" x2="200" y2="40" stroke="#1a1a1a" strokeWidth="1" />
            <line x1="0" y1="60" x2="200" y2="60" stroke="#1a1a1a" strokeWidth="1" />
            <line x1="50" y1="0" x2="50" y2="80" stroke="#1a1a1a" strokeWidth="1" />
            <line x1="100" y1="0" x2="100" y2="80" stroke="#1a1a1a" strokeWidth="1" />
            <line x1="150" y1="0" x2="150" y2="80" stroke="#1a1a1a" strokeWidth="1" />

            {/* Fake equity line */}
            <polyline
              points="0,60 20,55 40,50 60,45 80,52 100,38 120,30 140,35 160,22 180,18 200,12"
              stroke="#00D66F"
              strokeWidth="2"
              fill="none"
              opacity="0.5"
            />

            {/* Fake benchmark line */}
            <polyline
              points="0,60 20,58 40,55 60,52 80,50 100,48 120,46 140,44 160,42 180,40 200,38"
              stroke="#4A4A4A"
              strokeWidth="1"
              strokeDasharray="4,3"
              fill="none"
              opacity="0.5"
            />

            {/* Buy/sell markers */}
            <circle cx="60" cy="45" r="3" fill="#00D66F" opacity="0.6" />
            <circle cx="140" cy="35" r="3" fill="#FF3B3B" opacity="0.6" />
          </svg>

          <div style={{
            fontSize: '11px',
            color: FINCEPT.MUTED,
            fontWeight: 600,
            marginBottom: '4px',
          }}>
            Run a backtest to see equity curve
          </div>
          <div style={{
            fontSize: '10px',
            color: FINCEPT.MUTED,
            opacity: 0.6,
          }}>
            Select a strategy and click RUN
          </div>
        </div>
      )}

      {/* Running overlay */}
      {isRunning && (
        <div style={{
          position: 'absolute',
          inset: 0,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor: 'rgba(10,10,10,0.8)',
          pointerEvents: 'none',
        }}>
          {/* Animated bars */}
          <div style={{
            display: 'flex',
            gap: '4px',
            marginBottom: '10px',
          }}>
            {[0, 1, 2, 3, 4].map(i => (
              <div
                key={i}
                style={{
                  width: '4px',
                  height: `${16 + Math.sin(i * 1.2) * 8}px`,
                  backgroundColor: FINCEPT.ORANGE,
                  borderRadius: '1px',
                  animation: `pulse 1s ease-in-out ${i * 0.15}s infinite`,
                }}
              />
            ))}
          </div>
          <div style={{
            fontSize: '12px',
            color: FINCEPT.ORANGE,
            fontWeight: 700,
            letterSpacing: '1px',
          }}>
            EXECUTING...
          </div>
        </div>
      )}
    </div>
  );
};
