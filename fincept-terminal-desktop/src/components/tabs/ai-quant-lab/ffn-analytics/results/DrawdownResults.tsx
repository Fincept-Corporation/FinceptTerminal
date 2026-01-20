/**
 * DrawdownResults Component
 * Displays drawdown analysis section
 */

import React from 'react';
import { TrendingDown, ChevronUp, ChevronDown } from 'lucide-react';
import { FINCEPT } from '../constants';
import { formatPercent } from '../utils';
import { MetricCard } from '../components/MetricCard';
import type { DrawdownResultsProps } from '../types';

export function DrawdownResults({
  drawdowns,
  expanded,
  toggleSection
}: DrawdownResultsProps) {
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
          <TrendingDown size={16} color={FINCEPT.RED} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Drawdown Analysis
          </span>
        </div>
        {expanded ? (
          <ChevronUp size={16} color={FINCEPT.GRAY} />
        ) : (
          <ChevronDown size={16} color={FINCEPT.GRAY} />
        )}
      </button>

      {expanded && (
        <div className="p-4">
          <div className="mb-4">
            <MetricCard
              label="Max Drawdown"
              value={formatPercent(drawdowns.max_drawdown)}
              color={FINCEPT.RED}
              icon={<TrendingDown size={14} />}
              large
            />
          </div>

          {drawdowns.top_drawdowns && drawdowns.top_drawdowns.length > 0 && (
            <div>
              <h4 className="text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
                TOP DRAWDOWN PERIODS
              </h4>
              <div className="space-y-2">
                {drawdowns.top_drawdowns.map((dd, idx) => (
                  <div
                    key={idx}
                    className="p-3 rounded flex items-center justify-between"
                    style={{ backgroundColor: FINCEPT.DARK_BG }}
                  >
                    <div className="flex items-center gap-3">
                      <span
                        className="w-6 h-6 rounded-full flex items-center justify-center text-xs font-bold"
                        style={{ backgroundColor: FINCEPT.RED, color: FINCEPT.WHITE }}
                      >
                        {idx + 1}
                      </span>
                      <div>
                        <span className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                          {dd.start} → {dd.end}
                        </span>
                      </div>
                    </div>
                    <span className="text-sm font-bold" style={{ color: FINCEPT.RED }}>
                      {formatPercent(dd.drawdown)}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
