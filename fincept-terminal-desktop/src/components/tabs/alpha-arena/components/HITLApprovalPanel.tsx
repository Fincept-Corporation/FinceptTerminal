/**
 * HITL Approval Panel - Human-in-the-Loop Trading Approval
 *
 * Allows users to review and approve/reject AI trading decisions
 * before execution. Shows pending approvals with risk levels.
 */

import React, { useState, useReducer, useRef, useEffect } from 'react';
import {
  Shield, AlertTriangle, Check, X, Clock, RefreshCw,
  Settings, ChevronDown, ChevronUp, Loader2,
  AlertCircle, CheckCircle,
} from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import { useCache, cacheKey } from '@/hooks/useCache';
import {
  alphaArenaEnhancedService,
  type ApprovalRequest,
  type HITLStatus,
} from '../services/alphaArenaEnhancedService';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#3B82F6',
  YELLOW: '#EAB308',
  PURPLE: '#8B5CF6',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

const TERMINAL_FONT = '"IBM Plex Mono", "Consolas", monospace';

const RISK_COLORS: Record<string, string> = {
  low: FINCEPT.GREEN,
  medium: FINCEPT.YELLOW,
  high: FINCEPT.ORANGE,
  critical: FINCEPT.RED,
};

interface HITLApprovalPanelProps {
  competitionId?: string;
  onApprovalChange?: () => void;
}

// State machine for approve/reject actions
type ActionState =
  | { status: 'idle' }
  | { status: 'processing'; requestId: string }
  | { status: 'error'; error: string };

type ActionDispatch =
  | { type: 'PROCESS_START'; requestId: string }
  | { type: 'PROCESS_SUCCESS' }
  | { type: 'PROCESS_ERROR'; error: string }
  | { type: 'CLEAR_ERROR' };

function actionReducer(_state: ActionState, action: ActionDispatch): ActionState {
  switch (action.type) {
    case 'PROCESS_START': return { status: 'processing', requestId: action.requestId };
    case 'PROCESS_SUCCESS': return { status: 'idle' };
    case 'PROCESS_ERROR': return { status: 'error', error: action.error };
    case 'CLEAR_ERROR': return { status: 'idle' };
    default: return _state;
  }
}

interface HITLData {
  status: HITLStatus | null;
  pending_approvals: ApprovalRequest[];
}

const HITL_STYLES = `
  .hitl-panel *::-webkit-scrollbar { width: 6px; height: 6px; }
  .hitl-panel *::-webkit-scrollbar-track { background: #000000; }
  .hitl-panel *::-webkit-scrollbar-thumb { background: #2A2A2A; }
  @keyframes hitl-spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
`;

