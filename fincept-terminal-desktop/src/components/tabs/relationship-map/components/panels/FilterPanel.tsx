import React from 'react';
import type { FilterState } from '../../types';
import { FINCEPT, NODE_COLORS } from '../../constants';

interface FilterPanelProps {
  filters: FilterState;
  onChange: (filters: FilterState) => void;
  nodeCounts: Record<string, number>;
}

const TOGGLES: { key: keyof FilterState; label: string; colorKey: keyof typeof NODE_COLORS }[] = [
  { key: 'showPeers', label: 'PEERS', colorKey: 'peer' },
  { key: 'showInstitutional', label: 'INSTITUTIONAL', colorKey: 'institutional' },
  { key: 'showFundFamilies', label: 'FUND FAMILIES', colorKey: 'fund_family' },
  { key: 'showInsiders', label: 'INSIDERS', colorKey: 'insider' },
  { key: 'showEvents', label: 'EVENTS', colorKey: 'event' },
  { key: 'showSupplyChain', label: 'SUPPLY CHAIN', colorKey: 'supply_chain' },
  { key: 'showSegments', label: 'SEGMENTS', colorKey: 'segment' },
  { key: 'showDebtHolders', label: 'DEBT/CREDIT', colorKey: 'debt_holder' },
  { key: 'showMetrics', label: 'METRICS', colorKey: 'metrics' },
];

const FilterPanel: React.FC<FilterPanelProps> = ({ filters, onChange, nodeCounts }) => {
  const toggle = (key: keyof FilterState) => {
    onChange({ ...filters, [key]: !filters[key] });
  };

  return (
    <div style={{ position: 'absolute', top: '48px', left: 0, width: '200px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderTop: 'none', zIndex: 90, padding: '12px' }}>
      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>LAYERS</div>

      {TOGGLES.map(({ key, label, colorKey }) => {
        const countKey = colorKey;
        return (
          <div key={key} style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px', cursor: 'pointer', padding: '4px 6px', borderRadius: '2px', backgroundColor: filters[key] ? `${NODE_COLORS[colorKey].border}10` : 'transparent', transition: 'all 0.2s' }} onClick={() => toggle(key)}>
            <div style={{ width: '10px', height: '10px', borderRadius: '2px', border: `1.5px solid ${NODE_COLORS[colorKey].border}`, backgroundColor: filters[key] ? NODE_COLORS[colorKey].border : 'transparent' }} />
            <span style={{ fontSize: '9px', fontWeight: 700, color: filters[key] ? FINCEPT.WHITE : FINCEPT.MUTED, flex: 1 }}>{label}</span>
            <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>{nodeCounts[countKey] || 0}</span>
          </div>
        );
      })}

      <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, marginTop: '8px', paddingTop: '8px' }}>
        <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>MIN OWNERSHIP</div>
        <input
          type="range"
          min={0} max={10} step={0.5}
          value={filters.minOwnership}
          onChange={(e) => onChange({ ...filters, minOwnership: parseFloat(e.target.value) })}
          style={{ width: '100%', accentColor: FINCEPT.ORANGE }}
        />
        <div style={{ fontSize: '8px', color: FINCEPT.CYAN, textAlign: 'center' }}>{filters.minOwnership.toFixed(1)}%</div>
      </div>

      <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, marginTop: '8px', paddingTop: '8px' }}>
        <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>PEER FILTER</div>
        {(['all', 'undervalued', 'overvalued'] as const).map(f => (
          <div key={f} style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px', cursor: 'pointer', padding: '2px 4px' }} onClick={() => onChange({ ...filters, peerFilter: f })}>
            <div style={{ width: '8px', height: '8px', borderRadius: '50%', border: `1.5px solid ${FINCEPT.ORANGE}`, backgroundColor: filters.peerFilter === f ? FINCEPT.ORANGE : 'transparent' }} />
            <span style={{ fontSize: '8px', color: filters.peerFilter === f ? FINCEPT.WHITE : FINCEPT.MUTED, fontWeight: 700, textTransform: 'uppercase' }}>{f}</span>
          </div>
        ))}
      </div>

      <button onClick={() => onChange({ showInstitutional: true, showInsiders: true, showPeers: true, showEvents: true, showMetrics: true, showFundFamilies: true, showSupplyChain: true, showSegments: true, showDebtHolders: true, minOwnership: 0, peerFilter: 'all', eventTypes: [] })} style={{ width: '100%', marginTop: '12px', padding: '6px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY, fontSize: '8px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', letterSpacing: '0.5px' }}>
        RESET
      </button>
    </div>
  );
};

export default FilterPanel;
