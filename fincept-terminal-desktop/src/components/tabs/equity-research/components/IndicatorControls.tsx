// Indicator controls for parameter adjustment and display

import React, { useState } from 'react';
import {
  MomentumIndicatorParams,
  DEFAULT_MOMENTUM_PARAMS,
  IndicatorType,
  MomentumIndicatorResult,
} from '../types';
import { ChevronDown, ChevronUp, Settings, TrendingUp } from 'lucide-react';

interface IndicatorControlsProps {
  params: MomentumIndicatorParams;
  results: MomentumIndicatorResult;
  activeIndicators: Set<IndicatorType>;
  onParamsChange: (params: MomentumIndicatorParams) => void;
  onToggleIndicator: (indicator: IndicatorType) => void;
}

const INDICATOR_CONFIGS: Record<
  IndicatorType,
  { name: string; description: string; color: string }
> = {
  rsi: {
    name: 'RSI',
    description: 'Relative Strength Index',
    color: '#FFA500',
  },
  macd: {
    name: 'MACD',
    description: 'Moving Average Convergence Divergence',
    color: '#2962FF',
  },
  stochastic: {
    name: 'Stochastic',
    description: 'Stochastic Oscillator',
    color: '#00CED1',
  },
  cci: {
    name: 'CCI',
    description: 'Commodity Channel Index',
    color: '#FFD700',
  },
  roc: {
    name: 'ROC',
    description: 'Rate of Change',
    color: '#9370DB',
  },
  williams_r: {
    name: 'Williams %R',
    description: 'Williams Percent Range',
    color: '#00FA9A',
  },
  awesome_oscillator: {
    name: 'Awesome Oscillator',
    description: 'Awesome Oscillator',
    color: '#FF1493',
  },
  tsi: {
    name: 'TSI',
    description: 'True Strength Index',
    color: '#4169E1',
  },
  ultimate_oscillator: {
    name: 'Ultimate Oscillator',
    description: 'Ultimate Oscillator',
    color: '#32CD32',
  },
};

