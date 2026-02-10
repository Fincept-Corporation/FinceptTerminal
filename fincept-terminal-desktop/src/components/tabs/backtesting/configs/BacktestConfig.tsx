/**
 * BacktestConfig - Market data, strategy selection, execution settings
 */

import React from 'react';
import { Database, Activity, Settings } from 'lucide-react';
import { FINCEPT, FONT_FAMILY } from '../constants';
import { renderInput, renderSection } from '../components/primitives';
import type { BacktestingState } from '../types';

interface BacktestConfigProps {
  state: BacktestingState;
}

export const BacktestConfig: React.FC<BacktestConfigProps> = ({ state }) => {
  const {
    symbols, setSymbols, startDate, setStartDate, endDate, setEndDate,
    initialCapital, setInitialCapital, commission, setCommission, slippage, setSlippage,
    selectedStrategy, setSelectedStrategy, strategyParams, setStrategyParams,
    customCode, setCustomCode, providerStrategies, selectedCategory,
    providerCategoryInfo, currentStrategy, expandedSections, toggleSection,
    showAdvanced,
  } = state;

  const categoryStrategies = providerStrategies[selectedCategory] || [];

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Market Data */}
      {renderSection('market', 'MARKET DATA', Database,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInput('Symbols', symbols, setSymbols, 'text', 'SPY, AAPL, MSFT')}
          {renderInput('Initial Capital', initialCapital, setInitialCapital, 'number')}
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>,
        expandedSections, toggleSection
      )}

      {/* Strategy Selection */}
      {renderSection('strategy',
        `${providerCategoryInfo[selectedCategory]?.label.toUpperCase() || 'STRATEGIES'}`,
        Activity,
        <>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '6px', marginBottom: '12px' }}>
            {categoryStrategies.map((strategy: any) => (
              <button
                key={strategy.id}
                onClick={() => setSelectedStrategy(strategy.id)}
                style={{
                  padding: '8px 10px',
                  backgroundColor: selectedStrategy === strategy.id ? FINCEPT.ORANGE : 'transparent',
                  color: selectedStrategy === strategy.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  textAlign: 'left',
                  transition: 'all 0.2s',
                  fontFamily: FONT_FAMILY,
                }}
                onMouseEnter={e => {
                  if (selectedStrategy !== strategy.id) {
                    e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                    e.currentTarget.style.color = FINCEPT.WHITE;
                  }
                }}
                onMouseLeave={e => {
                  if (selectedStrategy !== strategy.id) {
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    e.currentTarget.style.color = FINCEPT.GRAY;
                  }
                }}
              >
                <div>{strategy.name}</div>
                <div style={{ fontSize: '7px', color: FINCEPT.MUTED, marginTop: '2px' }}>
                  {strategy.description}
                </div>
              </button>
            ))}
          </div>

          {/* Custom Code */}
          {selectedStrategy === 'code' ? (
            <div>
              <label style={{
                display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY,
                marginBottom: '4px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
              }}>
                CUSTOM PYTHON CODE
              </label>
              <textarea
                value={customCode}
                onChange={e => setCustomCode(e.target.value)}
                rows={12}
                style={{
                  width: '100%', padding: '8px',
                  backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.CYAN,
                  border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                  fontSize: '9px', fontFamily: FONT_FAMILY, outline: 'none', resize: 'vertical',
                }}
                onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
                onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
              />
            </div>
          ) : currentStrategy && currentStrategy.params.length > 0 && (
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
              {currentStrategy.params.map((param: any) => (
                <div key={param.name}>
                  {renderInput(
                    param.label,
                    strategyParams[param.name] ?? param.default,
                    (val: any) => setStrategyParams((prev: any) => ({ ...prev, [param.name]: val })),
                    'number', undefined, param.min, param.max, param.step
                  )}
                </div>
              ))}
            </div>
          )}
        </>,
        expandedSections, toggleSection
      )}

      {/* Execution Settings */}
      {renderSection('execution', 'EXECUTION SETTINGS', Settings,
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
          {renderInput('Commission (%)', commission * 100, (v: number) => setCommission(v / 100), 'number', undefined, 0, 5, 0.01)}
          {renderInput('Slippage (%)', slippage * 100, (v: number) => setSlippage(v / 100), 'number', undefined, 0, 5, 0.01)}
        </div>,
        expandedSections, toggleSection
      )}
    </div>
  );
};
