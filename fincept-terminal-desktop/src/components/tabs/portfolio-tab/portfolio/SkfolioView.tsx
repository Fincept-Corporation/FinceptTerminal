import React, { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  LineChart, Line, Legend, RadarChart, Radar, PolarGrid, PolarAngleAxis,
  PolarRadiusAxis, PieChart, Pie, Cell, ScatterChart, Scatter
} from 'recharts';
import { Activity, Play, Loader2, Target, Zap, Shield, TrendingUp, BarChart3, Layers } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS, COMMON_STYLES } from '../finceptStyles';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  CYAN: FINCEPT.CYAN,
  BLUE: FINCEPT.BLUE,
  PURPLE: FINCEPT.PURPLE,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  HEADER_BG: FINCEPT.HEADER_BG,
  HOVER: FINCEPT.HOVER,
  BORDER: FINCEPT.BORDER,
  MUTED: FINCEPT.MUTED,
};

const PIE_COLORS = ['#FF8800', '#00E5FF', '#00D66F', '#FF3B3B', '#9D4EDD', '#FFD700', '#0088FF', '#FF6B6B'];

type SkfolioMode = 'optimize' | 'frontier' | 'compare' | 'risk' | 'backtest' | 'stress';

const OPTIMIZATION_METHODS = [
  { value: 'mean_risk', label: 'Mean-Risk' },
  { value: 'hrp', label: 'Hierarchical Risk Parity' },
  { value: 'herc', label: 'HERC' },
  { value: 'nco', label: 'Nested Clusters' },
  { value: 'risk_budgeting', label: 'Risk Budgeting' },
  { value: 'max_diversification', label: 'Max Diversification' },
  { value: 'equal_weight', label: 'Equal Weight' },
  { value: 'inverse_volatility', label: 'Inverse Volatility' },
  { value: 'distributionally_robust_cvar', label: 'Distributionally Robust CVaR' },
];

const OBJECTIVE_FUNCTIONS = [
  { value: 'minimize_risk', label: 'Minimize Risk' },
  { value: 'maximize_return', label: 'Maximize Return' },
  { value: 'maximize_ratio', label: 'Maximize Sharpe' },
  { value: 'maximize_utility', label: 'Maximize Utility' },
];

const RISK_MEASURES = [
  { value: 'variance', label: 'Variance' },
  { value: 'semi_variance', label: 'Semi-Variance' },
  { value: 'cvar', label: 'CVaR' },
  { value: 'evar', label: 'EVaR' },
  { value: 'max_drawdown', label: 'Max Drawdown' },
  { value: 'cdar', label: 'CDaR' },
  { value: 'ulcer_index', label: 'Ulcer Index' },
  { value: 'mean_absolute_deviation', label: 'MAD' },
  { value: 'worst_realization', label: 'Worst Realization' },
  { value: 'gini_mean_difference', label: 'Gini Mean Diff' },
];

interface SkfolioViewProps {
  portfolioSummary: PortfolioSummary;
}

