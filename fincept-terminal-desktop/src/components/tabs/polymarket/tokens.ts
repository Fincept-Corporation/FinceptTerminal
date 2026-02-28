// Shared design tokens, helpers and mini-components for the Polymarket tab family.
import React from 'react';

export const C = {
  bg: '#000000',
  panel: '#0F0F0F',
  header: '#1A1A1A',
  hover: '#1A1A1A',
  border: '#2A2A2A',
  borderFaint: '#1A1A1A',
  muted: '#787878',
  faint: '#4A4A4A',
  orange: '#FF8800',
  green: '#00D66F',
  red: '#FF3B3B',
  cyan: '#00E5FF',
  white: '#FFFFFF',
  font: 'IBM Plex Mono, monospace',
} as const;

export const OUTCOME_COLORS = ['#00D66F', '#FF3B3B', '#FF8800', '#4F8EF7', '#A855F7'];

export const fmtVol = (val: any): string => {
  const n = typeof val === 'number' ? val : parseFloat(val) || 0;
  if (n >= 1_000_000) return `$${(n / 1_000_000).toFixed(2)}M`;
  if (n >= 1_000) return `$${(n / 1_000).toFixed(2)}K`;
  return `$${n.toFixed(2)}`;
};

export const parseJsonArr = (v: any): string[] => {
  try {
    const raw = typeof v === 'string' ? JSON.parse(v) : v;
    if (Array.isArray(raw)) return raw.map(String);
  } catch { /* ignore */ }
  return [];
};

export const parseNumArr = (v: any): number[] => {
  try {
    const raw = typeof v === 'string' ? JSON.parse(v) : v;
    if (Array.isArray(raw)) return raw.map((x: any) => parseFloat(x) || 0);
  } catch { /* ignore */ }
  return [];
};

export const sectionHeader = (title: string, extra?: React.ReactNode): React.ReactElement =>
  React.createElement('div', {
    style: {
      display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      padding: '6px 10px',
      backgroundColor: C.header,
      borderBottom: `1px solid ${C.border}`,
    },
  },
    React.createElement('span', {
      style: { fontSize: 9, fontWeight: 'bold', color: C.orange, textTransform: 'uppercase', letterSpacing: '0.5px', fontFamily: C.font },
    }, title),
    extra,
  );

export const statCell = (label: string, value: React.ReactNode, valueColor: string = C.white): React.ReactElement =>
  React.createElement('div', {
    key: label,
    style: { backgroundColor: C.bg, padding: '10px 12px' },
  },
    React.createElement('div', {
      style: { fontSize: 9, color: C.faint, fontWeight: 'bold', letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: 3, fontFamily: C.font },
    }, label),
    React.createElement('div', {
      style: { fontSize: 13, fontWeight: 'bold', color: valueColor, fontFamily: C.font },
    }, value),
  );
