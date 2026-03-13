// File: src/components/tabs/profile/ProfileTab.tsx
// Fincept Terminal – Profile & Account Management (FINCEPT Design System)
// Uses: State machine, cleanup, timeout, error boundaries, validation

import React, { useState, useReducer, useEffect, useCallback, useRef } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { UserApiService } from '@/services/auth/userApi';
import { TabHeader } from '@/components/common/TabHeader';
import { TabFooter } from '@/components/common/TabFooter';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { withTimeout } from '@/services/core/apiUtils';
import { validateEmail, sanitizeInput } from '@/services/core/validators';
import {
  User, CreditCard, Activity, Shield, Key, RefreshCw,
  LogOut, Zap, CheckCircle, AlertCircle, Eye, EyeOff, BarChart,
  MessageSquarePlus, Mail, Newspaper, Star, Send, Check, HeadphonesIcon,
  FileText, Package, Calendar,
} from 'lucide-react';
import {
  submitContactForm,
  submitFeedback,
  subscribeToNewsletter,
  type ContactFormData,
  type FeedbackFormData,
} from '@/services/support/supportApi';
import {
  type AccountSubscriptionData,
  type PaymentHistoryItem,
  type UsageData,
  type Section,
  type ProfileState,
  type ProfileAction,
  initialState,
  profileReducer,
  getProfileCompleteness,
} from './profileTypes';
import {
  F, FONT,
  Panel, DataRow, StatBox, SecurityRow, EnabledBadge, Badge,
  PrimaryBtn, SecondaryBtn, ActionBtn, Label, InputField,
  SubmitBtn, StatusBadge, LoadingState, EmptyState, ConfirmModal,
} from './profileUiComponents';

// ─── Constants ─────────────────────────────────────────────────────────────────
const API_TIMEOUT_MS = 30000;

