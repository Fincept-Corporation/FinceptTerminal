/**
 * IndicatorConfig - Indicator selection and parameter sweep
 */

import React from 'react';
import { Database, Activity } from 'lucide-react';
import { FINCEPT, FONT_FAMILY, INDICATORS } from '../constants';
import { renderInput, renderSection } from '../components/primitives';
import type { BacktestingState } from '../types';

interface IndicatorConfigProps {
  state: BacktestingState;
}

export const IndicatorConfig: React.FC<IndicatorConfigProps> = ({ state }) => {
  const {
    symbols, setSymbols, startDate, setStartDate, endDate, setEndDate,
    selectedIndicator, setSelectedIndicator, indicatorParams, setIndicatorParams,
    expandedSections, toggleSection,
  } = state;

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

      {renderSection('indicator', 'INDICATOR SELECTION', Activity,
        <>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '6px' }}>
            {INDICATORS.map(ind => (
              <button
                key={ind.id}
                onClick={() => setSelectedIndicator(ind.id)}
                style={{
                  padding: '6px 8px',
                  backgroundColor: selectedIndicator === ind.id ? FINCEPT.PURPLE : 'transparent',
                  color: selectedIndicator === ind.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                  fontSize: '9px', fontWeight: 700, cursor: 'pointer', fontFamily: FONT_FAMILY,
                }}
              >
                {ind.label}
              </button>
            ))}
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px', marginTop: '8px' }}>
            {INDICATORS.find(i => i.id === selectedIndicator)?.params.map(param => (
              <div key={param}>
                {renderInput(param.toUpperCase(), indicatorParams[param] ?? 14,
                  (v: any) => setIndicatorParams((prev: any) => ({ ...prev, [param]: v })), 'number')}
              </div>
            ))}
          </div>
        </>,
        expandedSections, toggleSection
      )}
    </div>
  );
};
