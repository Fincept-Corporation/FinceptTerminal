import React from 'react';
import { DocSubsection } from './types';
import { COLORS } from './constants';

interface DocsFooterProps {
  activeSubsection: DocSubsection | undefined;
}

export function DocsFooter({ activeSubsection }: DocsFooterProps) {
  return (
    <div
      style={{
        borderTop: `2px solid ${COLORS.ORANGE}`,
        backgroundColor: COLORS.PANEL_BG,
        padding: '8px 16px',
        fontSize: '11px',
        flexShrink: 0
      }}
    >
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexWrap: 'wrap',
          gap: '12px'
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', flexWrap: 'wrap' }}>
          <span style={{ color: COLORS.ORANGE, fontWeight: 'bold' }}>FINCEPT DOCS v3.0</span>
          <span style={{ color: COLORS.GRAY }}>|</span>
          <span style={{ color: COLORS.CYAN }}>FinScript Language Reference</span>
          <span style={{ color: COLORS.GRAY }}>|</span>
          <span style={{ color: COLORS.BLUE }}>API Documentation</span>
        </div>
        <div style={{ color: COLORS.GRAY }}>
          {activeSubsection ? activeSubsection.title : 'Browse Documentation'}
        </div>
      </div>
    </div>
  );
}