const SkfolioView: React.FC<SkfolioViewProps> = ({ portfolioSummary }) => {
  const [mode, setMode] = useState<SkfolioMode>('optimize');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<any>(null);

  // Optimization params
  const [method, setMethod] = useState('mean_risk');
  const [objective, setObjective] = useState('maximize_ratio');
  const [riskMeasure, setRiskMeasure] = useState('variance');
  const [tickerInput, setTickerInput] = useState('AAPL,MSFT,GOOGL,AMZN,TSLA,JPM,V,JNJ');
  const [period, setPeriod] = useState('3y');

  // Backtest params
  const [rebalanceFreq, setRebalanceFreq] = useState(21);
  const [windowSize, setWindowSize] = useState(252);

  // Frontier params
  const [nPortfolios, setNPortfolios] = useState(100);

  const buildPricesData = (): string => {
    const tickers = tickerInput.split(',').map(t => t.trim()).filter(Boolean);
    return JSON.stringify({ tickers, period });
  };

  const runOptimize = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    try {
      const res = await invoke<string>('skfolio_optimize_portfolio', {
        pricesData: buildPricesData(),
        optimizationMethod: method,
        objectiveFunction: objective,
        riskMeasure: riskMeasure,
        config: JSON.stringify({}),
      });
      setResult(JSON.parse(res));
    } catch (e: any) {
      setError(typeof e === 'string' ? e : e.message || 'Optimization failed');
    } finally {
      setLoading(false);
    }
  };

  const runFrontier = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    try {
      const res = await invoke<string>('skfolio_efficient_frontier', {
        pricesData: buildPricesData(),
        nPortfolios,
        config: JSON.stringify({ risk_measure: riskMeasure }),
      });
      setResult(JSON.parse(res));
    } catch (e: any) {
      setError(typeof e === 'string' ? e : e.message || 'Frontier generation failed');
    } finally {
      setLoading(false);
    }
  };

  const runCompare = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    try {
      const res = await invoke<string>('skfolio_compare_strategies', {
        pricesData: buildPricesData(),
        strategies: JSON.stringify(['mean_risk', 'hrp', 'herc', 'max_diversification', 'equal_weight', 'inverse_volatility', 'risk_budgeting']),
        metric: 'sharpe_ratio',
      });
      setResult(JSON.parse(res));
    } catch (e: any) {
      setError(typeof e === 'string' ? e : e.message || 'Comparison failed');
    } finally {
      setLoading(false);
    }
  };

  const runRisk = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    try {
      const res = await invoke<string>('skfolio_risk_metrics', {
        pricesData: buildPricesData(),
      });
      setResult(JSON.parse(res));
    } catch (e: any) {
      setError(typeof e === 'string' ? e : e.message || 'Risk metrics failed');
    } finally {
      setLoading(false);
    }
  };

  const runBacktest = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    try {
      const res = await invoke<string>('skfolio_backtest_strategy', {
        pricesData: buildPricesData(),
        rebalanceFreq,
        windowSize,
        config: JSON.stringify({ optimization_method: method, risk_measure: riskMeasure }),
      });
      setResult(JSON.parse(res));
    } catch (e: any) {
      setError(typeof e === 'string' ? e : e.message || 'Backtest failed');
    } finally {
      setLoading(false);
    }
  };

  const runStress = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    try {
      // First optimize to get weights
      const optRes = await invoke<string>('skfolio_optimize_portfolio', {
        pricesData: buildPricesData(),
        optimizationMethod: method,
        objectiveFunction: objective,
        riskMeasure: riskMeasure,
      });
      const optData = JSON.parse(optRes);
      if (optData.error) throw new Error(optData.error);

      const res = await invoke<string>('skfolio_stress_test', {
        pricesData: buildPricesData(),
        weights: JSON.stringify(optData.weights),
        nSimulations: 10000,
      });
      setResult(JSON.parse(res));
    } catch (e: any) {
      setError(typeof e === 'string' ? e : e.message || 'Stress test failed');
    } finally {
      setLoading(false);
    }
  };

  const handleRun = () => {
    switch (mode) {
      case 'optimize': return runOptimize();
      case 'frontier': return runFrontier();
      case 'compare': return runCompare();
      case 'risk': return runRisk();
      case 'backtest': return runBacktest();
      case 'stress': return runStress();
    }
  };

  const modes: { id: SkfolioMode; label: string; icon: React.ElementType }[] = [
    { id: 'optimize', label: 'OPTIMIZE', icon: Target },
    { id: 'frontier', label: 'FRONTIER', icon: TrendingUp },
    { id: 'compare', label: 'COMPARE', icon: BarChart3 },
    { id: 'risk', label: 'RISK', icon: Shield },
    { id: 'backtest', label: 'BACKTEST', icon: Activity },
    { id: 'stress', label: 'STRESS TEST', icon: Zap },
  ];

  const selectStyle: React.CSSProperties = {
    ...COMMON_STYLES.inputField,
    appearance: 'none' as const,
    cursor: 'pointer',
  };

  const labelStyle: React.CSSProperties = {
    ...COMMON_STYLES.dataLabel,
    marginBottom: '4px',
    display: 'block',
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Header */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Layers size={14} color={COLORS.ORANGE} />
          <span>SKFOLIO PORTFOLIO OPTIMIZATION</span>
        </div>
        <span style={{ fontSize: '9px', color: COLORS.GRAY }}>15+ Models | 19 Risk Measures | scikit-learn Compatible</span>
      </div>

      {/* Mode Tabs */}
      <div style={{
        display: 'flex',
        gap: '2px',
        padding: '6px 12px',
        backgroundColor: COLORS.DARK_BG,
        borderBottom: BORDERS.STANDARD,
        flexShrink: 0,
      }}>
        {modes.map(m => {
          const Icon = m.icon;
          const isActive = mode === m.id;
          return (
            <button
              key={m.id}
              onClick={() => { setMode(m.id); setResult(null); setError(null); }}
              style={{
                ...COMMON_STYLES.tabButton(isActive),
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              <Icon size={9} />
              {m.label}
            </button>
          );
        })}
      </div>

      {/* Body */}
      <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
        {/* Input Panel */}
        <div style={{ ...COMMON_STYLES.panel, marginBottom: SPACING.DEFAULT }}>
          {/* Tickers */}
          <div style={{ marginBottom: SPACING.MEDIUM }}>
            <label style={labelStyle}>TICKERS (comma-separated)</label>
            <input
              value={tickerInput}
              onChange={e => setTickerInput(e.target.value)}
              style={COMMON_STYLES.inputField}
              placeholder="AAPL,MSFT,GOOGL,AMZN"
            />
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(160px, 1fr))', gap: SPACING.GAP_MEDIUM }}>
            {/* Period */}
            <div>
              <label style={labelStyle}>PERIOD</label>
              <select value={period} onChange={e => setPeriod(e.target.value)} style={selectStyle}>
                <option value="1y">1 Year</option>
                <option value="2y">2 Years</option>
                <option value="3y">3 Years</option>
                <option value="5y">5 Years</option>
                <option value="10y">10 Years</option>
              </select>
            </div>

            {/* Method */}
            {(mode === 'optimize' || mode === 'backtest') && (
              <div>
                <label style={labelStyle}>OPTIMIZATION METHOD</label>
                <select value={method} onChange={e => setMethod(e.target.value)} style={selectStyle}>
                  {OPTIMIZATION_METHODS.map(m => (
                    <option key={m.value} value={m.value}>{m.label}</option>
                  ))}
                </select>
              </div>
            )}

            {/* Objective */}
            {mode === 'optimize' && (
              <div>
                <label style={labelStyle}>OBJECTIVE</label>
                <select value={objective} onChange={e => setObjective(e.target.value)} style={selectStyle}>
                  {OBJECTIVE_FUNCTIONS.map(o => (
                    <option key={o.value} value={o.value}>{o.label}</option>
                  ))}
                </select>
              </div>
            )}

            {/* Risk Measure */}
            {(mode === 'optimize' || mode === 'frontier' || mode === 'backtest') && (
              <div>
                <label style={labelStyle}>RISK MEASURE</label>
                <select value={riskMeasure} onChange={e => setRiskMeasure(e.target.value)} style={selectStyle}>
                  {RISK_MEASURES.map(r => (
                    <option key={r.value} value={r.value}>{r.label}</option>
                  ))}
                </select>
              </div>
            )}

            {/* Frontier points */}
            {mode === 'frontier' && (
              <div>
                <label style={labelStyle}>FRONTIER POINTS</label>
                <input
                  type="number"
                  value={nPortfolios}
                  onChange={e => setNPortfolios(Number(e.target.value))}
                  style={COMMON_STYLES.inputField}
                  min={10}
                  max={500}
                />
              </div>
            )}

            {/* Backtest params */}
            {mode === 'backtest' && (
              <>
                <div>
                  <label style={labelStyle}>REBALANCE FREQ (days)</label>
                  <input
                    type="number"
                    value={rebalanceFreq}
                    onChange={e => setRebalanceFreq(Number(e.target.value))}
                    style={COMMON_STYLES.inputField}
                    min={1}
                  />
                </div>
                <div>
                  <label style={labelStyle}>LOOKBACK WINDOW</label>
                  <input
                    type="number"
                    value={windowSize}
                    onChange={e => setWindowSize(Number(e.target.value))}
                    style={COMMON_STYLES.inputField}
                    min={60}
                  />
                </div>
              </>
            )}
          </div>

          {/* Run Button */}
          <div style={{ marginTop: SPACING.DEFAULT }}>
            <button
              onClick={handleRun}
              disabled={loading}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                opacity: loading ? 0.6 : 1,
              }}
            >
              {loading ? <Loader2 size={12} className="animate-spin" /> : <Play size={12} />}
              {loading ? 'RUNNING...' : `RUN ${modes.find(m => m.id === mode)?.label}`}
            </button>
          </div>
        </div>

        {/* Error */}
        {error && (
          <div style={{
            ...COMMON_STYLES.panel,
            borderColor: COLORS.RED,
            color: COLORS.RED,
            marginBottom: SPACING.DEFAULT,
            fontSize: TYPOGRAPHY.SMALL,
          }}>
            {error}
          </div>
        )}

        {/* Results */}
        {result && !result.error && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
            {mode === 'optimize' && renderOptimizeResults(result)}
            {mode === 'frontier' && renderFrontierResults(result)}
            {mode === 'compare' && renderCompareResults(result)}
            {mode === 'risk' && renderRiskResults(result)}
            {mode === 'backtest' && renderBacktestResults(result)}
            {mode === 'stress' && renderStressResults(result)}
          </div>
        )}

        {result?.error && (
          <div style={{
            ...COMMON_STYLES.panel,
            borderColor: COLORS.RED,
            color: COLORS.RED,
            fontSize: TYPOGRAPHY.SMALL,
          }}>
            {result.error}
          </div>
        )}
      </div>
    </div>
  );
};

