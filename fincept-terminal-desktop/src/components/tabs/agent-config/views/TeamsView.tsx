/**
 * TeamsView - Multi-agent team builder and execution panel
 */

import React from 'react';
import { Users, Trash2, Plus, Loader2, Play, Eye } from 'lucide-react';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';
import { FINCEPT, extractAgentResponseText } from '../types';
import type { AgentConfigState } from '../hooks/useAgentConfig';

interface TeamsViewProps {
  state: AgentConfigState;
}

export const TeamsView: React.FC<TeamsViewProps> = ({ state }) => {
  const {
    discoveredAgents, teamMembers, teamMode, setTeamMode,
    showMembersResponses, setShowMembersResponses,
    teamLeaderIndex, setTeamLeaderIndex,
    testQuery, setTestQuery, testResult,
    executing, isStreaming, streamingContent,
    addToTeam, removeFromTeam, runTeam, cancelStream,
  } = state;

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* Left - Team Members */}
      <div style={{
        width: '280px', minWidth: '280px',
        backgroundColor: FINCEPT.PANEL_BG, borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex', flexDirection: 'column', flexShrink: 0,
      }}>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              TEAM ({teamMembers.length})
            </span>
            <select
              value={teamMode}
              onChange={e => setTeamMode(e.target.value as any)}
              style={{ padding: '4px 8px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px' }}
            >
              <option value="coordinate">DELEGATE</option>
              <option value="route">ROUTE</option>
              <option value="collaborate">COLLABORATE</option>
            </select>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <select
              value={teamLeaderIndex}
              onChange={e => setTeamLeaderIndex(parseInt(e.target.value))}
              style={{ flex: 1, padding: '4px 6px', backgroundColor: FINCEPT.DARK_BG, color: teamLeaderIndex >= 0 ? FINCEPT.ORANGE : FINCEPT.GRAY, border: `1px solid ${teamLeaderIndex >= 0 ? FINCEPT.ORANGE : FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '8px' }}
            >
              <option value={-1}>NO LEADER</option>
              {teamMembers.map((m, i) => <option key={i} value={i}>{m.name.toUpperCase()}</option>)}
            </select>
            <button
              onClick={() => setShowMembersResponses(prev => !prev)}
              title="Show individual member responses"
              style={{ padding: '4px 8px', backgroundColor: showMembersResponses ? `${FINCEPT.ORANGE}20` : FINCEPT.DARK_BG, border: `1px solid ${showMembersResponses ? FINCEPT.ORANGE : FINCEPT.BORDER}`, color: showMembersResponses ? FINCEPT.ORANGE : FINCEPT.GRAY, fontSize: '8px', fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', borderRadius: '2px' }}
            >
              <Eye size={10} />SHOW ALL
            </button>
          </div>
          <div style={{ marginTop: '6px', fontSize: '8px', color: FINCEPT.GRAY, fontStyle: 'italic' }}>
            {teamMode === 'coordinate' && 'Leader delegates tasks to chosen members'}
            {teamMode === 'route' && 'Routes query to the most appropriate member'}
            {teamMode === 'collaborate' && 'All members receive the task and discuss consensus'}
          </div>
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
          {teamMembers.length === 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', padding: '32px 16px', textAlign: 'center' }}>
              <Users size={32} style={{ color: FINCEPT.MUTED, marginBottom: '12px', opacity: 0.5 }} />
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>No agents in team</span>
              <span style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '4px' }}>Add from available list</span>
            </div>
          ) : (
            teamMembers.map((agent, idx) => (
              <div key={agent.id} style={{
                padding: '10px', marginBottom: '4px',
                backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{ width: '20px', height: '20px', display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG, fontSize: '10px', fontWeight: 700, borderRadius: '2px' }}>
                    {idx + 1}
                  </span>
                  <div>
                    <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE }}>{agent.name}</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{agent.category}</div>
                  </div>
                </div>
                <button
                  onClick={() => removeFromTeam(agent.id)}
                  style={{ background: 'none', border: 'none', cursor: 'pointer', color: FINCEPT.RED, padding: '4px' }}
                >
                  <Trash2 size={12} />
                </button>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Center - Available Agents */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            AVAILABLE AGENTS ({discoveredAgents.filter(a => !teamMembers.find(m => m.id === a.id)).length})
          </span>
        </div>
        <div style={{ flex: 1, overflow: 'auto', padding: '16px', display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '8px', alignContent: 'start' }}>
          {discoveredAgents
            .filter(a => !teamMembers.find(m => m.id === a.id))
            .map(agent => (
              <div
                key={agent.id}
                onClick={() => addToTeam(agent)}
                style={{ padding: '12px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.2s' }}
                onMouseEnter={e => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG; }}
              >
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
                  <span style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE }}>{agent.name}</span>
                  <Plus size={12} style={{ color: FINCEPT.ORANGE }} />
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{agent.category}</div>
              </div>
            ))}
        </div>
      </div>

      {/* Right - Query Panel */}
      <div style={{ width: '300px', minWidth: '300px', backgroundColor: FINCEPT.PANEL_BG, borderLeft: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column', flexShrink: 0 }}>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>TEAM QUERY</span>
        </div>
        <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <textarea
            value={testQuery}
            onChange={e => setTestQuery(e.target.value)}
            rows={6}
            placeholder="Enter your query for the team..."
            style={{ width: '100%', padding: '10px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '11px', fontFamily: '"IBM Plex Mono", monospace', resize: 'none' }}
          />
          <button
            onClick={runTeam}
            disabled={executing || teamMembers.length === 0}
            style={{ width: '100%', padding: '10px', backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontSize: '10px', fontWeight: 700, cursor: (executing || teamMembers.length === 0) ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px', opacity: (executing || teamMembers.length === 0) ? 0.7 : 1 }}
          >
            {executing ? <Loader2 size={12} className="animate-spin" /> : <Play size={12} />}
            RUN TEAM
          </button>
        </div>

        <div style={{ flex: 1, borderTop: `1px solid ${FINCEPT.BORDER}`, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
          <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>RESULT</span>
          </div>
          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            {isStreaming && streamingContent ? (
              <div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px', padding: '8px', backgroundColor: FINCEPT.ORANGE + '15', borderRadius: '4px', borderLeft: `3px solid ${FINCEPT.ORANGE}` }}>
                  <Loader2 size={14} className="animate-spin" style={{ color: FINCEPT.ORANGE }} />
                  <span style={{ fontSize: '10px', color: FINCEPT.ORANGE, fontWeight: 600 }}>Streaming response...</span>
                  <button
                    onClick={cancelStream}
                    style={{ marginLeft: 'auto', padding: '4px 8px', fontSize: '9px', backgroundColor: FINCEPT.RED + '20', color: FINCEPT.RED, border: `1px solid ${FINCEPT.RED}40`, borderRadius: '3px', cursor: 'pointer' }}
                  >
                    Cancel
                  </button>
                </div>
                <MarkdownRenderer content={streamingContent} />
              </div>
            ) : testResult ? (
              <MarkdownRenderer
                content={extractAgentResponseText(
                  typeof testResult === 'string' ? testResult : (testResult.response || JSON.stringify(testResult, null, 2))
                )}
              />
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'center' }}>
                <Users size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <span>Run team query to see results</span>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
