/**
 * AgentConfigTab - Professional Agent Studio Interface (v2.1)
 *
 * This file is the thin shell: nav bar, layout, loading screen, error/success notifications.
 * All state and logic is in hooks/useAgentConfig.ts
 * All view panels are in views/*.tsx
 */

import React from 'react';
import {
  Bot, RefreshCw, Route, AlertCircle, CheckCircle,
  Users, Wrench, MessageSquare, Cpu, ListTree,
  GitBranch, Loader2, Plus,
} from 'lucide-react';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { cacheService } from '@/services/cache/cacheService';
import { useAgentConfig } from './hooks/useAgentConfig';
import { FINCEPT, TERMINAL_STYLES, AGENT_CACHE_KEYS } from './types';
import { AgentsView } from './views/AgentsView';
import { TeamsView } from './views/TeamsView';
import { WorkflowsView } from './views/WorkflowsView';
import { PlannerView } from './views/PlannerView';
import { ToolsView } from './views/ToolsView';
import { ChatView } from './views/ChatView';
import { SystemView } from './views/SystemView';
import { CreateAgentView } from './views/CreateAgentView';

// ─── View Mode tab definitions (with icons) ──────────────────────────────────

const VIEW_MODE_TABS = [
  { id: 'agents', name: 'AGENTS', icon: Bot },
  { id: 'create', name: 'CREATE', icon: Plus },
  { id: 'teams', name: 'TEAMS', icon: Users },
  { id: 'workflows', name: 'WORKFLOWS', icon: GitBranch },
  { id: 'planner', name: 'PLANNER', icon: ListTree },
  { id: 'tools', name: 'TOOLS', icon: Wrench },
  { id: 'chat', name: 'CHAT', icon: MessageSquare },
  { id: 'system', name: 'SYSTEM', icon: Cpu },
] as const;

// ─── Main Component ───────────────────────────────────────────────────────────