// ==================== RESULT RENDERERS ====================

function renderOptimizeResults(result: any) {
  const weights = result.weights || {};
  const metrics = result.metrics || {};
  const entries = Object.entries(weights)
    .map(([k, v]) => ({ name: k, weight: Number(v) }))
    .filter(e => Math.abs(e.weight) > 0.001)
    .sort((a, b) => b.weight - a.weight);

  return (
    <>
      {/* Metrics Summary */}
      <div style={COMMON_STYLES.panel}>
        <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>
          OPTIMIZATION RESULTS ({result.optimization_method?.toUpperCase()})
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(140px, 1fr))', gap: SPACING.GAP_MEDIUM }}>
          {Object.entries(metrics).map(([key, val]) => (
            <div key={key} style={COMMON_STYLES.metricCard}>
              <span style={COMMON_STYLES.dataLabel}>{key.replace(/_/g, ' ')}</span>
              <span style={{
                ...COMMON_STYLES.dataValue,
                color: key.includes('sharpe') || key.includes('sortino') || key.includes('calmar')
                  ? (Number(val) > 0 ? COLORS.GREEN : COLORS.RED)
                  : COLORS.CYAN,
              }}>
                {typeof val === 'number' ? val.toFixed(6) : String(val)}
              </span>
            </div>
          ))}
        </div>
      </div>

      {/* Weights Chart + Table */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
        {/* Pie Chart */}
        <div style={COMMON_STYLES.panel}>
          <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>ALLOCATION</div>
          <ResponsiveContainer width="100%" height={250}>
            <PieChart>
              <Pie
                data={entries}
                dataKey="weight"
                nameKey="name"
                cx="50%"
                cy="50%"
                outerRadius={90}
                label={({ name, weight }) => `${name} ${(weight * 100).toFixed(1)}%`}
                labelLine={false}
              >
                {entries.map((_, i) => (
                  <Cell key={i} fill={PIE_COLORS[i % PIE_COLORS.length]} />
                ))}
              </Pie>
              <Tooltip
                contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: '10px' }}
                formatter={(v: any) => `${(Number(v) * 100).toFixed(2)}%`}
              />
            </PieChart>
          </ResponsiveContainer>
        </div>

        {/* Weights Table */}
        <div style={COMMON_STYLES.panel}>
          <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>WEIGHTS</div>
          <div style={{ maxHeight: 250, overflow: 'auto' }}>
            <table style={{ width: '100%', borderCollapse: 'collapse' }}>
              <thead>
                <tr>
                  <th style={COMMON_STYLES.tableHeader}>ASSET</th>
                  <th style={{ ...COMMON_STYLES.tableHeader, textAlign: 'right' }}>WEIGHT</th>
                  <th style={{ ...COMMON_STYLES.tableHeader, textAlign: 'right' }}>%</th>
                </tr>
              </thead>
              <tbody>
                {entries.map((e, i) => (
                  <tr key={e.name} style={i % 2 ? COMMON_STYLES.tableRowAlt : {}}>
                    <td style={{ padding: '6px 12px', fontSize: '10px', color: COLORS.WHITE, borderBottom: BORDERS.STANDARD }}>{e.name}</td>
                    <td style={{ padding: '6px 12px', fontSize: '10px', color: COLORS.CYAN, textAlign: 'right', borderBottom: BORDERS.STANDARD }}>{e.weight.toFixed(6)}</td>
                    <td style={{ padding: '6px 12px', fontSize: '10px', color: COLORS.ORANGE, textAlign: 'right', borderBottom: BORDERS.STANDARD }}>{(e.weight * 100).toFixed(2)}%</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      </div>
    </>
  );
}

function renderFrontierResults(result: any) {
  const data = (result.returns || []).map((r: number, i: number) => ({
    volatility: result.volatility?.[i] || 0,
    return: r,
    sharpe: result.sharpe_ratios?.[i] || 0,
  }));

  return (
    <div style={COMMON_STYLES.panel}>
      <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>
        EFFICIENT FRONTIER ({result.n_portfolios} portfolios)
      </div>
      <ResponsiveContainer width="100%" height={400}>
        <ScatterChart margin={{ top: 20, right: 30, bottom: 20, left: 30 }}>
          <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
          <XAxis
            dataKey="volatility"
            type="number"
            name="Volatility"
            tick={{ fill: COLORS.GRAY, fontSize: 9 }}
            label={{ value: 'Annualized Volatility', position: 'bottom', fill: COLORS.GRAY, fontSize: 10 }}
          />
          <YAxis
            dataKey="return"
            type="number"
            name="Return"
            tick={{ fill: COLORS.GRAY, fontSize: 9 }}
            label={{ value: 'Annualized Return', angle: -90, position: 'insideLeft', fill: COLORS.GRAY, fontSize: 10 }}
          />
          <Tooltip
            contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: '10px' }}
            formatter={(v: any, name: string) => [
              `${(Number(v) * 100).toFixed(2)}%`,
              name === 'return' ? 'Return' : name === 'volatility' ? 'Volatility' : 'Sharpe'
            ]}
          />
          <Scatter name="Portfolios" data={data} fill={COLORS.CYAN} />
        </ScatterChart>
      </ResponsiveContainer>
    </div>
  );
}

