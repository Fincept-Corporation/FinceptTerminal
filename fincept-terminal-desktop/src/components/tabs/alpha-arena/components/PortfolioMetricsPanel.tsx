/**
 * Portfolio Metrics Panel
 *
 * Displays comprehensive portfolio analytics including returns,
 * risk metrics (Sharpe, Sortino), and performance statistics.
 */

import React, { useState } from 'react';
import {
  TrendingUp, TrendingDown, BarChart3, RefreshCw,
  Target, AlertTriangle, Award, Percent, Loader2,
  ChevronDown, ChevronUp, Info,
} from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import { useCache, cacheKey } from '@/hooks/useCache';
import { validateString } from '@/services/core/validators';
import {
  alphaArenaEnhancedService,
  type PortfolioMetrics,
} from '../services/alphaArenaEnhancedService';

const FINCEPT = {
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
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

const TERMINAL_FONT = '"IBM Plex Mono", "Consolas", monospace';

const METRICS_STYLES = `
  .metrics-panel *::-webkit-scrollbar { width: 6px; height: 6px; }
  .metrics-panel *::-webkit-scrollbar-track { background: #000000; }
  .metrics-panel *::-webkit-scrollbar-thumb { background: #2A2A2A; }
  @keyframes metrics-spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
`;

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
  color = FINCEPT.WHITE,
  tooltip,
  isPercentage = false,
}) => {
  const [showTooltip, setShowTooltip] = useState(false);

  const displayValue = typeof value === 'number'
    ? isPercentage
      ? `${((value ?? 0) * 100).toFixed(2)}%`
      : (value ?? 0).toFixed(2)
    : value;

  return (
    <div
      style={{
        padding: '10px',
        position: 'relative',
        backgroundColor: FINCEPT.CARD_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        fontFamily: TERMINAL_FONT,
      }}
      onMouseEnter={() => setShowTooltip(true)}
      onMouseLeave={() => setShowTooltip(false)}
    >
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>{label.toUpperCase()}</span>
        <div style={{ color }}>{icon}</div>
      </div>
      <div style={{ fontSize: '16px', fontWeight: 700, color }}>
        {displayValue}
      </div>
      {tooltip && showTooltip && (
        <div style={{
          position: 'absolute',
          bottom: '100%',
          left: '50%',
          transform: 'translateX(-50%)',
          marginBottom: '8px',
          padding: '4px 8px',
          fontSize: '9px',
          whiteSpace: 'nowrap',
          zIndex: 10,
          backgroundColor: FINCEPT.HEADER_BG,
          color: FINCEPT.WHITE,
          border: `1px solid ${FINCEPT.BORDER}`,
          fontFamily: TERMINAL_FONT,
        }}>
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
  const [selectedModel, setSelectedModel] = useState<string | null>(modelName || null);
  const [expandedSection, setExpandedSection] = useState<string | null>('returns');
  const [hoveredBtn, setHoveredBtn] = useState<string | null>(null);
  const [hoveredModel, setHoveredModel] = useState<string | null>(null);

  // Cache: all models overview (enabled when no model selected)
  const allModelsCache = useCache<Record<string, PortfolioMetrics>>({
    key: cacheKey('alpha-arena', 'metrics', competitionId, 'all'),
    category: 'alpha-arena',
    fetcher: async () => {
      const validId = validateString(competitionId, { minLength: 1 });
      if (!validId.valid) throw new Error(validId.error);
      const result = await alphaArenaEnhancedService.getPortfolioMetrics(competitionId, undefined);
      if (result.success && result.metrics && typeof result.metrics === 'object') {
        return result.metrics as Record<string, PortfolioMetrics>;
      }
      throw new Error(result.error || 'Failed to fetch metrics');
    },
    ttl: 300,
    enabled: !!competitionId && !selectedModel,
    staleWhileRevalidate: true,
  });

  // Cache: single model metrics (enabled when a model is selected)
  const singleModelCache = useCache<PortfolioMetrics>({
    key: cacheKey('alpha-arena', 'metrics', competitionId, selectedModel || ''),
    category: 'alpha-arena',
    fetcher: async () => {
      const validId = validateString(competitionId, { minLength: 1 });
      if (!validId.valid) throw new Error(validId.error);
      const result = await alphaArenaEnhancedService.getPortfolioMetrics(
        competitionId,
        selectedModel || undefined
      );
      if (result.success && result.metrics && typeof result.metrics === 'object') {
        return result.metrics as PortfolioMetrics;
      }
      throw new Error(result.error || 'Failed to fetch metrics');
    },
    ttl: 300,
    enabled: !!competitionId && !!selectedModel,
    staleWhileRevalidate: true,
  });

  const metrics = singleModelCache.data;
  const allMetrics = allModelsCache.data;
  const isLoading = selectedModel ? singleModelCache.isLoading : allModelsCache.isLoading;
  const error = selectedModel ? singleModelCache.error : allModelsCache.error;

  const handleRefresh = () => {
    if (selectedModel) {
      singleModelCache.refresh();
    } else {
      allModelsCache.refresh();
    }
  };

  const handleModelChange = (model: string | null) => {
    const normalizedModel = model && model.length > 0 ? model : null;
    setSelectedModel(normalizedModel);
    onModelSelect?.(normalizedModel || '');
  };

  const toggleSection = (section: string) => {
    setExpandedSection(expandedSection === section ? null : section);
  };

  const getReturnColor = (value: number) => {
    if (value > 0) return FINCEPT.GREEN;
    if (value < 0) return FINCEPT.RED;
    return FINCEPT.GRAY;
  };

  const renderMetricsSection = (metricsData: PortfolioMetrics) => (
    <>
      {/* Returns Section */}
      <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <button
          onClick={() => toggleSection('returns')}
          style={{
            width: '100%',
            padding: '8px 16px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            fontFamily: TERMINAL_FONT,
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <TrendingUp size={14} style={{ color: FINCEPT.GREEN }} />
            <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>RETURNS</span>
          </div>
          {expandedSection === 'returns' ? (
            <ChevronUp size={14} style={{ color: FINCEPT.GRAY }} />
          ) : (
            <ChevronDown size={14} style={{ color: FINCEPT.GRAY }} />
          )}
        </button>
        {expandedSection === 'returns' && (
          <div style={{ padding: '0 16px 12px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <MetricCard
              label="Total Return"
              value={`${(metricsData.total_return_pct ?? 0).toFixed(2)}%`}
              icon={<Percent size={14} />}
              color={getReturnColor(metricsData.total_return_pct ?? 0)}
              tooltip="Overall portfolio return"
            />
            <MetricCard
              label="Win Rate"
              value={`${(metricsData.win_rate ?? 0).toFixed(1)}%`}
              icon={<Target size={14} />}
              color={(metricsData.win_rate ?? 0) > 50 ? FINCEPT.GREEN : FINCEPT.RED}
              tooltip="Percentage of winning trades"
            />
            <MetricCard
              label="Profit Factor"
              value={metricsData.profit_factor ?? 0}
              icon={<Award size={14} />}
              color={(metricsData.profit_factor ?? 0) > 1 ? FINCEPT.GREEN : FINCEPT.RED}
              tooltip="Gross profit / Gross loss"
            />
            <MetricCard
              label="Expectancy"
              value={`$${(metricsData.expectancy ?? 0).toFixed(2)}`}
              icon={<BarChart3 size={14} />}
              color={(metricsData.expectancy ?? 0) > 0 ? FINCEPT.GREEN : FINCEPT.RED}
              tooltip="Expected profit per trade"
            />
          </div>
        )}
      </div>

      {/* Risk Metrics Section */}
      <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <button
          onClick={() => toggleSection('risk')}
          style={{
            width: '100%',
            padding: '8px 16px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            fontFamily: TERMINAL_FONT,
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <AlertTriangle size={14} style={{ color: FINCEPT.YELLOW }} />
            <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>RISK METRICS</span>
          </div>
          {expandedSection === 'risk' ? (
            <ChevronUp size={14} style={{ color: FINCEPT.GRAY }} />
          ) : (
            <ChevronDown size={14} style={{ color: FINCEPT.GRAY }} />
          )}
        </button>
        {expandedSection === 'risk' && (
          <div style={{ padding: '0 16px 12px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <MetricCard
              label="Sharpe Ratio"
              value={metricsData.sharpe_ratio ?? 0}
              icon={<BarChart3 size={14} />}
              color={(metricsData.sharpe_ratio ?? 0) > 1 ? FINCEPT.GREEN : (metricsData.sharpe_ratio ?? 0) > 0 ? FINCEPT.YELLOW : FINCEPT.RED}
              tooltip="Risk-adjusted return (> 1 is good)"
            />
            <MetricCard
              label="Sortino Ratio"
              value={metricsData.sortino_ratio ?? 0}
              icon={<BarChart3 size={14} />}
              color={(metricsData.sortino_ratio ?? 0) > 1 ? FINCEPT.GREEN : (metricsData.sortino_ratio ?? 0) > 0 ? FINCEPT.YELLOW : FINCEPT.RED}
              tooltip="Downside risk-adjusted return"
            />
            <MetricCard
              label="Max Drawdown"
              value={`${(metricsData.max_drawdown_pct ?? 0).toFixed(2)}%`}
              icon={<TrendingDown size={14} />}
              color={FINCEPT.RED}
              tooltip="Largest peak-to-trough decline"
            />
            <MetricCard
              label="Volatility"
              value={metricsData.annualized_volatility ?? 0}
              icon={<AlertTriangle size={14} />}
              color={(metricsData.annualized_volatility ?? 0) > 0.3 ? FINCEPT.RED : FINCEPT.YELLOW}
              isPercentage
              tooltip="Annualized standard deviation of returns"
            />
          </div>
        )}
      </div>

      {/* Trade Statistics Section */}
      <div>
        <button
          onClick={() => toggleSection('trades')}
          style={{
            width: '100%',
            padding: '8px 16px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            fontFamily: TERMINAL_FONT,
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <BarChart3 size={14} style={{ color: FINCEPT.BLUE }} />
            <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>TRADE STATISTICS</span>
          </div>
          {expandedSection === 'trades' ? (
            <ChevronUp size={14} style={{ color: FINCEPT.GRAY }} />
          ) : (
            <ChevronDown size={14} style={{ color: FINCEPT.GRAY }} />
          )}
        </button>
        {expandedSection === 'trades' && (
          <div style={{ padding: '0 16px 12px' }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '12px' }}>
              <div style={{ textAlign: 'center', padding: '8px', backgroundColor: FINCEPT.CARD_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.WHITE }}>
                  {metricsData.total_trades}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Total</div>
              </div>
              <div style={{ textAlign: 'center', padding: '8px', backgroundColor: FINCEPT.GREEN + '10', border: `1px solid ${FINCEPT.BORDER}` }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.GREEN }}>
                  {metricsData.winning_trades}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Wins</div>
              </div>
              <div style={{ textAlign: 'center', padding: '8px', backgroundColor: FINCEPT.RED + '10', border: `1px solid ${FINCEPT.BORDER}` }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.RED }}>
                  {metricsData.losing_trades}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Losses</div>
              </div>
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              <MetricCard
                label="Avg Win"
                value={`$${(metricsData.average_win ?? 0).toFixed(2)}`}
                icon={<TrendingUp size={14} />}
                color={FINCEPT.GREEN}
                tooltip="Average winning trade"
              />
              <MetricCard
                label="Avg Loss"
                value={`$${Math.abs(metricsData.average_loss ?? 0).toFixed(2)}`}
                icon={<TrendingDown size={14} />}
                color={FINCEPT.RED}
                tooltip="Average losing trade"
              />
            </div>
          </div>
        )}
      </div>
    </>
  );

  return (
    <div className="metrics-panel" style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      overflow: 'hidden',
      fontFamily: TERMINAL_FONT,
    }}>
      <style>{METRICS_STYLES}</style>

      {/* Header */}
      <div style={{
        padding: '10px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <BarChart3 size={16} style={{ color: FINCEPT.PURPLE }} />
          <span style={{ fontWeight: 600, fontSize: '12px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            PORTFOLIO METRICS
          </span>
        </div>
        <button
          onClick={handleRefresh}
          disabled={isLoading}
          onMouseEnter={() => setHoveredBtn('refresh')}
          onMouseLeave={() => setHoveredBtn(null)}
          style={{
            padding: '4px',
            backgroundColor: hoveredBtn === 'refresh' ? FINCEPT.HOVER : 'transparent',
            border: 'none',
            cursor: isLoading ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
          }}
        >
          <RefreshCw
            size={14}
            style={{
              color: FINCEPT.GRAY,
              animation: isLoading ? 'metrics-spin 1s linear infinite' : 'none',
            }}
          />
        </button>
      </div>

      {/* Model Selector */}
      {allMetrics && Object.keys(allMetrics).length > 0 && (
        <div style={{ padding: '8px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <select
            value={selectedModel || ''}
            onChange={(e) => handleModelChange(e.target.value || null)}
            style={{
              width: '100%',
              padding: '6px 8px',
              fontSize: '11px',
              fontFamily: TERMINAL_FONT,
              backgroundColor: FINCEPT.CARD_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              outline: 'none',
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
      <div style={{ maxHeight: '384px', overflowY: 'auto' }}>
        {error && (
          <div style={{
            padding: '12px 16px',
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            backgroundColor: FINCEPT.RED + '10',
          }}>
            <AlertTriangle size={14} style={{ color: FINCEPT.RED }} />
            <span style={{ fontSize: '11px', color: FINCEPT.RED, fontFamily: TERMINAL_FONT }}>{error.message}</span>
          </div>
        )}

        {isLoading ? (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '32px 0' }}>
            <Loader2 size={24} style={{ color: FINCEPT.ORANGE, animation: 'metrics-spin 1s linear infinite' }} />
          </div>
        ) : metrics ? (
          renderMetricsSection(metrics)
        ) : allMetrics && Object.keys(allMetrics).length > 0 ? (
          <div style={{ padding: '16px' }}>
            <div style={{ fontSize: '10px', marginBottom: '12px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Info size={12} />
              Showing overview for {Object.keys(allMetrics).length} models
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {Object.entries(allMetrics).map(([model, modelMetrics]) => (
                <button
                  key={model}
                  onClick={() => handleModelChange(model)}
                  onMouseEnter={() => setHoveredModel(model)}
                  onMouseLeave={() => setHoveredModel(null)}
                  style={{
                    width: '100%',
                    padding: '10px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    backgroundColor: hoveredModel === model ? FINCEPT.HOVER : FINCEPT.CARD_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    cursor: 'pointer',
                    fontFamily: TERMINAL_FONT,
                    textAlign: 'left',
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>{model}</span>
                  </div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px' }}>
                    <span style={{ color: getReturnColor(modelMetrics.total_return_pct ?? 0) }}>
                      {(modelMetrics.total_return_pct ?? 0).toFixed(2)}%
                    </span>
                    <span style={{ color: FINCEPT.GRAY }}>
                      {modelMetrics.total_trades ?? 0} trades
                    </span>
                    <span style={{ color: (modelMetrics.sharpe_ratio ?? 0) > 1 ? FINCEPT.GREEN : FINCEPT.GRAY }}>
                      SR: {(modelMetrics.sharpe_ratio ?? 0).toFixed(2)}
                    </span>
                  </div>
                </button>
              ))}
            </div>
          </div>
        ) : (
          <div style={{ padding: '32px 16px', textAlign: 'center' }}>
            <BarChart3 size={32} style={{ color: FINCEPT.GRAY, opacity: 0.3, margin: '0 auto 8px' }} />
            <p style={{ fontSize: '12px', color: FINCEPT.GRAY }}>No metrics available</p>
            <p style={{ fontSize: '10px', marginTop: '4px', color: FINCEPT.GRAY }}>
              Start a competition to see portfolio metrics
            </p>
          </div>
        )}
      </div>
    </div>
  );
};

export default withErrorBoundary(PortfolioMetricsPanel, { name: 'AlphaArena.PortfolioMetricsPanel' });
