/**
 * UnavailableState Component
 * Displays when FFN library is not installed
 */

import React from 'react';
import { AlertCircle, RefreshCw } from 'lucide-react';
import { FINCEPT } from '../constants';

interface UnavailableStateProps {
  onRetry: () => void;
}

export function UnavailableState({ onRetry }: UnavailableStateProps) {
  return (
    <div className="flex items-center justify-center h-full p-8" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      <div className="text-center max-w-md">
        <AlertCircle size={48} color={FINCEPT.RED} className="mx-auto mb-4" />
        <h3 className="text-lg font-bold uppercase mb-2" style={{ color: FINCEPT.WHITE }}>
          FFN Library Not Installed
        </h3>
        <code
          className="block p-4 rounded text-sm font-mono mt-4"
          style={{ backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.ORANGE }}
        >
          pip install ffn
        </code>
        <button
          onClick={onRetry}
          className="mt-6 px-6 py-2 rounded font-bold text-sm uppercase flex items-center gap-2 mx-auto"
          style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
        >
          <RefreshCw size={14} />
          Check Again
        </button>
      </div>
    </div>
  );
}
