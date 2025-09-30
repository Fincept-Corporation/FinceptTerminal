import React from 'react';
import { Book, Search } from 'lucide-react';
import { COLORS } from './constants';

interface DocsHeaderProps {
  searchQuery: string;
  onSearchChange: (query: string) => void;
}

export function DocsHeader({ searchQuery, onSearchChange }: DocsHeaderProps) {
  return (
    <>
      {/* Header */}
      <div
        style={{
          backgroundColor: COLORS.PANEL_BG,
          borderBottom: `2px solid ${COLORS.GRAY}`,
          padding: '8px 16px',
          flexShrink: 0
        }}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            flexWrap: 'wrap',
            gap: '12px'
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flexWrap: 'wrap' }}>
            <Book size={20} style={{ color: COLORS.ORANGE }} />
            <span style={{ color: COLORS.ORANGE, fontWeight: 'bold', fontSize: '16px' }}>DOCUMENTATION</span>
            <span style={{ color: COLORS.GRAY }}>|</span>
            <span style={{ color: COLORS.CYAN, fontSize: '12px' }}>Complete Guide & API Reference</span>
          </div>
        </div>
      </div>

      {/* Search Bar */}
      <div
        style={{
          backgroundColor: COLORS.PANEL_BG,
          borderBottom: `1px solid ${COLORS.GRAY}`,
          padding: '12px 16px',
          flexShrink: 0
        }}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            backgroundColor: COLORS.DARK_BG,
            border: `1px solid ${COLORS.GRAY}`,
            padding: '8px 12px'
          }}
        >
          <Search size={16} style={{ color: COLORS.GRAY, flexShrink: 0 }} />
          <input
            type="text"
            placeholder="Search documentation..."
            value={searchQuery}
            onChange={(e) => onSearchChange(e.target.value)}
            style={{
              flex: 1,
              backgroundColor: 'transparent',
              border: 'none',
              outline: 'none',
              color: COLORS.WHITE,
              fontSize: '13px',
              fontFamily: 'Consolas, monospace',
              minWidth: '150px'
            }}
          />
        </div>
      </div>
    </>
  );
}
