/**
 * PlannerView - Interactive execution plan builder, visualizer and results viewer
 * Supports: step editing (name/type/config), add/remove/reorder steps, plan history,
 * resizable results panel, markdown rendering, copy/export.
 */

import React, { useState, useCallback, useRef, useEffect } from 'react';
import {
  ListTree, Target, Play, Loader2, ChevronDown, ChevronRight,
  Copy, Check, Trash2, RotateCcw, Download, Clock, AlertCircle,
  CheckCircle, XCircle, BarChart2, TrendingUp, Shield, Layers,
  Search, PanelRightClose, PanelRightOpen, Maximize2, Minimize2,
  FileText, Plus, ArrowUp, ArrowDown, Edit2, Save, X, Settings,
  GripVertical,
} from 'lucide-react';
import { FINCEPT } from '../types';
import type { AgentConfigState } from '../hooks/useAgentConfig';
import type { ExecutionPlan, PlanStep } from '@/services/agentService';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';

// ─── Types ────────────────────────────────────────────────────────────────────

interface PlanHistoryEntry {
  id: string;
  symbol: string;
  planType: string;
  timestamp: Date;
  plan: ExecutionPlan;
  label: string;
}

type PlanTemplate = {
  id: string;
  name: string;
  desc: string;
  icon: React.ReactNode;
  defaultSymbol: string;
  color: string;
};

// Editable config row: {key, value} pairs that map to step.config
interface ConfigRow {
  key: string;
  value: string;
}

// ─── Content Extraction ───────────────────────────────────────────────────────

