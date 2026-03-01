/**
 * PlannerView - Interactive execution plan builder, visualizer and results viewer
 * Supports: step editing (name/type/config), add/remove/reorder steps, plan history,
 * resizable results panel, markdown rendering, copy/export.
 */

import React, { useState, useCallback, useRef, useEffect } from 'react';
import {
  ListTree, Target, Play, Loader2,
  Copy, Check, Trash2, RotateCcw, Download,
  Search, PanelRightClose, PanelRightOpen, Maximize2, Minimize2,
  Plus, GripVertical, Clock,
} from 'lucide-react';
import { FINCEPT } from '../types';
import type { AgentConfigState } from '../hooks/useAgentConfig';
import type { ExecutionPlan, PlanStep } from '@/services/agentService';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';
import {
  PlanHistoryEntry, PlanTemplate,
  extractContextContent, deepClonePlan, makeNewStep,
  PLAN_TEMPLATES,
  StepCard, PlanProgressBar,
} from './PlannerViewHelpers';

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