// ─── Main Component ───────────────────────────────────────────────────────────
const ProfileTab: React.FC = () => {
  const { session, logout, refreshUserData, updateApiKey, isLoggingOut } = useAuth();
  const [state, dispatch] = useReducer(profileReducer, initialState);
  const mountedRef = useRef(true);
  const fetchIdRef = useRef(0);
  const refreshIntervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  const fetchSectionData = useCallback(async () => {
    if (!session?.api_key) return;
    if (state.status === 'loading') return; // Prevent duplicate fetches

    const currentFetchId = ++fetchIdRef.current;
    dispatch({ type: 'START_LOADING' });

    try {
      if (state.activeSection === 'usage') {
        const result = session.user_type === 'guest'
          ? { success: true, data: { requests_today: session.requests_today || 0, credit_balance: session.credit_balance || 0, expires_at: session.expires_at }, error: undefined, status_code: 200 }
          : await withTimeout(UserApiService.getUserUsage(session.api_key, { days: 30 }), API_TIMEOUT_MS, 'Usage fetch timeout');

        if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

        console.log('[Profile] Usage API result:', { success: result.success, status: result.status_code, hasData: !!result.data, error: result.error });

        if (result.success && result.data) {
          const raw = result.data as any;
          const usagePayload = raw?.data || raw;
          console.log('[Profile] Usage data keys:', Object.keys(usagePayload || {}));
          dispatch({ type: 'SET_USAGE_DATA', data: usagePayload });
        } else {
          dispatch({ type: 'SET_ERROR', error: result.error || 'Failed to load usage data' });
        }
      } else if (state.activeSection === 'billing') {
        const [subResult, historyResult] = await Promise.all([
          withTimeout(UserApiService.getUserSubscription(session.api_key), API_TIMEOUT_MS, 'Subscription fetch timeout'),
          withTimeout(UserApiService.getPaymentHistory(session.api_key, 1, 20), API_TIMEOUT_MS, 'Payment history fetch timeout'),
        ]);

        if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

        if (subResult.success && subResult.data) {
          const raw = subResult.data as any;
          const d = raw?.data || raw;
          dispatch({ type: 'SET_SUBSCRIPTION_DATA', data: d });
        }
        if (historyResult.success && historyResult.data) {
          const raw = historyResult.data as any;
          const payments = raw?.data?.payments || raw?.payments || [];
          dispatch({ type: 'SET_PAYMENT_HISTORY', payments });
        }
        if (!subResult.success && !historyResult.success) {
          dispatch({ type: 'SET_ERROR', error: 'Failed to load billing data' });
        } else {
          dispatch({ type: 'SET_READY' });
        }
      } else {
        dispatch({ type: 'SET_READY' });
      }
    } catch (err) {
      if (mountedRef.current && currentFetchId === fetchIdRef.current) {
        dispatch({ type: 'SET_ERROR', error: err instanceof Error ? err.message : 'Network error' });
      }
    }
  }, [state.activeSection, state.status, session]);

  // Refresh credits/usage from session on mount and every 30 seconds
  const refreshCreditsAndUsage = useCallback(async () => {
    if (!mountedRef.current || !session?.api_key) return;
    await refreshUserData();
    // If on usage section, also re-fetch usage data
    if (state.activeSection === 'usage' && state.status !== 'loading') {
      fetchSectionData();
    }
  }, [refreshUserData, session?.api_key, state.activeSection, state.status, fetchSectionData]);

  // On mount: immediate refresh + start 30s interval
  useEffect(() => {
    refreshCreditsAndUsage();
    refreshIntervalRef.current = setInterval(refreshCreditsAndUsage, 30000);
    return () => {
      if (refreshIntervalRef.current) {
        clearInterval(refreshIntervalRef.current);
        refreshIntervalRef.current = null;
      }
    };
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  useEffect(() => {
    fetchSectionData();
  }, [state.activeSection]); // Only re-fetch when section changes

  const confirmLogout = useCallback(async () => {
    dispatch({ type: 'TOGGLE_LOGOUT_CONFIRM', show: false });
    // The logout function in AuthContext handles everything including state transitions
    // No need to do anything else - React will re-render when session becomes null
    await logout();
  }, [logout]);

  const confirmRegenerateKey = useCallback(async () => {
    if (!session?.api_key) return;
    dispatch({ type: 'TOGGLE_REGENERATE_CONFIRM', show: false });
    dispatch({ type: 'START_LOADING' });

    try {
      const result = await withTimeout(
        UserApiService.regenerateApiKey(session.api_key),
        API_TIMEOUT_MS,
        'Regenerate key timeout'
      );

      if (!mountedRef.current) return;

      if (result.success && result.data) {
        // Extract new API key from response
        const apiData = (result.data as any)?.data || result.data;
        const newApiKey = apiData?.api_key || apiData?.new_api_key;

        if (newApiKey) {
          // Update the API key in AuthContext - this propagates to ALL components
          await updateApiKey(newApiKey);
          dispatch({ type: 'SET_READY' });
          // Also refresh user data to get latest profile info
          await refreshUserData();
        } else {
          dispatch({ type: 'SET_ERROR', error: 'No new API key received' });
        }
      } else {
        dispatch({ type: 'SET_ERROR', error: result.error || 'Failed to regenerate API key' });
      }
    } catch (err) {
      if (mountedRef.current) {
        dispatch({ type: 'SET_ERROR', error: err instanceof Error ? err.message : 'Failed to regenerate API key' });
      }
    }
  }, [session, refreshUserData, updateApiKey]);

  // Destructure state for easier access
  const { status, activeSection, usageData, subscriptionData, paymentHistory, showApiKey, showLogoutConfirm, showRegenerateConfirm, error } = state;
  const loading = status === 'loading';

  const accountType = session?.user_info?.account_type || 'free';
  const credits = session?.user_info?.credit_balance || 0;
  const username = session?.user_info?.username || session?.user_info?.email || 'User';
  const email = session?.user_info?.email || 'N/A';

  const sections: { id: Section; label: string; icon: React.ElementType }[] = [
    { id: 'overview', label: 'OVERVIEW', icon: User },
    { id: 'usage', label: 'USAGE', icon: Activity },
    { id: 'security', label: 'SECURITY', icon: Shield },
    { id: 'billing', label: 'BILLING', icon: CreditCard },
    { id: 'support', label: 'SUPPORT', icon: HeadphonesIcon },
  ];

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: F.DARK_BG, fontFamily: FONT }}>
      <TabHeader
        title="PROFILE & ACCOUNT"
        subtitle={`${username} | ${email}`}
        icon={<User size={20} />}
        actions={
          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
            <Badge label="CREDITS" value={credits.toLocaleString()} color={F.CYAN} />
            <Badge label="PLAN" value={accountType.toUpperCase()} color={F.ORANGE} />
            <SecondaryBtn icon={<RefreshCw size={10} />} label="REFRESH" onClick={fetchSectionData} />
          </div>
        }
      />

      {/* Tab Navigation */}
      <div style={{ borderBottom: `1px solid ${F.BORDER}`, backgroundColor: F.HEADER_BG, display: 'flex', flexShrink: 0 }}>
        {sections.map(s => (
          <button
            key={s.id}
            onClick={() => dispatch({ type: 'SET_SECTION', section: s.id })}
            style={{
              padding: '6px 12px',
              backgroundColor: activeSection === s.id ? F.ORANGE : 'transparent',
              color: activeSection === s.id ? F.DARK_BG : F.GRAY,
              border: 'none',
              fontSize: '9px',
              fontWeight: 700,
              fontFamily: FONT,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              transition: 'all 0.2s',
            }}
          >
            {React.createElement(s.icon, { size: 12 })}
            {s.label}
          </button>
        ))}
      </div>

      {/* Error Banner */}
      {error && (
        <div style={{
          padding: '8px 16px',
          backgroundColor: `${F.RED}20`,
          borderBottom: `1px solid ${F.RED}`,
          color: F.RED,
          fontSize: '9px',
          fontFamily: FONT,
          fontWeight: 700,
        }}>
          {error}
        </div>
      )}

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px', minHeight: 0, display: 'flex', flexDirection: 'column' }}>
        <ErrorBoundary name="ProfileOverview" variant="minimal">
          {activeSection === 'overview' && (
            <OverviewSection session={session} onLogout={() => dispatch({ type: 'TOGGLE_LOGOUT_CONFIRM', show: true })} isLoggingOut={isLoggingOut} />
          )}
        </ErrorBoundary>
        <ErrorBoundary name="ProfileUsage" variant="minimal">
          {activeSection === 'usage' && (
            <UsageSection usageData={usageData} loading={loading} session={session} />
          )}
        </ErrorBoundary>
        <ErrorBoundary name="ProfileSecurity" variant="minimal">
          {activeSection === 'security' && (
            <SecuritySection
              session={session}
              onRegenerateKey={() => dispatch({ type: 'TOGGLE_REGENERATE_CONFIRM', show: true })}
              loading={loading}
              showApiKey={showApiKey}
              setShowApiKey={(show) => dispatch({ type: 'TOGGLE_API_KEY', show })}
            />
          )}
        </ErrorBoundary>
        <ErrorBoundary name="ProfileBilling" variant="minimal">
          {activeSection === 'billing' && (
            <BillingSection
              subscriptionData={subscriptionData}
              paymentHistory={paymentHistory}
              loading={loading}
            />
          )}
        </ErrorBoundary>
        <ErrorBoundary name="ProfileSupport" variant="minimal">
          {activeSection === 'support' && <SupportSection />}
        </ErrorBoundary>
      </div>

      <TabFooter
        tabName="PROFILE"
        leftInfo={[
          { label: 'USER', value: username, color: F.CYAN },
          { label: 'TYPE', value: session?.user_type?.toUpperCase(), color: F.WHITE },
        ]}
        statusInfo={`Updated: ${new Date().toLocaleTimeString()}`}
      />

      {/* Logout Modal */}
      {showLogoutConfirm && (
        <ConfirmModal
          icon={<LogOut size={16} />}
          title="CONFIRM LOGOUT"
          message="Are you sure you want to logout from Fincept Terminal?"
          confirmLabel="LOGOUT"
          confirmColor={F.RED}
          onCancel={() => dispatch({ type: 'TOGGLE_LOGOUT_CONFIRM', show: false })}
          onConfirm={confirmLogout}
        />
      )}

      {/* Regenerate Key Modal */}
      {showRegenerateConfirm && (
        <ConfirmModal
          icon={<Key size={16} />}
          title="REGENERATE API KEY"
          message="Your current API key will be invalidated. Are you sure you want to continue?"
          confirmLabel={loading ? 'REGENERATING...' : 'CONFIRM'}
          confirmColor={F.ORANGE}
          disabled={loading}
          onCancel={() => dispatch({ type: 'TOGGLE_REGENERATE_CONFIRM', show: false })}
          onConfirm={confirmRegenerateKey}
        />
      )}
    </div>
  );
};

