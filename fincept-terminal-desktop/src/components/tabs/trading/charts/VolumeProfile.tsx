/**
 * VolumeProfile - Volume distribution by price level
 * Shows where most trading activity occurred
 */

import React from 'react';
import { BarChart, Bar, XAxis, YAxis, Tooltip, ResponsiveContainer, Cell } from 'recharts';
import { useVolumeProfile } from '../hooks/useMarketData';
import type { Timeframe } from '../types';

interface VolumeProfileProps {
  symbol: string;
  timeframe?: Timeframe;
  height?: number;
  limit?: number;
}

export function VolumeProfile({
  symbol,
  timeframe = '1h',
  height = 400,
  limit = 500
}: VolumeProfileProps) {
  const { volumeProfile, isLoading } = useVolumeProfile(symbol, timeframe, limit);

  if (isLoading || !volumeProfile.length) {
    return (
      <div className="flex items-center justify-center bg-black border border-gray-800 rounded" style={{ height }}>
        {isLoading ? (
          <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500"></div>
        ) : (
          <div className="text-gray-500 text-sm">No volume data available</div>
        )}
      </div>
    );
  }

  // Find max volume for color scaling
  const maxVolume = Math.max(...volumeProfile.map((p) => p.volume));

  // Find POC (Point of Control) - price level with highest volume
  const poc = volumeProfile.reduce((max, current) =>
    current.volume > max.volume ? current : max
  );

  // Calculate total volume for display
  const totalVolume = volumeProfile.reduce((sum, p) => sum + p.volume, 0);

  // Format volume display
  const formatVolume = (vol: number) => {
    if (vol >= 1000000) return `${(vol / 1000000).toFixed(2)}M`;
    if (vol >= 1000) return `${(vol / 1000).toFixed(1)}K`;
    return vol.toFixed(0);
  };

  return (
    <div style={{ backgroundColor: '#000', border: '1px solid #2A2A2A', padding: '8px', height: '100%', display: 'flex', flexDirection: 'column', width: '100%', overflow: 'hidden' }}>
      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '10px', paddingBottom: '8px', borderBottom: '1px solid #2A2A2A', flexShrink: 0 }}>
        <span style={{ fontSize: '13px', color: '#FF8800', fontWeight: 700, fontFamily: 'monospace' }}>VOLUME BY PRICE</span>
        <div style={{ fontSize: '11px', color: '#FFF', fontFamily: 'monospace' }}>
          <span style={{ color: '#787878' }}>POC:</span> <span style={{ color: '#00E5FF', fontWeight: 700 }}>${poc.price.toLocaleString(undefined, { maximumFractionDigits: 2 })}</span>
        </div>
      </div>

      {/* Summary Stats */}
      <div style={{ display: 'flex', gap: '20px', marginBottom: '12px', fontSize: '12px', fontFamily: 'monospace', padding: '8px 12px', backgroundColor: '#0F0F0F', border: '1px solid #2A2A2A', flexShrink: 0 }}>
        <div style={{ flex: 1 }}>
          <div style={{ color: '#787878', fontSize: '9px', marginBottom: '2px' }}>TOTAL VOLUME</div>
          <div style={{ color: '#9D4EDD', fontWeight: 700, fontSize: '13px' }}>{formatVolume(totalVolume)}</div>
        </div>
        <div style={{ flex: 1 }}>
          <div style={{ color: '#787878', fontSize: '9px', marginBottom: '2px' }}>PRICE LEVELS</div>
          <div style={{ color: '#00E5FF', fontWeight: 700, fontSize: '13px' }}>{volumeProfile.length}</div>
        </div>
        <div style={{ flex: 1 }}>
          <div style={{ color: '#787878', fontSize: '9px', marginBottom: '2px' }}>TIMEFRAME</div>
          <div style={{ color: '#FFF', fontWeight: 700, fontSize: '13px' }}>{timeframe.toUpperCase()}</div>
        </div>
      </div>

      <div style={{ flex: 1, minHeight: 0 }}>
        <ResponsiveContainer width="100%" height="100%">
          <BarChart
            data={volumeProfile}
            layout="vertical"
            margin={{ top: 5, right: 5, left: 5, bottom: 5 }}
            barSize={12}
            barGap={2}
          >
            <XAxis
              type="number"
              stroke="#2A2A2A"
              tick={{ fill: '#FFFFFF', fontSize: 11, fontFamily: 'monospace' }}
              tickFormatter={(value) => {
                if (value >= 1000000) return `${(value / 1000000).toFixed(1)}M`;
                if (value >= 1000) return `${(value / 1000).toFixed(1)}K`;
                return value.toFixed(0);
              }}
              height={30}
            />

            <YAxis
              type="category"
              dataKey="price"
              stroke="#2A2A2A"
              orientation="right"
              tick={{ fill: '#FFFFFF', fontSize: 12, fontFamily: 'monospace' }}
              tickFormatter={(value) => `$${parseFloat(value).toLocaleString(undefined, { maximumFractionDigits: 0 })}`}
              width={90}
            />

          <Tooltip
            contentStyle={{
              backgroundColor: '#0F0F0F',
              border: '2px solid #FF8800',
              borderRadius: '4px',
              fontSize: '13px',
              fontFamily: 'monospace',
              padding: '12px'
            }}
            labelStyle={{ color: '#FF8800', fontWeight: 700, fontSize: '14px' }}
            itemStyle={{ color: '#FFF', fontSize: '13px' }}
            formatter={(value: number, name: string) => {
              if (name === 'volume') return [value.toLocaleString(undefined, { maximumFractionDigits: 2 }), 'Volume'];
              if (name === 'buyVolume') return [value.toLocaleString(undefined, { maximumFractionDigits: 2 }), 'Buy Vol'];
              if (name === 'sellVolume') return [value.toLocaleString(undefined, { maximumFractionDigits: 2 }), 'Sell Vol'];
              return value.toFixed(2);
            }}
            labelFormatter={(label) => `Price: $${parseFloat(label).toLocaleString(undefined, { maximumFractionDigits: 2 })}`}
          />

          <Bar dataKey="volume" radius={[0, 2, 2, 0]}>
            {volumeProfile.map((entry, index) => {
              // Color based on buy/sell ratio
              const buyRatio = entry.buyVolume / entry.volume;
              const isPOC = entry.price === poc.price;

              let color = '#6b7280'; // Default gray

              if (isPOC) {
                color = '#3b82f6'; // Blue for POC
              } else if (buyRatio > 0.6) {
                color = '#10b981'; // Green for buy-heavy
              } else if (buyRatio < 0.4) {
                color = '#ef4444'; // Red for sell-heavy
              }

              return <Cell key={`cell-${index}`} fill={color} />;
            })}
          </Bar>
          </BarChart>
        </ResponsiveContainer>
      </div>

      {/* Legend */}
      <div style={{ display: 'flex', justifyContent: 'center', gap: '16px', marginTop: '12px', fontSize: '10px', fontFamily: 'monospace', paddingTop: '10px', borderTop: '1px solid #2A2A2A', flexShrink: 0 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <div style={{ width: '10px', height: '10px', backgroundColor: '#3b82f6' }}></div>
          <span style={{ color: '#FFF', fontWeight: 600 }}>POC</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <div style={{ width: '10px', height: '10px', backgroundColor: '#10b981' }}></div>
          <span style={{ color: '#FFF', fontWeight: 600 }}>BUY</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <div style={{ width: '10px', height: '10px', backgroundColor: '#ef4444' }}></div>
          <span style={{ color: '#FFF', fontWeight: 600 }}>SELL</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <div style={{ width: '10px', height: '10px', backgroundColor: '#6b7280' }}></div>
          <span style={{ color: '#FFF', fontWeight: 600 }}>NEUTRAL</span>
        </div>
      </div>
    </div>
  );
}
