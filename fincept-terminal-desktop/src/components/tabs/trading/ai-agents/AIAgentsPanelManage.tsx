import React, { useState, useEffect } from 'react';
import {
  Bot, Play, Pause, Square, ChevronDown, ChevronUp
} from 'lucide-react';
import type { AgentInfo } from '../../../../services/trading/agnoTradingService';
import { FINCEPT } from './AIAgentsPanelQuickActions';

// ============================================================================
// AgentStatus interface
// ============================================================================

export interface AgentStatus {
  id: string;
  name: string;
  type: string;
  status: 'running' | 'paused' | 'stopped';
  autoTrading: boolean;
  metrics: {
    totalTrades: number;
    winningTrades: number;
    losingTrades: number;
    totalPnL: number;
    dailyPnL: number;
    currentDrawdown: number;
    confidence: number;
  };
  lastActivity?: string;
}

// ============================================================================
// MetricBox Component
// ============================================================================

export function MetricBox({ label, value, color }: { label: string; value: string; color: string }) {
  return (
    <div style={{
      background: FINCEPT.DARK_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      padding: '5px 6px'
    }}>
      <div style={{ color: FINCEPT.GRAY, fontSize: '7px', marginBottom: '2px', letterSpacing: '0.5px' }}>
        {label}
      </div>
      <div style={{ color, fontSize: '11px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
        {value}
      </div>
    </div>
  );
}

// ============================================================================
// AgentCard Component
// ============================================================================

export interface AgentCardProps {
  agent: AgentStatus;
  isExpanded: boolean;
  onToggleExpand: () => void;
  onStart: () => void;
  onPause: () => void;
  onStop: () => void;
}

export function AgentCard({ agent, isExpanded, onToggleExpand, onStart, onPause, onStop }: AgentCardProps) {
  const winRate = agent.metrics.totalTrades > 0
    ? (agent.metrics.winningTrades / agent.metrics.totalTrades) * 100
    : 0;

  const statusColor = {
    running: FINCEPT.GREEN,
    paused: FINCEPT.YELLOW,
    stopped: FINCEPT.GRAY
  }[agent.status];

  return (
    <div style={{
      background: FINCEPT.DARK_BG,
      border: `1px solid ${agent.status === 'running' ? FINCEPT.GREEN : FINCEPT.BORDER}`,
      borderRadius: '2px',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div
        onClick={onToggleExpand}
        style={{
          padding: '6px 8px',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          background: agent.status === 'running' ? `${FINCEPT.GREEN}08` : 'transparent'
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flex: 1 }}>
          <div style={{
            width: '6px',
            height: '6px',
            borderRadius: '50%',
            background: statusColor,
            boxShadow: agent.status === 'running' ? `0 0 6px ${statusColor}` : 'none'
          }} />

          <div style={{ flex: 1 }}>
            <div style={{
              color: FINCEPT.WHITE,
              fontSize: '10px',
              fontWeight: '600',
              marginBottom: '1px'
            }}>
              {agent.name}
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>
              {agent.type.replace(/_/g, ' ').toUpperCase()}
              {agent.autoTrading && <span style={{ color: FINCEPT.ORANGE, marginLeft: '6px' }}>● AUTO</span>}
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '7px', letterSpacing: '0.5px' }}>P&L</div>
              <div style={{
                color: agent.metrics.totalPnL >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                fontSize: '10px',
                fontWeight: '700',
                fontFamily: '"IBM Plex Mono", monospace'
              }}>
                ${agent.metrics.totalPnL.toFixed(0)}
              </div>
            </div>

            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '7px', letterSpacing: '0.5px' }}>WIN</div>
              <div style={{ color: FINCEPT.WHITE, fontSize: '10px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
                {winRate.toFixed(0)}%
              </div>
            </div>

            <div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '7px', letterSpacing: '0.5px' }}>TRD</div>
              <div style={{ color: FINCEPT.WHITE, fontSize: '10px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
                {agent.metrics.totalTrades}
              </div>
            </div>
          </div>
        </div>

        {isExpanded ? <ChevronUp size={12} color={FINCEPT.GRAY} /> : <ChevronDown size={12} color={FINCEPT.GRAY} />}
      </div>

      {/* Expanded Details */}
      {isExpanded && (
        <div style={{
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          padding: '8px',
          background: FINCEPT.PANEL_BG
        }}>
          {/* Metrics Grid */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(3, 1fr)',
            gap: '6px',
            marginBottom: '8px'
          }}>
            <MetricBox label="DAILY" value={`$${agent.metrics.dailyPnL.toFixed(0)}`}
              color={agent.metrics.dailyPnL >= 0 ? FINCEPT.GREEN : FINCEPT.RED} />
            <MetricBox label="DD" value={`${(agent.metrics.currentDrawdown * 100).toFixed(1)}%`}
              color={FINCEPT.ORANGE} />
            <MetricBox label="CONF" value={`${(agent.metrics.confidence * 100).toFixed(0)}%`}
              color={FINCEPT.CYAN} />
            <MetricBox label="WIN" value={agent.metrics.winningTrades.toString()}
              color={FINCEPT.GREEN} />
            <MetricBox label="LOSS" value={agent.metrics.losingTrades.toString()}
              color={FINCEPT.RED} />
            <MetricBox label="STATE" value={agent.status.toUpperCase()}
              color={statusColor} />
          </div>

          {/* Controls */}
          <div style={{ display: 'flex', gap: '4px' }}>
            {agent.status !== 'running' && (
              <button
                onClick={onStart}
                style={{
                  flex: 1,
                  background: FINCEPT.GREEN,
                  border: 'none',
                  color: FINCEPT.DARK_BG,
                  padding: '6px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '600',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                  letterSpacing: '0.3px'
                }}
              >
                <Play size={10} />
                START
              </button>
            )}

            {agent.status === 'running' && (
              <button
                onClick={onPause}
                style={{
                  flex: 1,
                  background: FINCEPT.YELLOW,
                  border: 'none',
                  color: FINCEPT.DARK_BG,
                  padding: '6px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '600',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                  letterSpacing: '0.3px'
                }}
              >
                <Pause size={10} />
                PAUSE
              </button>
            )}

            {agent.status !== 'stopped' && (
              <button
                onClick={onStop}
                style={{
                  flex: 1,
                  background: FINCEPT.RED,
                  border: 'none',
                  color: FINCEPT.WHITE,
                  padding: '6px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '600',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '4px',
                  letterSpacing: '0.3px'
                }}
              >
                <Square size={10} />
                STOP
              </button>
            )}
          </div>
        </div>
      )}
    </div>
  );
}

// ============================================================================
// ManageAgentsView Component
// ============================================================================

export function ManageAgentsView({ agents }: { agents: AgentInfo[] }) {
  const [agentStatuses, setAgentStatuses] = useState<AgentStatus[]>([]);
  const [expandedAgent, setExpandedAgent] = useState<string | null>(null);

  // Mock data for demonstration
  useEffect(() => {
    if (agents.length > 0) {
      const mockStatuses: AgentStatus[] = agents.map((agent, idx) => ({
        id: agent.agent_id,
        name: agent.config.name,
        type: agent.config.role,
        status: idx === 0 ? 'running' : 'stopped',
        autoTrading: idx === 0,
        metrics: {
          totalTrades: Math.floor(Math.random() * 50),
          winningTrades: Math.floor(Math.random() * 30),
          losingTrades: Math.floor(Math.random() * 20),
          totalPnL: (Math.random() - 0.5) * 5000,
          dailyPnL: (Math.random() - 0.5) * 500,
          currentDrawdown: Math.random() * 0.15,
          confidence: 0.6 + Math.random() * 0.3
        },
        lastActivity: new Date().toISOString()
      }));
      setAgentStatuses(mockStatuses);
    }
  }, [agents]);

  const handleStart = (agentId: string) => {
    setAgentStatuses(prev => prev.map(a =>
      a.id === agentId ? { ...a, status: 'running' as const } : a
    ));
  };

  const handlePause = (agentId: string) => {
    setAgentStatuses(prev => prev.map(a =>
      a.id === agentId ? { ...a, status: 'paused' as const } : a
    ));
  };

  const handleStop = (agentId: string) => {
    setAgentStatuses(prev => prev.map(a =>
      a.id === agentId ? { ...a, status: 'stopped' as const, autoTrading: false } : a
    ));
  };

  const handleEmergencyStop = () => {
    setAgentStatuses(prev => prev.map(a => ({ ...a, status: 'stopped' as const, autoTrading: false })));
  };

  if (agents.length === 0) {
    return (
      <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '30px 12px' }}>
        <Bot size={32} color={FINCEPT.GRAY} style={{ margin: '0 auto 10px' }} />
        <div style={{ fontSize: '11px', fontWeight: '600', marginBottom: '4px', letterSpacing: '0.5px' }}>
          NO AGENTS
        </div>
        <div style={{ fontSize: '9px', lineHeight: '1.4' }}>
          Create your first AI agent
        </div>
      </div>
    );
  }

  const runningAgents = agentStatuses.filter(a => a.status === 'running').length;
  const totalPnL = agentStatuses.reduce((sum, a) => sum + a.metrics.totalPnL, 0);
  const totalTrades = agentStatuses.reduce((sum, a) => sum + a.metrics.totalTrades, 0);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      {/* Overview Stats */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px' }}>
        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '6px 8px'
        }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '8px', marginBottom: '3px', letterSpacing: '0.5px' }}>
            ACTIVE
          </div>
          <div style={{ color: FINCEPT.WHITE, fontSize: '14px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
            {runningAgents}/{agents.length}
          </div>
        </div>

        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '6px 8px'
        }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '8px', marginBottom: '3px', letterSpacing: '0.5px' }}>
            P&L
          </div>
          <div style={{
            color: totalPnL >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: '14px',
            fontWeight: '700',
            fontFamily: '"IBM Plex Mono", monospace'
          }}>
            ${totalPnL.toFixed(0)}
          </div>
        </div>

        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '6px 8px'
        }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '8px', marginBottom: '3px', letterSpacing: '0.5px' }}>
            TRADES
          </div>
          <div style={{ color: FINCEPT.WHITE, fontSize: '14px', fontWeight: '700', fontFamily: '"IBM Plex Mono", monospace' }}>
            {totalTrades}
          </div>
        </div>

        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '6px 8px'
        }}>
          <button
            onClick={handleEmergencyStop}
            disabled={runningAgents === 0}
            style={{
              width: '100%',
              height: '100%',
              background: runningAgents > 0 ? FINCEPT.RED : 'transparent',
              border: `1px solid ${runningAgents > 0 ? FINCEPT.RED : FINCEPT.BORDER}`,
              color: runningAgents > 0 ? FINCEPT.WHITE : FINCEPT.GRAY,
              borderRadius: '2px',
              fontSize: '8px',
              fontWeight: '600',
              cursor: runningAgents > 0 ? 'pointer' : 'not-allowed',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '3px',
              letterSpacing: '0.3px'
            }}
          >
            <Square size={10} />
            STOP
          </button>
        </div>
      </div>

      {/* Agent List */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
        {agentStatuses.map(agent => (
          <AgentCard
            key={agent.id}
            agent={agent}
            isExpanded={expandedAgent === agent.id}
            onToggleExpand={() => setExpandedAgent(expandedAgent === agent.id ? null : agent.id)}
            onStart={() => handleStart(agent.id)}
            onPause={() => handlePause(agent.id)}
            onStop={() => handleStop(agent.id)}
          />
        ))}
      </div>
    </div>
  );
}
