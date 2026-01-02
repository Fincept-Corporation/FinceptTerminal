/**
 * FFN Analytics Panel - Portfolio Performance Analysis
 * Bloomberg Professional Design
 * Integrated with FFN library via PyO3
 */

import React, { useState, useEffect } from 'react';
import {
  TrendingUp,
  TrendingDown,
  Activity,
  BarChart2,
  Calendar,
  RefreshCw,
  AlertCircle,
  CheckCircle2,
  ChevronDown,
  ChevronUp,
  LineChart,
  PieChart,
  Percent,
  DollarSign,
  Clock,
  Target
} from 'lucide-react';
import { ffnService, type PerformanceMetrics, type DrawdownInfo, type FFNConfig } from '@/services/aiQuantLab/ffnService';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

// Sample data for demonstration
const SAMPLE_PRICES = {
  '2023-01-03': 100.0,
  '2023-01-04': 101.2,
  '2023-01-05': 99.8,
  '2023-01-06': 102.5,
  '2023-01-09': 103.1,
  '2023-01-10': 104.0,
  '2023-01-11': 102.3,
  '2023-01-12': 105.2,
  '2023-01-13': 106.8,
  '2023-01-17': 105.5,
  '2023-01-18': 107.2,
  '2023-01-19': 106.0,
  '2023-01-20': 108.5,
  '2023-01-23': 109.3,
  '2023-01-24': 110.0,
  '2023-01-25': 108.7,
  '2023-01-26': 111.2,
  '2023-01-27': 112.5,
  '2023-01-30': 111.0,
  '2023-01-31': 113.8
};

interface AnalysisResult {
  performance?: PerformanceMetrics;
  drawdowns?: {
    max_drawdown: number;
    top_drawdowns: DrawdownInfo[];
  };
  riskMetrics?: {
    ulcer_index?: number;
    skewness?: number;
    kurtosis?: number;
    var_95?: number;
    cvar_95?: number;
  };
}

