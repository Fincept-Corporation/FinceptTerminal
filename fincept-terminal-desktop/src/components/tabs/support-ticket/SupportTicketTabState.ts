/**
 * SupportTicketTab - State machine, types, and constants
 */

// ══════════════════════════════════════════════════════════════
// STATE MACHINE & TYPES
// ══════════════════════════════════════════════════════════════
export type SupportView = 'list' | 'create' | 'details';
export type SupportStatus = 'idle' | 'loading' | 'ready' | 'error';

export interface SupportState {
  status: SupportStatus;
  currentView: SupportView;
  tickets: any[];
  selectedTicket: any | null;
  stats: { total_tickets: number; open_tickets: number; resolved_tickets: number };
  error: string | null;
}

export type SupportAction =
  | { type: 'SET_LOADING' }
  | { type: 'SET_READY' }
  | { type: 'SET_ERROR'; error: string }
  | { type: 'SET_VIEW'; view: SupportView }
  | { type: 'SET_TICKETS'; tickets: any[] }
  | { type: 'SET_SELECTED_TICKET'; ticket: any | null }
  | { type: 'SET_STATS'; stats: { total_tickets: number; open_tickets: number; resolved_tickets: number } }
  | { type: 'RESET_ERROR' };

// Demo ticket to show users how the system works
export const DEMO_TICKET = {
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

export const initialState: SupportState = {
  status: 'idle',
  currentView: 'list',
  tickets: [DEMO_TICKET],
  selectedTicket: null,
  stats: { total_tickets: 1, open_tickets: 1, resolved_tickets: 0 },
  error: null
};

export function supportReducer(state: SupportState, action: SupportAction): SupportState {
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

export const API_TIMEOUT_MS = 30000;
