/**
 * PlannerView - Execution plan builder and step visualization
 */

import React from 'react';
import { ListTree, Target, Play, Loader2 } from 'lucide-react';
import { FINCEPT } from '../types';
import type { AgentConfigState } from '../hooks/useAgentConfig';

interface PlannerViewProps {
  state: AgentConfigState;
}

export const PlannerView: React.FC<PlannerViewProps> = ({ state }) => {
  const {
    workflowSymbol, setWorkflowSymbol,
    currentPlan, planExecuting, executing,
    createPlan, executePlan,
  } = state;

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* Left - Plan Creation */}
      <div style={{ width: '320px', minWidth: '320px', backgroundColor: FINCEPT.PANEL_BG, borderRight: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column', flexShrink: 0 }}>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', gap: '8px' }}>
          <ListTree size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>EXECUTION PLANNER</span>
        </div>

        <div style={{ padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px' }}>CREATE PLAN</label>
          <input
            type="text"
            value={workflowSymbol}
            onChange={e => setWorkflowSymbol(e.target.value.toUpperCase())}
            placeholder="Symbol (e.g., AAPL)"
            style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', marginBottom: '8px', fontFamily: '"IBM Plex Mono", monospace' }}
          />
          <button
            onClick={createPlan}
            disabled={executing}
            style={{ width: '100%', padding: '8px', backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: executing ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px', opacity: executing ? 0.7 : 1 }}
          >
            {executing ? <Loader2 size={10} className="animate-spin" /> : <Target size={10} />}
            CREATE PLAN
          </button>
        </div>

        {/* Templates */}
        <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
          <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px' }}>TEMPLATES</label>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
            {[
              { id: 'stock_analysis', name: 'Stock Analysis', desc: 'Fundamental + technical' },
              { id: 'portfolio_rebalance', name: 'Portfolio Rebalance', desc: 'Optimize allocation' },
              { id: 'sector_rotation', name: 'Sector Rotation', desc: 'Identify opportunities' },
              { id: 'risk_audit', name: 'Risk Audit', desc: 'Risk assessment' },
            ].map(template => (
              <button
                key={template.id}
                onClick={() => { setWorkflowSymbol('AAPL'); createPlan(); }}
                style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', textAlign: 'left', transition: 'all 0.2s' }}
                onMouseEnter={e => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
              >
                <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '2px' }}>{template.name}</div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{template.desc}</div>
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Center - Plan Visualization */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
        {currentPlan ? (
          <>
            <div style={{ padding: '12px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
              <div>
                <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>{currentPlan.name}</div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{currentPlan.description}</div>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ padding: '4px 8px', backgroundColor: currentPlan.is_complete ? `${FINCEPT.GREEN}20` : currentPlan.has_failed ? `${FINCEPT.RED}20` : `${FINCEPT.ORANGE}20`, color: currentPlan.is_complete ? FINCEPT.GREEN : currentPlan.has_failed ? FINCEPT.RED : FINCEPT.ORANGE, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                  {currentPlan.status}
                </span>
                <button
                  onClick={executePlan}
                  disabled={planExecuting || currentPlan.is_complete}
                  style={{ padding: '6px 12px', backgroundColor: currentPlan.is_complete ? FINCEPT.GREEN : FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: (planExecuting || currentPlan.is_complete) ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', gap: '4px', opacity: planExecuting ? 0.7 : 1 }}
                >
                  {planExecuting ? <Loader2 size={10} className="animate-spin" /> : <Play size={10} />}
                  {currentPlan.is_complete ? 'DONE' : 'EXECUTE'}
                </button>
              </div>
            </div>

            <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {currentPlan.steps.map((step, idx) => (
                  <div
                    key={step.id}
                    style={{
                      padding: '12px', backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderLeft: `3px solid ${step.status === 'completed' ? FINCEPT.GREEN : step.status === 'failed' ? FINCEPT.RED : step.status === 'running' ? FINCEPT.ORANGE : FINCEPT.GRAY}`,
                      borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '12px',
                    }}
                  >
                    <span style={{ width: '24px', height: '24px', display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.GRAY, fontSize: '10px', fontWeight: 700, borderRadius: '2px' }}>
                      {idx + 1}
                    </span>
                    <div style={{ flex: 1 }}>
                      <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE }}>{step.name}</div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {step.step_type}{step.dependencies.length > 0 && ` • Deps: ${step.dependencies.join(', ')}`}
                      </div>
                    </div>
                    <span style={{
                      padding: '4px 8px',
                      backgroundColor: step.status === 'completed' ? `${FINCEPT.GREEN}20` : step.status === 'failed' ? `${FINCEPT.RED}20` : step.status === 'running' ? `${FINCEPT.ORANGE}20` : FINCEPT.BORDER,
                      color: step.status === 'completed' ? FINCEPT.GREEN : step.status === 'failed' ? FINCEPT.RED : step.status === 'running' ? FINCEPT.ORANGE : FINCEPT.GRAY,
                      fontSize: '8px', fontWeight: 700, borderRadius: '2px', textTransform: 'uppercase',
                    }}>
                      {step.status}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          </>
        ) : (
          <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: FINCEPT.PANEL_BG }}>
            <div style={{ textAlign: 'center', color: FINCEPT.MUTED }}>
              <ListTree size={40} style={{ marginBottom: '12px', opacity: 0.5 }} />
              <div style={{ fontSize: '11px' }}>Create a plan to visualize steps</div>
            </div>
          </div>
        )}
      </div>

      {/* Right - Results (only when plan is complete) */}
      {currentPlan?.is_complete && currentPlan.context && (
        <div style={{ width: '300px', minWidth: '300px', backgroundColor: FINCEPT.PANEL_BG, borderLeft: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column', flexShrink: 0 }}>
          <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>PLAN RESULTS</span>
          </div>
          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            <pre style={{ fontSize: '9px', color: FINCEPT.WHITE, whiteSpace: 'pre-wrap', wordBreak: 'break-word', margin: 0, fontFamily: '"IBM Plex Mono", monospace' }}>
              {JSON.stringify(currentPlan.context, null, 2)}
            </pre>
          </div>
        </div>
      )}
    </div>
  );
};