export function FFNAnalyticsPanel() {
  const [isFFNAvailable, setIsFFNAvailable] = useState<boolean | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [analysisResult, setAnalysisResult] = useState<AnalysisResult | null>(null);
  const [priceInput, setPriceInput] = useState(JSON.stringify(SAMPLE_PRICES, null, 2));
  const [config, setConfig] = useState<FFNConfig>({
    risk_free_rate: 0.02,
    annualization_factor: 252,
    drawdown_threshold: 0.05
  });
  const [expandedSections, setExpandedSections] = useState({
    performance: true,
    drawdowns: true,
    risk: true
  });

  // Check FFN availability on mount
  useEffect(() => {
    checkFFNStatus();
  }, []);

  const checkFFNStatus = async () => {
    try {
      const status = await ffnService.checkStatus();
      setIsFFNAvailable(status.available);
      if (!status.available) {
        setError('FFN library not installed. Run: pip install ffn');
      }
    } catch (err) {
      setIsFFNAvailable(false);
      setError('Failed to check FFN status');
    }
  };

  const runAnalysis = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const prices = JSON.parse(priceInput);

      // Run full analysis
      const result = await ffnService.fullAnalysis(prices, config);

      if (!result.success) {
        setError(result.error || 'Analysis failed');
        return;
      }

      // Get additional risk metrics
      const riskResult = await ffnService.riskMetrics(prices, config.risk_free_rate);

      setAnalysisResult({
        performance: result.performance as PerformanceMetrics,
        drawdowns: result.drawdowns,
        riskMetrics: riskResult.success ? {
          ulcer_index: riskResult.ulcer_index,
          skewness: riskResult.skewness,
          kurtosis: riskResult.kurtosis,
          var_95: riskResult.var_95,
          cvar_95: riskResult.cvar_95
        } : undefined
      });
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Analysis failed');
    } finally {
      setIsLoading(false);
    }
  };

  const toggleSection = (section: keyof typeof expandedSections) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  const formatPercent = (value: number | null | undefined, decimals = 2): string => {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return `${(value * 100).toFixed(decimals)}%`;
  };

  const formatRatio = (value: number | null | undefined, decimals = 2): string => {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return value.toFixed(decimals);
  };

  const getValueColor = (value: number | null | undefined, inverse = false): string => {
    if (value === null || value === undefined) return BLOOMBERG.GRAY;
    if (inverse) return value < 0 ? BLOOMBERG.GREEN : value > 0 ? BLOOMBERG.RED : BLOOMBERG.WHITE;
    return value > 0 ? BLOOMBERG.GREEN : value < 0 ? BLOOMBERG.RED : BLOOMBERG.WHITE;
  };

  // Loading/Error states
  if (isFFNAvailable === null) {
    return (
      <div className="flex items-center justify-center h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
        <RefreshCw size={32} color={BLOOMBERG.ORANGE} className="animate-spin" />
      </div>
    );
  }

  if (!isFFNAvailable) {
    return (
      <div className="flex items-center justify-center h-full p-8" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
        <div className="text-center max-w-md">
          <AlertCircle size={48} color={BLOOMBERG.RED} className="mx-auto mb-4" />
          <h3 className="text-lg font-bold uppercase mb-2" style={{ color: BLOOMBERG.WHITE }}>
            FFN Library Not Installed
          </h3>
          <code
            className="block p-4 rounded text-sm font-mono mt-4"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG, color: BLOOMBERG.ORANGE }}
          >
            pip install ffn
          </code>
          <button
            onClick={checkFFNStatus}
            className="mt-6 px-6 py-2 rounded font-bold text-sm uppercase flex items-center gap-2 mx-auto"
            style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
          >
            <RefreshCw size={14} />
            Check Again
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="flex h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
      {/* Left Panel - Input */}
      <div
        className="w-80 flex flex-col border-r"
        style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}
      >
        {/* Input Header */}
        <div
          className="px-4 py-3 border-b flex items-center gap-2"
          style={{ backgroundColor: BLOOMBERG.HEADER_BG, borderColor: BLOOMBERG.BORDER }}
        >
          <LineChart size={16} color={BLOOMBERG.ORANGE} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
            Price Data Input
          </span>
        </div>

        {/* Price Input */}
        <div className="flex-1 p-4 overflow-auto">
          <label className="block text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
            PRICE DATA (JSON)
          </label>
          <textarea
            value={priceInput}
            onChange={(e) => setPriceInput(e.target.value)}
            className="w-full h-48 p-3 rounded text-xs font-mono resize-none"
            style={{
              backgroundColor: BLOOMBERG.DARK_BG,
              color: BLOOMBERG.WHITE,
              border: `1px solid ${BLOOMBERG.BORDER}`
            }}
            placeholder='{"2023-01-01": 100, "2023-01-02": 101, ...}'
          />

          {/* Config Options */}
          <div className="mt-4 space-y-3">
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                RISK-FREE RATE
              </label>
              <input
                type="number"
                step="0.01"
                value={config.risk_free_rate}
                onChange={(e) => setConfig({ ...config, risk_free_rate: parseFloat(e.target.value) || 0 })}
                className="w-full p-2 rounded text-xs font-mono"
                style={{
                  backgroundColor: BLOOMBERG.DARK_BG,
                  color: BLOOMBERG.WHITE,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}
              />
            </div>

            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                DRAWDOWN THRESHOLD
              </label>
              <input
                type="number"
                step="0.01"
                value={config.drawdown_threshold}
                onChange={(e) => setConfig({ ...config, drawdown_threshold: parseFloat(e.target.value) || 0.05 })}
                className="w-full p-2 rounded text-xs font-mono"
                style={{
                  backgroundColor: BLOOMBERG.DARK_BG,
                  color: BLOOMBERG.WHITE,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}
              />
            </div>
          </div>
        </div>

        {/* Run Button */}
        <div className="p-4 border-t" style={{ borderColor: BLOOMBERG.BORDER }}>
          <button
            onClick={runAnalysis}
            disabled={isLoading}
            className="w-full py-3 rounded font-bold text-sm uppercase flex items-center justify-center gap-2 disabled:opacity-50"
            style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
          >
            {isLoading ? (
              <>
                <RefreshCw size={16} className="animate-spin" />
                ANALYZING...
              </>
            ) : (
              <>
                <Activity size={16} />
                RUN ANALYSIS
              </>
            )}
          </button>
        </div>
      </div>

      {/* Right Panel - Results */}
      <div className="flex-1 overflow-auto p-4">
        {error && (
          <div
            className="mb-4 p-4 rounded flex items-center gap-3"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderLeft: `3px solid ${BLOOMBERG.RED}` }}
          >
            <AlertCircle size={20} color={BLOOMBERG.RED} />
            <span className="text-sm font-mono" style={{ color: BLOOMBERG.RED }}>{error}</span>
          </div>
        )}

        {!analysisResult && !error && (
          <div className="flex items-center justify-center h-full">
            <div className="text-center">
              <BarChart2 size={64} color={BLOOMBERG.MUTED} className="mx-auto mb-4" />
              <p className="text-sm font-mono" style={{ color: BLOOMBERG.GRAY }}>
                Enter price data and click "Run Analysis" to see results
              </p>
            </div>
          </div>
        )}

        {analysisResult && (
          <div className="space-y-4">
            {/* Performance Metrics Section */}
            <div
              className="rounded overflow-hidden"
              style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
            >
              <button
                onClick={() => toggleSection('performance')}
                className="w-full px-4 py-3 flex items-center justify-between"
                style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
              >
                <div className="flex items-center gap-2">
                  <TrendingUp size={16} color={BLOOMBERG.GREEN} />
                  <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                    Performance Metrics
                  </span>
                </div>
                {expandedSections.performance ? (
                  <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                ) : (
                  <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                )}
              </button>

              {expandedSections.performance && analysisResult.performance && (
                <div className="p-4 grid grid-cols-3 gap-4">
                  <MetricCard
                    label="Total Return"
                    value={formatPercent(analysisResult.performance.total_return)}
                    color={getValueColor(analysisResult.performance.total_return)}
                    icon={<Percent size={14} />}
                  />
                  <MetricCard
                    label="CAGR"
                    value={formatPercent(analysisResult.performance.cagr)}
                    color={getValueColor(analysisResult.performance.cagr)}
                    icon={<TrendingUp size={14} />}
                  />
                  <MetricCard
                    label="Sharpe Ratio"
                    value={formatRatio(analysisResult.performance.sharpe_ratio)}
                    color={getValueColor(analysisResult.performance.sharpe_ratio)}
                    icon={<Target size={14} />}
                  />
                  <MetricCard
                    label="Sortino Ratio"
                    value={formatRatio(analysisResult.performance.sortino_ratio)}
                    color={getValueColor(analysisResult.performance.sortino_ratio)}
                    icon={<Target size={14} />}
                  />
                  <MetricCard
                    label="Volatility"
                    value={formatPercent(analysisResult.performance.volatility)}
                    color={BLOOMBERG.YELLOW}
                    icon={<Activity size={14} />}
                  />
                  <MetricCard
                    label="Calmar Ratio"
                    value={formatRatio(analysisResult.performance.calmar_ratio)}
                    color={getValueColor(analysisResult.performance.calmar_ratio)}
                    icon={<BarChart2 size={14} />}
                  />
                  <MetricCard
                    label="Best Day"
                    value={formatPercent(analysisResult.performance.best_day)}
                    color={BLOOMBERG.GREEN}
                    icon={<TrendingUp size={14} />}
                  />
                  <MetricCard
                    label="Worst Day"
                    value={formatPercent(analysisResult.performance.worst_day)}
                    color={BLOOMBERG.RED}
                    icon={<TrendingDown size={14} />}
                  />
                  <MetricCard
                    label="Daily Mean"
                    value={formatPercent(analysisResult.performance.daily_mean, 4)}
                    color={getValueColor(analysisResult.performance.daily_mean)}
                    icon={<LineChart size={14} />}
                  />
                </div>
              )}
            </div>

            {/* Drawdowns Section */}
            <div
              className="rounded overflow-hidden"
              style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
            >
              <button
                onClick={() => toggleSection('drawdowns')}
                className="w-full px-4 py-3 flex items-center justify-between"
                style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
              >
                <div className="flex items-center gap-2">
                  <TrendingDown size={16} color={BLOOMBERG.RED} />
                  <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                    Drawdown Analysis
                  </span>
                </div>
                {expandedSections.drawdowns ? (
                  <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                ) : (
                  <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                )}
              </button>

              {expandedSections.drawdowns && analysisResult.drawdowns && (
                <div className="p-4">
                  <div className="mb-4">
                    <MetricCard
                      label="Max Drawdown"
                      value={formatPercent(analysisResult.drawdowns.max_drawdown)}
                      color={BLOOMBERG.RED}
                      icon={<TrendingDown size={14} />}
                      large
                    />
                  </div>

                  {analysisResult.drawdowns.top_drawdowns && analysisResult.drawdowns.top_drawdowns.length > 0 && (
                    <div>
                      <h4 className="text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
                        TOP DRAWDOWN PERIODS
                      </h4>
                      <div className="space-y-2">
                        {analysisResult.drawdowns.top_drawdowns.map((dd, idx) => (
                          <div
                            key={idx}
                            className="p-3 rounded flex items-center justify-between"
                            style={{ backgroundColor: BLOOMBERG.DARK_BG }}
                          >
                            <div className="flex items-center gap-3">
                              <span
                                className="w-6 h-6 rounded-full flex items-center justify-center text-xs font-bold"
                                style={{ backgroundColor: BLOOMBERG.RED, color: BLOOMBERG.WHITE }}
                              >
                                {idx + 1}
                              </span>
                              <div>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.WHITE }}>
                                  {dd.start} → {dd.end}
                                </span>
                              </div>
                            </div>
                            <span className="text-sm font-bold" style={{ color: BLOOMBERG.RED }}>
                              {formatPercent(dd.drawdown)}
                            </span>
                          </div>
                        ))}
                      </div>
                    </div>
                  )}
                </div>
              )}
            </div>

            {/* Risk Metrics Section */}
            <div
              className="rounded overflow-hidden"
              style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
            >
              <button
                onClick={() => toggleSection('risk')}
                className="w-full px-4 py-3 flex items-center justify-between"
                style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
              >
                <div className="flex items-center gap-2">
                  <AlertCircle size={16} color={BLOOMBERG.YELLOW} />
                  <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                    Risk Metrics
                  </span>
                </div>
                {expandedSections.risk ? (
                  <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                ) : (
                  <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                )}
              </button>

              {expandedSections.risk && analysisResult.riskMetrics && (
                <div className="p-4 grid grid-cols-3 gap-4">
                  <MetricCard
                    label="Ulcer Index"
                    value={formatRatio(analysisResult.riskMetrics.ulcer_index, 4)}
                    color={BLOOMBERG.YELLOW}
                    icon={<Activity size={14} />}
                  />
                  <MetricCard
                    label="Skewness"
                    value={formatRatio(analysisResult.riskMetrics.skewness)}
                    color={BLOOMBERG.CYAN}
                    icon={<BarChart2 size={14} />}
                  />
                  <MetricCard
                    label="Kurtosis"
                    value={formatRatio(analysisResult.riskMetrics.kurtosis)}
                    color={BLOOMBERG.CYAN}
                    icon={<BarChart2 size={14} />}
                  />
                  <MetricCard
                    label="VaR (95%)"
                    value={formatPercent(analysisResult.riskMetrics.var_95)}
                    color={BLOOMBERG.RED}
                    icon={<TrendingDown size={14} />}
                  />
                  <MetricCard
                    label="CVaR (95%)"
                    value={formatPercent(analysisResult.riskMetrics.cvar_95)}
                    color={BLOOMBERG.RED}
                    icon={<TrendingDown size={14} />}
                  />
                </div>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

// Metric Card Component
function MetricCard({
  label,
  value,
  color,
  icon,
  large = false
}: {
  label: string;
  value: string;
  color: string;
  icon: React.ReactNode;
  large?: boolean;
}) {
  return (
    <div
      className={`p-3 rounded ${large ? 'col-span-3' : ''}`}
      style={{ backgroundColor: BLOOMBERG.DARK_BG }}
    >
      <div className="flex items-center gap-2 mb-1">
        <span style={{ color: BLOOMBERG.GRAY }}>{icon}</span>
        <span className="text-xs font-mono uppercase" style={{ color: BLOOMBERG.GRAY }}>
          {label}
        </span>
      </div>
      <span
        className={`font-bold font-mono ${large ? 'text-2xl' : 'text-lg'}`}
        style={{ color }}
      >
        {value}
      </span>
    </div>
  );
}
