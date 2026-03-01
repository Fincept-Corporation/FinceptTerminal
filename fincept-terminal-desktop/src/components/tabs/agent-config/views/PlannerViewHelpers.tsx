/**
 * PlannerView - Types, Constants, Helpers, and Sub-Components
 */

import React, { useState } from 'react';
import {
  ChevronDown, ChevronRight, Clock, AlertCircle,
  CheckCircle, XCircle, BarChart2, TrendingUp, Shield, Layers,
  FileText, Plus, ArrowUp, ArrowDown, Edit2, Save, X, Settings,
  Loader2,
} from 'lucide-react';
import { FINCEPT } from '../types';
import type { ExecutionPlan, PlanStep } from '@/services/agentService';

// ─── Types ────────────────────────────────────────────────────────────────────

export interface PlanHistoryEntry {
  id: string;
  symbol: string;
  planType: string;
  timestamp: Date;
  plan: ExecutionPlan;
  label: string;
}

export type PlanTemplate = {
  id: string;
  name: string;
  desc: string;
  icon: React.ReactNode;
  defaultSymbol: string;
  color: string;
};

// Editable config row: {key, value} pairs that map to step.config
export interface ConfigRow {
  key: string;
  value: string;
}

// ─── Content Extraction ───────────────────────────────────────────────────────

