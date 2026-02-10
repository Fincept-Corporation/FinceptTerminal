/**
 * LabelsConfig - ML label generation config (VectorBT only)
 * Label types: FIXLB, MEANLB, LEXLB, TRENDLB, BOLB
 */

import React from 'react';
import { Database, Layers } from 'lucide-react';
import { FINCEPT, FONT_FAMILY, LABEL_TYPES } from '../constants';
import { renderInput, renderSelect, renderSection } from '../components/primitives';
import type { BacktestingState } from '../types';

interface LabelsConfigProps {
  state: BacktestingState;
}

const LABEL_DEFAULTS: Record<string, Record<string, number | string>> = {
  FIXLB: { horizon: 5, threshold: 0.02 },
  MEANLB: { window: 20, threshold: 1.5 },
  LEXLB: { window: 10 },
  TRENDLB: { window: 20, threshold: 0.02, method: 'slope' },
  BOLB: { window: 20, alpha: 2.0 },
};

export const LabelsConfig: React.FC<LabelsConfigProps> = ({ state }) => {
  const {
    symbols, setSymbols, startDate, setStartDate, endDate, setEndDate,
    labelsType, setLabelsType, labelsParams, setLabelsParams,
    expandedSections, toggleSection,
  } = state;

  const labelConfig = LABEL_TYPES.find(l => l.id === labelsType) || LABEL_TYPES[0];

  const handleTypeChange = (type: string) => {
    setLabelsType(type);
    setLabelsParams(LABEL_DEFAULTS[type] || {});
  };

  const updateParam = (key: string, value: any) => {
    setLabelsParams((prev: Record<string, any>) => ({ ...prev, [key]: value }));
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

      {/* Label Type */}
      {renderSection('labelType', 'LABEL GENERATOR', Layers,
        <>
          {renderSelect('Label Type', labelsType,
            handleTypeChange,
            LABEL_TYPES.map(l => ({ value: l.id, label: l.label }))
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
            {labelConfig.description}
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px', marginTop: '12px' }}>
            {labelConfig.params.map(param => {
              if (param === 'method') {
                return (
                  <div key={param}>
                    {renderSelect('Method',
                      (labelsParams.method as string) || 'slope',
                      (v: string) => updateParam('method', v),
                      [{ value: 'slope', label: 'Slope' }, { value: 'pct', label: 'Percentage' }]
                    )}
                  </div>
                );
              }
              return (
                <div key={param}>
                  {renderInput(
                    param.replace(/_/g, ' '),
                    labelsParams[param] ?? (LABEL_DEFAULTS[labelsType]?.[param] || 0),
                    (v: number) => updateParam(param, v),
                    'number', undefined,
                    param === 'threshold' ? 0 : 1,
                    param === 'threshold' ? 10 : 200,
                    param === 'threshold' || param === 'alpha' ? 0.01 : 1
                  )}
                </div>
              );
            })}
          </div>
        </>,
        expandedSections, toggleSection
      )}
    </div>
  );
};
