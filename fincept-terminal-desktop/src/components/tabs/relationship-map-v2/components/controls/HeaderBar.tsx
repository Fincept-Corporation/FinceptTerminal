import React, { useState, useCallback } from 'react';
import type { LayoutMode, DataQualityScore } from '../../types';

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

const LAYOUT_OPTIONS: { value: LayoutMode; label: string }[] = [
  { value: 'layered', label: 'Layered' },
  { value: 'force', label: 'Force' },
  { value: 'circular', label: 'Circular' },
  { value: 'hierarchical', label: 'Tree' },
  { value: 'radial', label: 'Radial' },
];

const HeaderBar: React.FC<HeaderBarProps> = ({
  onSearch,
  layoutMode,
  onLayoutChange,
  onToggleFilters,
  showFilters,
  dataQuality,
  loading,
  progress,
  message,
}) => {
  const [searchInput, setSearchInput] = useState('');

  const handleSubmit = useCallback((e: React.FormEvent) => {
    e.preventDefault();
    if (searchInput.trim()) {
      onSearch(searchInput.trim().toUpperCase());
    }
  }, [searchInput, onSearch]);

  return (
    <div style={{
      position: 'absolute',
      top: 0,
      left: 0,
      right: 0,
      height: '48px',
      background: 'rgba(10, 10, 10, 0.9)',
      borderBottom: '1px solid var(--ft-color-text-muted)',
      display: 'flex',
      alignItems: 'center',
      padding: '0 12px',
      gap: '12px',
      zIndex: 60,
      backdropFilter: 'blur(4px)',
    }}>
      {/* Search */}
      <form onSubmit={handleSubmit} style={{ display: 'flex', gap: '6px' }}>
        <input
          type="text"
          value={searchInput}
          onChange={(e) => setSearchInput(e.target.value.toUpperCase())}
          placeholder="Enter Ticker (e.g., AAPL)"
          style={{
            background: 'rgba(255,255,255,0.05)',
            border: '1px solid var(--ft-color-text-muted)',
            borderRadius: '4px',
            padding: '6px 10px',
            color: 'var(--ft-color-text)',
            fontSize: '12px',
            width: '180px',
            outline: 'none',
          }}
        />
        <button
          type="submit"
          disabled={loading}
          style={{
            background: 'var(--ft-color-primary)',
            border: 'none',
            borderRadius: '4px',
            padding: '6px 12px',
            color: '#000',
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: loading ? 'not-allowed' : 'pointer',
            opacity: loading ? 0.6 : 1,
          }}
        >
          {loading ? 'Loading...' : 'Map'}
        </button>
      </form>

      {/* Loading Progress */}
      {loading && (
        <div style={{ flex: 1, display: 'flex', alignItems: 'center', gap: '8px' }}>
          <div style={{ flex: 1, height: '4px', background: 'rgba(255,255,255,0.1)', borderRadius: '2px', overflow: 'hidden' }}>
            <div style={{
              width: `${progress}%`,
              height: '100%',
              background: 'var(--ft-color-primary)',
              borderRadius: '2px',
              transition: 'width 0.3s ease',
            }} />
          </div>
          <span style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', whiteSpace: 'nowrap' }}>{message}</span>
        </div>
      )}

      {/* Data Quality */}
      {!loading && dataQuality && (
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginLeft: 'auto' }}>
          <div style={{
            width: '8px', height: '8px', borderRadius: '50%',
            background: dataQuality.overall >= 70 ? 'var(--ft-color-success)' : dataQuality.overall >= 40 ? 'var(--ft-color-warning)' : 'var(--ft-color-alert)',
          }} />
          <span style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>
            Quality: {dataQuality.overall}%
          </span>
        </div>
      )}

      {/* Layout Selector */}
      {!loading && (
        <div style={{ display: 'flex', gap: '2px', marginLeft: loading ? '0' : 'auto' }}>
          {LAYOUT_OPTIONS.map(opt => (
            <button
              key={opt.value}
              onClick={() => onLayoutChange(opt.value)}
              style={{
                fontSize: '9px',
                padding: '4px 8px',
                borderRadius: '3px',
                border: `1px solid ${layoutMode === opt.value ? 'var(--ft-color-primary)' : 'rgba(120,120,120,0.3)'}`,
                background: layoutMode === opt.value ? 'rgba(255,165,0,0.15)' : 'transparent',
                color: layoutMode === opt.value ? 'var(--ft-color-primary)' : 'var(--ft-color-text-muted)',
                cursor: 'pointer',
              }}
            >
              {opt.label}
            </button>
          ))}
        </div>
      )}

      {/* Filter Toggle */}
      <button
        onClick={onToggleFilters}
        style={{
          fontSize: '10px',
          padding: '5px 10px',
          borderRadius: '4px',
          border: `1px solid ${showFilters ? 'var(--ft-color-primary)' : 'var(--ft-color-text-muted)'}`,
          background: showFilters ? 'rgba(255,165,0,0.15)' : 'transparent',
          color: showFilters ? 'var(--ft-color-primary)' : 'var(--ft-color-text-muted)',
          cursor: 'pointer',
        }}
      >
        Filters
      </button>
    </div>
  );
};

export default HeaderBar;