export const IndicatorControls: React.FC<IndicatorControlsProps> = ({
  params,
  results,
  activeIndicators,
  onParamsChange,
  onToggleIndicator,
}) => {
  const [expandedIndicator, setExpandedIndicator] = useState<IndicatorType | null>(null);

  const getSignalBadge = (signal: 'BUY' | 'SELL' | 'NEUTRAL') => {
    const colors = {
      BUY: 'bg-green-500/20 text-green-400 border-green-500/30',
      SELL: 'bg-red-500/20 text-red-400 border-red-500/30',
      NEUTRAL: 'bg-gray-500/20 text-gray-400 border-gray-500/30',
    };

    return (
      <span
        className={`px-2 py-0.5 text-xs font-semibold rounded border ${colors[signal]}`}
      >
        {signal}
      </span>
    );
  };

  const updateParam = (indicator: IndicatorType, key: string, value: number) => {
    const newParams = {
      ...params,
      [indicator]: {
        ...params[indicator],
        [key]: value,
      },
    };
    onParamsChange(newParams);
  };

  const resetParams = (indicator: IndicatorType) => {
    const newParams = {
      ...params,
      [indicator]: DEFAULT_MOMENTUM_PARAMS[indicator],
    };
    onParamsChange(newParams);
  };

  const renderRSIControls = () => {
    const rsiParams = params.rsi || DEFAULT_MOMENTUM_PARAMS.rsi!;
    const rsiResult = results.rsi;

    return (
      <div className="space-y-3">
        <div className="grid grid-cols-3 gap-3">
          <div>
            <label className="text-xs text-gray-400 block mb-1">Period</label>
            <input
              type="number"
              value={rsiParams.period}
              onChange={(e) => updateParam('rsi', 'period', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="2"
              max="100"
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 block mb-1">Overbought</label>
            <input
              type="number"
              value={rsiParams.overbought}
              onChange={(e) => updateParam('rsi', 'overbought', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="50"
              max="100"
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 block mb-1">Oversold</label>
            <input
              type="number"
              value={rsiParams.oversold}
              onChange={(e) => updateParam('rsi', 'oversold', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="0"
              max="50"
            />
          </div>
        </div>
        {rsiResult && (
          <div className="flex items-center justify-between text-sm">
            <span className="text-gray-400">
              Current: <span className="text-white font-semibold">{rsiResult.current_value.toFixed(2)}</span>
            </span>
            {getSignalBadge(rsiResult.signal)}
          </div>
        )}
      </div>
    );
  };

  const renderMACDControls = () => {
    const macdParams = params.macd || DEFAULT_MOMENTUM_PARAMS.macd!;
    const macdResult = results.macd;

    return (
      <div className="space-y-3">
        <div className="grid grid-cols-3 gap-3">
          <div>
            <label className="text-xs text-gray-400 block mb-1">Fast Period</label>
            <input
              type="number"
              value={macdParams.fast_period}
              onChange={(e) => updateParam('macd', 'fast_period', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="2"
              max="50"
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 block mb-1">Slow Period</label>
            <input
              type="number"
              value={macdParams.slow_period}
              onChange={(e) => updateParam('macd', 'slow_period', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="2"
              max="100"
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 block mb-1">Signal Period</label>
            <input
              type="number"
              value={macdParams.signal_period}
              onChange={(e) => updateParam('macd', 'signal_period', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="2"
              max="50"
            />
          </div>
        </div>
        {macdResult && (
          <div className="flex items-center justify-between text-sm">
            <span className="text-gray-400">
              Histogram: <span className="text-white font-semibold">{macdResult.current_histogram.toFixed(4)}</span>
            </span>
            {getSignalBadge(macdResult.signal)}
          </div>
        )}
      </div>
    );
  };

  const renderStochasticControls = () => {
    const stochParams = params.stochastic || DEFAULT_MOMENTUM_PARAMS.stochastic!;
    const stochResult = results.stochastic;

    return (
      <div className="space-y-3">
        <div className="grid grid-cols-4 gap-3">
          <div>
            <label className="text-xs text-gray-400 block mb-1">K Period</label>
            <input
              type="number"
              value={stochParams.k_period}
              onChange={(e) => updateParam('stochastic', 'k_period', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="2"
              max="50"
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 block mb-1">D Period</label>
            <input
              type="number"
              value={stochParams.d_period}
              onChange={(e) => updateParam('stochastic', 'd_period', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="2"
              max="20"
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 block mb-1">Overbought</label>
            <input
              type="number"
              value={stochParams.overbought}
              onChange={(e) => updateParam('stochastic', 'overbought', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="50"
              max="100"
            />
          </div>
          <div>
            <label className="text-xs text-gray-400 block mb-1">Oversold</label>
            <input
              type="number"
              value={stochParams.oversold}
              onChange={(e) => updateParam('stochastic', 'oversold', parseInt(e.target.value))}
              className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
              min="0"
              max="50"
            />
          </div>
        </div>
        {stochResult && (
          <div className="flex items-center justify-between text-sm">
            <span className="text-gray-400">
              %K: <span className="text-white font-semibold">{stochResult.current_k.toFixed(2)}</span>
              {' | '}
              %D: <span className="text-white font-semibold">{stochResult.current_d.toFixed(2)}</span>
            </span>
            {getSignalBadge(stochResult.signal)}
          </div>
        )}
      </div>
    );
  };

  const renderSimpleIndicatorControls = (
    indicator: IndicatorType,
    paramKeys: string[]
  ) => {
    const indicatorParams = params[indicator] || DEFAULT_MOMENTUM_PARAMS[indicator];
    const result = results[indicator];

    if (!indicatorParams) return null;

    return (
      <div className="space-y-3">
        <div className="grid grid-cols-3 gap-3">
          {paramKeys.map((key) => (
            <div key={key}>
              <label className="text-xs text-gray-400 block mb-1 capitalize">
                {key.replace(/_/g, ' ')}
              </label>
              <input
                type="number"
                value={(indicatorParams as any)[key]}
                onChange={(e) => updateParam(indicator, key, parseInt(e.target.value))}
                className="w-full bg-[#1a1a1a] border border-[#333333] rounded px-2 py-1 text-sm text-white"
                min="1"
                max="200"
              />
            </div>
          ))}
        </div>
        {result && 'current_value' in result && (
          <div className="flex items-center justify-between text-sm">
            <span className="text-gray-400">
              Current: <span className="text-white font-semibold">{result.current_value.toFixed(2)}</span>
            </span>
            {getSignalBadge(result.signal)}
          </div>
        )}
      </div>
    );
  };

  const renderIndicatorPanel = (indicator: IndicatorType) => {
    const config = INDICATOR_CONFIGS[indicator];
    const isActive = activeIndicators.has(indicator);
    const isExpanded = expandedIndicator === indicator;

    return (
      <div
        key={indicator}
        className="border border-[#333333] rounded-lg overflow-hidden bg-[#1a1a1a]"
      >
        <div
          className="flex items-center justify-between p-3 cursor-pointer hover:bg-[#222222] transition-colors"
          onClick={() => setExpandedIndicator(isExpanded ? null : indicator)}
        >
          <div className="flex items-center gap-3">
            <input
              type="checkbox"
              checked={isActive}
              onChange={(e) => {
                e.stopPropagation();
                onToggleIndicator(indicator);
              }}
              className="w-4 h-4 rounded border-[#333333] bg-[#0a0a0a] text-orange-500 focus:ring-orange-500"
            />
            <div className="flex items-center gap-2">
              <div
                className="w-3 h-3 rounded-full"
                style={{ backgroundColor: config.color }}
              />
              <div>
                <div className="font-semibold text-white">{config.name}</div>
                <div className="text-xs text-gray-400">{config.description}</div>
              </div>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={(e) => {
                e.stopPropagation();
                resetParams(indicator);
              }}
              className="p-1 hover:bg-[#333333] rounded transition-colors"
              title="Reset to defaults"
            >
              <Settings className="w-4 h-4 text-gray-400" />
            </button>
            {isExpanded ? (
              <ChevronUp className="w-5 h-5 text-gray-400" />
            ) : (
              <ChevronDown className="w-5 h-5 text-gray-400" />
            )}
          </div>
        </div>

        {isExpanded && (
          <div className="p-3 border-t border-[#333333] bg-[#0f0f0f]">
            {indicator === 'rsi' && renderRSIControls()}
            {indicator === 'macd' && renderMACDControls()}
            {indicator === 'stochastic' && renderStochasticControls()}
            {indicator === 'cci' && renderSimpleIndicatorControls('cci', ['period', 'overbought', 'oversold'])}
            {indicator === 'roc' && renderSimpleIndicatorControls('roc', ['period'])}
            {indicator === 'williams_r' && renderSimpleIndicatorControls('williams_r', ['period', 'overbought', 'oversold'])}
            {indicator === 'awesome_oscillator' && renderSimpleIndicatorControls('awesome_oscillator', ['fast_period', 'slow_period'])}
            {indicator === 'tsi' && renderSimpleIndicatorControls('tsi', ['long_period', 'short_period', 'signal_period'])}
            {indicator === 'ultimate_oscillator' && renderSimpleIndicatorControls('ultimate_oscillator', ['period1', 'period2', 'period3', 'weight1', 'weight2', 'weight3'])}
          </div>
        )}
      </div>
    );
  };

  return (
    <div className="space-y-2">
      <div className="flex items-center gap-2 mb-4">
        <TrendingUp className="w-5 h-5 text-orange-500" />
        <h3 className="text-lg font-semibold text-white">Momentum Indicators</h3>
      </div>
      {Object.keys(INDICATOR_CONFIGS).map((indicator) =>
        renderIndicatorPanel(indicator as IndicatorType)
      )}
    </div>
  );
};
