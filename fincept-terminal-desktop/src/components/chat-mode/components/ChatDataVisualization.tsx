// File: src/components/chat-mode/components/ChatDataVisualization.tsx
// Data visualization component for chat mode - renders charts, tables, etc.

import React from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { LineChart, Line, AreaChart, Area, BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { TrendingUp, TrendingDown, DollarSign, Percent } from 'lucide-react';

interface ChatDataVisualizationProps {
  data: any;
  dataType: 'chart' | 'table' | 'portfolio' | 'news' | 'market' | 'analytics' | 'help';
}

const ChatDataVisualization: React.FC<ChatDataVisualizationProps> = ({ data, dataType }) => {
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  const renderChart = () => {
    if (!data || !data.chartData) return null;

    return (
      <div
        style={{
          marginTop: '16px',
          background: colors.panel,
          border: `1px solid ${colors.primary}33`,
          borderRadius: '8px',
          padding: '16px',
          marginLeft: '48px'
        }}
      >
        <h4 style={{ color: colors.primary, fontSize: fontSize.body, marginBottom: '12px', fontWeight: 'bold' }}>
          {data.title || 'Data Visualization'}
        </h4>
        <ResponsiveContainer width="100%" height={300}>
          {data.chartType === 'line' ? (
            <LineChart data={data.chartData}>
              <CartesianGrid strokeDasharray="3 3" stroke={`${colors.textMuted}22`} />
              <XAxis dataKey="date" stroke={colors.textMuted} style={{ fontSize: fontSize.tiny }} />
              <YAxis stroke={colors.textMuted} style={{ fontSize: fontSize.tiny }} />
              <Tooltip
                contentStyle={{ background: colors.background, border: `1px solid ${colors.primary}`, borderRadius: '4px' }}
                labelStyle={{ color: colors.primary }}
              />
              <Legend />
              {data.series?.map((series: any, idx: number) => (
                <Line
                  key={series.name}
                  type="monotone"
                  dataKey={series.key}
                  stroke={[colors.primary, colors.success, colors.info, colors.warning][idx % 4]}
                  strokeWidth={2}
                  dot={false}
                />
              ))}
            </LineChart>
          ) : data.chartType === 'area' ? (
            <AreaChart data={data.chartData}>
              <CartesianGrid strokeDasharray="3 3" stroke={`${colors.textMuted}22`} />
              <XAxis dataKey="date" stroke={colors.textMuted} style={{ fontSize: fontSize.tiny }} />
              <YAxis stroke={colors.textMuted} style={{ fontSize: fontSize.tiny }} />
              <Tooltip
                contentStyle={{ background: colors.background, border: `1px solid ${colors.primary}`, borderRadius: '4px' }}
                labelStyle={{ color: colors.primary }}
              />
              <Legend />
              {data.series?.map((series: any, idx: number) => (
                <Area
                  key={series.name}
                  type="monotone"
                  dataKey={series.key}
                  stroke={[colors.primary, colors.success, colors.info, colors.warning][idx % 4]}
                  fill={`${[colors.primary, colors.success, colors.info, colors.warning][idx % 4]}33`}
                />
              ))}
            </AreaChart>
          ) : (
            <BarChart data={data.chartData}>
              <CartesianGrid strokeDasharray="3 3" stroke={`${colors.textMuted}22`} />
              <XAxis dataKey="name" stroke={colors.textMuted} style={{ fontSize: fontSize.tiny }} />
              <YAxis stroke={colors.textMuted} style={{ fontSize: fontSize.tiny }} />
              <Tooltip
                contentStyle={{ background: colors.background, border: `1px solid ${colors.primary}`, borderRadius: '4px' }}
                labelStyle={{ color: colors.primary }}
              />
              <Legend />
              {data.series?.map((series: any, idx: number) => (
                <Bar
                  key={series.name}
                  dataKey={series.key}
                  fill={[colors.primary, colors.success, colors.info, colors.warning][idx % 4]}
                />
              ))}
            </BarChart>
          )}
        </ResponsiveContainer>
      </div>
    );
  };

  const renderTable = () => {
    if (!data || !data.rows) return null;

    return (
      <div
        style={{
          marginTop: '16px',
          background: colors.panel,
          border: `1px solid ${colors.primary}33`,
          borderRadius: '8px',
          overflow: 'hidden',
          marginLeft: '48px'
        }}
      >
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ background: `${colors.primary}22`, borderBottom: `1px solid ${colors.primary}` }}>
              {data.columns?.map((col: string, idx: number) => (
                <th
                  key={idx}
                  style={{
                    padding: '12px',
                    textAlign: 'left',
                    color: colors.primary,
                    fontSize: fontSize.small,
                    fontWeight: 'bold'
                  }}
                >
                  {col}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {data.rows.map((row: any, rowIdx: number) => (
              <tr
                key={rowIdx}
                style={{
                  borderBottom: `1px solid ${colors.textMuted}22`,
                  transition: 'background 0.2s'
                }}
                onMouseEnter={(e) => e.currentTarget.style.background = `${colors.primary}11`}
                onMouseLeave={(e) => e.currentTarget.style.background = 'transparent'}
              >
                {Object.values(row).map((cell: any, cellIdx: number) => (
                  <td
                    key={cellIdx}
                    style={{
                      padding: '12px',
                      color: colors.text,
                      fontSize: fontSize.small
                    }}
                  >
                    {cell}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    );
  };

  const renderMarketData = () => {
    if (!data || !data.symbols) return null;

    return (
      <div
        style={{
          marginTop: '16px',
          marginLeft: '48px',
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(250px, 1fr))',
          gap: '12px'
        }}
      >
        {data.symbols.map((symbol: any, idx: number) => (
          <div
            key={idx}
            style={{
              background: colors.panel,
              border: `1px solid ${colors.primary}33`,
              borderRadius: '8px',
              padding: '14px'
            }}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '8px' }}>
              <div>
                <div style={{ color: colors.primary, fontSize: fontSize.body, fontWeight: 'bold' }}>
                  {symbol.symbol}
                </div>
                <div style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
                  {symbol.name}
                </div>
              </div>
              <div style={{ color: colors.text, fontSize: fontSize.heading, fontWeight: 'bold' }}>
                ${symbol.price}
              </div>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              {symbol.change >= 0 ? (
                <TrendingUp size={14} color={colors.success} />
              ) : (
                <TrendingDown size={14} color={colors.alert} />
              )}
              <span style={{ color: symbol.change >= 0 ? colors.success : colors.alert, fontSize: fontSize.small, fontWeight: 'bold' }}>
                {symbol.change >= 0 ? '+' : ''}{symbol.change} ({symbol.changePercent}%)
              </span>
            </div>
          </div>
        ))}
      </div>
    );
  };

  const renderPortfolio = () => {
    if (!data || !data.summary) return null;

    return (
      <div
        style={{
          marginTop: '16px',
          marginLeft: '48px'
        }}
      >
        <div
          style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))',
            gap: '12px',
            marginBottom: '16px'
          }}
        >
          <div
            style={{
              background: colors.panel,
              border: `1px solid ${colors.success}33`,
              borderRadius: '8px',
              padding: '14px'
            }}
          >
            <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, marginBottom: '4px' }}>
              TOTAL VALUE
            </div>
            <div style={{ color: colors.text, fontSize: fontSize.heading, fontWeight: 'bold' }}>
              ${data.summary.totalValue?.toLocaleString()}
            </div>
          </div>
          <div
            style={{
              background: colors.panel,
              border: `1px solid ${data.summary.totalGain >= 0 ? colors.success : colors.alert}33`,
              borderRadius: '8px',
              padding: '14px'
            }}
          >
            <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, marginBottom: '4px' }}>
              TOTAL GAIN/LOSS
            </div>
            <div style={{ color: data.summary.totalGain >= 0 ? colors.success : colors.alert, fontSize: fontSize.heading, fontWeight: 'bold' }}>
              {data.summary.totalGain >= 0 ? '+' : ''}${data.summary.totalGain?.toLocaleString()} ({data.summary.totalGainPercent}%)
            </div>
          </div>
        </div>
        {data.holdings && renderTable()}
      </div>
    );
  };

  switch (dataType) {
    case 'chart':
      return renderChart();
    case 'table':
      return renderTable();
    case 'market':
      return renderMarketData();
    case 'portfolio':
      return renderPortfolio();
    case 'analytics':
      return renderChart() || renderTable();
    default:
      return null;
  }
};

export default ChatDataVisualization;