function renderCompareResults(result: any) {
  const strategies = result.strategies || {};
  const chartData = Object.entries(strategies)
    .filter(([_, v]: any) => !v.error)
    .map(([key, val]: any) => ({
      name: key.replace(/_/g, ' ').toUpperCase(),
      sharpe: val.metrics?.sharpe_ratio || 0,
      return: (val.metrics?.annualized_mean || 0) * 100,
      volatility: (val.metrics?.annualized_standard_deviation || 0) * 100,
      max_drawdown: Math.abs(val.metrics?.max_drawdown || 0) * 100,
    }));

  return (
    <>
      {/* Bar chart comparison */}
      <div style={COMMON_STYLES.panel}>
        <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>
          STRATEGY COMPARISON ({chartData.length} strategies)
        </div>
        <ResponsiveContainer width="100%" height={300}>
          <BarChart data={chartData} margin={{ top: 10, right: 30, bottom: 40, left: 10 }}>
            <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
            <XAxis dataKey="name" tick={{ fill: COLORS.GRAY, fontSize: 8 }} angle={-30} textAnchor="end" height={60} />
            <YAxis tick={{ fill: COLORS.GRAY, fontSize: 9 }} />
            <Tooltip contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: '10px' }} />
            <Legend wrapperStyle={{ fontSize: '9px' }} />
            <Bar dataKey="sharpe" name="Sharpe Ratio" fill={COLORS.CYAN} />
            <Bar dataKey="return" name="Return %" fill={COLORS.GREEN} />
            <Bar dataKey="max_drawdown" name="Max DD %" fill={COLORS.RED} />
          </BarChart>
        </ResponsiveContainer>
      </div>

      {/* Strategy Details */}
      <div style={COMMON_STYLES.panel}>
        <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>STRATEGY DETAILS</div>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr>
              <th style={COMMON_STYLES.tableHeader}>STRATEGY</th>
              <th style={{ ...COMMON_STYLES.tableHeader, textAlign: 'right' }}>SHARPE</th>
              <th style={{ ...COMMON_STYLES.tableHeader, textAlign: 'right' }}>RETURN</th>
              <th style={{ ...COMMON_STYLES.tableHeader, textAlign: 'right' }}>VOLATILITY</th>
              <th style={{ ...COMMON_STYLES.tableHeader, textAlign: 'right' }}>MAX DD</th>
            </tr>
          </thead>
          <tbody>
            {chartData.sort((a, b) => b.sharpe - a.sharpe).map((s, i) => (
              <tr key={s.name} style={i % 2 ? COMMON_STYLES.tableRowAlt : {}}>
                <td style={{ padding: '6px 12px', fontSize: '10px', color: COLORS.ORANGE, borderBottom: BORDERS.STANDARD }}>{s.name}</td>
                <td style={{ padding: '6px 12px', fontSize: '10px', color: s.sharpe > 0 ? COLORS.GREEN : COLORS.RED, textAlign: 'right', borderBottom: BORDERS.STANDARD }}>{s.sharpe.toFixed(4)}</td>
                <td style={{ padding: '6px 12px', fontSize: '10px', color: s.return > 0 ? COLORS.GREEN : COLORS.RED, textAlign: 'right', borderBottom: BORDERS.STANDARD }}>{s.return.toFixed(2)}%</td>
                <td style={{ padding: '6px 12px', fontSize: '10px', color: COLORS.CYAN, textAlign: 'right', borderBottom: BORDERS.STANDARD }}>{s.volatility.toFixed(2)}%</td>
                <td style={{ padding: '6px 12px', fontSize: '10px', color: COLORS.RED, textAlign: 'right', borderBottom: BORDERS.STANDARD }}>-{s.max_drawdown.toFixed(2)}%</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  );
}

function renderRiskResults(result: any) {
  const metrics = result.metrics || {};
  const entries = Object.entries(metrics).filter(([_, v]) => v !== null);

  return (
    <div style={COMMON_STYLES.panel}>
      <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>
        RISK METRICS ({entries.length} measures)
      </div>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))', gap: SPACING.GAP_MEDIUM }}>
        {entries.map(([key, val]) => (
          <div key={key} style={COMMON_STYLES.metricCard}>
            <span style={COMMON_STYLES.dataLabel}>{key.replace(/_/g, ' ')}</span>
            <span style={{
              ...COMMON_STYLES.dataValue,
              color: key.includes('sharpe') || key.includes('sortino') || key.includes('calmar') || key.includes('return')
                ? (Number(val) > 0 ? COLORS.GREEN : COLORS.RED)
                : COLORS.CYAN,
            }}>
              {typeof val === 'number'
                ? (Math.abs(val) > 1 ? val.toFixed(4) : val.toFixed(6))
                : String(val)}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}

function renderBacktestResults(result: any) {
  const cumulative = result.portfolio_cumulative || [];
  const benchmark = result.benchmark_cumulative || [];
  const dates = result.dates || [];
  const metrics = result.metrics || {};

  // Sample to avoid rendering too many points
  const step = Math.max(1, Math.floor(cumulative.length / 200));
  const chartData = cumulative.filter((_: any, i: number) => i % step === 0).map((v: number, i: number) => ({
    date: dates[i * step] ? dates[i * step].slice(0, 10) : i.toString(),
    portfolio: v,
    benchmark: benchmark[i * step] || 1,
  }));

  return (
    <>
      {/* Backtest metrics */}
      <div style={COMMON_STYLES.panel}>
        <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>
          BACKTEST RESULTS ({result.n_rebalances} rebalances)
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(150px, 1fr))', gap: SPACING.GAP_MEDIUM }}>
          {Object.entries(metrics).map(([key, val]) => (
            <div key={key} style={COMMON_STYLES.metricCard}>
              <span style={COMMON_STYLES.dataLabel}>{key.replace(/_/g, ' ')}</span>
              <span style={{
                ...COMMON_STYLES.dataValue,
                color: key.includes('return') || key.includes('sharpe') || key.includes('calmar')
                  ? (Number(val) > 0 ? COLORS.GREEN : COLORS.RED)
                  : COLORS.CYAN,
              }}>
                {typeof val === 'number'
                  ? (key.includes('return') || key.includes('drawdown')
                    ? `${(Number(val) * 100).toFixed(2)}%`
                    : Number(val).toFixed(4))
                  : String(val)}
              </span>
            </div>
          ))}
        </div>
      </div>

      {/* Cumulative chart */}
      <div style={COMMON_STYLES.panel}>
        <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>CUMULATIVE RETURNS</div>
        <ResponsiveContainer width="100%" height={350}>
          <LineChart data={chartData} margin={{ top: 10, right: 30, bottom: 10, left: 10 }}>
            <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
            <XAxis dataKey="date" tick={{ fill: COLORS.GRAY, fontSize: 8 }} />
            <YAxis tick={{ fill: COLORS.GRAY, fontSize: 9 }} />
            <Tooltip contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: '10px' }} />
            <Legend wrapperStyle={{ fontSize: '9px' }} />
            <Line type="monotone" dataKey="portfolio" stroke={COLORS.ORANGE} dot={false} name="Portfolio" strokeWidth={2} />
            <Line type="monotone" dataKey="benchmark" stroke={COLORS.GRAY} dot={false} name="Equal Weight" strokeWidth={1} />
          </LineChart>
        </ResponsiveContainer>
      </div>
    </>
  );
}

