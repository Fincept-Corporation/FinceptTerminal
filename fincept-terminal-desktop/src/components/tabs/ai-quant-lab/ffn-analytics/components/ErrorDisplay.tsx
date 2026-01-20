/**
 * ErrorDisplay Component
 * Displays error messages
 */

import React from 'react';
import { AlertCircle } from 'lucide-react';
import { FINCEPT } from '../constants';

interface ErrorDisplayProps {
  error: string;
}

export function ErrorDisplay({ error }: ErrorDisplayProps) {
  return (
    <div
      className="mb-4 p-4 rounded flex items-center gap-3"
      style={{ backgroundColor: FINCEPT.PANEL_BG, borderLeft: `3px solid ${FINCEPT.RED}` }}
    >
      <AlertCircle size={20} color={FINCEPT.RED} />
      <span className="text-sm font-mono" style={{ color: FINCEPT.RED }}>{error}</span>
    </div>
  );
}
