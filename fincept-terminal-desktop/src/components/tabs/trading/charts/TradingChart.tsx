/**
 * TradingChart - TradingView-style candlestick chart with drawing tools
 * Uses ProChartWithToolkit for professional drawing capabilities
 */

import React, { useState } from 'react';
import { ProChartWithToolkit } from './ProChartWithToolkit';
import { useOHLCV } from '../hooks/useMarketData';
import type { Timeframe } from '../types';

interface TradingChartProps {
  symbol: string;
  timeframe?: Timeframe;
  height?: number;
  showVolume?: boolean;
}

export function TradingChart({
  symbol,
  timeframe = '1h',
  height = 500,
  showVolume = true
}: TradingChartProps) {
  const [selectedTimeframe, setSelectedTimeframe] = useState<Timeframe>(timeframe);
  const { ohlcv, isLoading, error } = useOHLCV(symbol, selectedTimeframe, 200);

  const timeframes: Timeframe[] = ['1m', '5m', '15m', '1h', '4h', '1d'];

  // Prepare data for ProChartWithToolkit
  const chartData = ohlcv.map((candle) => ({
    time: candle.timestamp > 10000000000 ? Math.floor(candle.timestamp / 1000) : candle.timestamp,
    open: candle.open,
    high: candle.high,
    low: candle.low,
    close: candle.close,
    volume: candle.volume,
  }));

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      width: '100%',
      position: 'relative'
    }}>
      {/* Timeframe selector with OHLC data - Bloomberg style */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '4px 12px',
        flexShrink: 0,
        backgroundColor: '#000000',
        borderBottom: '1px solid #2A2A2A',
        height: '28px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ fontSize: '10px', color: '#787878', fontWeight: 600, fontFamily: 'monospace' }}>TIMEFRAME:</span>
          <div style={{ display: 'flex', gap: '3px' }}>
            {timeframes.map((tf) => (
              <button
                key={tf}
                onClick={() => setSelectedTimeframe(tf)}
                style={{
                  padding: '3px 8px',
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  borderRadius: '2px',
                  border: 'none',
                  cursor: 'pointer',
                  backgroundColor: selectedTimeframe === tf ? '#FF8800' : '#1A1A1A',
                  color: selectedTimeframe === tf ? '#000000' : '#787878',
                  fontWeight: selectedTimeframe === tf ? 600 : 400,
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  if (selectedTimeframe !== tf) {
                    e.currentTarget.style.backgroundColor = '#2A2A2A';
                    e.currentTarget.style.color = '#FFFFFF';
                  }
                }}
                onMouseLeave={(e) => {
                  if (selectedTimeframe !== tf) {
                    e.currentTarget.style.backgroundColor = '#1A1A1A';
                    e.currentTarget.style.color = '#787878';
                  }
                }}
              >
                {tf}
              </button>
            ))}
          </div>
        </div>

        {/* OHLC Data */}
        {!isLoading && !error && chartData.length > 0 && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '2px', fontSize: '10px', fontFamily: 'monospace' }}>
              <span style={{ color: '#FF8800', fontWeight: 600, marginRight: '8px' }}>
                {symbol} - {selectedTimeframe}
              </span>
              <span style={{ color: '#787878' }}>O</span>
              <span style={{ color: '#FFFFFF', marginRight: '6px' }}>{chartData[chartData.length - 1].open.toFixed(2)}</span>
              <span style={{ color: '#787878' }}>H</span>
              <span style={{ color: '#FFFFFF', marginRight: '6px' }}>{chartData[chartData.length - 1].high.toFixed(2)}</span>
              <span style={{ color: '#787878' }}>L</span>
              <span style={{ color: '#FFFFFF', marginRight: '6px' }}>{chartData[chartData.length - 1].low.toFixed(2)}</span>
              <span style={{ color: '#787878' }}>C</span>
              <span
                style={{
                  color: chartData[chartData.length - 1].close >= chartData[chartData.length - 1].open
                    ? '#00D66F'
                    : '#FF3B3B',
                  fontWeight: 600,
                  marginRight: '12px'
                }}
              >
                {chartData[chartData.length - 1].close.toFixed(2)}
              </span>
              <span style={{ color: '#787878', fontSize: '9px' }}>{chartData.length} BARS</span>
            </div>
          </div>
        )}
      </div>

      {/* Chart container with absolute positioning for perfect fit */}
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

        {/* ProChartWithToolkit with drawing tools */}
        {!isLoading && !error && chartData.length > 0 && (
          <ProChartWithToolkit
            data={chartData}
            symbol={`${symbol} - ${selectedTimeframe}`}
            height={height}
            showVolume={showVolume}
            showToolbar={true}
            showHeader={false}
          />
        )}

        {/* Empty state when no data */}
        {!isLoading && !error && chartData.length === 0 && (
          <div style={{
            position: 'absolute',
            inset: 0,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: '#6b7280',
            fontSize: '12px'
          }}>
            No data available
          </div>
        )}
      </div>
    </div>
  );
}
