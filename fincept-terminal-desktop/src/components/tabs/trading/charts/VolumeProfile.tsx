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
  limit = 100
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

  return (
    <div className="bg-black border border-gray-800 rounded p-2">
      <div className="flex items-center justify-between mb-2 px-2">
        <span className="text-xs text-gray-400">VOLUME PROFILE</span>
        <span className="text-xs font-mono text-blue-400">
          POC: ${poc.price.toFixed(2)}
        </span>
      </div>

      <ResponsiveContainer width="100%" height={height}>
        <BarChart
          data={volumeProfile}
          layout="horizontal"
          margin={{ top: 5, right: 5, left: 50, bottom: 20 }}
        >
          <XAxis
            type="number"
            stroke="#4b5563"
            tick={{ fill: '#9ca3af', fontSize: 11 }}
            tickFormatter={(value) => `${(value / 1000).toFixed(0)}K`}
          />

          <YAxis
            type="number"
            dataKey="price"
            stroke="#4b5563"
            tick={{ fill: '#9ca3af', fontSize: 11 }}
            tickFormatter={(value) => `$${value.toFixed(0)}`}
            domain={['auto', 'auto']}
          />

          <Tooltip
            contentStyle={{
              backgroundColor: '#1f2937',
              border: '1px solid #374151',
              borderRadius: '4px',
              fontSize: '12px',
            }}
            labelStyle={{ color: '#d1d5db' }}
            itemStyle={{ color: '#d1d5db' }}
            formatter={(value: number, name: string) => {
              if (name === 'buyVolume' || name === 'sellVolume') {
                return value.toFixed(2);
              }
              return value.toFixed(2);
            }}
            labelFormatter={(label) => `Price: $${label}`}
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

      {/* Legend */}
      <div className="flex items-center justify-center gap-4 mt-2 text-xs">
        <div className="flex items-center gap-1">
          <div className="w-3 h-3 bg-blue-500 rounded"></div>
          <span className="text-gray-400">POC</span>
        </div>
        <div className="flex items-center gap-1">
          <div className="w-3 h-3 bg-green-500 rounded"></div>
          <span className="text-gray-400">Buy Heavy</span>
        </div>
        <div className="flex items-center gap-1">
          <div className="w-3 h-3 bg-red-500 rounded"></div>
          <span className="text-gray-400">Sell Heavy</span>
        </div>
        <div className="flex items-center gap-1">
          <div className="w-3 h-3 bg-gray-500 rounded"></div>
          <span className="text-gray-400">Neutral</span>
        </div>
      </div>
    </div>
  );
}
