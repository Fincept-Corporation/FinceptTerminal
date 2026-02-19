/**
 * VBStrategyPanel - NodePalette-identical structure.
 * Header / Search / Expandable category headers / Strategy items with dot+name+description / Footer
 */

import React, { useMemo, useState } from 'react';
import { Layers, Search, ChevronDown } from 'lucide-react';
import {
  FINCEPT, TYPOGRAPHY, EFFECTS, BORDERS,
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
  const totalCount = useMemo(
    () => categories.reduce((sum, cat) => sum + (providerStrategies[cat]?.length || 0), 0),
    [categories, providerStrategies]
  );

  const [searchQuery, setSearchQuery] = useState('');
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(
    () => new Set(categories)
  );

  const toggleCategory = (cat: string) => {
    const next = new Set(expandedCategories);
    if (next.has(cat)) next.delete(cat); else next.add(cat);
    setExpandedCategories(next);
  };

  // Filter strategies by search
  const filteredStrategies = useMemo(() => {
    const q = searchQuery.toLowerCase().trim();
    if (!q) return providerStrategies;
    const result: typeof providerStrategies = {};
    categories.forEach(cat => {
      const filtered = (providerStrategies[cat] || []).filter((s: any) =>
        s.name.toLowerCase().includes(q) || (s.description || '').toLowerCase().includes(q)
      );
      if (filtered.length > 0) result[cat] = filtered;
    });
    return result;
  }, [searchQuery, providerStrategies, categories]);

  const focus = createFocusHandlers();

  return (
    <div style={{
      width: '300px',
      flexShrink: 0,
      borderRight: `1px solid #2a2a2a`,
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      backgroundColor: '#000000',
      fontFamily: '"IBM Plex Mono", Consolas, monospace',
    }}>

      {/* ── Header ── */}
      <div style={{
        padding: '10px 12px',
        backgroundColor: '#0f0f0f',
        borderBottom: '1px solid #2a2a2a',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '7px' }}>
          <Layers size={13} style={{ color: '#FF8800' }} />
          <span style={{
            color: '#FF8800',
            fontSize: '10px',
            fontWeight: 700,
            letterSpacing: '0.8px',
          }}>
            STRATEGIES
          </span>
        </div>
      </div>

      {/* ── Search ── */}
      <div style={{
        padding: '8px 10px',
        borderBottom: '1px solid #2a2a2a',
        backgroundColor: '#000000',
        flexShrink: 0,
      }}>
        <div style={{ position: 'relative' }}>
          <Search size={11} style={{
            position: 'absolute',
            left: '8px',
            top: '50%',
            transform: 'translateY(-50%)',
            color: '#787878',
            pointerEvents: 'none',
          }} />
          <input
            type="text"
            value={searchQuery}
            onChange={e => setSearchQuery(e.target.value)}
            placeholder="Search strategies..."
            style={{
              width: '100%',
              backgroundColor: '#0f0f0f',
              border: '1px solid #2a2a2a',
              borderRadius: '2px',
              padding: '6px 8px 6px 26px',
              color: '#ffffff',
              fontSize: '10px',
              fontFamily: '"IBM Plex Mono", Consolas, monospace',
              outline: 'none',
              boxSizing: 'border-box' as const,
              transition: 'border-color 0.15s',
            }}
            onFocus={e => (e.currentTarget.style.borderColor = '#FF8800')}
            onBlur={e => (e.currentTarget.style.borderColor = '#2a2a2a')}
          />
        </div>
        <div style={{ color: '#787878', fontSize: '9px', marginTop: '5px', letterSpacing: '0.3px' }}>
          {totalCount} STRATEGIES AVAILABLE
        </div>
      </div>

      {/* ── Category + Strategy List ── */}
      <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
        {Object.keys(filteredStrategies).length === 0 && (
          <div style={{ padding: '32px 16px', textAlign: 'center' as const }}>
            <Search size={20} style={{ color: '#2a2a2a', margin: '0 auto 10px' }} />
            <div style={{ fontSize: '9px', fontWeight: 700, color: '#787878', letterSpacing: '0.5px' }}>
              NO STRATEGIES FOUND
            </div>
            <div style={{ fontSize: '9px', color: '#4a4a4a', marginTop: '4px' }}>
              Try a different search term
            </div>
          </div>
        )}

        {Object.keys(filteredStrategies).map(cat => {
          const info = providerCategoryInfo[cat];
          const strategies = filteredStrategies[cat] || [];
          if (strategies.length === 0) return null;
          const catColor = info?.color || '#FF8800';
          const CatIcon = info?.icon;
          const isExpanded = expandedCategories.has(cat);

          return (
            <div key={cat}>
              {/* Category Header */}
              <button
                onClick={() => toggleCategory(cat)}
                style={{
                  width: '100%',
                  padding: '7px 10px',
                  backgroundColor: '#0f0f0f',
                  border: 'none',
                  borderBottom: '1px solid #2a2a2a',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  cursor: 'pointer',
                  transition: 'background-color 0.15s',
                }}
                onMouseEnter={e => (e.currentTarget.style.backgroundColor = '#1a1a1a')}
                onMouseLeave={e => (e.currentTarget.style.backgroundColor = '#0f0f0f')}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '7px' }}>
                  {CatIcon && (
                    <span style={{ color: catColor, display: 'flex', alignItems: 'center' }}>
                      <CatIcon size={12} />
                    </span>
                  )}
                  <span style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: '#d4d4d4',
                    letterSpacing: '0.5px',
                  }}>
                    {(info?.label || cat).toUpperCase()}
                  </span>
                  <span style={{
                    fontSize: '8px',
                    color: '#787878',
                    backgroundColor: '#1a1a1a',
                    border: '1px solid #2a2a2a',
                    borderRadius: '2px',
                    padding: '1px 4px',
                    letterSpacing: '0.3px',
                  }}>
                    {strategies.length}
                  </span>
                </div>
                <ChevronDown size={11} style={{
                  color: '#787878',
                  transform: isExpanded ? 'rotate(0deg)' : 'rotate(-90deg)',
                  transition: 'transform 0.15s',
                }} />
              </button>

              {/* Strategy Items */}
              {isExpanded && (
                <div style={{
                  padding: '4px 6px 6px',
                  backgroundColor: '#000000',
                  borderBottom: '1px solid #1a1a1a',
                }}>
                  {strategies.map((strat: any) => {
                    const isActive = selectedStrategy === strat.id;
                    return (
                      <div
                        key={strat.id}
                        onClick={() => { setSelectedCategory(cat); setSelectedStrategy(strat.id); }}
                        style={{
                          backgroundColor: isActive ? `${catColor}15` : '#0f0f0f',
                          border: `1px solid ${isActive ? catColor + '50' : '#2a2a2a'}`,
                          borderLeft: `2px solid ${isActive ? catColor : '#2a2a2a'}`,
                          borderRadius: '2px',
                          padding: '6px 8px',
                          marginBottom: '3px',
                          cursor: 'pointer',
                          transition: 'all 0.15s',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '8px',
                        }}
                        onMouseEnter={e => {
                          if (!isActive) {
                            e.currentTarget.style.backgroundColor = `${catColor}10`;
                            e.currentTarget.style.borderColor = `${catColor}50`;
                            e.currentTarget.style.borderLeftColor = catColor;
                            e.currentTarget.style.borderLeftWidth = '2px';
                          }
                        }}
                        onMouseLeave={e => {
                          if (!isActive) {
                            e.currentTarget.style.backgroundColor = '#0f0f0f';
                            e.currentTarget.style.borderColor = '#2a2a2a';
                            e.currentTarget.style.borderLeftWidth = '1px';
                          }
                        }}
                      >
                        {/* Color dot */}
                        <div style={{
                          width: '6px',
                          height: '6px',
                          borderRadius: '50%',
                          backgroundColor: isActive ? catColor : '#787878',
                          flexShrink: 0,
                          boxShadow: isActive ? `0 0 4px ${catColor}60` : 'none',
                        }} />
                        {/* Info */}
                        <div style={{ flex: 1, minWidth: 0 }}>
                          <div style={{
                            color: isActive ? '#ffffff' : '#d4d4d4',
                            fontSize: '10px',
                            fontWeight: 700,
                            letterSpacing: '0.3px',
                            whiteSpace: 'nowrap',
                            overflow: 'hidden',
                            textOverflow: 'ellipsis',
                          }}>
                            {strat.name}
                          </div>
                          {strat.description && (
                            <div style={{
                              color: '#787878',
                              fontSize: '8px',
                              letterSpacing: '0.2px',
                              whiteSpace: 'nowrap',
                              overflow: 'hidden',
                              textOverflow: 'ellipsis',
                              marginTop: '2px',
                            }}>
                              {strat.description.length > 36
                                ? `${strat.description.substring(0, 36)}...`
                                : strat.description}
                            </div>
                          )}
                        </div>
                      </div>
                    );
                  })}
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
          borderTop: '1px solid #2a2a2a',
          backgroundColor: '#0f0f0f',
          flexShrink: 0,
        }}>
          <div style={{
            fontSize: '9px', fontWeight: 700, color: '#FF8800',
            marginBottom: '8px', letterSpacing: '0.5px',
          }}>
            PARAMETERS
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
            {currentStrategy.params.map((param: any) => (
              <div key={param.name}>
                <label style={{
                  fontSize: '9px', color: '#787878', fontWeight: 700,
                  display: 'block', marginBottom: '3px',
                }}>
                  {param.label}
                </label>
                <input
                  type="number"
                  value={strategyParams[param.name] ?? param.default}
                  onChange={e => setStrategyParams((prev: any) => ({ ...prev, [param.name]: Number(e.target.value) }))}
                  min={param.min} max={param.max} step={param.step || 1}
                  style={{
                    width: '100%', padding: '5px 8px',
                    backgroundColor: '#000000', color: FINCEPT.CYAN,
                    border: '1px solid #2a2a2a', borderRadius: '2px',
                    fontSize: '10px', fontFamily: '"IBM Plex Mono", Consolas, monospace',
                    outline: 'none', fontWeight: 600,
                  }}
                  onFocus={e => (e.currentTarget.style.borderColor = '#FF8800')}
                  onBlur={e => (e.currentTarget.style.borderColor = '#2a2a2a')}
                />
              </div>
            ))}
          </div>
        </div>
      )}

      {/* ── Custom Code ── */}
      {selectedStrategy === 'code' && (
        <div style={{ padding: '10px 12px', borderTop: '1px solid #2a2a2a', backgroundColor: '#0f0f0f', flexShrink: 0 }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: '#FF8800', marginBottom: '6px' }}>CUSTOM CODE</div>
          <textarea
            value={customCode}
            onChange={e => setCustomCode(e.target.value)}
            style={{
              width: '100%', height: '100px', padding: '8px',
              backgroundColor: '#000000', color: FINCEPT.CYAN,
              border: '1px solid #2a2a2a', borderRadius: '2px',
              fontSize: '10px', fontFamily: '"IBM Plex Mono", Consolas, monospace',
              outline: 'none', resize: 'vertical' as const,
            }}
          />
        </div>
      )}

      {/* ── Optimize Config ── */}
      {activeCommand === 'optimize' && (
        <div style={{ padding: '10px 12px', borderTop: '1px solid #2a2a2a', backgroundColor: '#0f0f0f', flexShrink: 0 }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: '#FF8800', marginBottom: '8px' }}>OPTIMIZE</div>
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
        <div style={{ padding: '10px 12px', borderTop: '1px solid #2a2a2a', backgroundColor: '#0f0f0f', flexShrink: 0 }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: '#FF8800', marginBottom: '8px' }}>WALK FORWARD</div>
          <div style={{ display: 'grid', gap: '6px' }}>
            {renderInput('Splits', wfSplits, setWfSplits, 'number', undefined, 2, 20)}
            {renderInput('Train Ratio', wfTrainRatio, setWfTrainRatio, 'number', undefined, 0.5, 0.9, 0.05)}
          </div>
        </div>
      )}

      {/* ── Advanced Config ── */}
      {showAdvanced && (
        <div style={{
          padding: '10px 12px', borderTop: '1px solid #2a2a2a',
          backgroundColor: '#0f0f0f', flexShrink: 0, maxHeight: '240px', overflowY: 'auto',
        }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: '#FF8800', marginBottom: '8px' }}>ADVANCED</div>
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
            <label style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '9px', color: '#787878', cursor: 'pointer', fontWeight: 600 }}>
              <input type="checkbox" checked={allowShort} onChange={e => setAllowShort(e.target.checked)} style={{ accentColor: '#FF8800' }} />
              ALLOW SHORT
            </label>
            <label style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '9px', color: '#787878', cursor: 'pointer', fontWeight: 600 }}>
              <input type="checkbox" checked={enableBenchmark} onChange={e => setEnableBenchmark(e.target.checked)} style={{ accentColor: '#FF8800' }} />
              BENCHMARK
            </label>
            {enableBenchmark && renderInput('Benchmark', benchmarkSymbol, setBenchmarkSymbol, 'text', 'SPY')}
          </div>
        </div>
      )}

      {/* ── Footer ── */}
      <div style={{
        padding: '6px 10px',
        borderTop: '1px solid #2a2a2a',
        backgroundColor: '#0f0f0f',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        gap: '6px',
        flexShrink: 0,
      }}>
        <div style={{ width: '4px', height: '4px', borderRadius: '50%', backgroundColor: '#FF8800' }} />
        <span style={{ color: '#4a4a4a', fontSize: '8px', letterSpacing: '0.5px', fontWeight: 700 }}>
          CLICK TO SELECT STRATEGY
        </span>
      </div>
    </div>
  );
};
