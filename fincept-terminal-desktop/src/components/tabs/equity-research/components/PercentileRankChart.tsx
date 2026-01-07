// Percentile Rank Chart Component
// Visualizes percentile rankings with bar charts

import React from 'react';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, Cell } from 'recharts';
import type { PercentileRanking } from '../../../../types/peer';
import { getPercentileColor } from '../../../../types/peer';

interface PercentileRankChartProps {
  rankings: PercentileRanking[];
  orientation?: 'horizontal' | 'vertical';
  title?: string;
  height?: number;
}

export const PercentileRankChart: React.FC<PercentileRankChartProps> = ({
  rankings,
  orientation = 'horizontal',
  title = 'Percentile Rankings',
  height = 400,
}) => {
  // Prepare data for chart
  const chartData = rankings.map((ranking) => ({
    metric: ranking.metricName,
    percentile: ranking.percentile,
    value: ranking.value,
    median: ranking.peerMedian,
    mean: ranking.peerMean,
    zScore: ranking.zScore,
  }));

  const CustomTooltip = ({ active, payload }: any) => {
    if (active && payload && payload.length) {
      const data = payload[0].payload;
      return (
        <div className="bg-gray-900 border border-gray-700 rounded-lg p-3 shadow-lg">
          <p className="text-white font-semibold mb-2">{data.metric}</p>
          <div className="space-y-1 text-sm">
            <p className="text-blue-400">
              Percentile: <span className="font-semibold">{data.percentile.toFixed(1)}%</span>
            </p>
            <p className="text-green-400">
              Value: <span className="font-semibold">{data.value.toFixed(2)}</span>
            </p>
            <p className="text-yellow-400">
              Peer Median: <span className="font-semibold">{data.median.toFixed(2)}</span>
            </p>
            <p className="text-purple-400">
              Z-Score: <span className="font-semibold">{data.zScore.toFixed(2)}</span>
            </p>
          </div>
        </div>
      );
    }
    return null;
  };

  if (orientation === 'horizontal') {
    return (
      <div className="w-full">
        {title && <h3 className="text-lg font-semibold text-white mb-4">{title}</h3>}
        <ResponsiveContainer width="100%" height={height}>
          <BarChart
            data={chartData}
            layout="vertical"
            margin={{ top: 5, right: 30, left: 100, bottom: 5 }}
          >
            <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
            <XAxis type="number" domain={[0, 100]} stroke="#9ca3af" />
            <YAxis dataKey="metric" type="category" stroke="#9ca3af" width={90} />
            <Tooltip content={<CustomTooltip />} />
            <Legend />
            <Bar dataKey="percentile" name="Percentile Rank" radius={[0, 4, 4, 0]}>
              {chartData.map((entry, index) => (
                <Cell key={`cell-${index}`} fill={getPercentileColor(entry.percentile)} />
              ))}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </div>
    );
  }

  // Vertical orientation
  return (
    <div className="w-full">
      {title && <h3 className="text-lg font-semibold text-white mb-4">{title}</h3>}
      <ResponsiveContainer width="100%" height={height}>
        <BarChart
          data={chartData}
          margin={{ top: 5, right: 30, left: 20, bottom: 100 }}
        >
          <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
          <XAxis
            dataKey="metric"
            stroke="#9ca3af"
            angle={-45}
            textAnchor="end"
            height={100}
          />
          <YAxis domain={[0, 100]} stroke="#9ca3af" />
          <Tooltip content={<CustomTooltip />} />
          <Legend />
          <Bar dataKey="percentile" name="Percentile Rank" radius={[4, 4, 0, 0]}>
            {chartData.map((entry, index) => (
              <Cell key={`cell-${index}`} fill={getPercentileColor(entry.percentile)} />
            ))}
          </Bar>
        </BarChart>
      </ResponsiveContainer>
    </div>
  );
};

