import React from 'react';
import { FINCEPT, FONT_FAMILY } from './types';

// Tab Button Component
export const TabButton: React.FC<{ active: boolean; onClick: () => void; icon: React.ReactNode; label: string }> = ({ active, onClick, icon, label }) => (
  <button
    onClick={onClick}
    style={{
      padding: '6px 12px',
      backgroundColor: active ? FINCEPT.ORANGE : 'transparent',
      color: active ? FINCEPT.DARK_BG : FINCEPT.GRAY,
      border: 'none',
      fontSize: '9px',
      fontWeight: 700,
      letterSpacing: '0.5px',
      cursor: 'pointer',
      display: 'flex',
      alignItems: 'center',
      gap: '4px',
      borderRadius: '2px',
      transition: 'all 0.2s',
      fontFamily: FONT_FAMILY
    }}
    onMouseEnter={(e) => {
      if (!active) {
        e.currentTarget.style.color = FINCEPT.WHITE;
        e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
      }
    }}
    onMouseLeave={(e) => {
      if (!active) {
        e.currentTarget.style.color = FINCEPT.GRAY;
        e.currentTarget.style.backgroundColor = 'transparent';
      }
    }}
  >
    {icon}
    {label}
  </button>
);

// Stat Box Component
export const StatBox: React.FC<{ label: string; value: string; color: string }> = ({ label, value, color }) => (
  <div style={{
    padding: '8px',
    backgroundColor: FINCEPT.DARK_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    textAlign: 'center'
  }}>
    <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', letterSpacing: '0.5px' }}>
      {label}
    </div>
    <div style={{ fontSize: '11px', fontWeight: 700, color }}>
      {value}
    </div>
  </div>
);

// Stat Card Component
export const StatCard: React.FC<{ label: string; value: number; color: string }> = ({ label, value, color }) => (
  <div style={{
    backgroundColor: FINCEPT.PANEL_BG,
    border: `2px solid ${color}`,
    borderRadius: '2px',
    padding: '16px',
    textAlign: 'center'
  }}>
    <div style={{
      fontSize: '9px',
      color: FINCEPT.GRAY,
      marginBottom: '6px',
      letterSpacing: '0.5px'
    }}>
      {label}
    </div>
    <div style={{
      fontSize: '28px',
      fontWeight: 700,
      color
    }}>
      {value.toLocaleString()}
    </div>
  </div>
);
