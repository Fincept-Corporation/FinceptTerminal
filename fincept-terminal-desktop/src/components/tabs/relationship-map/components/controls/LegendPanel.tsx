import React from 'react';
import { FINCEPT, NODE_COLORS } from '../../constants';

interface LegendPanelProps {
  visible: boolean;
}

const ITEMS = [
  { label: 'COMPANY', color: NODE_COLORS.company.border },
  { label: 'PEERS', color: NODE_COLORS.peer.border },
  { label: 'INSTITUTIONAL', color: NODE_COLORS.institutional.border },
  { label: 'FUND FAMILY', color: NODE_COLORS.fund_family.border },
  { label: 'INSIDERS', color: NODE_COLORS.insider.border },
  { label: 'EVENTS', color: NODE_COLORS.event.border },
  { label: 'METRICS', color: NODE_COLORS.metrics.border },
];

const LegendPanel: React.FC<LegendPanelProps> = ({ visible }) => {
  if (!visible) return null;
  return (
    <div style={{ position: 'absolute', bottom: '40px', left: '12px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', padding: '8px 12px', zIndex: 50 }}>
      <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>LEGEND</div>
      {ITEMS.map(item => (
        <div key={item.label} style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '3px' }}>
          <div style={{ width: '8px', height: '8px', borderRadius: '2px', backgroundColor: item.color }} />
          <span style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700 }}>{item.label}</span>
        </div>
      ))}
      <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, marginTop: '6px', paddingTop: '6px' }}>
        {[{ c: FINCEPT.GREEN, l: 'BUY' }, { c: FINCEPT.YELLOW, l: 'HOLD' }, { c: FINCEPT.RED, l: 'SELL' }].map(s => (
          <div key={s.l} style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '2px' }}>
            <div style={{ width: '8px', height: '4px', backgroundColor: s.c, borderRadius: '1px' }} />
            <span style={{ fontSize: '7px', color: s.c, fontWeight: 700 }}>{s.l}</span>
          </div>
        ))}
      </div>
    </div>
  );
};

export default LegendPanel;
