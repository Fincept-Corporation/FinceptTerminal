/**
 * ComparisonResults Component
 * Displays asset comparison section with correlation matrix
 */

import React from 'react';
import { Layers, ChevronUp, ChevronDown } from 'lucide-react';
import { FINCEPT } from '../constants';
import { formatPercent, formatRatio, getValueColor } from '../utils';
import type { ComparisonResultsProps } from '../types';

export function ComparisonResults({
  comparison,
  expanded,
  toggleSection
}: ComparisonResultsProps) {
  if (!comparison.success) return null;

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
          <Layers size={16} color={FINCEPT.BLUE} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Asset Comparison
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
          {/* Correlation Matrix */}
          {comparison.correlation_matrix && (
            <div>
              <h4 className="text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
                CORRELATION MATRIX
              </h4>
              <div className="overflow-auto">
                <table className="w-full text-xs font-mono">
                  <thead>
                    <tr>
                      <th className="p-2" style={{ color: FINCEPT.GRAY }}></th>
                      {Object.keys(comparison.correlation_matrix).map(symbol => (
                        <th key={symbol} className="p-2" style={{ color: FINCEPT.WHITE }}>
                          {symbol}
                        </th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {Object.entries(comparison.correlation_matrix).map(([symbol, correlations]) => (
                      <tr key={symbol}>
                        <td className="p-2" style={{ color: FINCEPT.WHITE }}>{symbol}</td>
                        {Object.entries(correlations).map(([otherSymbol, corr]) => (
                          <td
                            key={otherSymbol}
                            className="p-2 text-center rounded"
                            style={{
                              backgroundColor: `rgba(0, 136, 255, ${Math.abs(corr)})`,
                              color: FINCEPT.WHITE
                            }}
                          >
                            {corr.toFixed(2)}
                          </td>
                        ))}
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          )}

          {/* Asset Stats Comparison */}
          {comparison.asset_stats && (
            <div>
              <h4 className="text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
                PERFORMANCE COMPARISON
              </h4>
              <div className="overflow-auto">
                <table className="w-full text-xs font-mono">
                  <thead>
                    <tr>
                      <th className="p-2 text-left" style={{ color: FINCEPT.GRAY }}>Asset</th>
                      <th className="p-2 text-right" style={{ color: FINCEPT.GRAY }}>Return</th>
                      <th className="p-2 text-right" style={{ color: FINCEPT.GRAY }}>Vol</th>
                      <th className="p-2 text-right" style={{ color: FINCEPT.GRAY }}>Sharpe</th>
                      <th className="p-2 text-right" style={{ color: FINCEPT.GRAY }}>MaxDD</th>
                    </tr>
                  </thead>
                  <tbody>
                    {Object.entries(comparison.asset_stats).map(([symbol, stats]) => (
                      <tr key={symbol} className="border-t" style={{ borderColor: FINCEPT.BORDER }}>
                        <td className="p-2" style={{ color: FINCEPT.WHITE }}>{symbol}</td>
                        <td className="p-2 text-right" style={{ color: getValueColor(stats.total_return) }}>
                          {formatPercent(stats.total_return)}
                        </td>
                        <td className="p-2 text-right" style={{ color: FINCEPT.YELLOW }}>
                          {formatPercent(stats.volatility)}
                        </td>
                        <td className="p-2 text-right" style={{ color: getValueColor(stats.sharpe_ratio) }}>
                          {formatRatio(stats.sharpe_ratio)}
                        </td>
                        <td className="p-2 text-right" style={{ color: FINCEPT.RED }}>
                          {formatPercent(stats.max_drawdown)}
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
