// File: src/components/tabs/profile/ProfileTab.tsx
// Fincept Terminal – Profile & Account Management (FINCEPT Design System)

import React, { useState, useEffect, useCallback } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { UserApiService } from '@/services/auth/userApi';
import { TabHeader } from '@/components/common/TabHeader';
import { TabFooter } from '@/components/common/TabFooter';
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

// ─── FINCEPT Design System Colors ─────────────────────────────────────────────
const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
};

const FONT = '"IBM Plex Mono", "Consolas", monospace';

// ─── Types (matched to actual API responses) ─────────────────────────────────
// GET /payment/subscription → { success, data: { user_id, account_type, credit_balance, ... } }
interface AccountSubscriptionData {
  user_id: number;
  account_type: string;
  credit_balance: number;
  credits_expire_at: string | null;
  support_type: string;
  last_credit_purchase_at: string | null;
  created_at: string;
}

// GET /payment/payments → { success, data: { payments: [...], pagination: {...} } }
interface PaymentHistoryItem {
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

interface UsageData {
  total_requests?: number;
  month_requests?: number;
  week_requests?: number;
}

type Section = 'overview' | 'usage' | 'security' | 'billing' | 'support';

// ─── Main Component ───────────────────────────────────────────────────────────
const ProfileTab: React.FC = () => {
  const { session, logout, refreshUserData } = useAuth();
  const [activeSection, setActiveSection] = useState<Section>('overview');
  const [loading, setLoading] = useState(false);
  const [usageData, setUsageData] = useState<UsageData | null>(null);
  const [subscriptionData, setSubscriptionData] = useState<AccountSubscriptionData | null>(null);
  const [paymentHistory, setPaymentHistory] = useState<PaymentHistoryItem[]>([]);
  const [showApiKey, setShowApiKey] = useState(false);
  const [showLogoutConfirm, setShowLogoutConfirm] = useState(false);
  const [showRegenerateConfirm, setShowRegenerateConfirm] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const fetchSectionData = useCallback(async () => {
    if (!session?.api_key) return;
    setLoading(true);
    setError(null);
    try {
      if (activeSection === 'usage') {
        const result = session.user_type === 'guest'
          ? await UserApiService.getGuestUsage(session.api_key)
          : await UserApiService.getUserUsage(session.api_key);
        if (result.success) setUsageData(result.data);
        else setError(result.error || 'Failed to load usage data');
      } else if (activeSection === 'billing') {
        // /payment/subscription returns account-level data
        // /payment/payments returns actual payment history (NOT /payment/history which is 404)
        const [subResult, historyResult] = await Promise.all([
          UserApiService.getUserSubscription(session.api_key),
          UserApiService.getPaymentHistory(session.api_key, 1, 20),
        ]);
        if (subResult.success && subResult.data) {
          // API wraps in { success, message, data: { ... } }
          const raw = subResult.data as any;
          const d = raw?.data || raw;
          setSubscriptionData(d);
        }
        if (historyResult.success && historyResult.data) {
          const raw = historyResult.data as any;
          const payments = raw?.data?.payments || raw?.payments || [];
          setPaymentHistory(payments);
        }
        if (!subResult.success && !historyResult.success) {
          setError('Failed to load billing data');
        }
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Network error');
    } finally {
      setLoading(false);
    }
  }, [activeSection, session]);

  useEffect(() => {
    fetchSectionData();
  }, [fetchSectionData]);

  const confirmLogout = async () => {
    setShowLogoutConfirm(false);
    await logout();
  };

  const confirmRegenerateKey = async () => {
    if (!session?.api_key) return;
    setShowRegenerateConfirm(false);
    setLoading(true);
    const result = await UserApiService.regenerateApiKey(session.api_key);
    setLoading(false);
    if (result.success) await refreshUserData();
  };

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
            onClick={() => setActiveSection(s.id)}
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
        {activeSection === 'overview' && (
          <OverviewSection session={session} onLogout={() => setShowLogoutConfirm(true)} />
        )}
        {activeSection === 'usage' && (
          <UsageSection usageData={usageData} loading={loading} session={session} />
        )}
        {activeSection === 'security' && (
          <SecuritySection
            session={session}
            onRegenerateKey={() => setShowRegenerateConfirm(true)}
            loading={loading}
            showApiKey={showApiKey}
            setShowApiKey={setShowApiKey}
          />
        )}
        {activeSection === 'billing' && (
          <BillingSection
            subscriptionData={subscriptionData}
            paymentHistory={paymentHistory}
            loading={loading}
          />
        )}
        {activeSection === 'support' && <SupportSection />}
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
          onCancel={() => setShowLogoutConfirm(false)}
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
          onCancel={() => setShowRegenerateConfirm(false)}
          onConfirm={confirmRegenerateKey}
        />
      )}
    </div>
  );
};

