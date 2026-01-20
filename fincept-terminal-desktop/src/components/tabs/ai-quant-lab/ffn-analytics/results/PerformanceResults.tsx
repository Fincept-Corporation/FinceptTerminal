/**
 * PerformanceResults Component
 * Displays performance metrics section
 */

import React from 'react';
import {
  TrendingUp,
  TrendingDown,
  Activity,
  BarChart2,
  LineChart,
  Percent,
  Target,
  ChevronUp,
  ChevronDown
} from 'lucide-react';
import { FINCEPT } from '../constants';
import { formatPercent, formatRatio, getValueColor } from '../utils';
import { MetricCard } from '../components/MetricCard';
import type { PerformanceResultsProps } from '../types';

export function PerformanceResults({
  performance,
  expanded,
  toggleSection
}: PerformanceResultsProps) {
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
          <TrendingUp size={16} color={FINCEPT.GREEN} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Performance Metrics
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
            label="Total Return"
            value={formatPercent(performance.total_return)}
            color={getValueColor(performance.total_return)}
            icon={<Percent size={14} />}
          />
          <MetricCard
            label="CAGR"
            value={formatPercent(performance.cagr)}
            color={getValueColor(performance.cagr)}
            icon={<TrendingUp size={14} />}
          />
          <MetricCard
            label="Sharpe Ratio"
            value={formatRatio(performance.sharpe_ratio)}
            color={getValueColor(performance.sharpe_ratio)}
            icon={<Target size={14} />}
          />
          <MetricCard
            label="Sortino Ratio"
            value={formatRatio(performance.sortino_ratio)}
            color={getValueColor(performance.sortino_ratio)}
            icon={<Target size={14} />}
          />
          <MetricCard
            label="Volatility"
            value={formatPercent(performance.volatility)}
            color={FINCEPT.YELLOW}
            icon={<Activity size={14} />}
          />
          <MetricCard
            label="Calmar Ratio"
            value={formatRatio(performance.calmar_ratio)}
            color={getValueColor(performance.calmar_ratio)}
            icon={<BarChart2 size={14} />}
          />
          <MetricCard
            label="Best Day"
            value={formatPercent(performance.best_day)}
            color={FINCEPT.GREEN}
            icon={<TrendingUp size={14} />}
          />
          <MetricCard
            label="Worst Day"
            value={formatPercent(performance.worst_day)}
            color={FINCEPT.RED}
            icon={<TrendingDown size={14} />}
          />
          <MetricCard
            label="Daily Mean"
            value={formatPercent(performance.daily_mean, 4)}
            color={getValueColor(performance.daily_mean)}
            icon={<LineChart size={14} />}
          />
        </div>
      )}
    </div>
  );
}
