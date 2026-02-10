/**
 * SignalsConfig - Random signal generator config (VectorBT only)
 * Generator types: RAND, RANDX, RANDNX, RPROB, RPROBX
 */

import React from 'react';
import { Database, Shuffle } from 'lucide-react';
import { FINCEPT, FONT_FAMILY, SIGNAL_GENERATORS } from '../constants';
import { renderInput, renderSelect, renderSection } from '../components/primitives';
import type { BacktestingState } from '../types';

interface SignalsConfigProps {
  state: BacktestingState;
}

const GENERATOR_DEFAULTS: Record<string, Record<string, number>> = {
  RAND: { n: 10, seed: 42 },
  RANDX: { n: 10, seed: 42 },
  RANDNX: { n: 10, min_hold: 1, max_hold: 10 },
  RPROB: { entry_prob: 0.05, exit_prob: 0.05, cooldown: 0 },
  RPROBX: { entry_prob: 0.05, exit_prob: 0.05, cooldown: 0 },
};

export const SignalsConfig: React.FC<SignalsConfigProps> = ({ state }) => {
  const {
    symbols, setSymbols, startDate, setStartDate, endDate, setEndDate,
    signalGeneratorType, setSignalGeneratorType,
    signalGeneratorParams, setSignalGeneratorParams,
    expandedSections, toggleSection,
  } = state;

  const genConfig = SIGNAL_GENERATORS.find(g => g.id === signalGeneratorType) || SIGNAL_GENERATORS[0];

  const handleTypeChange = (type: string) => {
    setSignalGeneratorType(type);
    setSignalGeneratorParams(GENERATOR_DEFAULTS[type] || {});
  };

  const updateParam = (key: string, value: number) => {
    setSignalGeneratorParams((prev: Record<string, any>) => ({ ...prev, [key]: value }));
  };

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

      {/* Signal Generator */}
      {renderSection('signalGen', 'SIGNAL GENERATOR', Shuffle,
        <>
          {renderSelect('Generator Type', signalGeneratorType,
            handleTypeChange,
            SIGNAL_GENERATORS.map(g => ({ value: g.id, label: g.label }))
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
            {genConfig.description}
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px', marginTop: '12px' }}>
            {genConfig.params.map(param => (
              <div key={param}>
                {renderInput(
                  param.replace(/_/g, ' '),
                  signalGeneratorParams[param] ?? (GENERATOR_DEFAULTS[signalGeneratorType]?.[param] || 0),
                  (v: number) => updateParam(param, v),
                  'number', undefined,
                  param.includes('prob') ? 0 : 0,
                  param.includes('prob') ? 1 : 1000,
                  param.includes('prob') ? 0.01 : 1
                )}
              </div>
            ))}
          </div>
        </>,
        expandedSections, toggleSection
      )}
    </div>
  );
};
