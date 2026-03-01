/**
 * AdvancedAnalytics - Types, Constants, Helpers, and Sub-Components
 */

import React, { useState } from 'react';
import { ChevronDown, ChevronUp } from 'lucide-react';
import { MA_COLORS, CHART_STYLE } from '../constants';
import { FINCEPT, TYPOGRAPHY } from '../../portfolio-tab/finceptStyles';

export type AnalysisType = 'montecarlo' | 'regression';

export interface CompanyData {
  name: string;
  ev: number;
  revenue: number;
  ebitda: number;
  growth: number;
}

export const DEFAULT_COMPS: CompanyData[] = [
  { name: 'Comp A', ev: 5000, revenue: 1200, ebitda: 350, growth: 15 },
  { name: 'Comp B', ev: 3200, revenue: 850, ebitda: 220, growth: 12 },
  { name: 'Comp C', ev: 7500, revenue: 1800, ebitda: 520, growth: 18 },
  { name: 'Comp D', ev: 4100, revenue: 950, ebitda: 280, growth: 10 },
];

export const ACCENT = MA_COLORS.advanced; // #FF3B8E pink

// ---------- Histogram / CDF generation utilities ----------

export const generateHistogramBins = (mean: number, std: number, p5: number, p95: number) => {
  const bins = 20;
  const range = p95 - p5;
  const binWidth = range / bins;
  return Array.from({ length: bins }, (_, i) => {
    const x = p5 + i * binWidth;
    const z = (x - mean) / std;
    const freq = Math.exp(-0.5 * z * z);
    let fill: string;
    if (z < -1) fill = '#FF3B3B';
    else if (z < 0) fill = '#FFD700';
    else if (z < 1) fill = '#00D66F';
    else fill = '#00E5FF';
    return {
      x: x / 1e6 >= 1 ? x / 1e6 : x,
      xRaw: x,
      freq: parseFloat(freq.toFixed(4)),
      fill,
      label: x / 1e6 >= 1 ? `${(x / 1e6).toFixed(1)}B` : `${x.toFixed(0)}M`,
    };
  });
};

export const generateCdfPoints = (result: any) => {
  const points = [
    { percentile: 5, value: result.p5 || 0 },
    { percentile: 25, value: result.p25 || 0 },
    { percentile: 50, value: result.median || 0 },
    { percentile: 75, value: result.p75 || 0 },
    { percentile: 95, value: result.p95 || 0 },
  ];
  return points.map(p => ({
    ...p,
    label: `P${p.percentile}`,
    displayValue: `$${p.value.toFixed(0)}M`,
  }));
};

export const formatValuation = (val: number): string => {
  if (Math.abs(val) >= 1e6) return `$${(val / 1e6).toFixed(1)}B`;
  if (Math.abs(val) >= 1e3) return `$${(val / 1e3).toFixed(1)}B`;
  return `$${val.toFixed(0)}M`;
};

export const formatAxisTick = (val: number): string => {
  if (Math.abs(val) >= 1e6) return `${(val / 1e6).toFixed(1)}B`;
  if (Math.abs(val) >= 1e3) return `${(val / 1e3).toFixed(0)}K`;
  return `${val.toFixed(0)}`;
};

// ---------- Collapsible section wrapper ----------

export const CollapsibleSection: React.FC<{
  title: string;
  icon?: React.ReactNode;
  defaultOpen?: boolean;
  children: React.ReactNode;
}> = ({ title, icon, defaultOpen = true, children }) => {
  const [open, setOpen] = useState(defaultOpen);
  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      overflow: 'hidden',
    }}>
      <div
        onClick={() => setOpen(!open)}
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '8px 12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: open ? `1px solid ${FINCEPT.BORDER}` : 'none',
          cursor: 'pointer',
          userSelect: 'none',
        }}
      >
        {icon && <span style={{ color: ACCENT, display: 'flex' }}>{icon}</span>}
        <span style={{
          fontSize: '10px',
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: ACCENT,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase',
        }}>
          {title}
        </span>
        <div style={{ flex: 1 }} />
        {open
          ? <ChevronUp size={12} color={FINCEPT.GRAY} />
          : <ChevronDown size={12} color={FINCEPT.GRAY} />
        }
      </div>
      {open && (
        <div style={{ padding: '12px' }}>
          {children}
        </div>
      )}
    </div>
  );
};

// ---------- Custom chart tooltip ----------

export const ChartTooltipContent: React.FC<any> = ({ active, payload, label, formatter }: any) => {
  if (!active || !payload?.length) return null;
  return (
    <div style={{
      ...CHART_STYLE.tooltip,
      padding: '8px 12px',
    }}>
      {label !== undefined && (
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontFamily: TYPOGRAPHY.MONO }}>
          {label}
        </div>
      )}
      {payload.map((entry: any, i: number) => (
        <div key={i} style={{
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          fontSize: '10px',
          fontFamily: TYPOGRAPHY.MONO,
          color: entry.color || FINCEPT.WHITE,
        }}>
          <div style={{ width: 6, height: 6, borderRadius: '1px', backgroundColor: entry.color || ACCENT }} />
          <span style={{ color: FINCEPT.GRAY }}>{entry.name}:</span>
          <span style={{ fontWeight: 700, color: FINCEPT.WHITE }}>
            {formatter ? formatter(entry.value) : entry.value}
          </span>
        </div>
      ))}
    </div>
  );
};
