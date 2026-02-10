/**
 * AdvancedConfig - Risk management, position sizing, leverage, benchmark
 */

import React from 'react';
import { Target } from 'lucide-react';
import { FINCEPT, FONT_FAMILY, POSITION_SIZING } from '../constants';
import { renderInput, renderSection } from '../components/primitives';
import type { BacktestingState } from '../types';

interface AdvancedConfigProps {
  state: BacktestingState;
}

export const AdvancedConfig: React.FC<AdvancedConfigProps> = ({ state }) => {
  const {
    stopLoss, setStopLoss, takeProfit, setTakeProfit, trailingStop, setTrailingStop,
    positionSizing, setPositionSizing, positionSizeValue, setPositionSizeValue,
    leverage, setLeverage, margin, setMargin, allowShort, setAllowShort,
    enableBenchmark, setEnableBenchmark, benchmarkSymbol, setBenchmarkSymbol,
    randomBenchmark, setRandomBenchmark, expandedSections, toggleSection,
  } = state;

  return renderSection('advanced', 'ADVANCED SETTINGS', Target,
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Risk Management */}
      <div>
        <div style={{
          fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN,
          marginBottom: '6px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
        }}>
          RISK MANAGEMENT
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
          {renderInput('Stop Loss (%)', stopLoss ?? '', (v: any) => setStopLoss(v || null), 'number', 'Optional', 0, 100, 0.1)}
          {renderInput('Take Profit (%)', takeProfit ?? '', (v: any) => setTakeProfit(v || null), 'number', 'Optional', 0, 100, 0.1)}
          {renderInput('Trailing Stop (%)', trailingStop ?? '', (v: any) => setTrailingStop(v || null), 'number', 'Optional', 0, 100, 0.1)}
        </div>
      </div>

      {/* Position Sizing */}
      <div>
        <div style={{
          fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN,
          marginBottom: '6px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
        }}>
          POSITION SIZING
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <div>
            <label style={{
              display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY,
              marginBottom: '4px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
            }}>
              METHOD
            </label>
            <select
              value={positionSizing}
              onChange={e => setPositionSizing(e.target.value)}
              style={{
                width: '100%', padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                fontSize: '10px', fontFamily: FONT_FAMILY, outline: 'none',
              }}
            >
              {POSITION_SIZING.map(ps => (
                <option key={ps.id} value={ps.id}>{ps.label}</option>
              ))}
            </select>
          </div>
          {renderInput('Size Value', positionSizeValue, setPositionSizeValue, 'number', undefined, 0, 100)}
        </div>
      </div>

      {/* Leverage & Margin */}
      <div>
        <div style={{
          fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN,
          marginBottom: '6px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
        }}>
          LEVERAGE & MARGIN
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
          {renderInput('Leverage', leverage, setLeverage, 'number', undefined, 1, 10, 0.1)}
          {renderInput('Margin', margin, setMargin, 'number', undefined, 0, 1, 0.1)}
          <label style={{
            display: 'flex', alignItems: 'center', gap: '6px',
            fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY,
            cursor: 'pointer', fontFamily: FONT_FAMILY, paddingTop: '16px',
          }}>
            <input type="checkbox" checked={allowShort} onChange={e => setAllowShort(e.target.checked)} />
            ALLOW SHORT
          </label>
        </div>
      </div>

      {/* Benchmark */}
      <div>
        <div style={{
          fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN,
          marginBottom: '6px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
        }}>
          BENCHMARK COMPARISON
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 2fr', gap: '8px' }}>
          <label style={{
            display: 'flex', alignItems: 'center', gap: '6px',
            fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY,
            cursor: 'pointer', fontFamily: FONT_FAMILY,
          }}>
            <input type="checkbox" checked={enableBenchmark} onChange={e => setEnableBenchmark(e.target.checked)} />
            ENABLE
          </label>
          {enableBenchmark && (
            <>
              {renderInput('Benchmark Symbol', benchmarkSymbol, setBenchmarkSymbol, 'text', 'SPY')}
              <label style={{
                display: 'flex', alignItems: 'center', gap: '6px',
                fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY,
                cursor: 'pointer', fontFamily: FONT_FAMILY,
              }}>
                <input type="checkbox" checked={randomBenchmark} onChange={e => setRandomBenchmark(e.target.checked)} />
                RANDOM BENCHMARK
              </label>
            </>
          )}
        </div>
      </div>
    </div>,
    expandedSections, toggleSection
  );
};
