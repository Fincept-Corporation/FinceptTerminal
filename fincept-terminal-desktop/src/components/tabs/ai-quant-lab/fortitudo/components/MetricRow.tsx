/**
 * MetricRow Component
 * Displays a labeled metric with optional positive/negative coloring
 */

import React from 'react';
import { FINCEPT } from '../constants';
import type { MetricRowProps } from '../types';

export const MetricRow: React.FC<MetricRowProps> = ({
  label,
  value,
  positive,
  negative
}) => {
  let valueColor = FINCEPT.WHITE;
  if (positive) valueColor = FINCEPT.GREEN;
  if (negative) valueColor = FINCEPT.RED;

  return (
    <div
      className="flex items-center justify-between py-1.5 border-b"
      style={{ borderColor: FINCEPT.BORDER }}
    >
      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
        {label}
      </span>
      <span
        className="text-sm font-mono font-semibold"
        style={{ color: valueColor }}
      >
        {value}
      </span>
    </div>
  );
};