export function extractContextContent(value: unknown): string {
  if (typeof value === 'string') return value;
  if (typeof value !== 'object' || value === null) return String(value ?? '');

  const obj = value as Record<string, unknown>;

  for (const field of ['result', 'output', 'content', 'text', 'analysis', 'summary', 'response']) {
    if (typeof obj[field] === 'string' && (obj[field] as string).length > 0) {
      return obj[field] as string;
    }
  }

  const entries = Object.entries(obj);
  const stringEntries = entries.filter(([, v]) => typeof v === 'string' && (v as string).trim().length > 0);

  if (stringEntries.length > 0 && stringEntries.length >= Math.ceil(entries.length * 0.6)) {
    return stringEntries
      .map(([key, val]) => {
        const heading = key.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
        return `## ${heading}\n\n${val as string}`;
      })
      .join('\n\n---\n\n');
  }

  return '```json\n' + JSON.stringify(value, null, 2) + '\n```';
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

export function deepClonePlan(plan: ExecutionPlan): ExecutionPlan {
  return JSON.parse(JSON.stringify(plan));
}

export function makeNewStep(index: number): PlanStep {
  return {
    id: `custom_step_${Date.now()}_${index}`,
    name: `Custom Step ${index + 1}`,
    step_type: 'analysis',
    config: {},
    dependencies: [],
    status: 'pending',
  };
}

export function configToRows(config: Record<string, any>): ConfigRow[] {
  return Object.entries(config).map(([key, value]) => ({
    key,
    value: typeof value === 'string' ? value : JSON.stringify(value),
  }));
}

export function rowsToConfig(rows: ConfigRow[]): Record<string, any> {
  const result: Record<string, any> = {};
  for (const row of rows) {
    if (!row.key.trim()) continue;
    try {
      result[row.key.trim()] = JSON.parse(row.value);
    } catch {
      result[row.key.trim()] = row.value;
    }
  }
  return result;
}

// ─── Constants ────────────────────────────────────────────────────────────────

export const PLAN_TEMPLATES: PlanTemplate[] = [
  {
    id: 'stock_analysis',
    name: 'Stock Analysis',
    desc: 'Fundamental + technical deep-dive',
    icon: <TrendingUp size={12} />,
    defaultSymbol: 'AAPL',
    color: FINCEPT.ORANGE,
  },
  {
    id: 'portfolio_rebalance',
    name: 'Portfolio Rebalance',
    desc: 'Optimize asset allocation',
    icon: <BarChart2 size={12} />,
    defaultSymbol: 'PORTFOLIO',
    color: FINCEPT.CYAN,
  },
  {
    id: 'risk_audit',
    name: 'Risk Audit',
    desc: 'VaR, drawdown & stress test',
    icon: <Shield size={12} />,
    defaultSymbol: 'PORTFOLIO',
    color: FINCEPT.RED,
  },
  {
    id: 'sector_rotation',
    name: 'Sector Rotation',
    desc: 'Identify rotation opportunities',
    icon: <Layers size={12} />,
    defaultSymbol: 'SPY',
    color: FINCEPT.GREEN,
  },
];

export const STEP_TYPES = [
  'fetch', 'analysis', 'research', 'synthesis',
  'technical', 'fundamental', 'news', 'sentiment',
  'risk', 'portfolio', 'report', 'custom',
];

export const STATUS_COLOR: Record<string, string> = {
  completed: FINCEPT.GREEN,
  failed: FINCEPT.RED,
  running: FINCEPT.ORANGE,
  pending: FINCEPT.GRAY,
  skipped: FINCEPT.MUTED,
};

export const STATUS_ICON: Record<string, React.ReactNode> = {
  completed: <CheckCircle size={12} />,
  failed: <XCircle size={12} />,
  running: <Loader2 size={12} className="animate-spin" />,
  pending: <Clock size={12} />,
  skipped: <AlertCircle size={12} />,
};

// ─── Inline field style helpers ───────────────────────────────────────────────

export const inputStyle = (focused?: boolean): React.CSSProperties => ({
  width: '100%',
  padding: '5px 8px',
  backgroundColor: FINCEPT.DARK_BG,
  color: FINCEPT.WHITE,
  border: `1px solid ${focused ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
  borderRadius: '2px',
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", monospace',
  outline: 'none',
  boxSizing: 'border-box' as const,
});

// ─── StepEditor component ─────────────────────────────────────────────────────

interface StepEditorProps {
  step: PlanStep;
  onSave: (updated: PlanStep) => void;
  onCancel: () => void;
}

export const StepEditor: React.FC<StepEditorProps> = ({ step, onSave, onCancel }) => {
  const [name, setName] = useState(step.name);
  const [stepType, setStepType] = useState(step.step_type);
  const [configRows, setConfigRows] = useState<ConfigRow[]>(() =>
    configToRows(step.config || {}).length > 0
      ? configToRows(step.config || {})
      : [{ key: '', value: '' }]
  );
  const [depsText, setDepsText] = useState(step.dependencies.join(', '));
  const [nameFocused, setNameFocused] = useState(false);
  const [depsFocused, setDepsFocused] = useState(false);

  const addConfigRow = () => setConfigRows(prev => [...prev, { key: '', value: '' }]);

  const removeConfigRow = (i: number) =>
    setConfigRows(prev => prev.filter((_, idx) => idx !== i));

  const updateConfigRow = (i: number, field: 'key' | 'value', val: string) =>
    setConfigRows(prev => prev.map((row, idx) => idx === i ? { ...row, [field]: val } : row));

  const handleSave = () => {
    onSave({
      ...step,
      name: name.trim() || step.name,
      step_type: stepType,
      config: rowsToConfig(configRows),
      dependencies: depsText.split(',').map(s => s.trim()).filter(Boolean),
    });
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
      {/* Name */}
      <div>
        <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>STEP NAME</div>
        <input
          value={name}
          onChange={e => setName(e.target.value)}
          style={inputStyle(nameFocused)}
          onFocus={() => setNameFocused(true)}
          onBlur={() => setNameFocused(false)}
        />
      </div>

      {/* Step type */}
      <div>
        <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>STEP TYPE</div>
        <select
          value={stepType}
          onChange={e => setStepType(e.target.value)}
          style={{
            ...inputStyle(),
            backgroundColor: FINCEPT.DARK_BG,
            cursor: 'pointer',
            appearance: 'none' as const,
          }}
        >
          {STEP_TYPES.map(t => (
            <option key={t} value={t} style={{ backgroundColor: FINCEPT.DARK_BG }}>{t}</option>
          ))}
          {!STEP_TYPES.includes(stepType) && (
            <option value={stepType} style={{ backgroundColor: FINCEPT.DARK_BG }}>{stepType}</option>
          )}
        </select>
      </div>

      {/* Dependencies */}
      <div>
        <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>DEPENDENCIES <span style={{ color: FINCEPT.MUTED, fontWeight: 400 }}>(comma separated step IDs)</span></div>
        <input
          value={depsText}
          onChange={e => setDepsText(e.target.value)}
          placeholder="step_id_1, step_id_2"
          style={inputStyle(depsFocused)}
          onFocus={() => setDepsFocused(true)}
          onBlur={() => setDepsFocused(false)}
        />
      </div>

      {/* Config key-value pairs */}
      <div>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
          <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>CONFIG</div>
          <button
            onClick={addConfigRow}
            style={{ background: 'none', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', color: FINCEPT.ORANGE, padding: '2px 6px', fontSize: '8px', display: 'flex', alignItems: 'center', gap: '3px' }}
          >
            <Plus size={9} /> ADD KEY
          </button>
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          {configRows.map((row, i) => (
            <div key={i} style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
              <input
                value={row.key}
                onChange={e => updateConfigRow(i, 'key', e.target.value)}
                placeholder="key"
                style={{ ...inputStyle(), flex: '0 0 36%' }}
              />
              <input
                value={row.value}
                onChange={e => updateConfigRow(i, 'value', e.target.value)}
                placeholder="value"
                style={{ ...inputStyle(), flex: 1 }}
              />
              <button
                onClick={() => removeConfigRow(i)}
                style={{ background: 'none', border: 'none', cursor: 'pointer', color: FINCEPT.MUTED, padding: '4px', flexShrink: 0, display: 'flex', alignItems: 'center' }}
              >
                <X size={10} />
              </button>
            </div>
          ))}
          {configRows.length === 0 && (
            <div style={{ fontSize: '8px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>No config — click ADD KEY to add parameters</div>
          )}
        </div>
      </div>

      {/* Save / Cancel */}
      <div style={{ display: 'flex', gap: '6px', paddingTop: '4px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
        <button
          onClick={handleSave}
          style={{ flex: 1, padding: '6px', backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', cursor: 'pointer', fontSize: '8px', fontWeight: 700, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}
        >
          <Save size={10} /> SAVE STEP
        </button>
        <button
          onClick={onCancel}
          style={{ flex: 1, padding: '6px', background: 'none', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', fontSize: '8px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}
        >
          <X size={10} /> CANCEL
        </button>
      </div>
    </div>
  );
};

// ─── StepCard ─────────────────────────────────────────────────────────────────

export interface StepCardProps {
  step: PlanStep;
  index: number;
  total: number;
  isExpanded: boolean;
  isEditing: boolean;
  canEdit: boolean;
  onToggle: () => void;
  onJumpToResult: () => void;
  onEdit: () => void;
  onSave: (updated: PlanStep) => void;
  onCancelEdit: () => void;
  onDelete: () => void;
  onMoveUp: () => void;
  onMoveDown: () => void;
}

export const StepCard: React.FC<StepCardProps> = ({
  step, index, total, isExpanded, isEditing, canEdit,
  onToggle, onJumpToResult, onEdit, onSave, onCancelEdit, onDelete, onMoveUp, onMoveDown,
}) => {
  const color = STATUS_COLOR[step.status] ?? FINCEPT.GRAY;
  const icon = STATUS_ICON[step.status] ?? <Clock size={12} />;
  const resultPreview = step.result ? extractContextContent(step.result).slice(0, 160) : null;

  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${isEditing ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
      borderLeft: `3px solid ${isEditing ? FINCEPT.ORANGE : color}`,
      borderRadius: '2px',
      overflow: 'hidden',
      transition: 'border-color 0.15s',
    }}>
      {/* ── Header row ───────────────────────────────────────────────── */}
      <div style={{ padding: '9px 12px', display: 'flex', alignItems: 'center', gap: '8px' }}>

        {/* Drag handle / index */}
        <span style={{
          width: '22px', height: '22px', minWidth: '22px',
          display: 'flex', alignItems: 'center', justifyContent: 'center',
          backgroundColor: FINCEPT.DARK_BG, color: isEditing ? FINCEPT.ORANGE : color,
          fontSize: '9px', fontWeight: 700, borderRadius: '2px',
          border: `1px solid ${isEditing ? FINCEPT.ORANGE : color}30`,
          flexShrink: 0,
        }}>
          {index + 1}
        </span>

        {/* Name + type — clicking toggles expand */}
        <div style={{ flex: 1, minWidth: 0, cursor: 'pointer', userSelect: 'none' }} onClick={onToggle}>
          <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
            {step.name}
          </div>
          <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '1px' }}>
            <span style={{ color: FINCEPT.CYAN }}>{step.step_type}</span>
            {step.dependencies.length > 0 && (
              <span style={{ color: FINCEPT.GRAY }}> • deps: {step.dependencies.join(', ')}</span>
            )}
          </div>
        </div>

        {/* Action buttons (only visible for pending steps) */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px', flexShrink: 0 }}>
          {canEdit && (
            <>
              {/* Move up */}
              <button
                onClick={onMoveUp}
                disabled={index === 0}
                title="Move step up"
                style={{ background: 'none', border: 'none', cursor: index === 0 ? 'not-allowed' : 'pointer', color: index === 0 ? FINCEPT.BORDER : FINCEPT.GRAY, padding: '3px', display: 'flex', alignItems: 'center' }}
              >
                <ArrowUp size={10} />
              </button>
              {/* Move down */}
              <button
                onClick={onMoveDown}
                disabled={index === total - 1}
                title="Move step down"
                style={{ background: 'none', border: 'none', cursor: index === total - 1 ? 'not-allowed' : 'pointer', color: index === total - 1 ? FINCEPT.BORDER : FINCEPT.GRAY, padding: '3px', display: 'flex', alignItems: 'center' }}
              >
                <ArrowDown size={10} />
              </button>
              {/* Edit */}
              <button
                onClick={e => { e.stopPropagation(); onEdit(); }}
                title="Edit step"
                style={{ background: 'none', border: `1px solid ${isEditing ? FINCEPT.ORANGE : FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', color: isEditing ? FINCEPT.ORANGE : FINCEPT.GRAY, padding: '3px 5px', display: 'flex', alignItems: 'center' }}
              >
                <Edit2 size={10} />
              </button>
              {/* Delete */}
              <button
                onClick={e => { e.stopPropagation(); onDelete(); }}
                title="Remove step"
                style={{ background: 'none', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', color: FINCEPT.RED, padding: '3px 5px', display: 'flex', alignItems: 'center' }}
              >
                <X size={10} />
              </button>
            </>
          )}

          {/* Jump to result */}
          {step.status === 'completed' && (
            <button
              onClick={e => { e.stopPropagation(); onJumpToResult(); }}
              title="View result"
              style={{ background: 'none', border: 'none', cursor: 'pointer', color: FINCEPT.CYAN, display: 'flex', alignItems: 'center', padding: '3px' }}
            >
              <FileText size={11} />
            </button>
          )}

          {/* Status badge */}
          <span style={{
            display: 'flex', alignItems: 'center', gap: '4px',
            padding: '3px 7px',
            backgroundColor: `${color}15`,
            color,
            fontSize: '8px', fontWeight: 700, borderRadius: '2px',
          }}>
            {icon}
            {step.status.toUpperCase()}
          </span>

          {/* Expand toggle */}
          <span style={{ cursor: 'pointer', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center' }} onClick={onToggle}>
            {isExpanded ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
          </span>
        </div>
      </div>

      {/* ── Expanded body ─────────────────────────────────────────────── */}
      {isExpanded && (
        <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, padding: '12px', backgroundColor: `${FINCEPT.DARK_BG}80` }}>

          {/* Editor mode */}
          {isEditing ? (
            <StepEditor
              step={step}
              onSave={onSave}
              onCancel={onCancelEdit}
            />
          ) : (
            <>
              {/* Error */}
              {step.error && (
                <div style={{ padding: '8px', backgroundColor: `${FINCEPT.RED}10`, border: `1px solid ${FINCEPT.RED}30`, borderRadius: '2px', marginBottom: '8px' }}>
                  <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.RED, marginBottom: '4px' }}>ERROR</div>
                  <div style={{ fontSize: '9px', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace', whiteSpace: 'pre-wrap' }}>{step.error}</div>
                </div>
              )}

              {/* Result preview */}
              {resultPreview && (
                <div style={{ marginBottom: '8px' }}>
                  <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '4px', letterSpacing: '0.5px' }}>RESULT PREVIEW</div>
                  <div style={{ fontSize: '9px', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace', whiteSpace: 'pre-wrap', wordBreak: 'break-word', lineHeight: '1.4', opacity: 0.85 }}>
                    {resultPreview}{resultPreview.length >= 160 ? '…' : ''}
                  </div>
                </div>
              )}

              {/* Config viewer */}
              {Object.keys(step.config || {}).length > 0 && (
                <div style={{ marginBottom: '8px' }}>
                  <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>CONFIG</div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '3px' }}>
                    {Object.entries(step.config).map(([k, v]) => (
                      <div key={k} style={{ display: 'flex', gap: '8px', alignItems: 'flex-start' }}>
                        <span style={{ fontSize: '8px', color: FINCEPT.CYAN, fontFamily: '"IBM Plex Mono", monospace', flexShrink: 0, paddingTop: '1px' }}>{k}:</span>
                        <span style={{ fontSize: '8px', color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace', wordBreak: 'break-word' }}>
                          {typeof v === 'string' ? v : JSON.stringify(v)}
                        </span>
                      </div>
                    ))}
                  </div>
                </div>
              )}

              {/* No content state */}
              {!step.error && !resultPreview && Object.keys(step.config || {}).length === 0 && (
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                  {step.status === 'pending'
                    ? canEdit
                      ? 'Click the edit icon above to configure this step.'
                      : 'Waiting to execute…'
                    : 'No output yet'}
                </div>
              )}

              {/* Edit prompt for pending steps with no config */}
              {canEdit && step.status === 'pending' && Object.keys(step.config || {}).length === 0 && !step.error && (
                <button
                  onClick={onEdit}
                  style={{ marginTop: '4px', padding: '5px 10px', background: 'none', border: `1px dashed ${FINCEPT.ORANGE}60`, borderRadius: '2px', cursor: 'pointer', fontSize: '8px', color: FINCEPT.ORANGE, display: 'flex', alignItems: 'center', gap: '4px' }}
                >
                  <Settings size={9} /> CONFIGURE STEP
                </button>
              )}
            </>
          )}
        </div>
      )}
    </div>
  );
};

// ─── Progress Bar ─────────────────────────────────────────────────────────────

export const PlanProgressBar: React.FC<{ steps: PlanStep[] }> = ({ steps }) => {
  if (!steps.length) return null;
  const completed = steps.filter(s => s.status === 'completed').length;
  const failed = steps.filter(s => s.status === 'failed').length;
  const running = steps.filter(s => s.status === 'running').length;
  const pct = Math.round((completed / steps.length) * 100);

  return (
    <div style={{ padding: '7px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}`, backgroundColor: FINCEPT.DARK_BG }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
        <div style={{ display: 'flex', gap: '12px' }}>
          {[
            { label: 'DONE', val: completed, color: FINCEPT.GREEN },
            { label: 'FAIL', val: failed, color: FINCEPT.RED },
            { label: 'RUN', val: running, color: FINCEPT.ORANGE },
            { label: 'WAIT', val: steps.length - completed - failed - running, color: FINCEPT.GRAY },
          ].map(item => (
            <span key={item.label} style={{ fontSize: '8px', color: item.color, fontWeight: 700 }}>
              {item.val} {item.label}
            </span>
          ))}
        </div>
        <span style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700 }}>{pct}%</span>
      </div>
      <div style={{ height: '3px', backgroundColor: FINCEPT.BORDER, borderRadius: '2px', overflow: 'hidden' }}>
        <div style={{
          height: '100%', width: `${pct}%`,
          backgroundColor: failed > 0 ? FINCEPT.RED : running > 0 ? FINCEPT.ORANGE : FINCEPT.GREEN,
          transition: 'width 0.4s ease, background-color 0.3s',
          borderRadius: '2px',
        }} />
      </div>
    </div>
  );
};
