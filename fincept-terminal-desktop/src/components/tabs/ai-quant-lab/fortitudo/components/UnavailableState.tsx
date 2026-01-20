/**
 * UnavailableState Component
 * Shown when fortitudo.tech library is not installed
 */

import React from 'react';
import { AlertCircle, RefreshCw } from 'lucide-react';
import { FINCEPT } from '../constants';

interface UnavailableStateProps {
  onRetry: () => void;
}

export const UnavailableState: React.FC<UnavailableStateProps> = ({ onRetry }) => {
  return (
    <div
      className="flex items-center justify-center h-full p-8"
      style={{ backgroundColor: FINCEPT.DARK_BG }}
    >
      <div className="max-w-lg text-center">
        <AlertCircle size={64} color={FINCEPT.RED} className="mx-auto mb-6" />
        <h2
          className="text-2xl font-bold uppercase tracking-wide mb-4"
          style={{ color: FINCEPT.WHITE }}
        >
          FORTITUDO.TECH NOT INSTALLED
        </h2>
        <p className="text-sm font-mono mb-6" style={{ color: FINCEPT.GRAY }}>
          Install the library to enable portfolio risk analytics:
        </p>
        <code
          className="block p-4 rounded text-sm font-mono mb-6"
          style={{ backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.ORANGE }}
        >
          pip install fortitudo.tech cvxopt
        </code>
        <button
          onClick={onRetry}
          className="px-6 py-2 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors inline-flex items-center gap-2"
          style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
        >
          <RefreshCw size={16} />
          RETRY
        </button>
      </div>
    </div>
  );
};
