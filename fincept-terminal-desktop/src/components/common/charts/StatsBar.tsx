// StatsBar — Generic statistics strip for any DataPoint[] dataset

import React from 'react';
import { TrendingUp, TrendingDown } from 'lucide-react';
import { FINCEPT_COLORS } from './types';
import type { ChartStats } from './types';

interface StatsBarProps {
  stats: ChartStats;
  sourceColor: string;
  formatValue: (val: number) => string;
}

export function StatsBar({ stats, sourceColor, formatValue }: StatsBarProps) {
  return (
    <div
      style={{
        padding: '12px 16px',
        backgroundColor: FINCEPT_COLORS.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}`,
        display: 'flex',
        gap: '24px',
        flexWrap: 'wrap',
      }}
    >
      <div>
        <div style={{ fontSize: '9px', color: FINCEPT_COLORS.GRAY, marginBottom: '4px' }}>LATEST VALUE</div>
        <div style={{ fontSize: '18px', fontWeight: 700, color: sourceColor }}>{formatValue(stats.latest)}</div>
      </div>
      <div>
        <div style={{ fontSize: '9px', color: FINCEPT_COLORS.GRAY, marginBottom: '4px' }}>CHANGE</div>
        <div style={{ fontSize: '14px', fontWeight: 700, color: stats.change >= 0 ? FINCEPT_COLORS.GREEN : FINCEPT_COLORS.RED, display: 'flex', alignItems: 'center', gap: '4px' }}>
          {stats.change >= 0 ? <TrendingUp size={14} /> : <TrendingDown size={14} />}
          {stats.change >= 0 ? '+' : ''}{stats.change.toFixed(2)}%
        </div>
      </div>
      <div>
        <div style={{ fontSize: '9px', color: FINCEPT_COLORS.GRAY, marginBottom: '4px' }}>MIN</div>
        <div style={{ fontSize: '12px', color: FINCEPT_COLORS.WHITE }}>{formatValue(stats.min)}</div>
      </div>
      <div>
        <div style={{ fontSize: '9px', color: FINCEPT_COLORS.GRAY, marginBottom: '4px' }}>MAX</div>
        <div style={{ fontSize: '12px', color: FINCEPT_COLORS.WHITE }}>{formatValue(stats.max)}</div>
      </div>
      <div>
        <div style={{ fontSize: '9px', color: FINCEPT_COLORS.GRAY, marginBottom: '4px' }}>AVERAGE</div>
        <div style={{ fontSize: '12px', color: FINCEPT_COLORS.WHITE }}>{formatValue(stats.avg)}</div>
      </div>
      <div>
        <div style={{ fontSize: '9px', color: FINCEPT_COLORS.GRAY, marginBottom: '4px' }}>DATA POINTS</div>
        <div style={{ fontSize: '12px', color: FINCEPT_COLORS.WHITE }}>{stats.count}</div>
      </div>
    </div>
  );
}

export default StatsBar;
