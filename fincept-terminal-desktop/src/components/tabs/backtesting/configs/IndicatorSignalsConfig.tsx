/**
 * IndicatorSignalsConfig - Signal mode selection and mode-specific params
 */

import React from 'react';
import { Database, Zap, Activity } from 'lucide-react';
import { FINCEPT, FONT_FAMILY, SIGNAL_MODES } from '../constants';
import { renderInput, renderSection, renderInlineSelect } from '../components/primitives';
import type { BacktestingState } from '../types';

interface IndicatorSignalsConfigProps {
  state: BacktestingState;
}

export const IndicatorSignalsConfig: React.FC<IndicatorSignalsConfigProps> = ({ state }) => {
  const {
    symbols, setSymbols, startDate, setStartDate, endDate, setEndDate,
    indSignalMode, setIndSignalMode, indSignalParams, setIndSignalParams,
    selectedIndicator, setSelectedIndicator,
    expandedSections, toggleSection,
  } = state;

  const updateParam = (key: string, value: any) =>
    setIndSignalParams((prev: any) => ({ ...prev, [key]: value }));

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {renderSection('market', 'MARKET DATA', Database,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInput('Symbols', symbols, setSymbols, 'text')}
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>,
        expandedSections, toggleSection
      )}

      {renderSection('signal_mode', 'SIGNAL MODE', Zap,
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '6px' }}>
          {SIGNAL_MODES.map(mode => (
            <button
              key={mode.id}
              onClick={() => { setIndSignalMode(mode.id); setIndSignalParams({}); }}
              style={{
                padding: '8px',
                backgroundColor: indSignalMode === mode.id ? FINCEPT.YELLOW : 'transparent',
                color: indSignalMode === mode.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                fontSize: '9px', fontWeight: 700, cursor: 'pointer',
                fontFamily: FONT_FAMILY, textAlign: 'left',
              }}
            >
              <div>{mode.label}</div>
              <div style={{ fontSize: '7px', fontWeight: 400, marginTop: '2px', opacity: 0.7 }}>
                {mode.description}
              </div>
            </button>
          ))}
        </div>,
        expandedSections, toggleSection
      )}

      {/* Mode-specific parameters */}
      {indSignalMode === 'crossover_signals' && renderSection('crossover_params', 'CROSSOVER PARAMETERS', Activity,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInlineSelect('Fast Indicator', indSignalParams.fast_indicator ?? 'ma', (v) => updateParam('fast_indicator', v),
            [{ value: 'ma', label: 'SMA' }, { value: 'ema', label: 'EMA' }])}
          {renderInput('Fast Period', indSignalParams.fast_period ?? 10, (v: any) => updateParam('fast_period', v), 'number')}
          {renderInlineSelect('Slow Indicator', indSignalParams.slow_indicator ?? 'ma', (v) => updateParam('slow_indicator', v),
            [{ value: 'ma', label: 'SMA' }, { value: 'ema', label: 'EMA' }])}
          {renderInput('Slow Period', indSignalParams.slow_period ?? 20, (v: any) => updateParam('slow_period', v), 'number')}
        </div>,
        expandedSections, toggleSection
      )}

      {indSignalMode === 'threshold_signals' && renderSection('threshold_params', 'THRESHOLD PARAMETERS', Activity,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          {renderInlineSelect('Indicator', selectedIndicator, setSelectedIndicator,
            [{ value: 'rsi', label: 'RSI' }, { value: 'cci', label: 'CCI' },
             { value: 'williams_r', label: 'Williams %R' }, { value: 'stoch', label: 'Stochastic' }])}
          {renderInput('Period', indSignalParams.period ?? 14, (v: any) => updateParam('period', v), 'number')}
          {renderInput('Lower Threshold', indSignalParams.lower ?? 30, (v: any) => updateParam('lower', v), 'number')}
          {renderInput('Upper Threshold', indSignalParams.upper ?? 70, (v: any) => updateParam('upper', v), 'number')}
        </div>,
        expandedSections, toggleSection
      )}

      {indSignalMode === 'breakout_signals' && renderSection('breakout_params', 'BREAKOUT PARAMETERS', Activity,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInlineSelect('Channel Type', indSignalParams.channel ?? 'donchian', (v) => updateParam('channel', v),
            [{ value: 'donchian', label: 'Donchian Channel' }, { value: 'bbands', label: 'Bollinger Bands' },
             { value: 'keltner', label: 'Keltner Channel' }])}
          {renderInput('Period', indSignalParams.period ?? 20, (v: any) => updateParam('period', v), 'number')}
        </div>,
        expandedSections, toggleSection
      )}

      {indSignalMode === 'mean_reversion_signals' && renderSection('mean_reversion_params', 'MEAN REVERSION PARAMETERS', Activity,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
          {renderInput('Period', indSignalParams.period ?? 20, (v: any) => updateParam('period', v), 'number')}
          {renderInput('Z-Score Entry', indSignalParams.z_entry ?? 2.0, (v: any) => updateParam('z_entry', v), 'number')}
          {renderInput('Z-Score Exit', indSignalParams.z_exit ?? 0.0, (v: any) => updateParam('z_exit', v), 'number')}
        </div>,
        expandedSections, toggleSection
      )}

      {indSignalMode === 'signal_filter' && renderSection('filter_params', 'SIGNAL FILTER PARAMETERS', Activity,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInlineSelect('Base Indicator', indSignalParams.base_indicator ?? 'rsi', (v) => updateParam('base_indicator', v),
            [{ value: 'rsi', label: 'RSI' }, { value: 'cci', label: 'CCI' },
             { value: 'williams_r', label: 'Williams %R' }, { value: 'momentum', label: 'Momentum' },
             { value: 'stoch', label: 'Stochastic' }, { value: 'macd', label: 'MACD' }])}
          {renderInput('Base Period', indSignalParams.base_period ?? 14, (v: any) => updateParam('base_period', v), 'number')}
          {renderInlineSelect('Filter Indicator', indSignalParams.filter_indicator ?? 'adx', (v) => updateParam('filter_indicator', v),
            [{ value: 'adx', label: 'ADX' }, { value: 'atr', label: 'ATR' },
             { value: 'mstd', label: 'Moving Std Dev' }, { value: 'zscore', label: 'Z-Score' },
             { value: 'rsi', label: 'RSI' }])}
          {renderInput('Filter Period', indSignalParams.filter_period ?? 14, (v: any) => updateParam('filter_period', v), 'number')}
          {renderInput('Filter Threshold', indSignalParams.filter_threshold ?? 25, (v: any) => updateParam('filter_threshold', v), 'number')}
          {renderInlineSelect('Filter Type', indSignalParams.filter_type ?? 'above', (v) => updateParam('filter_type', v),
            [{ value: 'above', label: 'Above Threshold' }, { value: 'below', label: 'Below Threshold' }])}
        </div>,
        expandedSections, toggleSection
      )}
    </div>
  );
};