const HITLApprovalPanel: React.FC<HITLApprovalPanelProps> = ({
  competitionId,
  onApprovalChange,
}) => {
  const [showSettings, setShowSettings] = useState(false);
  const [expandedApproval, setExpandedApproval] = useState<string | null>(null);
  const [actionState, dispatchAction] = useReducer(actionReducer, { status: 'idle' });
  const mountedRef = useRef(true);
  const [hoveredBtn, setHoveredBtn] = useState<string | null>(null);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  // Cache: HITL status + pending approvals with auto-refresh every 5s
  const hitlCache = useCache<HITLData>({
    key: cacheKey('alpha-arena', 'hitl', competitionId || 'none'),
    category: 'alpha-arena',
    fetcher: async () => {
      const [statusResult, pendingResult] = await Promise.all([
        alphaArenaEnhancedService.getHITLStatus(),
        alphaArenaEnhancedService.getPendingApprovals(),
      ]);

      return {
        status: statusResult.success && statusResult.status ? statusResult.status : null,
        pending_approvals: pendingResult.success && pendingResult.pending_approvals
          ? pendingResult.pending_approvals
          : [],
      };
    },
    ttl: 30,
    enabled: !!competitionId,
    staleWhileRevalidate: true,
    refetchInterval: 5000,
  });

  const hitlStatus = hitlCache.data?.status || null;
  const pendingApprovals = hitlCache.data?.pending_approvals || [];
  const isLoading = hitlCache.isLoading;
  const fetchError = hitlCache.error;

  const handleApprove = async (requestId: string) => {
    if (actionState.status === 'processing') return;
    dispatchAction({ type: 'PROCESS_START', requestId });
    try {
      const result = await alphaArenaEnhancedService.approveDecision(requestId);
      if (!mountedRef.current) return;
      if (result.success) {
        dispatchAction({ type: 'PROCESS_SUCCESS' });
        hitlCache.invalidate();
        onApprovalChange?.();
      } else {
        dispatchAction({ type: 'PROCESS_ERROR', error: result.error || 'Failed to approve' });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatchAction({ type: 'PROCESS_ERROR', error: err instanceof Error ? err.message : 'Failed to approve' });
    }
  };

  const handleReject = async (requestId: string) => {
    if (actionState.status === 'processing') return;
    dispatchAction({ type: 'PROCESS_START', requestId });
    try {
      const result = await alphaArenaEnhancedService.rejectDecision(requestId);
      if (!mountedRef.current) return;
      if (result.success) {
        dispatchAction({ type: 'PROCESS_SUCCESS' });
        hitlCache.invalidate();
        onApprovalChange?.();
      } else {
        dispatchAction({ type: 'PROCESS_ERROR', error: result.error || 'Failed to reject' });
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatchAction({ type: 'PROCESS_ERROR', error: err instanceof Error ? err.message : 'Failed to reject' });
    }
  };

  const handleToggleRule = async (ruleName: string, currentEnabled: boolean) => {
    try {
      const result = await alphaArenaEnhancedService.updateHITLRule(ruleName, !currentEnabled);
      if (!mountedRef.current) return;
      if (result.success) {
        hitlCache.refresh();
      }
    } catch (err) {
      if (!mountedRef.current) return;
      dispatchAction({ type: 'PROCESS_ERROR', error: err instanceof Error ? err.message : 'Failed to update rule' });
    }
  };

  const getTimeRemaining = (expiresAt: string | null): string => {
    if (!expiresAt) return 'No expiry';
    const remaining = new Date(expiresAt).getTime() - Date.now();
    if (remaining <= 0) return 'Expired';
    const seconds = Math.floor(remaining / 1000);
    if (seconds < 60) return `${seconds}s`;
    const minutes = Math.floor(seconds / 60);
    return `${minutes}m ${seconds % 60}s`;
  };

  const isProcessing = actionState.status === 'processing' ? actionState.requestId : null;
  const error = actionState.status === 'error' ? actionState.error : fetchError?.message || null;

  return (
    <div className="hitl-panel" style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      overflow: 'hidden',
      fontFamily: TERMINAL_FONT,
    }}>
      <style>{HITL_STYLES}</style>

      {/* Header */}
      <div style={{
        padding: '10px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Shield size={16} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontWeight: 600, fontSize: '12px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            HITL APPROVAL
          </span>
          {pendingApprovals.length > 0 && (
            <span style={{
              padding: '1px 8px',
              fontSize: '10px',
              fontWeight: 600,
              backgroundColor: FINCEPT.RED + '20',
              color: FINCEPT.RED,
              fontFamily: TERMINAL_FONT,
            }}>
              {pendingApprovals.length} PENDING
            </span>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <button
            onClick={() => setShowSettings(!showSettings)}
            onMouseEnter={() => setHoveredBtn('settings')}
            onMouseLeave={() => setHoveredBtn(null)}
            style={{
              padding: '4px',
              backgroundColor: hoveredBtn === 'settings' ? FINCEPT.HOVER : 'transparent',
              border: 'none',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
            }}
            title="Settings"
          >
            <Settings size={14} style={{ color: FINCEPT.GRAY }} />
          </button>
          <button
            onClick={() => hitlCache.refresh()}
            disabled={isLoading}
            onMouseEnter={() => setHoveredBtn('refresh')}
            onMouseLeave={() => setHoveredBtn(null)}
            style={{
              padding: '4px',
              backgroundColor: hoveredBtn === 'refresh' ? FINCEPT.HOVER : 'transparent',
              border: 'none',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
            }}
            title="Refresh"
          >
            <RefreshCw
              size={14}
              style={{
                color: FINCEPT.GRAY,
                animation: isLoading ? 'hitl-spin 1s linear infinite' : 'none',
              }}
            />
          </button>
        </div>
      </div>

      {/* Error */}
      {error && (
        <div style={{
          padding: '8px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          backgroundColor: FINCEPT.RED + '10',
        }}>
          <AlertCircle size={14} style={{ color: FINCEPT.RED }} />
          <span style={{ fontSize: '11px', color: FINCEPT.RED, fontFamily: TERMINAL_FONT }}>{error}</span>
          <button
            onClick={() => dispatchAction({ type: 'CLEAR_ERROR' })}
            style={{ marginLeft: 'auto', background: 'none', border: 'none', cursor: 'pointer' }}
          >
            <X size={12} style={{ color: FINCEPT.RED }} />
          </button>
        </div>
      )}

      {/* Settings Panel */}
      {showSettings && hitlStatus && (
        <div style={{
          padding: '12px 16px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.CARD_BG,
        }}>
          <div style={{ fontSize: '10px', fontWeight: 500, marginBottom: '8px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            APPROVAL RULES
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            {hitlStatus.rules.map(rule => (
              <div key={rule.name} style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <span style={{ fontSize: '11px', color: FINCEPT.WHITE, fontFamily: TERMINAL_FONT }}>{rule.name}</span>
                    <span style={{
                      fontSize: '9px',
                      padding: '1px 6px',
                      backgroundColor: RISK_COLORS[rule.risk_level] + '20',
                      color: RISK_COLORS[rule.risk_level],
                      fontFamily: TERMINAL_FONT,
                    }}>
                      {rule.risk_level.toUpperCase()}
                    </span>
                  </div>
                  <p style={{ fontSize: '10px', color: FINCEPT.GRAY, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                    {rule.description}
                  </p>
                </div>
                <button
                  onClick={() => handleToggleRule(rule.name, rule.enabled)}
                  style={{
                    width: '36px',
                    height: '18px',
                    backgroundColor: rule.enabled ? FINCEPT.GREEN : '#444',
                    border: 'none',
                    cursor: 'pointer',
                    position: 'relative',
                    flexShrink: 0,
                  }}
                >
                  <div style={{
                    width: '14px',
                    height: '14px',
                    backgroundColor: FINCEPT.WHITE,
                    position: 'absolute',
                    top: '2px',
                    left: rule.enabled ? '20px' : '2px',
                    transition: 'left 0.2s',
                  }} />
                </button>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Pending Approvals */}
      <div style={{ maxHeight: '320px', overflowY: 'auto' }}>
        {pendingApprovals.length === 0 ? (
          <div style={{ padding: '32px 16px', textAlign: 'center' }}>
            <CheckCircle size={32} style={{ color: FINCEPT.GREEN, opacity: 0.5, margin: '0 auto 8px' }} />
            <p style={{ fontSize: '12px', color: FINCEPT.GRAY }}>No pending approvals</p>
            <p style={{ fontSize: '10px', marginTop: '4px', color: FINCEPT.GRAY }}>
              All trading decisions approved or auto-approved
            </p>
          </div>
        ) : (
          <div>
            {pendingApprovals.map((approval, idx) => (
              <div key={approval.id} style={{
                padding: '12px',
                borderBottom: idx < pendingApprovals.length - 1 ? `1px solid ${FINCEPT.BORDER}` : 'none',
              }}>
                {/* Header */}
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <AlertTriangle size={14} style={{ color: RISK_COLORS[approval.risk_level] }} />
                    <span style={{ fontSize: '11px', fontWeight: 500, color: FINCEPT.WHITE, fontFamily: TERMINAL_FONT }}>
                      {approval.decision.model_name}
                    </span>
                    <span style={{
                      fontSize: '10px',
                      padding: '1px 6px',
                      fontWeight: 500,
                      fontFamily: TERMINAL_FONT,
                      backgroundColor: approval.decision.action === 'buy' ? FINCEPT.GREEN + '20' :
                        approval.decision.action === 'sell' ? FINCEPT.RED + '20' : FINCEPT.GRAY + '20',
                      color: approval.decision.action === 'buy' ? FINCEPT.GREEN :
                        approval.decision.action === 'sell' ? FINCEPT.RED : FINCEPT.GRAY,
                    }}>
                      {approval.decision.action.toUpperCase()}
                    </span>
                  </div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '10px', color: FINCEPT.GRAY }}>
                    <Clock size={10} />
                    {getTimeRemaining(approval.expires_at)}
                  </div>
                </div>

                {/* Trade Details */}
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr 1fr',
                  gap: '8px',
                  marginBottom: '8px',
                  fontSize: '11px',
                }}>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Symbol: </span>
                    <span style={{ color: FINCEPT.WHITE }}>{approval.decision.symbol}</span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Qty: </span>
                    <span style={{ color: FINCEPT.WHITE }}>{approval.decision.quantity.toFixed(4)}</span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Conf: </span>
                    <span style={{ color: FINCEPT.WHITE }}>{(approval.decision.confidence * 100).toFixed(0)}%</span>
                  </div>
                </div>

                {/* Risk Level */}
                <div style={{
                  fontSize: '10px',
                  padding: '4px 8px',
                  marginBottom: '8px',
                  backgroundColor: RISK_COLORS[approval.risk_level] + '10',
                  color: RISK_COLORS[approval.risk_level],
                  fontFamily: TERMINAL_FONT,
                }}>
                  {approval.reason.split('\n').slice(0, 2).join(' | ')}
                </div>

                {/* Expandable Reasoning */}
                <button
                  onClick={() => setExpandedApproval(expandedApproval === approval.id ? null : approval.id)}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    fontSize: '10px',
                    marginBottom: '8px',
                    color: FINCEPT.GRAY,
                    background: 'none',
                    border: 'none',
                    cursor: 'pointer',
                    fontFamily: TERMINAL_FONT,
                    padding: 0,
                  }}
                >
                  {expandedApproval === approval.id ? <ChevronUp size={12} /> : <ChevronDown size={12} />}
                  {expandedApproval === approval.id ? 'Hide' : 'Show'} reasoning
                </button>

                {expandedApproval === approval.id && (
                  <div style={{
                    fontSize: '10px',
                    padding: '8px',
                    marginBottom: '8px',
                    backgroundColor: FINCEPT.CARD_BG,
                    color: FINCEPT.GRAY,
                    fontFamily: TERMINAL_FONT,
                    border: `1px solid ${FINCEPT.BORDER}`,
                  }}>
                    {approval.decision.reasoning || 'No reasoning provided'}
                  </div>
                )}

                {/* Actions */}
                <div style={{ display: 'flex', gap: '8px' }}>
                  <button
                    onClick={() => handleApprove(approval.id)}
                    disabled={isProcessing === approval.id}
                    onMouseEnter={() => setHoveredBtn(`approve-${approval.id}`)}
                    onMouseLeave={() => setHoveredBtn(null)}
                    style={{
                      flex: 1,
                      padding: '6px',
                      fontSize: '11px',
                      fontWeight: 600,
                      fontFamily: TERMINAL_FONT,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '4px',
                      backgroundColor: FINCEPT.GREEN,
                      color: FINCEPT.DARK_BG,
                      border: 'none',
                      cursor: isProcessing === approval.id ? 'not-allowed' : 'pointer',
                      opacity: isProcessing === approval.id ? 0.5 : 1,
                      letterSpacing: '0.5px',
                    }}
                  >
                    {isProcessing === approval.id ? (
                      <Loader2 size={12} style={{ animation: 'hitl-spin 1s linear infinite' }} />
                    ) : (
                      <Check size={12} />
                    )}
                    APPROVE
                  </button>
                  <button
                    onClick={() => handleReject(approval.id)}
                    disabled={isProcessing === approval.id}
                    onMouseEnter={() => setHoveredBtn(`reject-${approval.id}`)}
                    onMouseLeave={() => setHoveredBtn(null)}
                    style={{
                      flex: 1,
                      padding: '6px',
                      fontSize: '11px',
                      fontWeight: 600,
                      fontFamily: TERMINAL_FONT,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '4px',
                      backgroundColor: FINCEPT.RED,
                      color: FINCEPT.WHITE,
                      border: 'none',
                      cursor: isProcessing === approval.id ? 'not-allowed' : 'pointer',
                      opacity: isProcessing === approval.id ? 0.5 : 1,
                      letterSpacing: '0.5px',
                    }}
                  >
                    {isProcessing === approval.id ? (
                      <Loader2 size={12} style={{ animation: 'hitl-spin 1s linear infinite' }} />
                    ) : (
                      <X size={12} />
                    )}
                    REJECT
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Footer Stats */}
      {hitlStatus && (
        <div style={{
          padding: '6px 16px',
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          fontSize: '9px',
          backgroundColor: FINCEPT.HEADER_BG,
        }}>
          <span style={{ color: FINCEPT.GRAY }}>
            Rules: {hitlStatus.rules.filter(r => r.enabled).length}/{hitlStatus.rules.length} active
          </span>
          <span style={{ color: FINCEPT.GRAY }}>
            History: {hitlStatus.history_count} decisions
          </span>
        </div>
      )}
    </div>
  );
};

export default withErrorBoundary(HITLApprovalPanel, { name: 'AlphaArena.HITLApprovalPanel' });
