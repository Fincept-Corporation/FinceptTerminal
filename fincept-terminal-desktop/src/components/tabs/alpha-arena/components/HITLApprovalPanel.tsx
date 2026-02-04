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
  AlertCircle, CheckCircle, Info,
} from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import { useCache, cacheKey } from '@/hooks/useCache';
import {
  alphaArenaEnhancedService,
  type ApprovalRequest,
  type HITLStatus,
} from '../services/alphaArenaEnhancedService';

const COLORS = {
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
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
};

const RISK_COLORS: Record<string, string> = {
  low: COLORS.GREEN,
  medium: COLORS.YELLOW,
  high: COLORS.ORANGE,
  critical: COLORS.RED,
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

const HITLApprovalPanel: React.FC<HITLApprovalPanelProps> = ({
  competitionId,
  onApprovalChange,
}) => {
  const [showSettings, setShowSettings] = useState(false);
  const [expandedApproval, setExpandedApproval] = useState<string | null>(null);
  const [actionState, dispatchAction] = useReducer(actionReducer, { status: 'idle' });
  const mountedRef = useRef(true);

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
    <div
      className="rounded-lg overflow-hidden"
      style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}
    >
      {/* Header */}
      <div className="px-4 py-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-2">
          <Shield size={16} style={{ color: COLORS.ORANGE }} />
          <span className="font-semibold text-sm" style={{ color: COLORS.WHITE }}>
            HITL Approval
          </span>
          {pendingApprovals.length > 0 && (
            <span
              className="px-2 py-0.5 rounded-full text-xs font-medium"
              style={{ backgroundColor: COLORS.RED + '20', color: COLORS.RED }}
            >
              {pendingApprovals.length} pending
            </span>
          )}
        </div>
        <div className="flex items-center gap-2">
          <button
            onClick={() => setShowSettings(!showSettings)}
            className="p-1.5 rounded transition-colors hover:bg-[#1A1A1A]"
            title="Settings"
          >
            <Settings size={14} style={{ color: COLORS.GRAY }} />
          </button>
          <button
            onClick={() => hitlCache.refresh()}
            disabled={isLoading}
            className="p-1.5 rounded transition-colors hover:bg-[#1A1A1A]"
            title="Refresh"
          >
            <RefreshCw
              size={14}
              className={isLoading ? 'animate-spin' : ''}
              style={{ color: COLORS.GRAY }}
            />
          </button>
        </div>
      </div>

      {/* Error */}
      {error && (
        <div className="px-4 py-2 flex items-center gap-2" style={{ backgroundColor: COLORS.RED + '10' }}>
          <AlertCircle size={14} style={{ color: COLORS.RED }} />
          <span className="text-xs" style={{ color: COLORS.RED }}>{error}</span>
          <button onClick={() => dispatchAction({ type: 'CLEAR_ERROR' })} className="ml-auto">
            <X size={12} style={{ color: COLORS.RED }} />
          </button>
        </div>
      )}

      {/* Settings Panel */}
      {showSettings && hitlStatus && (
        <div className="px-4 py-3 border-b" style={{ borderColor: COLORS.BORDER, backgroundColor: COLORS.CARD_BG }}>
          <h4 className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>Approval Rules</h4>
          <div className="space-y-2">
            {hitlStatus.rules.map(rule => (
              <div key={rule.name} className="flex items-center justify-between">
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-2">
                    <span className="text-xs" style={{ color: COLORS.WHITE }}>{rule.name}</span>
                    <span
                      className="text-xs px-1.5 py-0.5 rounded"
                      style={{ backgroundColor: RISK_COLORS[rule.risk_level] + '20', color: RISK_COLORS[rule.risk_level] }}
                    >
                      {rule.risk_level}
                    </span>
                  </div>
                  <p className="text-xs truncate" style={{ color: COLORS.GRAY }}>{rule.description}</p>
                </div>
                <button
                  onClick={() => handleToggleRule(rule.name, rule.enabled)}
                  className={`w-10 h-5 rounded-full transition-colors relative ${rule.enabled ? 'bg-green-500' : 'bg-gray-600'}`}
                >
                  <div
                    className={`w-4 h-4 rounded-full bg-white absolute top-0.5 transition-transform ${rule.enabled ? 'translate-x-5' : 'translate-x-0.5'}`}
                  />
                </button>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Pending Approvals */}
      <div className="max-h-80 overflow-y-auto">
        {pendingApprovals.length === 0 ? (
          <div className="p-8 text-center">
            <CheckCircle size={32} className="mx-auto mb-2" style={{ color: COLORS.GREEN, opacity: 0.5 }} />
            <p className="text-sm" style={{ color: COLORS.GRAY }}>No pending approvals</p>
            <p className="text-xs mt-1" style={{ color: COLORS.GRAY }}>All trading decisions approved or auto-approved</p>
          </div>
        ) : (
          <div className="divide-y" style={{ borderColor: COLORS.BORDER }}>
            {pendingApprovals.map(approval => (
              <div key={approval.id} className="p-3">
                {/* Header */}
                <div className="flex items-center justify-between mb-2">
                  <div className="flex items-center gap-2">
                    <AlertTriangle
                      size={14}
                      style={{ color: RISK_COLORS[approval.risk_level] }}
                    />
                    <span className="text-xs font-medium" style={{ color: COLORS.WHITE }}>
                      {approval.decision.model_name}
                    </span>
                    <span
                      className="text-xs px-1.5 py-0.5 rounded font-medium"
                      style={{
                        backgroundColor: approval.decision.action === 'buy' ? COLORS.GREEN + '20' :
                          approval.decision.action === 'sell' ? COLORS.RED + '20' : COLORS.GRAY + '20',
                        color: approval.decision.action === 'buy' ? COLORS.GREEN :
                          approval.decision.action === 'sell' ? COLORS.RED : COLORS.GRAY,
                      }}
                    >
                      {approval.decision.action.toUpperCase()}
                    </span>
                  </div>
                  <div className="flex items-center gap-1 text-xs" style={{ color: COLORS.GRAY }}>
                    <Clock size={10} />
                    {getTimeRemaining(approval.expires_at)}
                  </div>
                </div>

                {/* Trade Details */}
                <div className="grid grid-cols-3 gap-2 mb-2 text-xs">
                  <div>
                    <span style={{ color: COLORS.GRAY }}>Symbol: </span>
                    <span style={{ color: COLORS.WHITE }}>{approval.decision.symbol}</span>
                  </div>
                  <div>
                    <span style={{ color: COLORS.GRAY }}>Qty: </span>
                    <span style={{ color: COLORS.WHITE }}>{approval.decision.quantity.toFixed(4)}</span>
                  </div>
                  <div>
                    <span style={{ color: COLORS.GRAY }}>Conf: </span>
                    <span style={{ color: COLORS.WHITE }}>{(approval.decision.confidence * 100).toFixed(0)}%</span>
                  </div>
                </div>

                {/* Risk Level */}
                <div
                  className="text-xs px-2 py-1 rounded mb-2"
                  style={{
                    backgroundColor: RISK_COLORS[approval.risk_level] + '10',
                    color: RISK_COLORS[approval.risk_level],
                  }}
                >
                  {approval.reason.split('\n').slice(0, 2).join(' • ')}
                </div>

                {/* Expandable Reasoning */}
                <button
                  onClick={() => setExpandedApproval(expandedApproval === approval.id ? null : approval.id)}
                  className="flex items-center gap-1 text-xs mb-2"
                  style={{ color: COLORS.GRAY }}
                >
                  {expandedApproval === approval.id ? <ChevronUp size={12} /> : <ChevronDown size={12} />}
                  {expandedApproval === approval.id ? 'Hide' : 'Show'} reasoning
                </button>

                {expandedApproval === approval.id && (
                  <div
                    className="text-xs p-2 rounded mb-2"
                    style={{ backgroundColor: COLORS.CARD_BG, color: COLORS.GRAY }}
                  >
                    {approval.decision.reasoning || 'No reasoning provided'}
                  </div>
                )}

                {/* Actions */}
                <div className="flex gap-2">
                  <button
                    onClick={() => handleApprove(approval.id)}
                    disabled={isProcessing === approval.id}
                    className="flex-1 py-1.5 rounded text-xs font-medium flex items-center justify-center gap-1"
                    style={{
                      backgroundColor: COLORS.GREEN,
                      color: COLORS.DARK_BG,
                      opacity: isProcessing === approval.id ? 0.5 : 1,
                    }}
                  >
                    {isProcessing === approval.id ? (
                      <Loader2 size={12} className="animate-spin" />
                    ) : (
                      <Check size={12} />
                    )}
                    Approve
                  </button>
                  <button
                    onClick={() => handleReject(approval.id)}
                    disabled={isProcessing === approval.id}
                    className="flex-1 py-1.5 rounded text-xs font-medium flex items-center justify-center gap-1"
                    style={{
                      backgroundColor: COLORS.RED,
                      color: COLORS.WHITE,
                      opacity: isProcessing === approval.id ? 0.5 : 1,
                    }}
                  >
                    {isProcessing === approval.id ? (
                      <Loader2 size={12} className="animate-spin" />
                    ) : (
                      <X size={12} />
                    )}
                    Reject
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Footer Stats */}
      {hitlStatus && (
        <div className="px-4 py-2 border-t flex items-center justify-between text-xs" style={{ borderColor: COLORS.BORDER }}>
          <span style={{ color: COLORS.GRAY }}>
            Rules: {hitlStatus.rules.filter(r => r.enabled).length}/{hitlStatus.rules.length} active
          </span>
          <span style={{ color: COLORS.GRAY }}>
            History: {hitlStatus.history_count} decisions
          </span>
        </div>
      )}
    </div>
  );
};

export default withErrorBoundary(HITLApprovalPanel, { name: 'AlphaArena.HITLApprovalPanel' });
