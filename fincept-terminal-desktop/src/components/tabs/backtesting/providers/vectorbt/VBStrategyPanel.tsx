/**
 * VBStrategyPanel - Left 300px panel with card-based category selector,
 * strategy cards with left color bars, parameter inputs, and advanced config.
 *
 * Redesign: card grid categories, 11px strategy names, 10px+ labels.
 */

import React, { useMemo } from 'react';
import {
  FINCEPT, TYPOGRAPHY, EFFECTS, COMMON_STYLES, BORDERS, SPACING,
  createFocusHandlers,
} from '../../../portfolio-tab/finceptStyles';
import { POSITION_SIZING } from '../../constants';
import { renderInput, renderSelect } from '../../components/primitives';
import type { BacktestingState } from '../../types';

interface VBStrategyPanelProps {
  state: BacktestingState;
}

export const VBStrategyPanel: React.FC<VBStrategyPanelProps> = ({ state }) => {
  const {
    selectedCategory, setSelectedCategory,
    selectedStrategy, setSelectedStrategy,
    strategyParams, setStrategyParams,
    customCode, setCustomCode,
    providerStrategies, providerCategoryInfo, currentStrategy,
    showAdvanced,
    commission, setCommission,
    slippage, setSlippage,
    stopLoss, setStopLoss,
    takeProfit, setTakeProfit,
    trailingStop, setTrailingStop,
    positionSizing, setPositionSizing,
    positionSizeValue, setPositionSizeValue,
    leverage, setLeverage,
    allowShort, setAllowShort,
    enableBenchmark, setEnableBenchmark,
    benchmarkSymbol, setBenchmarkSymbol,
    activeCommand,
    optimizeObjective, setOptimizeObjective,
    optimizeMethod, setOptimizeMethod,
    maxIterations, setMaxIterations,
    wfSplits, setWfSplits,
    wfTrainRatio, setWfTrainRatio,
  } = state;

  const categories = useMemo(() => Object.keys(providerStrategies), [providerStrategies]);
  const strategies = useMemo(
    () => providerStrategies[selectedCategory] || [],
    [providerStrategies, selectedCategory]
  );
  const focus = createFocusHandlers();

  return (
    <div style={{
      width: '300px',
      flexShrink: 0,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      backgroundColor: FINCEPT.PANEL_BG,
    }}>
      {/* ── Header ── */}
      <div style={{
        padding: '10px 14px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
      }}>
        <span style={{
          fontSize: '11px',
          fontWeight: 700,
          color: FINCEPT.ORANGE,
          letterSpacing: '0.5px',
        }}>
          STRATEGIES
        </span>
        <span style={{
          fontSize: '10px',
          color: FINCEPT.GRAY,
          fontFamily: TYPOGRAPHY.MONO,
        }}>
          {strategies.length} available
        </span>
      </div>

      {/* ── Category Card Grid ── */}
      <div style={{
        padding: '8px 10px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'grid',
        gridTemplateColumns: 'repeat(3, 1fr)',
        gap: '4px',
        flexShrink: 0,
      }}>
        {categories.map(cat => {
          const info = providerCategoryInfo[cat];
          const isActive = selectedCategory === cat;
          const count = (providerStrategies[cat] || []).length;
          if (count === 0) return null;

          const catColor = info?.color || FINCEPT.ORANGE;
          const CatIcon = info?.icon;

          return (
            <button
              key={cat}
              onClick={() => {
                setSelectedCategory(cat);
                const first = providerStrategies[cat]?.[0];
                if (first) setSelectedStrategy(first.id);
              }}
              style={{
                padding: '6px 8px',
                backgroundColor: isActive ? `${catColor}15` : 'transparent',
                color: isActive ? catColor : FINCEPT.GRAY,
                border: isActive ? `1px solid ${catColor}` : BORDERS.STANDARD,
                borderLeft: isActive ? `3px solid ${catColor}` : `3px solid transparent`,
                borderRadius: '3px',
                fontSize: '10px',
                fontWeight: 700,
                cursor: 'pointer',
                letterSpacing: '0.3px',
                textTransform: 'uppercase' as const,
                transition: EFFECTS.TRANSITION_FAST,
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                gap: '2px',
                textAlign: 'center' as const,
              }}
            >
              {CatIcon && <CatIcon size={14} style={{ opacity: isActive ? 1 : 0.5 }} />}
              <span style={{ fontSize: '9px', lineHeight: 1.2 }}>
                {info?.label || cat}
              </span>
              <span style={{
                fontSize: '9px',
                fontWeight: 600,
                color: isActive ? catColor : FINCEPT.MUTED,
                backgroundColor: isActive ? `${catColor}20` : 'transparent',
                padding: '0 4px',
                borderRadius: '2px',
              }}>
                {count}
              </span>
            </button>
          );
        })}
      </div>

      {/* ── Strategy List ── */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '6px' }}>
        {strategies.map((strat: any) => {
          const isActive = selectedStrategy === strat.id;
          const catInfo = providerCategoryInfo[selectedCategory];
          const catColor = catInfo?.color || FINCEPT.ORANGE;

          return (
            <div
              key={strat.id}
              className="vb-strat"
              onClick={() => setSelectedStrategy(strat.id)}
              style={{
                padding: '10px 12px',
                cursor: 'pointer',
                borderLeft: `3px solid ${isActive ? catColor : 'transparent'}`,
                backgroundColor: isActive ? `${catColor}08` : 'transparent',
                transition: EFFECTS.TRANSITION_FAST,
                marginBottom: '2px',
                borderRadius: '2px',
                minHeight: '44px',
              }}
            >
              <div style={{
                fontSize: '11px',
                fontWeight: isActive ? 700 : 600,
                color: isActive ? FINCEPT.WHITE : FINCEPT.GRAY,
                marginBottom: '3px',
              }}>
                {strat.name}
              </div>
              <div style={{
                fontSize: '9px',
                color: FINCEPT.MUTED,
                lineHeight: 1.4,
                marginBottom: strat.params?.length > 0 ? '4px' : '0',
              }}>
                {strat.description}
              </div>
              {/* Inline param preview */}
              {strat.params?.length > 0 && (
                <div style={{
                  fontSize: '10px',
                  color: FINCEPT.CYAN,
                  fontFamily: TYPOGRAPHY.MONO,
                  opacity: 0.8,
                }}>
                  {strat.params.map((p: any) =>
                    `${p.label.toLowerCase()}=${state.strategyParams[p.name] ?? p.default}`
                  ).join(', ')}
                </div>
              )}
            </div>
          );
        })}
      </div>

      {/* ── Strategy Parameters ── */}
      {currentStrategy && currentStrategy.params.length > 0 && (
        <div style={{
          padding: '10px 12px',
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          flexShrink: 0,
        }}>
          <div style={{
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.ORANGE,
            marginBottom: '8px',
            letterSpacing: '0.5px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}>
            <div style={{
              width: '16px', height: '1px',
              backgroundColor: FINCEPT.ORANGE,
            }} />
            PARAMETERS
            <div style={{
              flex: 1, height: '1px',
              backgroundColor: FINCEPT.BORDER,
            }} />
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
            {currentStrategy.params.map((param: any) => (
              <div key={param.name}>
                <label style={{
                  fontSize: '10px',
                  color: FINCEPT.GRAY,
                  fontWeight: 700,
                  display: 'block',
                  marginBottom: '3px',
                }}>
                  {param.label}
                </label>
                <input
                  type="number"
                  value={strategyParams[param.name] ?? param.default}
                  onChange={e => setStrategyParams(prev => ({ ...prev, [param.name]: Number(e.target.value) }))}
                  min={param.min}
                  max={param.max}
                  step={param.step || 1}
                  style={{
                    width: '100%',
                    padding: '6px 8px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.CYAN,
                    border: BORDERS.STANDARD,
                    borderRadius: '2px',
                    fontSize: '11px',
                    fontFamily: TYPOGRAPHY.MONO,
                    outline: 'none',
                    fontWeight: 600,
                  }}
                  {...focus}
                />
              </div>
            ))}
          </div>
        </div>
      )}

      {/* ── Custom Code ── */}
      {selectedStrategy === 'code' && (
        <div style={{
          padding: '10px 12px',
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          flexShrink: 0,
        }}>
          <div style={{
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.ORANGE,
            marginBottom: '6px',
          }}>
            CUSTOM CODE
          </div>
          <textarea
            value={customCode}
            onChange={e => setCustomCode(e.target.value)}
            style={{
              width: '100%',
              height: '100px',
              padding: '8px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.CYAN,
              border: BORDERS.STANDARD,
              borderRadius: '2px',
              fontSize: '10px',
              fontFamily: TYPOGRAPHY.MONO,
              outline: 'none',
              resize: 'vertical',
            }}
          />
        </div>
      )}

      {/* ── Optimize Config ── */}
      {activeCommand === 'optimize' && (
        <div style={{
          padding: '10px 12px',
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          flexShrink: 0,
        }}>
          <div style={{
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.ORANGE,
            marginBottom: '8px',
          }}>
            OPTIMIZE
          </div>
          <div style={{ display: 'grid', gap: '6px' }}>
            {renderSelect('Objective', optimizeObjective, v => setOptimizeObjective(v as any), [
              { value: 'sharpe', label: 'Sharpe Ratio' },
              { value: 'sortino', label: 'Sortino Ratio' },
              { value: 'calmar', label: 'Calmar Ratio' },
              { value: 'return', label: 'Total Return' },
            ])}
            {renderSelect('Method', optimizeMethod, v => setOptimizeMethod(v as any), [
              { value: 'grid', label: 'Grid Search' },
              { value: 'random', label: 'Random Search' },
            ])}
            {renderInput('Max Iterations', maxIterations, setMaxIterations, 'number', undefined, 10, 10000)}
          </div>
        </div>
      )}

      {/* ── Walk Forward Config ── */}
      {activeCommand === 'walk_forward' && (
        <div style={{
          padding: '10px 12px',
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          flexShrink: 0,
        }}>
          <div style={{
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.ORANGE,
            marginBottom: '8px',
          }}>
            WALK FORWARD
          </div>
          <div style={{ display: 'grid', gap: '6px' }}>
            {renderInput('Splits', wfSplits, setWfSplits, 'number', undefined, 2, 20)}
            {renderInput('Train Ratio', wfTrainRatio, setWfTrainRatio, 'number', undefined, 0.5, 0.9, 0.05)}
          </div>
        </div>
      )}

      {/* ── Advanced Config ── */}
      {showAdvanced && (
        <div style={{
          padding: '10px 12px',
          borderTop: `1px solid ${FINCEPT.ORANGE}30`,
          flexShrink: 0,
          maxHeight: '240px',
          overflowY: 'auto',
        }}>
          <div style={{
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.ORANGE,
            marginBottom: '8px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}>
            <div style={{
              width: '16px', height: '1px',
              backgroundColor: FINCEPT.ORANGE,
            }} />
            ADVANCED
            <div style={{
              flex: 1, height: '1px',
              backgroundColor: FINCEPT.BORDER,
            }} />
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
            {renderInput('Commission', commission, setCommission, 'number', undefined, 0, 0.1, 0.0001)}
            {renderInput('Slippage', slippage, setSlippage, 'number', undefined, 0, 0.1, 0.0001)}
            {renderInput('Stop Loss', stopLoss ?? '', (v: any) => setStopLoss(v === '' ? null : Number(v)), 'number', 'None', 0, 1, 0.01)}
            {renderInput('Take Profit', takeProfit ?? '', (v: any) => setTakeProfit(v === '' ? null : Number(v)), 'number', 'None', 0, 10, 0.01)}
            {renderInput('Trailing Stop', trailingStop ?? '', (v: any) => setTrailingStop(v === '' ? null : Number(v)), 'number', 'None', 0, 1, 0.01)}
            {renderInput('Leverage', leverage, setLeverage, 'number', undefined, 1, 100)}
          </div>
          <div style={{ marginTop: '8px', display: 'flex', flexDirection: 'column', gap: '6px' }}>
            {renderSelect('Position Sizing', positionSizing, setPositionSizing,
              POSITION_SIZING.map(p => ({ value: p.id, label: p.label }))
            )}
            {renderInput('Size Value', positionSizeValue, setPositionSizeValue, 'number', undefined, 0, 100, 0.01)}
            <label style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              fontSize: '10px',
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              fontWeight: 600,
            }}>
              <input
                type="checkbox"
                checked={allowShort}
                onChange={e => setAllowShort(e.target.checked)}
                style={{ accentColor: FINCEPT.ORANGE }}
              />
              ALLOW SHORT
            </label>
            <label style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              fontSize: '10px',
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              fontWeight: 600,
            }}>
              <input
                type="checkbox"
                checked={enableBenchmark}
                onChange={e => setEnableBenchmark(e.target.checked)}
                style={{ accentColor: FINCEPT.ORANGE }}
              />
              BENCHMARK
            </label>
            {enableBenchmark && renderInput('Benchmark', benchmarkSymbol, setBenchmarkSymbol, 'text', 'SPY')}
          </div>
        </div>
      )}
    </div>
  );
};
