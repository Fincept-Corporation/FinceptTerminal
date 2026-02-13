// US Treasury Summary Panel - Aggregate market statistics with visual breakdown

import React, { useMemo } from 'react';
import { FINCEPT } from '../constants';
import type { TreasurySummaryResponse } from '../types';

interface Props {
  data: TreasurySummaryResponse;
}

const SECURITY_COLORS: Record<string, string> = {
  'MARKET BASED BILL': '#3B82F6',
  'MARKET BASED NOTE': '#10B981',
  'MARKET BASED BOND': '#F59E0B',
  'TIPS': '#8B5CF6',
  'MARKET BASED FRN': '#EC4899',
};

const formatPrice = (val: number): string => val.toFixed(2);
const formatRate = (val: number): string => `${(val * 100).toFixed(3)}%`;

export function USTreasurySummaryPanel({ data }: Props) {
  const securityEntries = useMemo(() => {
    return Object.entries(data.security_types).sort((a, b) => b[1] - a[1]);
  }, [data.security_types]);

  const maxCount = useMemo(() => {
    return Math.max(...securityEntries.map(([, v]) => v));
  }, [securityEntries]);

  const cardStyle: React.CSSProperties = {
    backgroundColor: FINCEPT.PANEL_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '4px',
    padding: '16px',
  };

  const labelStyle: React.CSSProperties = {
    fontSize: '9px',
    fontWeight: 700,
    color: FINCEPT.GRAY,
    textTransform: 'uppercase' as const,
    letterSpacing: '0.5px',
  };

  const valueStyle: React.CSSProperties = {
    fontSize: '24px',
    fontWeight: 700,
    color: FINCEPT.WHITE,
    marginTop: '4px',
  };

  return (
    <div style={{ padding: '20px', overflow: 'auto', height: '100%' }}>
      {/* Header */}
      <div style={{ marginBottom: '20px' }}>
        <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>
          Treasury Market Summary
        </div>
        <div style={{ fontSize: '11px', color: FINCEPT.GRAY, marginTop: '4px' }}>
          Date: <span style={{ color: '#3B82F6' }}>{data.date}</span> &bull; Total Securities: <span style={{ color: FINCEPT.WHITE }}>{data.total_securities}</span>
        </div>
      </div>

      {/* Top stats row */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px', marginBottom: '20px' }}>
        <div style={cardStyle}>
          <div style={labelStyle}>Total Securities</div>
          <div style={valueStyle}>{data.total_securities}</div>
        </div>
        <div style={cardStyle}>
          <div style={labelStyle}>Avg Rate</div>
          <div style={{ ...valueStyle, color: FINCEPT.YELLOW }}>{formatRate(data.yield_summary.avg_rate)}</div>
        </div>
        <div style={cardStyle}>
          <div style={labelStyle}>Avg Price</div>
          <div style={{ ...valueStyle, color: FINCEPT.GREEN }}>{formatPrice(data.price_summary.avg_price)}</div>
        </div>
        <div style={cardStyle}>
          <div style={labelStyle}>Price Range</div>
          <div style={{ ...valueStyle, fontSize: '18px' }}>
            <span style={{ color: FINCEPT.RED }}>{formatPrice(data.price_summary.min_price)}</span>
            <span style={{ color: FINCEPT.GRAY, fontSize: '14px' }}> - </span>
            <span style={{ color: FINCEPT.GREEN }}>{formatPrice(data.price_summary.max_price)}</span>
          </div>
        </div>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
        {/* Security Type Breakdown */}
        <div style={cardStyle}>
          <div style={{ ...labelStyle, marginBottom: '14px' }}>Security Type Breakdown</div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
            {securityEntries.map(([type, count]) => {
              const pct = (count / data.total_securities) * 100;
              const barWidth = (count / maxCount) * 100;
              const color = SECURITY_COLORS[type] || FINCEPT.GRAY;
              return (
                <div key={type}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                    <span style={{ fontSize: '10px', color: FINCEPT.WHITE }}>{type}</span>
                    <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                      {count} <span style={{ color }}>({pct.toFixed(1)}%)</span>
                    </span>
                  </div>
                  <div style={{
                    width: '100%',
                    height: '6px',
                    backgroundColor: `${FINCEPT.BORDER}`,
                    borderRadius: '3px',
                    overflow: 'hidden',
                  }}>
                    <div style={{
                      width: `${barWidth}%`,
                      height: '100%',
                      backgroundColor: color,
                      borderRadius: '3px',
                      transition: 'width 0.3s ease',
                    }} />
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {/* Yield & Price Details */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
          {/* Yield Summary */}
          <div style={cardStyle}>
            <div style={{ ...labelStyle, marginBottom: '12px' }}>Yield Summary</div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <StatRow label="Min Rate" value={formatRate(data.yield_summary.min_rate)} color={FINCEPT.RED} />
              <StatRow label="Max Rate" value={formatRate(data.yield_summary.max_rate)} color={FINCEPT.GREEN} />
              <StatRow label="Avg Rate" value={formatRate(data.yield_summary.avg_rate)} color={FINCEPT.YELLOW} />
              <StatRow label="Securities w/ Rates" value={String(data.yield_summary.total_with_rates)} color={FINCEPT.WHITE} />
            </div>
          </div>

          {/* Price Summary */}
          <div style={cardStyle}>
            <div style={{ ...labelStyle, marginBottom: '12px' }}>Price Summary</div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <StatRow label="Min Price" value={formatPrice(data.price_summary.min_price)} color={FINCEPT.RED} />
              <StatRow label="Max Price" value={formatPrice(data.price_summary.max_price)} color={FINCEPT.GREEN} />
              <StatRow label="Avg Price" value={formatPrice(data.price_summary.avg_price)} color='#3B82F6' />
              <StatRow label="Securities w/ Prices" value={String(data.price_summary.total_with_prices)} color={FINCEPT.WHITE} />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function StatRow({ label, value, color }: { label: string; value: string; color: string }) {
  return (
    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
      <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>{label}</span>
      <span style={{ fontSize: '12px', fontWeight: 600, color, fontFamily: '"IBM Plex Mono", monospace' }}>{value}</span>
    </div>
  );
}

export default USTreasurySummaryPanel;
