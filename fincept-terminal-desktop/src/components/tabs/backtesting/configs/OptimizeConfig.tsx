/**
 * OptimizeConfig - Optimization settings, parameter ranges
 */

import React from 'react';
import { Database, Target } from 'lucide-react';
import { FINCEPT, FONT_FAMILY } from '../constants';
import { renderInput, renderSection } from '../components/primitives';
import type { BacktestingState } from '../types';

interface OptimizeConfigProps {
  state: BacktestingState;
}

export const OptimizeConfig: React.FC<OptimizeConfigProps> = ({ state }) => {
  const {
    symbols, setSymbols, startDate, setStartDate, endDate, setEndDate,
    initialCapital, setInitialCapital, selectedStrategy, setSelectedStrategy,
    providerStrategies, selectedCategory, currentStrategy,
    optimizeObjective, setOptimizeObjective, optimizeMethod, setOptimizeMethod,
    maxIterations, setMaxIterations, paramRanges, setParamRanges,
    expandedSections, toggleSection,
  } = state;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {renderSection('market', 'MARKET DATA', Database,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInput('Symbols', symbols, setSymbols, 'text', 'SPY, AAPL')}
          {renderInput('Initial Capital', initialCapital, setInitialCapital, 'number')}
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>,
        expandedSections, toggleSection
      )}

      {renderSection('strategy', 'STRATEGY', Target,
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '6px' }}>
          {(providerStrategies[selectedCategory] || []).map((strategy: any) => (
            <button
              key={strategy.id}
              onClick={() => setSelectedStrategy(strategy.id)}
              style={{
                padding: '8px 10px',
                backgroundColor: selectedStrategy === strategy.id ? FINCEPT.GREEN : 'transparent',
                color: selectedStrategy === strategy.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                fontSize: '9px', fontWeight: 700, cursor: 'pointer',
                textAlign: 'left', fontFamily: FONT_FAMILY,
              }}
            >
              {strategy.name}
            </button>
          ))}
        </div>,
        expandedSections, toggleSection
      )}

      {renderSection('optimization', 'OPTIMIZATION SETTINGS', Target,
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
            <div>
              <label style={{
                display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY,
                marginBottom: '4px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
              }}>OBJECTIVE</label>
              <select
                value={optimizeObjective}
                onChange={e => setOptimizeObjective(e.target.value as any)}
                style={{
                  width: '100%', padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                  fontSize: '10px', fontFamily: FONT_FAMILY, outline: 'none',
                }}
              >
                <option value="sharpe">Sharpe Ratio</option>
                <option value="sortino">Sortino Ratio</option>
                <option value="calmar">Calmar Ratio</option>
                <option value="return">Total Return</option>
              </select>
            </div>
            <div>
              <label style={{
                display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY,
                marginBottom: '4px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
              }}>METHOD</label>
              <select
                value={optimizeMethod}
                onChange={e => setOptimizeMethod(e.target.value as any)}
                style={{
                  width: '100%', padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                  fontSize: '10px', fontFamily: FONT_FAMILY, outline: 'none',
                }}
              >
                <option value="grid">Grid Search</option>
                <option value="random">Random Search</option>
              </select>
            </div>
            {renderInput('Max Iterations', maxIterations, setMaxIterations, 'number', undefined, 10, 10000)}
          </div>

          {/* Parameter Ranges */}
          {currentStrategy && currentStrategy.params.length > 0 && (
            <div style={{ marginTop: '8px' }}>
              <div style={{
                fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN,
                marginBottom: '6px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
              }}>PARAMETER RANGES</div>
              {currentStrategy.params.map((param: any) => (
                <div key={param.name} style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px', marginBottom: '6px' }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, paddingTop: '20px', fontFamily: FONT_FAMILY }}>
                    {param.label}
                  </div>
                  {renderInput('Min', paramRanges[param.name]?.min ?? param.min ?? 0,
                    (v: number) => setParamRanges((prev: any) => ({ ...prev, [param.name]: { ...prev[param.name], min: v } })), 'number')}
                  {renderInput('Max', paramRanges[param.name]?.max ?? param.max ?? 100,
                    (v: number) => setParamRanges((prev: any) => ({ ...prev, [param.name]: { ...prev[param.name], max: v } })), 'number')}
                  {renderInput('Step', paramRanges[param.name]?.step ?? param.step ?? 1,
                    (v: number) => setParamRanges((prev: any) => ({ ...prev, [param.name]: { ...prev[param.name], step: v } })), 'number')}
                </div>
              ))}
            </div>
          )}
        </div>,
        expandedSections, toggleSection
      )}
    </div>
  );
};
