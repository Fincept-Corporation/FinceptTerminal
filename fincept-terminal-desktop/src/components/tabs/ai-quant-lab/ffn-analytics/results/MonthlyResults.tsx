/**
 * MonthlyResults Component
 * Displays monthly returns heatmap section
 */

import React from 'react';
import { Calendar, ChevronUp, ChevronDown } from 'lucide-react';
import { FINCEPT, MONTH_NAMES } from '../constants';
import { getHeatmapColor } from '../utils';
import type { MonthlyResultsProps } from '../types';

export function MonthlyResults({
  monthlyReturns,
  expanded,
  toggleSection
}: MonthlyResultsProps) {
  return (
    <div
      className="rounded overflow-hidden"
      style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}
    >
      <button
        onClick={toggleSection}
        className="w-full px-4 py-3 flex items-center justify-between"
        style={{ backgroundColor: FINCEPT.HEADER_BG }}
      >
        <div className="flex items-center gap-2">
          <Calendar size={16} color={FINCEPT.CYAN} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Monthly Returns Heatmap
          </span>
        </div>
        {expanded ? (
          <ChevronUp size={16} color={FINCEPT.GRAY} />
        ) : (
          <ChevronDown size={16} color={FINCEPT.GRAY} />
        )}
      </button>

      {expanded && (
        <div className="p-4 overflow-auto">
          <div className="min-w-[600px]">
            {/* Month headers */}
            <div className="flex">
              <div className="w-16 text-xs font-mono text-center" style={{ color: FINCEPT.GRAY }}>
                Year
              </div>
              {MONTH_NAMES.map(m => (
                <div key={m} className="flex-1 text-xs font-mono text-center" style={{ color: FINCEPT.GRAY }}>
                  {m}
                </div>
              ))}
            </div>
            {/* Year rows */}
            {Object.entries(monthlyReturns).sort().map(([year, months]) => (
              <div key={year} className="flex mt-1">
                <div className="w-16 text-xs font-mono text-center py-2" style={{ color: FINCEPT.WHITE }}>
                  {year}
                </div>
                {[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].map(m => {
                  const value = months[String(m)] ?? null;
                  return (
                    <div
                      key={m}
                      className="flex-1 text-xs font-mono text-center py-2 mx-0.5 rounded"
                      style={{
                        backgroundColor: getHeatmapColor(value),
                        color: FINCEPT.WHITE
                      }}
                      title={value !== null ? `${(value * 100).toFixed(2)}%` : 'N/A'}
                    >
                      {value !== null ? `${(value * 100).toFixed(1)}` : '-'}
                    </div>
                  );
                })}
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
