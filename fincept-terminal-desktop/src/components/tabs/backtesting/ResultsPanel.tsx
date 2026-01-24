/**
 * ResultsPanel - Right panel with multi-tab results view
 *
 * Tabs: SUMMARY | TRADES | ANALYSIS
 */

import React, { useState } from 'react';
import { F, panelStyle, tabStyle, ExtendedTrade, AdvancedMetrics } from './backtestingStyles';
import MetricsGrid from './MetricsGrid';
import TradeListView from './TradeListView';

type ResultTab = 'summary' | 'trades' | 'analysis';

interface ResultsPanelProps {
  performance: Record<string, number>;
  statistics: Record<string, number | string>;
  trades: ExtendedTrade[];
  advancedMetrics?: AdvancedMetrics;
  usingSyntheticData?: boolean;
  extendedStats?: Record<string, number | string>;
  tradeAnalysis?: Record<string, number | string>;
  drawdownAnalysis?: any;
}

const ResultsPanel: React.FC<ResultsPanelProps> = ({
  performance, statistics, trades, advancedMetrics, usingSyntheticData, extendedStats,
  tradeAnalysis, drawdownAnalysis,
}) => {
  const [activeTab, setActiveTab] = useState<ResultTab>('summary');

  const tabs: { key: ResultTab; label: string }[] = [
    { key: 'summary', label: 'SUMMARY' },
    { key: 'trades', label: 'TRADES' },
    { key: 'analysis', label: 'ANALYSIS' },
  ];

  return (
    <div style={{ ...panelStyle, display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Tab Bar */}
      <div style={{
        display: 'flex', borderBottom: `1px solid ${F.BORDER}`,
        backgroundColor: F.PANEL_BG, padding: '0 4px',
      }}>
        {tabs.map(tab => (
          <button
            key={tab.key}
            style={tabStyle(activeTab === tab.key)}
            onClick={() => setActiveTab(tab.key)}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Synthetic Data Warning */}
      {usingSyntheticData && (
        <div style={{
          padding: '4px 10px', backgroundColor: '#332200',
          borderBottom: `1px solid ${F.BORDER}`,
          fontSize: '9px', color: F.YELLOW,
        }}>
          SYNTHETIC DATA - Results do not reflect real markets
        </div>
      )}

      {/* Content */}
      <div style={{ flex: 1, minHeight: 0, overflow: 'hidden' }}>
        {activeTab === 'summary' && (
          <MetricsGrid
            performance={performance}
            statistics={statistics}
            advancedMetrics={advancedMetrics}
            extendedStats={extendedStats}
          />
        )}
        {activeTab === 'trades' && (
          <TradeListView trades={trades} />
        )}
        {activeTab === 'analysis' && (
          <AnalysisView
            advancedMetrics={advancedMetrics}
            statistics={statistics}
            tradeAnalysis={tradeAnalysis}
            drawdownAnalysis={drawdownAnalysis}
          />
        )}
      </div>
    </div>
  );
};

// ============================================================================
// Analysis Sub-View
// ============================================================================

const AnalysisView: React.FC<{
  advancedMetrics?: AdvancedMetrics;
  statistics: Record<string, number | string>;
  tradeAnalysis?: Record<string, number | string>;
  drawdownAnalysis?: any;
}> = ({ advancedMetrics, statistics, tradeAnalysis, drawdownAnalysis }) => {
  if (!advancedMetrics && !tradeAnalysis) {
    return (
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '11px' }}>
        Run a backtest to see analysis
      </div>
    );
  }

  const am = advancedMetrics || {} as Partial<AdvancedMetrics>;
  const ta = tradeAnalysis || {};
  const da = drawdownAnalysis || {};

  const safeNum = (v: any) => typeof v === 'number' && isFinite(v) ? v : 0;
  const fmtPct = (v: any) => `${(Math.abs(safeNum(v)) * 100).toFixed(3)}%`;
  const fmtNum = (v: any, d = 3) => safeNum(v).toFixed(d);

  type Section = { title: string; rows: { label: string; value: string; description: string }[] };
  const sections: Section[] = [
    {
      title: 'RISK METRICS',
      rows: [
        { label: 'Value at Risk (95%)', value: fmtPct(am.var95), description: 'Max daily loss with 95% confidence' },
        { label: 'Value at Risk (99%)', value: fmtPct(am.var99), description: 'Max daily loss with 99% confidence' },
        { label: 'Expected Shortfall (95%)', value: fmtPct(am.cvar95), description: 'Average loss beyond VaR' },
        { label: 'Expected Shortfall (99%)', value: fmtPct(am.cvar99), description: 'Average loss beyond 99% VaR' },
        { label: 'Omega Ratio', value: fmtNum(am.omegaRatio), description: 'Probability-weighted gains/losses' },
        { label: 'Ulcer Index', value: fmtNum(am.ulcerIndex), description: 'Drawdown severity measure' },
        { label: 'Tail Ratio', value: fmtNum(am.tailRatio), description: 'Right tail vs left tail size' },
        { label: 'Information Ratio', value: fmtNum(am.informationRatio), description: 'Active return per unit of tracking error' },
      ],
    },
    {
      title: 'RETURN DISTRIBUTION',
      rows: [
        { label: 'Skewness', value: fmtNum(am.skewness), description: 'Return distribution asymmetry' },
        { label: 'Excess Kurtosis', value: fmtNum(am.kurtosis), description: 'Fat-tailedness vs normal' },
        { label: 'Daily P5', value: fmtPct(am.dailyReturnP5), description: '5th percentile daily return' },
        { label: 'Daily P25', value: fmtPct(am.dailyReturnP25), description: '25th percentile daily return' },
        { label: 'Daily P75', value: fmtPct(am.dailyReturnP75), description: '75th percentile daily return' },
        { label: 'Daily P95', value: fmtPct(am.dailyReturnP95), description: '95th percentile daily return' },
      ],
    },
    {
      title: 'TRADE ANALYSIS',
      rows: [
        { label: 'Payoff Ratio', value: fmtNum(ta.payoffRatio || ta.payoff_ratio, 2), description: 'Avg win / avg loss' },
        { label: 'Avg Trade Duration', value: `${fmtNum(ta.avgDuration || ta.avg_duration, 1)} bars`, description: 'Mean holding period' },
        { label: 'Position Coverage', value: `${(safeNum(ta.positionCoverage || ta.position_coverage) * 100).toFixed(1)}%`, description: 'Time in market' },
        { label: 'Max Consec. Wins', value: String(ta.maxConsecWins || ta.max_consec_wins || statistics.consecutiveWins || 0), description: 'Longest winning streak' },
        { label: 'Max Consec. Losses', value: String(ta.maxConsecLosses || ta.max_consec_losses || statistics.consecutiveLosses || 0), description: 'Longest losing streak' },
        { label: 'SQN', value: fmtNum(ta.sqn, 2), description: 'System Quality Number (>2 good, >3 excellent)' },
      ],
    },
    {
      title: 'BENCHMARK',
      rows: [
        { label: 'Alpha', value: fmtPct(am.benchmarkAlpha), description: 'Excess return vs benchmark' },
        { label: 'Beta', value: fmtNum(am.benchmarkBeta), description: 'Market sensitivity' },
        { label: 'Tracking Error', value: fmtPct(am.trackingError), description: 'Active return volatility vs benchmark' },
        { label: 'R-Squared', value: fmtNum(am.rSquared, 4), description: 'Variance explained by benchmark' },
        { label: 'Correlation', value: fmtNum(am.benchmarkCorrelation), description: 'Return correlation with benchmark' },
      ],
    },
    {
      title: 'DRAWDOWN ANALYSIS',
      rows: [
        { label: 'Max Drawdown Duration', value: `${da.maxDuration || da.max_duration || '-'} bars`, description: 'Longest peak-to-recovery' },
        { label: 'Avg Drawdown', value: fmtPct(da.avgDrawdown || da.avg_drawdown), description: 'Average drawdown depth' },
        { label: 'Avg Duration', value: `${fmtNum(da.avgDuration || da.avg_duration, 0)} bars`, description: 'Average drawdown length' },
        { label: 'Drawdown Periods', value: String(da.numPeriods || da.num_periods || '-'), description: 'Total number of drawdown periods' },
        { label: 'Recovery Factor', value: fmtNum(da.recoveryFactor || da.recovery_factor, 2), description: 'Total return / max drawdown' },
      ],
    },
  ];

  return (
    <div style={{ overflowY: 'auto', height: '100%', padding: '8px' }}>
      {sections.map(section => (
        <div key={section.title} style={{ marginBottom: '12px' }}>
          <div style={{ fontSize: '9px', color: F.GRAY, fontWeight: 700, textTransform: 'uppercase', marginBottom: '6px', letterSpacing: '0.5px' }}>
            {section.title}
          </div>
          {section.rows.map(row => (
            <div key={row.label} style={{
              padding: '5px 8px', marginBottom: '3px',
              backgroundColor: F.HEADER_BG, borderRadius: '2px',
              border: `1px solid ${F.BORDER}`,
            }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: '9px', color: F.WHITE }}>{row.label}</span>
                <span style={{ fontSize: '10px', fontWeight: 700, color: F.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>
                  {row.value}
                </span>
              </div>
              <div style={{ fontSize: '7px', color: F.MUTED, marginTop: '1px' }}>
                {row.description}
              </div>
            </div>
          ))}
        </div>
      ))}
    </div>
  );
};

export default ResultsPanel;
