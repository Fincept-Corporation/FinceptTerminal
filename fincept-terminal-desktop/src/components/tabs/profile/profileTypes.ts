/**
 * Profile Tab — Types, State & Reducer
 *
 * Extracted from ProfileTab.tsx.
 */

// ─── Types (matched to actual API responses) ─────────────────────────────────
// GET /cashfree/subscription → { success, data: { user_id, account_type, credit_balance, ... } }
export interface AccountSubscriptionData {
  user_id: number;
  account_type: string;
  credit_balance: number;
  credits_expire_at: string | null;
  support_type: string;
  last_credit_purchase_at: string | null;
  created_at: string;
}

// GET /payment/payments → { success, data: { payments: [...], pagination: {...} } }
export interface PaymentHistoryItem {
  payment_uuid: string;
  payment_gateway: string;
  amount_usd: number;
  currency: string;
  status: string;
  plan_name: string;
  credits_purchased: number | null;
  payment_method: string | null;
  created_at: string;
  completed_at: string | null;
}

// GET /user/usage → { success, data: { account, period_days, summary, daily_usage, endpoint_breakdown, recent_requests, ... } }
export interface UsageData {
  account?: {
    credit_balance: number;
    account_type: string;
    credits_expire_at: string | null;
    rate_limit_per_hour: number;
    support_type: string;
  };
  period_days?: number;
  summary?: {
    total_requests: number;
    total_credits_used: number;
    avg_credits_per_request: number;
    avg_response_time_ms: number;
    first_request_at: string;
    last_request_at: string;
  };
  daily_usage?: Array<{
    date: string;
    request_count: number;
    credits_used: number;
  }>;
  endpoint_breakdown?: Array<{
    endpoint: string;
    request_count: number;
    credits_used: number;
    avg_response_time_ms: number;
  }>;
  recent_requests?: Array<{
    timestamp: string;
    endpoint: string;
    method: string;
    status_code: number;
    credits_used: number;
    response_time_ms: number;
  }>;
  pagination?: {
    page: number;
    page_size: number;
    total_records: number;
    total_pages: number;
  };
}

export type Section = 'overview' | 'usage' | 'security' | 'billing' | 'support';

// ─── State Machine ─────────────────────────────────────────────────────────────
export type ProfileStatus = 'idle' | 'loading' | 'ready' | 'error';

export interface ProfileState {
  status: ProfileStatus;
  activeSection: Section;
  usageData: UsageData | null;
  subscriptionData: AccountSubscriptionData | null;
  paymentHistory: PaymentHistoryItem[];
  showApiKey: boolean;
  showLogoutConfirm: boolean;
  showRegenerateConfirm: boolean;
  error: string | null;
}

export type ProfileAction =
  | { type: 'START_LOADING' }
  | { type: 'SET_READY' }
  | { type: 'SET_ERROR'; error: string }
  | { type: 'CLEAR_ERROR' }
  | { type: 'SET_SECTION'; section: Section }
  | { type: 'SET_USAGE_DATA'; data: UsageData }
  | { type: 'SET_SUBSCRIPTION_DATA'; data: AccountSubscriptionData }
  | { type: 'SET_PAYMENT_HISTORY'; payments: PaymentHistoryItem[] }
  | { type: 'TOGGLE_API_KEY'; show: boolean }
  | { type: 'TOGGLE_LOGOUT_CONFIRM'; show: boolean }
  | { type: 'TOGGLE_REGENERATE_CONFIRM'; show: boolean };

export const initialState: ProfileState = {
  status: 'idle',
  activeSection: 'overview',
  usageData: null,
  subscriptionData: null,
  paymentHistory: [],
  showApiKey: false,
  showLogoutConfirm: false,
  showRegenerateConfirm: false,
  error: null,
};

export function profileReducer(state: ProfileState, action: ProfileAction): ProfileState {
  switch (action.type) {
    case 'START_LOADING':
      return { ...state, status: 'loading', error: null };
    case 'SET_READY':
      return { ...state, status: 'ready' };
    case 'SET_ERROR':
      return { ...state, status: 'error', error: action.error };
    case 'CLEAR_ERROR':
      return { ...state, error: null };
    case 'SET_SECTION':
      return { ...state, activeSection: action.section };
    case 'SET_USAGE_DATA':
      return { ...state, usageData: action.data, status: 'ready' };
    case 'SET_SUBSCRIPTION_DATA':
      return { ...state, subscriptionData: action.data };
    case 'SET_PAYMENT_HISTORY':
      return { ...state, paymentHistory: action.payments, status: 'ready' };
    case 'TOGGLE_API_KEY':
      return { ...state, showApiKey: action.show };
    case 'TOGGLE_LOGOUT_CONFIRM':
      return { ...state, showLogoutConfirm: action.show };
    case 'TOGGLE_REGENERATE_CONFIRM':
      return { ...state, showRegenerateConfirm: action.show };
    default:
      return state;
  }
}

// ─── Profile completeness helper ─────────────────────────────────────────────
export function getProfileCompleteness(session: any): { missing: string[]; percent: number } {
  if (session?.user_type !== 'registered') return { missing: [], percent: 100 };
  const info = session?.user_info || {};
  const checks: { key: string; label: string }[] = [
    { key: 'username', label: 'Username' },
    { key: 'email', label: 'Email' },
    { key: 'phone', label: 'Phone number' },
    { key: 'country', label: 'Country' },
    { key: 'country_code', label: 'Country code' },
  ];
  const missing = checks.filter(c => !info[c.key]).map(c => c.label);
  const percent = Math.round(((checks.length - missing.length) / checks.length) * 100);
  return { missing, percent };
}
