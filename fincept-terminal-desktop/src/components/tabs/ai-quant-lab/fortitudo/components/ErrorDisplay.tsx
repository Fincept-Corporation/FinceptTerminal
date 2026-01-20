/**
 * ErrorDisplay Component
 * Shows error messages with alert styling
 */

import React from 'react';
import { AlertCircle } from 'lucide-react';
import { FINCEPT } from '../constants';

interface ErrorDisplayProps {
  error: string;
}

export const ErrorDisplay: React.FC<ErrorDisplayProps> = ({ error }) => {
  return (
    <div
      className="flex items-start gap-3 p-3 rounded border"
      style={{ backgroundColor: '#1a0000', borderColor: FINCEPT.RED }}
    >
      <AlertCircle
        size={18}
        color={FINCEPT.RED}
        className="flex-shrink-0 mt-0.5"
      />
      <div>
        <p className="text-sm font-bold" style={{ color: FINCEPT.RED }}>
          Error
        </p>
        <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
          {error}
        </p>
      </div>
    </div>
  );
};
