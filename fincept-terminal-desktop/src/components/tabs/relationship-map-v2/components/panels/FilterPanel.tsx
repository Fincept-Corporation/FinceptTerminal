import React from 'react';
import type { FilterState } from '../../types';

interface FilterPanelProps {
  filters: FilterState;
  onChange: (filters: FilterState) => void;
  nodeCounts: Record<string, number>;
}

const FilterPanel: React.FC<FilterPanelProps> = ({ filters, onChange, nodeCounts }) => {
  const toggleFilter = (key: keyof FilterState) => {
    onChange({ ...filters, [key]: !filters[key] });
  };

  const resetFilters = () => {
    onChange({
      showInstitutional: true,
      showInsiders: true,
      showPeers: true,
      showEvents: true,
      showMetrics: true,
      showFundFamilies: true,
      minOwnership: 0,
      peerFilter: 'all',
      eventTypes: [],
    });
  };

  return (
    <div style={{
      position: 'absolute',
      left: '12px',
      top: '60px',
      background: 'rgba(10, 10, 10, 0.92)',
      border: '1px solid var(--ft-color-text-muted)',
      borderRadius: '6px',
      padding: '12px',
      width: '200px',
      zIndex: 50,
      backdropFilter: 'blur(4px)',
    }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '10px' }}>
        <span style={{ fontSize: '11px', fontWeight: 'bold', color: 'var(--ft-color-primary)' }}>Filters</span>
        <button
          onClick={resetFilters}
          style={{ fontSize: '9px', color: 'var(--ft-color-info)', background: 'none', border: 'none', cursor: 'pointer', textDecoration: 'underline' }}
        >
          Reset
        </button>
      </div>

      {/* Relationship Type Toggles */}
      <div style={{ marginBottom: '10px' }}>
        <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginBottom: '6px' }}>Relationship Types</div>
        <FilterCheckbox
          label={`Peers (${nodeCounts.peer || 0})`}
          checked={filters.showPeers}
          color="var(--ft-color-info)"
          onChange={() => toggleFilter('showPeers')}
        />
        <FilterCheckbox
          label={`Institutional (${nodeCounts.institutional || 0})`}
          checked={filters.showInstitutional}
          color="var(--ft-color-success)"
          onChange={() => toggleFilter('showInstitutional')}
        />
        <FilterCheckbox
          label={`Fund Families (${nodeCounts.fund_family || 0})`}
          checked={filters.showFundFamilies}
          color="var(--ft-color-purple)"
          onChange={() => toggleFilter('showFundFamilies')}
        />
        <FilterCheckbox
          label={`Insiders (${nodeCounts.insider || 0})`}
          checked={filters.showInsiders}
          color="var(--ft-color-accent)"
          onChange={() => toggleFilter('showInsiders')}
        />
        <FilterCheckbox
          label={`Events (${nodeCounts.event || 0})`}
          checked={filters.showEvents}
          color="var(--ft-color-alert)"
          onChange={() => toggleFilter('showEvents')}
        />
        <FilterCheckbox
          label={`Metrics (${nodeCounts.metrics || 0})`}
          checked={filters.showMetrics}
          color="var(--ft-color-text-muted)"
          onChange={() => toggleFilter('showMetrics')}
        />
      </div>

      {/* Min Ownership Slider */}
      <div style={{ marginBottom: '10px' }}>
        <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginBottom: '4px' }}>
          Min Ownership: {filters.minOwnership.toFixed(1)}%
        </div>
        <input
          type="range"
          min="0"
          max="10"
          step="0.5"
          value={filters.minOwnership}
          onChange={(e) => onChange({ ...filters, minOwnership: parseFloat(e.target.value) })}
          style={{ width: '100%', accentColor: 'var(--ft-color-primary)' }}
        />
      </div>

      {/* Peer Valuation Filter */}
      <div>
        <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginBottom: '4px' }}>Peer Valuation</div>
        <div style={{ display: 'flex', gap: '4px' }}>
          {(['all', 'undervalued', 'overvalued'] as const).map(opt => (
            <button
              key={opt}
              onClick={() => onChange({ ...filters, peerFilter: opt })}
              style={{
                fontSize: '8px',
                padding: '3px 6px',
                borderRadius: '3px',
                border: `1px solid ${filters.peerFilter === opt ? 'var(--ft-color-primary)' : 'var(--ft-color-text-muted)'}`,
                background: filters.peerFilter === opt ? 'rgba(255,165,0,0.2)' : 'transparent',
                color: filters.peerFilter === opt ? 'var(--ft-color-primary)' : 'var(--ft-color-text-muted)',
                cursor: 'pointer',
                textTransform: 'capitalize',
              }}
            >
              {opt}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
};

function FilterCheckbox({ label, checked, color, onChange }: {
  label: string;
  checked: boolean;
  color: string;
  onChange: () => void;
}) {
  return (
    <label style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px', cursor: 'pointer', fontSize: '10px', color: 'var(--ft-color-text)' }}>
      <input
        type="checkbox"
        checked={checked}
        onChange={onChange}
        style={{ accentColor: color, width: '12px', height: '12px' }}
      />
      <span style={{ display: 'inline-block', width: '8px', height: '8px', borderRadius: '2px', background: color, marginRight: '2px' }} />
      {label}
    </label>
  );
}

export default FilterPanel;
