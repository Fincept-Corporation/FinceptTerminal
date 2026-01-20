/**
 * MetricCard Component
 * Displays a single metric with icon, label, and value
 */

import React from 'react';
import { FINCEPT } from '../constants';
import type { MetricCardProps } from '../types';

export function MetricCard({
  label,
  value,
  color,
  icon,
  large = false
}: MetricCardProps) {
  return (
    <div
      className={`p-3 rounded ${large ? 'col-span-3' : ''}`}
      style={{ backgroundColor: FINCEPT.DARK_BG }}
    >
      <div className="flex items-center gap-2 mb-1">
        <span style={{ color: FINCEPT.GRAY }}>{icon}</span>
        <span className="text-xs font-mono uppercase" style={{ color: FINCEPT.GRAY }}>
          {label}
        </span>
      </div>
      <span
        className={`font-bold font-mono ${large ? 'text-2xl' : 'text-lg'}`}
        style={{ color }}
      >
        {value}
      </span>
    </div>
  );
}
