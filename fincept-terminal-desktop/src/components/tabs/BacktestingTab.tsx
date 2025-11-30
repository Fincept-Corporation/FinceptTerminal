/**
 * Backtesting Tab
 *
 * Dedicated interface for creating, running, and analyzing backtests.
 * Features strategy editor, configuration, results display, and history.
 */

import React, { useState, useEffect } from 'react';
import {
  Activity,
  Play,
  Save,
  FolderOpen,
  TrendingUp,
  TrendingDown,
  BarChart3,
  Calendar,
  DollarSign,
  Settings,
  RefreshCw,
  Download,
  Code,
  Eye,
  Loader,
  CheckCircle,
  AlertCircle,
  FileText,
} from 'lucide-react';
import { backtestingService } from '@/services/backtesting/BacktestingService';
import { backtestingRegistry } from '@/services/backtesting/BacktestingProviderRegistry';
import { sqliteService, type BacktestRun } from '@/services/sqliteService';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { BacktestRequest, BacktestResult } from '@/services/backtesting/interfaces/types';
import { StrategyDefinition } from '@/services/backtesting/interfaces/IStrategyDefinition';

type EditorMode = 'code' | 'visual' | 'template';

export default function BacktestingTab() {
  const { colors } = useTerminalTheme();

  // State
  const [editorMode, setEditorMode] = useState<EditorMode>('code');
  const [strategyCode, setStrategyCode] = useState(DEFAULT_STRATEGY_CODE);
  const [strategyName, setStrategyName] = useState('My Strategy');

  // Configuration
  const [config, setConfig] = useState({
    startDate: new Date(Date.now() - 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    endDate: new Date().toISOString().split('T')[0],
    initialCapital: 100000,
    assets: [{ symbol: 'SPY', assetClass: 'stocks' as const, timeframe: 'daily' as const }],
  });

  // Execution state
  const [isRunning, setIsRunning] = useState(false);
  const [currentResult, setCurrentResult] = useState<BacktestResult | null>(null);
  const [error, setError] = useState<string | null>(null);

  // History
  const [backtestHistory, setBacktestHistory] = useState<BacktestRun[]>([]);
  const [selectedHistoryId, setSelectedHistoryId] = useState<string | null>(null);

  // Provider
  const [activeProvider, setActiveProvider] = useState<string | null>(null);

  useEffect(() => {
    loadBacktestHistory();
    const provider = backtestingRegistry.getActiveProvider();
    setActiveProvider(provider?.name || null);
  }, []);

  const loadBacktestHistory = async () => {
    try {
      const runs = await sqliteService.getBacktestRuns();
      setBacktestHistory(runs);
    } catch (error) {
      console.error('Failed to load backtest history:', error);
    }
  };

  const handleRunBacktest = async () => {
    if (!activeProvider) {
      setError('No backtesting provider active. Please activate a provider in Settings.');
      return;
    }

    setIsRunning(true);
    setError(null);
    setCurrentResult(null);

    try {
      // Build strategy definition
      const strategy: StrategyDefinition = {
        name: strategyName,
        version: '1.0.0',
        description: 'User-created strategy',
        author: 'User',
        type: 'code',
        code: {
          language: 'python',
          source: strategyCode,
        },
        parameters: [],
        requires: {
          dataFeeds: [],
          indicators: [],
          assetClasses: config.assets.map(a => a.assetClass),
        },
      };

      // Build backtest request
      const request: BacktestRequest = {
        strategy,
        startDate: config.startDate,
        endDate: config.endDate,
        initialCapital: config.initialCapital,
        assets: config.assets,
        parameters: {},
      };

      // Run backtest
      const result = await backtestingService.runBacktest(request);
      setCurrentResult(result);

      // Save to database
      await sqliteService.saveBacktestRun({
        id: result.id,
        strategy_id: undefined,
        provider_name: activeProvider,
        config: JSON.stringify(config),
        results: JSON.stringify(result),
        status: result.status,
        performance_metrics: JSON.stringify(result.performance),
        error_message: result.error,
      });

      // Reload history
      await loadBacktestHistory();
    } catch (err) {
      setError(String(err));
      console.error('Backtest failed:', err);
    } finally {
      setIsRunning(false);
    }
  };

  const handleLoadHistory = async (runId: string) => {
    const run = backtestHistory.find((r) => r.id === runId);
    if (run && run.results) {
      try {
        const result = JSON.parse(run.results);
        setCurrentResult(result);
        setSelectedHistoryId(runId);

        // Load config if available
        if (run.config) {
          const savedConfig = JSON.parse(run.config);
          setConfig(savedConfig);
        }
      } catch (error) {
        console.error('Failed to load backtest result:', error);
      }
    }
  };

  const formatPercentage = (value: number) => {
    return (value * 100).toFixed(2) + '%';
  };

  const formatCurrency = (value: number) => {
    return '$' + value.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      background: colors.background,
      color: colors.text,
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        padding: '16px 20px',
        borderBottom: `1px solid ${colors.textMuted}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Activity size={24} style={{ color: colors.accent }} />
          <div>
            <h1 style={{ margin: 0, fontSize: '1.5rem' }}>Backtesting</h1>
            <p style={{ margin: 0, fontSize: '0.85rem', color: colors.textMuted }}>
              {activeProvider ? `Using ${activeProvider}` : 'No provider active'}
            </p>
          </div>
        </div>

        <div style={{ display: 'flex', gap: '8px' }}>
          <button
            onClick={handleRunBacktest}
            disabled={isRunning || !activeProvider}
            style={{
              padding: '8px 16px',
              background: isRunning || !activeProvider ? colors.panel : colors.accent,
              color: 'white',
              border: 'none',
              borderRadius: '6px',
              cursor: isRunning || !activeProvider ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontWeight: '600',
              opacity: isRunning || !activeProvider ? 0.6 : 1,
            }}
          >
            {isRunning ? (
              <>
                <Loader size={16} className="animate-spin" />
                Running...
              </>
            ) : (
              <>
                <Play size={16} />
                Run Backtest
              </>
            )}
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Strategy Editor */}
        <div style={{
          width: '40%',
          borderRight: `1px solid ${colors.textMuted}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}>
          {/* Editor Tabs */}
          <div style={{
            display: 'flex',
            borderBottom: `1px solid ${colors.textMuted}`,
            background: colors.panel,
          }}>
            {(['code', 'visual', 'template'] as EditorMode[]).map((mode) => (
              <button
                key={mode}
                onClick={() => setEditorMode(mode)}
                style={{
                  padding: '12px 20px',
                  background: editorMode === mode ? colors.background : 'transparent',
                  color: editorMode === mode ? colors.text : colors.textMuted,
                  border: 'none',
                  borderBottom: editorMode === mode ? `2px solid ${colors.accent}` : '2px solid transparent',
                  cursor: 'pointer',
                  fontSize: '0.9rem',
                  fontWeight: editorMode === mode ? '600' : '400',
                  textTransform: 'capitalize',
                }}
              >
                {mode}
              </button>
            ))}
          </div>

          {/* Strategy Name */}
          <div style={{ padding: '12px 16px', borderBottom: `1px solid ${colors.textMuted}` }}>
            <input
              type="text"
              value={strategyName}
              onChange={(e) => setStrategyName(e.target.value)}
              placeholder="Strategy Name"
              style={{
                width: '100%',
                padding: '8px 12px',
                background: colors.panel,
                border: `1px solid ${colors.textMuted}`,
                borderRadius: '4px',
                color: colors.text,
                fontSize: '0.9rem',
              }}
            />
          </div>

          {/* Code Editor */}
          {editorMode === 'code' && (
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
              <div style={{ padding: '12px 16px', background: colors.panel, borderBottom: `1px solid ${colors.textMuted}` }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '0.85rem' }}>
                  <Code size={14} />
                  <span>Python Strategy Code</span>
                </div>
              </div>
              <textarea
                value={strategyCode}
                onChange={(e) => setStrategyCode(e.target.value)}
                style={{
                  flex: 1,
                  padding: '16px',
                  background: colors.background,
                  color: colors.text,
                  border: 'none',
                  fontFamily: 'Consolas, Monaco, monospace',
                  fontSize: '0.85rem',
                  resize: 'none',
                  outline: 'none',
                }}
                spellCheck={false}
              />
            </div>
          )}

          {editorMode === 'visual' && (
            <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', color: colors.textMuted }}>
              <div style={{ textAlign: 'center' }}>
                <Eye size={48} style={{ opacity: 0.5, margin: '0 auto 16px' }} />
                <p>Visual strategy builder</p>
                <p style={{ fontSize: '0.85rem' }}>Coming soon...</p>
              </div>
            </div>
          )}

          {editorMode === 'template' && (
            <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', color: colors.textMuted }}>
              <div style={{ textAlign: 'center' }}>
                <FileText size={48} style={{ opacity: 0.5, margin: '0 auto 16px' }} />
                <p>Strategy templates</p>
                <p style={{ fontSize: '0.85rem' }}>Coming soon...</p>
              </div>
            </div>
          )}

          {/* Configuration Panel */}
          <div style={{
            padding: '16px',
            borderTop: `1px solid ${colors.textMuted}`,
            background: colors.panel,
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
              <Settings size={16} />
              <span style={{ fontWeight: '600' }}>Configuration</span>
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <div>
                <label style={{ display: 'block', fontSize: '0.85rem', marginBottom: '4px', color: colors.textMuted }}>
                  Start Date
                </label>
                <input
                  type="date"
                  value={config.startDate}
                  onChange={(e) => setConfig({ ...config, startDate: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px',
                    background: colors.background,
                    border: `1px solid ${colors.textMuted}`,
                    borderRadius: '4px',
                    color: colors.text,
                    fontSize: '0.85rem',
                  }}
                />
              </div>

              <div>
                <label style={{ display: 'block', fontSize: '0.85rem', marginBottom: '4px', color: colors.textMuted }}>
                  End Date
                </label>
                <input
                  type="date"
                  value={config.endDate}
                  onChange={(e) => setConfig({ ...config, endDate: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px',
                    background: colors.background,
                    border: `1px solid ${colors.textMuted}`,
                    borderRadius: '4px',
                    color: colors.text,
                    fontSize: '0.85rem',
                  }}
                />
              </div>

              <div style={{ gridColumn: '1 / -1' }}>
                <label style={{ display: 'block', fontSize: '0.85rem', marginBottom: '4px', color: colors.textMuted }}>
                  Initial Capital
                </label>
                <input
                  type="number"
                  value={config.initialCapital}
                  onChange={(e) => setConfig({ ...config, initialCapital: Number(e.target.value) })}
                  style={{
                    width: '100%',
                    padding: '8px',
                    background: colors.background,
                    border: `1px solid ${colors.textMuted}`,
                    borderRadius: '4px',
                    color: colors.text,
                    fontSize: '0.85rem',
                  }}
                />
              </div>
            </div>
          </div>
        </div>

        {/* Right Panel - Results */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Results Tabs */}
          <div style={{
            display: 'flex',
            borderBottom: `1px solid ${colors.textMuted}`,
            background: colors.panel,
          }}>
            <button
              style={{
                padding: '12px 20px',
                background: colors.background,
                color: colors.text,
                border: 'none',
                borderBottom: `2px solid ${colors.accent}`,
                cursor: 'pointer',
                fontSize: '0.9rem',
                fontWeight: '600',
              }}
            >
              Results
            </button>
            <button
              style={{
                padding: '12px 20px',
                background: 'transparent',
                color: colors.textMuted,
                border: 'none',
                borderBottom: '2px solid transparent',
                cursor: 'pointer',
                fontSize: '0.9rem',
              }}
            >
              History
            </button>
          </div>

          {/* Results Content */}
          <div style={{ flex: 1, overflow: 'auto', padding: '20px' }}>
            {error && (
              <div style={{
                padding: '16px',
                background: colors.alert + '20',
                border: `1px solid ${colors.alert}`,
                borderRadius: '6px',
                marginBottom: '20px',
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
              }}>
                <AlertCircle size={20} color={colors.alert} />
                <div>
                  <div style={{ fontWeight: '600', marginBottom: '4px' }}>Backtest Failed</div>
                  <div style={{ fontSize: '0.85rem', color: colors.textMuted }}>{error}</div>
                </div>
              </div>
            )}

            {currentResult && currentResult.status === 'completed' && (
              <div>
                {/* Performance Metrics */}
                <div style={{ marginBottom: '24px' }}>
                  <h3 style={{ fontSize: '1.1rem', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <TrendingUp size={18} />
                    Performance Metrics
                  </h3>

                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '12px' }}>
                    {[
                      { label: 'Total Return', value: formatPercentage(currentResult.performance.totalReturn), positive: currentResult.performance.totalReturn > 0 },
                      { label: 'Annualized Return', value: formatPercentage(currentResult.performance.annualizedReturn), positive: currentResult.performance.annualizedReturn > 0 },
                      { label: 'Sharpe Ratio', value: currentResult.performance.sharpeRatio.toFixed(2), positive: currentResult.performance.sharpeRatio > 0 },
                      { label: 'Max Drawdown', value: formatPercentage(currentResult.performance.maxDrawdown), positive: false },
                      { label: 'Win Rate', value: formatPercentage(currentResult.performance.winRate), positive: true },
                      { label: 'Total Trades', value: currentResult.performance.totalTrades.toString(), positive: true },
                    ].map((metric) => (
                      <div key={metric.label} style={{
                        padding: '16px',
                        background: colors.panel,
                        border: `1px solid ${colors.textMuted}`,
                        borderRadius: '6px',
                      }}>
                        <div style={{ fontSize: '0.85rem', color: colors.textMuted, marginBottom: '4px' }}>
                          {metric.label}
                        </div>
                        <div style={{
                          fontSize: '1.5rem',
                          fontWeight: 'bold',
                          color: metric.positive ? colors.success : colors.alert,
                        }}>
                          {metric.value}
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                {/* Statistics */}
                <div style={{ marginBottom: '24px' }}>
                  <h3 style={{ fontSize: '1.1rem', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <BarChart3 size={18} />
                    Statistics
                  </h3>

                  <div style={{
                    padding: '16px',
                    background: colors.panel,
                    border: `1px solid ${colors.textMuted}`,
                    borderRadius: '6px',
                  }}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', fontSize: '0.9rem' }}>
                      <div>
                        <span style={{ color: colors.textMuted }}>Initial Capital:</span>
                        <span style={{ float: 'right', fontWeight: '600' }}>
                          {formatCurrency(currentResult.statistics.initialCapital)}
                        </span>
                      </div>
                      <div>
                        <span style={{ color: colors.textMuted }}>Final Capital:</span>
                        <span style={{ float: 'right', fontWeight: '600' }}>
                          {formatCurrency(currentResult.statistics.finalCapital)}
                        </span>
                      </div>
                      <div>
                        <span style={{ color: colors.textMuted }}>Total Fees:</span>
                        <span style={{ float: 'right', fontWeight: '600' }}>
                          {formatCurrency(currentResult.statistics.totalFees)}
                        </span>
                      </div>
                      <div>
                        <span style={{ color: colors.textMuted }}>Total Trades:</span>
                        <span style={{ float: 'right', fontWeight: '600' }}>
                          {currentResult.statistics.totalTrades}
                        </span>
                      </div>
                    </div>
                  </div>
                </div>

                {/* Success Badge */}
                <div style={{
                  padding: '12px',
                  background: colors.success + '20',
                  border: `1px solid ${colors.success}`,
                  borderRadius: '6px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '12px',
                }}>
                  <CheckCircle size={20} color={colors.success} />
                  <span>Backtest completed successfully</span>
                </div>
              </div>
            )}

            {!currentResult && !error && !isRunning && (
              <div style={{
                height: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                color: colors.textMuted,
              }}>
                <div style={{ textAlign: 'center' }}>
                  <Activity size={64} style={{ opacity: 0.3, margin: '0 auto 20px' }} />
                  <p>No backtest results yet</p>
                  <p style={{ fontSize: '0.85rem' }}>Configure your strategy and click "Run Backtest"</p>
                </div>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Footer / Status Bar */}
      <div style={{
        borderTop: `1px solid ${colors.textMuted}`,
        backgroundColor: colors.panel,
        padding: '4px 12px',
        fontSize: '10px',
        color: colors.textMuted,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Backtesting System v1.0 | Strategy testing and optimization platform</span>
            <span>
              {activeProvider ? `Provider: ${activeProvider}` : 'No provider active'} |
              {backtestHistory.length > 0 ? ` History: ${backtestHistory.length} runs` : ' No backtest history'}
            </span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            {currentResult && currentResult.status === 'completed' && (
              <>
                <span>Total Return: {formatPercentage(currentResult.performance.totalReturn)}</span>
                <span>Sharpe: {currentResult.performance.sharpeRatio.toFixed(2)}</span>
                <span>Max DD: {formatPercentage(currentResult.performance.maxDrawdown)}</span>
              </>
            )}
            <span style={{ color: isRunning ? colors.warning : colors.secondary }}>
              {isRunning ? 'RUNNING...' : 'READY'}
            </span>
          </div>
        </div>
      </div>
    </div>
  );
}

const DEFAULT_STRATEGY_CODE = `# Simple Moving Average Crossover Strategy
# This is a basic example - modify as needed

from AlgorithmImports import *

class MyStrategy(QCAlgorithm):
    def Initialize(self):
        self.SetStartDate(2020, 1, 1)
        self.SetEndDate(2023, 12, 31)
        self.SetCash(100000)

        # Add equity
        self.symbol = self.AddEquity("SPY", Resolution.Daily).Symbol

        # Create indicators
        self.fast_sma = self.SMA(self.symbol, 50, Resolution.Daily)
        self.slow_sma = self.SMA(self.symbol, 200, Resolution.Daily)

    def OnData(self, data):
        if not self.fast_sma.IsReady or not self.slow_sma.IsReady:
            return

        # Buy signal: fast SMA crosses above slow SMA
        if self.fast_sma.Current.Value > self.slow_sma.Current.Value:
            if not self.Portfolio.Invested:
                self.SetHoldings(self.symbol, 1.0)

        # Sell signal: fast SMA crosses below slow SMA
        elif self.fast_sma.Current.Value < self.slow_sma.Current.Value:
            if self.Portfolio.Invested:
                self.Liquidate(self.symbol)
`;
