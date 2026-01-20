import React, { memo, useMemo } from 'react';
import { LineChart, Line, AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { Loader2, TrendingUp, AlertCircle } from 'lucide-react';
import type { FREDSeries } from '../types';
import { CHART_COLORS } from '../constants';

interface SeriesChartProps {
  data: FREDSeries[];
  chartType: 'line' | 'area';
  normalizeData: boolean;
  loading: boolean;
  loadingMessage: string;
  error: string | null;
  apiKeyConfigured: boolean;
  colors: any;
  fontSize: any;
}

function SeriesChart({
  data,
  chartType,
  normalizeData,
  loading,
  loadingMessage,
  error,
  apiKeyConfigured,
  colors,
  fontSize,
}: SeriesChartProps) {
  // Memoize chart data preparation
  const chartData = useMemo(() => {
    if (data.length === 0) return [];

    const allDates = new Set<string>();
    data.forEach(series => {
      series.observations.forEach(obs => allDates.add(obs.date));
    });

    const sortedDates = Array.from(allDates).sort();

    // First pass: collect all values
    const rawData = sortedDates.map(date => {
      const point: Record<string, any> = { date };
      data.forEach(series => {
        const obs = series.observations.find(o => o.date === date);
        point[series.series_id] = obs?.value ?? null;
      });
      return point;
    });

    // If normalization is enabled, normalize each series to 0-100 scale
    if (normalizeData && data.length > 1) {
      const seriesStats: Record<string, { min: number; max: number }> = {};

      data.forEach(series => {
        const values = series.observations
          .map(obs => parseFloat(obs.value.toString()))
          .filter(v => !isNaN(v) && v !== null);
        if (values.length > 0) {
          seriesStats[series.series_id] = {
            min: Math.min(...values),
            max: Math.max(...values)
          };
        }
      });

      return rawData.map(point => {
        const normalizedPoint: Record<string, any> = { date: point.date };
        data.forEach(series => {
          const value = point[series.series_id];
          if (value !== null && value !== undefined && seriesStats[series.series_id]) {
            const { min, max } = seriesStats[series.series_id];
            const range = max - min;
            normalizedPoint[series.series_id] = range > 0 ? ((value - min) / range) * 100 : 50;
          } else {
            normalizedPoint[series.series_id] = null;
          }
        });
        return normalizedPoint;
      });
    }

    return rawData;
  }, [data, normalizeData]);

  // Memoize tooltip style
  const tooltipStyle = useMemo(() => ({
    contentStyle: {
      background: colors.background,
      border: `1px solid ${colors.primary}`,
      borderRadius: '3px',
      fontSize: fontSize.small
    },
    labelStyle: { color: colors.primary, fontSize: fontSize.tiny },
    itemStyle: { color: colors.text, fontSize: fontSize.tiny }
  }), [colors, fontSize]);

  // Memoize axis style
  const axisStyle = useMemo(() => ({
    fontSize: fontSize.tiny,
    fill: colors.textMuted
  }), [colors.textMuted, fontSize.tiny]);

  if (loading) {
    return (
      <div style={{
        flex: 1,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        flexDirection: 'column',
        gap: '16px'
      }}>
        <Loader2 size={48} color={colors.primary} className="animate-spin" />
        <div style={{ textAlign: 'center' }}>
          <p style={{ color: colors.primary, fontSize: fontSize.body, fontWeight: 'bold', marginBottom: '6px' }}>
            {loadingMessage}
          </p>
          <p style={{ color: colors.textMuted, fontSize: fontSize.small }}>
            Please wait while we fetch data from FRED...
          </p>
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{
        background: `${colors.alert}11`,
        border: `2px solid ${colors.alert}`,
        color: colors.alert,
        padding: '16px',
        borderRadius: '4px',
        fontSize: '11px',
        marginBottom: '12px',
        display: 'flex',
        alignItems: 'flex-start',
        gap: '12px',
        boxShadow: `0 2px 8px ${colors.alert}33`
      }}>
        <AlertCircle size={20} style={{ flexShrink: 0, marginTop: '2px' }} />
        <div style={{ flex: 1 }}>
          <p style={{ fontWeight: 'bold', marginBottom: '4px' }}>Error Fetching Data</p>
          <p style={{ color: colors.text, fontSize: '10px', opacity: 0.8 }}>{error}</p>
          {!apiKeyConfigured && (
            <p style={{ color: colors.text, fontSize: '10px', marginTop: '8px', opacity: 0.8 }}>
              💡 Tip: Make sure you've configured your FRED API key in Settings → Credentials
            </p>
          )}
        </div>
      </div>
    );
  }

  if (data.length === 0) {
    return (
      <div style={{
        flex: 1,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color: colors.textMuted,
        fontSize: fontSize.body,
        flexDirection: 'column',
        gap: '10px'
      }}>
        <TrendingUp size={48} color={colors.textMuted} style={{ opacity: 0.3 }} />
        <p style={{ color: colors.text }}>No data to display</p>
        <p style={{ fontSize: fontSize.small, color: colors.textMuted }}>
          Enter series IDs and click Fetch to load data
        </p>
      </div>
    );
  }

  const ChartComponent = chartType === 'line' ? LineChart : AreaChart;
  const DataComponent = chartType === 'line' ? Line : Area;

  return (
    <div style={{ flex: 1, minHeight: 0 }}>
      <ResponsiveContainer width="100%" height="100%">
        <ChartComponent data={chartData} margin={{ top: 5, right: 20, left: 10, bottom: 5 }}>
          <CartesianGrid strokeDasharray="3 3" stroke={`${colors.textMuted}22`} />
          <XAxis dataKey="date" stroke={colors.textMuted} style={axisStyle} />
          <YAxis
            stroke={colors.textMuted}
            style={axisStyle}
            label={normalizeData ? {
              value: 'Normalized (0-100)',
              angle: -90,
              position: 'insideLeft',
              style: { fontSize: fontSize.tiny, fill: colors.textMuted }
            } : undefined}
          />
          <Tooltip {...tooltipStyle} />
          <Legend wrapperStyle={{ fontSize: fontSize.tiny }} />
          {data.map((series, idx) => {
            const color = CHART_COLORS[idx % CHART_COLORS.length];
            return chartType === 'line' ? (
              <Line
                key={series.series_id}
                type="monotone"
                dataKey={series.series_id}
                stroke={color}
                strokeWidth={2}
                dot={false}
                isAnimationActive={false}
              />
            ) : (
              <Area
                key={series.series_id}
                type="monotone"
                dataKey={series.series_id}
                stroke={color}
                fill={`${color}33`}
                isAnimationActive={false}
              />
            );
          })}
        </ChartComponent>
      </ResponsiveContainer>
    </div>
  );
}

export default memo(SeriesChart);
