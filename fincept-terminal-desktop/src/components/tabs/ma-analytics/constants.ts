// M&A Analytics module constants — accent colors, chart palettes, module definitions
import { FINCEPT } from '../portfolio-tab/finceptStyles';

// Module accent colors for M&A tabs
export const MA_COLORS = {
  valuation: '#FF6B35',    // Orange — core valuation
  merger: '#00E5FF',       // Cyan — merger analysis
  dealDb: '#0088FF',       // Blue — deal database
  startup: '#9D4EDD',      // Purple — startup valuation
  fairness: '#00D66F',     // Green — fairness opinion
  industry: '#FFC400',     // Yellow — industry metrics
  advanced: '#FF3B8E',     // Pink — advanced analytics
  comparison: '#00BCD4',   // Teal — deal comparison
} as const;

// Chart series color palette (up to 8 series)
export const CHART_PALETTE = [
  '#FF6B35', '#00E5FF', '#9D4EDD', '#00D66F',
  '#FFC400', '#FF3B8E', '#0088FF', '#00BCD4',
];

// Chart styling constants
export const CHART_STYLE = {
  tooltip: {
    backgroundColor: FINCEPT.HEADER_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '9px',
    fontFamily: 'var(--ft-font-family, monospace)',
    color: FINCEPT.WHITE,
    padding: '6px 10px',
  },
  axis: {
    fontSize: 8,
    fill: FINCEPT.MUTED,
    fontFamily: 'var(--ft-font-family, monospace)',
  },
  grid: {
    stroke: FINCEPT.BORDER,
    strokeDasharray: '2 4',
  },
  label: {
    fontSize: 9,
    fill: FINCEPT.GRAY,
    fontFamily: 'var(--ft-font-family, monospace)',
  },
} as const;

// Module definitions for sidebar navigation
export const MA_MODULES = [
  { id: 'valuation', label: 'VALUATION TOOLKIT', category: 'CORE', color: MA_COLORS.valuation, shortLabel: 'VAL' },
  { id: 'merger', label: 'MERGER ANALYSIS', category: 'CORE', color: MA_COLORS.merger, shortLabel: 'MRG' },
  { id: 'deals', label: 'DEAL DATABASE', category: 'CORE', color: MA_COLORS.dealDb, shortLabel: 'DDB' },
  { id: 'startup', label: 'STARTUP VALUATION', category: 'SPECIALIZED', color: MA_COLORS.startup, shortLabel: 'STV' },
  { id: 'fairness', label: 'FAIRNESS OPINION', category: 'SPECIALIZED', color: MA_COLORS.fairness, shortLabel: 'FOP' },
  { id: 'industry', label: 'INDUSTRY METRICS', category: 'SPECIALIZED', color: MA_COLORS.industry, shortLabel: 'IND' },
  { id: 'advanced', label: 'ADVANCED ANALYTICS', category: 'ANALYTICS', color: MA_COLORS.advanced, shortLabel: 'ADV' },
  { id: 'comparison', label: 'DEAL COMPARISON', category: 'ANALYTICS', color: MA_COLORS.comparison, shortLabel: 'CMP' },
] as const;

export type MAModuleId = typeof MA_MODULES[number]['id'];

// Heatmap color scale (red → yellow → green)
export const HEATMAP_SCALE = [
  '#FF3B3B', '#FF6B35', '#FF9500', '#FFC400', '#FFD700',
  '#C8E600', '#7FD66F', '#00D66F', '#00C851', '#00A844',
];

// Format helpers
export function formatDealValue(value: number): string {
  if (Math.abs(value) >= 1e9) return `$${(value / 1e9).toFixed(1)}B`;
  if (Math.abs(value) >= 1e6) return `$${(value / 1e6).toFixed(1)}M`;
  if (Math.abs(value) >= 1e3) return `$${(value / 1e3).toFixed(0)}K`;
  return `$${value.toFixed(0)}`;
}

export function formatMultiple(value: number): string {
  return `${value.toFixed(1)}x`;
}

export function formatBps(value: number): string {
  return `${(value * 100).toFixed(0)}bps`;
}
