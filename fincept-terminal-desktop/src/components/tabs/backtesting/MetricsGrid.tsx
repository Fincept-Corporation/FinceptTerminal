/**
 * MetricsGrid - Grouped expandable metrics display
 *
 * Sections: Returns, Risk, Execution, Consistency, Benchmark
 */

import React, { useState } from 'react';
import { ChevronDown, ChevronRight } from 'lucide-react';
import { F, sectionHeaderStyle, AdvancedMetrics, formatPercent, formatNumber } from './backtestingStyles';

interface MetricsGridProps {
  performance: Record<string, number>;
  statistics: Record<string, number | string>;
  advancedMetrics?: AdvancedMetrics;
  extendedStats?: Record<string, number | string>;
}

interface MetricRow {
  label: string;
  value: string;
  color?: string;
}

const MetricsGrid: React.FC<MetricsGridProps> = ({ performance, statistics, advancedMetrics, extendedStats }) => {
  const [expanded, setExpanded] = useState<Record<string, boolean>>({
    returns: true, risk: true, execution: true, consistency: false, benchmark: false, system: false,
  });

  const toggle = (key: string) => setExpanded(prev => ({ ...prev, [key]: !prev[key] }));

  const pf = performance || {};
  const st = statistics || {};
  const am = advancedMetrics || {} as Partial<AdvancedMetrics>;
  const es = extendedStats || {} as Record<string, number | string>;

  const pctColor = (v: number) => v > 0 ? F.GREEN : v < 0 ? F.RED : F.WHITE;

  const sections: { key: string; label: string; metrics: MetricRow[] }[] = [
    {
      key: 'returns',
      label: 'RETURNS',
      metrics: [
        { label: 'Total Return', value: formatPercent(pf.totalReturn || 0), color: pctColor(pf.totalReturn || 0) },
        { label: 'Ann. Return', value: formatPercent(pf.annualizedReturn || 0), color: pctColor(pf.annualizedReturn || 0) },
        { label: 'Final Capital', value: `$${formatNumber(Number(st.finalCapital) || 0, 0)}` },
        { label: 'Profit Factor', value: formatNumber(pf.profitFactor || 0) },
        { label: 'Expectancy', value: formatPercent(pf.expectancy || 0), color: pctColor(pf.expectancy || 0) },
      ],
    },
    {
      key: 'risk',
      label: 'RISK',
      metrics: [
        { label: 'Sharpe Ratio', value: formatNumber(pf.sharpeRatio || 0) },
        { label: 'Sortino Ratio', value: formatNumber(pf.sortinoRatio || 0) },
        { label: 'Calmar Ratio', value: formatNumber(pf.calmarRatio || 0) },
        { label: 'Max Drawdown', value: formatPercent(pf.maxDrawdown || 0), color: F.RED },
        { label: 'Volatility', value: formatPercent(pf.volatility || 0) },
        { label: 'VaR (95%)', value: formatPercent(Math.abs(am.var95 || 0)), color: F.RED },
        { label: 'CVaR (95%)', value: formatPercent(Math.abs(am.cvar95 || 0)), color: F.RED },
        { label: 'Ulcer Index', value: formatNumber(am.ulcerIndex || 0) },
        { label: 'Omega Ratio', value: formatNumber(am.omegaRatio || 0) },
        { label: 'Tail Ratio', value: formatNumber(am.tailRatio || 0) },
        { label: 'Skewness', value: formatNumber(am.skewness || 0) },
        { label: 'Kurtosis', value: formatNumber(am.kurtosis || 0) },
      ],
    },
    {
      key: 'execution',
      label: 'EXECUTION',
      metrics: [
        { label: 'Total Trades', value: String(pf.totalTrades || st.totalTrades || 0) },
        { label: 'Winning', value: String(pf.winningTrades || 0), color: F.GREEN },
        { label: 'Losing', value: String(pf.losingTrades || 0), color: F.RED },
        { label: 'Win Rate', value: formatPercent(pf.winRate || 0), color: pctColor((pf.winRate || 0) - 0.5) },
        { label: 'Avg Win', value: formatPercent(pf.averageWin || 0), color: F.GREEN },
        { label: 'Avg Loss', value: formatPercent(Math.abs(pf.averageLoss || 0)), color: F.RED },
        { label: 'Largest Win', value: formatPercent(pf.largestWin || 0), color: F.GREEN },
        { label: 'Largest Loss', value: formatPercent(Math.abs(pf.largestLoss || 0)), color: F.RED },
      ],
    },
    {
      key: 'consistency',
      label: 'CONSISTENCY',
      metrics: [
        { label: 'Avg Daily Return', value: formatPercent(am.avgDailyReturn || 0), color: pctColor(am.avgDailyReturn || 0) },
        { label: 'Daily Std Dev', value: formatPercent(am.dailyReturnStd || 0) },
        { label: '5th Percentile', value: formatPercent(am.dailyReturnP5 || 0), color: F.RED },
        { label: '95th Percentile', value: formatPercent(am.dailyReturnP95 || 0), color: F.GREEN },
        { label: 'Consec. Wins', value: String(st.consecutiveWins || 0) },
        { label: 'Consec. Losses', value: String(st.consecutiveLosses || 0) },
      ],
    },
    {
      key: 'benchmark',
      label: 'BENCHMARK',
      metrics: [
        { label: 'Alpha', value: formatPercent(am.benchmarkAlpha || 0), color: pctColor(am.benchmarkAlpha || 0) },
        { label: 'Beta', value: formatNumber(am.benchmarkBeta || 0) },
        { label: 'Info Ratio', value: formatNumber(am.informationRatio || 0) },
        { label: 'Tracking Error', value: formatPercent(am.trackingError || 0) },
        { label: 'R-Squared', value: formatNumber(am.rSquared || 0) },
        { label: 'Correlation', value: formatNumber(am.benchmarkCorrelation || 0) },
      ],
    },
    {
      key: 'system',
      label: 'SYSTEM QUALITY',
      metrics: [
        { label: 'SQN', value: formatNumber(Number(es.sqn) || 0), color: (Number(es.sqn) || 0) > 2 ? F.GREEN : (Number(es.sqn) || 0) > 0 ? F.WHITE : F.RED },
        { label: 'Kelly Criterion', value: formatPercent(Number(es.kellyCriterion) || 0), color: pctColor(Number(es.kellyCriterion) || 0) },
        { label: 'CAGR', value: formatPercent(Number(es.cagr) || 0), color: pctColor(Number(es.cagr) || 0) },
        { label: 'Exposure Time', value: formatPercent(Number(es.exposureTime) || 0) },
        { label: 'B&H Return', value: formatPercent(Number(es.buyAndHoldReturn) || 0), color: pctColor(Number(es.buyAndHoldReturn) || 0) },
        { label: 'Avg Drawdown', value: formatPercent(Math.abs(Number(es.avgDrawdown) || 0)), color: F.RED },
        { label: 'Max DD Duration', value: String(es.maxDrawdownDuration || '-') },
        { label: 'Avg Trade Duration', value: String(es.avgTradeDuration || '-') },
      ],
    },
  ];

  return (
    <div style={{ overflowY: 'auto', height: '100%' }}>
      {sections.map(section => (
        <div key={section.key}>
          <div
            style={{ ...sectionHeaderStyle, padding: '6px 10px' }}
            onClick={() => toggle(section.key)}
          >
            {expanded[section.key]
              ? <ChevronDown size={10} color={F.ORANGE} />
              : <ChevronRight size={10} color={F.GRAY} />}
            <span style={{ fontSize: '9px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
              {section.label}
            </span>
          </div>
          {expanded[section.key] && (
            <div style={{ padding: '4px 0' }}>
              {section.metrics.map(m => (
                <div key={m.label} style={{
                  display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                  padding: '3px 12px',
                }}>
                  <span style={{ fontSize: '9px', color: F.GRAY }}>{m.label}</span>
                  <span style={{
                    fontSize: '10px', fontWeight: 700, color: m.color || F.WHITE,
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}>
                    {m.value}
                  </span>
                </div>
              ))}
            </div>
          )}
        </div>
      ))}
    </div>
  );
};

export default MetricsGrid;
