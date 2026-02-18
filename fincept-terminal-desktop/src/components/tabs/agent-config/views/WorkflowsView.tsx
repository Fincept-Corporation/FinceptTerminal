/**
 * WorkflowsView - Financial workflow execution cards + results panel
 */

import React from 'react';
import { Workflow, TrendingUp, Brain, AlertCircle, Target, FileJson, Route, Database } from 'lucide-react';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';
import { FINCEPT, extractAgentResponseText } from '../types';
import type { AgentConfigState } from '../hooks/useAgentConfig';

interface WorkflowsViewProps {
  state: AgentConfigState;
}

export const WorkflowsView: React.FC<WorkflowsViewProps> = ({ state }) => {
  const {
    workflowSymbol, setWorkflowSymbol,
    testQuery, testResult, executing,
    runWorkflow,
  } = state;

  const btnStyle = (color: string) => ({
    width: '100%', padding: '8px',
    backgroundColor: color, color: color === FINCEPT.ORANGE ? FINCEPT.DARK_BG : FINCEPT.WHITE,
    border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700 as const,
    cursor: executing ? 'not-allowed' as const : 'pointer' as const,
    opacity: executing ? 0.7 : 1,
  });

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* Left - Workflow Cards */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        <div style={{ marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Workflow size={16} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>FINANCIAL WORKFLOWS</span>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: '12px' }}>
          {/* Stock Analysis */}
          <WorkflowCard icon={<TrendingUp size={16} style={{ color: FINCEPT.ORANGE }} />} title="STOCK ANALYSIS">
            <input type="text" value={workflowSymbol} onChange={e => setWorkflowSymbol(e.target.value.toUpperCase())} placeholder="Symbol (e.g., AAPL)"
              style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', marginBottom: '12px', fontFamily: '"IBM Plex Mono", monospace' }}
            />
            <button onClick={() => runWorkflow('stock_analysis')} disabled={executing} style={btnStyle(FINCEPT.ORANGE)}>ANALYZE</button>
          </WorkflowCard>

          {/* Portfolio Rebalancing */}
          <WorkflowCard icon={<Brain size={16} style={{ color: FINCEPT.PURPLE }} />} title="PORTFOLIO REBALANCING">
            <p style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '12px', lineHeight: 1.4 }}>
              Optimize your portfolio allocation based on risk/return targets.
            </p>
            <button onClick={() => runWorkflow('portfolio_rebal')} disabled={executing} style={{ ...btnStyle(FINCEPT.PURPLE), color: FINCEPT.WHITE }}>REBALANCE</button>
          </WorkflowCard>

          {/* Risk Assessment */}
          <WorkflowCard icon={<AlertCircle size={16} style={{ color: FINCEPT.RED }} />} title="RISK ASSESSMENT">
            <p style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '12px', lineHeight: 1.4 }}>
              Analyze portfolio risk metrics and potential vulnerabilities.
            </p>
            <button onClick={() => runWorkflow('risk_assessment')} disabled={executing} style={{ ...btnStyle(FINCEPT.RED), color: FINCEPT.WHITE }}>ASSESS RISK</button>
          </WorkflowCard>

          {/* Portfolio Plan */}
          <WorkflowCard icon={<Target size={16} style={{ color: FINCEPT.GREEN }} />} title="PORTFOLIO PLAN">
            <p style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '12px', lineHeight: 1.4 }}>
              Generate an AI-powered investment plan based on goals and risk profile.
            </p>
            <button onClick={() => runWorkflow('portfolio_plan', { goals: 'Balanced growth', budget: 100000, risk_tolerance: 'moderate' })} disabled={executing} style={{ ...btnStyle(FINCEPT.GREEN), color: FINCEPT.DARK_BG }}>CREATE PLAN</button>
          </WorkflowCard>

          {/* Paper Trading */}
          <WorkflowCard icon={<FileJson size={16} style={{ color: FINCEPT.CYAN }} />} title="PAPER TRADING">
            <div style={{ display: 'flex', gap: '6px', marginBottom: '8px' }}>
              <button onClick={() => runWorkflow('paper_trade', { symbol: workflowSymbol, action: 'buy', quantity: 10, price: 150 })} disabled={executing}
                style={{ flex: 1, padding: '6px', backgroundColor: `${FINCEPT.GREEN}30`, color: FINCEPT.GREEN, border: `1px solid ${FINCEPT.GREEN}60`, borderRadius: '2px', fontSize: '8px', fontWeight: 700, cursor: executing ? 'not-allowed' : 'pointer' }}>
                BUY {workflowSymbol || 'AAPL'}
              </button>
              <button onClick={() => runWorkflow('paper_trade', { symbol: workflowSymbol, action: 'sell', quantity: 10, price: 150 })} disabled={executing}
                style={{ flex: 1, padding: '6px', backgroundColor: `${FINCEPT.RED}30`, color: FINCEPT.RED, border: `1px solid ${FINCEPT.RED}60`, borderRadius: '2px', fontSize: '8px', fontWeight: 700, cursor: executing ? 'not-allowed' : 'pointer' }}>
                SELL {workflowSymbol || 'AAPL'}
              </button>
            </div>
            <div style={{ display: 'flex', gap: '6px' }}>
              <button onClick={() => runWorkflow('paper_portfolio')} disabled={executing}
                style={{ flex: 1, padding: '6px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.CYAN, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '8px', fontWeight: 700, cursor: executing ? 'not-allowed' : 'pointer' }}>
                PORTFOLIO
              </button>
              <button onClick={() => runWorkflow('paper_positions')} disabled={executing}
                style={{ flex: 1, padding: '6px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.CYAN, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '8px', fontWeight: 700, cursor: executing ? 'not-allowed' : 'pointer' }}>
                POSITIONS
              </button>
            </div>
          </WorkflowCard>

          {/* Session & Memory */}
          <WorkflowCard icon={<Database size={16} style={{ color: FINCEPT.YELLOW }} />} title="SESSION & MEMORY">
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              <button onClick={() => runWorkflow('save_session', { session_id: `session-${Date.now()}` })} disabled={executing}
                style={{ width: '100%', padding: '6px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.YELLOW, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '8px', fontWeight: 700, cursor: executing ? 'not-allowed' : 'pointer' }}>
                SAVE CURRENT SESSION
              </button>
              <button onClick={() => runWorkflow('save_memory', { content: testQuery || 'Important note', category: 'general' })} disabled={executing}
                style={{ width: '100%', padding: '6px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.YELLOW, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '8px', fontWeight: 700, cursor: executing ? 'not-allowed' : 'pointer' }}>
                SAVE TO MEMORY
              </button>
              <button onClick={() => runWorkflow('search_memories', { query: testQuery || 'recent analysis' })} disabled={executing}
                style={{ width: '100%', padding: '6px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.YELLOW, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '8px', fontWeight: 700, cursor: executing ? 'not-allowed' : 'pointer' }}>
                SEARCH MEMORIES
              </button>
            </div>
          </WorkflowCard>

          {/* Multi-Query */}
          <WorkflowCard icon={<Route size={16} style={{ color: FINCEPT.BLUE }} />} title="MULTI-QUERY">
            <p style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '12px', lineHeight: 1.4 }}>
              Route your query to multiple agents simultaneously for parallel analysis.
            </p>
            <button onClick={() => runWorkflow('multi_query', { query: testQuery })} disabled={executing || !testQuery.trim()}
              style={{ ...btnStyle(FINCEPT.BLUE), color: FINCEPT.WHITE, opacity: (executing || !testQuery.trim()) ? 0.7 : 1, cursor: (executing || !testQuery.trim()) ? 'not-allowed' : 'pointer' }}>
              EXECUTE MULTI-QUERY
            </button>
          </WorkflowCard>
        </div>
      </div>

      {/* Right - Results Panel */}
      <div style={{ width: '350px', minWidth: '350px', backgroundColor: FINCEPT.PANEL_BG, borderLeft: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column', flexShrink: 0 }}>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>WORKFLOW RESULT</span>
        </div>
        <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
          {testResult ? (
            <MarkdownRenderer
              content={extractAgentResponseText(
                typeof testResult === 'string' ? testResult : (testResult.response || JSON.stringify(testResult, null, 2))
              )}
            />
          ) : (
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'center' }}>
              <Workflow size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
              <span>Run a workflow to see results</span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

// Small reusable card wrapper
const WorkflowCard: React.FC<{ icon: React.ReactNode; title: string; children: React.ReactNode }> = ({ icon, title, children }) => (
  <div style={{ padding: '16px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px' }}>
    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
      {icon}
      <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>{title}</span>
    </div>
    {children}
  </div>
);
