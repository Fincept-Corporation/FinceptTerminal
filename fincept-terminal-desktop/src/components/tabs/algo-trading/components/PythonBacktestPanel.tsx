import React, { useState, useEffect } from 'react';
import {
  Play,
  Loader2,
  AlertCircle,
  TrendingUp,
  TrendingDown,
  BarChart3,
  Calendar,
  DollarSign,
  Target,
  Percent,
  ArrowDownRight,
  Clock,
  X,
  Settings,
  ChevronDown,
  ChevronUp,
} from 'lucide-react';
import type { PythonStrategy, PythonBacktestResult, BacktestMetrics, EquityPoint, BacktestTrade } from '../types';
import { runPythonBacktest, getPythonStrategyCode } from '../services/algoTradingService';
import ParameterEditor from './ParameterEditor';
import { F } from '../constants/theme';

interface PythonBacktestPanelProps {
  strategy: PythonStrategy;
  onClose: () => void;
}

const PythonBacktestPanel: React.FC<PythonBacktestPanelProps> = ({ strategy, onClose }) => {
  const [symbols, setSymbols] = useState('SPY,AAPL,MSFT');
  const [startDate, setStartDate] = useState(() => {
    const date = new Date();
    date.setFullYear(date.getFullYear() - 1);
    return date.toISOString().split('T')[0];
  });
  const [endDate, setEndDate] = useState(() => new Date().toISOString().split('T')[0]);
  const [initialCash, setInitialCash] = useState(100000);
  const [dataProvider, setDataProvider] = useState<'yfinance' | 'fyers'>('yfinance');
  const [parameters, setParameters] = useState<Record<string, string>>({});
  const [showParams, setShowParams] = useState(false);
  const [strategyCode, setStrategyCode] = useState<string | undefined>(undefined);

  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<PythonBacktestResult | null>(null);

  // Load strategy code for parameter extraction
  useEffect(() => {
    const loadCode = async () => {
      try {
        const response = await getPythonStrategyCode(strategy.id);
        if (response.success && response.data) {
          setStrategyCode(response.data.code);
        }
      } catch (err) {
        console.error('Failed to load strategy code:', err);
      }
    };
    loadCode();
  }, [strategy.id]);

  const handleRunBacktest = async () => {
    setLoading(true);
    setError(null);
    setResult(null);

    try {
      const response = await runPythonBacktest({
        strategy_id: strategy.id,
        symbols: symbols.split(',').map((s) => s.trim().toUpperCase()),
        start_date: startDate,
        end_date: endDate,
        initial_cash: initialCash,
        parameters,
        data_provider: dataProvider,
      });

      if (!response.success) {
        setError(response.error || 'Backtest failed');
        return;
      }

      if (response.data) {
        setResult(response.data);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.8)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 1000,
      }}
      onClick={onClose}
    >
      <div
        style={{
          width: '900px',
          maxHeight: '90vh',
          backgroundColor: F.PANEL_BG,
          border: `1px solid ${F.BORDER}`,
          borderRadius: '2px',
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div
          style={{
            padding: '16px',
            borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            backgroundColor: F.HEADER_BG,
          }}
        >
          <div>
            <div style={{ fontSize: '12px', fontWeight: 700, color: F.WHITE }}>
              BACKTEST: {strategy.name}
            </div>
            <div style={{ fontSize: '10px', color: F.GRAY, marginTop: '4px' }}>
              {strategy.id} - {strategy.category}
            </div>
          </div>
          <button
            onClick={onClose}
            style={{
              backgroundColor: 'transparent',
              border: 'none',
              color: F.GRAY,
              cursor: 'pointer',
              padding: '4px',
            }}
          >
            <X size={18} />
          </button>
        </div>

        {/* Content */}
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {/* Configuration */}
          <div
            style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(3, 1fr)',
              gap: '12px',
              marginBottom: '16px',
            }}
          >
            {/* Symbols */}
            <div>
              <label style={{ fontSize: '10px', color: F.GRAY, marginBottom: '4px', display: 'block' }}>
                SYMBOLS (comma-separated)
              </label>
              <input
                type="text"
                value={symbols}
                onChange={(e) => setSymbols(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: F.DARK_BG,
                  border: `1px solid ${F.BORDER}`,
                  borderRadius: '2px',
                  color: F.WHITE,
                  fontSize: '11px',
                }}
                placeholder="SPY, AAPL, MSFT"
              />
            </div>

            {/* Start Date */}
            <div>
              <label style={{ fontSize: '10px', color: F.GRAY, marginBottom: '4px', display: 'block' }}>
                START DATE
              </label>
              <input
                type="date"
                value={startDate}
                onChange={(e) => setStartDate(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: F.DARK_BG,
                  border: `1px solid ${F.BORDER}`,
                  borderRadius: '2px',
                  color: F.WHITE,
                  fontSize: '11px',
                }}
              />
            </div>

            {/* End Date */}
            <div>
              <label style={{ fontSize: '10px', color: F.GRAY, marginBottom: '4px', display: 'block' }}>
                END DATE
              </label>
              <input
                type="date"
                value={endDate}
                onChange={(e) => setEndDate(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: F.DARK_BG,
                  border: `1px solid ${F.BORDER}`,
                  borderRadius: '2px',
                  color: F.WHITE,
                  fontSize: '11px',
                }}
              />
            </div>

            {/* Initial Cash */}
            <div>
              <label style={{ fontSize: '10px', color: F.GRAY, marginBottom: '4px', display: 'block' }}>
                INITIAL CAPITAL
              </label>
              <input
                type="number"
                value={initialCash}
                onChange={(e) => setInitialCash(Number(e.target.value))}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: F.DARK_BG,
                  border: `1px solid ${F.BORDER}`,
                  borderRadius: '2px',
                  color: F.WHITE,
                  fontSize: '11px',
                }}
                min={1000}
                step={10000}
              />
            </div>

            {/* Data Provider */}
            <div>
              <label style={{ fontSize: '10px', color: F.GRAY, marginBottom: '4px', display: 'block' }}>
                DATA PROVIDER
              </label>
              <select
                value={dataProvider}
                onChange={(e) => setDataProvider(e.target.value as 'yfinance' | 'fyers')}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: F.DARK_BG,
                  border: `1px solid ${F.BORDER}`,
                  borderRadius: '2px',
                  color: F.WHITE,
                  fontSize: '11px',
                }}
              >
                <option value="yfinance">Yahoo Finance</option>
                <option value="fyers">Fyers (Local Cache)</option>
              </select>
            </div>

            {/* Run Button */}
            <div style={{ display: 'flex', alignItems: 'flex-end' }}>
              <button
                onClick={handleRunBacktest}
                disabled={loading || !symbols.trim()}
                style={{
                  width: '100%',
                  padding: '10px',
                  backgroundColor: loading ? F.GRAY : F.ORANGE,
                  color: F.DARK_BG,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '11px',
                  fontWeight: 700,
                  cursor: loading ? 'not-allowed' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                }}
              >
                {loading ? (
                  <>
                    <Loader2 size={14} style={{ animation: 'spin 1s linear infinite' }} />
                    RUNNING...
                  </>
                ) : (
                  <>
                    <Play size={14} />
                    RUN BACKTEST
                  </>
                )}
              </button>
              <style>{`@keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }`}</style>
            </div>
          </div>

          {/* Parameters Toggle */}
          <button
            onClick={() => setShowParams(!showParams)}
            style={{
              width: '100%',
              padding: '10px 12px',
              backgroundColor: F.DARK_BG,
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              color: F.GRAY,
              fontSize: '10px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              marginBottom: '16px',
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Settings size={12} />
              STRATEGY PARAMETERS
            </div>
            {showParams ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>

          {/* Parameter Editor */}
          {showParams && strategyCode && (
            <div style={{ marginBottom: '16px' }}>
              <ParameterEditor
                code={strategyCode}
                values={parameters}
                onChange={setParameters}
                disabled={loading}
              />
            </div>
          )}

          {/* Error */}
          {error && (
            <div
              style={{
                padding: '12px',
                backgroundColor: `${F.RED}20`,
                border: `1px solid ${F.RED}`,
                borderRadius: '2px',
                marginBottom: '16px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                color: F.RED,
                fontSize: '11px',
              }}
            >
              <AlertCircle size={16} />
              {error}
            </div>
          )}

          {/* Results */}
          {result && (
            <div>
              {/* Metrics Grid */}
              <div
                style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(4, 1fr)',
                  gap: '12px',
                  marginBottom: '16px',
                }}
              >
                <MetricCard
                  label="Total Return"
                  value={`${result.metrics.total_return_pct >= 0 ? '+' : ''}${result.metrics.total_return_pct.toFixed(2)}%`}
                  valueColor={result.metrics.total_return_pct >= 0 ? F.GREEN : F.RED}
                  icon={result.metrics.total_return_pct >= 0 ? TrendingUp : TrendingDown}
                />
                <MetricCard
                  label="Total Trades"
                  value={result.metrics.total_trades.toString()}
                  icon={BarChart3}
                />
                <MetricCard
                  label="Win Rate"
                  value={`${result.metrics.win_rate.toFixed(1)}%`}
                  valueColor={result.metrics.win_rate >= 50 ? F.GREEN : F.YELLOW}
                  icon={Target}
                />
                <MetricCard
                  label="Max Drawdown"
                  value={`-${result.metrics.max_drawdown_pct.toFixed(2)}%`}
                  valueColor={F.RED}
                  icon={ArrowDownRight}
                />
                <MetricCard
                  label="Sharpe Ratio"
                  value={result.metrics.sharpe_ratio.toFixed(2)}
                  valueColor={result.metrics.sharpe_ratio >= 1 ? F.GREEN : F.YELLOW}
                  icon={Percent}
                />
                <MetricCard
                  label="Profit Factor"
                  value={result.metrics.profit_factor.toFixed(2)}
                  valueColor={result.metrics.profit_factor >= 1.5 ? F.GREEN : F.YELLOW}
                />
                <MetricCard
                  label="Avg P&L"
                  value={`$${result.metrics.avg_pnl.toFixed(2)}`}
                  valueColor={result.metrics.avg_pnl >= 0 ? F.GREEN : F.RED}
                  icon={DollarSign}
                />
                <MetricCard
                  label="Avg Hold Time"
                  value={`${result.metrics.avg_bars_held.toFixed(0)} bars`}
                  icon={Clock}
                />
              </div>

              {/* Equity Curve */}
              {result.equity_curve.length > 0 && (
                <div
                  style={{
                    marginBottom: '16px',
                    padding: '12px',
                    backgroundColor: F.DARK_BG,
                    borderRadius: '2px',
                    border: `1px solid ${F.BORDER}`,
                  }}
                >
                  <div style={{ fontSize: '10px', color: F.GRAY, marginBottom: '12px' }}>
                    EQUITY CURVE
                  </div>
                  <EquityChart curve={result.equity_curve} />
                </div>
              )}

              {/* Trades Table */}
              {result.trades.length > 0 && (
                <div
                  style={{
                    padding: '12px',
                    backgroundColor: F.DARK_BG,
                    borderRadius: '2px',
                    border: `1px solid ${F.BORDER}`,
                  }}
                >
                  <div style={{ fontSize: '10px', color: F.GRAY, marginBottom: '12px' }}>
                    TRADES ({result.trades.length})
                  </div>
                  <div style={{ maxHeight: '200px', overflow: 'auto' }}>
                    <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
                      <thead>
                        <tr style={{ color: F.GRAY }}>
                          <th style={{ textAlign: 'left', padding: '4px 8px' }}>Symbol</th>
                          <th style={{ textAlign: 'right', padding: '4px 8px' }}>Entry</th>
                          <th style={{ textAlign: 'right', padding: '4px 8px' }}>Exit</th>
                          <th style={{ textAlign: 'right', padding: '4px 8px' }}>P&L</th>
                          <th style={{ textAlign: 'right', padding: '4px 8px' }}>Bars</th>
                        </tr>
                      </thead>
                      <tbody>
                        {result.trades.slice(0, 50).map((trade, idx) => (
                          <tr
                            key={trade.id || idx}
                            style={{
                              borderTop: `1px solid ${F.BORDER}`,
                              color: F.WHITE,
                            }}
                          >
                            <td style={{ padding: '4px 8px' }}>{trade.symbol}</td>
                            <td style={{ textAlign: 'right', padding: '4px 8px' }}>
                              ${trade.entry_price.toFixed(2)}
                            </td>
                            <td style={{ textAlign: 'right', padding: '4px 8px' }}>
                              ${trade.exit_price?.toFixed(2) || '-'}
                            </td>
                            <td
                              style={{
                                textAlign: 'right',
                                padding: '4px 8px',
                                color: trade.pnl >= 0 ? F.GREEN : F.RED,
                              }}
                            >
                              {trade.pnl >= 0 ? '+' : ''}${trade.pnl.toFixed(2)}
                            </td>
                            <td style={{ textAlign: 'right', padding: '4px 8px', color: F.GRAY }}>
                              {trade.bars_held}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                    {result.trades.length > 50 && (
                      <div style={{ textAlign: 'center', padding: '8px', color: F.GRAY, fontSize: '9px' }}>
                        Showing first 50 of {result.trades.length} trades
                      </div>
                    )}
                  </div>
                </div>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

// Metric Card Component
const MetricCard: React.FC<{
  label: string;
  value: string;
  valueColor?: string;
  icon?: React.ElementType;
}> = ({ label, value, valueColor = F.WHITE, icon: Icon }) => (
  <div
    style={{
      padding: '12px',
      backgroundColor: F.DARK_BG,
      borderRadius: '2px',
      border: `1px solid ${F.BORDER}`,
    }}
  >
    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
      {Icon && <Icon size={12} style={{ color: F.GRAY }} />}
      <span style={{ fontSize: '9px', color: F.GRAY, textTransform: 'uppercase' }}>{label}</span>
    </div>
    <div style={{ fontSize: '14px', fontWeight: 700, color: valueColor }}>{value}</div>
  </div>
);

// Simple Equity Chart (SVG based)
const EquityChart: React.FC<{ curve: EquityPoint[] }> = ({ curve }) => {
  if (curve.length === 0) return null;

  const values = curve.map((p) => p.value);
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;

  const width = 100;
  const height = 60;

  const points = curve.map((p, i) => {
    const x = (i / (curve.length - 1)) * width;
    const y = height - ((p.value - min) / range) * height;
    return `${x},${y}`;
  });

  const pathD = `M ${points.join(' L ')}`;

  return (
    <svg viewBox={`0 0 ${width} ${height}`} style={{ width: '100%', height: '80px' }}>
      <defs>
        <linearGradient id="equityGradient" x1="0%" y1="0%" x2="0%" y2="100%">
          <stop offset="0%" stopColor={F.ORANGE} stopOpacity="0.3" />
          <stop offset="100%" stopColor={F.ORANGE} stopOpacity="0" />
        </linearGradient>
      </defs>
      <path
        d={`${pathD} L ${width},${height} L 0,${height} Z`}
        fill="url(#equityGradient)"
      />
      <path d={pathD} fill="none" stroke={F.ORANGE} strokeWidth="1" />
    </svg>
  );
};

export default PythonBacktestPanel;