// ─── Overview Section ─────────────────────────────────────────────────────────
const OverviewSection: React.FC<{ session: any; onLogout: () => void }> = ({ session, onLogout }) => {
  const accountType = session?.user_info?.account_type || 'free';
  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
      <Panel title="ACCOUNT INFORMATION" icon={<User size={14} />}>
        <DataRow label="USERNAME" value={session?.user_info?.username || 'N/A'} />
        <DataRow label="EMAIL" value={session?.user_info?.email || 'N/A'} />
        <DataRow label="USER TYPE" value={session?.user_type?.toUpperCase() || 'N/A'} />
        <DataRow label="ACCOUNT TYPE" value={accountType.toUpperCase()} valueColor={F.ORANGE} />
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
              <DataRow label="DAILY LIMIT" value={session?.daily_limit?.toString() || '0'} />
              <DataRow label="REQUESTS TODAY" value={session?.requests_today?.toString() || '0'} />
            </>
          )}
        </div>
      </Panel>

      <div style={{ gridColumn: '1 / -1' }}>
        <Panel title="QUICK ACTIONS" icon={<Activity size={14} />}>
          <div style={{ display: 'flex', gap: '12px', padding: '8px 0' }}>
            <ActionBtn label="LOGOUT" onClick={onLogout} danger />
          </div>
        </Panel>
      </div>
    </div>
  );
};

