/**
 * FinancialsTab - Reusable Components, Constants, and Helpers
 */
import React, { useState } from 'react';
import { FINCEPT, TYPOGRAPHY, BORDERS } from '../../portfolio-tab/finceptStyles';
import { ArrowUpRight, ArrowDownRight, ChevronDown, ChevronRight } from 'lucide-react';

export const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  MUTED: FINCEPT.MUTED,
  BLUE: FINCEPT.BLUE,
  CYAN: FINCEPT.CYAN,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  HEADER_BG: FINCEPT.HEADER_BG,
  HOVER: FINCEPT.HOVER,
  BORDER: FINCEPT.BORDER,
  PURPLE: FINCEPT.PURPLE,
};

export const CHART_COLORS = [COLORS.GREEN, COLORS.CYAN, COLORS.ORANGE, COLORS.YELLOW, COLORS.PURPLE, COLORS.BLUE, COLORS.RED];

// ============================================================================
// Helper functions
// ============================================================================
export const formatPercent = (value: number | null | undefined): string => {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return `${(value * 100).toFixed(1)}%`;
};

export const formatCurrency = (value: number | null | undefined): string => {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  const absValue = Math.abs(value);
  const sign = value < 0 ? '-' : '';
  if (absValue >= 1e12) return `${sign}$${(absValue / 1e12).toFixed(2)}T`;
  if (absValue >= 1e9) return `${sign}$${(absValue / 1e9).toFixed(2)}B`;
  if (absValue >= 1e6) return `${sign}$${(absValue / 1e6).toFixed(2)}M`;
  if (absValue >= 1e3) return `${sign}$${(absValue / 1e3).toFixed(2)}K`;
  return `${sign}$${absValue.toFixed(2)}`;
};

export const formatRatio = (value: number | null | undefined): string => {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return `${value.toFixed(2)}x`;
};

export const getHealthColor = (value: number, thresholds: { good: number; warning: number }, higherIsBetter: boolean = true): string => {
  if (higherIsBetter) {
    if (value >= thresholds.good) return COLORS.GREEN;
    if (value >= thresholds.warning) return COLORS.YELLOW;
    return COLORS.RED;
  } else {
    if (value <= thresholds.good) return COLORS.GREEN;
    if (value <= thresholds.warning) return COLORS.YELLOW;
    return COLORS.RED;
  }
};

export const calculateGrowth = (current: number, previous: number): number | null => {
  if (!previous || previous === 0) return null;
  return ((current - previous) / Math.abs(previous)) * 100;
};

// ============================================================================
// Reusable components
// ============================================================================

