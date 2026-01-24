import React from 'react';
import { NODE_COLORS, SIGNAL_COLORS } from '../../constants';

interface LegendPanelProps {
  visible: boolean;
}

const LegendPanel: React.FC<LegendPanelProps> = ({ visible }) => {
  if (!visible) return null;

  return (
    <div style={{
      position: 'absolute',
      left: '12px',
      bottom: '12px',
      background: 'rgba(10, 10, 10, 0.9)',
      border: '1px solid rgba(120,120,120,0.3)',
      borderRadius: '6px',
      padding: '10px 14px',
      zIndex: 50,
      maxWidth: '220px',
    }}>
      <div style={{ fontSize: '10px', fontWeight: 'bold', color: 'var(--ft-color-primary)', marginBottom: '8px' }}>Legend</div>

      {/* Node Types */}
      <div style={{ marginBottom: '8px' }}>
        <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)', marginBottom: '4px' }}>Node Types</div>
        <LegendItem color={NODE_COLORS.company.border} label="Company (Center)" />
        <LegendItem color={NODE_COLORS.peer.border} label="Peer Company" />
        <LegendItem color={NODE_COLORS.institutional.border} label="Institutional Holder" />
        <LegendItem color={NODE_COLORS.fund_family.border} label="Fund Family" />
        <LegendItem color={NODE_COLORS.insider.border} label="Insider" />
        <LegendItem color={NODE_COLORS.event.border} label="Corporate Event" />
        <LegendItem color={NODE_COLORS.metrics.border} label="Metrics Cluster" />
      </div>

      {/* Valuation Signals */}
      <div style={{ marginBottom: '8px' }}>
        <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)', marginBottom: '4px' }}>Valuation Signals</div>
        <LegendItem color={SIGNAL_COLORS.BUY.text} label="BUY (Undervalued)" />
        <LegendItem color={SIGNAL_COLORS.HOLD.text} label="HOLD (Fair Value)" />
        <LegendItem color={SIGNAL_COLORS.SELL.text} label="SELL (Overvalued)" />
      </div>

      {/* Edge Types */}
      <div>
        <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)', marginBottom: '4px' }}>Edge Types</div>
        <LegendEdge color="var(--ft-color-success)" label="Ownership" dashed={false} />
        <LegendEdge color="var(--ft-color-info)" label="Peer Comparison" dashed={true} />
        <LegendEdge color="var(--ft-color-alert)" label="Event Link" dashed={true} />
        <LegendEdge color="var(--ft-color-text-muted)" label="Internal" dashed={false} />
      </div>
    </div>
  );
};

function LegendItem({ color, label }: { color: string; label: string }) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '3px' }}>
      <div style={{ width: '10px', height: '10px', borderRadius: '3px', border: `2px solid ${color}`, background: 'rgba(0,0,0,0.3)' }} />
      <span style={{ fontSize: '9px', color: 'var(--ft-color-text)' }}>{label}</span>
    </div>
  );
}

function LegendEdge({ color, label, dashed }: { color: string; label: string; dashed: boolean }) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '3px' }}>
      <svg width="16" height="6">
        <line x1="0" y1="3" x2="16" y2="3"
          stroke={color} strokeWidth="2"
          strokeDasharray={dashed ? '3 2' : undefined}
        />
      </svg>
      <span style={{ fontSize: '9px', color: 'var(--ft-color-text)' }}>{label}</span>
    </div>
  );
}

export default LegendPanel;
