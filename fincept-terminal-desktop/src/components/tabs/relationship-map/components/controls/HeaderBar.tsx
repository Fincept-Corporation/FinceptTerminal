import React, { useState } from 'react';
import type { LayoutMode, DataQualityScore } from '../../types';
import { FINCEPT } from '../../constants';

interface HeaderBarProps {
  onSearch: (ticker: string) => void;
  layoutMode: LayoutMode;
  onLayoutChange: (mode: LayoutMode) => void;
  onToggleFilters: () => void;
  showFilters: boolean;
  dataQuality?: DataQualityScore;
  loading: boolean;
  progress: number;
  message: string;
}

const LAYOUTS: { mode: LayoutMode; label: string }[] = [
  { mode: 'layered', label: 'RING' },
  { mode: 'force', label: 'FORCE' },
  { mode: 'circular', label: 'CIRCLE' },
  { mode: 'hierarchical', label: 'TREE' },
  { mode: 'radial', label: 'RADIAL' },
];

const HeaderBar: React.FC<HeaderBarProps> = ({
  onSearch, layoutMode, onLayoutChange, onToggleFilters, showFilters,
  dataQuality, loading, progress, message,
}) => {
  const [input, setInput] = useState('');

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (input.trim()) {
      onSearch(input.trim().toUpperCase());
      setInput('');
    }
  };

  return (
    <div style={{ position: 'absolute', top: 0, left: 0, right: 0, height: '48px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `2px solid ${FINCEPT.ORANGE}`, padding: '0 16px', display: 'flex', alignItems: 'center', gap: '12px', zIndex: 100, boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20` }}>
      <form onSubmit={handleSubmit} style={{ display: 'flex', gap: '6px' }}>
        <input
          value={input}
          onChange={(e) => setInput(e.target.value.toUpperCase())}
          placeholder="TICKER"
          style={{ width: '80px', padding: '6px 8px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', fontWeight: 700 }}
        />
        <button type="submit" disabled={loading} style={{ padding: '6px 12px', backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: loading ? 'not-allowed' : 'pointer', opacity: loading ? 0.5 : 1 }}>
          MAP
        </button>
      </form>

      {loading && (
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flex: 1 }}>
          <div style={{ flex: 1, maxWidth: '200px', height: '4px', backgroundColor: FINCEPT.BORDER, borderRadius: '2px', overflow: 'hidden' }}>
            <div style={{ width: `${progress}%`, height: '100%', backgroundColor: FINCEPT.ORANGE, transition: 'width 0.3s' }} />
          </div>
          <span style={{ fontSize: '8px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>{message}</span>
        </div>
      )}

      {!loading && <div style={{ flex: 1 }} />}

      {dataQuality && (
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <div style={{ width: '8px', height: '8px', borderRadius: '50%', backgroundColor: dataQuality.overall >= 70 ? FINCEPT.GREEN : dataQuality.overall >= 40 ? FINCEPT.YELLOW : FINCEPT.RED }} />
          <span style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700 }}>{dataQuality.overall}%</span>
        </div>
      )}

      <div style={{ display: 'flex', gap: '2px' }}>
        {LAYOUTS.map(({ mode, label }) => (
          <button key={mode} onClick={() => onLayoutChange(mode)} style={{ padding: '4px 8px', backgroundColor: layoutMode === mode ? FINCEPT.ORANGE : 'transparent', color: layoutMode === mode ? FINCEPT.DARK_BG : FINCEPT.GRAY, border: 'none', fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px', cursor: 'pointer', borderRadius: '2px', transition: 'all 0.2s' }}>
            {label}
          </button>
        ))}
      </div>

      <button onClick={onToggleFilters} style={{ padding: '4px 10px', backgroundColor: showFilters ? `${FINCEPT.ORANGE}20` : 'transparent', color: showFilters ? FINCEPT.ORANGE : FINCEPT.GRAY, border: `1px solid ${showFilters ? FINCEPT.ORANGE : FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '8px', fontWeight: 700, cursor: 'pointer', letterSpacing: '0.5px' }}>
        FILTER
      </button>
    </div>
  );
};

export default HeaderBar;
