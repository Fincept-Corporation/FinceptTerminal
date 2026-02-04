/**
 * OptimizationMode Component
 * Portfolio optimization and efficient frontier visualization
 */

import React from 'react';
import { Target, TrendingUp, RefreshCw } from 'lucide-react';
import {
  ResponsiveContainer,
  ComposedChart,
  CartesianGrid,
  XAxis,
  YAxis,
  Tooltip,
  Line,
  Scatter,
  Cell,
  PieChart,
  Pie
} from 'recharts';
import { FINCEPT, PIE_COLORS } from '../constants';
import { formatPercent, formatRatio, normalizeWeights } from '../utils';
import { MetricRow } from '../components';
import type { OptimizationModeProps, OptimizationObjective } from '../types';

export const OptimizationMode: React.FC<OptimizationModeProps> = ({
  optimizationType,
  setOptimizationType,
  optimizationObjective,
  setOptimizationObjective,
  optMaxWeight,
  setOptMaxWeight,
  optTargetReturn,
  setOptTargetReturn,
  optAlpha,
  setOptAlpha,
  optLongOnly,
  setOptLongOnly,
  riskFreeRate,
  isLoading,
  optimizationResult,
  frontierResult,
  selectedFrontierIndex,
  setSelectedFrontierIndex,
  runOptimization,
  runEfficientFrontier
}) => {
  return (
    <div
      className="rounded border"
      style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
    >
      <div className="p-4 space-y-4">
        {/* Optimization Type Toggle */}
        <div
          className="flex rounded border overflow-hidden"
          style={{ borderColor: FINCEPT.BORDER }}
        >
          <button
            onClick={() => setOptimizationType('mv')}
            className="flex-1 py-2 text-xs font-bold uppercase tracking-wide"
            style={{
              backgroundColor:
                optimizationType === 'mv' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
              color: optimizationType === 'mv' ? FINCEPT.DARK_BG : FINCEPT.GRAY
            }}
          >
            MEAN-VARIANCE
          </button>
          <button
            onClick={() => setOptimizationType('cvar')}
            className="flex-1 py-2 text-xs font-bold uppercase tracking-wide"
            style={{
              backgroundColor:
                optimizationType === 'cvar' ? FINCEPT.CYAN : FINCEPT.DARK_BG,
              color: optimizationType === 'cvar' ? FINCEPT.DARK_BG : FINCEPT.GRAY
            }}
          >
            MEAN-CVaR
          </button>
        </div>

        {/* Configuration */}
        <div className="grid grid-cols-4 gap-4">
          <div>
            <label
              className="block text-xs font-mono mb-1"
              style={{ color: FINCEPT.GRAY }}
            >
              OBJECTIVE
            </label>
            <select
              value={optimizationObjective}
              onChange={(e) =>
                setOptimizationObjective(e.target.value as OptimizationObjective)
              }
              className="w-full px-3 py-2 rounded text-sm font-mono border"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                borderColor: FINCEPT.BORDER,
                color: FINCEPT.WHITE
              }}
            >
              <option value="min_variance">
                {optimizationType === 'mv' ? 'Min Variance' : 'Min CVaR'}
              </option>
              <option value="max_sharpe">Max Sharpe</option>
              <option value="target_return">Target Return</option>
            </select>
          </div>
          <div>
            <label
              className="block text-xs font-mono mb-1"
              style={{ color: FINCEPT.GRAY }}
            >
              MAX WEIGHT
            </label>
            <input
              type="text"
              inputMode="decimal"
              value={String(optMaxWeight)}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptMaxWeight(parseFloat(v) || 0); }}
              className="w-full px-3 py-2 rounded text-sm font-mono border"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                borderColor: FINCEPT.BORDER,
                color: FINCEPT.WHITE
              }}
            />
          </div>
          {optimizationObjective === 'target_return' && (
            <div>
              <label
                className="block text-xs font-mono mb-1"
                style={{ color: FINCEPT.GRAY }}
              >
                TARGET RETURN
              </label>
              <input
                type="text"
                inputMode="decimal"
                value={String(optTargetReturn)}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptTargetReturn(parseFloat(v) || 0); }}
                className="w-full px-3 py-2 rounded text-sm font-mono border"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  borderColor: FINCEPT.BORDER,
                  color: FINCEPT.WHITE
                }}
              />
            </div>
          )}
          {optimizationType === 'cvar' && (
            <div>
              <label
                className="block text-xs font-mono mb-1"
                style={{ color: FINCEPT.GRAY }}
              >
                CVaR ALPHA
              </label>
              <input
                type="text"
                inputMode="decimal"
                value={String(optAlpha)}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setOptAlpha(parseFloat(v) || 0); }}
                className="w-full px-3 py-2 rounded text-sm font-mono border"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  borderColor: FINCEPT.BORDER,
                  color: FINCEPT.WHITE
                }}
              />
            </div>
          )}
          <div className="flex items-end">
            <label className="flex items-center gap-2 cursor-pointer">
              <input
                type="checkbox"
                checked={optLongOnly}
                onChange={(e) => setOptLongOnly(e.target.checked)}
                className="w-4 h-4"
              />
              <span className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                LONG ONLY
              </span>
            </label>
          </div>
        </div>

        {/* Buttons */}
        <div className="grid grid-cols-2 gap-4">
          <button
            onClick={runOptimization}
            disabled={isLoading}
            className="py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
            style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
          >
            {isLoading ? (
              <RefreshCw size={16} className="animate-spin" />
            ) : (
              <Target size={16} />
            )}
            OPTIMIZE PORTFOLIO
          </button>
          <button
            onClick={runEfficientFrontier}
            disabled={isLoading}
            className="py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
            style={{ backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}
          >
            {isLoading ? (
              <RefreshCw size={16} className="animate-spin" />
            ) : (
              <TrendingUp size={16} />
            )}
            EFFICIENT FRONTIER
          </button>
        </div>

        {/* Optimization Result */}
        {optimizationResult && optimizationResult.success && (
          <OptimalPortfolioDisplay
            optimizationResult={optimizationResult}
            optimizationType={optimizationType}
            optimizationObjective={optimizationObjective}
            optLongOnly={optLongOnly}
          />
        )}

        {/* Efficient Frontier Result */}
        {frontierResult && frontierResult.success && frontierResult.frontier && (
          <EfficientFrontierDisplay
            frontierResult={frontierResult}
            optimizationType={optimizationType}
            selectedFrontierIndex={selectedFrontierIndex}
            setSelectedFrontierIndex={setSelectedFrontierIndex}
          />
        )}
      </div>
    </div>
  );
};

