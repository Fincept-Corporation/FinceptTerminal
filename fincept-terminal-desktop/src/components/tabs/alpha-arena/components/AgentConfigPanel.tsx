/**
 * Agent Config Panel
 *
 * Expandable per-model configuration panel for Alpha Arena.
 * Allows LLM parameter tuning, trading style selection, and feature toggles.
 */

import React, { useState } from 'react';
import {
  Settings, ChevronDown, ChevronUp, Check,
  Brain, Cpu, BarChart3, BookOpen, Shield, Activity, MessageSquare, Search,
} from 'lucide-react';
import { type AlphaArenaModel } from '../useAlphaArena';
import { DEFAULT_ADVANCED_CONFIG, FULL_AGENT_CONFIG } from '../types';
import type { AgentAdvancedConfig } from '../types';
import { FINCEPT, TERMINAL_FONT } from '../constants';

const AgentConfigPanel: React.FC<{
  model: AlphaArenaModel;
  onUpdate: (modelId: string, config: Partial<AgentAdvancedConfig>) => void;
}> = ({ model, onUpdate }) => {
  const [expanded, setExpanded] = useState(false);
  const cfg = model.advancedConfig;

  const isFullAgent = cfg.enable_memory && cfg.enable_reasoning && cfg.enable_tools &&
    cfg.enable_knowledge && cfg.enable_sentiment && cfg.enable_research;

  const applyPreset = (preset: 'simple' | 'full') => {
    if (preset === 'simple') {
      onUpdate(model.id, { ...DEFAULT_ADVANCED_CONFIG });
    } else {
      onUpdate(model.id, { ...FULL_AGENT_CONFIG });
    }
  };

  const toggle = (key: keyof AgentAdvancedConfig) => {
    onUpdate(model.id, { [key]: !cfg[key] } as Partial<AgentAdvancedConfig>);
  };

  const renderToggle = (label: string, key: keyof AgentAdvancedConfig, icon: React.ReactNode, desc: string) => (
    <button
      type="button"
      onClick={() => toggle(key)}
      style={{
        display: 'flex', alignItems: 'center', gap: '6px', padding: '5px 8px', width: '100%',
        backgroundColor: cfg[key] ? `${FINCEPT.GREEN}10` : FINCEPT.CARD_BG,
        border: `1px solid ${cfg[key] ? `${FINCEPT.GREEN}40` : FINCEPT.BORDER}`,
        cursor: 'pointer', textAlign: 'left', transition: 'all 0.15s',
      }}
    >
      <div style={{
        width: '12px', height: '12px', borderRadius: '2px', flexShrink: 0,
        backgroundColor: cfg[key] ? FINCEPT.GREEN : FINCEPT.BORDER,
        display: 'flex', alignItems: 'center', justifyContent: 'center',
      }}>
        {cfg[key] && <Check size={8} style={{ color: FINCEPT.DARK_BG }} />}
      </div>
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          {icon}
          <span style={{ fontSize: '10px', fontWeight: 600, color: cfg[key] ? FINCEPT.GREEN : FINCEPT.WHITE }}>{label}</span>
        </div>
        <span style={{ fontSize: '8px', color: FINCEPT.GRAY }}>{desc}</span>
      </div>
    </button>
  );

  const renderSlider = (label: string, value: number, min: number, max: number, step: number,
    onChange: (v: number) => void, format?: (v: number) => string) => (
    <div style={{ marginBottom: '6px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 600 }}>{label}</span>
        <span style={{ fontSize: '9px', color: FINCEPT.ORANGE, fontWeight: 600 }}>{format ? format(value) : value}</span>
      </div>
      <input type="range" min={min} max={max} step={step} value={value}
        onChange={e => onChange(Number(e.target.value))}
        style={{ width: '100%', height: '4px', cursor: 'pointer', accentColor: FINCEPT.ORANGE }}
      />
    </div>
  );

  if (!expanded) {
    return (
      <button type="button" onClick={() => setExpanded(true)}
        style={{
          display: 'flex', alignItems: 'center', gap: '4px', padding: '3px 8px', width: '100%',
          backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, borderTop: 'none',
          cursor: 'pointer', fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 600,
          transition: 'all 0.15s',
        }}
        onMouseEnter={e => { e.currentTarget.style.color = FINCEPT.ORANGE; e.currentTarget.style.borderColor = `${FINCEPT.ORANGE}40`; }}
        onMouseLeave={e => { e.currentTarget.style.color = FINCEPT.GRAY; e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
      >
        <Settings size={9} />
        <span>CONFIGURE</span>
        <ChevronDown size={9} />
        <span style={{ marginLeft: 'auto', fontSize: '8px', color: isFullAgent ? FINCEPT.GREEN : FINCEPT.MUTED }}>
          {isFullAgent ? 'FULL AGENT' : 'SIMPLE'}
        </span>
      </button>
    );
  }

  return (
    <div style={{
      border: `1px solid ${FINCEPT.ORANGE}40`, borderTop: 'none',
      backgroundColor: `${FINCEPT.PANEL_BG}`,
    }}>
      {/* Header */}
      <button type="button" onClick={() => setExpanded(false)}
        style={{
          display: 'flex', alignItems: 'center', gap: '4px', padding: '4px 8px', width: '100%',
          backgroundColor: `${FINCEPT.ORANGE}10`, border: 'none', borderBottom: `1px solid ${FINCEPT.ORANGE}30`,
          cursor: 'pointer', fontSize: '9px', color: FINCEPT.ORANGE, fontWeight: 700, letterSpacing: '0.5px',
        }}
      >
        <Settings size={10} />
        <span>AGENT CONFIG</span>
        <ChevronUp size={9} />
      </button>

      <div style={{ padding: '6px 8px' }}>
        {/* Agent Mode Preset */}
        <div style={{ display: 'flex', gap: '4px', marginBottom: '8px' }}>
          <button type="button" onClick={() => applyPreset('simple')}
            style={{
              flex: 1, padding: '4px 6px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              backgroundColor: !isFullAgent ? `${FINCEPT.ORANGE}20` : FINCEPT.CARD_BG,
              color: !isFullAgent ? FINCEPT.ORANGE : FINCEPT.GRAY,
              border: `1px solid ${!isFullAgent ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
            }}>RAW SIMPLE</button>
          <button type="button" onClick={() => applyPreset('full')}
            style={{
              flex: 1, padding: '4px 6px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              backgroundColor: isFullAgent ? `${FINCEPT.GREEN}20` : FINCEPT.CARD_BG,
              color: isFullAgent ? FINCEPT.GREEN : FINCEPT.GRAY,
              border: `1px solid ${isFullAgent ? FINCEPT.GREEN : FINCEPT.BORDER}`,
            }}>FULLY LOADED</button>
        </div>

        {/* LLM Parameters */}
        <div style={{ marginBottom: '8px', padding: '6px', backgroundColor: FINCEPT.CARD_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>LLM PARAMETERS</div>
          {renderSlider('Temperature', cfg.temperature, 0, 2, 0.1,
            v => onUpdate(model.id, { temperature: v }), v => v.toFixed(1))}
          {renderSlider('Max Tokens', cfg.max_tokens, 100, 8000, 100,
            v => onUpdate(model.id, { max_tokens: v }), v => v.toString())}
        </div>

        {/* Trading Style */}
        <div style={{ marginBottom: '8px' }}>
          <label style={{ display: 'block', fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '3px' }}>TRADING STYLE</label>
          <select value={cfg.trading_style || ''} onChange={e => onUpdate(model.id, { trading_style: e.target.value || null })}
            style={{
              width: '100%', padding: '4px 6px', backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`, fontSize: '10px', fontFamily: TERMINAL_FONT, outline: 'none',
            }}>
            <option value="" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Auto-assign</option>
            <option value="aggressive" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Aggressive</option>
            <option value="conservative" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Conservative</option>
            <option value="momentum" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Momentum</option>
            <option value="contrarian" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Contrarian</option>
            <option value="scalper" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Scalper</option>
            <option value="swing" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Swing Trader</option>
            <option value="technical" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Technical</option>
            <option value="fundamental" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Fundamental</option>
            <option value="neutral" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Market Neutral</option>
            <option value="value" style={{ backgroundColor: FINCEPT.HEADER_BG }}>Value</option>
          </select>
        </div>

        {/* Agno Feature Toggles */}
        <div style={{ marginBottom: '8px', padding: '6px', backgroundColor: FINCEPT.CARD_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>AGENT FEATURES</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '3px' }}>
            {renderToggle('Memory', 'enable_memory', <Brain size={9} style={{ color: FINCEPT.PURPLE }} />, 'Trade history recall')}
            {renderToggle('Reasoning', 'enable_reasoning', <Cpu size={9} style={{ color: FINCEPT.CYAN }} />, 'Step-by-step analysis')}
            {renderToggle('Tools', 'enable_tools', <BarChart3 size={9} style={{ color: FINCEPT.BLUE }} />, 'Tech indicators')}
            {renderToggle('Knowledge', 'enable_knowledge', <BookOpen size={9} style={{ color: FINCEPT.YELLOW }} />, 'Market knowledge')}
            {renderToggle('Guardrails', 'enable_guardrails', <Shield size={9} style={{ color: FINCEPT.GREEN }} />, 'Position limits')}
            {renderToggle('Features', 'enable_features_pipeline', <Activity size={9} style={{ color: FINCEPT.ORANGE }} />, 'RSI, MACD, etc.')}
            {renderToggle('Sentiment', 'enable_sentiment', <MessageSquare size={9} style={{ color: '#EC4899' }} />, 'News sentiment')}
            {renderToggle('Research', 'enable_research', <Search size={9} style={{ color: '#14B8A6' }} />, 'SEC filings')}
          </div>
        </div>

        {/* Risk Management (when guardrails enabled) */}
        {cfg.enable_guardrails && (
          <div style={{ marginBottom: '8px', padding: '6px', backgroundColor: FINCEPT.CARD_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>RISK MANAGEMENT</div>
            {renderSlider('Max Position %', cfg.max_position_pct, 0.1, 1, 0.05,
              v => onUpdate(model.id, { max_position_pct: v }), v => `${(v * 100).toFixed(0)}%`)}
            {renderSlider('Max Single Trade %', cfg.max_single_trade_pct, 0.05, 0.5, 0.05,
              v => onUpdate(model.id, { max_single_trade_pct: v }), v => `${(v * 100).toFixed(0)}%`)}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
              <div>
                <label style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 600 }}>Stop Loss %</label>
                <input type="number" min="1" max="50" step="1" placeholder="—"
                  value={cfg.stop_loss_pct !== null ? cfg.stop_loss_pct * 100 : ''}
                  onChange={e => onUpdate(model.id, { stop_loss_pct: e.target.value ? Number(e.target.value) / 100 : null })}
                  style={{
                    width: '100%', padding: '3px 6px', backgroundColor: FINCEPT.HEADER_BG,
                    color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, fontSize: '10px',
                    fontFamily: TERMINAL_FONT, outline: 'none', boxSizing: 'border-box',
                  }}
                />
              </div>
              <div>
                <label style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 600 }}>Take Profit %</label>
                <input type="number" min="1" max="100" step="1" placeholder="—"
                  value={cfg.take_profit_pct !== null ? cfg.take_profit_pct * 100 : ''}
                  onChange={e => onUpdate(model.id, { take_profit_pct: e.target.value ? Number(e.target.value) / 100 : null })}
                  style={{
                    width: '100%', padding: '3px 6px', backgroundColor: FINCEPT.HEADER_BG,
                    color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, fontSize: '10px',
                    fontFamily: TERMINAL_FONT, outline: 'none', boxSizing: 'border-box',
                  }}
                />
              </div>
            </div>
          </div>
        )}

        {/* Reasoning config (when reasoning enabled) */}
        {cfg.enable_reasoning && (
          <div style={{ marginBottom: '8px', padding: '6px', backgroundColor: FINCEPT.CARD_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>REASONING</div>
            {renderSlider('Min Steps', cfg.reasoning_min_steps, 1, 5, 1,
              v => onUpdate(model.id, { reasoning_min_steps: v }))}
            {renderSlider('Max Steps', cfg.reasoning_max_steps, 3, 15, 1,
              v => onUpdate(model.id, { reasoning_max_steps: v }))}
          </div>
        )}

        {/* Custom Prompt */}
        <div>
          <label style={{ display: 'block', fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '3px' }}>CUSTOM PROMPT (optional)</label>
          <textarea value={cfg.custom_system_prompt}
            onChange={e => onUpdate(model.id, { custom_system_prompt: e.target.value })}
            placeholder="Override system prompt..."
            rows={2}
            style={{
              width: '100%', padding: '4px 6px', backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`, fontSize: '9px', fontFamily: TERMINAL_FONT,
              outline: 'none', resize: 'vertical', boxSizing: 'border-box',
            }}
            onFocus={e => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
            onBlur={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
          />
        </div>
      </div>
    </div>
  );
};

export default AgentConfigPanel;