// Metric Card with proper sizing
export const MetricCard: React.FC<{
  label: string;
  value: string | number;
  change?: number | null;
  color?: string;
  icon?: React.ReactNode;
  subtitle?: string;
  trend?: 'up' | 'down' | 'neutral';
  size?: 'small' | 'medium' | 'large';
}> = ({ label, value, change, color = COLORS.WHITE, icon, subtitle, trend, size = 'medium' }) => {
  const TrendIcon = trend === 'up' ? ArrowUpRight : trend === 'down' ? ArrowDownRight : null;
  const trendColor = trend === 'up' ? COLORS.GREEN : trend === 'down' ? COLORS.RED : COLORS.GRAY;

  const padding = size === 'small' ? '10px' : size === 'large' ? '16px' : '12px';
  const labelSize = size === 'small' ? '10px' : size === 'large' ? '12px' : '11px';
  const valueSize = size === 'small' ? '16px' : size === 'large' ? '24px' : '20px';

  return (
    <div style={{
      backgroundColor: COLORS.DARK_BG,
      border: BORDERS.STANDARD,
      borderLeft: `3px solid ${color}`,
      padding: padding,
      minHeight: size === 'large' ? '90px' : '70px',
      display: 'flex',
      flexDirection: 'column',
      justifyContent: 'space-between',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
        <span style={{ color: COLORS.GRAY, fontSize: labelSize, textTransform: 'uppercase', fontWeight: 600 }}>{label}</span>
        {icon && <span style={{ color: color, opacity: 0.8 }}>{icon}</span>}
      </div>
      <div style={{ color: color, fontSize: valueSize, fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>{value}</div>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '4px' }}>
        {(change !== undefined && change !== null) && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
            {TrendIcon && <TrendIcon size={12} color={trendColor} />}
            <span style={{ color: trendColor, fontSize: '11px', fontWeight: 600 }}>
              {change >= 0 ? '+' : ''}{change.toFixed(1)}% YoY
            </span>
          </div>
        )}
        {subtitle && <span style={{ color: COLORS.MUTED, fontSize: '10px' }}>{subtitle}</span>}
      </div>
    </div>
  );
};

// Health Score with visual bar
export const HealthScore: React.FC<{
  label: string;
  value: number;
  max?: number;
  thresholds: { good: number; warning: number };
  higherIsBetter?: boolean;
  format?: 'percent' | 'ratio' | 'number';
  description?: string;
}> = ({ label, value, max = 100, thresholds, higherIsBetter = true, format = 'number', description }) => {
  const healthColor = getHealthColor(value, thresholds, higherIsBetter);
  const percentage = Math.min(100, Math.max(0, (Math.abs(value) / max) * 100));

  let displayValue = value.toFixed(2);
  if (format === 'percent') displayValue = `${(value * 100).toFixed(1)}%`;
  if (format === 'ratio') displayValue = `${value.toFixed(2)}x`;

  return (
    <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
        <span style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600 }}>{label}</span>
        <span style={{ color: healthColor, fontSize: '16px', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>{displayValue}</span>
      </div>
      <div style={{ height: '6px', backgroundColor: COLORS.PANEL_BG, borderRadius: '3px', overflow: 'hidden' }}>
        <div style={{
          width: `${percentage}%`,
          height: '100%',
          backgroundColor: healthColor,
          borderRadius: '3px',
          transition: 'width 0.3s ease'
        }} />
      </div>
      {description && <div style={{ color: COLORS.MUTED, fontSize: '10px', marginTop: '6px' }}>{description}</div>}
    </div>
  );
};

// Section Panel with collapsible content
export const SectionPanel: React.FC<{
  title: string;
  color: string;
  icon: React.ReactNode;
  children: React.ReactNode;
  defaultExpanded?: boolean;
  subtitle?: string;
}> = ({ title, color, icon, children, defaultExpanded = true, subtitle }) => {
  const [expanded, setExpanded] = useState(defaultExpanded);

  return (
    <div style={{ backgroundColor: COLORS.PANEL_BG, border: BORDERS.STANDARD, marginBottom: '16px', borderRadius: '4px', overflow: 'hidden' }}>
      <div
        onClick={() => setExpanded(!expanded)}
        style={{
          backgroundColor: COLORS.HEADER_BG,
          padding: '12px 16px',
          borderBottom: expanded ? `2px solid ${color}` : 'none',
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
          cursor: 'pointer',
        }}
      >
        {expanded ? <ChevronDown size={16} color={color} /> : <ChevronRight size={16} color={color} />}
        {icon}
        <div style={{ flex: 1 }}>
          <span style={{ color: color, fontSize: '13px', fontWeight: 700, textTransform: 'uppercase' }}>{title}</span>
          {subtitle && <span style={{ color: COLORS.MUTED, fontSize: '11px', marginLeft: '12px' }}>{subtitle}</span>}
        </div>
      </div>
      {expanded && <div style={{ padding: '16px' }}>{children}</div>}
    </div>
  );
};

// Chart Wrapper for consistent styling
export const ChartWrapper: React.FC<{
  title: string;
  color: string;
  height?: number;
  children: React.ReactNode;
}> = ({ title, color, height = 220, children }) => (
  <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, borderRadius: '4px', overflow: 'hidden' }}>
    <div style={{
      padding: '10px 14px',
      borderBottom: `2px solid ${color}`,
      backgroundColor: COLORS.HEADER_BG
    }}>
      <span style={{ color: color, fontSize: '12px', fontWeight: 700, textTransform: 'uppercase' }}>{title}</span>
    </div>
    <div style={{ padding: '12px', height: height }}>
      {children}
    </div>
  </div>
);

// Custom Tooltip for charts
export const CustomTooltip: React.FC<any> = ({ active, payload, label, formatter }) => {
  if (!active || !payload?.length) return null;
  return (
    <div style={{
      backgroundColor: COLORS.DARK_BG,
      border: BORDERS.STANDARD,
      padding: '10px 14px',
      borderRadius: '4px',
      boxShadow: '0 4px 12px rgba(0,0,0,0.3)'
    }}>
      <div style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600, marginBottom: '6px' }}>{label}</div>
      {payload.map((entry: any, idx: number) => (
        <div key={idx} style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '4px' }}>
          <div style={{ width: '10px', height: '10px', backgroundColor: entry.color, borderRadius: '2px' }} />
          <span style={{ color: COLORS.GRAY, fontSize: '11px' }}>{entry.name}:</span>
          <span style={{ color: entry.color, fontSize: '11px', fontWeight: 600, fontFamily: TYPOGRAPHY.MONO }}>
            {formatter ? formatter(entry.value) : entry.value}
          </span>
        </div>
      ))}
    </div>
  );
};