// ─── Usage Section ────────────────────────────────────────────────────────────
const UsageSection: React.FC<{ usageData: UsageData | null; loading: boolean; session: any }> = ({ usageData, loading, session }) => {
  const credits = session?.user_info?.credit_balance || 0;
  const dailyLimit = session?.daily_limit || 0;
  const requestsToday = session?.requests_today || 0;
  const usagePercent = dailyLimit > 0 ? (requestsToday / dailyLimit) * 100 : 0;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <Panel title="CREDIT BALANCE" icon={<Zap size={14} />}>
        <div style={{ padding: '20px 0', textAlign: 'center' }}>
          <div style={{ fontSize: '48px', color: F.CYAN, fontWeight: 700, marginBottom: '8px', fontFamily: FONT }}>
            {credits.toLocaleString()}
          </div>
          <div style={{ fontSize: '9px', color: F.GRAY, letterSpacing: '0.5px', fontWeight: 700 }}>AVAILABLE CREDITS</div>
        </div>
      </Panel>

      {session?.user_type === 'guest' && (
        <Panel title="DAILY USAGE LIMIT" icon={<Activity size={14} />}>
          <div style={{ padding: '12px 0' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', fontSize: '10px' }}>
              <span style={{ color: F.GRAY, fontWeight: 700, fontSize: '9px', letterSpacing: '0.5px' }}>REQUESTS TODAY</span>
              <span style={{ color: F.WHITE, fontWeight: 700, fontFamily: FONT }}>
                {requestsToday} / {dailyLimit}
              </span>
            </div>
            <div style={{ height: '6px', backgroundColor: F.HEADER_BG, borderRadius: '2px', position: 'relative' }}>
              <div style={{
                height: '100%',
                width: `${Math.min(usagePercent, 100)}%`,
                backgroundColor: usagePercent >= 90 ? F.RED : usagePercent >= 70 ? F.YELLOW : F.GREEN,
                borderRadius: '2px',
                transition: 'width 0.3s',
              }} />
            </div>
          </div>
        </Panel>
      )}

      {loading ? (
        <LoadingState message="Loading usage data..." />
      ) : usageData ? (
        <Panel title="USAGE STATISTICS" icon={<BarChart size={14} />}>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '16px', padding: '12px 0' }}>
            <StatBox label="TOTAL REQUESTS" value={usageData.total_requests || 0} />
            <StatBox label="THIS MONTH" value={usageData.month_requests || 0} />
            <StatBox label="THIS WEEK" value={usageData.week_requests || 0} />
          </div>
        </Panel>
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
    const result = await UserApiService.enableMFA(session.api_key);
    setMfaLoading(false);
    if (result.success) {
      setStatusMessage({ type: 'success', text: 'MFA enabled! OTP codes will be sent via email during login.' });
      setTimeout(async () => { await refreshUserData(); setStatusMessage(null); }, 2000);
    } else {
      setStatusMessage({ type: 'error', text: result.error || 'Failed to enable MFA' });
      setTimeout(() => setStatusMessage(null), 3000);
    }
  };

  const handleDisableMFA = () => {
    if (!session?.api_key) return;
    setShowPasswordPrompt(true);
    setStatusMessage(null);
  };

  const confirmDisableMFA = async () => {
    if (!password) {
      setStatusMessage({ type: 'error', text: 'Please enter your password' });
      setTimeout(() => setStatusMessage(null), 3000);
      return;
    }
    setMfaLoading(true);
    setStatusMessage(null);
    const result = await UserApiService.disableMFA(session.api_key, password);
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
    if (!feedbackRating || !feedbackText.trim()) { setFeedbackError('Please provide a rating and feedback'); return; }
    if (!session?.api_key) { setFeedbackError('Please login to submit feedback'); return; }
    setFeedbackLoading(true);
    setFeedbackError('');
    const data: FeedbackFormData = { rating: feedbackRating, feedback_text: feedbackText, category: feedbackCategory };
    const result = await submitFeedback(data, session.api_key);
    setFeedbackLoading(false);
    if (result.success) {
      setFeedbackSuccess(true);
      setTimeout(() => { setFeedbackRating(0); setFeedbackText(''); setFeedbackCategory('general'); setFeedbackSuccess(false); }, 3000);
    } else {
      setFeedbackError(result.error || 'Failed to submit feedback');
    }
  };

  const handleContactSubmit = async () => {
    if (!contactName.trim() || !contactEmail.trim() || !contactSubject.trim() || !contactMessage.trim()) {
      setContactError('Please fill in all required fields'); return;
    }
    setContactLoading(true);
    setContactError('');
    const data: ContactFormData = { name: contactName, email: contactEmail, subject: contactSubject, message: contactMessage };
    const result = await submitContactForm(data);
    setContactLoading(false);
    if (result.success) {
      setContactSuccess(true);
      setTimeout(() => { setContactName(''); setContactEmail(''); setContactSubject(''); setContactMessage(''); setContactSuccess(false); }, 3000);
    } else {
      setContactError(result.error || 'Failed to submit contact form');
    }
  };

  const handleNewsletterSubmit = async () => {
    if (!newsletterEmail.trim()) { setNewsletterError('Please enter your email'); return; }
    setNewsletterLoading(true);
    setNewsletterError('');
    const result = await subscribeToNewsletter({ email: newsletterEmail, source: 'profile_tab' });
    setNewsletterLoading(false);
    if (result.success) {
      setNewsletterSuccess(true);
      setTimeout(() => { setNewsletterEmail(''); setNewsletterSuccess(false); }, 3000);
    } else {
      setNewsletterError(result.error || 'Failed to subscribe');
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

// ─── Shared UI Components (FINCEPT Design System) ─────────────────────────────

const Panel: React.FC<{ title: string; icon: React.ReactNode; children: React.ReactNode }> = ({ title, icon, children }) => (
  <div style={{ backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
    <div style={{
      padding: '12px',
      backgroundColor: F.HEADER_BG,
      borderBottom: `1px solid ${F.BORDER}`,
      display: 'flex',
      alignItems: 'center',
      gap: '8px',
    }}>
      <span style={{ color: F.ORANGE }}>{icon}</span>
      <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>{title}</span>
    </div>
    <div style={{ padding: '12px' }}>{children}</div>
  </div>
);

const DataRow: React.FC<{ label: string; value: string; valueColor?: string }> = ({ label, value, valueColor }) => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    padding: '6px 0',
    fontSize: '10px',
    borderBottom: `1px solid ${F.BORDER}`,
    fontFamily: FONT,
  }}>
    <span style={{ color: F.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px' }}>{label}</span>
    <span style={{ color: valueColor || F.WHITE, fontWeight: 700 }}>{value}</span>
  </div>
);

const StatBox: React.FC<{ label: string; value: number | string; isText?: boolean; color?: string }> = ({ label, value, isText, color }) => (
  <div style={{
    padding: '12px',
    backgroundColor: F.HEADER_BG,
    border: `1px solid ${F.BORDER}`,
    borderRadius: '2px',
    textAlign: 'center',
  }}>
    <div style={{ fontSize: isText ? '14px' : '24px', color: color || F.CYAN, fontWeight: 700, fontFamily: FONT }}>
      {typeof value === 'number' ? value.toLocaleString() : value}
    </div>
    <div style={{ fontSize: '9px', color: F.GRAY, marginTop: '4px', fontWeight: 700, letterSpacing: '0.5px' }}>{label}</div>
  </div>
);

const SecurityRow: React.FC<{ label: string; enabled: boolean }> = ({ label, enabled }) => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: '10px 0',
    borderBottom: `1px solid ${F.BORDER}`,
  }}>
    <span style={{ color: F.WHITE, fontSize: '10px', fontFamily: FONT }}>{label}</span>
    <EnabledBadge enabled={enabled} />
  </div>
);

const EnabledBadge: React.FC<{ enabled: boolean }> = ({ enabled }) => (
  <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
    <span style={{
      padding: '2px 6px',
      backgroundColor: enabled ? `${F.GREEN}20` : `${F.RED}20`,
      color: enabled ? F.GREEN : F.RED,
      fontSize: '8px',
      fontWeight: 700,
      borderRadius: '2px',
    }}>
      {enabled ? 'ENABLED' : 'DISABLED'}
    </span>
    {enabled ? <CheckCircle size={12} color={F.GREEN} /> : <AlertCircle size={12} color={F.RED} />}
  </div>
);

const Badge: React.FC<{ label: string; value: string; color: string }> = ({ label, value, color }) => (
  <div style={{
    padding: '6px 10px',
    backgroundColor: F.PANEL_BG,
    border: `1px solid ${F.BORDER}`,
    borderRadius: '2px',
    fontFamily: FONT,
    fontSize: '9px',
  }}>
    <span style={{ color: F.GRAY, fontWeight: 700, letterSpacing: '0.5px' }}>{label} </span>
    <span style={{ color, fontWeight: 700 }}>{value}</span>
  </div>
);

const PrimaryBtn: React.FC<{
  label: string;
  onClick: () => void;
  disabled?: boolean;
  color?: string;
  icon?: React.ReactNode;
}> = ({ label, onClick, disabled, color = F.ORANGE, icon }) => (
  <button
    onClick={onClick}
    disabled={disabled}
    style={{
      padding: '8px 16px',
      backgroundColor: color,
      color: F.DARK_BG,
      border: 'none',
      borderRadius: '2px',
      fontSize: '9px',
      fontWeight: 700,
      fontFamily: FONT,
      cursor: disabled ? 'not-allowed' : 'pointer',
      opacity: disabled ? 0.5 : 1,
      display: 'flex',
      alignItems: 'center',
      gap: '6px',
      transition: 'all 0.2s',
    }}
  >
    {icon}
    {label}
  </button>
);

const SecondaryBtn: React.FC<{ label: string; onClick: () => void; icon?: React.ReactNode }> = ({ label, onClick, icon }) => (
  <button
    onClick={(e) => { e.preventDefault(); e.stopPropagation(); onClick(); }}
    style={{
      padding: '6px 10px',
      backgroundColor: 'transparent',
      border: `1px solid ${F.BORDER}`,
      borderRadius: '2px',
      color: F.GRAY,
      fontSize: '9px',
      fontWeight: 700,
      fontFamily: FONT,
      cursor: 'pointer',
      display: 'flex',
      alignItems: 'center',
      gap: '4px',
      transition: 'all 0.2s',
    }}
  >
    {icon}
    {label}
  </button>
);

const ActionBtn: React.FC<{ label: string; onClick: () => void; danger?: boolean }> = ({ label, onClick, danger }) => (
  <button
    onClick={onClick}
    style={{
      padding: '8px 16px',
      backgroundColor: danger ? F.RED : F.ORANGE,
      color: F.DARK_BG,
      border: 'none',
      borderRadius: '2px',
      fontSize: '9px',
      fontWeight: 700,
      fontFamily: FONT,
      cursor: 'pointer',
      transition: 'all 0.2s',
    }}
  >
    {label}
  </button>
);

const Label: React.FC<{ text: string; required?: boolean }> = ({ text, required }) => (
  <label style={{ color: F.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', display: 'block', marginBottom: '4px' }}>
    {text} {required && <span style={{ color: F.RED }}>*</span>}
  </label>
);

const InputField: React.FC<{
  label: string;
  value: string;
  onChange: (v: string) => void;
  placeholder?: string;
  type?: string;
  required?: boolean;
  multiline?: boolean;
  rows?: number;
}> = ({ label, value, onChange, placeholder, type = 'text', required, multiline, rows = 4 }) => (
  <div style={{ marginBottom: '12px' }}>
    <Label text={label} required={required} />
    {multiline ? (
      <textarea
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        rows={rows}
        style={{
          width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`,
          borderRadius: '2px', color: F.WHITE, fontSize: '10px', fontFamily: FONT, resize: 'vertical',
        }}
      />
    ) : (
      <input
        type={type}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        style={{
          width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`,
          borderRadius: '2px', color: F.WHITE, fontSize: '10px', fontFamily: FONT,
        }}
      />
    )}
  </div>
);

const SubmitBtn: React.FC<{
  onClick: () => void;
  loading: boolean;
  success: boolean;
  text: string;
  color?: string;
}> = ({ onClick, loading, success, text, color = F.ORANGE }) => (
  <button
    onClick={onClick}
    disabled={loading}
    style={{
      width: '100%',
      backgroundColor: success ? F.GREEN : color,
      color: F.DARK_BG,
      border: 'none',
      borderRadius: '2px',
      padding: '8px 16px',
      fontSize: '9px',
      fontWeight: 700,
      fontFamily: FONT,
      cursor: loading ? 'not-allowed' : 'pointer',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      gap: '8px',
      opacity: loading ? 0.7 : 1,
      transition: 'all 0.2s',
    }}
  >
    {loading ? 'SUBMITTING...' : success ? <><Check size={12} /> SUCCESS!</> : <><Send size={12} /> {text}</>}
  </button>
);

const StatusBadge: React.FC<{ type: 'success' | 'error'; text: string }> = ({ type, text }) => (
  <div style={{
    padding: '8px 12px',
    backgroundColor: type === 'success' ? `${F.GREEN}15` : `${F.RED}15`,
    border: `1px solid ${type === 'success' ? F.GREEN : F.RED}`,
    borderRadius: '2px',
    color: type === 'success' ? F.GREEN : F.RED,
    fontSize: '9px',
    marginBottom: '12px',
    fontFamily: FONT,
    fontWeight: 700,
  }}>
    {text}
  </div>
);

const LoadingState: React.FC<{ message: string }> = ({ message }) => (
  <div style={{
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    justifyContent: 'center',
    padding: '40px',
    color: F.MUTED,
    fontSize: '10px',
    fontFamily: FONT,
  }}>
    <RefreshCw size={20} style={{ marginBottom: '8px', opacity: 0.5 }} className="animate-spin" />
    <span>{message}</span>
  </div>
);

const EmptyState: React.FC<{ icon: React.ElementType; message: string }> = ({ icon: Icon, message }) => (
  <div style={{
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    justifyContent: 'center',
    padding: '32px',
    color: F.MUTED,
    fontSize: '10px',
    textAlign: 'center',
    fontFamily: FONT,
  }}>
    <Icon size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
    <span>{message}</span>
  </div>
);

const ConfirmModal: React.FC<{
  icon: React.ReactNode;
  title: string;
  message: string;
  confirmLabel: string;
  confirmColor: string;
  disabled?: boolean;
  onCancel: () => void;
  onConfirm: () => void;
  children?: React.ReactNode;
}> = ({ icon, title, message, confirmLabel, confirmColor, disabled, onCancel, onConfirm, children }) => (
  <div style={{
    position: 'fixed',
    top: 0, left: 0, right: 0, bottom: 0,
    backgroundColor: 'rgba(0,0,0,0.8)',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    zIndex: 1000,
  }}>
    <div style={{
      backgroundColor: F.PANEL_BG,
      border: `1px solid ${F.ORANGE}`,
      borderRadius: '2px',
      padding: '24px',
      maxWidth: '400px',
      width: '90%',
    }}>
      <div style={{
        color: F.ORANGE,
        fontSize: '11px',
        fontWeight: 700,
        marginBottom: '16px',
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        fontFamily: FONT,
        letterSpacing: '0.5px',
      }}>
        {icon}
        {title}
      </div>
      <div style={{ color: F.GRAY, fontSize: '10px', marginBottom: '16px', fontFamily: FONT, lineHeight: '1.5' }}>
        {message}
      </div>
      {children}
      <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
        <SecondaryBtn label="CANCEL" onClick={onCancel} />
        <PrimaryBtn label={confirmLabel} onClick={onConfirm} disabled={disabled} color={confirmColor} />
      </div>
    </div>
  </div>
);

export default ProfileTab;
