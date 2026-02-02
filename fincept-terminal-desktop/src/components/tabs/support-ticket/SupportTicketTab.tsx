import React, { useState, useEffect, useReducer, useCallback, useRef } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { SupportApiService } from '@/services/forum/supportApi';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { RefreshCw, Plus, ArrowLeft, Send, CheckCircle2, XCircle, Clock } from 'lucide-react';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { showWarning, showSuccess, showError } from '@/utils/notifications';

// ══════════════════════════════════════════════════════════════
// STATE MACHINE & TYPES
// ══════════════════════════════════════════════════════════════
type SupportView = 'list' | 'create' | 'details';
type SupportStatus = 'idle' | 'loading' | 'ready' | 'error';

interface SupportState {
  status: SupportStatus;
  currentView: SupportView;
  tickets: any[];
  selectedTicket: any | null;
  stats: { total_tickets: number; open_tickets: number; resolved_tickets: number };
  error: string | null;
}

type SupportAction =
  | { type: 'SET_LOADING' }
  | { type: 'SET_READY' }
  | { type: 'SET_ERROR'; error: string }
  | { type: 'SET_VIEW'; view: SupportView }
  | { type: 'SET_TICKETS'; tickets: any[] }
  | { type: 'SET_SELECTED_TICKET'; ticket: any | null }
  | { type: 'SET_STATS'; stats: { total_tickets: number; open_tickets: number; resolved_tickets: number } }
  | { type: 'RESET_ERROR' };

// Demo ticket to show users how the system works
const DEMO_TICKET = {
  id: 'DEMO-001',
  ticket_id: 'DEMO-001',
  subject: 'Welcome to Fincept Support - Demo Ticket',
  description: `This is a demonstration ticket to help you understand how our support system works.

HOW TO USE SUPPORT:
1. Click "+ CREATE TICKET" to submit a new support request
2. Fill in the subject, category, priority, and detailed description
3. Our support team will respond within 24 hours
4. You can add messages to continue the conversation
5. Close the ticket when your issue is resolved

AVAILABLE CATEGORIES:
- Technical Issue: For bugs, errors, or technical problems
- Billing: Questions about payments, subscriptions, or invoices
- Feature Request: Suggest new features or improvements
- Bug Report: Report software bugs with detailed steps
- Account: Account access, settings, or profile issues
- Other: General inquiries

This demo ticket shows you exactly how your real support tickets will appear. Feel free to explore and create your own ticket when you need assistance!`,
  category: 'technical',
  priority: 'medium',
  status: 'open',
  created_at: new Date().toISOString(),
  messages: [
    {
      sender_type: 'support',
      message: 'Welcome to Fincept Terminal Support! This is how our support team will respond to your tickets. We typically respond within 24 hours during business days.',
      created_at: new Date(Date.now() - 3600000).toISOString()
    },
    {
      sender_type: 'user',
      message: 'Thank you! I understand how the system works now.',
      created_at: new Date(Date.now() - 1800000).toISOString()
    },
    {
      sender_type: 'support',
      message: 'Great! Feel free to create a real ticket anytime you need help. Our team is here to assist you 24/7.',
      created_at: new Date(Date.now() - 900000).toISOString()
    }
  ]
};

const initialState: SupportState = {
  status: 'idle',
  currentView: 'list',
  tickets: [DEMO_TICKET],
  selectedTicket: null,
  stats: { total_tickets: 1, open_tickets: 1, resolved_tickets: 0 },
  error: null
};

function supportReducer(state: SupportState, action: SupportAction): SupportState {
  switch (action.type) {
    case 'SET_LOADING':
      return { ...state, status: 'loading', error: null };
    case 'SET_READY':
      return { ...state, status: 'ready', error: null };
    case 'SET_ERROR':
      return { ...state, status: 'error', error: action.error };
    case 'SET_VIEW':
      return { ...state, currentView: action.view };
    case 'SET_TICKETS':
      return { ...state, tickets: action.tickets };
    case 'SET_SELECTED_TICKET':
      return { ...state, selectedTicket: action.ticket };
    case 'SET_STATS':
      return { ...state, stats: action.stats };
    case 'RESET_ERROR':
      return { ...state, error: null };
    default:
      return state;
  }
}

const API_TIMEOUT_MS = 30000;

