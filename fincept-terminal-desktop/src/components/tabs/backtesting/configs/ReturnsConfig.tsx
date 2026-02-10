/**
 * ReturnsConfig - Returns analysis config (VectorBT only)
 * Analysis types: full, rolling, distribution, benchmark_comparison
 */

import React from 'react';
import { Database, LineChart } from 'lucide-react';
import { FINCEPT, FONT_FAMILY, RETURNS_ANALYSIS_TYPES } from '../constants';
import { renderInput, renderSelect, renderSection } from '../components/primitives';
import type { BacktestingState } from '../types';

interface ReturnsConfigProps {
  state: BacktestingState;
}

export const ReturnsConfig: React.FC<ReturnsConfigProps> = ({ state }) => {
  const {
    symbols, setSymbols, startDate, setStartDate, endDate, setEndDate,
    returnsAnalysisType, setReturnsAnalysisType,
    returnsRollingWindow, setReturnsRollingWindow,
    benchmarkSymbol, setBenchmarkSymbol,
    expandedSections, toggleSection,
  } = state;

  const analysisConfig = RETURNS_ANALYSIS_TYPES.find(r => r.id === returnsAnalysisType) || RETURNS_ANALYSIS_TYPES[0];

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Market Data */}
      {renderSection('market', 'MARKET DATA', Database,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInput('Symbols', symbols, setSymbols, 'text', 'SPY, AAPL')}
          <div />
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>,
        expandedSections, toggleSection
      )}

      {/* Returns Analysis */}
      {renderSection('returnsAnalysis', 'RETURNS ANALYSIS', LineChart,
        <>
          {renderSelect('Analysis Type', returnsAnalysisType,
            setReturnsAnalysisType,
            RETURNS_ANALYSIS_TYPES.map(r => ({ value: r.id, label: r.label }))
          )}

          <div style={{
            marginTop: '8px',
            padding: '8px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            fontSize: '8px',
            color: FINCEPT.MUTED,
            fontFamily: FONT_FAMILY,
          }}>
            {analysisConfig.description}
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px', marginTop: '12px' }}>
            {(returnsAnalysisType === 'rolling') && (
              renderInput('Rolling Window', returnsRollingWindow, setReturnsRollingWindow, 'number', undefined, 5, 252, 1)
            )}
            {(returnsAnalysisType === 'benchmark_comparison') && (
              renderInput('Benchmark Symbol', benchmarkSymbol, setBenchmarkSymbol, 'text', 'SPY')
            )}
          </div>
        </>,
        expandedSections, toggleSection
      )}
    </div>
  );
};
