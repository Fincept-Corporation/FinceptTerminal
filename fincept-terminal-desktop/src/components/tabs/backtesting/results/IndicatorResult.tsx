/**
 * IndicatorResult - Indicator sweep results display
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from '../constants';
import { ResultRaw } from './ResultRaw';

interface IndicatorResultProps {
  result: any;
}

const renderStat = (label: string, value: number | undefined, color: string) => (
  <div>
    <span style={{ color: FINCEPT.GRAY }}>{label}: </span>
    <span style={{ color }}>{value?.toFixed(2) ?? 'N/A'}</span>
  </div>
);

const renderSingleOutput = (r: any, idx: number) => (
  <div key={idx} style={{
    padding: '6px 8px',
    backgroundColor: `${FINCEPT.ORANGE}10`,
    borderLeft: `2px solid ${FINCEPT.PURPLE}`,
    borderRadius: '2px',
    display: 'grid',
    gridTemplateColumns: '1fr 1fr 1fr 1fr',
    gap: '8px',
    fontSize: '8px',
    fontFamily: FONT_FAMILY,
  }}>
    <div>
      <span style={{ color: FINCEPT.GRAY }}>Params: </span>
      <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>
        {JSON.stringify(r.params)}
      </span>
    </div>
    {renderStat('Mean', r.mean, FINCEPT.CYAN)}
    {renderStat('Last', r.last, FINCEPT.ORANGE)}
    <div>
      <span style={{ color: FINCEPT.GRAY }}>Range: </span>
      <span style={{ color: FINCEPT.WHITE }}>
        {r.min?.toFixed(2) ?? 'N/A'} - {r.max?.toFixed(2) ?? 'N/A'}
      </span>
    </div>
  </div>
);

const renderMultiOutput = (r: any, idx: number) => {
  const outputs = r.outputs ?? {};
  const componentNames = Object.keys(outputs);

  return (
    <div key={idx} style={{
      padding: '8px',
      backgroundColor: `${FINCEPT.ORANGE}10`,
      borderLeft: `2px solid ${FINCEPT.PURPLE}`,
      borderRadius: '2px',
      fontSize: '8px',
      fontFamily: FONT_FAMILY,
    }}>
      {/* Parameters */}
      <div style={{ marginBottom: '8px', paddingBottom: '6px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <span style={{ color: FINCEPT.GRAY }}>Params: </span>
        <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>
          {JSON.stringify(r.params)}
        </span>
      </div>

      {/* Components */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
        {componentNames.map(componentName => {
          const component = outputs[componentName];
          return (
            <div key={componentName} style={{
              padding: '4px 6px',
              backgroundColor: `${FINCEPT.CYAN}15`,
              borderRadius: '2px',
            }}>
              <div style={{ color: FINCEPT.CYAN, fontWeight: 700, marginBottom: '4px', fontSize: '7px', fontFamily: FONT_FAMILY }}>
                {componentName.toUpperCase()}
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '6px' }}>
                {renderStat('Mean', component.mean, FINCEPT.WHITE)}
                {renderStat('Std', component.std, FINCEPT.GRAY)}
                {renderStat('Last', component.last, FINCEPT.ORANGE)}
                <div>
                  <span style={{ color: FINCEPT.GRAY }}>Range: </span>
                  <span style={{ color: FINCEPT.WHITE }}>
                    {component.min?.toFixed(2) ?? 'N/A'} - {component.max?.toFixed(2) ?? 'N/A'}
                  </span>
                </div>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
};

export const IndicatorResult: React.FC<IndicatorResultProps> = ({ result }) => {
  if (!result?.data) return <ResultRaw result={result} />;

  const data = result.data;
  const indicator = data.indicator ?? 'Unknown';
  const totalCombinations = data.totalCombinations ?? 0;
  const results = data.results ?? [];

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      <div style={{
        padding: '4px 8px',
        backgroundColor: `${FINCEPT.GREEN}20`,
        color: FINCEPT.GREEN,
        fontSize: '8px',
        fontWeight: 700,
        borderRadius: '2px',
        display: 'inline-block',
        alignSelf: 'flex-start',
        fontFamily: FONT_FAMILY,
      }}>
        INDICATOR SWEEP COMPLETED
      </div>

      {/* Summary Card */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
      }}>
        <div style={{ marginBottom: '8px' }}>
          <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, fontFamily: FONT_FAMILY }}>
            {indicator.toUpperCase()} PARAMETER SWEEP
          </div>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, fontFamily: FONT_FAMILY }}>
            Total Combinations: {totalCombinations}
          </div>
        </div>
      </div>

      {/* Results Table */}
      {results.length > 0 ? (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '8px', fontWeight: 600, fontFamily: FONT_FAMILY }}>
            PARAMETER COMBINATIONS (showing first 20)
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            {results.slice(0, 20).map((r: any, idx: number) => {
              const isMultiOutput = r.outputs && Object.keys(r.outputs).length > 0;
              return isMultiOutput ? renderMultiOutput(r, idx) : renderSingleOutput(r, idx);
            })}
          </div>
        </div>
      ) : (
        <div style={{
          padding: '12px',
          backgroundColor: `${FINCEPT.YELLOW}20`,
          border: `1px solid ${FINCEPT.YELLOW}`,
          borderRadius: '2px',
        }}>
          <div style={{ fontSize: '9px', color: FINCEPT.YELLOW, fontFamily: FONT_FAMILY }}>
            No parameter combinations generated. Check parameter ranges.
          </div>
        </div>
      )}
    </div>
  );
};