// Compact version for dashboard widgets
export const PercentileRankMini: React.FC<{ ranking: PercentileRanking }> = ({ ranking }) => {
  const color = getPercentileColor(ranking.percentile);
  const width = `${ranking.percentile}%`;

  // Determine interpretation based on metric type
  const isValuationMetric = ranking.metricName.includes('Ratio') ||
                            ranking.metricName.includes('P/E') ||
                            ranking.metricName.includes('P/B') ||
                            ranking.metricName.includes('P/S') ||
                            ranking.metricName.includes('Debt');

  // For valuation and leverage metrics, lower is better
  // For profitability and growth metrics, higher is better
  let interpretation = '';
  if (ranking.percentile <= 25) {
    interpretation = isValuationMetric ? 'Lower than peers (better value)' : 'Lower than peers';
  } else if (ranking.percentile <= 50) {
    interpretation = 'Below median';
  } else if (ranking.percentile <= 75) {
    interpretation = 'Above median';
  } else {
    interpretation = isValuationMetric ? 'Higher than peers (expensive)' : 'Higher than peers (good)';
  }

  return (
    <div className="space-y-1">
      <div className="flex justify-between items-center text-sm">
        <span className="text-gray-400">{ranking.metricName}</span>
        <div className="flex items-center gap-2">
          <span className="text-white font-semibold">{ranking.percentile !== null && ranking.percentile !== undefined ? ranking.percentile.toFixed(1) : 'N/A'}%</span>
          <span className="text-xs text-gray-500">({interpretation})</span>
        </div>
      </div>
      <div className="w-full bg-gray-700 rounded-full h-2">
        <div
          className="h-2 rounded-full transition-all duration-300"
          style={{ width, backgroundColor: color }}
        />
      </div>
      <div className="flex justify-between text-xs text-gray-500">
        <span>Value: {ranking.value !== null && ranking.value !== undefined ? ranking.value.toFixed(2) : 'N/A'}</span>
        <span>Median: {ranking.peerMedian !== null && ranking.peerMedian !== undefined ? ranking.peerMedian.toFixed(2) : 'N/A'}</span>
      </div>
    </div>
  );
};

// Percentile comparison table
interface PercentileTableProps {
  rankings: PercentileRanking[];
}

export const PercentileTable: React.FC<PercentileTableProps> = ({ rankings }) => {
  return (
    <div className="overflow-x-auto">
      <table className="min-w-full divide-y divide-gray-700">
        <thead className="bg-gray-800">
          <tr>
            <th className="px-4 py-3 text-left text-xs font-medium text-gray-400 uppercase tracking-wider">
              Metric
            </th>
            <th className="px-4 py-3 text-right text-xs font-medium text-gray-400 uppercase tracking-wider">
              Value
            </th>
            <th className="px-4 py-3 text-right text-xs font-medium text-gray-400 uppercase tracking-wider">
              Percentile
            </th>
            <th className="px-4 py-3 text-right text-xs font-medium text-gray-400 uppercase tracking-wider">
              Z-Score
            </th>
            <th className="px-4 py-3 text-right text-xs font-medium text-gray-400 uppercase tracking-wider">
              Peer Median
            </th>
            <th className="px-4 py-3 text-right text-xs font-medium text-gray-400 uppercase tracking-wider">
              Status
            </th>
          </tr>
        </thead>
        <tbody className="bg-gray-900 divide-y divide-gray-800">
          {rankings.map((ranking, index) => {
            const color = getPercentileColor(ranking.percentile);
            const status =
              ranking.percentile >= 75 ? 'Excellent' :
              ranking.percentile >= 50 ? 'Above Average' :
              ranking.percentile >= 25 ? 'Below Average' :
              'Poor';

            return (
              <tr key={index} className="hover:bg-gray-800 transition-colors">
                <td className="px-4 py-3 whitespace-nowrap text-sm font-medium text-white">
                  {ranking.metricName}
                </td>
                <td className="px-4 py-3 whitespace-nowrap text-sm text-right text-gray-300">
                  {ranking.value.toFixed(2)}
                </td>
                <td className="px-4 py-3 whitespace-nowrap text-sm text-right">
                  <span className="font-semibold" style={{ color }}>
                    {ranking.percentile.toFixed(1)}%
                  </span>
                </td>
                <td className="px-4 py-3 whitespace-nowrap text-sm text-right text-gray-300">
                  {ranking.zScore.toFixed(2)}
                </td>
                <td className="px-4 py-3 whitespace-nowrap text-sm text-right text-gray-400">
                  {ranking.peerMedian.toFixed(2)}
                </td>
                <td className="px-4 py-3 whitespace-nowrap text-sm text-right">
                  <span
                    className="px-2 py-1 rounded-full text-xs font-medium"
                    style={{
                      backgroundColor: `${color}20`,
                      color: color,
                    }}
                  >
                    {status}
                  </span>
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
};
