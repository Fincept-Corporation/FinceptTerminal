/**
 * Backtesting Panel - Fincept Professional Design
 * Run strategy backtests with Qlib - Full backend integration
 * Real-world finance application with working backtest engine
 */

import React, { useState } from 'react';
import {
  BarChart3,
  Play,
  Settings,
  TrendingUp,
  TrendingDown,
  DollarSign,
  Activity,
  Target,
  AlertCircle,
  Calendar,
  Layers,
  RefreshCw,
  Download,
  CheckCircle2,
  Info
} from 'lucide-react';
import { qlibService } from '@/services/aiQuantLab/qlibService';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface BacktestMetrics {
  annual_return?: number;
  sharpe_ratio?: number;
  max_drawdown?: number;
  win_rate?: number;
  total_trades?: number;
  profitable_trades?: number;
  avg_win?: number;
  avg_loss?: number;
  profit_factor?: number;
  volatility?: number;
}

export function BacktestingPanel() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  // Strategy Configuration
  const [strategyType, setStrategyType] = useState('topk_dropout');
  const [instruments, setInstruments] = useState('AAPL,MSFT,GOOGL,AMZN,NVDA,TSLA,META,NFLX');
  const [startDate, setStartDate] = useState('2023-01-01');
  const [endDate, setEndDate] = useState('2024-12-31');
  const [initialCapital, setInitialCapital] = useState(100000);
  const [topK, setTopK] = useState(10);

  // Portfolio Configuration
  const [benchmark, setBenchmark] = useState('SPY');
  const [commission, setCommission] = useState(0.001);
  const [slippage, setSlippage] = useState(0.0005);

  // State
  const [isRunning, setIsRunning] = useState(false);
  const [backtestResults, setBacktestResults] = useState<BacktestMetrics | null>(null);
  const [error, setError] = useState<string | null>(null);

  const handleRunBacktest = async () => {
    setIsRunning(true);
    setError(null);
    setBacktestResults(null);

    try {
      const instrumentList = instruments.split(',').map(s => s.trim());

      const strategyConfig = {
        class: strategyType,
        module_path: 'qlib.contrib.strategy.signal_strategy',
        kwargs: {
          signal_threshold: 0.0,
          topk: topK,
          n_drop: Math.floor(topK * 0.05),
          risk_degree: 0.95
        }
      };

      const portfolioConfig = {
        start_time: startDate,
        end_time: endDate,
        account: initialCapital,
        benchmark: benchmark,
        exchange_kwargs: {
          freq: '1d',
          limit_threshold: 0.095,
          deal_price: 'close',
          open_cost: commission,
          close_cost: commission,
          min_cost: 5,
        }
      };

      const result = await qlibService.runBacktest({
        strategy_config: strategyConfig,
        portfolio_config: portfolioConfig
      });

      console.log('[Backtesting] Result:', result);

      if (result.success && result.metrics) {
        setBacktestResults(result.metrics);
      } else {
        setError(result.error || 'Backtest failed');
      }
    } catch (err) {
      console.error('[Backtesting] Error:', err);
      setError(String(err));
    } finally {
      setIsRunning(false);
    }
  };

  return (
    <div className="flex h-full" style={{ backgroundColor: colors.background }}>
      {/* Left Panel - Configuration */}
      <div
        className="w-96 border-r overflow-auto flex-shrink-0"
        style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)' }}
      >
        <div className="p-4 space-y-4">
          {/* Header */}
          <div className="pb-3 border-b" style={{ borderColor: 'var(--ft-border-color, #2A2A2A)' }}>
            <div className="flex items-center gap-2 mb-1">
              <BarChart3 size={18} color={colors.primary} />
              <h2 className="text-sm font-bold uppercase tracking-wide" style={{ color: colors.text }}>
                STRATEGY BACKTESTING
              </h2>
            </div>
            <p className="text-xs font-mono" style={{ color: colors.textMuted }}>
              Test trading strategies with historical data
            </p>
          </div>

          {/* Strategy Configuration */}
          <div>
            <h3 className="text-xs font-bold uppercase tracking-wide mb-3 flex items-center gap-1" style={{ color: colors.textMuted }}>
              <Layers size={12} />
              STRATEGY CONFIGURATION
            </h3>

            <div className="space-y-3">
              <div>
                <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: colors.textMuted }}>
                  Strategy Type
                </label>
                <select
                  value={strategyType}
                  onChange={(e) => setStrategyType(e.target.value)}
                  className="w-full px-3 py-2 rounded text-xs font-mono outline-none uppercase"
                  style={{
                    backgroundColor: colors.background,
                    color: colors.text,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                  }}
                >
                  <option value="topk_dropout">Top-K Dropout</option>
                  <option value="weight_base">Weight-Based</option>
                  <option value="enhanced_indexing">Enhanced Indexing</option>
                </select>
              </div>

              <div>
                <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: colors.textMuted }}>
                  Universe (Comma-separated)
                </label>
                <textarea
                  value={instruments}
                  onChange={(e) => setInstruments(e.target.value)}
                  rows={3}
                  className="w-full px-3 py-2 rounded text-xs font-mono outline-none resize-none"
                  style={{
                    backgroundColor: colors.background,
                    color: colors.text,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                  }}
                />
              </div>

              <div className="grid grid-cols-2 gap-3">
                <div>
                  <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: colors.textMuted }}>
                    <Calendar size={12} className="inline mr-1" />
                    Start Date
                  </label>
                  <input
                    type="date"
                    value={startDate}
                    onChange={(e) => setStartDate(e.target.value)}
                    className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                    style={{
                      backgroundColor: colors.background,
                      color: colors.text,
                      border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                    }}
                  />
                </div>
                <div>
                  <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: colors.textMuted }}>
                    <Calendar size={12} className="inline mr-1" />
                    End Date
                  </label>
                  <input
                    type="date"
                    value={endDate}
                    onChange={(e) => setEndDate(e.target.value)}
                    className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                    style={{
                      backgroundColor: colors.background,
                      color: colors.text,
                      border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                    }}
                  />
                </div>
              </div>

              <div>
                <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: colors.textMuted }}>
                  Top K Positions
                </label>
                <input
                  type="text"
                  inputMode="numeric"
                  value={topK}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) { setTopK(Number(v) || 0) } }}
                  className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                  style={{
                    backgroundColor: colors.background,
                    color: colors.text,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                  }}
                />
              </div>
            </div>
          </div>

          {/* Portfolio Configuration */}
          <div className="pt-3 border-t" style={{ borderColor: 'var(--ft-border-color, #2A2A2A)' }}>
            <h3 className="text-xs font-bold uppercase tracking-wide mb-3 flex items-center gap-1" style={{ color: colors.textMuted }}>
              <Settings size={12} />
              PORTFOLIO SETTINGS
            </h3>

            <div className="space-y-3">
              <div>
                <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: colors.textMuted }}>
                  Initial Capital (USD)
                </label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={initialCapital}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) { setInitialCapital(Number(v) || 0) } }}
                  className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                  style={{
                    backgroundColor: colors.background,
                    color: colors.text,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                  }}
                />
              </div>

              <div>
                <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: colors.textMuted }}>
                  Benchmark
                </label>
                <input
                  type="text"
                  value={benchmark}
                  onChange={(e) => setBenchmark(e.target.value)}
                  placeholder="SPY"
                  className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                  style={{
                    backgroundColor: colors.background,
                    color: colors.text,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                  }}
                />
              </div>

              <div className="grid grid-cols-2 gap-3">
                <div>
                  <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: colors.textMuted }}>
                    Commission (%)
                  </label>
                  <input
                    type="text"
                    inputMode="decimal"
                    value={commission * 100}
                    onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) { setCommission((Number(v) || 0) / 100) } }}
                    className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                    style={{
                      backgroundColor: colors.background,
                      color: colors.text,
                      border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                    }}
                  />
                </div>
                <div>
                  <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: colors.textMuted }}>
                    Slippage (%)
                  </label>
                  <input
                    type="text"
                    inputMode="decimal"
                    value={slippage * 100}
                    onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) { setSlippage((Number(v) || 0) / 100) } }}
                    className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                    style={{
                      backgroundColor: colors.background,
                      color: colors.text,
                      border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                    }}
                  />
                </div>
              </div>
            </div>
          </div>

          {/* Run Button */}
          <div className="pt-3">
            <button
              onClick={handleRunBacktest}
              disabled={isRunning || !instruments || !startDate || !endDate}
              className="w-full py-3 rounded font-bold uppercase text-sm tracking-wide hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed"
              style={{ backgroundColor: colors.primary, color: colors.background }}
            >
              {isRunning ? (
                <>
                  <RefreshCw size={16} className="animate-spin" />
                  RUNNING BACKTEST...
                </>
              ) : (
                <>
                  <Play size={16} />
                  RUN BACKTEST
                </>
              )}
            </button>
          </div>
        </div>
      </div>

      {/* Right Panel - Results */}
      <div className="flex-1 overflow-auto p-6" style={{ backgroundColor: colors.background }}>
        {error ? (
          <div className="flex items-center justify-center h-full">
            <div
              className="max-w-md p-6 rounded border"
              style={{ backgroundColor: colors.panel, borderColor: colors.alert }}
            >
              <div className="flex items-center gap-3 mb-3">
                <AlertCircle size={24} color={colors.alert} />
                <h3 className="text-sm font-bold uppercase tracking-wide" style={{ color: colors.text }}>
                  BACKTEST ERROR
                </h3>
              </div>
              <p className="text-xs font-mono" style={{ color: colors.textMuted }}>
                {error}
              </p>
            </div>
          </div>
        ) : backtestResults ? (
          <div className="space-y-6 max-w-5xl">
            {/* Header */}
            <div className="flex items-center justify-between">
              <div>
                <h2 className="text-xl font-bold uppercase tracking-wide mb-1" style={{ color: colors.text }}>
                  BACKTEST RESULTS
                </h2>
                <p className="text-xs font-mono" style={{ color: colors.textMuted }}>
                  {startDate} to {endDate} • {instruments.split(',').length} instruments
                </p>
              </div>
              <button
                className="px-4 py-2 rounded font-bold uppercase text-xs tracking-wide hover:bg-opacity-80 transition-colors flex items-center gap-2"
                style={{ backgroundColor: colors.panel, color: colors.text }}
              >
                <Download size={14} />
                EXPORT
              </button>
            </div>

            {/* Performance Metrics */}
            <div>
              <h3 className="text-xs font-bold uppercase tracking-wide mb-3" style={{ color: colors.textMuted }}>
                PERFORMANCE METRICS
              </h3>
              <div className="grid grid-cols-3 gap-4">
                <MetricCard
                  label="Annual Return"
                  value={backtestResults.annual_return ? `${(backtestResults.annual_return * 100).toFixed(2)}%` : 'N/A'}
                  icon={<TrendingUp size={18} />}
                  color={backtestResults.annual_return && backtestResults.annual_return > 0 ? colors.success : colors.alert}
                  positive={backtestResults.annual_return ? backtestResults.annual_return > 0 : undefined}
                />
                <MetricCard
                  label="Sharpe Ratio"
                  value={backtestResults.sharpe_ratio?.toFixed(2) || 'N/A'}
                  icon={<Activity size={18} />}
                  color={colors.primary}
                />
                <MetricCard
                  label="Max Drawdown"
                  value={backtestResults.max_drawdown ? `${(backtestResults.max_drawdown * 100).toFixed(2)}%` : 'N/A'}
                  icon={<TrendingDown size={18} />}
                  color={colors.alert}
                />
                <MetricCard
                  label="Volatility"
                  value={backtestResults.volatility ? `${(backtestResults.volatility * 100).toFixed(2)}%` : 'N/A'}
                  icon={<Activity size={18} />}
                  color={colors.accent}
                />
                <MetricCard
                  label="Win Rate"
                  value={backtestResults.win_rate ? `${(backtestResults.win_rate * 100).toFixed(1)}%` : 'N/A'}
                  icon={<Target size={18} />}
                  color={colors.success}
                />
                <MetricCard
                  label="Total Trades"
                  value={backtestResults.total_trades?.toString() || 'N/A'}
                  icon={<BarChart3 size={18} />}
                  color={colors.text}
                />
              </div>
            </div>

            {/* Trade Statistics */}
            {(backtestResults.avg_win !== undefined || backtestResults.avg_loss !== undefined) && (
              <div>
                <h3 className="text-xs font-bold uppercase tracking-wide mb-3" style={{ color: colors.textMuted }}>
                  TRADE STATISTICS
                </h3>
                <div className="grid grid-cols-3 gap-4">
                  <MetricCard
                    label="Profitable Trades"
                    value={backtestResults.profitable_trades?.toString() || 'N/A'}
                    icon={<CheckCircle2 size={18} />}
                    color={colors.success}
                  />
                  <MetricCard
                    label="Average Win"
                    value={backtestResults.avg_win ? `${(backtestResults.avg_win * 100).toFixed(2)}%` : 'N/A'}
                    icon={<TrendingUp size={18} />}
                    color={colors.success}
                  />
                  <MetricCard
                    label="Average Loss"
                    value={backtestResults.avg_loss ? `${(backtestResults.avg_loss * 100).toFixed(2)}%` : 'N/A'}
                    icon={<TrendingDown size={18} />}
                    color={colors.alert}
                  />
                </div>
              </div>
            )}

            {/* Full Results JSON */}
            <div>
              <h3 className="text-xs font-bold uppercase tracking-wide mb-3" style={{ color: colors.textMuted }}>
                <Info size={12} className="inline mr-1" />
                DETAILED RESULTS
              </h3>
              <pre
                className="p-4 rounded text-xs font-mono overflow-x-auto border"
                style={{
                  backgroundColor: colors.panel,
                  color: colors.text,
                  borderColor: 'var(--ft-border-color, #2A2A2A)'
                }}
              >
                {JSON.stringify(backtestResults, null, 2)}
              </pre>
            </div>
          </div>
        ) : (
          <div className="flex items-center justify-center h-full">
            <div className="text-center max-w-md">
              <BarChart3 size={64} color={colors.textMuted} className="mx-auto mb-4" />
              <h3 className="text-base font-bold uppercase tracking-wide mb-2" style={{ color: colors.text }}>
                NO BACKTEST RESULTS
              </h3>
              <p className="text-sm font-mono" style={{ color: colors.textMuted }}>
                Configure your strategy parameters and run a backtest<br/>
                to see performance metrics and trade analysis
              </p>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

function MetricCard({
  label,
  value,
  icon,
  color,
  positive
}: {
  label: string;
  value: string;
  icon: React.ReactNode;
  color: string;
  positive?: boolean;
}) {
  const { colors } = useTerminalTheme();

  return (
    <div
      className="p-4 rounded border"
      style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)' }}
    >
      <div className="flex items-center justify-between mb-2">
        <span className="text-xs uppercase font-mono" style={{ color: colors.textMuted }}>
          {label}
        </span>
        <div style={{ color }}>{icon}</div>
      </div>
      <div className="text-2xl font-bold font-mono flex items-center gap-2" style={{ color }}>
        {value}
        {positive !== undefined && (
          positive ? <TrendingUp size={16} color={colors.success} /> : <TrendingDown size={16} color={colors.alert} />
        )}
      </div>
    </div>
  );
}