function extractContextContent(value: unknown): string {
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

function deepClonePlan(plan: ExecutionPlan): ExecutionPlan {
  return JSON.parse(JSON.stringify(plan));
}

function makeNewStep(index: number): PlanStep {
  return {
    id: `custom_step_${Date.now()}_${index}`,
    name: `Custom Step ${index + 1}`,
    step_type: 'analysis',
    config: {},
    dependencies: [],
    status: 'pending',
  };
}

function configToRows(config: Record<string, any>): ConfigRow[] {
  return Object.entries(config).map(([key, value]) => ({
    key,
    value: typeof value === 'string' ? value : JSON.stringify(value),
  }));
}

function rowsToConfig(rows: ConfigRow[]): Record<string, any> {
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

const PLAN_TEMPLATES: PlanTemplate[] = [
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

const STEP_TYPES = [
  'fetch', 'analysis', 'research', 'synthesis',
  'technical', 'fundamental', 'news', 'sentiment',
  'risk', 'portfolio', 'report', 'custom',
];

const STATUS_COLOR: Record<string, string> = {
  completed: FINCEPT.GREEN,
  failed: FINCEPT.RED,
  running: FINCEPT.ORANGE,
  pending: FINCEPT.GRAY,
  skipped: FINCEPT.MUTED,
};

const STATUS_ICON: Record<string, React.ReactNode> = {
  completed: <CheckCircle size={12} />,
  failed: <XCircle size={12} />,
  running: <Loader2 size={12} className="animate-spin" />,
  pending: <Clock size={12} />,
  skipped: <AlertCircle size={12} />,
};

// ─── Inline field style helpers ───────────────────────────────────────────────

const inputStyle = (focused?: boolean): React.CSSProperties => ({
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

const StepEditor: React.FC<StepEditorProps> = ({ step, onSave, onCancel }) => {
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

interface StepCardProps {
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

const StepCard: React.FC<StepCardProps> = ({
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
                <Trash2 size={10} />
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

const PlanProgressBar: React.FC<{ steps: PlanStep[] }> = ({ steps }) => {
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

// ─── Main Component ───────────────────────────────────────────────────────────

interface PlannerViewProps {
  state: AgentConfigState;
}

export const PlannerView: React.FC<PlannerViewProps> = ({ state }) => {
  const {
    workflowSymbol, setWorkflowSymbol,
    currentPlan, planExecuting, executing,
    planGeneratedBy,
    createPlan, executePlan,
  } = state;

  // ─── Left sidebar UI state ──────────────────────────────────────────
  const [selectedTemplate, setSelectedTemplate] = useState<string>('stock_analysis');
  const [useCustomQuery, setUseCustomQuery] = useState(false);
  const [customQuery, setCustomQuery] = useState('');
  const [showHistory, setShowHistory] = useState(false);
  const [historySearch, setHistorySearch] = useState('');
  const [planHistory, setPlanHistory] = useState<PlanHistoryEntry[]>([]);
  const [activeHistoryId, setActiveHistoryId] = useState<string | null>(null);

  // ─── Center panel UI state ──────────────────────────────────────────
  const [expandedSteps, setExpandedSteps] = useState<Set<string>>(new Set());
  const [editingStepId, setEditingStepId] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState('');

  // ─── LOCAL editable plan (deep clone of currentPlan, fully mutable) ─
  const [editablePlan, setEditablePlan] = useState<ExecutionPlan | null>(null);

  // ─── Results panel state ────────────────────────────────────────────
  const [copiedResult, setCopiedResult] = useState(false);
  const [activeResultStep, setActiveResultStep] = useState<string | null>(null);
  const [resultsCollapsed, setResultsCollapsed] = useState(false);
  const [resultsWidth, setResultsWidth] = useState(420);
  const [isDraggingResults, setIsDraggingResults] = useState(false);

  const dragStartX = useRef(0);
  const dragStartWidth = useRef(0);
  const resultStepRefs = useRef<Record<string, HTMLDivElement | null>>({});

  // ─── Sync editablePlan from currentPlan ─────────────────────────────
  // When backend returns a new/updated plan, deep-clone it into local state.
  // We only reset editablePlan when the plan ID changes (new plan created),
  // so mid-execution updates merge step statuses without blowing away edits.
  useEffect(() => {
    if (!currentPlan) { setEditablePlan(null); return; }

    setEditablePlan(prev => {
      if (!prev || prev.id !== currentPlan.id) {
        // Brand new plan — full reset
        setEditingStepId(null);
        setExpandedSteps(new Set());
        setActiveResultStep(null);
        return deepClonePlan(currentPlan);
      }
      // Same plan — merge status/result/error from backend without losing local config edits
      return {
        ...prev,
        status: currentPlan.status,
        is_complete: currentPlan.is_complete,
        has_failed: currentPlan.has_failed,
        context: currentPlan.context,
        steps: prev.steps.map(localStep => {
          const serverStep = currentPlan.steps.find(s => s.id === localStep.id);
          if (!serverStep) return localStep;
          return {
            ...localStep,               // keep local name/type/config edits
            status: serverStep.status,  // take live status from server
            result: serverStep.result,
            error: serverStep.error,
          };
        }),
      };
    });
  }, [currentPlan]);

  // ─── Auto-save completed plan to history ────────────────────────────
  useEffect(() => {
    if (!currentPlan || (!currentPlan.is_complete && !currentPlan.has_failed)) return;
    setPlanHistory(prev => {
      if (prev.find(h => h.id === currentPlan.id)) return prev;
      const entry: PlanHistoryEntry = {
        id: currentPlan.id,
        symbol: workflowSymbol,
        planType: selectedTemplate,
        timestamp: new Date(),
        plan: deepClonePlan(currentPlan),
        label: `${workflowSymbol} · ${PLAN_TEMPLATES.find(t => t.id === selectedTemplate)?.name ?? selectedTemplate}`,
      };
      return [entry, ...prev.slice(0, 19)];
    });
  }, [currentPlan?.is_complete, currentPlan?.has_failed]);

  // ─── Auto-expand running step ────────────────────────────────────────
  useEffect(() => {
    if (!editablePlan) return;
    const running = editablePlan.steps.find(s => s.status === 'running');
    if (running) setExpandedSteps(prev => new Set([...prev, running.id]));
  }, [editablePlan]);

  // ─── Derived display plan (history view OR live editable) ────────────
  const displayPlan = activeHistoryId
    ? planHistory.find(h => h.id === activeHistoryId)?.plan ?? editablePlan
    : editablePlan;

  const isHistoryView = !!activeHistoryId;
  // Steps can be edited only when looking at the live editable plan and it hasn't started
  const canEditSteps = !isHistoryView && !!editablePlan && !editablePlan.is_complete && !planExecuting;

  // ─── Resize handler ──────────────────────────────────────────────────
  const onResizeMouseDown = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    dragStartX.current = e.clientX;
    dragStartWidth.current = resultsWidth;
    setIsDraggingResults(true);
    const onMouseMove = (ev: MouseEvent) => {
      const delta = dragStartX.current - ev.clientX;
      setResultsWidth(Math.max(280, Math.min(700, dragStartWidth.current + delta)));
    };
    const onMouseUp = () => {
      setIsDraggingResults(false);
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
    };
    window.addEventListener('mousemove', onMouseMove);
    window.addEventListener('mouseup', onMouseUp);
  }, [resultsWidth]);

  // ─── Step mutation helpers ───────────────────────────────────────────
  const updateStep = useCallback((stepId: string, updated: PlanStep) => {
    setEditablePlan(prev => {
      if (!prev) return prev;
      return { ...prev, steps: prev.steps.map(s => s.id === stepId ? updated : s) };
    });
    setEditingStepId(null);
  }, []);

  const deleteStep = useCallback((stepId: string) => {
    setEditablePlan(prev => {
      if (!prev) return prev;
      return { ...prev, steps: prev.steps.filter(s => s.id !== stepId) };
    });
    setEditingStepId(null);
  }, []);

  const moveStep = useCallback((stepId: string, direction: 'up' | 'down') => {
    setEditablePlan(prev => {
      if (!prev) return prev;
      const steps = [...prev.steps];
      const idx = steps.findIndex(s => s.id === stepId);
      if (idx < 0) return prev;
      const targetIdx = direction === 'up' ? idx - 1 : idx + 1;
      if (targetIdx < 0 || targetIdx >= steps.length) return prev;
      [steps[idx], steps[targetIdx]] = [steps[targetIdx], steps[idx]];
      return { ...prev, steps };
    });
  }, []);

  const addStep = useCallback((afterIndex: number) => {
    setEditablePlan(prev => {
      if (!prev) return prev;
      const newStep = makeNewStep(afterIndex + 1);
      const steps = [...prev.steps];
      steps.splice(afterIndex + 1, 0, newStep);
      return { ...prev, steps };
    });
    // Auto-expand and open editor for the new step
    setTimeout(() => {
      setEditablePlan(prev => {
        if (!prev) return prev;
        const newStep = prev.steps[afterIndex + 1];
        if (newStep) {
          setExpandedSteps(s => new Set([...s, newStep.id]));
          setEditingStepId(newStep.id);
        }
        return prev;
      });
    }, 50);
  }, []);

  // ─── Plan action handlers ────────────────────────────────────────────
  const handleCreatePlan = useCallback(() => {
    setActiveHistoryId(null);
    setEditingStepId(null);
    setExpandedSteps(new Set());
    setActiveResultStep(null);
    // Build query: custom text takes priority, otherwise derive from template
    let query: string | undefined;
    if (useCustomQuery && customQuery.trim()) {
      query = customQuery.trim();
    } else {
      const templateQueryMap: Record<string, string> = {
        stock_analysis: 'Perform a comprehensive stock analysis including price data, fundamentals, news sentiment, technical analysis, and investment recommendation',
        portfolio_rebalance: 'Analyze and rebalance the portfolio: fetch current positions, evaluate risk metrics, generate rebalancing recommendations, and calculate required trades',
        risk_audit: 'Conduct a risk audit: assess volatility, drawdown, correlation, concentration risk, and provide risk-adjusted recommendations',
        sector_rotation: 'Analyze sector rotation opportunities: evaluate sector performance, macroeconomic trends, relative strength, and recommend sector allocation adjustments',
      };
      query = templateQueryMap[selectedTemplate];
    }
    createPlan(query);
  }, [createPlan, useCustomQuery, customQuery, selectedTemplate]);

  const handleExecutePlan = useCallback(() => {
    if (!editablePlan) return;
    setActiveHistoryId(null);
    setEditingStepId(null);
    executePlan(editablePlan);
  }, [executePlan, editablePlan]);

  const handleTemplateSelect = (template: PlanTemplate) => {
    setSelectedTemplate(template.id);
    if (template.defaultSymbol !== 'PORTFOLIO') setWorkflowSymbol(template.defaultSymbol);
  };

  // ─── Results helpers ─────────────────────────────────────────────────
  const jumpToResult = (stepKey: string) => {
    setActiveResultStep(stepKey);
    setResultsCollapsed(false);
    setTimeout(() => {
      resultStepRefs.current[stepKey]?.scrollIntoView({ behavior: 'smooth', block: 'start' });
    }, 80);
  };

  const handleCopyResults = useCallback(() => {
    if (!displayPlan?.context) return;
    const text = Object.entries(displayPlan.context)
      .map(([key, val]) => `## ${key.replace(/_/g, ' ').toUpperCase()}\n\n${extractContextContent(val)}`)
      .join('\n\n---\n\n');
    navigator.clipboard.writeText(text).then(() => {
      setCopiedResult(true);
      setTimeout(() => setCopiedResult(false), 2000);
    });
  }, [displayPlan]);

  const handleExportMarkdown = useCallback(() => {
    if (!displayPlan) return;
    const lines: string[] = [
      `# ${displayPlan.name}`,
      `> ${displayPlan.description}`,
      '',
      `**Status:** ${displayPlan.status} | **Steps:** ${displayPlan.steps.length}`,
      '',
      '---',
      '',
    ];
    Object.entries(displayPlan.context || {}).forEach(([key, val]) => {
      lines.push(`## ${key.replace(/_/g, ' ')}`);
      lines.push('');
      lines.push(extractContextContent(val));
      lines.push('');
      lines.push('---');
      lines.push('');
    });
    const blob = new Blob([lines.join('\n')], { type: 'text/markdown' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${workflowSymbol}-plan-${Date.now()}.md`;
    a.click();
    URL.revokeObjectURL(url);
  }, [displayPlan, workflowSymbol]);

  // ─── Filtered steps ──────────────────────────────────────────────────
  const filteredSteps = (displayPlan?.steps ?? []).filter(s =>
    !searchQuery ||
    s.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    s.step_type.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const filteredHistory = planHistory.filter(h =>
    !historySearch || h.label.toLowerCase().includes(historySearch.toLowerCase())
  );

  const contextEntries = Object.entries(displayPlan?.context ?? {});
  const currentTemplate = PLAN_TEMPLATES.find(t => t.id === selectedTemplate);

  // ─── Render ──────────────────────────────────────────────────────────
  return (
    <div style={{
      display: 'flex', height: '100%', overflow: 'hidden',
      fontFamily: '"IBM Plex Mono", monospace',
      userSelect: isDraggingResults ? 'none' : 'auto',
    }}>

      {/* ══ LEFT SIDEBAR ════════════════════════════════════════════════ */}
      <div style={{
        width: '280px', minWidth: '280px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex', flexDirection: 'column', flexShrink: 0, overflow: 'hidden',
      }}>

        {/* Header */}
        <div style={{ padding: '10px 12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <ListTree size={13} style={{ color: FINCEPT.ORANGE }} />
            <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>EXECUTION PLANNER</span>
          </div>
          <button
            onClick={() => setShowHistory(v => !v)}
            title="Plan history"
            style={{ background: 'none', border: 'none', cursor: 'pointer', color: showHistory ? FINCEPT.ORANGE : FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '3px', padding: '2px' }}
          >
            <Clock size={13} />
            {planHistory.length > 0 && (
              <span style={{ fontSize: '8px', color: FINCEPT.ORANGE, fontWeight: 700 }}>{planHistory.length}</span>
            )}
          </button>
        </div>

        {/* ── History mode ── */}
        {showHistory ? (
          <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
            <div style={{ padding: '8px 12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <div style={{ position: 'relative' }}>
                <Search size={10} style={{ position: 'absolute', left: '8px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.MUTED }} />
                <input
                  value={historySearch}
                  onChange={e => setHistorySearch(e.target.value)}
                  placeholder="Search history…"
                  style={{ width: '100%', padding: '6px 8px 6px 24px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', boxSizing: 'border-box', outline: 'none' }}
                />
              </div>
            </div>
            <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
              {filteredHistory.length === 0 ? (
                <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.MUTED, fontSize: '9px' }}>No plan history yet</div>
              ) : filteredHistory.map(entry => (
                <button
                  key={entry.id}
                  onClick={() => { setActiveHistoryId(entry.id); setShowHistory(false); setExpandedSteps(new Set()); setActiveResultStep(null); }}
                  style={{
                    width: '100%', padding: '9px 10px', marginBottom: '4px',
                    backgroundColor: activeHistoryId === entry.id ? `${FINCEPT.ORANGE}15` : FINCEPT.DARK_BG,
                    border: `1px solid ${activeHistoryId === entry.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                    borderRadius: '2px', cursor: 'pointer', textAlign: 'left',
                  }}
                >
                  <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '3px', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                    {entry.label}
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ fontSize: '8px', color: entry.plan.is_complete ? FINCEPT.GREEN : entry.plan.has_failed ? FINCEPT.RED : FINCEPT.GRAY }}>
                      {entry.plan.is_complete ? '✓ Complete' : entry.plan.has_failed ? '✗ Failed' : entry.plan.status}
                    </span>
                    <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
                      {entry.timestamp.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
                    </span>
                  </div>
                </button>
              ))}
            </div>
            {planHistory.length > 0 && (
              <div style={{ padding: '8px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
                <button
                  onClick={() => { setPlanHistory([]); setShowHistory(false); setActiveHistoryId(null); }}
                  style={{ width: '100%', padding: '6px', background: 'none', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', fontSize: '8px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}
                >
                  <Trash2 size={9} /> CLEAR HISTORY
                </button>
              </div>
            )}
          </div>
        ) : (
          /* ── Normal sidebar mode ── */
          <div style={{ flex: 1, overflow: 'auto', display: 'flex', flexDirection: 'column' }}>

            {/* Plan type grid */}
            <div style={{ padding: '10px 12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>PLAN TYPE</div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '5px' }}>
                {PLAN_TEMPLATES.map(template => (
                  <button
                    key={template.id}
                    onClick={() => handleTemplateSelect(template)}
                    style={{
                      padding: '8px 6px',
                      backgroundColor: selectedTemplate === template.id ? `${template.color}15` : FINCEPT.DARK_BG,
                      border: `1px solid ${selectedTemplate === template.id ? template.color : FINCEPT.BORDER}`,
                      borderRadius: '2px', cursor: 'pointer', textAlign: 'left', transition: 'all 0.15s',
                    }}
                    onMouseEnter={e => { if (selectedTemplate !== template.id) e.currentTarget.style.borderColor = template.color + '60'; }}
                    onMouseLeave={e => { if (selectedTemplate !== template.id) e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                  >
                    <div style={{ color: template.color, marginBottom: '3px', display: 'flex', alignItems: 'center', gap: '4px' }}>
                      {template.icon}
                      <span style={{ fontSize: '9px', fontWeight: 700 }}>{template.name.split(' ')[0]}</span>
                    </div>
                    <div style={{ fontSize: '8px', color: FINCEPT.MUTED, lineHeight: 1.3 }}>{template.desc}</div>
                  </button>
                ))}
              </div>
            </div>

            {/* Target input */}
            <div style={{ padding: '10px 12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>TARGET</div>
              <input
                type="text"
                value={workflowSymbol}
                onChange={e => setWorkflowSymbol(e.target.value.toUpperCase())}
                placeholder="Symbol (e.g., AAPL)"
                style={{ width: '100%', padding: '7px 10px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', boxSizing: 'border-box', fontFamily: '"IBM Plex Mono", monospace', outline: 'none' }}
                onFocus={e => { e.currentTarget.style.borderColor = currentTemplate?.color ?? FINCEPT.ORANGE; }}
                onBlur={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
              />
              <div style={{ marginTop: '8px' }}>
                <label style={{ display: 'flex', alignItems: 'center', gap: '6px', cursor: 'pointer' }}>
                  <input type="checkbox" checked={useCustomQuery} onChange={e => setUseCustomQuery(e.target.checked)} style={{ accentColor: FINCEPT.ORANGE, cursor: 'pointer' }} />
                  <span style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 600 }}>CUSTOM QUERY</span>
                </label>
                {useCustomQuery && (
                  <textarea
                    value={customQuery}
                    onChange={e => setCustomQuery(e.target.value)}
                    placeholder="Describe your analysis request…"
                    rows={3}
                    style={{ marginTop: '6px', width: '100%', padding: '7px 10px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', resize: 'vertical', boxSizing: 'border-box', fontFamily: '"IBM Plex Mono", monospace', lineHeight: 1.5, outline: 'none' }}
                    onFocus={e => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
                    onBlur={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                  />
                )}
              </div>
            </div>

            {/* Action buttons */}
            <div style={{ padding: '10px 12px', borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column', gap: '6px' }}>
              <button
                onClick={handleCreatePlan}
                disabled={executing || !workflowSymbol.trim()}
                style={{ width: '100%', padding: '9px', backgroundColor: executing ? `${FINCEPT.ORANGE}50` : currentTemplate?.color ?? FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: executing || !workflowSymbol.trim() ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px', opacity: !workflowSymbol.trim() ? 0.5 : 1 }}
              >
                {executing ? <Loader2 size={11} className="animate-spin" /> : <Target size={11} />}
                {executing ? 'AI GENERATING…' : 'CREATE PLAN'}
              </button>
              {!executing && (
                <div style={{ fontSize: '8px', color: FINCEPT.MUTED, textAlign: 'center', letterSpacing: '0.3px' }}>
                  LLM generates a custom plan for your query
                </div>
              )}

              {editablePlan && !editablePlan.is_complete && !isHistoryView && (
                <button
                  onClick={handleExecutePlan}
                  disabled={planExecuting}
                  style={{ width: '100%', padding: '9px', backgroundColor: planExecuting ? `${FINCEPT.GREEN}40` : FINCEPT.GREEN, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: planExecuting ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px' }}
                >
                  {planExecuting ? <Loader2 size={11} className="animate-spin" /> : <Play size={11} />}
                  {planExecuting ? 'EXECUTING…' : 'EXECUTE PLAN'}
                </button>
              )}

              {displayPlan && (
                <div style={{ display: 'flex', gap: '5px' }}>
                  {displayPlan.is_complete && (
                    <>
                      <button
                        onClick={handleCopyResults}
                        style={{ flex: 1, padding: '7px', background: 'none', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', fontSize: '8px', color: copiedResult ? FINCEPT.GREEN : FINCEPT.GRAY, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', transition: 'color 0.2s' }}
                      >
                        {copiedResult ? <Check size={10} /> : <Copy size={10} />}
                        {copiedResult ? 'COPIED' : 'COPY'}
                      </button>
                      <button
                        onClick={handleExportMarkdown}
                        style={{ flex: 1, padding: '7px', background: 'none', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', fontSize: '8px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}
                      >
                        <Download size={10} /> EXPORT
                      </button>
                    </>
                  )}
                  {isHistoryView && (
                    <button
                      onClick={() => { setActiveHistoryId(null); setExpandedSteps(new Set()); }}
                      style={{ flex: 1, padding: '7px', background: 'none', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', fontSize: '8px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}
                    >
                      <RotateCcw size={10} /> BACK TO LIVE
                    </button>
                  )}
                </div>
              )}
            </div>

            {/* Plan info */}
            {displayPlan && (
              <div style={{ padding: '10px 12px', flex: 1, overflow: 'auto' }}>
                <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>PLAN INFO</div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '5px' }}>
                  {[
                    { label: 'NAME', value: displayPlan.name },
                    { label: 'STATUS', value: displayPlan.status.toUpperCase(), color: displayPlan.is_complete ? FINCEPT.GREEN : displayPlan.has_failed ? FINCEPT.RED : FINCEPT.ORANGE },
                    { label: 'STEPS', value: `${displayPlan.steps.length} total` },
                    { label: 'DONE', value: `${displayPlan.steps.filter(s => s.status === 'completed').length} / ${displayPlan.steps.length}` },
                  ].map(row => (
                    <div key={row.label} style={{ display: 'flex', justifyContent: 'space-between', gap: '8px' }}>
                      <span style={{ fontSize: '8px', color: FINCEPT.MUTED, flexShrink: 0 }}>{row.label}</span>
                      <span style={{ fontSize: '8px', color: (row as any).color ?? FINCEPT.WHITE, textAlign: 'right', wordBreak: 'break-word' }}>{row.value}</span>
                    </div>
                  ))}
                  {displayPlan.description && (
                    <div style={{ marginTop: '4px', padding: '6px 8px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}` }}>
                      <span style={{ fontSize: '8px', color: FINCEPT.GRAY, lineHeight: 1.4 }}>{displayPlan.description}</span>
                    </div>
                  )}
                  {canEditSteps && (
                    <div style={{ marginTop: '4px', padding: '5px 8px', backgroundColor: `${FINCEPT.ORANGE}08`, borderRadius: '2px', border: `1px dashed ${FINCEPT.ORANGE}40` }}>
                      <span style={{ fontSize: '8px', color: FINCEPT.ORANGE }}>✎ Plan is editable — use step controls to modify before executing.</span>
                    </div>
                  )}
                </div>
              </div>
            )}
          </div>
        )}
      </div>

      {/* ══ CENTER — Steps ══════════════════════════════════════════════ */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
        {displayPlan ? (
          <>
            {/* Toolbar */}
            <div style={{ padding: '8px 12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', gap: '8px' }}>
              <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, flex: 1, display: 'flex', alignItems: 'center', gap: '6px', minWidth: 0 }}>
                <span style={{ whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                  {isHistoryView ? '📂 ' : ''}{displayPlan.name}
                </span>
                {!isHistoryView && planGeneratedBy === 'llm' && (
                  <span style={{ flexShrink: 0, fontSize: '8px', fontWeight: 700, padding: '2px 5px', backgroundColor: '#6366f120', color: '#818cf8', border: '1px solid #6366f140', borderRadius: '2px', letterSpacing: '0.4px' }}>
                    AI
                  </span>
                )}
                {!isHistoryView && planGeneratedBy === 'fallback' && (
                  <span style={{ flexShrink: 0, fontSize: '8px', fontWeight: 700, padding: '2px 5px', backgroundColor: `${FINCEPT.ORANGE}20`, color: FINCEPT.ORANGE, border: `1px solid ${FINCEPT.ORANGE}40`, borderRadius: '2px', letterSpacing: '0.4px' }}>
                    TPL
                  </span>
                )}
              </div>

              {/* Step search */}
              <div style={{ position: 'relative', flexShrink: 0 }}>
                <Search size={9} style={{ position: 'absolute', left: '7px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.MUTED }} />
                <input
                  value={searchQuery}
                  onChange={e => setSearchQuery(e.target.value)}
                  placeholder="Filter steps…"
                  style={{ width: '120px', padding: '5px 8px 5px 22px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none' }}
                />
              </div>

              {/* Expand/Collapse all */}
              <button
                onClick={() => setExpandedSteps(prev => prev.size > 0 ? new Set() : new Set(displayPlan.steps.map(s => s.id)))}
                style={{ background: 'none', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', color: FINCEPT.GRAY, padding: '4px 6px', display: 'flex', alignItems: 'center', gap: '3px', fontSize: '8px', flexShrink: 0 }}
              >
                {expandedSteps.size > 0 ? <Minimize2 size={10} /> : <Maximize2 size={10} />}
                {expandedSteps.size > 0 ? 'COLLAPSE' : 'EXPAND'}
              </button>

              {/* Add step at end (only when editable) */}
              {canEditSteps && (
                <button
                  onClick={() => addStep(displayPlan.steps.length - 1)}
                  title="Add new step at end"
                  style={{ background: 'none', border: `1px solid ${FINCEPT.ORANGE}60`, borderRadius: '2px', cursor: 'pointer', color: FINCEPT.ORANGE, padding: '4px 7px', display: 'flex', alignItems: 'center', gap: '3px', fontSize: '8px', flexShrink: 0 }}
                >
                  <Plus size={10} /> ADD STEP
                </button>
              )}
            </div>

            {/* Progress bar */}
            <PlanProgressBar steps={displayPlan.steps} />

            {/* Steps list */}
            <div style={{ flex: 1, overflow: 'auto', padding: '10px 12px', display: 'flex', flexDirection: 'column', gap: '5px' }}>
              {filteredSteps.length === 0 ? (
                <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.MUTED, fontSize: '10px' }}>
                  {searchQuery ? 'No steps match your filter' : 'No steps in this plan'}
                </div>
              ) : filteredSteps.map((step, idx) => (
                <React.Fragment key={step.id}>
                  <StepCard
                    step={step}
                    index={displayPlan.steps.indexOf(step)}
                    total={displayPlan.steps.length}
                    isExpanded={expandedSteps.has(step.id)}
                    isEditing={editingStepId === step.id}
                    canEdit={canEditSteps && step.status === 'pending'}
                    onToggle={() => setExpandedSteps(prev => {
                      const next = new Set(prev);
                      if (next.has(step.id)) { next.delete(step.id); } else { next.add(step.id); }
                      return next;
                    })}
                    onJumpToResult={() => {
                      const ctxKey =
                        Object.keys(displayPlan.context ?? {}).find(k =>
                          k === step.id ||
                          k === step.name ||
                          k.toLowerCase().includes(step.name.toLowerCase().replace(/\s+/g, '_')) ||
                          k.toLowerCase().replace(/_/g, ' ').includes(step.name.toLowerCase())
                        ) ?? Object.keys(displayPlan.context ?? {})[idx] ?? null;
                      if (ctxKey) jumpToResult(ctxKey);
                    }}
                    onEdit={() => {
                      setExpandedSteps(prev => new Set([...prev, step.id]));
                      setEditingStepId(prev => prev === step.id ? null : step.id);
                    }}
                    onSave={updated => updateStep(step.id, updated)}
                    onCancelEdit={() => setEditingStepId(null)}
                    onDelete={() => deleteStep(step.id)}
                    onMoveUp={() => moveStep(step.id, 'up')}
                    onMoveDown={() => moveStep(step.id, 'down')}
                  />

                  {/* Insert-step-here button between steps */}
                  {canEditSteps && idx < filteredSteps.length - 1 && (
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', padding: '0 4px' }}>
                      <div style={{ flex: 1, height: '1px', backgroundColor: FINCEPT.BORDER }} />
                      <button
                        onClick={() => addStep(displayPlan.steps.indexOf(step))}
                        title="Insert step here"
                        style={{ background: 'none', border: `1px dashed ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', color: FINCEPT.MUTED, padding: '1px 8px', fontSize: '8px', display: 'flex', alignItems: 'center', gap: '3px', flexShrink: 0, transition: 'color 0.15s, border-color 0.15s' }}
                        onMouseEnter={e => { e.currentTarget.style.color = FINCEPT.ORANGE; e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
                        onMouseLeave={e => { e.currentTarget.style.color = FINCEPT.MUTED; e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                      >
                        <Plus size={8} /> INSERT
                      </button>
                      <div style={{ flex: 1, height: '1px', backgroundColor: FINCEPT.BORDER }} />
                    </div>
                  )}
                </React.Fragment>
              ))}
            </div>
          </>
        ) : (
          <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: FINCEPT.PANEL_BG }}>
            <div style={{ textAlign: 'center', color: FINCEPT.MUTED, maxWidth: '260px' }}>
              <ListTree size={36} style={{ marginBottom: '12px', opacity: 0.3, color: FINCEPT.ORANGE }} />
              <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '6px' }}>NO ACTIVE PLAN</div>
              <div style={{ fontSize: '9px', lineHeight: 1.6 }}>
                Select a plan type and target symbol on the left, then click{' '}
                <strong style={{ color: FINCEPT.ORANGE }}>CREATE PLAN</strong> to generate an execution roadmap.
              </div>
            </div>
          </div>
        )}
      </div>

      {/* ══ RESULTS PANEL ═══════════════════════════════════════════════ */}
      {contextEntries.length > 0 && (
        <>
          {!resultsCollapsed && (
            <div
              onMouseDown={onResizeMouseDown}
              style={{ width: '4px', cursor: 'col-resize', backgroundColor: isDraggingResults ? FINCEPT.ORANGE : 'transparent', transition: 'background-color 0.2s', flexShrink: 0 }}
              onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${FINCEPT.ORANGE}60`; }}
              onMouseLeave={e => { if (!isDraggingResults) e.currentTarget.style.backgroundColor = 'transparent'; }}
            />
          )}

          <div style={{
            width: resultsCollapsed ? '36px' : `${resultsWidth}px`,
            minWidth: resultsCollapsed ? '36px' : '280px',
            backgroundColor: FINCEPT.PANEL_BG,
            borderLeft: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex', flexDirection: 'column', flexShrink: 0,
            overflow: 'hidden', transition: resultsCollapsed ? 'width 0.2s' : 'none',
          }}>
            {/* Header */}
            <div style={{ padding: '8px 10px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', gap: '6px', flexShrink: 0 }}>
              <button
                onClick={() => setResultsCollapsed(v => !v)}
                style={{ background: 'none', border: 'none', cursor: 'pointer', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', padding: '2px', flexShrink: 0 }}
              >
                {resultsCollapsed ? <PanelRightOpen size={13} /> : <PanelRightClose size={13} />}
              </button>
              {!resultsCollapsed && (
                <>
                  <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', flex: 1 }}>PLAN RESULTS</span>
                  <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>{contextEntries.length} step{contextEntries.length !== 1 ? 's' : ''}</span>
                </>
              )}
            </div>

            {/* Scroll area */}
            {!resultsCollapsed && (
              <div style={{ flex: 1, overflow: 'auto', padding: '12px', display: 'flex', flexDirection: 'column', gap: '14px' }}>
                {contextEntries.map(([stepKey, stepValue]) => {
                  const content = extractContextContent(stepValue);
                  const isActive = activeResultStep === stepKey;

                  return (
                    <div
                      key={stepKey}
                      ref={el => { resultStepRefs.current[stepKey] = el; }}
                      style={{ borderLeft: `2px solid ${isActive ? FINCEPT.CYAN : FINCEPT.ORANGE}`, paddingLeft: '10px', transition: 'border-color 0.3s', scrollMarginTop: '8px' }}
                    >
                      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
                        <div style={{ fontSize: '8px', fontWeight: 700, color: isActive ? FINCEPT.CYAN : FINCEPT.ORANGE, letterSpacing: '0.5px', textTransform: 'uppercase' }}>
                          {stepKey.replace(/_/g, ' ')}
                        </div>
                        <button
                          onClick={() => navigator.clipboard.writeText(content)}
                          title="Copy section"
                          style={{ background: 'none', border: 'none', cursor: 'pointer', color: FINCEPT.MUTED, padding: '2px', display: 'flex', alignItems: 'center' }}
                        >
                          <Copy size={9} />
                        </button>
                      </div>
                      <MarkdownRenderer content={content} style={{ fontSize: '10px', lineHeight: '1.5' }} />
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        </>
      )}
    </div>
  );
};
