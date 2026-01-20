/**
 * LoadingState Component
 * Full-screen loading spinner
 */

import React from 'react';
import { RefreshCw } from 'lucide-react';
import { FINCEPT } from '../constants';

export const LoadingState: React.FC = () => {
  return (
    <div
      className="flex items-center justify-center h-full"
      style={{ backgroundColor: FINCEPT.DARK_BG }}
    >
      <RefreshCw size={32} color={FINCEPT.ORANGE} className="animate-spin" />
    </div>
  );
};
