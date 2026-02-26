/**
 * AgentsView - Agent list + config editor + query execution panel
 */

import React from 'react';
import {
  Bot, RefreshCw, Plus, FileJson, Sliders, Edit3,
  CheckCircle, AlertCircle, BookOpen, Shield, Save,
  Database, Layers, Activity, Minimize2, Anchor, ClipboardCheck,
  Brain, Target, Loader2, Play, ToggleLeft, ToggleRight, Route,
} from 'lucide-react';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';
import RenaissanceTechnologiesPanel from '../RenaissanceTechnologiesPanel';
import { FINCEPT, OUTPUT_MODELS, extractAgentResponseText } from '../types';
import type { AgentConfigState } from '../hooks/useAgentConfig';

interface AgentsViewProps {
  state: AgentConfigState;
}

export const AgentsView: React.FC<AgentsViewProps> = ({ state }) => {
  const {
    discoveredAgents, selectedAgent, setSelectedAgent,
    agentsByCategory, selectedCategory, setSelectedCategory,
    loading, executing,
    editMode, setEditMode, editedConfig, setEditedConfig,
    configuredLLMs,
    testQuery, setTestQuery, testResult,
    selectedOutputModel, setSelectedOutputModel,
    showJsonEditor, setShowJsonEditor, jsonText, setJsonText,
    useAutoRouting, setUseAutoRouting, lastRoutingResult,
    selectedTools,
    loadDiscoveredAgents, loadCustomAgentsFromDb, updateEditedConfigFromAgent,
    addToTeam, runAgent,
    toggleToolSelection,
  } = state;

  const handleRefreshAgents = async () => {
    await loadDiscoveredAgents();
    await loadCustomAgentsFromDb();
  };

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* LEFT SIDEBAR - Agent List */}
      <div style={{
        width: '280px', minWidth: '280px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex', flexDirection: 'column', flexShrink: 0,
      }}>
        <div style={{
          padding: '12px', backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              AGENTS ({discoveredAgents.length})
            </span>
            <button
              onClick={handleRefreshAgents}
              disabled={loading}
              style={{ padding: '4px', backgroundColor: 'transparent', border: 'none', cursor: loading ? 'not-allowed' : 'pointer', opacity: loading ? 0.5 : 1 }}
            >
              <RefreshCw size={12} className={loading ? 'animate-spin' : ''} style={{ color: FINCEPT.ORANGE }} />
            </button>
          </div>
          <select
            value={selectedCategory}
            onChange={e => setSelectedCategory(e.target.value)}
            style={{
              width: '100%', padding: '6px 8px',
              backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', cursor: 'pointer',
            }}
          >
            <option value="all">All Categories ({discoveredAgents.length})</option>
            {Object.entries(agentsByCategory)
              .filter(([k]) => k !== 'all')
              .map(([cat, agents]: [string, any]) => (
                <option key={cat} value={cat}>{cat.toUpperCase()} ({agents.length})</option>
              ))}
          </select>
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '4px' }}>
          {loading ? (
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', padding: '32px 16px' }}>
              <Loader2 size={20} className="animate-spin" style={{ color: FINCEPT.ORANGE, marginBottom: '12px' }} />
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Loading agents...</span>
            </div>
          ) : discoveredAgents.length === 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', padding: '32px 16px', textAlign: 'center' }}>
              <Bot size={32} style={{ color: FINCEPT.MUTED, marginBottom: '12px', opacity: 0.5 }} />
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '8px' }}>No agents discovered</span>
              <button
                onClick={loadDiscoveredAgents}
                style={{ padding: '6px 12px', backgroundColor: FINCEPT.ORANGE, color: FINCEPT.WHITE, border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px' }}
              >
                <RefreshCw size={10} /> RETRY
              </button>
            </div>
          ) : (
            (agentsByCategory[selectedCategory] || []).map((agent: any) => (
              <div
                key={agent.id}
                onClick={() => { setSelectedAgent(agent); updateEditedConfigFromAgent(agent); }}
                style={{
                  padding: '10px 12px', marginBottom: '2px',
                  backgroundColor: selectedAgent?.id === agent.id ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: selectedAgent?.id === agent.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  cursor: 'pointer', transition: 'all 0.2s',
                }}
                onMouseEnter={e => { if (selectedAgent?.id !== agent.id) e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
                onMouseLeave={e => { if (selectedAgent?.id !== agent.id) e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
                  <span style={{ fontSize: '11px', fontWeight: 600, color: selectedAgent?.id === agent.id ? FINCEPT.ORANGE : FINCEPT.WHITE, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                    {agent.name}
                  </span>
                  <span style={{ fontSize: '8px', padding: '2px 4px', backgroundColor: `${FINCEPT.CYAN}20`, color: FINCEPT.CYAN, borderRadius: '2px' }}>
                    v{agent.version}
                  </span>
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {agent.category}
                </div>
              </div>
            ))
          )}
        </div>

        <div style={{
          padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.BORDER}`,
          display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px',
        }}>
          <div style={{ padding: '8px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', textAlign: 'center' }}>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.CYAN }}>{discoveredAgents.length}</div>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>TOTAL</div>
          </div>
          <div style={{ padding: '8px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px', textAlign: 'center' }}>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.GREEN }}>{state.teamMembers.length}</div>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>IN TEAM</div>
          </div>
        </div>
      </div>

      {/* CENTER - Agent Detail + Config */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
        {selectedAgent ? (
          <>
            <div style={{
              padding: '12px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            }}>
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                  <span style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>{selectedAgent.name}</span>
                  {selectedAgent.capabilities?.slice(0, 3).map((cap: string) => (
                    <span key={cap} style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.BLUE}20`, color: FINCEPT.BLUE, fontSize: '8px', borderRadius: '2px' }}>
                      {cap}
                    </span>
                  ))}
                </div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {selectedAgent.description}
                </div>
              </div>
              <div style={{ display: 'flex', gap: '8px', flexShrink: 0 }}>
                <button
                  onClick={() => addToTeam(selectedAgent)}
                  style={{ padding: '6px 10px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.WHITE, fontSize: '9px', fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', borderRadius: '2px' }}
                >
                  <Plus size={10} /> ADD TO TEAM
                </button>
                <button
                  onClick={() => setShowJsonEditor(!showJsonEditor)}
                  style={{ padding: '6px 10px', backgroundColor: showJsonEditor ? `${FINCEPT.PURPLE}20` : 'transparent', border: `1px solid ${showJsonEditor ? FINCEPT.PURPLE : FINCEPT.BORDER}`, color: showJsonEditor ? FINCEPT.PURPLE : FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', borderRadius: '2px' }}
                >
                  <FileJson size={10} /> JSON
                </button>
              </div>
            </div>

            <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
              {selectedAgent.id === 'renaissance_technologies' ? (
                <RenaissanceTechnologiesPanel />
              ) : (
                <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
                  {showJsonEditor ? (
                    <textarea
                      value={jsonText}
                      onChange={e => setJsonText(e.target.value)}
                      style={{ width: '100%', height: '100%', padding: '12px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '11px', fontFamily: '"IBM Plex Mono", monospace', resize: 'none' }}
                    />
                  ) : (
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                      <AgentConfigEditor state={state} />

                      <div style={{
                        padding: '12px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                      }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                          <Route size={16} style={{ color: FINCEPT.PURPLE }} />
                          <div>
                            <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE }}>SuperAgent Auto-Routing</div>
                            <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Automatically route queries to the best agent</div>
                          </div>
                        </div>
                        <button onClick={() => setUseAutoRouting(!useAutoRouting)} style={{ background: 'none', border: 'none', cursor: 'pointer', color: useAutoRouting ? FINCEPT.GREEN : FINCEPT.GRAY }}>
                          {useAutoRouting ? <ToggleRight size={24} /> : <ToggleLeft size={24} />}
                        </button>
                      </div>

                      {lastRoutingResult && (
                        <div style={{ padding: '12px', backgroundColor: `${FINCEPT.PURPLE}10`, border: `1px solid ${FINCEPT.PURPLE}40`, borderRadius: '2px' }}>
                          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.PURPLE, marginBottom: '6px' }}>LAST ROUTING RESULT</div>
                          <div style={{ fontSize: '11px', color: FINCEPT.WHITE }}>
                            <span style={{ color: FINCEPT.GRAY }}>Intent:</span>{' '}
                            <span style={{ color: FINCEPT.ORANGE }}>{lastRoutingResult.intent}</span>
                            {' • '}
                            <span style={{ color: FINCEPT.GRAY }}>Agent:</span>{' '}
                            <span style={{ color: FINCEPT.ORANGE }}>{lastRoutingResult.agent_id}</span>
                            {' • '}
                            <span style={{ color: FINCEPT.GRAY }}>Confidence:</span>{' '}
                            <span style={{ color: FINCEPT.GREEN }}>{(lastRoutingResult.confidence * 100).toFixed(0)}%</span>
                          </div>
                        </div>
                      )}
                    </div>
                  )}
                </div>
              )}
            </div>
          </>
        ) : (
          <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: FINCEPT.PANEL_BG }}>
            <div style={{ textAlign: 'center', padding: '32px' }}>
              <Bot size={48} style={{ color: FINCEPT.MUTED, marginBottom: '16px', opacity: 0.5 }} />
              <div style={{ fontSize: '12px', color: FINCEPT.GRAY }}>Select an agent to view details</div>
            </div>
          </div>
        )}
      </div>

      {/* RIGHT PANEL - Query & Results */}
      <div style={{
        width: '300px', minWidth: '300px',
        backgroundColor: FINCEPT.PANEL_BG, borderLeft: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex', flexDirection: 'column', flexShrink: 0,
      }}>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>QUERY EXECUTION</span>
        </div>

        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>OUTPUT MODEL</label>
            <select
              value={selectedOutputModel}
              onChange={e => setSelectedOutputModel(e.target.value)}
              style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px' }}
            >
              <option value="">None (free text)</option>
              {OUTPUT_MODELS.map(m => <option key={m.id} value={m.id}>{m.name}</option>)}
            </select>
          </div>

          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>QUERY</label>
            <textarea
              value={testQuery}
              onChange={e => setTestQuery(e.target.value)}
              placeholder="Enter your query..."
              rows={4}
              style={{ width: '100%', padding: '10px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '11px', fontFamily: '"IBM Plex Mono", monospace', resize: 'none' }}
            />
          </div>

          <button
            onClick={runAgent}
            disabled={executing}
            style={{ width: '100%', padding: '10px', backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontSize: '10px', fontWeight: 700, cursor: loading ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px', opacity: executing ? 0.7 : 1 }}
          >
            {executing ? <Loader2 size={12} className="animate-spin" /> : <Play size={12} />}
            {useAutoRouting ? 'RUN WITH AUTO-ROUTING' : 'RUN AGENT'}
          </button>
        </div>

        <div style={{ flex: 1, borderTop: `1px solid ${FINCEPT.BORDER}`, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
          <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>RESULT</span>
            {testResult && <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.GREEN}20`, color: FINCEPT.GREEN, fontSize: '8px', borderRadius: '2px' }}>SUCCESS</span>}
          </div>
          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            {testResult ? (
              <MarkdownRenderer
                content={extractAgentResponseText(
                  typeof testResult === 'string'
                    ? testResult
                    : typeof testResult.response === 'string'
                      ? testResult.response
                      : JSON.stringify(testResult, null, 2)
                )}
              />
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'center' }}>
                <Target size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <span>Run a query to see results</span>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

// ─── Agent Config Editor ──────────────────────────────────────────────────────

const AgentConfigEditor: React.FC<{ state: AgentConfigState }> = ({ state }) => {
  const { editMode, setEditMode, editedConfig, setEditedConfig, configuredLLMs, selectedTools, toggleToolSelection } = state;

  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px' }}>
      <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Sliders size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>AGENT CONFIGURATION</span>
        </div>
        <button
          onClick={() => setEditMode(!editMode)}
          style={{ padding: '4px 10px', backgroundColor: editMode ? `${FINCEPT.GREEN}20` : 'transparent', border: `1px solid ${editMode ? FINCEPT.GREEN : FINCEPT.BORDER}`, color: editMode ? FINCEPT.GREEN : FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', borderRadius: '2px' }}
        >
          <Edit3 size={10} />
          {editMode ? 'EDITING' : 'EDIT'}
        </button>
      </div>

      <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {/* LLM Selection */}
        {configuredLLMs.length > 0 ? (
          <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.GREEN}40`, borderRadius: '2px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '10px' }}>
              <CheckCircle size={10} style={{ color: FINCEPT.GREEN }} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GREEN }}>SELECT LLM PROVIDER</span>
            </div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
              {configuredLLMs.map((llm: any, idx: number) => {
                const hasKey = !!llm.api_key && llm.api_key.length > 0;
                const isSelected = editedConfig.provider === llm.provider && editedConfig.model_id === llm.model;
                return (
                  <button key={idx}
                    onClick={() => editMode && setEditedConfig(prev => ({ ...prev, provider: llm.provider, model_id: llm.model }))}
                    style={{ padding: '6px 10px', backgroundColor: isSelected ? FINCEPT.ORANGE : FINCEPT.BORDER, color: isSelected ? FINCEPT.DARK_BG : FINCEPT.WHITE, border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 600, cursor: editMode ? 'pointer' : 'default', opacity: hasKey ? 1 : 0.5, display: 'flex', alignItems: 'center', gap: '6px' }}
                  >
                    {llm.is_active && <span style={{ color: isSelected ? FINCEPT.DARK_BG : FINCEPT.GREEN }}>●</span>}
                    <span>{llm.provider.toUpperCase()}</span>
                    <span style={{ color: isSelected ? FINCEPT.DARK_BG : FINCEPT.GRAY }}>/ {llm.model}</span>
                    {!hasKey && <AlertCircle size={10} style={{ color: FINCEPT.RED }} />}
                  </button>
                );
              })}
            </div>
            {configuredLLMs.some((l: any) => !l.api_key || l.api_key.length === 0) && (
              <div style={{ fontSize: '9px', marginTop: '8px', color: FINCEPT.RED }}>⚠ Some providers missing API keys - configure in Settings</div>
            )}
          </div>
        ) : (
          <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.RED}40`, borderRadius: '2px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px' }}>
              <AlertCircle size={10} style={{ color: FINCEPT.RED }} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.RED }}>NO LLM CONFIGURED</span>
            </div>
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Go to Settings → LLM Configuration to add API keys.</span>
          </div>
        )}

        {/* Parameters Grid */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>PROVIDER</label>
            <div style={{ padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', color: FINCEPT.CYAN }}>{editedConfig.provider || 'None'}</div>
          </div>
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>MODEL</label>
            <div style={{ padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', color: FINCEPT.CYAN }}>{editedConfig.model_id || 'None'}</div>
          </div>
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>
              TEMPERATURE: <span style={{ color: FINCEPT.YELLOW }}>{editedConfig.temperature.toFixed(2)}</span>
            </label>
            <input type="range" min="0" max="2" step="0.1" value={editedConfig.temperature}
              onChange={e => setEditedConfig(prev => ({ ...prev, temperature: parseFloat(e.target.value) }))}
              disabled={!editMode}
              style={{ width: '100%', accentColor: FINCEPT.ORANGE, opacity: editMode ? 1 : 0.7 }}
            />
          </div>
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>MAX TOKENS</label>
            <input type="text" inputMode="numeric" value={editedConfig.max_tokens}
              onChange={e => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setEditedConfig(prev => ({ ...prev, max_tokens: v ? parseInt(v) : 4096 })); }}
              disabled={!editMode}
              style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.YELLOW, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', opacity: editMode ? 1 : 0.7 }}
            />
          </div>
        </div>

        {/* Feature Toggles */}
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
          {([
            { key: 'enable_memory' as const, label: 'MEMORY', icon: Database, color: FINCEPT.ORANGE },
            { key: 'enable_reasoning' as const, label: 'REASONING', icon: Brain, color: '#a855f7' },
            { key: 'enable_knowledge' as const, label: 'KNOWLEDGE', icon: BookOpen, color: '#3b82f6' },
            { key: 'enable_guardrails' as const, label: 'GUARDRAILS', icon: Shield, color: '#ef4444' },
            { key: 'enable_agentic_memory' as const, label: 'AGENT MEMORY', icon: Layers, color: '#f59e0b' },
            { key: 'enable_storage' as const, label: 'STORAGE', icon: Save, color: '#10b981' },
            { key: 'enable_tracing' as const, label: 'TRACING', icon: Activity, color: '#6366f1' },
            { key: 'enable_compression' as const, label: 'COMPRESS', icon: Minimize2, color: '#8b5cf6' },
            { key: 'enable_hooks' as const, label: 'HOOKS', icon: Anchor, color: '#ec4899' },
            { key: 'enable_evaluation' as const, label: 'EVAL', icon: ClipboardCheck, color: '#14b8a6' },
          ] as const).map(({ key, label, icon: Icon, color }) => (
            <button key={key}
              onClick={() => editMode && setEditedConfig(prev => ({ ...prev, [key]: !prev[key] }))}
              style={{ padding: '6px 10px', backgroundColor: editedConfig[key] ? `${color}20` : FINCEPT.DARK_BG, border: `1px solid ${editedConfig[key] ? color : FINCEPT.BORDER}`, color: editedConfig[key] ? color : FINCEPT.GRAY, fontSize: '8px', fontWeight: 700, cursor: editMode ? 'pointer' : 'default', display: 'flex', alignItems: 'center', gap: '4px', borderRadius: '2px', letterSpacing: '0.3px' }}
            >
              <Icon size={11} />{label}
            </button>
          ))}
        </div>

        {/* Expandable Config Panels (only in edit mode) */}
        {editMode && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            {editedConfig.enable_knowledge && (
              <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, border: '1px solid #3b82f640', borderRadius: '2px' }}>
                <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: '#3b82f6', marginBottom: '8px', letterSpacing: '0.5px' }}>
                  <BookOpen size={10} style={{ marginRight: '4px', verticalAlign: 'middle' }} />KNOWLEDGE BASE CONFIG
                </label>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                  <div>
                    <label style={{ display: 'block', fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TYPE</label>
                    <select value={editedConfig.knowledge_type} onChange={e => setEditedConfig(prev => ({ ...prev, knowledge_type: e.target.value as any }))}
                      style={{ width: '100%', padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }}>
                      <option value="url">URL</option><option value="pdf">PDF</option><option value="text">Text</option><option value="combined">Combined</option>
                    </select>
                  </div>
                  <div>
                    <label style={{ display: 'block', fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>VECTOR DB</label>
                    <select value={editedConfig.knowledge_vector_db} onChange={e => setEditedConfig(prev => ({ ...prev, knowledge_vector_db: e.target.value }))}
                      style={{ width: '100%', padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }}>
                      <option value="lancedb">LanceDB</option><option value="pgvector">PgVector</option><option value="qdrant">Qdrant</option>
                    </select>
                  </div>
                </div>
                <div style={{ marginTop: '8px' }}>
                  <label style={{ display: 'block', fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>URLS / PATHS (one per line)</label>
                  <textarea value={editedConfig.knowledge_urls} onChange={e => setEditedConfig(prev => ({ ...prev, knowledge_urls: e.target.value }))} rows={3}
                    style={{ width: '100%', padding: '8px', backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace', resize: 'none' }} />
                </div>
              </div>
            )}

            {editedConfig.enable_guardrails && (
              <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, border: '1px solid #ef444440', borderRadius: '2px' }}>
                <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: '#ef4444', marginBottom: '8px', letterSpacing: '0.5px' }}>
                  <Shield size={10} style={{ marginRight: '4px', verticalAlign: 'middle' }} />GUARDRAILS CONFIG
                </label>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                  {([
                    { key: 'guardrails_pii' as const, label: 'PII Detection', desc: 'Detect and redact personal information' },
                    { key: 'guardrails_injection' as const, label: 'Injection Check', desc: 'Block prompt injection attempts' },
                    { key: 'guardrails_compliance' as const, label: 'Financial Compliance', desc: 'Enforce financial regulation rules' },
                  ] as const).map(({ key, label, desc }) => (
                    <div key={key} onClick={() => setEditedConfig(prev => ({ ...prev, [key]: !prev[key] }))}
                      style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '6px 8px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer' }}>
                      <div>
                        <div style={{ fontSize: '9px', fontWeight: 700, color: editedConfig[key] ? '#ef4444' : FINCEPT.GRAY }}>{label}</div>
                        <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>{desc}</div>
                      </div>
                      <div style={{ width: '28px', height: '14px', borderRadius: '7px', backgroundColor: editedConfig[key] ? '#ef4444' : FINCEPT.BORDER, position: 'relative', transition: 'background 0.2s' }}>
                        <div style={{ width: '10px', height: '10px', borderRadius: '50%', backgroundColor: FINCEPT.WHITE, position: 'absolute', top: '2px', left: editedConfig[key] ? '16px' : '2px', transition: 'left 0.2s' }} />
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {editedConfig.enable_storage && (
              <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, border: '1px solid #10b98140', borderRadius: '2px' }}>
                <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: '#10b981', marginBottom: '8px', letterSpacing: '0.5px' }}>
                  <Save size={10} style={{ marginRight: '4px', verticalAlign: 'middle' }} />STORAGE CONFIG
                </label>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                  <div>
                    <label style={{ display: 'block', fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TYPE</label>
                    <select value={editedConfig.storage_type} onChange={e => setEditedConfig(prev => ({ ...prev, storage_type: e.target.value as any }))}
                      style={{ width: '100%', padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }}>
                      <option value="sqlite">SQLite</option><option value="postgres">PostgreSQL</option>
                    </select>
                  </div>
                  <div>
                    <label style={{ display: 'block', fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TABLE NAME</label>
                    <input value={editedConfig.storage_table} onChange={e => setEditedConfig(prev => ({ ...prev, storage_table: e.target.value }))} placeholder="agent_sessions"
                      style={{ width: '100%', padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }} />
                  </div>
                </div>
                <div style={{ marginTop: '8px' }}>
                  <label style={{ display: 'block', fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>DB PATH</label>
                  <input value={editedConfig.storage_db_path} onChange={e => setEditedConfig(prev => ({ ...prev, storage_db_path: e.target.value }))} placeholder="tmp/agent_storage.db"
                    style={{ width: '100%', padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }} />
                </div>
              </div>
            )}

            {editedConfig.enable_memory && (
              <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.ORANGE}40`, borderRadius: '2px' }}>
                <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px', letterSpacing: '0.5px' }}>
                  <Database size={10} style={{ marginRight: '4px', verticalAlign: 'middle' }} />MEMORY CONFIG
                </label>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                  <div>
                    <label style={{ display: 'block', fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>DB PATH</label>
                    <input value={editedConfig.memory_db_path} onChange={e => setEditedConfig(prev => ({ ...prev, memory_db_path: e.target.value }))} placeholder="tmp/agent_memory.db"
                      style={{ width: '100%', padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }} />
                  </div>
                  <div>
                    <label style={{ display: 'block', fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TABLE</label>
                    <input value={editedConfig.memory_table} onChange={e => setEditedConfig(prev => ({ ...prev, memory_table: e.target.value }))} placeholder="agent_memory"
                      style={{ width: '100%', padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }} />
                  </div>
                </div>
              </div>
            )}

            {editedConfig.enable_agentic_memory && (
              <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, border: '1px solid #f59e0b40', borderRadius: '2px' }}>
                <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: '#f59e0b', marginBottom: '8px', letterSpacing: '0.5px' }}>
                  <Layers size={10} style={{ marginRight: '4px', verticalAlign: 'middle' }} />AGENTIC MEMORY CONFIG
                </label>
                <div>
                  <label style={{ display: 'block', fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>USER ID</label>
                  <input value={editedConfig.agentic_memory_user_id} onChange={e => setEditedConfig(prev => ({ ...prev, agentic_memory_user_id: e.target.value }))} placeholder="user-001"
                    style={{ width: '100%', padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }} />
                </div>
              </div>
            )}

            <div>
              <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>INSTRUCTIONS</label>
              <textarea value={editedConfig.instructions} onChange={e => setEditedConfig(prev => ({ ...prev, instructions: e.target.value }))} rows={3} placeholder="Enter agent instructions..."
                style={{ width: '100%', padding: '10px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', resize: 'none' }} />
            </div>
          </div>
        )}

        {/* Selected Tools */}
        {editedConfig.tools.length > 0 && (
          <div>
            <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px' }}>TOOLS ({editedConfig.tools.length})</label>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
              {editedConfig.tools.map(tool => (
                <span key={tool} onClick={() => editMode && toggleToolSelection(tool)}
                  style={{ padding: '4px 8px', backgroundColor: `${FINCEPT.ORANGE}20`, color: FINCEPT.ORANGE, fontSize: '9px', borderRadius: '2px', cursor: editMode ? 'pointer' : 'default' }}>
                  {tool} {editMode && '×'}
                </span>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
