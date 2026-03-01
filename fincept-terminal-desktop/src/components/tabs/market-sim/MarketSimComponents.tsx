/**
 * Market Simulation Tab - Reusable Sub-Components
 */
import React from 'react';
import { F, FONT, formatPrice, formatQty } from './MarketSimConstants';
import type { PriceLevel } from './MarketSimTypes';

export const SectionHeader: React.FC<{ title: string; icon?: React.ReactNode; right?: React.ReactNode }> = ({ title, icon, right }) => (
  <div style={{
    padding: '8px 12px',
    backgroundColor: F.HEADER_BG,
    borderBottom: `1px solid ${F.BORDER}`,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'space-between',
  }}>
    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
      {icon}
      <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
        {title}
      </span>
    </div>
    {right}
  </div>
);

export const StatBox: React.FC<{ label: string; value: string; color?: string; small?: boolean }> = ({ label, value, color = F.CYAN, small }) => (
  <div style={{ padding: small ? '4px 8px' : '6px 10px' }}>
    <div style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '2px' }}>
      {label}
    </div>
    <div style={{ fontSize: small ? '10px' : '12px', fontWeight: 700, color, fontFamily: FONT }}>
      {value}
    </div>
  </div>
);

export const OrderBookRow: React.FC<{
  level: PriceLevel;
  side: 'bid' | 'ask';
  maxQty: number;
}> = ({ level, side, maxQty }) => {
  const pct = maxQty > 0 ? (level.quantity / maxQty) * 100 : 0;
  const color = side === 'bid' ? F.GREEN : F.RED;
  return (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      padding: '2px 8px',
      position: 'relative',
      fontSize: '10px',
      fontFamily: FONT,
    }}>
      <div style={{
        position: 'absolute',
        [side === 'bid' ? 'right' : 'left']: 0,
        top: 0,
        bottom: 0,
        width: `${pct}%`,
        backgroundColor: `${color}10`,
        transition: 'width 0.2s',
      }} />
      <span style={{ flex: 1, color, textAlign: side === 'bid' ? 'left' : 'right', zIndex: 1 }}>
        {formatPrice(level.price)}
      </span>
      <span style={{ width: '70px', textAlign: 'right', color: F.WHITE, zIndex: 1 }}>
        {formatQty(level.quantity)}
      </span>
      <span style={{ width: '30px', textAlign: 'right', color: F.MUTED, fontSize: '8px', zIndex: 1 }}>
        {level.order_count}
      </span>
    </div>
  );
};