// Sub-component for optimal portfolio display
interface OptimalPortfolioDisplayProps {
  optimizationResult: any;
  optimizationType: 'mv' | 'cvar';
  optimizationObjective: OptimizationObjective;
  optLongOnly: boolean;
}

const OptimalPortfolioDisplay: React.FC<OptimalPortfolioDisplayProps> = ({
  optimizationResult,
  optimizationType,
  optimizationObjective,
  optLongOnly
}) => {
  const normalizedWeights = normalizeWeights(
    optimizationResult.weights,
    optimizationResult.assets
  );

  return (
    <div
      className="p-4 rounded border"
      style={{ backgroundColor: FINCEPT.DARK_BG, borderColor: FINCEPT.BORDER }}
    >
      <h3
        className="text-sm font-bold uppercase mb-4"
        style={{ color: FINCEPT.ORANGE }}
      >
        OPTIMAL PORTFOLIO
      </h3>
      <div className="grid grid-cols-3 gap-6">
        {/* Pie Chart */}
        <div>
          <h4
            className="text-xs font-bold uppercase mb-3"
            style={{ color: FINCEPT.CYAN }}
          >
            ALLOCATION
          </h4>
          <div style={{ height: 180 }}>
            <ResponsiveContainer width="100%" height="100%">
              <PieChart>
                <Pie
                  data={normalizedWeights
                    .filter((item) => item.weight > 0.001)
                    .map((item, idx) => ({
                      name: item.asset,
                      value: Math.round(item.weight * 1000) / 10,
                      weight: item.weight,
                      fill: PIE_COLORS[idx % PIE_COLORS.length]
                    }))}
                  cx="50%"
                  cy="50%"
                  innerRadius={35}
                  outerRadius={60}
                  paddingAngle={2}
                  dataKey="value"
                  stroke={FINCEPT.DARK_BG}
                  strokeWidth={2}
                >
                  {normalizedWeights
                    .filter((item) => item.weight > 0.001)
                    .map((_, idx) => (
                      <Cell key={idx} fill={PIE_COLORS[idx % PIE_COLORS.length]} />
                    ))}
                </Pie>
                <Tooltip
                  contentStyle={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: 4,
                    padding: '6px 10px'
                  }}
                  formatter={(value: number) => [`${value.toFixed(1)}%`, 'Weight']}
                  labelStyle={{
                    color: FINCEPT.WHITE,
                    fontWeight: 'bold',
                    fontSize: 11
                  }}
                  itemStyle={{ color: FINCEPT.GRAY, fontSize: 10 }}
                />
              </PieChart>
            </ResponsiveContainer>
          </div>
          {/* Pie Legend */}
          <div className="mt-2 space-y-1">
            {normalizedWeights
              .filter((item) => item.weight > 0.001)
              .map((item, idx) => (
                <div key={idx} className="flex items-center gap-2">
                  <div
                    className="w-2 h-2 rounded-full"
                    style={{ backgroundColor: PIE_COLORS[idx % PIE_COLORS.length] }}
                  />
                  <span
                    className="text-xs font-mono"
                    style={{ color: FINCEPT.GRAY }}
                  >
                    {item.asset}
                  </span>
                  <span
                    className="text-xs font-mono font-bold ml-auto"
                    style={{ color: FINCEPT.WHITE }}
                  >
                    {(item.weight * 100).toFixed(1)}%
                  </span>
                </div>
              ))}
          </div>
        </div>

        {/* Weight Bars */}
        <div>
          <h4
            className="text-xs font-bold uppercase mb-3"
            style={{ color: FINCEPT.CYAN }}
          >
            WEIGHTS
          </h4>
          <div className="space-y-2">
            {normalizedWeights.map((item, i) => (
              <div key={i} className="flex items-center justify-between">
                <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                  {item.asset}
                </span>
                <div className="flex items-center gap-2">
                  <div
                    className="h-2 rounded"
                    style={{
                      width: `${Math.max(item.weight * 100, 5)}px`,
                      backgroundColor: item.weight > 0 ? FINCEPT.GREEN : FINCEPT.RED
                    }}
                  />
                  <span
                    className="text-sm font-mono font-bold"
                    style={{ color: FINCEPT.WHITE }}
                  >
                    {(item.weight * 100).toFixed(1)}%
                  </span>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Metrics */}
        <div>
          <h4
            className="text-xs font-bold uppercase mb-3"
            style={{ color: FINCEPT.CYAN }}
          >
            METRICS
          </h4>
          <div className="space-y-2">
            <MetricRow
              label="Expected Return"
              value={formatPercent(optimizationResult.expected_return)}
              positive={(optimizationResult.expected_return ?? 0) > 0}
            />
            <MetricRow
              label="Volatility"
              value={formatPercent(optimizationResult.volatility)}
            />
            <MetricRow
              label="Sharpe Ratio"
              value={formatRatio(optimizationResult.sharpe_ratio)}
              positive={(optimizationResult.sharpe_ratio ?? 0) > 0}
            />
            {optimizationResult.cvar !== undefined && (
              <MetricRow
                label="CVaR"
                value={formatPercent(optimizationResult.cvar)}
                negative
              />
            )}
          </div>
          {/* Optimization Info */}
          <div
            className="mt-4 p-2 rounded"
            style={{ backgroundColor: FINCEPT.PANEL_BG }}
          >
            <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
              Type:{' '}
              <span
                style={{
                  color: optimizationType === 'mv' ? FINCEPT.ORANGE : FINCEPT.CYAN
                }}
              >
                {optimizationType === 'mv' ? 'Mean-Variance' : 'Mean-CVaR'}
              </span>
            </p>
            <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
              Objective:{' '}
              <span style={{ color: FINCEPT.WHITE }}>
                {optimizationObjective.replace('_', ' ').toUpperCase()}
              </span>
            </p>
            <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
              Constraints:{' '}
              <span style={{ color: FINCEPT.WHITE }}>
                {optLongOnly ? 'Long Only' : 'Allow Short'}
              </span>
            </p>
          </div>
        </div>
      </div>
    </div>
  );
};

// Sub-component for efficient frontier display
interface EfficientFrontierDisplayProps {
  frontierResult: any;
  optimizationType: 'mv' | 'cvar';
  selectedFrontierIndex: number | null;
  setSelectedFrontierIndex: (value: number | null) => void;
}

const EfficientFrontierDisplay: React.FC<EfficientFrontierDisplayProps> = ({
  frontierResult,
  optimizationType,
  selectedFrontierIndex,
  setSelectedFrontierIndex
}) => {
  const frontier = frontierResult.frontier;

  return (
    <div
      className="p-4 rounded border"
      style={{ backgroundColor: FINCEPT.DARK_BG, borderColor: FINCEPT.BORDER }}
    >
      <h3
        className="text-sm font-bold uppercase mb-4"
        style={{ color: FINCEPT.CYAN }}
      >
        EFFICIENT FRONTIER ({frontier.length} POINTS)
      </h3>

      {/* Chart Visualization */}
      <div className="mb-4" style={{ height: 300 }}>
        <ResponsiveContainer width="100%" height="100%">
          <ComposedChart margin={{ top: 20, right: 30, left: 20, bottom: 20 }}>
            <CartesianGrid
              strokeDasharray="3 3"
              stroke={FINCEPT.BORDER}
              vertical={true}
            />
            <XAxis
              type="number"
              dataKey="risk"
              name={optimizationType === 'mv' ? 'Volatility' : 'CVaR'}
              domain={['auto', 'auto']}
              tickFormatter={(value) => `${(value * 100).toFixed(1)}%`}
              stroke={FINCEPT.GRAY}
              tick={{ fill: FINCEPT.GRAY, fontSize: 10 }}
              label={{
                value: optimizationType === 'mv' ? 'VOLATILITY (%)' : 'CVaR (%)',
                position: 'bottom',
                fill: FINCEPT.GRAY,
                fontSize: 10,
                offset: 0
              }}
            />
            <YAxis
              type="number"
              dataKey="return"
              name="Expected Return"
              domain={['auto', 'auto']}
              tickFormatter={(value) => `${(value * 100).toFixed(1)}%`}
              stroke={FINCEPT.GRAY}
              tick={{ fill: FINCEPT.GRAY, fontSize: 10 }}
              label={{
                value: 'EXPECTED RETURN (%)',
                angle: -90,
                position: 'insideLeft',
                fill: FINCEPT.GRAY,
                fontSize: 10
              }}
            />
            <Tooltip
              contentStyle={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: 4,
                padding: '8px 12px'
              }}
              formatter={(value: number, name: string) => {
                if (name === 'return') return [`${(value * 100).toFixed(2)}%`, 'Return'];
                if (name === 'risk')
                  return [
                    `${(value * 100).toFixed(2)}%`,
                    optimizationType === 'mv' ? 'Volatility' : 'CVaR'
                  ];
                if (name === 'sharpe') return [value.toFixed(3), 'Sharpe'];
                return [value, name];
              }}
              labelStyle={{ color: FINCEPT.WHITE }}
              itemStyle={{ color: FINCEPT.GRAY }}
            />
            {/* Frontier Line */}
            <Line
              data={frontier.map((point: any, idx: number) => ({
                risk: point.volatility ?? point.cvar ?? 0,
                return: point.expected_return,
                sharpe: point.sharpe_ratio ?? 0,
                index: idx
              }))}
              type="monotone"
              dataKey="return"
              stroke={FINCEPT.CYAN}
              strokeWidth={2}
              dot={false}
            />
            {/* Frontier Points */}
            <Scatter
              data={frontier.map((point: any, idx: number) => ({
                risk: point.volatility ?? point.cvar ?? 0,
                return: point.expected_return,
                sharpe: point.sharpe_ratio ?? 0,
                index: idx,
                isMaxSharpe:
                  point.sharpe_ratio ===
                  Math.max(...frontier.map((p: any) => p.sharpe_ratio ?? 0))
              }))}
              fill={FINCEPT.ORANGE}
              onClick={(data: any) => {
                if (data && typeof data.index === 'number') {
                  setSelectedFrontierIndex(data.index);
                }
              }}
              cursor="pointer"
            >
              {frontier.map((point: any, idx: number) => {
                const maxSharpe = Math.max(
                  ...frontier.map((p: any) => p.sharpe_ratio ?? 0)
                );
                const isMaxSharpe = point.sharpe_ratio === maxSharpe;
                const isMinRisk = idx === 0;
                const isSelected = selectedFrontierIndex === idx;
                return (
                  <Cell
                    key={idx}
                    fill={
                      isSelected
                        ? FINCEPT.YELLOW
                        : isMaxSharpe
                        ? FINCEPT.GREEN
                        : isMinRisk
                        ? FINCEPT.ORANGE
                        : FINCEPT.CYAN
                    }
                    stroke={isSelected ? FINCEPT.WHITE : 'none'}
                    strokeWidth={isSelected ? 2 : 0}
                    r={isSelected ? 8 : isMaxSharpe || isMinRisk ? 6 : 4}
                  />
                );
              })}
            </Scatter>
          </ComposedChart>
        </ResponsiveContainer>
      </div>

      {/* Legend */}
      <div className="flex items-center justify-center gap-6 mb-4">
        <div className="flex items-center gap-2">
          <div
            className="w-3 h-3 rounded-full"
            style={{ backgroundColor: FINCEPT.ORANGE }}
          />
          <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
            MIN RISK
          </span>
        </div>
        <div className="flex items-center gap-2">
          <div
            className="w-3 h-3 rounded-full"
            style={{ backgroundColor: FINCEPT.GREEN }}
          />
          <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
            MAX SHARPE
          </span>
        </div>
        <div className="flex items-center gap-2">
          <div
            className="w-3 h-3 rounded-full"
            style={{ backgroundColor: FINCEPT.CYAN }}
          />
          <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
            FRONTIER
          </span>
        </div>
        {selectedFrontierIndex !== null && (
          <div className="flex items-center gap-2">
            <div
              className="w-3 h-3 rounded-full border-2"
              style={{
                backgroundColor: FINCEPT.YELLOW,
                borderColor: FINCEPT.WHITE
              }}
            />
            <span className="text-xs font-mono" style={{ color: FINCEPT.YELLOW }}>
              SELECTED
            </span>
          </div>
        )}
      </div>

      {/* Selected Portfolio Details */}
      {selectedFrontierIndex !== null && frontier[selectedFrontierIndex] && (
        <SelectedPortfolioDetails
          frontierResult={frontierResult}
          selectedFrontierIndex={selectedFrontierIndex}
          setSelectedFrontierIndex={setSelectedFrontierIndex}
          optimizationType={optimizationType}
        />
      )}

      {/* Summary Stats */}
      <div
        className="grid grid-cols-3 gap-4 p-3 rounded"
        style={{ backgroundColor: FINCEPT.PANEL_BG }}
      >
        <div className="text-center">
          <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
            MIN RISK
          </p>
          <p className="text-sm font-bold" style={{ color: FINCEPT.ORANGE }}>
            {(
              (frontier[0]?.volatility ?? frontier[0]?.cvar ?? 0) * 100
            ).toFixed(2)}
            %
          </p>
          <p className="text-xs font-mono" style={{ color: FINCEPT.MUTED }}>
            Return: {(frontier[0]?.expected_return * 100).toFixed(2)}%
          </p>
        </div>
        <div className="text-center">
          <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
            MAX SHARPE
          </p>
          {(() => {
            const maxSharpePoint = frontier.reduce(
              (max: any, p: any) =>
                (p.sharpe_ratio ?? 0) > (max.sharpe_ratio ?? 0) ? p : max,
              frontier[0]
            );
            return (
              <>
                <p className="text-sm font-bold" style={{ color: FINCEPT.GREEN }}>
                  {maxSharpePoint.sharpe_ratio?.toFixed(3) ?? 'N/A'}
                </p>
                <p className="text-xs font-mono" style={{ color: FINCEPT.MUTED }}>
                  Return: {(maxSharpePoint.expected_return * 100).toFixed(2)}%
                </p>
              </>
            );
          })()}
        </div>
        <div className="text-center">
          <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
            MAX RETURN
          </p>
          <p className="text-sm font-bold" style={{ color: FINCEPT.CYAN }}>
            {(frontier[frontier.length - 1]?.expected_return * 100).toFixed(2)}%
          </p>
          <p className="text-xs font-mono" style={{ color: FINCEPT.MUTED }}>
            Risk:{' '}
            {(
              (frontier[frontier.length - 1]?.volatility ??
                frontier[frontier.length - 1]?.cvar ??
                0) * 100
            ).toFixed(2)}
            %
          </p>
        </div>
      </div>
    </div>
  );
};

