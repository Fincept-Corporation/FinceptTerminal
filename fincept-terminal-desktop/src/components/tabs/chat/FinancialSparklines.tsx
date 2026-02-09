import React, { useMemo } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface DataPoint {
  period: string;
  value: number;
}

interface ChartData {
  revenue?: DataPoint[];
  net_income?: DataPoint[];
  total_assets?: DataPoint[];
  total_equity?: DataPoint[];
  growth?: {
    revenue_yoy?: number;
    net_income_yoy?: number;
    total_assets_yoy?: number;
    total_equity_yoy?: number;
  };
}

interface FinancialSparklinesProps {
  data: ChartData;
  ticker?: string;
  company?: string;
  responseText?: string;
}

// Keywords to match in response text
const METRIC_KEYWORDS: Record<string, string[]> = {
  revenue: ['revenue', 'sales', 'top line', 'total revenue'],
  net_income: ['net income', 'profit', 'earnings', 'bottom line', 'net profit'],
  total_assets: ['assets', 'total assets'],
  total_equity: ['equity', 'shareholders equity', 'book value'],
};

// Format large numbers
const formatValue = (value: number): string => {
  if (value >= 1000) return `$${(value / 1000).toFixed(1)}B`;
  if (value >= 1) return `$${value.toFixed(1)}M`;
  return `$${(value * 1000).toFixed(0)}K`;
};

// Simple SVG sparkline
const Sparkline: React.FC<{ values: number[]; color: string; width?: number; height?: number }> = ({
  values,
  color,
  width = 60,
  height = 24
}) => {
  if (values.length < 2) return null;

  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;

  const points = values.map((v, i) => {
    const x = (i / (values.length - 1)) * width;
    const y = height - ((v - min) / range) * (height - 4) - 2;
    return `${x},${y}`;
  }).join(' ');

  return (
    <svg width={width} height={height} style={{ display: 'block' }}>
      <polyline
        points={points}
        fill="none"
        stroke={color}
        strokeWidth="2"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
      {/* End dot */}
      <circle
        cx={(values.length - 1) / (values.length - 1) * width}
        cy={height - ((values[values.length - 1] - min) / range) * (height - 4) - 2}
        r="3"
        fill={color}
      />
    </svg>
  );
};

// Single metric row
const MetricRow: React.FC<{
  label: string;
  values: DataPoint[];
  growth?: number;
  color: string;
}> = ({ label, values, growth, color }) => {
  const { colors, fontSize } = useTerminalTheme();
  const sortedValues = [...values].sort((a, b) => a.period.localeCompare(b.period));
  const numericValues = sortedValues.map(v => v.value);
  const latest = sortedValues[sortedValues.length - 1];
  const oldest = sortedValues[0];

  return (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      gap: '12px',
      padding: '6px 10px',
      background: 'rgba(255,255,255,0.03)',
      borderRadius: '4px',
      borderLeft: `3px solid ${color}`,
    }}>
      <div style={{ minWidth: '80px' }}>
        <div style={{ fontSize: fontSize.small, color: colors.textMuted, textTransform: 'uppercase', letterSpacing: '0.5px' }}>
          {label}
        </div>
        <div style={{ fontSize: fontSize.subheading, color: colors.text, fontWeight: 600 }}>
          {formatValue(latest.value)}
        </div>
      </div>

      <Sparkline values={numericValues} color={color} />

      <div style={{ fontSize: fontSize.small, color: colors.textMuted, minWidth: '70px' }}>
        {formatValue(oldest.value)} → {formatValue(latest.value)}
      </div>

      {growth !== undefined && (
        <div style={{
          fontSize: fontSize.body,
          fontWeight: 600,
          color: growth >= 0 ? colors.success : colors.alert,
          minWidth: '50px',
          textAlign: 'right'
        }}>
          {growth >= 0 ? '+' : ''}{growth.toFixed(1)}%
        </div>
      )}
    </div>
  );
};

const FinancialSparklines: React.FC<FinancialSparklinesProps> = ({
  data,
  ticker,
  company,
  responseText = ''
}) => {
  const { colors, fontSize } = useTerminalTheme();

  // Determine which metrics to show based on response text
  const metricsToShow = useMemo(() => {
    const text = responseText.toLowerCase();
    const metrics: Array<{ key: keyof ChartData; label: string; color: string }> = [];

    // Check each metric type - use theme colors
    if (data.revenue?.length) {
      const hasKeyword = METRIC_KEYWORDS.revenue.some(kw => text.includes(kw));
      if (hasKeyword || !responseText) {
        metrics.push({ key: 'revenue', label: 'Revenue', color: colors.secondary });
      }
    }

    if (data.net_income?.length) {
      const hasKeyword = METRIC_KEYWORDS.net_income.some(kw => text.includes(kw));
      if (hasKeyword || !responseText) {
        metrics.push({ key: 'net_income', label: 'Net Income', color: colors.success });
      }
    }

    if (data.total_assets?.length) {
      const hasKeyword = METRIC_KEYWORDS.total_assets.some(kw => text.includes(kw));
      if (hasKeyword) {
        metrics.push({ key: 'total_assets', label: 'Assets', color: colors.warning });
      }
    }

    if (data.total_equity?.length) {
      const hasKeyword = METRIC_KEYWORDS.total_equity.some(kw => text.includes(kw));
      if (hasKeyword) {
        metrics.push({ key: 'total_equity', label: 'Equity', color: colors.primary });
      }
    }

    // If no keywords matched but we have data, show revenue and net_income by default
    if (metrics.length === 0) {
      if (data.revenue?.length) metrics.push({ key: 'revenue', label: 'Revenue', color: colors.secondary });
      if (data.net_income?.length) metrics.push({ key: 'net_income', label: 'Net Income', color: colors.success });
    }

    return metrics;
  }, [data, responseText, colors]);

  if (metricsToShow.length === 0) return null;

  return (
    <div style={{
      marginTop: '12px',
      padding: '10px',
      background: 'rgba(0,0,0,0.3)',
      borderRadius: '6px',
      border: '1px solid rgba(255,255,255,0.1)',
    }}>
      {(ticker || company) && (
        <div style={{
          fontSize: fontSize.small,
          color: colors.textMuted,
          marginBottom: '8px',
          textTransform: 'uppercase',
          letterSpacing: '0.5px'
        }}>
          {company || ticker} • Financial Trends
        </div>
      )}

      <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
        {metricsToShow.map(metric => {
          const values = data[metric.key] as DataPoint[] | undefined;
          if (!values?.length) return null;

          const growthKey = `${metric.key}_yoy` as keyof NonNullable<ChartData['growth']>;
          const growth = data.growth?.[growthKey];

          return (
            <MetricRow
              key={metric.key}
              label={metric.label}
              values={values}
              growth={growth}
              color={metric.color}
            />
          );
        })}
      </div>
    </div>
  );
};

export default FinancialSparklines;
