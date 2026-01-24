/**
 * OptimizationPanel - Grid/Random/Walk-Forward optimization UI
 *
 * Allows users to define parameter ranges, select objective,
 * trigger optimization, and view results.
 */

import React, { useState } from 'react';
import { Play, Zap, TrendingUp, Plus, X } from 'lucide-react';
import {
  F, inputStyle, labelStyle, selectStyle, buttonStyle, panelStyle, sectionHeaderStyle,
  BacktestConfigExtended, STRATEGY_DEFINITIONS, formatNumber, formatPercent,
} from './backtestingStyles';

interface ParameterRange {
  key: string;
  label: string;
  min: number;
  max: number;
  step: number;
}

interface OptimizationResult {
  bestParameters: Record<string, number>;
  bestObjective: number;
  totalCombinations: number;
  results: { parameters: Record<string, number>; objective: number }[];
  sensitivity?: Record<string, number>;
  significance?: { zScore: number; pValue: number };
}

interface WalkForwardResult {
  splits: { trainReturn: number; testReturn: number; bestParams: Record<string, number> }[];
  oosReturn: number;
  degradation: number;
}

interface OptimizationPanelProps {
  config: BacktestConfigExtended;
  onOptimize: (params: {
    parameterRanges: ParameterRange[];
    objective: string;
    algorithm: 'grid' | 'random';
    maxIterations?: number;
  }) => Promise<OptimizationResult | null>;
  onWalkForward: (params: {
    parameterRanges: ParameterRange[];
    objective: string;
    nSplits: number;
    trainRatio: number;
  }) => Promise<WalkForwardResult | null>;
  isRunning: boolean;
}

