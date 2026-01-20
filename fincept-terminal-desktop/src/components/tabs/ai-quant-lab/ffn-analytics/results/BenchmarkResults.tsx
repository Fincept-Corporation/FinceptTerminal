/**
 * BenchmarkResults Component
 * Displays benchmark comparison results section
 */

import React from 'react';
import {
  GitCompare,
  TrendingUp,
  TrendingDown,
  Activity,
  Target,
  LineChart,
  BarChart2,
  Calendar,
  Briefcase,
  ChevronUp,
  ChevronDown
} from 'lucide-react';
import { FINCEPT } from '../constants';
import { formatPercent, formatRatio, getValueColor } from '../utils';
import { MetricCard } from '../components/MetricCard';
import type { BenchmarkResultsProps } from '../types';

export function BenchmarkResults({
  benchmark,
  expanded,
  toggleSection
}: BenchmarkResultsProps) {
  if (!benchmark.success) return null;

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
          <GitCompare size={16} color={FINCEPT.PURPLE} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Benchmark Comparison - {benchmark.benchmark_name || 'Benchmark'}
          </span>
        </div>
        {expanded ? (
          <ChevronUp size={16} color={FINCEPT.GRAY} />
        ) : (
          <ChevronDown size={16} color={FINCEPT.GRAY} />
        )}
      </button>

      {expanded && (
        <div className="p-4 space-y-4">
          {/* Side by Side Performance Comparison */}
          <div className="grid grid-cols-2 gap-4">
            {/* Portfolio Stats */}
            <div>
              <h4 className="text-xs font-mono mb-3 flex items-center gap-2" style={{ color: FINCEPT.CYAN }}>
                <Briefcase size={14} />
                PORTFOLIO
              </h4>
              <div className="space-y-2">
                {benchmark.portfolio_stats && (
                  <>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Total Return</span>
                      <span className="text-xs font-mono font-bold" style={{ color: getValueColor(benchmark.portfolio_stats.total_return) }}>
                        {formatPercent(benchmark.portfolio_stats.total_return)}
                      </span>
                    </div>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>CAGR</span>
                      <span className="text-xs font-mono font-bold" style={{ color: getValueColor(benchmark.portfolio_stats.cagr) }}>
                        {formatPercent(benchmark.portfolio_stats.cagr)}
                      </span>
                    </div>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Volatility</span>
                      <span className="text-xs font-mono font-bold" style={{ color: FINCEPT.YELLOW }}>
                        {formatPercent(benchmark.portfolio_stats.volatility)}
                      </span>
                    </div>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Sharpe Ratio</span>
                      <span className="text-xs font-mono font-bold" style={{ color: getValueColor(benchmark.portfolio_stats.sharpe_ratio) }}>
                        {formatRatio(benchmark.portfolio_stats.sharpe_ratio)}
                      </span>
                    </div>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Max Drawdown</span>
                      <span className="text-xs font-mono font-bold" style={{ color: FINCEPT.RED }}>
                        {formatPercent(benchmark.portfolio_stats.max_drawdown)}
                      </span>
                    </div>
                  </>
                )}
              </div>
            </div>

            {/* Benchmark Stats */}
            <div>
              <h4 className="text-xs font-mono mb-3 flex items-center gap-2" style={{ color: FINCEPT.ORANGE }}>
                <Target size={14} />
                {benchmark.benchmark_name || 'BENCHMARK'}
              </h4>
              <div className="space-y-2">
                {benchmark.benchmark_stats && (
                  <>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Total Return</span>
                      <span className="text-xs font-mono font-bold" style={{ color: getValueColor(benchmark.benchmark_stats.total_return) }}>
                        {formatPercent(benchmark.benchmark_stats.total_return)}
                      </span>
                    </div>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>CAGR</span>
                      <span className="text-xs font-mono font-bold" style={{ color: getValueColor(benchmark.benchmark_stats.cagr) }}>
                        {formatPercent(benchmark.benchmark_stats.cagr)}
                      </span>
                    </div>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Volatility</span>
                      <span className="text-xs font-mono font-bold" style={{ color: FINCEPT.YELLOW }}>
                        {formatPercent(benchmark.benchmark_stats.volatility)}
                      </span>
                    </div>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Sharpe Ratio</span>
                      <span className="text-xs font-mono font-bold" style={{ color: getValueColor(benchmark.benchmark_stats.sharpe_ratio) }}>
                        {formatRatio(benchmark.benchmark_stats.sharpe_ratio)}
                      </span>
                    </div>
                    <div className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Max Drawdown</span>
                      <span className="text-xs font-mono font-bold" style={{ color: FINCEPT.RED }}>
                        {formatPercent(benchmark.benchmark_stats.max_drawdown)}
                      </span>
                    </div>
                  </>
                )}
              </div>
            </div>
          </div>

          {/* Relative Metrics */}
          {benchmark.relative_metrics && (
            <div>
              <h4 className="text-xs font-mono mb-3" style={{ color: FINCEPT.GRAY }}>
                RELATIVE METRICS
              </h4>
              <div className="grid grid-cols-4 gap-3">
                <MetricCard
                  label="Alpha"
                  value={formatPercent(benchmark.relative_metrics.alpha)}
                  color={getValueColor(benchmark.relative_metrics.alpha)}
                  icon={<TrendingUp size={14} />}
                />
                <MetricCard
                  label="Beta"
                  value={formatRatio(benchmark.relative_metrics.beta)}
                  color={FINCEPT.CYAN}
                  icon={<Activity size={14} />}
                />
                <MetricCard
                  label="Correlation"
                  value={formatRatio(benchmark.relative_metrics.correlation)}
                  color={FINCEPT.BLUE}
                  icon={<LineChart size={14} />}
                />
                <MetricCard
                  label="Tracking Error"
                  value={formatPercent(benchmark.relative_metrics.tracking_error)}
                  color={FINCEPT.YELLOW}
                  icon={<Target size={14} />}
                />
                <MetricCard
                  label="Info Ratio"
                  value={formatRatio(benchmark.relative_metrics.information_ratio)}
                  color={getValueColor(benchmark.relative_metrics.information_ratio)}
                  icon={<BarChart2 size={14} />}
                />
                <MetricCard
                  label="Up Capture"
                  value={formatPercent(benchmark.relative_metrics.up_capture)}
                  color={FINCEPT.GREEN}
                  icon={<TrendingUp size={14} />}
                />
                <MetricCard
                  label="Down Capture"
                  value={formatPercent(benchmark.relative_metrics.down_capture)}
                  color={FINCEPT.RED}
                  icon={<TrendingDown size={14} />}
                />
              </div>
            </div>
          )}

          {/* Date Range Info */}
          {benchmark.date_range && (
            <div className="flex items-center justify-between p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <div className="flex items-center gap-2">
                <Calendar size={14} color={FINCEPT.GRAY} />
                <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                  {benchmark.date_range.start} to {benchmark.date_range.end}
                </span>
              </div>
              <span className="text-xs font-mono" style={{ color: FINCEPT.MUTED }}>
                {benchmark.date_range.data_points} data points
              </span>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
