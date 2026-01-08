// File: src/components/tabs/ProfileTab.tsx
// Bloomberg-style Professional Profile & Account Management Interface

import React, { useState, useEffect } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { UserApiService } from '@/services/userApi';
import { TabHeader } from '@/components/common/TabHeader';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';
import {
  User, CreditCard, Activity, Shield, Key, RefreshCw,
  LogOut, Crown, Zap, FileText, CheckCircle, AlertCircle, Eye, EyeOff, BarChart
} from 'lucide-react';

// Bloomberg Professional Color Palette
const COLORS = {
  ORANGE: '#ea580c',
  WHITE: '#ffffff',
  RED: '#ef4444',
  GREEN: '#10b981',
  GRAY: '#6b7280',
  DARK_BG: '#000000',
  PANEL_BG: '#0a0a0a',
  HEADER_BG: '#1a1a1a',
  CYAN: '#06b6d4',
  YELLOW: '#eab308',
  BORDER: '#262626',
  MUTED: '#737373'
};

type Section = 'overview' | 'usage' | 'security' | 'billing';

const ProfileTab: React.FC = () => {
  const { t } = useTranslation('profile');
  const { session, logout, refreshUserData } = useAuth();
  const [activeSection, setActiveSection] = useState<Section>('overview');
  const [loading, setLoading] = useState(false);
  const [usageData, setUsageData] = useState<any>(null);
  const [paymentHistory, setPaymentHistory] = useState<any[]>([]);
  const [showApiKey, setShowApiKey] = useState(false);
  const [showLogoutConfirm, setShowLogoutConfirm] = useState(false);
  const [showRegenerateConfirm, setShowRegenerateConfirm] = useState(false);

  useEffect(() => {
    fetchSectionData();
  }, [activeSection, session]);

  const fetchSectionData = async () => {
    if (!session?.api_key) return;
    setLoading(true);
    try {
      if (activeSection === 'usage') {
        const result = session.user_type === 'guest'
          ? await UserApiService.getGuestUsage(session.api_key)
          : await UserApiService.getUserUsage(session.api_key);
        if (result.success) setUsageData(result.data);
      } else if (activeSection === 'billing') {
        const result = await UserApiService.getPaymentHistory(session.api_key, 1, 10);
        if (result.success) setPaymentHistory(result.data?.payments || []);
      }
    } catch (error) {
      console.error('Error fetching data:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleLogout = async () => {
    setShowLogoutConfirm(true);
  };

  const confirmLogout = async () => {
    setShowLogoutConfirm(false);
    await logout();
  };

  const handleRegenerateKey = async () => {
    if (!session?.api_key) return;
    setShowRegenerateConfirm(true);
  };

  const confirmRegenerateKey = async () => {
    if (!session?.api_key) return;
    setShowRegenerateConfirm(false);
    setLoading(true);
    const result = await UserApiService.regenerateApiKey(session.api_key);
    setLoading(false);
    if (result.success) {
      await refreshUserData();
    }
  };

  const accountType = session?.user_info?.account_type || 'free';
  const credits = session?.user_info?.credit_balance || 0;
  const username = session?.user_info?.username || session?.user_info?.email || 'User';
  const email = session?.user_info?.email || 'N/A';
  const apiKey = session?.api_key || '';

  const headerActions = (
    <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
      <div style={{
        padding: '6px 12px',
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        fontFamily: 'monospace',
        fontSize: '11px'
      }}>
        <span style={{ color: COLORS.MUTED }}>{t('extracted.credits')} </span>
        <span style={{ color: COLORS.CYAN, fontWeight: 'bold' }}>{credits.toLocaleString()}</span>
      </div>
      <div style={{
        padding: '6px 12px',
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        fontFamily: 'monospace',
        fontSize: '11px'
      }}>
        <span style={{ color: COLORS.MUTED }}>{t('extracted.plan')} </span>
        <span style={{ color: COLORS.ORANGE, fontWeight: 'bold' }}>{accountType.toUpperCase()}</span>
      </div>
      <button
        onClick={() => fetchSectionData()}
        style={{
          padding: '6px 12px',
          backgroundColor: COLORS.HEADER_BG,
          border: `1px solid ${COLORS.BORDER}`,
          color: COLORS.WHITE,
          cursor: 'pointer',
          fontFamily: 'monospace',
          fontSize: '11px',
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}
      >
        <RefreshCw size={12} />
        {t('refresh')}
      </button>
    </div>
  );

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: COLORS.DARK_BG,
      fontFamily: 'Consolas, "Courier New", monospace'
    }}>
      <TabHeader
        title={t('extracted.profileAccount')}
        subtitle={`${username} | ${email}`}
        icon={<User size={20} />}
        actions={headerActions}
      />

      {/* Navigation Bar */}
      <div style={{
        borderBottom: `1px solid ${COLORS.BORDER}`,
        backgroundColor: COLORS.HEADER_BG,
        display: 'flex',
        gap: '0',
        flexShrink: 0
      }}>
        {[
          { id: 'overview', label: 'OVERVIEW', icon: User },
          { id: 'usage', label: 'USAGE & CREDITS', icon: Activity },
          { id: 'security', label: 'SECURITY', icon: Shield },
          { id: 'billing', label: 'BILLING', icon: CreditCard },
        ].map(section => (
          <button
            key={section.id}
            onClick={() => setActiveSection(section.id as Section)}
            style={{
              padding: '10px 20px',
              backgroundColor: activeSection === section.id ? COLORS.PANEL_BG : 'transparent',
              border: 'none',
              borderBottom: activeSection === section.id ? `2px solid ${COLORS.ORANGE}` : '2px solid transparent',
              color: activeSection === section.id ? COLORS.ORANGE : COLORS.MUTED,
              cursor: 'pointer',
              fontFamily: 'monospace',
              fontSize: '11px',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              transition: 'all 0.2s'
            }}
          >
            {React.createElement(section.icon, { size: 14 })}
            {section.label}
          </button>
        ))}
      </div>

      {/* Content Area */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '16px',
        minHeight: 0,
        display: 'flex',
        flexDirection: 'column'
      }}>
        {activeSection === 'overview' && <OverviewSection session={session} onLogout={handleLogout} />}
        {activeSection === 'usage' && <UsageSection usageData={usageData} loading={loading} session={session} />}
        {activeSection === 'security' && (
          <SecuritySection
            session={session}
            onRegenerateKey={handleRegenerateKey}
            loading={loading}
            showApiKey={showApiKey}
            setShowApiKey={setShowApiKey}
          />
        )}
        {activeSection === 'billing' && <BillingSection paymentHistory={paymentHistory} loading={loading} />}
      </div>

      <TabFooter
        tabName="PROFILE"
        leftInfo={[
          { label: 'USER', value: username, color: COLORS.CYAN },
          { label: 'TYPE', value: session?.user_type?.toUpperCase(), color: COLORS.WHITE }
        ]}
        statusInfo={`Last Updated: ${new Date().toLocaleTimeString()}`}
      />

      {/* Logout Confirmation Modal */}
      {showLogoutConfirm && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `2px solid ${COLORS.ORANGE}`,
            padding: '24px',
            maxWidth: '400px',
            width: '90%'
          }}>
            <div style={{
              color: COLORS.ORANGE,
              fontSize: '14px',
              fontWeight: 'bold',
              marginBottom: '16px',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}>
              <LogOut size={18} />
              CONFIRM LOGOUT
            </div>
            <div style={{
              color: COLORS.MUTED,
              fontSize: '11px',
              marginBottom: '16px'
            }}>
              Are you sure you want to logout from Fincept Terminal?
            </div>
            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
              <button
                onClick={() => setShowLogoutConfirm(false)}
                style={{
                  padding: '8px 16px',
                  backgroundColor: COLORS.HEADER_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  color: COLORS.WHITE,
                  cursor: 'pointer',
                  fontSize: '11px',
                  fontFamily: 'monospace'
                }}
              >
                CANCEL
              </button>
              <button
                onClick={confirmLogout}
                style={{
                  padding: '8px 16px',
                  backgroundColor: COLORS.RED,
                  border: 'none',
                  color: COLORS.WHITE,
                  cursor: 'pointer',
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  fontWeight: 'bold'
                }}
              >
                LOGOUT
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Regenerate API Key Confirmation Modal */}
      {showRegenerateConfirm && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `2px solid ${COLORS.ORANGE}`,
            padding: '24px',
            maxWidth: '400px',
            width: '90%'
          }}>
            <div style={{
              color: COLORS.ORANGE,
              fontSize: '14px',
              fontWeight: 'bold',
              marginBottom: '16px',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}>
              <Key size={18} />
              REGENERATE API KEY
            </div>
            <div style={{
              color: COLORS.MUTED,
              fontSize: '11px',
              marginBottom: '16px'
            }}>
              Your current API key will be invalidated. Are you sure you want to continue?
            </div>
            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
              <button
                onClick={() => setShowRegenerateConfirm(false)}
                style={{
                  padding: '8px 16px',
                  backgroundColor: COLORS.HEADER_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  color: COLORS.WHITE,
                  cursor: 'pointer',
                  fontSize: '11px',
                  fontFamily: 'monospace'
                }}
              >
                CANCEL
              </button>
              <button
                onClick={confirmRegenerateKey}
                disabled={loading}
                style={{
                  padding: '8px 16px',
                  backgroundColor: COLORS.ORANGE,
                  border: 'none',
                  color: COLORS.WHITE,
                  cursor: loading ? 'not-allowed' : 'pointer',
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  fontWeight: 'bold',
                  opacity: loading ? 0.5 : 1
                }}
              >
                {loading ? 'REGENERATING...' : 'CONFIRM'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

// Overview Section
const OverviewSection: React.FC<{ session: any; onLogout: () => void }> = ({ session, onLogout }) => {
  const accountType = session?.user_info?.account_type || 'free';

  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
      {/* Account Info */}
      <DataPanel title="ACCOUNT INFORMATION" icon={<User size={16} />}>
        <DataRow label="USERNAME" value={session?.user_info?.username || 'N/A'} />
        <DataRow label="EMAIL" value={session?.user_info?.email || 'N/A'} />
        <DataRow label="USER TYPE" value={session?.user_type?.toUpperCase() || 'N/A'} />
        <DataRow label="ACCOUNT TYPE" value={accountType.toUpperCase()} valueColor={COLORS.ORANGE} />
        <DataRow
          label="EMAIL VERIFIED"
          value={session?.user_info?.is_verified ? 'YES' : 'NO'}
          valueColor={session?.user_info?.is_verified ? COLORS.GREEN : COLORS.RED}
        />
        <DataRow
          label="2FA ENABLED"
          value={session?.user_info?.mfa_enabled ? 'YES' : 'NO'}
          valueColor={session?.user_info?.mfa_enabled ? COLORS.GREEN : COLORS.RED}
        />
      </DataPanel>

      {/* Credits & Balance */}
      <DataPanel title="CREDITS & BALANCE" icon={<Zap size={16} />}>
        <div style={{ padding: '20px 0', textAlign: 'center', borderBottom: `1px solid ${COLORS.BORDER}` }}>
          <div style={{ fontSize: '36px', color: COLORS.CYAN, fontWeight: 'bold', marginBottom: '8px' }}>
            {(session?.user_info?.credit_balance || 0).toLocaleString()}
          </div>
          <div style={{ fontSize: '11px', color: COLORS.MUTED }}>AVAILABLE CREDITS</div>
        </div>
        <div style={{ padding: '12px 0' }}>
          <DataRow label="PLAN" value={accountType.toUpperCase()} valueColor={COLORS.ORANGE} />
          {session?.user_type === 'guest' && (
            <>
              <DataRow label="DAILY LIMIT" value={session?.daily_limit?.toString() || '0'} />
              <DataRow label="REQUESTS TODAY" value={session?.requests_today?.toString() || '0'} />
            </>
          )}
        </div>
      </DataPanel>

      {/* Quick Actions */}
      <div style={{ gridColumn: '1 / -1' }}>
        <DataPanel title="QUICK ACTIONS" icon={<Activity size={16} />}>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px', padding: '8px 0' }}>
            <ActionButton label="REFRESH DATA" onClick={() => window.location.reload()} />
            <ActionButton label="UPGRADE PLAN" onClick={() => {}} />
            <ActionButton label="SETTINGS" onClick={() => {}} />
            <ActionButton label="LOGOUT" onClick={onLogout} danger />
          </div>
        </DataPanel>
      </div>
    </div>
  );
};

// Usage Section
const UsageSection: React.FC<{ usageData: any; loading: boolean; session: any }> = ({ usageData, loading, session }) => {
  const credits = session?.user_info?.credit_balance || 0;
  const dailyLimit = session?.daily_limit || 0;
  const requestsToday = session?.requests_today || 0;
  const usagePercent = dailyLimit > 0 ? (requestsToday / dailyLimit) * 100 : 0;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <DataPanel title="CREDIT BALANCE" icon={<Zap size={16} />}>
        <div style={{ padding: '20px 0', textAlign: 'center' }}>
          <div style={{ fontSize: '48px', color: COLORS.CYAN, fontWeight: 'bold', marginBottom: '8px' }}>
            {credits.toLocaleString()}
          </div>
          <div style={{ fontSize: '11px', color: COLORS.MUTED }}>AVAILABLE CREDITS</div>
        </div>
      </DataPanel>

      {session?.user_type === 'guest' && (
        <DataPanel title="DAILY USAGE LIMIT" icon={<Activity size={16} />}>
          <div style={{ padding: '12px 0' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', fontSize: '11px' }}>
              <span style={{ color: COLORS.MUTED }}>REQUESTS TODAY</span>
              <span style={{ color: COLORS.WHITE, fontWeight: 'bold' }}>
                {requestsToday} / {dailyLimit}
              </span>
            </div>
            <div style={{ height: '8px', backgroundColor: COLORS.HEADER_BG, position: 'relative' }}>
              <div style={{
                height: '100%',
                width: `${usagePercent}%`,
                backgroundColor: usagePercent >= 90 ? COLORS.RED : COLORS.GREEN,
                transition: 'width 0.3s'
              }} />
            </div>
          </div>
        </DataPanel>
      )}

      {usageData && (
        <DataPanel title="USAGE STATISTICS" icon={<BarChart size={16} />}>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '16px', padding: '12px 0' }}>
            <StatBox label="TOTAL REQUESTS" value={usageData.total_requests || 0} />
            <StatBox label="THIS MONTH" value={usageData.month_requests || 0} />
            <StatBox label="THIS WEEK" value={usageData.week_requests || 0} />
          </div>
        </DataPanel>
      )}
    </div>
  );
};

