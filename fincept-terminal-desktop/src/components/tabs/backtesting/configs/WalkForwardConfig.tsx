/**
 * WalkForwardConfig - Walk-forward analysis settings
 */

import React from 'react';
import { Database, ChevronRight } from 'lucide-react';
import { renderInput, renderSection } from '../components/primitives';
import type { BacktestingState } from '../types';

interface WalkForwardConfigProps {
  state: BacktestingState;
}

export const WalkForwardConfig: React.FC<WalkForwardConfigProps> = ({ state }) => {
  const {
    symbols, setSymbols, startDate, setStartDate, endDate, setEndDate,
    initialCapital, setInitialCapital,
    wfSplits, setWfSplits, wfTrainRatio, setWfTrainRatio,
    expandedSections, toggleSection,
  } = state;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {renderSection('market', 'MARKET DATA', Database,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInput('Symbols', symbols, setSymbols, 'text')}
          {renderInput('Initial Capital', initialCapital, setInitialCapital, 'number')}
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>,
        expandedSections, toggleSection
      )}

      {renderSection('walkforward', 'WALK-FORWARD SETTINGS', ChevronRight,
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {renderInput('Number of Splits', wfSplits, setWfSplits, 'number', undefined, 2, 20)}
          {renderInput('Train Ratio', wfTrainRatio, setWfTrainRatio, 'number', undefined, 0.5, 0.9, 0.05)}
        </div>,
        expandedSections, toggleSection
      )}
    </div>
  );
};
