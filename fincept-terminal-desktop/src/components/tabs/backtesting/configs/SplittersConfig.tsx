/**
 * SplittersConfig - Cross-validation / time-series split config (VectorBT only)
 * Splitter types: RollingSplitter, ExpandingSplitter, PurgedKFold
 */

import React from 'react';
import { Database, GitBranch } from 'lucide-react';
import { FINCEPT, FONT_FAMILY, SPLITTER_TYPES } from '../constants';
import { renderInput, renderSelect, renderSection } from '../components/primitives';
import type { BacktestingState } from '../types';

interface SplittersConfigProps {
  state: BacktestingState;
}

const SPLITTER_DEFAULTS: Record<string, Record<string, number>> = {
  RollingSplitter: { window_len: 252, test_len: 63, step: 21 },
  ExpandingSplitter: { min_len: 126, test_len: 63, step: 21 },
  PurgedKFold: { n_splits: 5, purge_len: 5, embargo_len: 5 },
};

export const SplittersConfig: React.FC<SplittersConfigProps> = ({ state }) => {
  const {
    symbols, setSymbols, startDate, setStartDate, endDate, setEndDate,
    splitterType, setSplitterType, splitterParams, setSplitterParams,
    expandedSections, toggleSection,
  } = state;

  const splitterConfig = SPLITTER_TYPES.find(s => s.id === splitterType) || SPLITTER_TYPES[0];

  const handleTypeChange = (type: string) => {
    setSplitterType(type);
    setSplitterParams(SPLITTER_DEFAULTS[type] || {});
  };

  const updateParam = (key: string, value: number) => {
    setSplitterParams((prev: Record<string, any>) => ({ ...prev, [key]: value }));
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

      {/* Splitter Config */}
      {renderSection('splitterType', 'SPLITTER / CV', GitBranch,
        <>
          {renderSelect('Splitter Type', splitterType,
            handleTypeChange,
            SPLITTER_TYPES.map(s => ({ value: s.id, label: s.label }))
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
            {splitterConfig.description}
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px', marginTop: '12px' }}>
            {splitterConfig.params.map(param => (
              <div key={param}>
                {renderInput(
                  param.replace(/_/g, ' '),
                  splitterParams[param] ?? (SPLITTER_DEFAULTS[splitterType]?.[param] || 0),
                  (v: number) => updateParam(param, v),
                  'number', undefined, 1, 500, 1
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