// Security Section
const SecuritySection: React.FC<{
  session: any;
  onRegenerateKey: () => void;
  loading: boolean;
  showApiKey: boolean;
  setShowApiKey: (show: boolean) => void;
}> = ({ session, onRegenerateKey, loading, showApiKey, setShowApiKey }) => {
  const { refreshUserData } = useAuth();
  const apiKey = session?.api_key || '';
  const [mfaLoading, setMfaLoading] = React.useState(false);
  const [showPasswordPrompt, setShowPasswordPrompt] = React.useState(false);
  const [password, setPassword] = React.useState('');
  const [statusMessage, setStatusMessage] = React.useState<{ type: 'success' | 'error', text: string } | null>(null);

  const handleEnableMFA = async () => {
    if (!session?.api_key) return;
    setMfaLoading(true);
    setStatusMessage(null);
    const result = await UserApiService.enableMFA(session.api_key);
    setMfaLoading(false);
    if (result.success) {
      setStatusMessage({ type: 'success', text: 'MFA enabled! You will receive OTP codes via email during login.' });
      setTimeout(async () => {
        await refreshUserData();
        setStatusMessage(null);
      }, 2000);
    } else {
      setStatusMessage({ type: 'error', text: result.error || 'Failed to enable MFA' });
      setTimeout(() => setStatusMessage(null), 3000);
    }
  };

  const handleDisableMFA = async () => {
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
      setTimeout(async () => {
        await refreshUserData();
        setStatusMessage(null);
      }, 2000);
    } else {
      setStatusMessage({ type: 'error', text: result.error || 'Failed to disable MFA' });
      setTimeout(() => setStatusMessage(null), 3000);
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <DataPanel title="API KEY MANAGEMENT" icon={<Key size={16} />}>
        <div style={{ padding: '12px 0' }}>
          <div style={{ display: 'flex', gap: '8px', marginBottom: '12px' }}>
            <div style={{
              flex: 1,
              padding: '10px',
              backgroundColor: COLORS.HEADER_BG,
              border: `1px solid ${COLORS.BORDER}`,
              fontFamily: 'monospace',
              fontSize: '11px',
              color: COLORS.MUTED,
              overflow: 'hidden',
              textOverflow: 'ellipsis'
            }}>
              {showApiKey ? apiKey : apiKey.slice(0, 20) + '•'.repeat(Math.max(0, apiKey.length - 20))}
            </div>
            <button
              onClick={() => setShowApiKey(!showApiKey)}
              style={{
                padding: '10px 16px',
                backgroundColor: COLORS.HEADER_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                cursor: 'pointer',
                fontSize: '11px',
                fontFamily: 'monospace'
              }}
            >
              {showApiKey ? <><EyeOff size={12} /> HIDE</> : <><Eye size={12} /> SHOW</>}
            </button>
            <button
              onClick={onRegenerateKey}
              disabled={loading}
              style={{
                padding: '10px 16px',
                backgroundColor: COLORS.ORANGE,
                border: `1px solid ${COLORS.ORANGE}`,
                color: COLORS.WHITE,
                cursor: loading ? 'not-allowed' : 'pointer',
                fontSize: '11px',
                fontFamily: 'monospace',
                opacity: loading ? 0.5 : 1
              }}
            >
              {loading ? 'REGENERATING...' : 'REGENERATE'}
            </button>
          </div>
          <div style={{ fontSize: '10px', color: COLORS.MUTED }}>
            ⚠ Keep your API key secure. Never share it publicly.
          </div>
        </div>
      </DataPanel>

      <DataPanel title="SECURITY STATUS" icon={<Shield size={16} />}>
        <div style={{ padding: '8px 0' }}>
          {/* Status Message */}
          {statusMessage && (
            <div style={{
              padding: '8px 12px',
              backgroundColor: statusMessage.type === 'success' ? 'rgba(16, 185, 129, 0.1)' : 'rgba(239, 68, 68, 0.1)',
              border: `1px solid ${statusMessage.type === 'success' ? COLORS.GREEN : COLORS.RED}`,
              color: statusMessage.type === 'success' ? COLORS.GREEN : COLORS.RED,
              fontSize: '10px',
              marginBottom: '12px',
              fontFamily: 'monospace'
            }}>
              {statusMessage.text}
            </div>
          )}

          <SecurityRow
            label="EMAIL VERIFICATION"
            enabled={session?.user_info?.is_verified}
          />
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            padding: '10px 0',
            borderBottom: `1px solid ${COLORS.BORDER}`
          }}>
            <span style={{ color: COLORS.WHITE, fontSize: '11px' }}>MULTI-FACTOR AUTHENTICATION (EMAIL-BASED)</span>
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                <span style={{
                  color: session?.user_info?.mfa_enabled ? COLORS.GREEN : COLORS.RED,
                  fontSize: '11px',
                  fontWeight: 'bold'
                }}>
                  {session?.user_info?.mfa_enabled ? 'ENABLED' : 'DISABLED'}
                </span>
                {session?.user_info?.mfa_enabled ? (
                  <CheckCircle size={14} color={COLORS.GREEN} />
                ) : (
                  <AlertCircle size={14} color={COLORS.RED} />
                )}
              </div>
              <button
                onClick={session?.user_info?.mfa_enabled ? handleDisableMFA : handleEnableMFA}
                disabled={mfaLoading}
                style={{
                  padding: '6px 12px',
                  backgroundColor: session?.user_info?.mfa_enabled ? COLORS.RED : COLORS.GREEN,
                  border: 'none',
                  color: COLORS.WHITE,
                  cursor: mfaLoading ? 'not-allowed' : 'pointer',
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  fontWeight: 'bold',
                  opacity: mfaLoading ? 0.5 : 1
                }}
              >
                {mfaLoading ? 'PROCESSING...' : (session?.user_info?.mfa_enabled ? 'DISABLE' : 'ENABLE')}
              </button>
            </div>
          </div>
        </div>
      </DataPanel>

      {/* Password Prompt Modal for Disabling MFA */}
      {showPasswordPrompt && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `2px solid ${COLORS.ORANGE}`,
            padding: '24px',
            maxWidth: '400px',
            width: '90%'
          }}>
            <div style={{
              color: COLORS.ORANGE,
              fontSize: '14px',
              fontWeight: 'bold',
              marginBottom: '16px',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}>
              <Shield size={18} />
              DISABLE MULTI-FACTOR AUTHENTICATION
            </div>
            <div style={{
              color: COLORS.MUTED,
              fontSize: '11px',
              marginBottom: '16px'
            }}>
              Please enter your password to confirm disabling MFA:
            </div>
            <input
              type="password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              onKeyPress={(e) => e.key === 'Enter' && confirmDisableMFA()}
              placeholder="Enter your password"
              style={{
                width: '100%',
                padding: '10px',
                backgroundColor: COLORS.HEADER_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                fontSize: '11px',
                fontFamily: 'monospace',
                marginBottom: '16px'
              }}
            />
            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
              <button
                onClick={() => {
                  setShowPasswordPrompt(false);
                  setPassword('');
                }}
                style={{
                  padding: '8px 16px',
                  backgroundColor: COLORS.HEADER_BG,
                  border: `1px solid ${COLORS.BORDER}`,
                  color: COLORS.WHITE,
                  cursor: 'pointer',
                  fontSize: '11px',
                  fontFamily: 'monospace'
                }}
              >
                CANCEL
              </button>
              <button
                onClick={confirmDisableMFA}
                disabled={mfaLoading}
                style={{
                  padding: '8px 16px',
                  backgroundColor: COLORS.RED,
                  border: 'none',
                  color: COLORS.WHITE,
                  cursor: mfaLoading ? 'not-allowed' : 'pointer',
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  fontWeight: 'bold',
                  opacity: mfaLoading ? 0.5 : 1
                }}
              >
                {mfaLoading ? 'PROCESSING...' : 'CONFIRM DISABLE'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

// Billing Section
const BillingSection: React.FC<{ paymentHistory: any[]; loading: boolean }> = ({ paymentHistory, loading }) => {
  return (
    <DataPanel title="PAYMENT HISTORY" icon={<FileText size={16} />}>
      {loading ? (
        <div style={{ padding: '40px', textAlign: 'center', color: COLORS.MUTED }}>LOADING...</div>
      ) : paymentHistory.length === 0 ? (
        <div style={{ padding: '40px', textAlign: 'center', color: COLORS.MUTED }}>NO PAYMENT HISTORY</div>
      ) : (
        <table style={{ width: '100%', fontSize: '11px', fontFamily: 'monospace', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
              <th style={{ padding: '8px', textAlign: 'left', color: COLORS.MUTED }}>DATE</th>
              <th style={{ padding: '8px', textAlign: 'left', color: COLORS.MUTED }}>PLAN</th>
              <th style={{ padding: '8px', textAlign: 'right', color: COLORS.MUTED }}>AMOUNT</th>
              <th style={{ padding: '8px', textAlign: 'right', color: COLORS.MUTED }}>STATUS</th>
            </tr>
          </thead>
          <tbody>
            {paymentHistory.map((payment, index) => (
              <tr key={index} style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                <td style={{ padding: '8px', color: COLORS.WHITE }}>
                  {new Date(payment.created_at).toLocaleDateString()}
                </td>
                <td style={{ padding: '8px', color: COLORS.WHITE }}>
                  {payment.plan_name || 'PURCHASE'}
                </td>
                <td style={{ padding: '8px', textAlign: 'right', color: COLORS.CYAN, fontWeight: 'bold' }}>
                  ${payment.amount}
                </td>
                <td style={{
                  padding: '8px',
                  textAlign: 'right',
                  color: payment.status === 'success' ? COLORS.GREEN : COLORS.RED,
                  fontWeight: 'bold'
                }}>
                  {payment.status.toUpperCase()}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </DataPanel>
  );
};

// Helper Components
const DataPanel: React.FC<{ title: string; icon: React.ReactNode; children: React.ReactNode }> = ({ title, icon, children }) => (
  <div style={{
    backgroundColor: COLORS.PANEL_BG,
    border: `1px solid ${COLORS.BORDER}`,
    padding: '16px'
  }}>
    <div style={{
      display: 'flex',
      alignItems: 'center',
      gap: '8px',
      marginBottom: '12px',
      paddingBottom: '8px',
      borderBottom: `1px solid ${COLORS.BORDER}`
    }}>
      <span style={{ color: COLORS.ORANGE }}>{icon}</span>
      <span style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 'bold' }}>{title}</span>
    </div>
    {children}
  </div>
);

const DataRow: React.FC<{ label: string; value: string; valueColor?: string }> = ({ label, value, valueColor }) => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    padding: '6px 0',
    fontSize: '11px',
    borderBottom: `1px solid ${COLORS.BORDER}`
  }}>
    <span style={{ color: COLORS.MUTED }}>{label}</span>
    <span style={{ color: valueColor || COLORS.WHITE, fontWeight: 'bold' }}>{value}</span>
  </div>
);

const ActionButton: React.FC<{ label: string; onClick: () => void; danger?: boolean }> = ({ label, onClick, danger }) => (
  <button
    onClick={onClick}
    style={{
      padding: '12px',
      backgroundColor: COLORS.HEADER_BG,
      border: `1px solid ${COLORS.BORDER}`,
      color: danger ? COLORS.RED : COLORS.WHITE,
      cursor: 'pointer',
      fontSize: '10px',
      fontFamily: 'monospace',
      fontWeight: 'bold',
      transition: 'all 0.2s'
    }}
    onMouseEnter={(e) => {
      e.currentTarget.style.backgroundColor = COLORS.BORDER;
    }}
    onMouseLeave={(e) => {
      e.currentTarget.style.backgroundColor = COLORS.HEADER_BG;
    }}
  >
    {label}
  </button>
);

const StatBox: React.FC<{ label: string; value: number }> = ({ label, value }) => (
  <div style={{
    padding: '12px',
    backgroundColor: COLORS.HEADER_BG,
    border: `1px solid ${COLORS.BORDER}`,
    textAlign: 'center'
  }}>
    <div style={{ fontSize: '24px', color: COLORS.CYAN, fontWeight: 'bold' }}>{value.toLocaleString()}</div>
    <div style={{ fontSize: '10px', color: COLORS.MUTED, marginTop: '4px' }}>{label}</div>
  </div>
);

const SecurityRow: React.FC<{ label: string; enabled: boolean }> = ({ label, enabled }) => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: '10px 0',
    borderBottom: `1px solid ${COLORS.BORDER}`
  }}>
    <span style={{ color: COLORS.WHITE, fontSize: '11px' }}>{label}</span>
    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
      <span style={{
        color: enabled ? COLORS.GREEN : COLORS.RED,
        fontSize: '11px',
        fontWeight: 'bold'
      }}>
        {enabled ? 'ENABLED' : 'DISABLED'}
      </span>
      {enabled ? (
        <CheckCircle size={14} color={COLORS.GREEN} />
      ) : (
        <AlertCircle size={14} color={COLORS.RED} />
      )}
    </div>
  </div>
);

export default ProfileTab;
