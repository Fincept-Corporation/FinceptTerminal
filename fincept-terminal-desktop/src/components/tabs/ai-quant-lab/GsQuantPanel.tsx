/**
 * GS Quant Panel - Goldman Sachs Quant Analytics
 * Covers: Risk Metrics, Portfolio Analytics, Greeks, VaR, Stress Testing, Backtesting, Statistics
 */

import React, { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  Activity,
  TrendingUp,
  BarChart2,
  Calculator,
  RefreshCw,
  AlertCircle,
  Play,
  Shield,
  Target,
  Zap,
  PieChart,
  LineChart,
  BarChart3,
} from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

const GS_PURPLE = '#6F42C1';

type AnalysisMode = 'risk_metrics' | 'portfolio' | 'greeks' | 'var_analysis' | 'stress_test' | 'backtest' | 'statistics';

interface ModeConfig {
  id: AnalysisMode;
  label: string;
  icon: React.ElementType;
  description: string;
}

const MODES: ModeConfig[] = [
  { id: 'risk_metrics', label: 'Risk Metrics', icon: Shield, description: 'Volatility, drawdown, Sharpe, Sortino, VaR' },
  { id: 'portfolio', label: 'Portfolio Analytics', icon: PieChart, description: 'Alpha, tracking error, capture ratio, information ratio' },
  { id: 'greeks', label: 'Greeks', icon: Calculator, description: 'Delta, gamma, vega, theta, rho + higher-order' },
  { id: 'var_analysis', label: 'VaR Analysis', icon: Target, description: 'Parametric, historical, Monte Carlo VaR + CVaR' },
  { id: 'stress_test', label: 'Stress Test', icon: Zap, description: '9 standard market stress scenarios' },
  { id: 'backtest', label: 'Backtest', icon: BarChart3, description: 'Buy-and-hold, momentum, mean-reversion, rebalancing' },
  { id: 'statistics', label: 'Statistics', icon: LineChart, description: 'Descriptive statistics, distribution, percentiles' },
];

