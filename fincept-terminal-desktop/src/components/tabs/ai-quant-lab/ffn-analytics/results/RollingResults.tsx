/**
 * RollingResults Component
 * Displays rolling metrics charts section
 */

import React from 'react';
import { LineChart, ChevronUp, ChevronDown } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { RollingResultsProps } from '../types';

export function RollingResults({
  rollingMetrics,
  expanded,
  toggleSection
}: RollingResultsProps) {
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
          <LineChart size={16} color={FINCEPT.PURPLE} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Rolling Metrics (63-Day)
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
          {rollingMetrics.rolling_sharpe && (
            <div className="mb-4">
              <h4 className="text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
                ROLLING SHARPE RATIO
              </h4>
              <div className="h-32 flex items-end gap-px">
                {Object.values(rollingMetrics.rolling_sharpe).slice(-50).map((value, idx) => {
                  const height = Math.min(100, Math.max(10, ((value + 2) / 4) * 100));
                  return (
                    <div
                      key={idx}
                      className="flex-1 rounded-t"
                      style={{
                        height: `${height}%`,
                        backgroundColor: value > 0 ? FINCEPT.GREEN : FINCEPT.RED
                      }}
                    />
                  );
                })}
              </div>
            </div>
          )}
          {rollingMetrics.rolling_volatility && (
            <div>
              <h4 className="text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
                ROLLING VOLATILITY
              </h4>
              <div className="h-32 flex items-end gap-px">
                {Object.values(rollingMetrics.rolling_volatility).slice(-50).map((value, idx) => {
                  const height = Math.min(100, Math.max(10, (value / 0.5) * 100));
                  return (
                    <div
                      key={idx}
                      className="flex-1 rounded-t"
                      style={{
                        height: `${height}%`,
                        backgroundColor: FINCEPT.YELLOW
                      }}
                    />
                  );
                })}
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