const SupportTicketTab: React.FC = () => {
  const { t } = useTranslation('support');
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const C = {
    ORANGE: colors.primary,
    WHITE: colors.text,
    RED: colors.alert,
    GREEN: colors.secondary,
    YELLOW: colors.warning,
    GRAY: colors.textMuted,
    BLUE: colors.info,
    PURPLE: colors.purple,
    CYAN: colors.accent,
    DARK_BG: colors.background,
    PANEL_BG: colors.panel
  };

  const { session } = useAuth();
  const [state, dispatch] = useReducer(supportReducer, initialState);
  const { status, currentView, tickets, selectedTicket, stats, error } = state;

  // Cleanup refs for race condition prevention
  const mountedRef = useRef(true);
  const fetchIdRef = useRef(0);

  const [currentTime, setCurrentTime] = useState(new Date());
  const loading = status === 'loading';

  // Create ticket form
  const [newTicket, setNewTicket] = useState({
    subject: '',
    description: '',
    category: 'technical',
    priority: 'medium'
  });

  // Message form
  const [newMessage, setNewMessage] = useState('');

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Load tickets with race condition prevention
  const loadTickets = useCallback(async () => {
    if (!session?.api_key) return;

    const currentFetchId = ++fetchIdRef.current;
    dispatch({ type: 'SET_LOADING' });

    try {
      const result = await withTimeout(
        SupportApiService.getUserTickets(session.api_key),
        API_TIMEOUT_MS,
        'Load tickets timeout'
      );

      // Prevent stale updates
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

      if (result.success) {
        const apiTickets = result.data?.tickets || result.data || [];
        dispatch({ type: 'SET_TICKETS', tickets: [DEMO_TICKET, ...apiTickets] });
        dispatch({ type: 'SET_READY' });
      } else {
        dispatch({ type: 'SET_TICKETS', tickets: [DEMO_TICKET] });
        dispatch({ type: 'SET_READY' });
      }
    } catch (err) {
      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
      dispatch({ type: 'SET_TICKETS', tickets: [DEMO_TICKET] });
      dispatch({ type: 'SET_ERROR', error: err instanceof Error ? err.message : 'Failed to load tickets' });
    }
  }, [session?.api_key]);

  // Load stats
  const loadStats = useCallback(async () => {
    if (!session?.api_key) return;

    try {
      const result = await withTimeout(
        SupportApiService.getSupportStats(session.api_key),
        API_TIMEOUT_MS,
        'Load stats timeout'
      );

      if (!mountedRef.current) return;

      if (result.success && result.data) {
        dispatch({ type: 'SET_STATS', stats: result.data });
      }
    } catch (err) {
      // Stats failure is non-critical, don't show error
      console.error('[SupportTab] Failed to load stats:', err);
    }
  }, [session?.api_key]);

  // Initial load
  useEffect(() => {
    if (session?.api_key) {
      loadTickets();
      loadStats();
    }
  }, [session?.api_key, loadTickets, loadStats]);

  // Create ticket with validation
  const createTicket = useCallback(async () => {
    if (!session?.api_key) return;

    // Validate and sanitize inputs
    const sanitizedSubject = sanitizeInput(newTicket.subject);
    const sanitizedDescription = sanitizeInput(newTicket.description);

    if (!sanitizedSubject || !sanitizedDescription) {
      showWarning('Please fill in all required fields');
      return;
    }

    dispatch({ type: 'SET_LOADING' });

    try {
      const result = await withTimeout(
        SupportApiService.createTicket(session.api_key, {
          ...newTicket,
          subject: sanitizedSubject,
          description: sanitizedDescription
        }),
        API_TIMEOUT_MS,
        'Create ticket timeout'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        showSuccess('Ticket created successfully');
        setNewTicket({ subject: '', description: '', category: 'technical', priority: 'medium' });
        dispatch({ type: 'SET_VIEW', view: 'list' });
        dispatch({ type: 'SET_READY' });
        loadTickets();
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || 'Failed to create ticket' });
        showError('Failed to create ticket', [
          { label: 'ERROR', value: result.error || 'Unknown error' }
        ]);
      }
    } catch (err) {
      if (!mountedRef.current) return;
      const errorMsg = err instanceof Error ? err.message : 'Failed to create ticket';
      dispatch({ type: 'SET_ERROR', error: errorMsg });
      showError('Failed to create ticket', [
        { label: 'ERROR', value: errorMsg }
      ]);
    }
  }, [session?.api_key, newTicket, loadTickets]);

  // View ticket details
  const viewTicketDetails = useCallback(async (ticketId: number | string) => {
    if (ticketId === 'DEMO-001') {
      dispatch({ type: 'SET_SELECTED_TICKET', ticket: DEMO_TICKET });
      dispatch({ type: 'SET_VIEW', view: 'details' });
      return;
    }

    if (!session?.api_key) return;

    dispatch({ type: 'SET_LOADING' });

    try {
      const result = await withTimeout(
        SupportApiService.getTicketDetails(session.api_key, ticketId as number),
        API_TIMEOUT_MS,
        'Load ticket details timeout'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'SET_SELECTED_TICKET', ticket: result.data });
        dispatch({ type: 'SET_VIEW', view: 'details' });
        dispatch({ type: 'SET_READY' });
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || 'Failed to load ticket details' });
        showError('Failed to load ticket details', [
          { label: 'ERROR', value: result.error || 'Unknown error' }
        ]);
      }
    } catch (err) {
      if (!mountedRef.current) return;
      const errorMsg = err instanceof Error ? err.message : 'Failed to load ticket details';
      dispatch({ type: 'SET_ERROR', error: errorMsg });
      showError('Failed to load ticket details', [
        { label: 'ERROR', value: errorMsg }
      ]);
    }
  }, [session?.api_key]);

  // Add message with validation
  const addMessage = useCallback(async () => {
    if (!session?.api_key || !selectedTicket) return;

    const sanitizedMessage = sanitizeInput(newMessage);
    if (!sanitizedMessage.trim()) return;

    dispatch({ type: 'SET_LOADING' });

    try {
      const result = await withTimeout(
        SupportApiService.addTicketMessage(
          session.api_key,
          selectedTicket.id || selectedTicket.ticket_id,
          sanitizedMessage
        ),
        API_TIMEOUT_MS,
        'Add message timeout'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        setNewMessage('');
        dispatch({ type: 'SET_READY' });
        viewTicketDetails(selectedTicket.id || selectedTicket.ticket_id);
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || 'Failed to add message' });
        showError('Failed to add message', [
          { label: 'ERROR', value: result.error || 'Unknown error' }
        ]);
      }
    } catch (err) {
      if (!mountedRef.current) return;
      const errorMsg = err instanceof Error ? err.message : 'Failed to add message';
      dispatch({ type: 'SET_ERROR', error: errorMsg });
      showError('Failed to add message', [
        { label: 'ERROR', value: errorMsg }
      ]);
    }
  }, [session?.api_key, selectedTicket, newMessage, viewTicketDetails]);

  // Update ticket status
  const updateTicketStatus = useCallback(async (newStatus: string) => {
    if (!session?.api_key || !selectedTicket) return;

    dispatch({ type: 'SET_LOADING' });

    try {
      const result = await withTimeout(
        SupportApiService.updateTicket(
          session.api_key,
          selectedTicket.id || selectedTicket.ticket_id,
          newStatus
        ),
        API_TIMEOUT_MS,
        'Update ticket status timeout'
      );

      if (!mountedRef.current) return;

      if (result.success) {
        dispatch({ type: 'SET_READY' });
        viewTicketDetails(selectedTicket.id || selectedTicket.ticket_id);
        loadTickets();
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || 'Failed to update ticket' });
        showError('Failed to update ticket', [
          { label: 'ERROR', value: result.error || 'Unknown error' }
        ]);
      }
    } catch (err) {
      if (!mountedRef.current) return;
      const errorMsg = err instanceof Error ? err.message : 'Failed to update ticket';
      dispatch({ type: 'SET_ERROR', error: errorMsg });
      showError('Failed to update ticket', [
        { label: 'ERROR', value: errorMsg }
      ]);
    }
  }, [session?.api_key, selectedTicket, viewTicketDetails, loadTickets]);

  // View change handler
  const setCurrentView = useCallback((view: SupportView) => {
    dispatch({ type: 'SET_VIEW', view });
  }, []);

  const formatDate = (dateStr?: string) => {
    if (!dateStr) return 'N/A';
    return new Date(dateStr).toLocaleString('en-US', {
      year: 'numeric',
      month: 'short',
      day: 'numeric',
      hour: '2-digit',
      minute: '2-digit'
    });
  };

  const getStatusColor = (ticketStatus: string) => {
    switch (ticketStatus?.toLowerCase()) {
      case 'open':
      case 'in_progress':
        return C.CYAN;
      case 'resolved':
      case 'closed':
        return C.GREEN;
      case 'pending':
        return C.YELLOW;
      default:
        return C.GRAY;
    }
  };

  const getPriorityColor = (priority: string) => {
    switch (priority?.toLowerCase()) {
      case 'high':
      case 'urgent':
        return C.RED;
      case 'medium':
        return C.ORANGE;
      case 'low':
        return C.GREEN;
      default:
        return C.GRAY;
    }
  };

  const getStatusIcon = (ticketStatus: string) => {
    switch (ticketStatus?.toLowerCase()) {
      case 'open':
      case 'in_progress':
        return <Clock size={14} />;
      case 'resolved':
      case 'closed':
        return <CheckCircle2 size={14} />;
      default:
        return <XCircle size={14} />;
    }
  };

  // Main render
  if (!session) {
    return (
      <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: C.DARK_BG }}>
        <div style={{ textAlign: 'center', fontFamily: 'Consolas, monospace' }}>
          <div style={{ color: C.ORANGE, fontSize: '16px', marginBottom: '8px', fontWeight: 'bold' }}>{t('auth.required')}</div>
          <div style={{ color: C.GRAY, fontSize: '12px' }}>{t('auth.loginMessage')}</div>
        </div>
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: C.DARK_BG,
      color: C.WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        *::-webkit-scrollbar-track {
          background: #1a1a1a;
        }
        *::-webkit-scrollbar-thumb {
          background: #404040;
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: #525252;
        }
      `}</style>

      {/* Header Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 12px',
        backgroundColor: C.PANEL_BG,
        borderBottom: `1px solid ${C.GRAY}`,
        fontSize: '13px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: C.ORANGE, fontWeight: 'bold' }}>FINCEPT</span>
          <span style={{ color: C.WHITE }}>{t('title')}</span>
          <span style={{ color: C.GRAY }}>|</span>
          <span style={{ color: C.WHITE, fontSize: '11px' }}>
            {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: C.GRAY, fontSize: '11px' }}>{t('header.tickets')}:</span>
          <span style={{ color: C.CYAN, fontSize: '11px', fontWeight: 'bold' }}>{stats.total_tickets || 0}</span>
          <span style={{ color: C.GRAY }}>|</span>
          <span style={{ color: C.YELLOW, fontSize: '11px' }}>{t('header.open')}:</span>
          <span style={{ color: C.YELLOW, fontSize: '11px', fontWeight: 'bold' }}>{stats.open_tickets || 0}</span>
          <span style={{ color: C.GRAY }}>|</span>
          <span style={{ color: C.GREEN, fontSize: '11px' }}>{t('header.resolved')}:</span>
          <span style={{ color: C.GREEN, fontSize: '11px', fontWeight: 'bold' }}>{stats.resolved_tickets || 0}</span>
        </div>
      </div>

      {/* Control Panel */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 12px',
        backgroundColor: C.PANEL_BG,
        borderBottom: `1px solid ${C.GRAY}`,
        fontSize: '12px',
        gap: '8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {currentView === 'list' && (
            <>
              <button
                onClick={loadTickets}
                disabled={loading}
                style={{
                  backgroundColor: C.ORANGE,
                  color: 'black',
                  border: 'none',
                  padding: '4px 12px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: loading ? 'not-allowed' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
              >
                <RefreshCw size={12} />
                {t('buttons.refresh')}
              </button>
              <button
                onClick={() => setCurrentView('create')}
                style={{
                  backgroundColor: C.GREEN,
                  color: 'black',
                  border: 'none',
                  padding: '4px 12px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
              >
                <Plus size={12} />
                {t('buttons.createTicket')}
              </button>
            </>
          )}
          {(currentView === 'create' || currentView === 'details') && (
            <button
              onClick={() => setCurrentView('list')}
              style={{
                backgroundColor: C.GRAY,
                color: 'black',
                border: 'none',
                padding: '4px 12px',
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <ArrowLeft size={12} />
              {t('buttons.backToList')}
            </button>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: loading ? C.ORANGE : C.GREEN, fontSize: '14px' }}>●</span>
          <span style={{ color: loading ? C.ORANGE : C.GREEN, fontSize: '11px', fontWeight: 'bold' }}>
            {loading ? t('status.updating') : t('status.ready')}
          </span>
        </div>
      </div>

      {/* Main Content */}
      <ErrorBoundary name="SupportTicketContent" variant="minimal">
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '12px'
      }}>
        {currentView === 'list' && (
          <>
            {/* Section Header */}
            <div style={{
              color: C.ORANGE,
              fontSize: '14px',
              fontWeight: 'bold',
              marginBottom: '8px',
              letterSpacing: '0.5px'
            }}>
              {t('list.supportTickets')}
            </div>
            <div style={{ borderBottom: `1px solid ${C.GRAY}`, marginBottom: '16px' }}></div>

            {/* Tickets Grid */}
            {loading && tickets.length === 0 ? (
              <div style={{ color: C.GRAY, fontSize: '12px', textAlign: 'center', marginTop: '40px' }}>
                {t('list.loading')}
              </div>
            ) : tickets.length > 0 ? (
              <div style={{ display: 'grid', gap: '8px' }}>
                {tickets.map((ticket: any) => {
                  const isDemoTicket = ticket.id === 'DEMO-001' || ticket.ticket_id === 'DEMO-001';
                  return (
                    <div
                      key={ticket.id || ticket.ticket_id}
                      onClick={() => viewTicketDetails(ticket.id || ticket.ticket_id)}
                      style={{
                        backgroundColor: C.PANEL_BG,
                        border: `1px solid ${isDemoTicket ? C.BLUE : C.GRAY}`,
                        borderLeft: `4px solid ${getStatusColor(ticket.status)}`,
                        padding: '12px',
                        cursor: 'pointer',
                        transition: 'all 0.2s',
                        position: 'relative'
                      }}
                      onMouseEnter={(e) => {
                        e.currentTarget.style.borderColor = C.ORANGE;
                        e.currentTarget.style.backgroundColor = '#1a1a1a';
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.borderColor = isDemoTicket ? C.BLUE : C.GRAY;
                        e.currentTarget.style.backgroundColor = C.PANEL_BG;
                      }}
                    >
                      {isDemoTicket && (
                        <div style={{
                          position: 'absolute',
                          top: '8px',
                          right: '8px',
                          backgroundColor: C.BLUE,
                          color: 'black',
                          padding: '2px 8px',
                          fontSize: '9px',
                          fontWeight: 'bold'
                        }}>
                          {t('demo.badge')}
                        </div>
                      )}

                      <div style={{
                        display: 'grid',
                        gridTemplateColumns: '80px 1fr 120px 120px 120px 150px',
                        gap: '12px',
                        alignItems: 'center',
                        fontSize: '11px'
                      }}>
                        <div style={{ color: C.GRAY }}>
                          #{ticket.id || ticket.ticket_id}
                        </div>
                        <div style={{ color: C.WHITE, fontWeight: 'bold', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                          {ticket.subject}
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                          {getStatusIcon(ticket.status)}
                          <span style={{ color: getStatusColor(ticket.status), fontWeight: 'bold' }}>
                            {ticket.status?.toUpperCase()}
                          </span>
                        </div>
                        <div style={{
                          backgroundColor: getPriorityColor(ticket.priority),
                          color: 'black',
                          padding: '2px 8px',
                          fontSize: '9px',
                          fontWeight: 'bold',
                          textAlign: 'center'
                        }}>
                          {ticket.priority?.toUpperCase()}
                        </div>
                        <div style={{ color: C.CYAN, textAlign: 'center' }}>
                          {ticket.category?.toUpperCase()}
                        </div>
                        <div style={{ color: C.GRAY, fontSize: '10px', textAlign: 'right' }}>
                          {formatDate(ticket.created_at)}
                        </div>
                      </div>
                    </div>
                  );
                })}
              </div>
            ) : (
              <div style={{ color: C.GRAY, fontSize: '12px', textAlign: 'center', marginTop: '40px' }}>
                {t('list.noTickets')}
              </div>
            )}
          </>
        )}

        {currentView === 'create' && (
          <>
            {/* Section Header */}
            <div style={{
              color: C.ORANGE,
              fontSize: '14px',
              fontWeight: 'bold',
              marginBottom: '8px',
              letterSpacing: '0.5px'
            }}>
              {t('create.title')}
            </div>
            <div style={{ borderBottom: `1px solid ${C.GRAY}`, marginBottom: '16px' }}></div>

            {/* Form */}
            <div style={{
              backgroundColor: C.PANEL_BG,
              border: `1px solid ${C.GRAY}`,
              borderLeft: `4px solid ${C.GREEN}`,
              padding: '20px',
              maxWidth: '800px'
            }}>
              <div style={{ display: 'grid', gap: '16px' }}>
                <div>
                  <label style={{ color: C.GRAY, fontSize: '11px', display: 'block', marginBottom: '4px' }}>
                    SUBJECT *
                  </label>
                  <input
                    type="text"
                    value={newTicket.subject}
                    onChange={(e) => setNewTicket({ ...newTicket, subject: e.target.value })}
                    placeholder="Brief description of your issue"
                    style={{
                      width: '100%',
                      padding: '8px',
                      backgroundColor: C.DARK_BG,
                      border: `1px solid ${C.GRAY}`,
                      color: C.WHITE,
                      fontSize: '11px',
                      fontFamily: 'Consolas, monospace',
                      outline: 'none'
                    }}
                  />
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                  <div>
                    <label style={{ color: C.GRAY, fontSize: '11px', display: 'block', marginBottom: '4px' }}>
                      CATEGORY *
                    </label>
                    <select
                      value={newTicket.category}
                      onChange={(e) => setNewTicket({ ...newTicket, category: e.target.value })}
                      style={{
                        width: '100%',
                        padding: '8px',
                        backgroundColor: C.DARK_BG,
                        border: `1px solid ${C.GRAY}`,
                        color: C.WHITE,
                        fontSize: '11px',
                        fontFamily: 'Consolas, monospace',
                        outline: 'none',
                        cursor: 'pointer'
                      }}
                    >
                      <option value="technical">Technical Issue</option>
                      <option value="billing">Billing</option>
                      <option value="feature">Feature Request</option>
                      <option value="bug">Bug Report</option>
                      <option value="account">Account</option>
                      <option value="other">Other</option>
                    </select>
                  </div>

                  <div>
                    <label style={{ color: C.GRAY, fontSize: '11px', display: 'block', marginBottom: '4px' }}>
                      PRIORITY
                    </label>
                    <select
                      value={newTicket.priority}
                      onChange={(e) => setNewTicket({ ...newTicket, priority: e.target.value })}
                      style={{
                        width: '100%',
                        padding: '8px',
                        backgroundColor: C.DARK_BG,
                        border: `1px solid ${C.GRAY}`,
                        color: C.WHITE,
                        fontSize: '11px',
                        fontFamily: 'Consolas, monospace',
                        outline: 'none',
                        cursor: 'pointer'
                      }}
                    >
                      <option value="low">Low</option>
                      <option value="medium">Medium</option>
                      <option value="high">High</option>
                    </select>
                  </div>
                </div>

                <div>
                  <label style={{ color: C.GRAY, fontSize: '11px', display: 'block', marginBottom: '4px' }}>
                    DESCRIPTION *
                  </label>
                  <textarea
                    value={newTicket.description}
                    onChange={(e) => setNewTicket({ ...newTicket, description: e.target.value })}
                    placeholder="Provide detailed information about your issue..."
                    rows={12}
                    style={{
                      width: '100%',
                      padding: '8px',
                      backgroundColor: C.DARK_BG,
                      border: `1px solid ${C.GRAY}`,
                      color: C.WHITE,
                      fontSize: '11px',
                      fontFamily: 'Consolas, monospace',
                      outline: 'none',
                      resize: 'vertical'
                    }}
                  />
                </div>

                <button
                  onClick={createTicket}
                  disabled={loading}
                  style={{
                    padding: '10px 20px',
                    backgroundColor: loading ? C.GRAY : C.GREEN,
                    border: 'none',
                    color: 'black',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    cursor: loading ? 'not-allowed' : 'pointer',
                    width: 'fit-content'
                  }}
                >
                  {loading ? 'CREATING...' : 'CREATE TICKET'}
                </button>
              </div>
            </div>
          </>
        )}

        {currentView === 'details' && selectedTicket && (
          <>
            {/* Section Header */}
            <div style={{
              color: C.ORANGE,
              fontSize: '14px',
              fontWeight: 'bold',
              marginBottom: '8px',
              letterSpacing: '0.5px'
            }}>
              TICKET DETAILS - #{selectedTicket.id || selectedTicket.ticket_id}
            </div>
            <div style={{ borderBottom: `1px solid ${C.GRAY}`, marginBottom: '16px' }}></div>

            {(() => {
              const isDemoTicket = selectedTicket.id === 'DEMO-001' || selectedTicket.ticket_id === 'DEMO-001';
              const messages = selectedTicket.messages || [];

              return (
                <>
                  {isDemoTicket && (
                    <div style={{
                      backgroundColor: C.BLUE,
                      color: 'black',
                      padding: '8px 12px',
                      marginBottom: '12px',
                      fontSize: '11px',
                      fontWeight: 'bold',
                      textAlign: 'center'
                    }}>
                      DEMO TICKET - This shows you how the support system works. Create your own ticket to get real support!
                    </div>
                  )}

                  {/* Ticket Header */}
                  <div style={{
                    backgroundColor: C.PANEL_BG,
                    border: `1px solid ${isDemoTicket ? C.BLUE : C.GRAY}`,
                    borderLeft: `4px solid ${getStatusColor(selectedTicket.status)}`,
                    padding: '16px',
                    marginBottom: '16px'
                  }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '12px' }}>
                      <div>
                        <div style={{ color: C.WHITE, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                          {selectedTicket.subject}
                        </div>
                        <div style={{
                          display: 'grid',
                          gridTemplateColumns: 'repeat(4, auto)',
                          gap: '16px',
                          fontSize: '11px'
                        }}>
                          <div>
                            <span style={{ color: C.GRAY }}>Status: </span>
                            <span style={{ color: getStatusColor(selectedTicket.status), fontWeight: 'bold' }}>
                              {selectedTicket.status?.toUpperCase()}
                            </span>
                          </div>
                          <div>
                            <span style={{ color: C.GRAY }}>Priority: </span>
                            <span style={{ color: getPriorityColor(selectedTicket.priority), fontWeight: 'bold' }}>
                              {selectedTicket.priority?.toUpperCase()}
                            </span>
                          </div>
                          <div>
                            <span style={{ color: C.GRAY }}>Category: </span>
                            <span style={{ color: C.CYAN }}>{selectedTicket.category?.toUpperCase()}</span>
                          </div>
                          <div>
                            <span style={{ color: C.GRAY }}>Created: </span>
                            <span style={{ color: C.WHITE }}>{formatDate(selectedTicket.created_at)}</span>
                          </div>
                        </div>
                      </div>
                      <div style={{ display: 'flex', gap: '8px' }}>
                        {!isDemoTicket && selectedTicket.status !== 'closed' && (
                          <button
                            onClick={() => updateTicketStatus('closed')}
                            disabled={loading}
                            style={{
                              padding: '6px 12px',
                              backgroundColor: C.DARK_BG,
                              border: `1px solid ${C.RED}`,
                              color: C.RED,
                              fontSize: '10px',
                              fontWeight: 'bold',
                              cursor: 'pointer'
                            }}
                          >
                            CLOSE
                          </button>
                        )}
                        {!isDemoTicket && selectedTicket.status === 'closed' && (
                          <button
                            onClick={() => updateTicketStatus('open')}
                            disabled={loading}
                            style={{
                              padding: '6px 12px',
                              backgroundColor: C.DARK_BG,
                              border: `1px solid ${C.GREEN}`,
                              color: C.GREEN,
                              fontSize: '10px',
                              fontWeight: 'bold',
                              cursor: 'pointer'
                            }}
                          >
                            REOPEN
                          </button>
                        )}
                      </div>
                    </div>

                    <div style={{
                      padding: '12px',
                      backgroundColor: C.DARK_BG,
                      borderLeft: `4px solid ${C.BLUE}`,
                      color: C.WHITE,
                      fontSize: '11px',
                      lineHeight: '1.6',
                      whiteSpace: 'pre-wrap'
                    }}>
                      {selectedTicket.description}
                    </div>
                  </div>

                  {/* Messages */}
                  <div style={{
                    color: C.ORANGE,
                    fontSize: '12px',
                    fontWeight: 'bold',
                    marginBottom: '8px',
                    letterSpacing: '0.5px'
                  }}>
                    CONVERSATION ({messages.length})
                  </div>
                  <div style={{ borderBottom: `1px solid ${C.GRAY}`, marginBottom: '12px' }}></div>

                  {messages.length > 0 ? (
                    <div style={{ display: 'grid', gap: '8px', marginBottom: '16px' }}>
                      {messages.map((msg: any, idx: number) => (
                        <div
                          key={idx}
                          style={{
                            backgroundColor: C.PANEL_BG,
                            border: `1px solid ${C.GRAY}`,
                            borderLeft: `4px solid ${msg.sender_type === 'user' ? C.CYAN : C.PURPLE}`,
                            padding: '12px'
                          }}
                        >
                          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
                            <span style={{ color: msg.sender_type === 'user' ? C.CYAN : C.PURPLE, fontSize: '11px', fontWeight: 'bold' }}>
                              {msg.sender_type === 'user' ? 'YOU' : 'SUPPORT TEAM'}
                            </span>
                            <span style={{ color: C.GRAY, fontSize: '10px' }}>
                              {formatDate(msg.created_at)}
                            </span>
                          </div>
                          <div style={{ color: C.WHITE, fontSize: '11px', lineHeight: '1.6' }}>
                            {msg.message}
                          </div>
                        </div>
                      ))}
                    </div>
                  ) : (
                    <div style={{ color: C.GRAY, fontSize: '11px', textAlign: 'center', padding: '20px' }}>
                      No messages yet
                    </div>
                  )}

                  {/* Add Message */}
                  {selectedTicket.status !== 'closed' && !isDemoTicket && (
                    <div style={{
                      backgroundColor: C.PANEL_BG,
                      border: `1px solid ${C.GRAY}`,
                      borderLeft: `4px solid ${C.GREEN}`,
                      padding: '16px'
                    }}>
                      <div style={{ color: C.GREEN, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
                        ADD MESSAGE
                      </div>
                      <textarea
                        value={newMessage}
                        onChange={(e) => setNewMessage(e.target.value)}
                        placeholder="Type your message here..."
                        rows={4}
                        style={{
                          width: '100%',
                          padding: '8px',
                          backgroundColor: C.DARK_BG,
                          border: `1px solid ${C.GRAY}`,
                          color: C.WHITE,
                          fontSize: '11px',
                          fontFamily: 'Consolas, monospace',
                          outline: 'none',
                          resize: 'vertical',
                          marginBottom: '12px'
                        }}
                      />
                      <button
                        onClick={addMessage}
                        disabled={loading || !newMessage.trim()}
                        style={{
                          padding: '8px 16px',
                          backgroundColor: loading || !newMessage.trim() ? C.GRAY : C.GREEN,
                          border: 'none',
                          color: 'black',
                          fontSize: '11px',
                          fontWeight: 'bold',
                          cursor: loading || !newMessage.trim() ? 'not-allowed' : 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px'
                        }}
                      >
                        <Send size={12} />
                        {loading ? 'SENDING...' : 'SEND MESSAGE'}
                      </button>
                    </div>
                  )}

                  {isDemoTicket && (
                    <div style={{
                      backgroundColor: C.PANEL_BG,
                      border: `1px solid ${C.BLUE}`,
                      borderLeft: `4px solid ${C.BLUE}`,
                      padding: '16px'
                    }}>
                      <div style={{ color: C.BLUE, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
                        DEMO TICKET - READ ONLY
                      </div>
                      <div style={{ color: C.GRAY, fontSize: '10px', lineHeight: '1.6' }}>
                        This is a demonstration ticket. You cannot add messages to demo tickets.
                        <br /><br />
                        To create a real support ticket and communicate with our team:
                        <br />
                        1. Click "BACK TO LIST"
                        <br />
                        2. Click "+ CREATE TICKET"
                        <br />
                        3. Fill in your issue details
                        <br />
                        4. Submit and receive support from our team
                      </div>
                    </div>
                  )}
                </>
              );
            })()}
          </>
        )}
      </div>
      </ErrorBoundary>

      <TabFooter
        tabName="SUPPORT CENTER"
        leftInfo={[
          { label: `View: ${currentView.toUpperCase()}`, color: C.GRAY },
          { label: `Tickets: ${tickets.length}`, color: C.GRAY },
          { label: `User: ${session.user_info?.email || 'Guest'}`, color: C.GRAY },
        ]}
        statusInfo={`Response Time: < 24 hours | 24/7 Customer Service | ${loading ? 'UPDATING' : 'READY'}`}
        backgroundColor={C.PANEL_BG}
        borderColor={C.GRAY}
      />
    </div>
  );
};

export default SupportTicketTab;