export function GsQuantPanel() {
  const { colors } = useTerminalTheme();
  const [mode, setMode] = useState<AnalysisMode>('risk_metrics');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<Record<string, any> | null>(null);

  // Common form state
  const [returnsInput, setReturnsInput] = useState('0.01, -0.005, 0.008, -0.003, 0.012, -0.007, 0.006, 0.009, -0.004, 0.011, -0.002, 0.007, -0.006, 0.013, -0.001, 0.005, -0.008, 0.010, 0.003, -0.009');
  const [benchmarkInput, setBenchmarkInput] = useState('0.008, -0.003, 0.006, -0.002, 0.009, -0.005, 0.004, 0.007, -0.003, 0.008, -0.001, 0.005, -0.004, 0.010, -0.002, 0.003, -0.006, 0.007, 0.002, -0.007');
  const [riskFreeRate, setRiskFreeRate] = useState('0.02');
  const [confidence, setConfidence] = useState('0.95');
  const [positionValue, setPositionValue] = useState('1000000');

  // Greeks form
  const [spot, setSpot] = useState('100');
  const [strike, setStrike] = useState('100');
  const [expiry, setExpiry] = useState('0.25');
  const [rate, setRate] = useState('0.05');
  const [vol, setVol] = useState('0.2');
  const [optionType, setOptionType] = useState('call');

  // Backtest form
  const [strategy, setStrategy] = useState('buy_and_hold');
  const [initialCapital, setInitialCapital] = useState('100000');
  const [lookback, setLookback] = useState('20');

  const parseFloatArray = (input: string): number[] => {
    return input.split(',').map(s => parseFloat(s.trim())).filter(n => !isNaN(n));
  };

  const runAnalysis = useCallback(async () => {
    setLoading(true);
    setError(null);
    setResult(null);

    try {
      let response: string;
      const returns = parseFloatArray(returnsInput);
      const benchmark = parseFloatArray(benchmarkInput);

      switch (mode) {
        case 'risk_metrics':
          response = await invoke<string>('gs_quant_risk_metrics', {
            returns,
            riskFreeRate: parseFloat(riskFreeRate) || 0,
          });
          break;

        case 'portfolio':
          response = await invoke<string>('gs_quant_portfolio_analytics', {
            returns,
            benchmarkReturns: benchmark,
            riskFreeRate: parseFloat(riskFreeRate) || 0,
          });
          break;

        case 'greeks':
          response = await invoke<string>('gs_quant_greeks', {
            spot: parseFloat(spot),
            strike: parseFloat(strike),
            expiry: parseFloat(expiry),
            rate: parseFloat(rate),
            vol: parseFloat(vol),
            optionType,
          });
          break;

        case 'var_analysis':
          response = await invoke<string>('gs_quant_var_analysis', {
            returns,
            confidence: parseFloat(confidence) || 0.95,
            positionValue: parseFloat(positionValue) || 1000000,
          });
          break;

        case 'stress_test':
          response = await invoke<string>('gs_quant_stress_test', {
            returns,
            positionValue: parseFloat(positionValue) || 1000000,
          });
          break;

        case 'backtest': {
          const pricesArr = returns.length > 0
            ? returns.reduce((acc: number[], r, i) => {
                acc.push(i === 0 ? 100 : acc[i - 1] * (1 + r));
                return acc;
              }, [])
            : [100];
          response = await invoke<string>('gs_quant_backtest', {
            prices: JSON.stringify({ asset: pricesArr }),
            strategy,
            initialCapital: parseFloat(initialCapital) || 100000,
            lookback: parseInt(lookback) || 20,
          });
          break;
        }

        case 'statistics':
          response = await invoke<string>('gs_quant_statistics', {
            values: returns,
          });
          break;

        default:
          throw new Error(`Unknown mode: ${mode}`);
      }

      const parsed = JSON.parse(response);
      if (parsed.error) {
        setError(parsed.error);
      } else {
        setResult(parsed);
      }
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally {
      setLoading(false);
    }
  }, [mode, returnsInput, benchmarkInput, riskFreeRate, confidence, positionValue, spot, strike, expiry, rate, vol, optionType, strategy, initialCapital, lookback]);

  const inputStyle: React.CSSProperties = {
    backgroundColor: colors.background,
    border: `1px solid ${'#333'}`,
    color: colors.text,
    padding: '6px 10px',
    fontSize: '12px',
    fontFamily: 'monospace',
    width: '100%',
    outline: 'none',
  };

  const labelStyle: React.CSSProperties = {
    color: colors.textMuted,
    fontSize: '10px',
    fontWeight: 600,
    textTransform: 'uppercase' as const,
    letterSpacing: '0.5px',
    marginBottom: '4px',
  };

  const cardStyle: React.CSSProperties = {
    backgroundColor: colors.background,
    border: `1px solid ${'#333'}`,
    padding: '12px',
  };

  const renderCommonInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      <div>
        <div style={labelStyle}>Returns (comma-separated)</div>
        <textarea
          value={returnsInput}
          onChange={e => setReturnsInput(e.target.value)}
          rows={3}
          style={{ ...inputStyle, resize: 'vertical' }}
          placeholder="0.01, -0.005, 0.008, ..."
        />
      </div>
      {(mode === 'portfolio') && (
        <div>
          <div style={labelStyle}>Benchmark Returns</div>
          <textarea
            value={benchmarkInput}
            onChange={e => setBenchmarkInput(e.target.value)}
            rows={2}
            style={{ ...inputStyle, resize: 'vertical' }}
            placeholder="0.008, -0.003, 0.006, ..."
          />
        </div>
      )}
      {['risk_metrics', 'portfolio'].includes(mode) && (
        <div>
          <div style={labelStyle}>Risk-Free Rate</div>
          <input value={riskFreeRate} onChange={e => setRiskFreeRate(e.target.value)} style={inputStyle} />
        </div>
      )}
      {['var_analysis', 'stress_test'].includes(mode) && (
        <div style={{ display: 'flex', gap: '8px' }}>
          <div style={{ flex: 1 }}>
            <div style={labelStyle}>Confidence</div>
            <input value={confidence} onChange={e => setConfidence(e.target.value)} style={inputStyle} />
          </div>
          <div style={{ flex: 1 }}>
            <div style={labelStyle}>Position Value</div>
            <input value={positionValue} onChange={e => setPositionValue(e.target.value)} style={inputStyle} />
          </div>
        </div>
      )}
    </div>
  );

  const renderGreeksInputs = () => (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
      <div><div style={labelStyle}>Spot Price</div><input value={spot} onChange={e => setSpot(e.target.value)} style={inputStyle} /></div>
      <div><div style={labelStyle}>Strike Price</div><input value={strike} onChange={e => setStrike(e.target.value)} style={inputStyle} /></div>
      <div><div style={labelStyle}>Expiry (years)</div><input value={expiry} onChange={e => setExpiry(e.target.value)} style={inputStyle} /></div>
      <div><div style={labelStyle}>Risk-Free Rate</div><input value={rate} onChange={e => setRate(e.target.value)} style={inputStyle} /></div>
      <div><div style={labelStyle}>Volatility</div><input value={vol} onChange={e => setVol(e.target.value)} style={inputStyle} /></div>
      <div>
        <div style={labelStyle}>Option Type</div>
        <select value={optionType} onChange={e => setOptionType(e.target.value)} style={inputStyle}>
          <option value="call">Call</option>
          <option value="put">Put</option>
        </select>
      </div>
    </div>
  );

  const renderBacktestInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      <div>
        <div style={labelStyle}>Returns (comma-separated)</div>
        <textarea
          value={returnsInput}
          onChange={e => setReturnsInput(e.target.value)}
          rows={3}
          style={{ ...inputStyle, resize: 'vertical' }}
        />
      </div>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
        <div>
          <div style={labelStyle}>Strategy</div>
          <select value={strategy} onChange={e => setStrategy(e.target.value)} style={inputStyle}>
            <option value="buy_and_hold">Buy & Hold</option>
            <option value="momentum">Momentum</option>
            <option value="mean_reversion">Mean Reversion</option>
            <option value="rebalancing">Rebalancing</option>
          </select>
        </div>
        <div><div style={labelStyle}>Initial Capital</div><input value={initialCapital} onChange={e => setInitialCapital(e.target.value)} style={inputStyle} /></div>
        <div><div style={labelStyle}>Lookback</div><input value={lookback} onChange={e => setLookback(e.target.value)} style={inputStyle} /></div>
      </div>
    </div>
  );

  const renderMetricRow = (label: string, value: any, color?: string) => {
    const displayValue = typeof value === 'number'
      ? (Math.abs(value) < 0.01 && value !== 0 ? value.toExponential(4) : value.toFixed(4))
      : String(value ?? 'N/A');
    const isNeg = typeof value === 'number' && value < 0;
    return (
      <div key={label} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '4px 0', borderBottom: `1px solid ${'#222'}` }}>
        <span style={{ color: colors.textMuted, fontSize: '11px' }}>{label}</span>
        <span style={{ color: color || (isNeg ? colors.alert : colors.success), fontSize: '12px', fontFamily: 'monospace', fontWeight: 600 }}>{displayValue}</span>
      </div>
    );
  };

  const renderResults = () => {
    if (!result) return null;

    if (mode === 'greeks') {
      const greekNames = ['delta', 'gamma', 'vega', 'theta', 'rho', 'vanna', 'volga', 'charm', 'speed', 'zomma', 'color'];
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>GREEKS RESULT</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {greekNames.map(g => {
              const val = result[g];
              if (val === undefined || val === null) return null;
              return renderMetricRow(g.charAt(0).toUpperCase() + g.slice(1), val, GS_PURPLE);
            })}
            {result.price !== undefined && renderMetricRow('Option Price', result.price, colors.success)}
          </div>
        </div>
      );
    }

    if (mode === 'risk_metrics') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>RISK METRICS</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {renderMetricRow('Ann. Volatility', result.volatility_annualized)}
            {renderMetricRow('Daily Risk', result.daily_risk)}
            {renderMetricRow('Downside Risk', result.downside_risk)}
            {renderMetricRow('Max Drawdown', result.max_drawdown)}
            {renderMetricRow('Max DD Length', result.max_drawdown_length, colors.text)}
            {renderMetricRow('Recovery Period', result.max_recovery_period, colors.text)}
            {renderMetricRow('Sharpe Ratio', result.sharpe_ratio)}
            {renderMetricRow('Sortino Ratio', result.sortino_ratio)}
            {renderMetricRow('Calmar Ratio', result.calmar_ratio)}
            {renderMetricRow('Omega Ratio', result.omega_ratio)}
            {renderMetricRow('VaR (95%)', result.var_95)}
            {renderMetricRow('VaR (99%)', result.var_99)}
          </div>
        </div>
      );
    }

    if (mode === 'portfolio') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>PORTFOLIO ANALYTICS</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {renderMetricRow('Alpha', result.alpha)}
            {renderMetricRow('Jensen Alpha', result.jensen_alpha)}
            {renderMetricRow('Bull Alpha', result.jensen_alpha_bull)}
            {renderMetricRow('Bear Alpha', result.jensen_alpha_bear)}
            {renderMetricRow('Sharpe Ratio', result.sharpe_ratio)}
            {renderMetricRow('Sortino Ratio', result.sortino_ratio)}
            {renderMetricRow('Calmar Ratio', result.calmar_ratio)}
            {renderMetricRow('Treynor', result.treynor_measure)}
            {renderMetricRow('Modigliani (M2)', result.modigliani_ratio)}
            {renderMetricRow('Tracking Error', result.tracking_error)}
            {renderMetricRow('Info Ratio', result.information_ratio)}
            {renderMetricRow('R-squared', result.r_squared, colors.text)}
            {renderMetricRow('Hit Rate', result.hit_rate, colors.text)}
            {renderMetricRow('Up Capture', result.up_capture, colors.text)}
            {renderMetricRow('Down Capture', result.down_capture, colors.text)}
            {renderMetricRow('Max Drawdown', result.max_drawdown)}
            {renderMetricRow('Skewness', result.skewness, colors.text)}
            {renderMetricRow('Kurtosis', result.kurtosis, colors.text)}
          </div>
        </div>
      );
    }

    if (mode === 'var_analysis') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>VALUE AT RISK</div>
          {['parametric_var', 'historical_var', 'monte_carlo_var', 'cvar'].map(key => {
            const section = result[key];
            if (!section || typeof section !== 'object') return null;
            const displayName = key.replace(/_/g, ' ').toUpperCase();
            return (
              <div key={key} style={{ marginBottom: '12px' }}>
                <div style={{ color: GS_PURPLE, fontSize: '11px', fontWeight: 600, marginBottom: '4px' }}>{displayName}</div>
                {Object.entries(section).map(([k, v]) => renderMetricRow(k.replace(/_/g, ' '), v as number))}
              </div>
            );
          })}
        </div>
      );
    }

    if (mode === 'stress_test') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>STRESS TEST SCENARIOS</div>
          {Object.entries(result).map(([scenarioName, scenarioData]) => (
            <div key={scenarioName} style={{ marginBottom: '12px', padding: '8px', border: `1px solid ${'#333'}` }}>
              <div style={{ color: GS_PURPLE, fontSize: '11px', fontWeight: 600, marginBottom: '4px' }}>
                {scenarioName.replace(/_/g, ' ').toUpperCase()}
              </div>
              {typeof scenarioData === 'object' && scenarioData !== null
                ? Object.entries(scenarioData as Record<string, any>).map(([k, v]) =>
                    renderMetricRow(k.replace(/_/g, ' '), v as number)
                  )
                : <div style={{ color: colors.text, fontSize: '11px' }}>{String(scenarioData)}</div>
              }
            </div>
          ))}
        </div>
      );
    }

    if (mode === 'backtest') {
      const metrics = result.metrics || result;
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>BACKTEST RESULTS</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {Object.entries(metrics).map(([k, v]) => {
              if (typeof v === 'object') return null;
              return renderMetricRow(k.replace(/_/g, ' '), v as number);
            })}
          </div>
        </div>
      );
    }

    if (mode === 'statistics') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>DESCRIPTIVE STATISTICS</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {renderMetricRow('Mean', result.mean)}
            {renderMetricRow('Median', result.median)}
            {renderMetricRow('Std Dev', result.std)}
            {renderMetricRow('Variance', result.variance)}
            {renderMetricRow('Min', result.min)}
            {renderMetricRow('Max', result.max)}
            {renderMetricRow('Range', result.range)}
            {renderMetricRow('Skewness', result.skewness, colors.text)}
            {renderMetricRow('Kurtosis', result.kurtosis, colors.text)}
            {renderMetricRow('Count', result.count, colors.text)}
            {renderMetricRow('Sum', result.sum)}
            {renderMetricRow('Semi-Variance', result.semi_variance)}
            {renderMetricRow('Realized Var', result.realized_variance)}
          </div>
          {result.percentiles && (
            <div style={{ marginTop: '12px' }}>
              <div style={{ ...labelStyle, marginBottom: '4px' }}>PERCENTILES</div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '4px 16px' }}>
                {Object.entries(result.percentiles).map(([pct, v]) =>
                  renderMetricRow(`P${pct}`, v as number, colors.text)
                )}
              </div>
            </div>
          )}
        </div>
      );
    }

    // Fallback: render all key-value pairs
    return (
      <div style={cardStyle}>
        <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>RESULT</div>
        <pre style={{ color: colors.text, fontSize: '11px', fontFamily: 'monospace', whiteSpace: 'pre-wrap', overflowX: 'auto' }}>
          {JSON.stringify(result, null, 2)}
        </pre>
      </div>
    );
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Mode Tabs */}
      <div
        style={{
          display: 'flex',
          gap: '2px',
          padding: '8px 12px',
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${'#333'}`,
          flexWrap: 'wrap',
        }}
      >
        {MODES.map(m => {
          const Icon = m.icon;
          const isActive = mode === m.id;
          return (
            <button
              key={m.id}
              onClick={() => { setMode(m.id); setResult(null); setError(null); }}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                padding: '5px 10px',
                fontSize: '11px',
                fontWeight: isActive ? 700 : 400,
                color: isActive ? GS_PURPLE : colors.textMuted,
                backgroundColor: isActive ? `${GS_PURPLE}20` : 'transparent',
                border: isActive ? `1px solid ${GS_PURPLE}` : `1px solid transparent`,
                cursor: 'pointer',
                transition: 'all 0.15s',
              }}
            >
              <Icon size={13} />
              {m.label}
            </button>
          );
        })}
      </div>

      {/* Content Area */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {/* Description */}
        <div style={{ color: colors.textMuted, fontSize: '11px', padding: '6px 0' }}>
          {MODES.find(m => m.id === mode)?.description}
        </div>

        {/* Inputs */}
        {mode === 'greeks' ? renderGreeksInputs() :
         mode === 'backtest' ? renderBacktestInputs() :
         renderCommonInputs()}

        {/* Run Button */}
        <button
          onClick={runAnalysis}
          disabled={loading}
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            padding: '8px 16px',
            backgroundColor: loading ? `${GS_PURPLE}60` : GS_PURPLE,
            color: '#fff',
            border: 'none',
            cursor: loading ? 'not-allowed' : 'pointer',
            fontSize: '12px',
            fontWeight: 600,
          }}
        >
          {loading ? <RefreshCw size={14} className="animate-spin" /> : <Play size={14} />}
          {loading ? 'ANALYZING...' : 'RUN ANALYSIS'}
        </button>

        {/* Error */}
        {error && (
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px', padding: '10px', backgroundColor: `${colors.alert}15`, border: `1px solid ${colors.alert}` }}>
            <AlertCircle size={14} color={colors.alert} style={{ flexShrink: 0, marginTop: 2 }} />
            <div style={{ color: colors.alert, fontSize: '11px', fontFamily: 'monospace', whiteSpace: 'pre-wrap' }}>{error}</div>
          </div>
        )}

        {/* Results */}
        {renderResults()}
      </div>
    </div>
  );
}

export default GsQuantPanel;
