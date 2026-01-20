/**
 * OptimizationResults Component
 * Displays portfolio optimization results section
 */

import React from 'react';
import {
  Scale,
  TrendingUp,
  TrendingDown,
  Activity,
  Target,
  ChevronUp,
  ChevronDown
} from 'lucide-react';
import { FINCEPT } from '../constants';
import { formatPercent, formatRatio, getValueColor } from '../utils';
import { MetricCard } from '../components/MetricCard';
import type { OptimizationResultsProps } from '../types';

export function OptimizationResults({
  optimization,
  expanded,
  toggleSection
}: OptimizationResultsProps) {
  if (!optimization.success) return null;

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
          <Scale size={16} color={FINCEPT.CYAN} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Portfolio Optimization - {optimization.method}
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
          {/* Optimal Weights */}
          {optimization.weights && (
            <div>
              <h4 className="text-xs font-mono mb-3" style={{ color: FINCEPT.GRAY }}>
                OPTIMAL WEIGHTS
              </h4>
              <div className="space-y-2">
                {Object.entries(optimization.weights)
                  .sort(([, a], [, b]) => b - a)
                  .map(([symbol, weight]) => (
                    <div key={symbol} className="flex items-center gap-3">
                      <span className="w-16 text-xs font-mono font-bold" style={{ color: FINCEPT.WHITE }}>
                        {symbol}
                      </span>
                      <div className="flex-1 h-6 rounded overflow-hidden" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                        <div
                          className="h-full rounded"
                          style={{
                            width: `${weight * 100}%`,
                            backgroundColor: FINCEPT.CYAN
                          }}
                        />
                      </div>
                      <span className="w-16 text-right text-xs font-mono font-bold" style={{ color: FINCEPT.CYAN }}>
                        {formatPercent(weight)}
                      </span>
                    </div>
                  ))}
              </div>
            </div>
          )}

          {/* Portfolio Stats */}
          {optimization.portfolio_stats && (
            <div>
              <h4 className="text-xs font-mono mb-3" style={{ color: FINCEPT.GRAY }}>
                OPTIMIZED PORTFOLIO METRICS
              </h4>
              <div className="grid grid-cols-3 gap-3">
                <MetricCard
                  label="Expected Return"
                  value={formatPercent(optimization.portfolio_stats.total_return)}
                  color={getValueColor(optimization.portfolio_stats.total_return)}
                  icon={<TrendingUp size={14} />}
                />
                <MetricCard
                  label="CAGR"
                  value={formatPercent(optimization.portfolio_stats.cagr)}
                  color={getValueColor(optimization.portfolio_stats.cagr)}
                  icon={<TrendingUp size={14} />}
                />
                <MetricCard
                  label="Volatility"
                  value={formatPercent(optimization.portfolio_stats.volatility)}
                  color={FINCEPT.YELLOW}
                  icon={<Activity size={14} />}
                />
                <MetricCard
                  label="Sharpe Ratio"
                  value={formatRatio(optimization.portfolio_stats.sharpe_ratio)}
                  color={getValueColor(optimization.portfolio_stats.sharpe_ratio)}
                  icon={<Target size={14} />}
                />
                <MetricCard
                  label="Sortino Ratio"
                  value={formatRatio(optimization.portfolio_stats.sortino_ratio)}
                  color={getValueColor(optimization.portfolio_stats.sortino_ratio)}
                  icon={<Target size={14} />}
                />
                <MetricCard
                  label="Max Drawdown"
                  value={formatPercent(optimization.portfolio_stats.max_drawdown)}
                  color={FINCEPT.RED}
                  icon={<TrendingDown size={14} />}
                />
              </div>
            </div>
          )}

          {/* Asset Contributions */}
          {optimization.asset_contributions && (
            <div>
              <h4 className="text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
                ASSET CONTRIBUTIONS
              </h4>
              <div className="overflow-auto">
                <table className="w-full text-xs font-mono">
                  <thead>
                    <tr>
                      <th className="p-2 text-left" style={{ color: FINCEPT.GRAY }}>Asset</th>
                      <th className="p-2 text-right" style={{ color: FINCEPT.GRAY }}>Weight</th>
                      <th className="p-2 text-right" style={{ color: FINCEPT.GRAY }}>Vol</th>
                      <th className="p-2 text-right" style={{ color: FINCEPT.GRAY }}>Return</th>
                      <th className="p-2 text-right" style={{ color: FINCEPT.GRAY }}>Risk Contrib</th>
                    </tr>
                  </thead>
                  <tbody>
                    {Object.entries(optimization.asset_contributions).map(([symbol, contrib]) => (
                      <tr key={symbol} className="border-t" style={{ borderColor: FINCEPT.BORDER }}>
                        <td className="p-2" style={{ color: FINCEPT.WHITE }}>{symbol}</td>
                        <td className="p-2 text-right" style={{ color: FINCEPT.CYAN }}>
                          {formatPercent(contrib.weight)}
                        </td>
                        <td className="p-2 text-right" style={{ color: FINCEPT.YELLOW }}>
                          {formatPercent(contrib.volatility)}
                        </td>
                        <td className="p-2 text-right" style={{ color: getValueColor(contrib.return) }}>
                          {formatPercent(contrib.return)}
                        </td>
                        <td className="p-2 text-right" style={{ color: FINCEPT.PURPLE }}>
                          {formatPercent(contrib.risk_contribution)}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
