/**
 * LoadingState Component
 * Displays loading spinner while checking FFN availability
 */

import React from 'react';
import { RefreshCw } from 'lucide-react';
import { FINCEPT } from '../constants';

export function LoadingState() {
  return (
    <div className="flex items-center justify-center h-full" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      <RefreshCw size={32} color={FINCEPT.ORANGE} className="animate-spin" />
    </div>
  );
}