// Sub-component for selected portfolio details
interface SelectedPortfolioDetailsProps {
  frontierResult: any;
  selectedFrontierIndex: number;
  setSelectedFrontierIndex: (value: number | null) => void;
  optimizationType: 'mv' | 'cvar';
}

const SelectedPortfolioDetails: React.FC<SelectedPortfolioDetailsProps> = ({
  frontierResult,
  selectedFrontierIndex,
  setSelectedFrontierIndex,
  optimizationType
}) => {
  const frontier = frontierResult.frontier;
  const selectedPoint = frontier[selectedFrontierIndex];
  const weights = selectedPoint.weights;

  const getWeightsData = () => {
    if (typeof weights === 'object' && !Array.isArray(weights)) {
      return Object.entries(weights)
        .filter(([_, w]) => (w as number) > 0.001)
        .map(([asset, weight], idx) => ({
          name: asset,
          value: Math.round((weight as number) * 1000) / 10,
          weight: weight as number,
          fill: PIE_COLORS[idx % PIE_COLORS.length]
        }));
    } else if (Array.isArray(weights)) {
      return weights
        .filter((w) => w > 0.001)
        .map((w, idx) => ({
          name: `Asset ${idx + 1}`,
          value: Math.round(w * 1000) / 10,
          weight: w,
          fill: PIE_COLORS[idx % PIE_COLORS.length]
        }));
    }
    return [];
  };

  const weightsData = getWeightsData();

  return (
    <div
      className="mb-4 p-4 rounded border"
      style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.YELLOW }}
    >
      <div className="flex items-center justify-between mb-3">
        <h4
          className="text-sm font-bold uppercase"
          style={{ color: FINCEPT.YELLOW }}
        >
          SELECTED PORTFOLIO (#{selectedFrontierIndex + 1})
        </h4>
        <button
          onClick={() => setSelectedFrontierIndex(null)}
          className="text-xs font-mono px-2 py-1 rounded hover:bg-opacity-80"
          style={{ backgroundColor: FINCEPT.BORDER, color: FINCEPT.GRAY }}
        >
          CLEAR
        </button>
      </div>
      <div className="grid grid-cols-3 gap-6">
        {/* Pie Chart */}
        <div>
          <h5
            className="text-xs font-bold uppercase mb-2"
            style={{ color: FINCEPT.CYAN }}
          >
            ALLOCATION
          </h5>
          <div style={{ height: 180 }}>
            <ResponsiveContainer width="100%" height="100%">
              <PieChart>
                <Pie
                  data={weightsData}
                  cx="50%"
                  cy="50%"
                  innerRadius={35}
                  outerRadius={60}
                  paddingAngle={2}
                  dataKey="value"
                  stroke={FINCEPT.DARK_BG}
                  strokeWidth={2}
                >
                  {weightsData.map((_, idx) => (
                    <Cell key={idx} fill={PIE_COLORS[idx % PIE_COLORS.length]} />
                  ))}
                </Pie>
                <Tooltip
                  contentStyle={{
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: 4,
                    padding: '6px 10px'
                  }}
                  formatter={(value: number) => [`${value.toFixed(1)}%`, 'Weight']}
                  labelStyle={{
                    color: FINCEPT.WHITE,
                    fontWeight: 'bold',
                    fontSize: 11
                  }}
                  itemStyle={{ color: FINCEPT.GRAY, fontSize: 10 }}
                />
              </PieChart>
            </ResponsiveContainer>
          </div>
          {/* Pie Legend */}
          <div className="mt-2 space-y-1">
            {weightsData.map((item, idx) => (
              <div key={idx} className="flex items-center gap-2">
                <div
                  className="w-2 h-2 rounded-full"
                  style={{ backgroundColor: PIE_COLORS[idx % PIE_COLORS.length] }}
                />
                <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                  {item.name}
                </span>
                <span
                  className="text-xs font-mono font-bold ml-auto"
                  style={{ color: FINCEPT.WHITE }}
                >
                  {(item.weight * 100).toFixed(1)}%
                </span>
              </div>
            ))}
          </div>
        </div>

        {/* Weight Bars */}
        <div>
          <h5
            className="text-xs font-bold uppercase mb-2"
            style={{ color: FINCEPT.CYAN }}
          >
            WEIGHTS
          </h5>
          <div className="space-y-1">
            {typeof weights === 'object' && !Array.isArray(weights)
              ? Object.entries(weights).map(([asset, weight]) => (
                  <div key={asset} className="flex items-center justify-between">
                    <span
                      className="text-xs font-mono"
                      style={{ color: FINCEPT.GRAY }}
                    >
                      {asset}
                    </span>
                    <div className="flex items-center gap-2">
                      <div
                        className="h-2 rounded"
                        style={{
                          width: `${Math.max((weight as number) * 80, 3)}px`,
                          backgroundColor:
                            (weight as number) > 0 ? FINCEPT.GREEN : FINCEPT.RED
                        }}
                      />
                      <span
                        className="text-xs font-mono font-bold"
                        style={{ color: FINCEPT.WHITE }}
                      >
                        {((weight as number) * 100).toFixed(1)}%
                      </span>
                    </div>
                  </div>
                ))
              : Array.isArray(weights) &&
                weights.map((w, i) => (
                  <div key={i} className="flex items-center justify-between">
                    <span
                      className="text-xs font-mono"
                      style={{ color: FINCEPT.GRAY }}
                    >
                      Asset {i + 1}
                    </span>
                    <div className="flex items-center gap-2">
                      <div
                        className="h-2 rounded"
                        style={{
                          width: `${Math.max(w * 80, 3)}px`,
                          backgroundColor: w > 0 ? FINCEPT.GREEN : FINCEPT.RED
                        }}
                      />
                      <span
                        className="text-xs font-mono font-bold"
                        style={{ color: FINCEPT.WHITE }}
                      >
                        {(w * 100).toFixed(1)}%
                      </span>
                    </div>
                  </div>
                ))}
          </div>
        </div>

        {/* Metrics */}
        <div>
          <h5
            className="text-xs font-bold uppercase mb-2"
            style={{ color: FINCEPT.CYAN }}
          >
            METRICS
          </h5>
          <div className="space-y-1">
            <div
              className="flex items-center justify-between py-1 border-b"
              style={{ borderColor: FINCEPT.BORDER }}
            >
              <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                Expected Return
              </span>
              <span
                className="text-sm font-mono font-bold"
                style={{ color: FINCEPT.GREEN }}
              >
                {(selectedPoint.expected_return * 100).toFixed(2)}%
              </span>
            </div>
            <div
              className="flex items-center justify-between py-1 border-b"
              style={{ borderColor: FINCEPT.BORDER }}
            >
              <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                {optimizationType === 'mv' ? 'Volatility' : 'CVaR'}
              </span>
              <span
                className="text-sm font-mono font-bold"
                style={{ color: FINCEPT.RED }}
              >
                {(
                  (selectedPoint.volatility ?? selectedPoint.cvar ?? 0) * 100
                ).toFixed(2)}
                %
              </span>
            </div>
            <div className="flex items-center justify-between py-1">
              <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                Sharpe Ratio
              </span>
              <span
                className="text-sm font-mono font-bold"
                style={{ color: FINCEPT.WHITE }}
              >
                {selectedPoint.sharpe_ratio?.toFixed(3) ?? 'N/A'}
              </span>
            </div>
          </div>
          {/* Position on Frontier */}
          <div
            className="mt-4 p-2 rounded"
            style={{ backgroundColor: FINCEPT.DARK_BG }}
          >
            <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
              Position:{' '}
              <span style={{ color: FINCEPT.YELLOW }}>
                {selectedFrontierIndex + 1}
              </span>{' '}
              of {frontier.length}
            </p>
            <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
              {selectedFrontierIndex === 0 ? (
                <span style={{ color: FINCEPT.ORANGE }}>Minimum Risk Portfolio</span>
              ) : selectedFrontierIndex === frontier.length - 1 ? (
                <span style={{ color: FINCEPT.CYAN }}>Maximum Return Portfolio</span>
              ) : selectedPoint.sharpe_ratio ===
                Math.max(...frontier.map((p: any) => p.sharpe_ratio ?? 0)) ? (
                <span style={{ color: FINCEPT.GREEN }}>Maximum Sharpe Portfolio</span>
              ) : (
                <span style={{ color: FINCEPT.MUTED }}>Intermediate Portfolio</span>
              )}
            </p>
          </div>
        </div>
      </div>
    </div>
  );
};