const AgentConfigTab: React.FC = () => {
  const state = useAgentConfig();
  const {
    viewMode, setViewMode,
    loading, error, setError, success,
    discoveredAgents, configuredLLMs,
    useAutoRouting, setUseAutoRouting,
    selectedAgent, editedConfig,
    loadDiscoveredAgents,
  } = state;

  // Initial loading screen
  if (loading && discoveredAgents.length === 0 && !state.systemInfo && !state.toolsInfo) {
    return (
      <div
        className="agent-terminal flex flex-col h-full items-center justify-center"
        style={{ backgroundColor: FINCEPT.DARK_BG, fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}
      >
        <style>{TERMINAL_STYLES}</style>
        <div style={{ padding: '48px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', textAlign: 'center' }}>
          <Bot size={48} className="mx-auto mb-4" style={{ color: FINCEPT.ORANGE }} />
          <Loader2 size={32} className="animate-spin mx-auto mb-4" style={{ color: FINCEPT.ORANGE }} />
          <h2 style={{ fontSize: '18px', fontWeight: 700, marginBottom: '8px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            INITIALIZING AGENT STUDIO
          </h2>
          <p style={{ fontSize: '11px', marginBottom: '16px', color: FINCEPT.GRAY }}>
            Loading agents and system capabilities...
          </p>
          <button
            onClick={() => setError('Agent loading was skipped. Some features may not be available.')}
            style={{ padding: '8px 16px', backgroundColor: FINCEPT.BORDER, color: FINCEPT.WHITE, border: 'none', borderRadius: '2px', fontSize: '10px', fontWeight: 700, cursor: 'pointer', letterSpacing: '0.5px' }}
          >
            SKIP LOADING
          </button>
        </div>
      </div>
    );
  }

  return (
    <div
      className="agent-terminal flex flex-col h-full overflow-hidden"
      style={{ backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}
    >
      <style>{TERMINAL_STYLES}</style>

      {/* ── Top Navigation Bar ──────────────────────────────────────────────── */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG, borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px', display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        flexShrink: 0, boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
      }}>
        {/* Left: Logo + Status */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Bot size={20} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>AGENT STUDIO</span>
          <span style={{ padding: '2px 8px', backgroundColor: `${FINCEPT.PURPLE}20`, color: FINCEPT.PURPLE, fontSize: '9px', fontWeight: 700, borderRadius: '2px' }}>v2.1</span>
          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER, margin: '0 4px' }} />
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: discoveredAgents.length > 0 ? FINCEPT.GREEN : FINCEPT.RED }} />
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{discoveredAgents.length} AGENTS</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: configuredLLMs.length > 0 ? FINCEPT.GREEN : FINCEPT.YELLOW, animation: configuredLLMs.length === 0 ? 'pulse 2s infinite' : 'none' }} />
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{configuredLLMs.length} LLMs</span>
            </div>
          </div>
        </div>

        {/* Center: View Mode Tabs */}
        <div style={{ display: 'flex', gap: '2px' }}>
          {VIEW_MODE_TABS.map(mode => (
            <button
              key={mode.id}
              onClick={() => setViewMode(mode.id as any)}
              style={{
                padding: '6px 12px',
                backgroundColor: viewMode === mode.id ? FINCEPT.ORANGE : 'transparent',
                color: viewMode === mode.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: 'none', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
                display: 'flex', alignItems: 'center', gap: '4px',
                letterSpacing: '0.5px', transition: 'all 0.2s',
              }}
              onMouseEnter={e => { if (viewMode !== mode.id) { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; e.currentTarget.style.color = FINCEPT.WHITE; } }}
              onMouseLeave={e => { if (viewMode !== mode.id) { e.currentTarget.style.backgroundColor = 'transparent'; e.currentTarget.style.color = FINCEPT.GRAY; } }}
            >
              <mode.icon size={12} />
              {mode.name}
            </button>
          ))}
        </div>

        {/* Right: Actions */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <button
            onClick={async () => {
              await cacheService.delete(AGENT_CACHE_KEYS.DISCOVERED_AGENTS).catch(() => {});
              loadDiscoveredAgents();
            }}
            disabled={loading}
            style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY, cursor: loading ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', gap: '4px', fontSize: '9px', borderRadius: '2px', opacity: loading ? 0.5 : 1 }}
          >
            <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
            REFRESH
          </button>
          <button
            onClick={() => setUseAutoRouting(!useAutoRouting)}
            style={{ padding: '4px 8px', backgroundColor: useAutoRouting ? `${FINCEPT.GREEN}20` : 'transparent', border: `1px solid ${useAutoRouting ? FINCEPT.GREEN : FINCEPT.BORDER}`, color: useAutoRouting ? FINCEPT.GREEN : FINCEPT.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', fontSize: '9px', borderRadius: '2px' }}
          >
            <Route size={12} />
            AUTO-ROUTE
          </button>
        </div>
      </div>

      {/* ── Notifications ──────────────────────────────────────────────────── */}
      {error && (
        <div style={{ margin: '8px 16px', padding: '12px', backgroundColor: `${FINCEPT.RED}15`, border: `1px solid ${FINCEPT.RED}40`, borderRadius: '2px' }}>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
            <AlertCircle size={14} style={{ color: FINCEPT.RED, marginTop: '1px', flexShrink: 0 }} />
            <div style={{ flex: 1 }}>
              <div style={{ fontSize: '10px', fontWeight: 700, marginBottom: '4px', color: FINCEPT.RED }}>ERROR</div>
              <div style={{ fontSize: '10px', color: FINCEPT.RED }}>{error}</div>
              {error.includes('timeout') && (
                <div style={{ fontSize: '9px', marginTop: '4px', color: FINCEPT.MUTED }}>Backend may be slow. You can continue with manual configuration.</div>
              )}
            </div>
            <button onClick={() => setError(null)} style={{ background: 'none', border: 'none', color: FINCEPT.RED, cursor: 'pointer', fontSize: '14px', lineHeight: 1 }}>×</button>
          </div>
        </div>
      )}
      {success && (
        <div style={{ margin: '8px 16px', padding: '12px', backgroundColor: `${FINCEPT.GREEN}15`, border: `1px solid ${FINCEPT.GREEN}40`, borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <CheckCircle size={14} style={{ color: FINCEPT.GREEN }} />
          <span style={{ fontSize: '10px', color: FINCEPT.GREEN }}>{success}</span>
        </div>
      )}

      {/* ── Main Content ───────────────────────────────────────────────────── */}
      <div style={{ flex: 1, overflow: 'hidden' }}>
        {viewMode === 'agents' && <AgentsView state={state} />}
        {viewMode === 'create' && <CreateAgentView />}
        {viewMode === 'teams' && <TeamsView state={state} />}
        {viewMode === 'workflows' && <WorkflowsView state={state} />}
        {viewMode === 'planner' && <PlannerView state={state} />}
        {viewMode === 'tools' && <ToolsView state={state} />}
        {viewMode === 'chat' && <ChatView state={state} />}
        {viewMode === 'system' && <SystemView state={state} />}
      </div>

      {/* ── Status Bar ─────────────────────────────────────────────────────── */}
      <div style={{ backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.BORDER}`, padding: '4px 16px', display: 'flex', alignItems: 'center', justifyContent: 'space-between', fontSize: '9px', color: FINCEPT.GRAY, flexShrink: 0 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>MODE: <span style={{ color: useAutoRouting ? FINCEPT.GREEN : FINCEPT.ORANGE }}>{useAutoRouting ? 'AUTO-ROUTING' : 'MANUAL'}</span></span>
          <span>VIEW: <span style={{ color: FINCEPT.CYAN }}>{viewMode.toUpperCase()}</span></span>
          {selectedAgent && <span>AGENT: <span style={{ color: FINCEPT.ORANGE }}>{selectedAgent.name}</span></span>}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>LLM: <span style={{ color: FINCEPT.PURPLE }}>{editedConfig.provider}/{editedConfig.model_id}</span></span>
          <span style={{ color: FINCEPT.MUTED }}>AGENT STUDIO v2.1</span>
        </div>
      </div>
    </div>
  );
};

// ─── Exported Component - Wrapped with ErrorBoundary ─────────────────────────

const AgentConfigTabWithErrorBoundary: React.FC = () => (
  <ErrorBoundary
    name="Agent Studio"
    variant="default"
    onError={(error, errorInfo) => {
      console.error('[Agent Studio Error]', error.message, errorInfo.componentStack);
    }}
  >
    <AgentConfigTab />
  </ErrorBoundary>
);

export default AgentConfigTabWithErrorBoundary;
