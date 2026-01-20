/**
 * Portfolio Metrics Panel
 *
 * Displays comprehensive portfolio analytics including returns,
 * risk metrics (Sharpe, Sortino), and performance statistics.
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  TrendingUp, TrendingDown, BarChart3, RefreshCw,
  Target, AlertTriangle, Award, Percent, Loader2,
  ChevronDown, ChevronUp, Info,
} from 'lucide-react';
import {
  alphaArenaEnhancedService,
  type PortfolioMetrics,
} from '../services/alphaArenaEnhancedService';

const COLORS = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#3B82F6',
  YELLOW: '#EAB308',
  PURPLE: '#8B5CF6',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
};

interface PortfolioMetricsPanelProps {
  competitionId: string;
  modelName?: string;
  onModelSelect?: (model: string) => void;
}

interface MetricCardProps {
  label: string;
  value: string | number;
  icon: React.ReactNode;
  color?: string;
  tooltip?: string;
  isPercentage?: boolean;
}

const MetricCard: React.FC<MetricCardProps> = ({
  label,
  value,
  icon,
  color = COLORS.WHITE,
  tooltip,
  isPercentage = false,
}) => {
  const [showTooltip, setShowTooltip] = useState(false);

  const displayValue = typeof value === 'number'
    ? isPercentage
      ? `${(value * 100).toFixed(2)}%`
      : value.toFixed(2)
    : value;

  return (
    <div
      className="p-3 rounded-lg relative"
      style={{ backgroundColor: COLORS.CARD_BG }}
      onMouseEnter={() => setShowTooltip(true)}
      onMouseLeave={() => setShowTooltip(false)}
    >
      <div className="flex items-center justify-between mb-1">
        <span className="text-xs" style={{ color: COLORS.GRAY }}>{label}</span>
        <div style={{ color }}>{icon}</div>
      </div>
      <div className="text-lg font-bold" style={{ color }}>
        {displayValue}
      </div>
      {tooltip && showTooltip && (
        <div
          className="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-2 px-2 py-1 rounded text-xs whitespace-nowrap z-10"
          style={{ backgroundColor: COLORS.BORDER, color: COLORS.WHITE }}
        >
          {tooltip}
        </div>
      )}
    </div>
  );
};

const PortfolioMetricsPanel: React.FC<PortfolioMetricsPanelProps> = ({
  competitionId,
  modelName,
  onModelSelect,
}) => {
  const [metrics, setMetrics] = useState<PortfolioMetrics | null>(null);
  const [allMetrics, setAllMetrics] = useState<Record<string, PortfolioMetrics> | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [selectedModel, setSelectedModel] = useState<string | null>(modelName || null);
  const [expandedSection, setExpandedSection] = useState<string | null>('returns');

  const fetchMetrics = useCallback(async () => {
    if (!competitionId) return;

    setIsLoading(true);
    setError(null);
    try {
      const result = await alphaArenaEnhancedService.getPortfolioMetrics(
        competitionId,
        selectedModel || undefined
      );

      if (result.success && result.metrics) {
        if (selectedModel) {
          setMetrics(result.metrics as PortfolioMetrics);
          setAllMetrics(null);
        } else {
          setAllMetrics(result.metrics as Record<string, PortfolioMetrics>);
          setMetrics(null);
        }
      } else {
        setError(result.error || 'Failed to fetch metrics');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to fetch metrics');
    } finally {
      setIsLoading(false);
    }
  }, [competitionId, selectedModel]);

  useEffect(() => {
    fetchMetrics();
  }, [fetchMetrics]);

  const handleModelChange = (model: string | null) => {
    setSelectedModel(model);
    onModelSelect?.(model || '');
  };

  const toggleSection = (section: string) => {
    setExpandedSection(expandedSection === section ? null : section);
  };

  const getReturnColor = (value: number) => {
    if (value > 0) return COLORS.GREEN;
    if (value < 0) return COLORS.RED;
    return COLORS.GRAY;
  };

  const renderMetricsSection = (metricsData: PortfolioMetrics) => (
    <>
      {/* Returns Section */}
      <div className="border-b" style={{ borderColor: COLORS.BORDER }}>
        <button
          onClick={() => toggleSection('returns')}
          className="w-full px-4 py-2 flex items-center justify-between"
        >
          <div className="flex items-center gap-2">
            <TrendingUp size={14} style={{ color: COLORS.GREEN }} />
            <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>Returns</span>
          </div>
          {expandedSection === 'returns' ? (
            <ChevronUp size={14} style={{ color: COLORS.GRAY }} />
          ) : (
            <ChevronDown size={14} style={{ color: COLORS.GRAY }} />
          )}
        </button>
        {expandedSection === 'returns' && (
          <div className="px-4 pb-3 grid grid-cols-2 gap-2">
            <MetricCard
              label="Total Return"
              value={metricsData.total_return}
              icon={<Percent size={14} />}
              color={getReturnColor(metricsData.total_return)}
              isPercentage
              tooltip="Overall portfolio return"
            />
            <MetricCard
              label="Win Rate"
              value={metricsData.win_rate}
              icon={<Target size={14} />}
              color={metricsData.win_rate > 0.5 ? COLORS.GREEN : COLORS.RED}
              isPercentage
              tooltip="Percentage of winning trades"
            />
            <MetricCard
              label="Profit Factor"
              value={metricsData.profit_factor}
              icon={<Award size={14} />}
              color={metricsData.profit_factor > 1 ? COLORS.GREEN : COLORS.RED}
              tooltip="Gross profit / Gross loss"
            />
            <MetricCard
              label="Expectancy"
              value={`$${metricsData.expectancy.toFixed(2)}`}
              icon={<BarChart3 size={14} />}
              color={metricsData.expectancy > 0 ? COLORS.GREEN : COLORS.RED}
              tooltip="Expected profit per trade"
            />
          </div>
        )}
      </div>

      {/* Risk Metrics Section */}
      <div className="border-b" style={{ borderColor: COLORS.BORDER }}>
        <button
          onClick={() => toggleSection('risk')}
          className="w-full px-4 py-2 flex items-center justify-between"
        >
          <div className="flex items-center gap-2">
            <AlertTriangle size={14} style={{ color: COLORS.YELLOW }} />
            <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>Risk Metrics</span>
          </div>
          {expandedSection === 'risk' ? (
            <ChevronUp size={14} style={{ color: COLORS.GRAY }} />
          ) : (
            <ChevronDown size={14} style={{ color: COLORS.GRAY }} />
          )}
        </button>
        {expandedSection === 'risk' && (
          <div className="px-4 pb-3 grid grid-cols-2 gap-2">
            <MetricCard
              label="Sharpe Ratio"
              value={metricsData.sharpe_ratio}
              icon={<BarChart3 size={14} />}
              color={metricsData.sharpe_ratio > 1 ? COLORS.GREEN : metricsData.sharpe_ratio > 0 ? COLORS.YELLOW : COLORS.RED}
              tooltip="Risk-adjusted return (> 1 is good)"
            />
            <MetricCard
              label="Sortino Ratio"
              value={metricsData.sortino_ratio}
              icon={<BarChart3 size={14} />}
              color={metricsData.sortino_ratio > 1 ? COLORS.GREEN : metricsData.sortino_ratio > 0 ? COLORS.YELLOW : COLORS.RED}
              tooltip="Downside risk-adjusted return"
            />
            <MetricCard
              label="Max Drawdown"
              value={metricsData.max_drawdown_pct}
              icon={<TrendingDown size={14} />}
              color={COLORS.RED}
              isPercentage
              tooltip="Largest peak-to-trough decline"
            />
            <MetricCard
              label="Volatility"
              value={metricsData.volatility}
              icon={<AlertTriangle size={14} />}
              color={metricsData.volatility > 0.3 ? COLORS.RED : COLORS.YELLOW}
              isPercentage
              tooltip="Standard deviation of returns"
            />
          </div>
        )}
      </div>

      {/* Trade Statistics Section */}
      <div>
        <button
          onClick={() => toggleSection('trades')}
          className="w-full px-4 py-2 flex items-center justify-between"
        >
          <div className="flex items-center gap-2">
            <BarChart3 size={14} style={{ color: COLORS.BLUE }} />
            <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>Trade Statistics</span>
          </div>
          {expandedSection === 'trades' ? (
            <ChevronUp size={14} style={{ color: COLORS.GRAY }} />
          ) : (
            <ChevronDown size={14} style={{ color: COLORS.GRAY }} />
          )}
        </button>
        {expandedSection === 'trades' && (
          <div className="px-4 pb-3">
            <div className="grid grid-cols-3 gap-2 mb-3">
              <div className="text-center p-2 rounded" style={{ backgroundColor: COLORS.CARD_BG }}>
                <div className="text-lg font-bold" style={{ color: COLORS.WHITE }}>
                  {metricsData.total_trades}
                </div>
                <div className="text-xs" style={{ color: COLORS.GRAY }}>Total</div>
              </div>
              <div className="text-center p-2 rounded" style={{ backgroundColor: COLORS.GREEN + '10' }}>
                <div className="text-lg font-bold" style={{ color: COLORS.GREEN }}>
                  {metricsData.winning_trades}
                </div>
                <div className="text-xs" style={{ color: COLORS.GRAY }}>Wins</div>
              </div>
              <div className="text-center p-2 rounded" style={{ backgroundColor: COLORS.RED + '10' }}>
                <div className="text-lg font-bold" style={{ color: COLORS.RED }}>
                  {metricsData.losing_trades}
                </div>
                <div className="text-xs" style={{ color: COLORS.GRAY }}>Losses</div>
              </div>
            </div>
            <div className="grid grid-cols-2 gap-2">
              <MetricCard
                label="Avg Win"
                value={`$${metricsData.avg_win.toFixed(2)}`}
                icon={<TrendingUp size={14} />}
                color={COLORS.GREEN}
                tooltip="Average winning trade"
              />
              <MetricCard
                label="Avg Loss"
                value={`$${Math.abs(metricsData.avg_loss).toFixed(2)}`}
                icon={<TrendingDown size={14} />}
                color={COLORS.RED}
                tooltip="Average losing trade"
              />
            </div>
          </div>
        )}
      </div>
    </>
  );

  return (
    <div
      className="rounded-lg overflow-hidden"
      style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}
    >
      {/* Header */}
      <div className="px-4 py-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-2">
          <BarChart3 size={16} style={{ color: COLORS.PURPLE }} />
          <span className="font-semibold text-sm" style={{ color: COLORS.WHITE }}>
            Portfolio Metrics
          </span>
        </div>
        <button
          onClick={fetchMetrics}
          disabled={isLoading}
          className="p-1.5 rounded transition-colors hover:bg-[#1A1A1A]"
        >
          <RefreshCw
            size={14}
            className={isLoading ? 'animate-spin' : ''}
            style={{ color: COLORS.GRAY }}
          />
        </button>
      </div>

      {/* Model Selector */}
      {allMetrics && Object.keys(allMetrics).length > 0 && (
        <div className="px-4 py-2 border-b" style={{ borderColor: COLORS.BORDER }}>
          <select
            value={selectedModel || ''}
            onChange={(e) => handleModelChange(e.target.value || null)}
            className="w-full px-2 py-1.5 rounded text-xs"
            style={{
              backgroundColor: COLORS.CARD_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
            }}
          >
            <option value="">All Models</option>
            {Object.keys(allMetrics).map((model) => (
              <option key={model} value={model}>{model}</option>
            ))}
          </select>
        </div>
      )}

      {/* Content */}
      <div className="max-h-96 overflow-y-auto">
        {error && (
          <div className="px-4 py-3 flex items-center gap-2" style={{ backgroundColor: COLORS.RED + '10' }}>
            <AlertTriangle size={14} style={{ color: COLORS.RED }} />
            <span className="text-xs" style={{ color: COLORS.RED }}>{error}</span>
          </div>
        )}

        {isLoading ? (
          <div className="flex items-center justify-center py-8">
            <Loader2 size={24} className="animate-spin" style={{ color: COLORS.ORANGE }} />
          </div>
        ) : metrics ? (
          renderMetricsSection(metrics)
        ) : allMetrics && Object.keys(allMetrics).length > 0 ? (
          <div className="p-4">
            <div className="text-xs mb-3" style={{ color: COLORS.GRAY }}>
              <Info size={12} className="inline mr-1" />
              Showing overview for {Object.keys(allMetrics).length} models
            </div>
            <div className="space-y-2">
              {Object.entries(allMetrics).map(([model, modelMetrics]) => (
                <button
                  key={model}
                  onClick={() => handleModelChange(model)}
                  className="w-full p-3 rounded flex items-center justify-between transition-colors hover:bg-[#1A1A1A]"
                  style={{ backgroundColor: COLORS.CARD_BG }}
                >
                  <div className="flex items-center gap-2">
                    <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>{model}</span>
                  </div>
                  <div className="flex items-center gap-4 text-xs">
                    <span style={{ color: getReturnColor(modelMetrics.total_return) }}>
                      {(modelMetrics.total_return * 100).toFixed(1)}%
                    </span>
                    <span style={{ color: COLORS.GRAY }}>
                      {modelMetrics.total_trades} trades
                    </span>
                    <span style={{ color: modelMetrics.sharpe_ratio > 1 ? COLORS.GREEN : COLORS.GRAY }}>
                      SR: {modelMetrics.sharpe_ratio.toFixed(2)}
                    </span>
                  </div>
                </button>
              ))}
            </div>
          </div>
        ) : (
          <div className="p-8 text-center">
            <BarChart3 size={32} className="mx-auto mb-2 opacity-30" style={{ color: COLORS.GRAY }} />
            <p className="text-sm" style={{ color: COLORS.GRAY }}>No metrics available</p>
            <p className="text-xs mt-1" style={{ color: COLORS.GRAY }}>
              Start a competition to see portfolio metrics
            </p>
          </div>
        )}
      </div>
    </div>
  );
};

export default PortfolioMetricsPanel;
