/**
 * GreekRow Component
 * Displays option Greeks with label and description
 */

import React from 'react';
import { FINCEPT } from '../constants';
import { formatGreek, getGreekValueColor } from '../utils';
import type { GreekRowProps } from '../types';

export const GreekRow: React.FC<GreekRowProps> = ({
  label,
  value,
  description
}) => {
  return (
    <div
      className="flex items-center justify-between py-1.5 border-b"
      style={{ borderColor: FINCEPT.BORDER }}
    >
      <div>
        <span
          className="text-xs font-mono font-semibold"
          style={{ color: FINCEPT.WHITE }}
        >
          {label}
        </span>
        <span
          className="text-xs font-mono ml-2"
          style={{ color: FINCEPT.MUTED }}
        >
          {description}
        </span>
      </div>
      <span
        className="text-sm font-mono font-bold"
        style={{ color: getGreekValueColor(value) }}
      >
        {formatGreek(value)}
      </span>
    </div>
  );
};
