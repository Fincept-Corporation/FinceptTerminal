/**
 * ConfigPanel - Left panel for backtesting configuration
 *
 * Multi-symbol input, strategy selection, parameters,
 * position sizing, benchmark, and date range controls.
 */

import React, { useState } from 'react';
import { ChevronDown, ChevronRight, Play, X, Plus } from 'lucide-react';
import {
  F, inputStyle, labelStyle, selectStyle, sectionHeaderStyle, panelStyle, buttonStyle,
  BacktestConfigExtended, StrategyType, PositionSizing,
  STRATEGY_DEFINITIONS, getDefaultParameters,
} from './backtestingStyles';

interface ConfigPanelProps {
  config: BacktestConfigExtended;
  onConfigChange: (config: BacktestConfigExtended) => void;
  onRunBacktest: () => void;
  isRunning: boolean;
  availableProviders: string[];
  activeProvider: string | null;
  onProviderChange: (name: string) => void;
}

const ConfigPanel: React.FC<ConfigPanelProps> = ({
  config, onConfigChange, onRunBacktest, isRunning,
  availableProviders, activeProvider, onProviderChange,
}) => {
  const [expandedSections, setExpandedSections] = useState<Record<string, boolean>>({
    symbols: true, strategy: true, dates: true, execution: false, riskManagement: false, benchmark: false,
  });

  const [symbolInput, setSymbolInput] = useState('');

  const toggleSection = (key: string) => {
    setExpandedSections(prev => ({ ...prev, [key]: !prev[key] }));
  };

  const updateConfig = (partial: Partial<BacktestConfigExtended>) => {
    onConfigChange({ ...config, ...partial });
  };

  const addSymbol = () => {
    const sym = symbolInput.trim().toUpperCase();
    if (sym && !config.symbols.includes(sym)) {
      const newSymbols = [...config.symbols, sym];
      const newWeights = [...config.weights, 1.0 / newSymbols.length];
      // Normalize weights
      const total = newWeights.reduce((a, b) => a + b, 0);
      updateConfig({
        symbols: newSymbols,
        weights: newWeights.map(w => w / total),
      });
      setSymbolInput('');
    }
  };

  const removeSymbol = (idx: number) => {
    const newSymbols = config.symbols.filter((_, i) => i !== idx);
    const newWeights = config.weights.filter((_, i) => i !== idx);
    const total = newWeights.reduce((a, b) => a + b, 1);
    updateConfig({
      symbols: newSymbols,
      weights: newWeights.map(w => w / total),
    });
  };

  const setDateRange = (years: number | 'ytd' | 'max') => {
    const end = new Date().toISOString().split('T')[0];
    let start: string;
    if (years === 'ytd') {
      start = `${new Date().getFullYear()}-01-01`;
    } else if (years === 'max') {
      start = '2000-01-01';
    } else {
      const d = new Date();
      d.setFullYear(d.getFullYear() - years);
      start = d.toISOString().split('T')[0];
    }
    updateConfig({ startDate: start, endDate: end });
  };

  const strategyDef = STRATEGY_DEFINITIONS.find(s => s.type === config.strategyType);

  return (
    <div style={{ ...panelStyle, display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      {/* Provider Selector */}
      <div style={{ padding: '8px 12px', borderBottom: `1px solid ${F.BORDER}`, backgroundColor: F.HEADER_BG }}>
        <label style={labelStyle}>ENGINE</label>
        <select
          style={selectStyle}
          value={activeProvider || ''}
          onChange={e => onProviderChange(e.target.value)}
        >
          {availableProviders.map(p => (
            <option key={p} value={p}>{p}</option>
          ))}
        </select>
      </div>

      {/* Scrollable Config Sections */}
      <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
        {/* Symbols Section */}
        <div>
          <div style={sectionHeaderStyle} onClick={() => toggleSection('symbols')}>
            {expandedSections.symbols ? <ChevronDown size={12} color={F.ORANGE} /> : <ChevronRight size={12} color={F.GRAY} />}
            <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>SYMBOLS</span>
          </div>
          {expandedSections.symbols && (
            <div style={{ padding: '10px 12px' }}>
              <div style={{ display: 'flex', gap: '4px', marginBottom: '8px' }}>
                <input
                  style={{ ...inputStyle, flex: 1 }}
                  placeholder="AAPL, SPY..."
                  value={symbolInput}
                  onChange={e => setSymbolInput(e.target.value)}
                  onKeyDown={e => { if (e.key === 'Enter') addSymbol(); }}
                />
                <button
                  onClick={addSymbol}
                  style={{ ...buttonStyle, padding: '6px 8px', backgroundColor: F.ORANGE, color: F.DARK_BG }}
                >
                  <Plus size={10} />
                </button>
              </div>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                {config.symbols.map((sym, i) => (
                  <div key={sym} style={{
                    display: 'flex', alignItems: 'center', gap: '4px',
                    padding: '3px 8px', backgroundColor: F.HEADER_BG,
                    border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                    fontSize: '10px', color: F.WHITE, fontFamily: '"IBM Plex Mono", monospace',
                  }}>
                    <span>{sym}</span>
                    <span style={{ color: F.GRAY, fontSize: '9px' }}>
                      {(config.weights[i] * 100).toFixed(0)}%
                    </span>
                    <X size={8} color={F.GRAY} style={{ cursor: 'pointer' }} onClick={() => removeSymbol(i)} />
                  </div>
                ))}
              </div>
              {config.symbols.length === 0 && (
                <div style={{ fontSize: '9px', color: F.MUTED, marginTop: '4px' }}>Add at least one symbol</div>
              )}
            </div>
          )}
        </div>

        {/* Date Range Section */}
        <div>
          <div style={sectionHeaderStyle} onClick={() => toggleSection('dates')}>
            {expandedSections.dates ? <ChevronDown size={12} color={F.ORANGE} /> : <ChevronRight size={12} color={F.GRAY} />}
            <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>DATE RANGE</span>
          </div>
          {expandedSections.dates && (
            <div style={{ padding: '10px 12px' }}>
              <div style={{ display: 'flex', gap: '4px', marginBottom: '8px', flexWrap: 'wrap' }}>
                {[1, 2, 3, 5].map(y => (
                  <button key={y} onClick={() => setDateRange(y)} style={{
                    ...buttonStyle, padding: '4px 8px', fontSize: '9px',
                    backgroundColor: F.HEADER_BG, color: F.GRAY, border: `1px solid ${F.BORDER}`,
                  }}>{y}Y</button>
                ))}
                <button onClick={() => setDateRange('ytd')} style={{
                  ...buttonStyle, padding: '4px 8px', fontSize: '9px',
                  backgroundColor: F.HEADER_BG, color: F.GRAY, border: `1px solid ${F.BORDER}`,
                }}>YTD</button>
                <button onClick={() => setDateRange('max')} style={{
                  ...buttonStyle, padding: '4px 8px', fontSize: '9px',
                  backgroundColor: F.HEADER_BG, color: F.GRAY, border: `1px solid ${F.BORDER}`,
                }}>MAX</button>
              </div>
              <div style={{ display: 'flex', gap: '8px' }}>
                <div style={{ flex: 1 }}>
                  <label style={labelStyle}>FROM</label>
                  <input type="date" style={inputStyle} value={config.startDate}
                    onChange={e => updateConfig({ startDate: e.target.value })} />
                </div>
                <div style={{ flex: 1 }}>
                  <label style={labelStyle}>TO</label>
                  <input type="date" style={inputStyle} value={config.endDate}
                    onChange={e => updateConfig({ endDate: e.target.value })} />
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Strategy Section */}
        <div>
          <div style={sectionHeaderStyle} onClick={() => toggleSection('strategy')}>
            {expandedSections.strategy ? <ChevronDown size={12} color={F.ORANGE} /> : <ChevronRight size={12} color={F.GRAY} />}
            <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>STRATEGY</span>
          </div>
          {expandedSections.strategy && (
            <div style={{ padding: '10px 12px' }}>
              <label style={labelStyle}>TYPE</label>
              <select style={{ ...selectStyle, marginBottom: '10px' }}
                value={config.strategyType}
                onChange={e => {
                  const type = e.target.value as StrategyType;
                  updateConfig({ strategyType: type, parameters: getDefaultParameters(type) });
                }}
              >
                {STRATEGY_DEFINITIONS.map(s => (
                  <option key={s.type} value={s.type}>{s.label}</option>
                ))}
              </select>

              {strategyDef && (
                <div style={{ fontSize: '9px', color: F.MUTED, marginBottom: '10px' }}>
                  {strategyDef.description}
                </div>
              )}

              {/* Strategy Parameters */}
              {strategyDef && strategyDef.parameters.map(param => (
                <div key={param.key} style={{ marginBottom: '8px' }}>
                  <label style={labelStyle}>{param.label}</label>
                  <input
                    type="number"
                    style={inputStyle}
                    value={config.parameters[param.key] ?? param.default}
                    min={param.min}
                    max={param.max}
                    step={param.step}
                    onChange={e => {
                      const val = param.type === 'int' ? parseInt(e.target.value) : parseFloat(e.target.value);
                      if (!isNaN(val)) {
                        updateConfig({ parameters: { ...config.parameters, [param.key]: val } });
                      }
                    }}
                  />
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Execution Settings */}
        <div>
          <div style={sectionHeaderStyle} onClick={() => toggleSection('execution')}>
            {expandedSections.execution ? <ChevronDown size={12} color={F.ORANGE} /> : <ChevronRight size={12} color={F.GRAY} />}
            <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>EXECUTION</span>
          </div>
          {expandedSections.execution && (
            <div style={{ padding: '10px 12px' }}>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>INITIAL CAPITAL ($)</label>
                <input type="number" style={inputStyle} value={config.initialCapital}
                  onChange={e => updateConfig({ initialCapital: parseFloat(e.target.value) || 100000 })} />
              </div>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>COMMISSION (%)</label>
                <input type="number" style={inputStyle} step="0.01"
                  value={(config.commission * 100).toFixed(2)}
                  onChange={e => updateConfig({ commission: (parseFloat(e.target.value) || 0) / 100 })} />
              </div>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>SLIPPAGE (%)</label>
                <input type="number" style={inputStyle} step="0.01"
                  value={(config.slippage * 100).toFixed(2)}
                  onChange={e => updateConfig({ slippage: (parseFloat(e.target.value) || 0) / 100 })} />
              </div>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>LEVERAGE</label>
                <input type="number" style={inputStyle} step="0.1" min="1" max="10"
                  value={config.leverage}
                  onChange={e => updateConfig({ leverage: parseFloat(e.target.value) || 1 })} />
              </div>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>POSITION SIZING</label>
                <select style={selectStyle} value={config.positionSizing}
                  onChange={e => updateConfig({ positionSizing: e.target.value as PositionSizing })}>
                  <option value="fixed">Fixed Size</option>
                  <option value="kelly">Kelly Criterion</option>
                  <option value="fixed_fractional">Fixed Fractional</option>
                  <option value="vol_target">Volatility Target</option>
                </select>
              </div>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>
                  {config.positionSizing === 'fixed' ? 'SIZE (fraction of capital, e.g. 1.0 = 100%)'
                    : config.positionSizing === 'kelly' ? 'MAX SIZE CAP (fraction, e.g. 0.25 = 25%)'
                    : config.positionSizing === 'fixed_fractional' ? 'RISK FRACTION (e.g. 0.02 = 2%)'
                    : 'TARGET VOLATILITY (annual, e.g. 0.15 = 15%)'}
                </label>
                <input type="number" style={inputStyle} step="0.01" min="0.01" max="2.0"
                  value={config.positionSizeValue}
                  onChange={e => updateConfig({ positionSizeValue: parseFloat(e.target.value) || 1.0 })} />
              </div>
            </div>
          )}
        </div>

        {/* Risk Management Section */}
        <div>
          <div style={sectionHeaderStyle} onClick={() => toggleSection('riskManagement')}>
            {expandedSections.riskManagement ? <ChevronDown size={12} color={F.ORANGE} /> : <ChevronRight size={12} color={F.GRAY} />}
            <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>RISK MANAGEMENT</span>
          </div>
          {expandedSections.riskManagement && (
            <div style={{ padding: '10px 12px' }}>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>STOP LOSS (%)</label>
                <input type="number" style={inputStyle} step="0.5" min="0" max="50"
                  value={((config.stopLoss || 0) * 100).toFixed(1)}
                  onChange={e => updateConfig({ stopLoss: (parseFloat(e.target.value) || 0) / 100 })} />
                <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>0 = disabled</div>
              </div>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>TAKE PROFIT (%)</label>
                <input type="number" style={inputStyle} step="0.5" min="0" max="100"
                  value={((config.takeProfit || 0) * 100).toFixed(1)}
                  onChange={e => updateConfig({ takeProfit: (parseFloat(e.target.value) || 0) / 100 })} />
                <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>0 = disabled</div>
              </div>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>TRAILING STOP (%)</label>
                <input type="number" style={inputStyle} step="0.5" min="0" max="50"
                  value={((config.trailingStop || 0) * 100).toFixed(1)}
                  onChange={e => updateConfig({ trailingStop: (parseFloat(e.target.value) || 0) / 100 })} />
                <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>0 = disabled</div>
              </div>
              <div style={{ marginBottom: '8px' }}>
                <label style={labelStyle}>ALLOW SHORT SELLING</label>
                <select style={selectStyle}
                  value={config.allowShort ? 'yes' : 'no'}
                  onChange={e => updateConfig({ allowShort: e.target.value === 'yes' })}>
                  <option value="no">No (Long Only)</option>
                  <option value="yes">Yes</option>
                </select>
              </div>
            </div>
          )}
        </div>

        {/* Benchmark Section */}
        <div>
          <div style={sectionHeaderStyle} onClick={() => toggleSection('benchmark')}>
            {expandedSections.benchmark ? <ChevronDown size={12} color={F.ORANGE} /> : <ChevronRight size={12} color={F.GRAY} />}
            <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>BENCHMARK</span>
          </div>
          {expandedSections.benchmark && (
            <div style={{ padding: '10px 12px' }}>
              <label style={labelStyle}>BENCHMARK SYMBOL</label>
              <input style={inputStyle} placeholder="SPY"
                value={config.benchmark}
                onChange={e => updateConfig({ benchmark: e.target.value.toUpperCase() })} />
              <div style={{ fontSize: '9px', color: F.MUTED, marginTop: '4px', marginBottom: '8px' }}>
                Leave empty to skip benchmark comparison
              </div>
              <label style={labelStyle}>RANDOM PORTFOLIO COMPARISON</label>
              <select style={selectStyle}
                value={config.compareRandom ? 'yes' : 'no'}
                onChange={e => updateConfig({ compareRandom: e.target.value === 'yes' })}>
                <option value="no">Disabled</option>
                <option value="yes">Enabled (Statistical Significance)</option>
              </select>
              <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>
                Tests if strategy beats random entry/exit
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Run Button */}
      <div style={{ padding: '12px', borderTop: `1px solid ${F.BORDER}` }}>
        <button
          onClick={onRunBacktest}
          disabled={isRunning || config.symbols.length === 0}
          style={{
            ...buttonStyle, width: '100%', justifyContent: 'center',
            backgroundColor: isRunning ? F.MUTED : F.ORANGE,
            color: F.DARK_BG, opacity: (isRunning || config.symbols.length === 0) ? 0.5 : 1,
          }}
        >
          <Play size={12} />
          {isRunning ? 'RUNNING...' : 'RUN BACKTEST'}
        </button>
      </div>
    </div>
  );
};

export default ConfigPanel;