function renderStressResults(result: any) {
  const mc = result.monte_carlo || {};
  const scenarios = result.scenarios || {};
  const scenarioData = Object.entries(scenarios).map(([name, val]: any) => ({
    name: name.replace(/_/g, ' ').toUpperCase(),
    expected_loss: (val.expected_loss * 100),
    worst_case: (val.worst_case_loss * 100),
    ruin_prob: (val.probability_of_ruin * 100),
  }));

  return (
    <>
      {/* Monte Carlo */}
      <div style={COMMON_STYLES.panel}>
        <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>
          MONTE CARLO SIMULATION ({mc.n_simulations?.toLocaleString()} runs)
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(160px, 1fr))', gap: SPACING.GAP_MEDIUM }}>
          {[
            ['Mean Final Value', mc.mean_final_value, COLORS.CYAN],
            ['Median Final Value', mc.median_final_value, COLORS.CYAN],
            ['5th Percentile (VaR)', mc.var_5, COLORS.RED],
            ['1st Percentile', mc.var_1, COLORS.RED],
            ['95th Percentile', mc.best_case, COLORS.GREEN],
            ['P(Loss)', `${((mc.probability_loss || 0) * 100).toFixed(1)}%`, COLORS.YELLOW],
          ].map(([label, val, color]) => (
            <div key={String(label)} style={COMMON_STYLES.metricCard}>
              <span style={COMMON_STYLES.dataLabel}>{String(label)}</span>
              <span style={{ ...COMMON_STYLES.dataValue, color: String(color) }}>
                {typeof val === 'number' ? val.toFixed(4) : String(val)}
              </span>
            </div>
          ))}
        </div>
      </div>

      {/* Scenario Analysis */}
      <div style={COMMON_STYLES.panel}>
        <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: SPACING.MEDIUM }}>HISTORICAL STRESS SCENARIOS</div>
        <ResponsiveContainer width="100%" height={300}>
          <BarChart data={scenarioData} margin={{ top: 10, right: 30, bottom: 40, left: 10 }}>
            <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
            <XAxis dataKey="name" tick={{ fill: COLORS.GRAY, fontSize: 8 }} angle={-20} textAnchor="end" height={60} />
            <YAxis tick={{ fill: COLORS.GRAY, fontSize: 9 }} />
            <Tooltip contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: '10px' }} />
            <Legend wrapperStyle={{ fontSize: '9px' }} />
            <Bar dataKey="expected_loss" name="Expected Loss %" fill={COLORS.ORANGE} />
            <Bar dataKey="worst_case" name="Worst Case Loss %" fill={COLORS.RED} />
          </BarChart>
        </ResponsiveContainer>
      </div>
    </>
  );
}

export default SkfolioView;
