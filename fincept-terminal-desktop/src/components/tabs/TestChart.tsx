import React, { useEffect, useRef } from 'react';
import { createChart, ColorType } from 'lightweight-charts';

export function TestChart() {
  const chartContainerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!chartContainerRef.current) return;

    // Create chart
    const chart = createChart(chartContainerRef.current, {
      width: chartContainerRef.current.clientWidth,
      height: 400,
      layout: {
        background: { type: ColorType.Solid, color: '#0d0d0d' },
        textColor: '#999',
      },
      grid: {
        vertLines: { color: '#1a1a1a' },
        horzLines: { color: '#1a1a1a' },
      },
    });

    // Add candlestick series
    const candleSeries = (chart as any).addCandlestickSeries({
      upColor: '#10b981',
      downColor: '#dc2626',
      borderUpColor: '#10b981',
      borderDownColor: '#dc2626',
      wickUpColor: '#10b981',
      wickDownColor: '#dc2626',
    });

    // Dummy data
    const dummyData = [
      { time: '2024-01-01', open: 100, high: 110, low: 95, close: 105 },
      { time: '2024-01-02', open: 105, high: 115, low: 100, close: 112 },
      { time: '2024-01-03', open: 112, high: 120, low: 108, close: 109 },
      { time: '2024-01-04', open: 109, high: 118, low: 105, close: 116 },
      { time: '2024-01-05', open: 116, high: 125, low: 115, close: 122 },
    ];

    candleSeries.setData(dummyData);
    chart.timeScale().fitContent();

    return () => chart.remove();
  }, []);

  return (
    <div style={{ padding: '20px', backgroundColor: '#0d0d0d', color: 'white' }}>
      <h1>Test Lightweight Charts</h1>
      <div ref={chartContainerRef} style={{ width: '100%', height: '400px' }} />
    </div>
  );
}
