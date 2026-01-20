/**
 * RiskResults Component
 * Displays risk metrics section
 */

import React from 'react';
import {
  AlertCircle,
  Activity,
  BarChart2,
  TrendingDown,
  ChevronUp,
  ChevronDown
} from 'lucide-react';
import { FINCEPT } from '../constants';
import { formatPercent, formatRatio } from '../utils';
import { MetricCard } from '../components/MetricCard';
import type { RiskResultsProps } from '../types';

export function RiskResults({
  riskMetrics,
  expanded,
  toggleSection
}: RiskResultsProps) {
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
          <AlertCircle size={16} color={FINCEPT.YELLOW} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Risk Metrics
          </span>
        </div>
        {expanded ? (
          <ChevronUp size={16} color={FINCEPT.GRAY} />
        ) : (
          <ChevronDown size={16} color={FINCEPT.GRAY} />
        )}
      </button>

      {expanded && (
        <div className="p-4 grid grid-cols-3 gap-4">
          <MetricCard
            label="Ulcer Index"
            value={formatRatio(riskMetrics.ulcer_index, 4)}
            color={FINCEPT.YELLOW}
            icon={<Activity size={14} />}
          />
          <MetricCard
            label="Skewness"
            value={formatRatio(riskMetrics.skewness)}
            color={FINCEPT.CYAN}
            icon={<BarChart2 size={14} />}
          />
          <MetricCard
            label="Kurtosis"
            value={formatRatio(riskMetrics.kurtosis)}
            color={FINCEPT.CYAN}
            icon={<BarChart2 size={14} />}
          />
          <MetricCard
            label="VaR (95%)"
            value={formatPercent(riskMetrics.var_95)}
            color={FINCEPT.RED}
            icon={<TrendingDown size={14} />}
          />
          <MetricCard
            label="CVaR (95%)"
            value={formatPercent(riskMetrics.cvar_95)}
            color={FINCEPT.RED}
            icon={<TrendingDown size={14} />}
          />
        </div>
      )}
    </div>
  );
}
