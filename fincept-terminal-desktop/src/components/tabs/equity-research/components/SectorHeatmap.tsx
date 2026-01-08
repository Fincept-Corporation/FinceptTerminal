// Sector Heatmap Component
// Visualizes sector performance and peer distribution

import React, { useState } from 'react';
import { Treemap, ResponsiveContainer, Tooltip } from 'recharts';
import type { GICSSector, PeerCompany } from '../../../../types/peer';

interface SectorHeatmapProps {
  peers: PeerCompany[];
  selectedSector?: GICSSector;
  onSectorClick?: (sector: string) => void;
}

interface SectorData {
  name: string;
  value: number;
  count: number;
  avgMarketCap: number;
  avgSimilarity: number;
  color: string;
}

const SECTOR_COLORS: Record<string, string> = {
  'Energy': '#f59e0b',
  'Materials': '#8b5cf6',
  'Industrials': '#3b82f6',
  'Consumer Discretionary': '#ec4899',
  'Consumer Staples': '#10b981',
  'Health Care': '#ef4444',
  'Financials': '#06b6d4',
  'Information Technology': '#6366f1',
  'Communication Services': '#8b5cf6',
  'Utilities': '#14b8a6',
  'Real Estate': '#f97316',
};

export const SectorHeatmap: React.FC<SectorHeatmapProps> = ({
  peers,
  selectedSector,
  onSectorClick,
}) => {
  const [viewMode, setViewMode] = useState<'treemap' | 'grid'>('treemap');

  // Aggregate data by sector
  const sectorData: SectorData[] = React.useMemo(() => {
    const sectorMap = new Map<string, { count: number; totalMarketCap: number; totalSimilarity: number }>();

    peers.forEach((peer) => {
      const existing = sectorMap.get(peer.sector) || { count: 0, totalMarketCap: 0, totalSimilarity: 0 };
      sectorMap.set(peer.sector, {
        count: existing.count + 1,
        totalMarketCap: existing.totalMarketCap + peer.marketCap,
        totalSimilarity: existing.totalSimilarity + peer.similarityScore,
      });
    });

    return Array.from(sectorMap.entries()).map(([sector, data]) => ({
      name: sector,
      value: data.totalMarketCap,
      count: data.count,
      avgMarketCap: data.totalMarketCap / data.count,
      avgSimilarity: data.totalSimilarity / data.count,
      color: SECTOR_COLORS[sector] || '#6b7280',
    }));
  }, [peers]);

  const CustomTooltip = ({ active, payload }: any) => {
    if (active && payload && payload.length) {
      const data = payload[0].payload as SectorData;
      return (
        <div className="bg-gray-900 border border-gray-700 rounded-lg p-3 shadow-lg">
          <p className="text-white font-semibold mb-2">{data.name}</p>
          <div className="space-y-1 text-sm">
            <p className="text-blue-400">
              Companies: <span className="font-semibold">{data.count}</span>
            </p>
            <p className="text-green-400">
              Avg Market Cap: <span className="font-semibold">
                ${(data.avgMarketCap / 1e9).toFixed(2)}B
              </span>
            </p>
            <p className="text-purple-400">
              Avg Similarity: <span className="font-semibold">
                {(data.avgSimilarity * 100).toFixed(1)}%
              </span>
            </p>
          </div>
        </div>
      );
    }
    return null;
  };

  const CustomContent = (props: any) => {
    const { x, y, width, height, name, value, count, color } = props;

    if (width < 50 || height < 30) return null;

    return (
      <g>
        <rect
          x={x}
          y={y}
          width={width}
          height={height}
          fill={color}
          fillOpacity={0.8}
          stroke="#1f2937"
          strokeWidth={2}
          className="cursor-pointer hover:fill-opacity-100 transition-all"
          onClick={() => onSectorClick?.(name)}
        />
        <text
          x={x + width / 2}
          y={y + height / 2 - 10}
          textAnchor="middle"
          fill="#fff"
          fontSize={14}
          fontWeight="bold"
        >
          {name}
        </text>
        <text
          x={x + width / 2}
          y={y + height / 2 + 10}
          textAnchor="middle"
          fill="#fff"
          fontSize={12}
        >
          {count} {count === 1 ? 'company' : 'companies'}
        </text>
      </g>
    );
  };

  if (viewMode === 'grid') {
    return (
      <div className="space-y-4">
        <div className="flex justify-between items-center">
          <h3 className="text-lg font-semibold text-white">Sector Distribution</h3>
          <button
            onClick={() => setViewMode('treemap')}
            className="px-3 py-1 text-sm bg-gray-700 hover:bg-gray-600 text-white rounded transition-colors"
          >
            Switch to Treemap
          </button>
        </div>

        <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
          {sectorData.map((sector) => (
            <div
              key={sector.name}
              className={`p-4 rounded-lg border-2 transition-all cursor-pointer ${
                selectedSector === sector.name
                  ? 'border-blue-500 bg-gray-800'
                  : 'border-gray-700 bg-gray-900 hover:border-gray-600'
              }`}
              onClick={() => onSectorClick?.(sector.name)}
              style={{
                borderLeftWidth: '4px',
                borderLeftColor: sector.color,
              }}
            >
              <h4 className="text-white font-semibold text-sm mb-2 truncate">{sector.name}</h4>
              <div className="space-y-1 text-xs">
                <div className="flex justify-between text-gray-400">
                  <span>Companies:</span>
                  <span className="font-semibold text-white">{sector.count}</span>
                </div>
                <div className="flex justify-between text-gray-400">
                  <span>Avg Cap:</span>
                  <span className="font-semibold text-white">
                    ${(sector.avgMarketCap / 1e9).toFixed(1)}B
                  </span>
                </div>
                <div className="flex justify-between text-gray-400">
                  <span>Similarity:</span>
                  <span className="font-semibold" style={{ color: sector.color }}>
                    {(sector.avgSimilarity * 100).toFixed(0)}%
                  </span>
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>
    );
  }

  // Treemap view
  return (
    <div className="space-y-4">
      <div className="flex justify-between items-center">
        <h3 className="text-lg font-semibold text-white">Sector Heatmap</h3>
        <button
          onClick={() => setViewMode('grid')}
          className="px-3 py-1 text-sm bg-gray-700 hover:bg-gray-600 text-white rounded transition-colors"
        >
          Switch to Grid
        </button>
      </div>

      <ResponsiveContainer width="100%" height={400}>
        <Treemap
          data={sectorData}
          dataKey="value"
          stroke="#1f2937"
          fill="#8884d8"
          content={<CustomContent />}
        >
          <Tooltip content={<CustomTooltip />} />
        </Treemap>
      </ResponsiveContainer>

      <div className="flex flex-wrap gap-3 justify-center">
        {sectorData.map((sector) => (
          <div key={sector.name} className="flex items-center gap-2">
            <div
              className="w-4 h-4 rounded"
              style={{ backgroundColor: sector.color }}
            />
            <span className="text-sm text-gray-400">{sector.name}</span>
          </div>
        ))}
      </div>
    </div>
  );
};

// Sector performance indicator
interface SectorPerformanceProps {
  sector: string;
  metrics: {
    avgPE?: number;
    avgROE?: number;
    avgGrowth?: number;
  };
}

export const SectorPerformance: React.FC<SectorPerformanceProps> = ({ sector, metrics }) => {
  const color = SECTOR_COLORS[sector] || '#6b7280';

  return (
    <div className="bg-gray-900 rounded-lg p-4 border border-gray-700">
      <div className="flex items-center gap-3 mb-3">
        <div className="w-3 h-3 rounded-full" style={{ backgroundColor: color }} />
        <h4 className="text-white font-semibold">{sector}</h4>
      </div>

      <div className="grid grid-cols-3 gap-4 text-sm">
        {metrics.avgPE !== undefined && (
          <div>
            <p className="text-gray-400 text-xs">Avg P/E</p>
            <p className="text-white font-semibold">{metrics.avgPE.toFixed(2)}</p>
          </div>
        )}
        {metrics.avgROE !== undefined && (
          <div>
            <p className="text-gray-400 text-xs">Avg ROE</p>
            <p className="text-white font-semibold">{(metrics.avgROE * 100).toFixed(1)}%</p>
          </div>
        )}
        {metrics.avgGrowth !== undefined && (
          <div>
            <p className="text-gray-400 text-xs">Avg Growth</p>
            <p className="text-white font-semibold">{(metrics.avgGrowth * 100).toFixed(1)}%</p>
          </div>
        )}
      </div>
    </div>
  );
};
