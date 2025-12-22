/**
 * Backtesting Panel
 * Run strategy backtests with Qlib
 */

import React from 'react';
import { BarChart3 } from 'lucide-react';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

export function BacktestingPanel() {
  return (
    <div className="flex items-center justify-center h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
      <div className="text-center max-w-md">
        <BarChart3 size={48} color={BLOOMBERG.GRAY} className="mx-auto mb-4" />
        <h3 className="text-lg font-semibold mb-2 uppercase" style={{ color: BLOOMBERG.WHITE }}>
          Backtesting Panel
        </h3>
        <p className="text-sm" style={{ color: BLOOMBERG.GRAY }}>
          Strategy backtesting interface - Coming soon
        </p>
      </div>
    </div>
  );
}
