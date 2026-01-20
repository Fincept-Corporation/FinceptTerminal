/**
 * RunButton Component
 * Analysis run button
 */

import React from 'react';
import { Activity, RefreshCw } from 'lucide-react';
import { FINCEPT } from '../constants';

interface RunButtonProps {
  isLoading: boolean;
  onClick: () => void;
}

export function RunButton({ isLoading, onClick }: RunButtonProps) {
  return (
    <div className="p-2 border-t" style={{ borderColor: FINCEPT.BORDER }}>
      <button
        onClick={onClick}
        disabled={isLoading}
        className="w-full py-3 rounded font-bold text-sm uppercase flex items-center justify-center gap-2 disabled:opacity-50"
        style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
      >
        {isLoading ? (
          <>
            <RefreshCw size={16} className="animate-spin" />
            ANALYZING...
          </>
        ) : (
          <>
            <Activity size={16} />
            RUN ANALYSIS
          </>
        )}
      </button>
    </div>
  );
}