// ─── Overview Section ─────────────────────────────────────────────────────────
const OverviewSection: React.FC<{ session: any; onLogout: () => void; isLoggingOut?: boolean }> = ({ session, onLogout, isLoggingOut }) => {
  const { refreshUserData } = useAuth();
  const accountType = session?.user_info?.account_type || 'free';
  const { missing, percent } = getProfileCompleteness(session);

  // Edit profile state
  const [editing, setEditing] = useState(false);
  const [editUsername, setEditUsername] = useState(session?.user_info?.username || '');
  const [editPhone, setEditPhone] = useState(session?.user_info?.phone || '');
  const [editCountry, setEditCountry] = useState(session?.user_info?.country || '');
  const [editCountryCode, setEditCountryCode] = useState(session?.user_info?.country_code || '');
  const [saveLoading, setSaveLoading] = useState(false);
  const [saveStatus, setSaveStatus] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  const handleSaveProfile = async () => {
    if (!session?.api_key) return;
    setSaveLoading(true);
    setSaveStatus(null);
    try {
      const result = await withTimeout(
        UserApiService.updateUserProfile(session.api_key, {
          username: editUsername.trim() || undefined,
          phone: editPhone.trim() || null,
          country: editCountry.trim() || null,
          country_code: editCountryCode.trim() || null,
        }),
        API_TIMEOUT_MS,
        'Profile update timeout'
      );
      setSaveLoading(false);
      if (result.success) {
        setSaveStatus({ type: 'success', text: 'Profile updated successfully!' });
        await refreshUserData();
        setTimeout(() => { setSaveStatus(null); setEditing(false); }, 2000);
      } else {
        setSaveStatus({ type: 'error', text: result.error || 'Failed to update profile' });
      }
    } catch (err) {
      setSaveLoading(false);
      setSaveStatus({ type: 'error', text: err instanceof Error ? err.message : 'Network error' });
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {/* Profile Completeness Banner */}
      {session?.user_type === 'registered' && missing.length > 0 && (
        <div style={{
          padding: '12px 16px',
          backgroundColor: `${F.YELLOW}15`,
          border: `1px solid ${F.YELLOW}`,
          borderRadius: '2px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: '12px',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flex: 1 }}>
            <AlertCircle size={14} color={F.YELLOW} />
            <div>
              <div style={{ fontSize: '9px', fontWeight: 700, color: F.YELLOW, letterSpacing: '0.5px', marginBottom: '2px' }}>
                PROFILE INCOMPLETE ({percent}%) — Required for payment processing
              </div>
              <div style={{ fontSize: '9px', color: F.GRAY }}>
                Missing: {missing.join(', ')}
              </div>
            </div>
          </div>
          {/* Completeness bar */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <div style={{ width: '80px', height: '4px', backgroundColor: F.BORDER, borderRadius: '2px' }}>
              <div style={{ width: `${percent}%`, height: '100%', backgroundColor: percent === 100 ? F.GREEN : F.YELLOW, borderRadius: '2px', transition: 'width 0.3s' }} />
            </div>
            <SecondaryBtn label="COMPLETE NOW" onClick={() => setEditing(true)} />
          </div>
        </div>
      )}

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
        <Panel title="ACCOUNT INFORMATION" icon={<User size={14} />}>
          <DataRow label="USERNAME" value={session?.user_info?.username || 'N/A'} />
          <DataRow label="EMAIL" value={session?.user_info?.email || 'N/A'} />
          <DataRow label="USER TYPE" value={session?.user_type?.toUpperCase() || 'N/A'} />
          <DataRow label="ACCOUNT TYPE" value={accountType.toUpperCase()} valueColor={F.ORANGE} />
          <DataRow label="PHONE" value={session?.user_info?.phone || '—'} valueColor={session?.user_info?.phone ? F.WHITE : F.MUTED} />
          <DataRow label="COUNTRY" value={session?.user_info?.country || '—'} valueColor={session?.user_info?.country ? F.WHITE : F.MUTED} />
          <DataRow label="COUNTRY CODE" value={session?.user_info?.country_code || '—'} valueColor={session?.user_info?.country_code ? F.WHITE : F.MUTED} />
          <DataRow
            label="EMAIL VERIFIED"
            value={session?.user_info?.is_verified ? 'YES' : 'NO'}
            valueColor={session?.user_info?.is_verified ? F.GREEN : F.RED}
          />
          <DataRow
            label="2FA ENABLED"
            value={session?.user_info?.mfa_enabled ? 'YES' : 'NO'}
            valueColor={session?.user_info?.mfa_enabled ? F.GREEN : F.RED}
          />
          <div style={{ paddingTop: '10px' }}>
            <SecondaryBtn icon={<User size={10} />} label="EDIT PROFILE" onClick={() => setEditing(true)} />
          </div>
        </Panel>

        <Panel title="CREDITS & BALANCE" icon={<Zap size={14} />}>
          <div style={{ padding: '20px 0', textAlign: 'center', borderBottom: `1px solid ${F.BORDER}` }}>
            <div style={{ fontSize: '36px', color: F.CYAN, fontWeight: 700, marginBottom: '8px', fontFamily: FONT }}>
              {(session?.user_info?.credit_balance || 0).toLocaleString()}
            </div>
            <div style={{ fontSize: '9px', color: F.GRAY, letterSpacing: '0.5px', fontWeight: 700 }}>AVAILABLE CREDITS</div>
          </div>
          <div style={{ padding: '12px 0' }}>
            <DataRow label="PLAN" value={accountType.toUpperCase()} valueColor={F.ORANGE} />
            {session?.user_type === 'guest' && (
              <>
                <DataRow label="CREDITS" value={session?.credit_balance?.toString() || '0'} />
                <DataRow label="REQUESTS TODAY" value={session?.requests_today?.toString() || '0'} />
              </>
            )}
          </div>
        </Panel>

        <div style={{ gridColumn: '1 / -1' }}>
          <Panel title="QUICK ACTIONS" icon={<Activity size={14} />}>
            <div style={{ display: 'flex', gap: '12px', padding: '8px 0' }}>
              <SecondaryBtn icon={<User size={10} />} label="EDIT PROFILE" onClick={() => setEditing(true)} />
              <ActionBtn label={isLoggingOut ? "LOGGING OUT..." : "LOGOUT"} onClick={onLogout} danger disabled={isLoggingOut} />
            </div>
          </Panel>
        </div>
      </div>

      {/* Edit Profile Modal */}
      {editing && (
        <div style={{
          position: 'fixed', top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.85)', display: 'flex',
          alignItems: 'center', justifyContent: 'center', zIndex: 1000,
        }}>
          <div style={{
            backgroundColor: F.PANEL_BG, border: `1px solid ${F.ORANGE}`,
            borderRadius: '2px', padding: '24px', width: '480px', maxWidth: '95vw',
          }}>
            <div style={{ fontSize: '11px', fontWeight: 700, color: F.ORANGE, marginBottom: '20px', display: 'flex', alignItems: 'center', gap: '8px', fontFamily: FONT, letterSpacing: '0.5px' }}>
              <User size={14} />
              EDIT PROFILE
            </div>

            {saveStatus && <StatusBadge type={saveStatus.type} text={saveStatus.text} />}

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <div style={{ gridColumn: '1 / -1' }}>
                <InputField label="USERNAME" value={editUsername} onChange={setEditUsername} placeholder="Your username" />
              </div>
              <div style={{ gridColumn: '1 / -1' }}>
                <InputField
                  label="PHONE NUMBER (with country dial code, e.g. +91 9876543210)"
                  value={editPhone}
                  onChange={setEditPhone}
                  placeholder="+1 5551234567"
                  type="tel"
                />
                <div style={{ fontSize: '9px', color: F.MUTED, marginTop: '-8px', marginBottom: '12px' }}>
                  Required for payment processing (Cashfree). Include country dial code.
                </div>
              </div>
              <InputField label="COUNTRY (e.g. India)" value={editCountry} onChange={setEditCountry} placeholder="India" />
              <InputField label="COUNTRY CODE (e.g. IN)" value={editCountryCode} onChange={setEditCountryCode} placeholder="IN" />
            </div>

            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end', marginTop: '8px' }}>
              <SecondaryBtn label="CANCEL" onClick={() => { setEditing(false); setSaveStatus(null); }} />
              <PrimaryBtn
                label={saveLoading ? 'SAVING...' : 'SAVE CHANGES'}
                onClick={handleSaveProfile}
                disabled={saveLoading}
              />
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

// ─── Usage Section ────────────────────────────────────────────────────────────
const UsageSection: React.FC<{ usageData: UsageData | null; loading: boolean; session: any }> = ({ usageData, loading, session }) => {
  const credits = session?.user_type === 'guest'
    ? (session?.credit_balance || 0)
    : (usageData?.account?.credit_balance ?? session?.user_info?.credit_balance ?? 0);
  const requestsToday = session?.requests_today || 0;
  const accountType = usageData?.account?.account_type || session?.user_info?.account_type || 'free';
  const rateLimit = usageData?.account?.rate_limit_per_hour || 0;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {/* Account & Credit Balance */}
      <Panel title="ACCOUNT & CREDITS" icon={<Zap size={14} />}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '16px', padding: '12px 0' }}>
          <StatBox label="CREDIT BALANCE" value={credits} color={F.CYAN} />
          <StatBox label="ACCOUNT TYPE" value={accountType.toUpperCase()} />
          <StatBox label="RATE LIMIT / HR" value={rateLimit} />
        </div>
        {usageData?.account?.credits_expire_at && (
          <div style={{ fontSize: '9px', color: F.MUTED, textAlign: 'center', marginTop: '4px' }}>
            Credits expire: {new Date(usageData.account.credits_expire_at).toLocaleDateString()}
          </div>
        )}
      </Panel>

      {session?.user_type === 'guest' && (
        <Panel title="GUEST SESSION USAGE" icon={<Activity size={14} />}>
          <div style={{ padding: '12px 0' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', fontSize: '10px' }}>
              <span style={{ color: F.GRAY, fontWeight: 700, fontSize: '9px', letterSpacing: '0.5px' }}>REQUESTS TODAY</span>
              <span style={{ color: F.WHITE, fontWeight: 700, fontFamily: FONT }}>{requestsToday}</span>
            </div>
          </div>
        </Panel>
      )}

      {loading ? (
        <LoadingState message="Loading usage data..." />
      ) : usageData ? (
        <>
          {/* Summary Statistics */}
          <Panel title={`USAGE SUMMARY — LAST ${usageData.period_days || 7} DAYS`} icon={<BarChart size={14} />}>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '16px', padding: '12px 0' }}>
              <StatBox label="TOTAL REQUESTS" value={usageData.summary?.total_requests || 0} />
              <StatBox label="CREDITS USED" value={usageData.summary?.total_credits_used || 0} color={F.YELLOW} />
              <StatBox label="AVG CREDITS/REQ" value={usageData.summary?.avg_credits_per_request?.toFixed(2) || '0'} />
              <StatBox label="AVG RESPONSE (ms)" value={usageData.summary?.avg_response_time_ms?.toFixed(0) || '0'} />
            </div>
            {usageData.summary?.first_request_at && usageData.summary?.last_request_at && (
              <div style={{ fontSize: '9px', color: F.MUTED, textAlign: 'center', marginTop: '4px' }}>
                {new Date(usageData.summary.first_request_at).toLocaleDateString()} — {new Date(usageData.summary.last_request_at).toLocaleDateString()}
              </div>
            )}
          </Panel>

          {/* Daily Usage Chart */}
          {usageData.daily_usage && usageData.daily_usage.length > 0 && (
            <Panel title="DAILY USAGE" icon={<Activity size={14} />}>
              <div style={{ padding: '8px 0' }}>
                {/* Simple bar chart */}
                <div style={{ display: 'flex', alignItems: 'flex-end', gap: '2px', height: '80px', padding: '0 4px' }}>
                  {(() => {
                    const maxReqs = Math.max(...usageData.daily_usage!.map(d => d.request_count), 1);
                    return usageData.daily_usage!.map((day) => (
                      <div key={day.date} style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '2px' }}>
                        <div
                          title={`${day.date}: ${day.request_count} requests, ${day.credits_used} credits`}
                          style={{
                            width: '100%',
                            height: `${Math.max((day.request_count / maxReqs) * 70, 2)}px`,
                            backgroundColor: day.credits_used > 0 ? F.CYAN : F.MUTED,
                            borderRadius: '1px 1px 0 0',
                            opacity: 0.8,
                          }}
                        />
                      </div>
                    ));
                  })()}
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', padding: '4px 4px 0', fontSize: '8px', color: F.MUTED }}>
                  <span>{usageData.daily_usage[0]?.date?.slice(5)}</span>
                  <span>{usageData.daily_usage[usageData.daily_usage.length - 1]?.date?.slice(5)}</span>
                </div>
              </div>
              {/* Daily totals table */}
              <table style={{ width: '100%', fontSize: '10px', fontFamily: FONT, borderCollapse: 'collapse', marginTop: '8px' }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${F.BORDER}` }}>
                    {['DATE', 'REQUESTS', 'CREDITS'].map(h => (
                      <th key={h} style={{
                        padding: '6px 8px',
                        textAlign: h === 'DATE' ? 'left' : 'right',
                        color: F.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
                      }}>{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {usageData.daily_usage.slice().reverse().slice(0, 10).map((day) => (
                    <tr key={day.date} style={{ borderBottom: `1px solid ${F.BORDER}` }}>
                      <td style={{ padding: '6px 8px', color: F.WHITE, fontSize: '9px' }}>{day.date}</td>
                      <td style={{ padding: '6px 8px', textAlign: 'right', color: F.CYAN, fontWeight: 700 }}>{day.request_count.toLocaleString()}</td>
                      <td style={{ padding: '6px 8px', textAlign: 'right', color: day.credits_used > 0 ? F.YELLOW : F.MUTED, fontWeight: 700 }}>{day.credits_used.toLocaleString()}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </Panel>
          )}

          {/* Top Endpoints Breakdown */}
          {usageData.endpoint_breakdown && usageData.endpoint_breakdown.length > 0 && (
            <Panel title="TOP ENDPOINTS" icon={<Activity size={14} />}>
              <table style={{ width: '100%', fontSize: '10px', fontFamily: FONT, borderCollapse: 'collapse' }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${F.BORDER}` }}>
                    {['ENDPOINT', 'REQUESTS', 'CREDITS', 'AVG MS'].map(h => (
                      <th key={h} style={{
                        padding: '6px 8px',
                        textAlign: h === 'ENDPOINT' ? 'left' : 'right',
                        color: F.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
                      }}>{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {usageData.endpoint_breakdown
                    .slice(0, 15)
                    .map((ep) => (
                      <tr key={ep.endpoint} style={{ borderBottom: `1px solid ${F.BORDER}` }}>
                        <td style={{ padding: '6px 8px', color: F.WHITE, fontSize: '9px', maxWidth: '200px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{ep.endpoint}</td>
                        <td style={{ padding: '6px 8px', textAlign: 'right', color: F.CYAN, fontWeight: 700 }}>{ep.request_count.toLocaleString()}</td>
                        <td style={{ padding: '6px 8px', textAlign: 'right', color: ep.credits_used > 0 ? F.YELLOW : F.MUTED, fontWeight: 700 }}>{ep.credits_used.toLocaleString()}</td>
                        <td style={{ padding: '6px 8px', textAlign: 'right', color: F.MUTED, fontWeight: 700 }}>{ep.avg_response_time_ms.toFixed(0)}</td>
                      </tr>
                    ))}
                </tbody>
              </table>
            </Panel>
          )}

          {/* Recent Requests */}
          {usageData.recent_requests && usageData.recent_requests.length > 0 && (
            <Panel title="RECENT REQUESTS" icon={<FileText size={14} />}>
              <table style={{ width: '100%', fontSize: '10px', fontFamily: FONT, borderCollapse: 'collapse' }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${F.BORDER}` }}>
                    {['TIME', 'ENDPOINT', 'METHOD', 'STATUS', 'MS'].map(h => (
                      <th key={h} style={{
                        padding: '6px 8px',
                        textAlign: h === 'ENDPOINT' ? 'left' : 'center',
                        color: F.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
                      }}>{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {usageData.recent_requests.slice(0, 20).map((req, i) => (
                    <tr key={i} style={{ borderBottom: `1px solid ${F.BORDER}` }}>
                      <td style={{ padding: '6px 8px', color: F.MUTED, fontSize: '9px', whiteSpace: 'nowrap' }}>
                        {new Date(req.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })}
                      </td>
                      <td style={{ padding: '6px 8px', color: F.WHITE, fontSize: '9px', maxWidth: '180px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{req.endpoint}</td>
                      <td style={{ padding: '6px 8px', textAlign: 'center', color: req.method === 'GET' ? F.CYAN : F.ORANGE, fontWeight: 700, fontSize: '9px' }}>{req.method}</td>
                      <td style={{ padding: '6px 8px', textAlign: 'center', color: req.status_code < 400 ? F.GREEN : F.RED, fontWeight: 700, fontSize: '9px' }}>{req.status_code}</td>
                      <td style={{ padding: '6px 8px', textAlign: 'center', color: F.MUTED, fontSize: '9px' }}>{req.response_time_ms}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </Panel>
          )}
        </>
      ) : null}
    </div>
  );
};

// ─── Security Section ─────────────────────────────────────────────────────────
const SecuritySection: React.FC<{
  session: any;
  onRegenerateKey: () => void;
  loading: boolean;
  showApiKey: boolean;
  setShowApiKey: (show: boolean) => void;
}> = ({ session, onRegenerateKey, loading, showApiKey, setShowApiKey }) => {
  const { refreshUserData } = useAuth();
  const apiKey = session?.api_key || '';
  const [mfaLoading, setMfaLoading] = useState(false);
  const [showPasswordPrompt, setShowPasswordPrompt] = useState(false);
  const [password, setPassword] = useState('');
  const [statusMessage, setStatusMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  const handleEnableMFA = async () => {
    if (!session?.api_key) return;
    setMfaLoading(true);
    setStatusMessage(null);

    try {
      const result = await withTimeout(
        UserApiService.enableMFA(session.api_key),
        API_TIMEOUT_MS,
        'MFA enable timeout'
      );

      setMfaLoading(false);
      if (result.success) {
        setStatusMessage({ type: 'success', text: 'MFA enabled! OTP codes will be sent via email during login.' });
        setTimeout(async () => { await refreshUserData(); setStatusMessage(null); }, 2000);
      } else {
        setStatusMessage({ type: 'error', text: result.error || 'Failed to enable MFA' });
        setTimeout(() => setStatusMessage(null), 3000);
      }
    } catch (err) {
      setMfaLoading(false);
      setStatusMessage({ type: 'error', text: err instanceof Error ? err.message : 'Network error' });
      setTimeout(() => setStatusMessage(null), 3000);
    }
  };

  const handleDisableMFA = () => {
    if (!session?.api_key) return;
    setShowPasswordPrompt(true);
    setStatusMessage(null);
  };

  const confirmDisableMFA = async () => {
    const sanitizedPassword = password.trim();
    if (!sanitizedPassword) {
      setStatusMessage({ type: 'error', text: 'Please enter your password' });
      setTimeout(() => setStatusMessage(null), 3000);
      return;
    }

    setMfaLoading(true);
    setStatusMessage(null);

    try {
      const result = await withTimeout(
        UserApiService.disableMFA(session.api_key, sanitizedPassword),
        API_TIMEOUT_MS,
        'MFA disable timeout'
      );

      setMfaLoading(false);
      setShowPasswordPrompt(false);
      setPassword('');

      if (result.success) {
        setStatusMessage({ type: 'success', text: 'MFA disabled successfully!' });
        setTimeout(async () => { await refreshUserData(); setStatusMessage(null); }, 2000);
      } else {
        setStatusMessage({ type: 'error', text: result.error || 'Failed to disable MFA' });
        setTimeout(() => setStatusMessage(null), 3000);
      }
    } catch (err) {
      setMfaLoading(false);
      setShowPasswordPrompt(false);
      setPassword('');
      setStatusMessage({ type: 'error', text: err instanceof Error ? err.message : 'Network error' });
      setTimeout(() => setStatusMessage(null), 3000);
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <Panel title="API KEY MANAGEMENT" icon={<Key size={14} />}>
        <div style={{ padding: '12px 0' }}>
          <div style={{ display: 'flex', gap: '8px', marginBottom: '12px' }}>
            <div style={{
              flex: 1,
              padding: '8px 10px',
              backgroundColor: F.DARK_BG,
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              fontFamily: FONT,
              fontSize: '10px',
              color: F.GRAY,
              overflow: 'hidden',
              textOverflow: 'ellipsis',
            }}>
              {showApiKey ? apiKey : apiKey.slice(0, 20) + '\u2022'.repeat(Math.max(0, apiKey.length - 20))}
            </div>
            <SecondaryBtn
              icon={showApiKey ? <EyeOff size={10} /> : <Eye size={10} />}
              label={showApiKey ? 'HIDE' : 'SHOW'}
              onClick={() => setShowApiKey(!showApiKey)}
            />
            <PrimaryBtn
              label={loading ? 'REGENERATING...' : 'REGENERATE'}
              onClick={onRegenerateKey}
              disabled={loading}
            />
          </div>
          <div style={{ fontSize: '9px', color: F.MUTED, fontFamily: FONT }}>
            Keep your API key secure. Never share it publicly.
          </div>
        </div>
      </Panel>

      <Panel title="SECURITY STATUS" icon={<Shield size={14} />}>
        <div style={{ padding: '8px 0' }}>
          {statusMessage && (
            <StatusBadge type={statusMessage.type} text={statusMessage.text} />
          )}
          <SecurityRow label="EMAIL VERIFICATION" enabled={session?.user_info?.is_verified} />
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            padding: '10px 0',
            borderBottom: `1px solid ${F.BORDER}`,
          }}>
            <span style={{ color: F.WHITE, fontSize: '10px', fontFamily: FONT }}>MULTI-FACTOR AUTH (EMAIL)</span>
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
              <EnabledBadge enabled={session?.user_info?.mfa_enabled} />
              <PrimaryBtn
                label={mfaLoading ? 'PROCESSING...' : session?.user_info?.mfa_enabled ? 'DISABLE' : 'ENABLE'}
                onClick={session?.user_info?.mfa_enabled ? handleDisableMFA : handleEnableMFA}
                disabled={mfaLoading}
                color={session?.user_info?.mfa_enabled ? F.RED : F.GREEN}
              />
            </div>
          </div>
        </div>
      </Panel>

      {/* Password prompt for MFA disable */}
      {showPasswordPrompt && (
        <ConfirmModal
          icon={<Shield size={16} />}
          title="DISABLE MULTI-FACTOR AUTH"
          message="Enter your password to confirm disabling MFA:"
          confirmLabel={mfaLoading ? 'PROCESSING...' : 'CONFIRM DISABLE'}
          confirmColor={F.RED}
          disabled={mfaLoading}
          onCancel={() => { setShowPasswordPrompt(false); setPassword(''); }}
          onConfirm={confirmDisableMFA}
        >
          <input
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && confirmDisableMFA()}
            placeholder="Enter your password"
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: F.DARK_BG,
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              color: F.WHITE,
              fontSize: '10px',
              fontFamily: FONT,
              marginBottom: '16px',
            }}
          />
        </ConfirmModal>
      )}
    </div>
  );
};

// ─── Billing Section ──────────────────────────────────────────────────────────
const BillingSection: React.FC<{
  subscriptionData: AccountSubscriptionData | null;
  paymentHistory: PaymentHistoryItem[];
  loading: boolean;
}> = ({ subscriptionData, paymentHistory, loading }) => {
  if (loading) {
    return <LoadingState message="Loading billing data..." />;
  }

  const daysUntilExpiry = subscriptionData?.credits_expire_at
    ? Math.max(0, Math.ceil((new Date(subscriptionData.credits_expire_at).getTime() - Date.now()) / (1000 * 60 * 60 * 24)))
    : null;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {/* Account & Plan Info */}
      <Panel title="ACCOUNT & PLAN" icon={<Package size={14} />}>
        {subscriptionData ? (
          <div style={{ padding: '8px 0' }}>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '16px', marginBottom: '16px' }}>
              <StatBox label="PLAN" value={subscriptionData.account_type.toUpperCase()} isText color={F.ORANGE} />
              <StatBox label="CREDITS" value={subscriptionData.credit_balance} color={F.CYAN} />
              {daysUntilExpiry !== null ? (
                <StatBox label="CREDITS EXPIRE IN" value={`${daysUntilExpiry}d`} isText color={daysUntilExpiry <= 7 ? F.RED : F.GREEN} />
              ) : (
                <StatBox label="SUPPORT" value={subscriptionData.support_type.toUpperCase()} isText />
              )}
            </div>
            <DataRow label="SUPPORT TYPE" value={subscriptionData.support_type.toUpperCase()} />
            <DataRow label="MEMBER SINCE" value={new Date(subscriptionData.created_at).toLocaleDateString()} />
            {subscriptionData.credits_expire_at && (
              <DataRow
                label="CREDITS EXPIRE"
                value={new Date(subscriptionData.credits_expire_at).toLocaleDateString()}
                valueColor={daysUntilExpiry !== null && daysUntilExpiry <= 7 ? F.RED : F.CYAN}
              />
            )}
            {subscriptionData.last_credit_purchase_at && (
              <DataRow label="LAST PURCHASE" value={new Date(subscriptionData.last_credit_purchase_at).toLocaleDateString()} />
            )}
          </div>
        ) : (
          <EmptyState icon={Package} message="No account data available" />
        )}
      </Panel>

      {/* Payment History */}
      <Panel title="PAYMENT HISTORY" icon={<FileText size={14} />}>
        {paymentHistory.length === 0 ? (
          <EmptyState icon={CreditCard} message="No payment history" />
        ) : (
          <table style={{ width: '100%', fontSize: '10px', fontFamily: FONT, borderCollapse: 'collapse' }}>
            <thead>
              <tr style={{ borderBottom: `1px solid ${F.BORDER}` }}>
                {['DATE', 'PLAN', 'GATEWAY', 'AMOUNT', 'STATUS'].map(h => (
                  <th key={h} style={{
                    padding: '8px',
                    textAlign: h === 'AMOUNT' || h === 'STATUS' ? 'right' : 'left',
                    color: F.GRAY,
                    fontSize: '9px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                  }}>
                    {h}
                  </th>
                ))}
              </tr>
            </thead>
            <tbody>
              {paymentHistory.map((p) => (
                <tr key={p.payment_uuid} style={{ borderBottom: `1px solid ${F.BORDER}`, transition: 'all 0.2s' }}>
                  <td style={{ padding: '8px', color: F.WHITE }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <Calendar size={10} color={F.GRAY} />
                      {new Date(p.created_at).toLocaleDateString()}
                    </div>
                  </td>
                  <td style={{ padding: '8px', color: F.WHITE }}>{p.plan_name || 'PURCHASE'}</td>
                  <td style={{ padding: '8px', color: F.GRAY }}>{p.payment_gateway || p.payment_method || 'N/A'}</td>
                  <td style={{ padding: '8px', textAlign: 'right', color: F.CYAN, fontWeight: 700 }}>
                    ${p.amount_usd}
                  </td>
                  <td style={{ padding: '8px', textAlign: 'right' }}>
                    <span style={{
                      padding: '2px 6px',
                      backgroundColor: p.status === 'success' || p.status === 'completed' ? `${F.GREEN}20` : p.status === 'pending' ? `${F.YELLOW}20` : `${F.RED}20`,
                      color: p.status === 'success' || p.status === 'completed' ? F.GREEN : p.status === 'pending' ? F.YELLOW : F.RED,
                      fontSize: '8px',
                      fontWeight: 700,
                      borderRadius: '2px',
                    }}>
                      {p.status.toUpperCase()}
                    </span>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </Panel>
    </div>
  );
};

// ─── Support Section ──────────────────────────────────────────────────────────
const SupportSection: React.FC = () => {
  const { session } = useAuth();

  // Feedback
  const [feedbackRating, setFeedbackRating] = useState(0);
  const [feedbackText, setFeedbackText] = useState('');
  const [feedbackCategory, setFeedbackCategory] = useState('general');
  const [feedbackLoading, setFeedbackLoading] = useState(false);
  const [feedbackSuccess, setFeedbackSuccess] = useState(false);
  const [feedbackError, setFeedbackError] = useState('');

  // Contact
  const [contactName, setContactName] = useState('');
  const [contactEmail, setContactEmail] = useState('');
  const [contactSubject, setContactSubject] = useState('');
  const [contactMessage, setContactMessage] = useState('');
  const [contactLoading, setContactLoading] = useState(false);
  const [contactSuccess, setContactSuccess] = useState(false);
  const [contactError, setContactError] = useState('');

  // Newsletter
  const [newsletterEmail, setNewsletterEmail] = useState('');
  const [newsletterLoading, setNewsletterLoading] = useState(false);
  const [newsletterSuccess, setNewsletterSuccess] = useState(false);
  const [newsletterError, setNewsletterError] = useState('');

  const handleFeedbackSubmit = async () => {
    // Validate and sanitize
    const sanitizedFeedback = sanitizeInput(feedbackText);
    if (!feedbackRating || !sanitizedFeedback) {
      setFeedbackError('Please provide a rating and feedback');
      return;
    }
    if (!session?.api_key) {
      setFeedbackError('Please login to submit feedback');
      return;
    }

    setFeedbackLoading(true);
    setFeedbackError('');

    try {
      const data: FeedbackFormData = { rating: feedbackRating, feedback_text: sanitizedFeedback, category: feedbackCategory };
      const result = await withTimeout(submitFeedback(data, session.api_key), API_TIMEOUT_MS, 'Feedback submit timeout');

      setFeedbackLoading(false);
      if (result.success) {
        setFeedbackSuccess(true);
        setTimeout(() => { setFeedbackRating(0); setFeedbackText(''); setFeedbackCategory('general'); setFeedbackSuccess(false); }, 3000);
      } else {
        setFeedbackError(result.error || 'Failed to submit feedback');
      }
    } catch (err) {
      setFeedbackLoading(false);
      setFeedbackError(err instanceof Error ? err.message : 'Network error');
    }
  };

  const handleContactSubmit = async () => {
    // Validate and sanitize
    const sanitizedName = sanitizeInput(contactName);
    const sanitizedSubject = sanitizeInput(contactSubject);
    const sanitizedMessage = sanitizeInput(contactMessage);

    if (!sanitizedName || !contactEmail.trim() || !sanitizedSubject || !sanitizedMessage) {
      setContactError('Please fill in all required fields');
      return;
    }

    // Validate email
    const emailValidation = validateEmail(contactEmail);
    if (!emailValidation.valid) {
      setContactError(emailValidation.error || 'Invalid email address');
      return;
    }

    setContactLoading(true);
    setContactError('');

    try {
      const data: ContactFormData = { name: sanitizedName, email: contactEmail.trim(), subject: sanitizedSubject, message: sanitizedMessage };
      const result = await withTimeout(submitContactForm(data), API_TIMEOUT_MS, 'Contact submit timeout');

      setContactLoading(false);
      if (result.success) {
        setContactSuccess(true);
        setTimeout(() => { setContactName(''); setContactEmail(''); setContactSubject(''); setContactMessage(''); setContactSuccess(false); }, 3000);
      } else {
        setContactError(result.error || 'Failed to submit contact form');
      }
    } catch (err) {
      setContactLoading(false);
      setContactError(err instanceof Error ? err.message : 'Network error');
    }
  };

  const handleNewsletterSubmit = async () => {
    // Validate email
    const emailValidation = validateEmail(newsletterEmail);
    if (!emailValidation.valid) {
      setNewsletterError(emailValidation.error || 'Please enter a valid email');
      return;
    }

    setNewsletterLoading(true);
    setNewsletterError('');

    try {
      const result = await withTimeout(
        subscribeToNewsletter({ email: newsletterEmail.trim(), source: 'profile_tab' }),
        API_TIMEOUT_MS,
        'Newsletter subscribe timeout'
      );

      setNewsletterLoading(false);
      if (result.success) {
        setNewsletterSuccess(true);
        setTimeout(() => { setNewsletterEmail(''); setNewsletterSuccess(false); }, 3000);
      } else {
        setNewsletterError(result.error || 'Failed to subscribe');
      }
    } catch (err) {
      setNewsletterLoading(false);
      setNewsletterError(err instanceof Error ? err.message : 'Network error');
    }
  };

  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
      {/* Feedback */}
      <Panel title="SEND FEEDBACK" icon={<MessageSquarePlus size={14} />}>
        <div style={{ marginBottom: '12px' }}>
          <Label text="RATING" required />
          <div style={{ display: 'flex', gap: '4px' }}>
            {[1, 2, 3, 4, 5].map(s => (
              <button key={s} onClick={() => setFeedbackRating(s)} style={{ background: 'transparent', border: 'none', cursor: 'pointer', padding: '2px' }}>
                <Star size={18} fill={s <= feedbackRating ? F.YELLOW : 'transparent'} color={s <= feedbackRating ? F.YELLOW : F.MUTED} />
              </button>
            ))}
          </div>
        </div>
        <div style={{ marginBottom: '12px' }}>
          <Label text="CATEGORY" />
          <select
            value={feedbackCategory}
            onChange={(e) => setFeedbackCategory(e.target.value)}
            style={{
              width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`,
              borderRadius: '2px', color: F.WHITE, fontSize: '10px', fontFamily: FONT,
            }}
          >
            <option value="general">General</option>
            <option value="feature_request">Feature Request</option>
            <option value="bug_report">Bug Report</option>
            <option value="ui_ux">UI/UX</option>
            <option value="performance">Performance</option>
            <option value="data">Data Quality</option>
          </select>
        </div>
        <InputField label="FEEDBACK" value={feedbackText} onChange={setFeedbackText} placeholder="Tell us what you think..." multiline rows={3} required />
        {feedbackError && <div style={{ color: F.RED, fontSize: '9px', marginBottom: '8px' }}>{feedbackError}</div>}
        <SubmitBtn onClick={handleFeedbackSubmit} loading={feedbackLoading} success={feedbackSuccess} text="SUBMIT FEEDBACK" />
      </Panel>

      {/* Contact */}
      <Panel title="CONTACT US" icon={<Mail size={14} />}>
        <InputField label="NAME" value={contactName} onChange={setContactName} placeholder="Your name" required />
        <InputField label="EMAIL" value={contactEmail} onChange={setContactEmail} placeholder="your@email.com" type="email" required />
        <InputField label="SUBJECT" value={contactSubject} onChange={setContactSubject} placeholder="Subject of your message" required />
        <InputField label="MESSAGE" value={contactMessage} onChange={setContactMessage} placeholder="Your message..." multiline rows={3} required />
        {contactError && <div style={{ color: F.RED, fontSize: '9px', marginBottom: '8px' }}>{contactError}</div>}
        <SubmitBtn onClick={handleContactSubmit} loading={contactLoading} success={contactSuccess} text="SEND MESSAGE" color={F.BLUE} />
      </Panel>

      {/* Newsletter */}
      <div style={{ gridColumn: '1 / -1' }}>
        <Panel title="NEWSLETTER SUBSCRIPTION" icon={<Newspaper size={14} />}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr auto', gap: '12px', alignItems: 'flex-end' }}>
            <div>
              <p style={{ color: F.GRAY, fontSize: '10px', marginBottom: '12px', lineHeight: '1.5' }}>
                Stay updated with the latest features, market insights, and Fincept news.
              </p>
              <InputField label="EMAIL ADDRESS" value={newsletterEmail} onChange={setNewsletterEmail} placeholder="your@email.com" type="email" required />
              {newsletterError && <div style={{ color: F.RED, fontSize: '9px', marginBottom: '8px' }}>{newsletterError}</div>}
            </div>
            <div style={{ paddingBottom: newsletterError ? '28px' : '0' }}>
              <PrimaryBtn
                label={newsletterLoading ? 'SUBSCRIBING...' : newsletterSuccess ? 'SUBSCRIBED!' : 'SUBSCRIBE'}
                onClick={handleNewsletterSubmit}
                disabled={newsletterLoading}
                color={newsletterSuccess ? F.GREEN : F.GREEN}
                icon={newsletterSuccess ? <Check size={12} /> : <Newspaper size={12} />}
              />
            </div>
          </div>
          <p style={{ color: F.MUTED, fontSize: '9px', marginTop: '8px' }}>
            You can unsubscribe at any time. We respect your privacy.
          </p>
        </Panel>
      </div>
    </div>
  );
};

export default ProfileTab;
