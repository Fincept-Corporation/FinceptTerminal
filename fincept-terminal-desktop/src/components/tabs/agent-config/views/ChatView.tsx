/**
 * ChatView - Agent chat interface with streaming, auto-routing, portfolio context
 */

import React from 'react';
import { MessageSquare, Sparkles, Loader2, Zap, Bot, Database, ChevronDown, ToggleLeft, ToggleRight } from 'lucide-react';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';
import { FINCEPT } from '../types';
import type { AgentConfigState } from '../hooks/useAgentConfig';

interface ChatViewProps {
  state: AgentConfigState;
}

export const ChatView: React.FC<ChatViewProps> = ({ state }) => {
  const {
    chatMessages, chatInput, setChatInput, chatEndRef,
    selectedAgent, useAutoRouting, setUseAutoRouting,
    portfolios, selectedPortfolioCtx, setSelectedPortfolioCtx,
    showPortfolioDropdown, setShowPortfolioDropdown,
    executing,
    handleChatSubmit,
  } = state;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', backgroundColor: FINCEPT.PANEL_BG }}>
      {/* Header */}
      <div style={{ padding: '12px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <MessageSquare size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>AGENT CHAT</span>
          {selectedAgent && (
            <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.CYAN}20`, color: FINCEPT.CYAN, fontSize: '8px', borderRadius: '2px' }}>
              {selectedAgent.name}
            </span>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>AUTO-ROUTE</span>
          <button onClick={() => setUseAutoRouting(!useAutoRouting)} style={{ background: 'none', border: 'none', cursor: 'pointer', color: useAutoRouting ? FINCEPT.GREEN : FINCEPT.GRAY }}>
            {useAutoRouting ? <ToggleRight size={18} /> : <ToggleLeft size={18} />}
          </button>
        </div>
      </div>

      {/* Messages Area */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {chatMessages.length === 0 ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: FINCEPT.GRAY, textAlign: 'center' }}>
            <Sparkles size={40} style={{ marginBottom: '16px', opacity: 0.5 }} />
            <span style={{ fontSize: '12px', marginBottom: '8px' }}>Start a conversation</span>
            <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>
              {useAutoRouting ? 'Queries will be auto-routed to the best agent' : `Using: ${selectedAgent?.name || 'No agent selected'}`}
            </span>
          </div>
        ) : (
          chatMessages.map((msg, i) => (
            <div key={i} style={{ display: 'flex', justifyContent: msg.role === 'user' ? 'flex-end' : 'flex-start' }}>
              <div style={{
                maxWidth: '80%', padding: '12px', borderRadius: '4px',
                backgroundColor: msg.role === 'user' ? FINCEPT.ORANGE : msg.role === 'system' ? `${FINCEPT.PURPLE}20` : FINCEPT.DARK_BG,
                color: msg.role === 'user' ? FINCEPT.DARK_BG : FINCEPT.WHITE,
                border: msg.role === 'system' ? `1px solid ${FINCEPT.PURPLE}40` : msg.role === 'user' ? 'none' : `1px solid ${FINCEPT.BORDER}`,
              }}>
                {msg.agentName && (
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginBottom: '6px', fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN }}>
                    <Bot size={10} />
                    {msg.agentName.toUpperCase()}
                  </div>
                )}
                {msg.role === 'assistant' ? (
                  <MarkdownRenderer content={msg.content} />
                ) : (
                  <div style={{ fontSize: '11px', whiteSpace: 'pre-wrap', lineHeight: 1.5 }}>{msg.content}</div>
                )}
                <div style={{ fontSize: '9px', marginTop: '8px', opacity: 0.6, color: msg.role === 'user' ? FINCEPT.DARK_BG : FINCEPT.GRAY }}>
                  {msg.timestamp.toLocaleTimeString()}
                </div>
              </div>
            </div>
          ))
        )}
        <div ref={chatEndRef} />
      </div>

      {/* Portfolio Context Bar */}
      {portfolios.length > 0 && (
        <div style={{ padding: '6px 16px', backgroundColor: FINCEPT.DARK_BG, borderTop: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap' }}>
          <span style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700, letterSpacing: '0.5px' }}>PORTFOLIO CTX:</span>
          <div style={{ position: 'relative' }}>
            <button
              onClick={() => setShowPortfolioDropdown(v => !v)}
              style={{ padding: '3px 8px', backgroundColor: selectedPortfolioCtx ? `${FINCEPT.PURPLE}22` : 'transparent', border: `1px solid ${selectedPortfolioCtx ? FINCEPT.PURPLE : FINCEPT.BORDER}`, color: selectedPortfolioCtx ? FINCEPT.PURPLE : FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px' }}
            >
              <Database size={8} />
              {selectedPortfolioCtx?.name || 'Select Portfolio'}
              <ChevronDown size={8} />
            </button>
            {showPortfolioDropdown && (
              <>
                <div onClick={() => setShowPortfolioDropdown(false)} style={{ position: 'fixed', inset: 0, zIndex: 99 }} />
                <div style={{ position: 'absolute', bottom: '28px', left: 0, zIndex: 100, backgroundColor: '#0A0A0A', border: `1px solid ${FINCEPT.PURPLE}`, minWidth: '180px', boxShadow: '0 4px 12px rgba(0,0,0,0.8)' }}>
                  <div
                    onClick={() => { setSelectedPortfolioCtx(null); setShowPortfolioDropdown(false); }}
                    style={{ padding: '6px 10px', cursor: 'pointer', fontSize: '9px', color: FINCEPT.GRAY, borderBottom: `1px solid ${FINCEPT.BORDER}` }}
                  >
                    None
                  </div>
                  {portfolios.map(p => (
                    <div
                      key={p.id}
                      onClick={() => { setSelectedPortfolioCtx(p); setShowPortfolioDropdown(false); }}
                      style={{ padding: '6px 10px', cursor: 'pointer', fontSize: '9px', fontWeight: 600, color: selectedPortfolioCtx?.id === p.id ? FINCEPT.PURPLE : FINCEPT.WHITE, backgroundColor: selectedPortfolioCtx?.id === p.id ? `${FINCEPT.PURPLE}15` : 'transparent' }}
                      onMouseEnter={e => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
                      onMouseLeave={e => { e.currentTarget.style.backgroundColor = selectedPortfolioCtx?.id === p.id ? `${FINCEPT.PURPLE}15` : 'transparent'; }}
                    >
                      {p.name} <span style={{ color: FINCEPT.GRAY }}>{p.currency}</span>
                    </div>
                  ))}
                </div>
              </>
            )}
          </div>
          {selectedPortfolioCtx && (
            <>
              {[
                { label: 'ANALYZE', q: `Analyze my portfolio "${selectedPortfolioCtx.name}" (${selectedPortfolioCtx.currency}). Provide comprehensive risk assessment, diversification analysis, and actionable recommendations.` },
                { label: 'REBALANCE', q: `Suggest rebalancing strategy for portfolio "${selectedPortfolioCtx.name}". Identify overweight/underweight positions and optimal target allocations.` },
                { label: 'RISK', q: `Perform risk analysis on portfolio "${selectedPortfolioCtx.name}". Assess concentration risk, correlation, VaR, and stress test scenarios.` },
              ].map(({ label, q }) => (
                <button
                  key={label}
                  onClick={() => setChatInput(q)}
                  style={{ padding: '3px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.CYAN, fontSize: '8px', fontWeight: 700, cursor: 'pointer', letterSpacing: '0.5px' }}
                  onMouseEnter={e => { e.currentTarget.style.borderColor = FINCEPT.CYAN; }}
                  onMouseLeave={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                >
                  {label}
                </button>
              ))}
            </>
          )}
        </div>
      )}

      {/* Input Area */}
      <div style={{ padding: '12px 16px', backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', gap: '8px' }}>
          <input
            type="text"
            value={chatInput}
            onChange={e => setChatInput(e.target.value)}
            onKeyDown={e => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); handleChatSubmit(); } }}
            placeholder={useAutoRouting ? 'Ask anything - auto-routed to best agent...' : 'Type a message...'}
            style={{ flex: 1, padding: '10px 12px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '11px', fontFamily: '"IBM Plex Mono", monospace' }}
          />
          <button
            onClick={handleChatSubmit}
            disabled={executing || !chatInput.trim()}
            style={{ padding: '10px 16px', backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontSize: '10px', fontWeight: 700, cursor: (executing || !chatInput.trim()) ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', gap: '6px', opacity: (executing || !chatInput.trim()) ? 0.7 : 1 }}
          >
            {executing ? <Loader2 size={12} className="animate-spin" /> : <Zap size={12} />}
            SEND
          </button>
        </div>
      </div>
    </div>
  );
};