const OptimizationPanel: React.FC<OptimizationPanelProps> = ({
  config, onOptimize, onWalkForward, isRunning,
}) => {
  const [objective, setObjective] = useState<string>('sharpe_ratio');
  const [algorithm, setAlgorithm] = useState<'grid' | 'random'>('grid');
  const [maxIterations, setMaxIterations] = useState(500);
  const [nSplits, setNSplits] = useState(5);
  const [trainRatio, setTrainRatio] = useState(0.7);
  const [paramRanges, setParamRanges] = useState<ParameterRange[]>([]);
  const [optimResult, setOptimResult] = useState<OptimizationResult | null>(null);
  const [wfResult, setWfResult] = useState<WalkForwardResult | null>(null);
  const [mode, setMode] = useState<'optimize' | 'walkforward'>('optimize');

  // Auto-populate param ranges from strategy definition
  const populateFromStrategy = () => {
    const def = STRATEGY_DEFINITIONS.find(s => s.type === config.strategyType);
    if (!def) return;
    setParamRanges(def.parameters.map(p => ({
      key: p.key,
      label: p.label,
      min: p.min,
      max: p.max,
      step: p.step * 2,
    })));
  };

  const removeRange = (idx: number) => {
    setParamRanges(prev => prev.filter((_, i) => i !== idx));
  };

  const updateRange = (idx: number, field: keyof ParameterRange, value: number) => {
    setParamRanges(prev => prev.map((r, i) => i === idx ? { ...r, [field]: value } : r));
  };

  const handleOptimize = async () => {
    if (paramRanges.length === 0) return;
    const result = await onOptimize({ parameterRanges: paramRanges, objective, algorithm, maxIterations });
    if (result) setOptimResult(result);
  };

  const handleWalkForward = async () => {
    if (paramRanges.length === 0) return;
    const result = await onWalkForward({ parameterRanges: paramRanges, objective, nSplits, trainRatio });
    if (result) setWfResult(result);
  };

  const objectives = [
    { value: 'sharpe_ratio', label: 'Sharpe Ratio' },
    { value: 'total_return', label: 'Total Return' },
    { value: 'sortino_ratio', label: 'Sortino Ratio' },
    { value: 'calmar_ratio', label: 'Calmar Ratio' },
    { value: 'profit_factor', label: 'Profit Factor' },
    { value: 'sqn', label: 'SQN' },
  ];

  return (
    <div style={{ ...panelStyle, display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      {/* Mode Toggle */}
      <div style={{ display: 'flex', borderBottom: `1px solid ${F.BORDER}`, backgroundColor: F.PANEL_BG }}>
        <button onClick={() => setMode('optimize')} style={{
          flex: 1, padding: '8px', fontSize: '9px', fontWeight: 700,
          color: mode === 'optimize' ? F.ORANGE : F.GRAY,
          backgroundColor: mode === 'optimize' ? F.HEADER_BG : 'transparent',
          border: 'none', borderBottom: mode === 'optimize' ? `2px solid ${F.ORANGE}` : '2px solid transparent',
          cursor: 'pointer', fontFamily: '"IBM Plex Mono", monospace',
        }}>
          <Zap size={10} style={{ marginRight: '4px' }} />OPTIMIZE
        </button>
        <button onClick={() => setMode('walkforward')} style={{
          flex: 1, padding: '8px', fontSize: '9px', fontWeight: 700,
          color: mode === 'walkforward' ? F.ORANGE : F.GRAY,
          backgroundColor: mode === 'walkforward' ? F.HEADER_BG : 'transparent',
          border: 'none', borderBottom: mode === 'walkforward' ? `2px solid ${F.ORANGE}` : '2px solid transparent',
          cursor: 'pointer', fontFamily: '"IBM Plex Mono", monospace',
        }}>
          <TrendingUp size={10} style={{ marginRight: '4px' }} />WALK-FORWARD
        </button>
      </div>

      <div style={{ flex: 1, overflowY: 'auto', padding: '10px 12px' }}>
        {/* Objective */}
        <div style={{ marginBottom: '10px' }}>
          <label style={labelStyle}>OBJECTIVE FUNCTION</label>
          <select style={selectStyle} value={objective} onChange={e => setObjective(e.target.value)}>
            {objectives.map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
          </select>
        </div>

        {mode === 'optimize' && (
          <>
            <div style={{ display: 'flex', gap: '8px', marginBottom: '10px' }}>
              <div style={{ flex: 1 }}>
                <label style={labelStyle}>ALGORITHM</label>
                <select style={selectStyle} value={algorithm} onChange={e => setAlgorithm(e.target.value as 'grid' | 'random')}>
                  <option value="grid">Grid Search</option>
                  <option value="random">Random Search</option>
                </select>
              </div>
              {algorithm === 'random' && (
                <div style={{ flex: 1 }}>
                  <label style={labelStyle}>MAX ITERATIONS</label>
                  <input type="number" style={inputStyle} value={maxIterations}
                    onChange={e => setMaxIterations(parseInt(e.target.value) || 500)} />
                </div>
              )}
            </div>
          </>
        )}

        {mode === 'walkforward' && (
          <div style={{ display: 'flex', gap: '8px', marginBottom: '10px' }}>
            <div style={{ flex: 1 }}>
              <label style={labelStyle}>SPLITS</label>
              <input type="number" style={inputStyle} min={2} max={20} value={nSplits}
                onChange={e => setNSplits(parseInt(e.target.value) || 5)} />
            </div>
            <div style={{ flex: 1 }}>
              <label style={labelStyle}>TRAIN RATIO</label>
              <input type="number" style={inputStyle} step={0.05} min={0.5} max={0.9} value={trainRatio}
                onChange={e => setTrainRatio(parseFloat(e.target.value) || 0.7)} />
            </div>
          </div>
        )}

        {/* Parameter Ranges */}
        <div style={{ marginBottom: '10px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
            <label style={{ ...labelStyle, marginBottom: 0 }}>PARAMETER RANGES</label>
            <button onClick={populateFromStrategy} style={{
              ...buttonStyle, padding: '3px 6px', fontSize: '8px',
              backgroundColor: F.HEADER_BG, color: F.CYAN, border: `1px solid ${F.BORDER}`,
            }}>
              AUTO-FILL
            </button>
          </div>

          {paramRanges.map((r, i) => (
            <div key={i} style={{
              display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '4px',
              padding: '4px 6px', backgroundColor: F.HEADER_BG, borderRadius: '2px',
              border: `1px solid ${F.BORDER}`,
            }}>
              <span style={{ fontSize: '9px', color: F.WHITE, width: '60px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                {r.label}
              </span>
              <input type="number" style={{ ...inputStyle, width: '50px', padding: '3px 4px' }}
                value={r.min} onChange={e => updateRange(i, 'min', parseFloat(e.target.value) || 0)} />
              <span style={{ fontSize: '8px', color: F.MUTED }}>→</span>
              <input type="number" style={{ ...inputStyle, width: '50px', padding: '3px 4px' }}
                value={r.max} onChange={e => updateRange(i, 'max', parseFloat(e.target.value) || 0)} />
              <span style={{ fontSize: '8px', color: F.MUTED }}>±</span>
              <input type="number" style={{ ...inputStyle, width: '40px', padding: '3px 4px' }}
                value={r.step} onChange={e => updateRange(i, 'step', parseFloat(e.target.value) || 1)} />
              <X size={10} color={F.GRAY} style={{ cursor: 'pointer', flexShrink: 0 }} onClick={() => removeRange(i)} />
            </div>
          ))}

          {paramRanges.length === 0 && (
            <div style={{ fontSize: '9px', color: F.MUTED, padding: '8px', textAlign: 'center' }}>
              Click AUTO-FILL to populate from current strategy
            </div>
          )}
        </div>

        {/* Run Button */}
        <button
          onClick={mode === 'optimize' ? handleOptimize : handleWalkForward}
          disabled={isRunning || paramRanges.length === 0}
          style={{
            ...buttonStyle, width: '100%', justifyContent: 'center', marginBottom: '12px',
            backgroundColor: isRunning ? F.MUTED : F.ORANGE,
            color: F.DARK_BG, opacity: (isRunning || paramRanges.length === 0) ? 0.5 : 1,
          }}
        >
          <Play size={10} />
          {isRunning ? 'RUNNING...' : mode === 'optimize' ? 'RUN OPTIMIZATION' : 'RUN WALK-FORWARD'}
        </button>

        {/* Optimization Results */}
        {optimResult && mode === 'optimize' && (
          <div style={{ marginTop: '8px' }}>
            <div style={{ fontSize: '9px', color: F.GRAY, fontWeight: 700, marginBottom: '6px' }}>OPTIMIZATION RESULTS</div>
            <div style={{ padding: '6px 8px', backgroundColor: F.HEADER_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', marginBottom: '6px' }}>
              <div style={{ fontSize: '9px', color: F.MUTED, marginBottom: '4px' }}>BEST PARAMETERS</div>
              {Object.entries(optimResult.bestParameters).map(([k, v]) => (
                <div key={k} style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                  <span style={{ color: F.WHITE }}>{k}</span>
                  <span style={{ color: F.CYAN, fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace' }}>{typeof v === 'number' ? formatNumber(v) : String(v)}</span>
                </div>
              ))}
            </div>
            <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
              <div style={{ padding: '4px 8px', backgroundColor: F.HEADER_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '9px' }}>
                <span style={{ color: F.MUTED }}>Best: </span>
                <span style={{ color: F.GREEN, fontWeight: 700 }}>{formatNumber(optimResult.bestObjective)}</span>
              </div>
              <div style={{ padding: '4px 8px', backgroundColor: F.HEADER_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '9px' }}>
                <span style={{ color: F.MUTED }}>Tested: </span>
                <span style={{ color: F.WHITE }}>{optimResult.totalCombinations}</span>
              </div>
              {optimResult.significance && (
                <div style={{ padding: '4px 8px', backgroundColor: F.HEADER_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '9px' }}>
                  <span style={{ color: F.MUTED }}>p-value: </span>
                  <span style={{ color: optimResult.significance.pValue < 0.05 ? F.GREEN : F.RED, fontWeight: 700 }}>
                    {optimResult.significance.pValue.toFixed(4)}
                  </span>
                </div>
              )}
            </div>
            {optimResult.sensitivity && Object.keys(optimResult.sensitivity).length > 0 && (
              <div style={{ marginTop: '8px' }}>
                <div style={{ fontSize: '9px', color: F.MUTED, marginBottom: '4px' }}>SENSITIVITY</div>
                {Object.entries(optimResult.sensitivity).map(([k, v]) => (
                  <div key={k} style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', padding: '2px 0' }}>
                    <span style={{ color: F.WHITE }}>{k}</span>
                    <span style={{ color: Math.abs(v) > 0.5 ? F.YELLOW : F.GRAY }}>{formatNumber(v)}</span>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}

        {/* Walk-Forward Results */}
        {wfResult && mode === 'walkforward' && (
          <div style={{ marginTop: '8px' }}>
            <div style={{ fontSize: '9px', color: F.GRAY, fontWeight: 700, marginBottom: '6px' }}>WALK-FORWARD RESULTS</div>
            <div style={{ display: 'flex', gap: '6px', marginBottom: '8px', flexWrap: 'wrap' }}>
              <div style={{ padding: '4px 8px', backgroundColor: F.HEADER_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '9px' }}>
                <span style={{ color: F.MUTED }}>OOS Return: </span>
                <span style={{ color: wfResult.oosReturn >= 0 ? F.GREEN : F.RED, fontWeight: 700 }}>
                  {formatPercent(wfResult.oosReturn)}
                </span>
              </div>
              <div style={{ padding: '4px 8px', backgroundColor: F.HEADER_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '9px' }}>
                <span style={{ color: F.MUTED }}>Degradation: </span>
                <span style={{ color: wfResult.degradation < 0.3 ? F.GREEN : F.RED, fontWeight: 700 }}>
                  {formatPercent(wfResult.degradation)}
                </span>
              </div>
            </div>
            <div style={{ fontSize: '9px', color: F.MUTED, marginBottom: '4px' }}>SPLIT PERFORMANCE</div>
            {wfResult.splits.map((s, i) => (
              <div key={i} style={{
                display: 'flex', justifyContent: 'space-between', fontSize: '9px',
                padding: '3px 6px', marginBottom: '2px',
                backgroundColor: i % 2 === 0 ? F.HEADER_BG : 'transparent',
                borderRadius: '2px',
              }}>
                <span style={{ color: F.GRAY }}>Split {i + 1}</span>
                <span style={{ color: F.CYAN }}>IS: {formatPercent(s.trainReturn)}</span>
                <span style={{ color: s.testReturn >= 0 ? F.GREEN : F.RED }}>OOS: {formatPercent(s.testReturn)}</span>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

export default OptimizationPanel;
