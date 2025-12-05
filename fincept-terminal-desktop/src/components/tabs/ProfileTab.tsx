import React, { useState, useEffect } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { useNavigation } from '@/contexts/NavigationContext';
import { UserApiService } from '@/services/userApi';
import { useTerminalTheme } from '@/contexts/ThemeContext';

type ProfileScreen = 'overview' | 'usage' | 'payments' | 'subscriptions' | 'support' | 'settings';

const ProfileTab: React.FC = () => {
  const { colors } = useTerminalTheme();
  const { session, logout } = useAuth();
  const navigation = useNavigation();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [currentScreen, setCurrentScreen] = useState<ProfileScreen>('overview');
  const [loading, setLoading] = useState(false);

  // Data states
  const [usageData, setUsageData] = useState<any>(null);
  const [paymentHistory, setPaymentHistory] = useState<any[]>([]);
  const [subscriptions, setSubscriptions] = useState<any[]>([]);
  const [supportTickets, setSupportTickets] = useState<any[]>([]);
  const [detailedUsage, setDetailedUsage] = useState<any>(null);

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // F11 fullscreen toggle
  const toggleFullscreen = () => {
    if (!document.fullscreenElement) {
      document.documentElement.requestFullscreen();
    } else {
      if (document.exitFullscreen) {
        document.exitFullscreen();
      }
    }
  };

  const fetchData = async (showNotification = false) => {
    if (!session?.api_key) return;

    setLoading(true);
    try {
      if (currentScreen === 'usage') {
        if (session.user_type === 'guest') {
          const result = await UserApiService.getGuestUsage(session.api_key || '');
          if (result.success) setUsageData(result.data);
        } else {
          const result = await UserApiService.getUserUsage(session.api_key || '');
          if (result.success) setUsageData(result.data);

          const detailedResult = await UserApiService.getUsageDetails(session.api_key || '');
          if (detailedResult.success) setDetailedUsage(detailedResult.data);
        }
      } else if (currentScreen === 'payments') {
        const result = await UserApiService.getPaymentHistory(session.api_key || '', 1, 20);
        if (result.success) setPaymentHistory(result.data?.payments || result.data || []);
      } else if (currentScreen === 'subscriptions') {
        const result = await UserApiService.getUserSubscriptions(session.api_key || '');
        if (result.success) setSubscriptions(result.data?.subscriptions || result.data || []);
      } else if (currentScreen === 'support') {
        const result = await UserApiService.getUserTickets(session.api_key || '');
        if (result.success) setSupportTickets(result.data?.tickets || result.data || []);
      }

      if (showNotification) {
        alert('✓ Profile data refreshed successfully!');
      }
    } catch (error) {
      console.error('Error fetching data:', error);
      if (showNotification) {
        alert('✗ Failed to refresh data. Please try again.');
      }
    } finally {
      setLoading(false);
    }
  };

  // Fetch data based on current screen
  useEffect(() => {
    fetchData();
  }, [currentScreen, session]);

  const logoutUser = async () => {
    if (confirm('Are you sure you want to logout?')) {
      await logout();
    }
  };

  const regenerateApiKey = async () => {
    if (!session?.api_key) return;
    if (confirm('Are you sure you want to regenerate your API key? Your old key will be invalidated.')) {
      setLoading(true);
      const result = await UserApiService.regenerateApiKey(session.api_key || '');
      setLoading(false);
      if (result.success) {
        alert(`New API Key: ${result.data?.new_api_key || 'Check your profile'}\n\nPlease save this key securely. You will need to re-login.`);
        await logout();
      } else {
        alert(`Failed to regenerate API key: ${result.error}`);
      }
    }
  };

  const extendSession = async () => {
    if (!session?.api_key) return;
    setLoading(true);
    const result = await UserApiService.extendGuestSession(session.api_key || '');
    setLoading(false);
    if (result.success) {
      alert('Session extended successfully!');
    } else {
      alert(`Failed to extend session: ${result.error}`);
    }
  };

  const cancelSubscription = async () => {
    if (!session?.api_key) return;
    if (confirm('Are you sure you want to cancel your subscription? It will remain active until the end of the billing period.')) {
      setLoading(true);
      const result = await UserApiService.cancelSubscription(session.api_key || '');
      setLoading(false);
      if (result.success) {
        alert('Subscription cancelled successfully. It will remain active until the end of your billing period.');
      } else {
        alert(`Failed to cancel subscription: ${result.error}`);
      }
    }
  };

  const deleteAccount = async () => {
    if (!session?.api_key) return;
    const confirmation = prompt('This action cannot be undone. Type "DELETE MY ACCOUNT" to confirm:');
    if (confirmation === 'DELETE MY ACCOUNT') {
      setLoading(true);
      const result = await UserApiService.deleteUserAccount(session.api_key || '', { confirm: true });
      setLoading(false);
      if (result.success) {
        alert('Your account has been deleted.');
        await logout();
      } else {
        alert(`Failed to delete account: ${result.error}`);
      }
    }
  };

  const formatDate = (dateStr?: string) => {
    if (!dateStr) return 'N/A';
    return new Date(dateStr).toLocaleDateString('en-US', {
      year: 'numeric',
      month: 'short',
      day: 'numeric'
    });
  };

  const formatCurrency = (amount: number) => {
    return `$${(amount / 100).toFixed(2)}`;
  };

  // Guest Profile View
  const renderGuestProfile = () => {
    const displayDeviceId = session?.device_id?.substring(0, 30) + "..." || 'Unknown';
    const requestsToday = session?.requests_today || usageData?.requests_today || 0;
    const dailyLimit = session?.daily_limit || usageData?.daily_limit || 50;
    const remaining = Math.max(0, dailyLimit - requestsToday);
    const usagePercent = (requestsToday / dailyLimit) * 100;

    return (
      <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: colors.background, overflow: 'hidden' }}>
        {/* Header */}
        <div style={{ backgroundColor: colors.panel, borderBottom: `1px solid ${colors.textMuted}`, padding: '4px 8px', flexShrink: 0 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px' }}>
            <span style={{ color: colors.primary, fontWeight: 'bold' }}>FINCEPT TERMINAL GUEST PROFILE</span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.text }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.textMuted }}>REQUESTS:</span>
            <span style={{ color: colors.accent }}>{requestsToday}/{dailyLimit}</span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.textMuted }}>REMAINING:</span>
            <span style={{ color: remaining > 10 ? colors.secondary : colors.alert }}>{remaining}</span>
            <button
              onClick={() => fetchData(true)}
              disabled={loading}
              style={{
                marginLeft: 'auto',
                padding: '2px 8px',
                backgroundColor: colors.primary,
                color: colors.background,
                border: 'none',
                borderRadius: '2px',
                fontSize: '11px',
                cursor: loading ? 'not-allowed' : 'pointer',
                fontWeight: 'bold',
                opacity: loading ? 0.5 : 1
              }}
            >
              {loading ? 'LOADING...' : '↻ REFRESH'}
            </button>
          </div>
        </div>

        {/* Function Keys Bar */}
        <div style={{ backgroundColor: colors.panel, borderBottom: `1px solid ${colors.textMuted}`, padding: '2px 4px', flexShrink: 0 }}>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(6, 1fr)', gap: '2px' }}>
            {[
              { key: "F1", label: "OVERVIEW", screen: "overview" },
              { key: "F2", label: "USAGE", screen: "usage" },
              { key: "F3", label: "SETTINGS", screen: "settings" },
              { key: "F4", label: "REFRESH", action: 'refresh' },
              { key: "F5", label: "PRICING", action: 'pricing' },
              { key: "F6", label: "LOGOUT", action: 'logout' }
            ].map(item => (
              <button key={item.key}
                onClick={() => {
                  if (item.action === 'refresh') window.location.reload();
                  else if (item.action === 'extend') extendSession();
                  else if (item.action === 'pricing') navigation.navigateToPricing();
                  else if (item.action === 'logout') logoutUser();
                  else if (item.screen) setCurrentScreen(item.screen as ProfileScreen);
                }}
                disabled={loading && (item.action === 'extend' || item.action === 'refresh')}
                style={{
                  backgroundColor: currentScreen === item.screen ? colors.primary : colors.background,
                  border: `1px solid ${colors.textMuted}`,
                  color: currentScreen === item.screen ? colors.background : colors.text,
                  padding: '2px 4px',
                  fontSize: '12px',
                  height: '16px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  fontWeight: currentScreen === item.screen ? 'bold' : 'normal',
                  cursor: 'pointer'
                }}>
                <span style={{ color: colors.warning }}>{item.key}:</span>
                <span style={{ marginLeft: '2px' }}>{item.label}</span>
              </button>
            ))}
          </div>
        </div>

        {/* Main Content */}
        <div style={{ flex: 1, overflow: 'auto', padding: '4px', backgroundColor: '#050505' }}>
          {currentScreen === 'overview' && (
            <div style={{ display: 'flex', gap: '4px', height: '100%' }}>
              {/* Left Column */}
              <div style={{ flex: 1, backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', overflow: 'auto' }}>
                <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                  SESSION INFORMATION
                </div>
                <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>

                <div style={{ marginBottom: '8px', fontSize: '12px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                    <span style={{ color: colors.textMuted }}>Account Type:</span>
                    <span style={{ color: colors.warning, fontWeight: 'bold' }}>GUEST</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                    <span style={{ color: colors.textMuted }}>Session Expires:</span>
                    <span style={{ color: colors.alert }}>{session?.expires_at ? formatDate(session.expires_at) : 'N/A'}</span>
                  </div>
                  <div style={{ marginBottom: '2px', paddingTop: '4px', borderTop: `1px solid ${colors.textMuted}` }}>
                    <span style={{ color: colors.textMuted }}>Device ID:</span>
                    <div style={{ color: colors.text, fontSize: '11px', marginTop: '2px', wordBreak: 'break-all' }}>
                      {displayDeviceId}
                    </div>
                  </div>
                  <div style={{ marginBottom: '2px', paddingTop: '4px', borderTop: `1px solid ${colors.textMuted}` }}>
                    <span style={{ color: colors.textMuted }}>API Key:</span>
                    <div style={{ color: colors.text, fontSize: '11px', marginTop: '2px', wordBreak: 'break-all' }}>
                      {session?.api_key ? `${session.api_key.substring(0, 40)}...` : 'N/A'}
                    </div>
                  </div>
                </div>

                <div style={{ marginTop: '12px' }}>
                  <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                    DAILY USAGE STATS
                  </div>
                  <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>

                  <div style={{ fontSize: '12px', marginBottom: '6px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                      <span style={{ color: colors.textMuted }}>Today's Requests:</span>
                      <span style={{ color: colors.text, fontWeight: 'bold' }}>{requestsToday}/{dailyLimit}</span>
                    </div>
                    <div style={{ backgroundColor: colors.background, height: '6px', marginBottom: '4px', border: `1px solid ${colors.textMuted}` }}>
                      <div style={{
                        width: `${usagePercent}%`,
                        height: '100%',
                        backgroundColor: remaining > 10 ? colors.secondary : colors.alert,
                        transition: 'width 0.3s'
                      }} />
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                      <span style={{ color: colors.textMuted }}>Remaining:</span>
                      <span style={{ color: remaining > 10 ? colors.secondary : colors.alert, fontWeight: 'bold' }}>{remaining}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: colors.textMuted }}>Status:</span>
                      <span style={{ color: colors.secondary }}>ACTIVE</span>
                    </div>
                  </div>
                </div>
              </div>

              {/* Right Column */}
              <div style={{ flex: 1, backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', overflow: 'auto' }}>
                <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                  UPGRADE TO FULL ACCESS
                </div>
                <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>

                <div style={{ fontSize: '12px', color: colors.text, lineHeight: '1.4', marginBottom: '8px' }}>
                  <div style={{ color: colors.warning, fontWeight: 'bold', marginBottom: '4px' }}>
                    Create a free account to unlock:
                  </div>
                  <div style={{ marginLeft: '8px' }}>
                    <div>- Unlimited API requests</div>
                    <div>- Permanent API key</div>
                    <div>- All database access</div>
                    <div>- Premium features & tools</div>
                    <div>- Data marketplace</div>
                    <div>- Advanced analytics</div>
                  </div>
                </div>

                <button
                  onClick={() => navigation.navigateToPricing()}
                  style={{
                    width: '100%',
                    padding: '6px',
                    backgroundColor: colors.primary,
                    border: 'none',
                    color: colors.background,
                    fontSize: '11px',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}
                >
                  VIEW PRICING PLANS
                </button>
              </div>
            </div>
          )}

          {currentScreen === 'usage' && renderUsageScreen()}
          {currentScreen === 'settings' && renderGuestSettingsScreen()}
        </div>

        {/* Status Bar */}
        <div style={{ borderTop: `1px solid ${colors.textMuted}`, backgroundColor: colors.panel, padding: '2px 8px', fontSize: '11px', color: colors.textMuted, flexShrink: 0 }}>
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <span>FinceptTerminal v3.0.11 | Guest Session</span>
            <span>Device: {session?.device_id?.substring(0, 20) || 'UNKNOWN'} | Status: {loading ? 'UPDATING' : 'READY'}</span>
          </div>
        </div>
      </div>
    );
  };

  // Registered User Profile View
  const renderUserProfile = () => {
    const userInfo = session?.user_info;
    const accountType = userInfo?.account_type || 'free';
    const hasSubscription = (session?.subscription as any)?.data?.has_subscription || session?.subscription?.has_subscription;
    const subscriptionPlan = (session?.subscription as any)?.data?.subscription?.plan || session?.subscription?.subscription?.plan;
    const subscriptionStatus = (session?.subscription as any)?.data?.subscription?.status || session?.subscription?.subscription?.status;
    const creditBalance = userInfo?.credit_balance ?? 0;

    const accountTypeColor = {
      free: colors.textMuted,
      starter: colors.info,
      professional: colors.purple,
      enterprise: colors.primary,
      unlimited: colors.warning
    }[accountType] || colors.text;

    const totalRequests = usageData?.total_requests || 0;
    const todayRequests = usageData?.requests_today || usageData?.today_requests || 0;

    return (
      <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: colors.background, overflow: 'hidden' }}>
        {/* Header */}
        <div style={{ backgroundColor: colors.panel, borderBottom: `1px solid ${colors.textMuted}`, padding: '4px 8px', flexShrink: 0 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px' }}>
            <span style={{ color: colors.primary, fontWeight: 'bold' }}>FINCEPT TERMINAL PROFILE</span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.accent, fontWeight: 'bold' }}>
              {userInfo?.username?.toUpperCase() || userInfo?.email?.split('@')[0]?.toUpperCase() || 'USER'}
            </span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.text }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.textMuted }}>CREDITS:</span>
            <span style={{ color: creditBalance > 0 ? colors.secondary : colors.alert, fontWeight: 'bold' }}>
              {creditBalance.toFixed(2)}
            </span>
            <span style={{ color: colors.text }}>|</span>
            <span style={{ color: colors.textMuted }}>PLAN:</span>
            <span style={{ color: accountTypeColor, fontWeight: 'bold' }}>{accountType.toUpperCase()}</span>
            <button
              onClick={() => fetchData(true)}
              disabled={loading}
              style={{
                marginLeft: 'auto',
                padding: '3px 12px',
                backgroundColor: colors.primary,
                color: colors.background,
                border: 'none',
                borderRadius: '3px',
                fontSize: '11px',
                cursor: loading ? 'not-allowed' : 'pointer',
                fontWeight: 'bold',
                opacity: loading ? 0.5 : 1
              }}
            >
              {loading ? 'LOADING...' : '↻ REFRESH'}
            </button>
          </div>
        </div>

        {/* Function Keys Bar */}
        <div style={{ backgroundColor: colors.panel, borderBottom: `1px solid ${colors.textMuted}`, padding: '2px 4px', flexShrink: 0 }}>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '2px' }}>
            {[
              { key: "F1", label: "OVERVIEW", screen: "overview" },
              { key: "F2", label: "USAGE", screen: "usage" },
              { key: "F3", label: "PAYMENTS", screen: "payments" },
              { key: "F4", label: "SUBS", screen: "subscriptions" },
              { key: "F5", label: "SUPPORT", screen: "support" },
              { key: "F6", label: "SETTINGS", screen: "settings" },
              { key: "F7", label: "CREDITS", action: 'pricing' },
              { key: "F8", label: "REGEN", action: 'regen' },
              { key: "F9", label: "", screen: "" },
              { key: "F10", label: "", screen: "" },
              { key: "F11", label: "FULL", action: 'fullscreen' },
              { key: "F12", label: "LOGOUT", action: 'logout' }
            ].map(item => (
              <button key={item.key}
                onClick={() => {
                  if (item.action === 'logout') logoutUser();
                  else if (item.action === 'pricing') navigation.navigateToPricing();
                  else if (item.action === 'regen') regenerateApiKey();
                  else if (item.action === 'fullscreen') toggleFullscreen();
                  else if (item.screen) setCurrentScreen(item.screen as ProfileScreen);
                }}
                disabled={!item.label || (loading && item.action === 'regen')}
                style={{
                  backgroundColor: currentScreen === item.screen ? colors.primary : colors.background,
                  border: `1px solid ${colors.textMuted}`,
                  color: currentScreen === item.screen ? colors.background : colors.text,
                  padding: '2px 4px',
                  fontSize: '12px',
                  height: '16px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  fontWeight: currentScreen === item.screen ? 'bold' : 'normal',
                  cursor: item.label ? 'pointer' : 'default',
                  opacity: item.label ? 1 : 0.3
                }}>
                <span style={{ color: colors.warning }}>{item.key}:</span>
                {item.label && <span style={{ marginLeft: '2px' }}>{item.label}</span>}
              </button>
            ))}
          </div>
        </div>

        {/* Main Content */}
        <div style={{ flex: 1, overflow: 'auto', padding: '4px', backgroundColor: '#050505' }}>
          {currentScreen === 'overview' && (
            <div style={{ display: 'flex', gap: '4px', height: '100%' }}>
              {/* Left Column */}
              <div style={{ flex: 1, backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', overflow: 'auto' }}>
                {/* Credits Section */}
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                    CREDIT BALANCE
                  </div>
                  <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
                    <div style={{ color: colors.text, fontSize: '24px', fontWeight: 'bold' }}>
                      {creditBalance.toFixed(2)}
                    </div>
                    <button
                      onClick={() => navigation.navigateToPricing()}
                      style={{
                        padding: '4px 8px',
                        backgroundColor: colors.secondary,
                        border: 'none',
                        color: colors.background,
                        fontSize: '12px',
                        fontWeight: 'bold',
                        cursor: 'pointer'
                      }}
                    >
                      ADD CREDITS
                    </button>
                  </div>
                  <div style={{ color: colors.textMuted, fontSize: '11px' }}>
                    Available for API calls and premium features
                  </div>
                </div>

                {/* Account Details */}
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                    ACCOUNT DETAILS
                  </div>
                  <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
                  <div style={{ fontSize: '12px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                      <span style={{ color: colors.textMuted }}>Username:</span>
                      <span style={{ color: colors.text, fontWeight: 'bold' }}>{userInfo?.username || 'N/A'}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                      <span style={{ color: colors.textMuted }}>Email:</span>
                      <span style={{ color: colors.text }}>{userInfo?.email || 'N/A'}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                      <span style={{ color: colors.textMuted }}>Account Type:</span>
                      <span style={{ color: accountTypeColor, fontWeight: 'bold' }}>{accountType.toUpperCase()}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                      <span style={{ color: colors.textMuted }}>Member Since:</span>
                      <span style={{ color: colors.text }}>{formatDate((userInfo as any)?.created_at)}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: colors.textMuted }}>User ID:</span>
                      <span style={{ color: colors.textMuted, fontSize: '11px' }}>#{(userInfo as any)?.id || 'N/A'}</span>
                    </div>
                  </div>
                </div>

                {/* API Key */}
                <div>
                  <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                    API AUTHENTICATION
                  </div>
                  <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
                  <div style={{ fontSize: '12px' }}>
                    <div style={{ color: colors.textMuted, marginBottom: '2px' }}>API Key:</div>
                    <div style={{ backgroundColor: colors.background, padding: '4px', marginBottom: '4px', border: `1px solid ${colors.textMuted}`, fontSize: '11px', wordBreak: 'break-all', color: colors.text }}>
                      {session?.api_key || 'No API key available'}
                    </div>
                    <button
                      onClick={regenerateApiKey}
                      disabled={loading}
                      style={{
                        padding: '4px 8px',
                        backgroundColor: colors.background,
                        border: `1px solid ${colors.purple}`,
                        color: colors.purple,
                        fontSize: '12px',
                        fontWeight: 'bold',
                        cursor: 'pointer',
                        width: '100%'
                      }}
                    >
                      REGENERATE API KEY
                    </button>
                  </div>
                </div>
              </div>

              {/* Right Column */}
              <div style={{ flex: 1, backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', overflow: 'auto' }}>
                {/* Subscription Info */}
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                    SUBSCRIPTION STATUS
                  </div>
                  <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
                  {hasSubscription && subscriptionPlan ? (
                    <div style={{ fontSize: '12px' }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                        <span style={{ color: colors.textMuted }}>Plan:</span>
                        <span style={{ color: colors.secondary, fontWeight: 'bold' }}>{subscriptionPlan.name}</span>
                      </div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                        <span style={{ color: colors.textMuted }}>Price:</span>
                        <span style={{ color: colors.text }}>${subscriptionPlan.price_usd / 100}/mo</span>
                      </div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                        <span style={{ color: colors.textMuted }}>API Calls:</span>
                        <span style={{ color: colors.accent, fontWeight: 'bold' }}>
                          {subscriptionPlan.api_calls_limit === -1 ? 'UNLIMITED' :
                           subscriptionPlan.api_calls_limit != null ? subscriptionPlan.api_calls_limit.toLocaleString() : 'N/A'}
                        </span>
                      </div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                        <span style={{ color: colors.textMuted }}>Status:</span>
                        <span style={{ color: colors.secondary, fontWeight: 'bold' }}>{subscriptionStatus?.toUpperCase()}</span>
                      </div>
                      <div style={{ display: 'flex', gap: '4px' }}>
                        <button
                          onClick={cancelSubscription}
                          disabled={loading}
                          style={{
                            flex: 1,
                            padding: '4px',
                            backgroundColor: colors.background,
                            border: `1px solid ${colors.alert}`,
                            color: colors.alert,
                            fontSize: '11px',
                            fontWeight: 'bold',
                            cursor: 'pointer'
                          }}
                        >
                          CANCEL
                        </button>
                        <button
                          onClick={() => navigation.navigateToPricing()}
                          style={{
                            flex: 1,
                            padding: '4px',
                            backgroundColor: colors.background,
                            border: `1px solid ${colors.info}`,
                            color: colors.info,
                            fontSize: '11px',
                            fontWeight: 'bold',
                            cursor: 'pointer'
                          }}
                        >
                          CHANGE
                        </button>
                      </div>
                    </div>
                  ) : (
                    <div style={{ fontSize: '12px' }}>
                      <div style={{ color: colors.warning, marginBottom: '4px', fontWeight: 'bold' }}>No active subscription</div>
                      <div style={{ color: colors.text, marginBottom: '6px', lineHeight: '1.4' }}>
                        <div>- Unlimited API requests</div>
                        <div>- Priority support</div>
                        <div>- Advanced features</div>
                        <div>- Data marketplace access</div>
                      </div>
                      <button
                        onClick={() => navigation.navigateToPricing()}
                        style={{
                          width: '100%',
                          padding: '6px',
                          backgroundColor: colors.primary,
                          border: 'none',
                          color: colors.background,
                          fontSize: '12px',
                          fontWeight: 'bold',
                          cursor: 'pointer'
                        }}
                      >
                        VIEW PLANS
                      </button>
                    </div>
                  )}
                </div>

                {/* Usage Stats Quick View */}
                <div>
                  <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                    API USAGE SUMMARY
                  </div>
                  <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
                  <div style={{ fontSize: '12px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                      <span style={{ color: colors.textMuted }}>Total Requests:</span>
                      <span style={{ color: colors.text }}>{totalRequests.toLocaleString()}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: colors.textMuted }}>Today's Requests:</span>
                      <span style={{ color: colors.accent }}>{todayRequests}</span>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          )}

          {currentScreen === 'usage' && renderUsageScreen()}
          {currentScreen === 'payments' && renderPaymentsScreen()}
          {currentScreen === 'subscriptions' && renderSubscriptionsScreen()}
          {currentScreen === 'support' && renderSupportScreen()}
          {currentScreen === 'settings' && renderSettingsScreen()}
        </div>

        {/* Status Bar */}
        <div style={{ borderTop: `1px solid ${colors.textMuted}`, backgroundColor: colors.panel, padding: '2px 8px', fontSize: '11px', color: colors.textMuted, flexShrink: 0 }}>
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <span>FinceptTerminal v3.0.11 | User: {userInfo?.username || 'N/A'}</span>
            <span>Account: {accountType.toUpperCase()} | Requests: {todayRequests.toLocaleString()} | Status: {loading ? 'UPDATING' : 'READY'}</span>
          </div>
        </div>
      </div>
    );
  };

  const renderUsageScreen = () => {
    return (
      <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', height: '100%', overflow: 'auto' }}>
        <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
          API USAGE STATISTICS
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
        {loading ? (
          <div style={{ color: colors.textMuted, fontSize: '12px' }}>Loading usage data...</div>
        ) : usageData ? (
          <div style={{ fontSize: '12px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
              <span style={{ color: colors.textMuted }}>Total Requests:</span>
              <span style={{ color: colors.text, fontWeight: 'bold' }}>{usageData.total_requests?.toLocaleString() || 'N/A'}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
              <span style={{ color: colors.textMuted }}>Today's Requests:</span>
              <span style={{ color: colors.accent, fontWeight: 'bold' }}>{usageData.requests_today || usageData.today_requests || 0}</span>
            </div>
            {detailedUsage && (
              <>
                <div style={{ borderTop: `1px solid ${colors.textMuted}`, marginTop: '6px', paddingTop: '6px' }}>
                  <div style={{ color: colors.warning, fontWeight: 'bold', marginBottom: '4px' }}>Detailed Usage</div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
                    <span style={{ color: colors.textMuted }}>This Month:</span>
                    <span style={{ color: colors.text }}>{detailedUsage.this_month?.toLocaleString() || 'N/A'}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: colors.textMuted }}>Last 30 Days:</span>
                    <span style={{ color: colors.text }}>{detailedUsage.last_30_days?.toLocaleString() || 'N/A'}</span>
                  </div>
                </div>
              </>
            )}
          </div>
        ) : (
          <div style={{ color: colors.textMuted, fontSize: '12px' }}>No usage data available</div>
        )}
      </div>
    );
  };

  const renderPaymentsScreen = () => {
    return (
      <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', height: '100%', overflow: 'auto' }}>
        <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
          PAYMENT HISTORY
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
        {loading ? (
          <div style={{ color: colors.textMuted, fontSize: '12px' }}>Loading payment history...</div>
        ) : paymentHistory.length > 0 ? (
          paymentHistory.map((payment: any, idx: number) => (
            <div key={idx} style={{
              backgroundColor: idx % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
              borderLeft: `2px solid ${colors.secondary}`,
              paddingLeft: '4px',
              marginBottom: '4px',
              fontSize: '12px'
            }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                <span style={{ color: colors.textMuted }}>Amount:</span>
                <span style={{ color: colors.secondary, fontWeight: 'bold' }}>
                  {formatCurrency(payment.amount || payment.amount_paid || 0)}
                </span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                <span style={{ color: colors.textMuted }}>Date:</span>
                <span style={{ color: colors.text }}>{formatDate(payment.created_at || payment.payment_date)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: colors.textMuted }}>Status:</span>
                <span style={{ color: payment.status === 'completed' ? colors.secondary : colors.warning, fontWeight: 'bold' }}>
                  {payment.status?.toUpperCase() || 'N/A'}
                </span>
              </div>
            </div>
          ))
        ) : (
          <div style={{ color: colors.textMuted, fontSize: '12px' }}>No payment history found</div>
        )}
      </div>
    );
  };

  const renderSubscriptionsScreen = () => {
    return (
      <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', height: '100%', overflow: 'auto' }}>
        <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
          DATABASE SUBSCRIPTIONS
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
        {loading ? (
          <div style={{ color: colors.textMuted, fontSize: '12px' }}>Loading subscriptions...</div>
        ) : subscriptions.length > 0 ? (
          subscriptions.map((sub: any, idx: number) => (
            <div key={idx} style={{
              backgroundColor: idx % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
              borderLeft: `2px solid ${colors.purple}`,
              paddingLeft: '4px',
              marginBottom: '4px',
              fontSize: '12px'
            }}>
              <div style={{ color: colors.purple, fontWeight: 'bold', marginBottom: '1px' }}>
                {sub.database_name || sub.name || 'Unknown Database'}
              </div>
              <div style={{ color: colors.textMuted }}>
                Subscribed: {formatDate(sub.subscribed_at || sub.created_at)}
              </div>
            </div>
          ))
        ) : (
          <div style={{ color: colors.textMuted, fontSize: '12px' }}>No database subscriptions found</div>
        )}
      </div>
    );
  };

  const renderSupportScreen = () => {
    return (
      <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', height: '100%', overflow: 'auto' }}>
        <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
          SUPPORT TICKETS
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
        {loading ? (
          <div style={{ color: colors.textMuted, fontSize: '12px' }}>Loading tickets...</div>
        ) : supportTickets.length > 0 ? (
          supportTickets.map((ticket: any, idx: number) => (
            <div key={idx} style={{
              backgroundColor: idx % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
              borderLeft: `2px solid ${colors.info}`,
              paddingLeft: '4px',
              marginBottom: '4px',
              fontSize: '12px'
            }}>
              <div style={{ color: colors.info, fontWeight: 'bold', marginBottom: '1px' }}>
                #{ticket.id} - {ticket.subject}
              </div>
              <div style={{ color: colors.textMuted, marginBottom: '2px', fontSize: '11px' }}>
                {ticket.description?.substring(0, 100)}...
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: colors.textMuted }}>Status:</span>
                <span style={{ color: ticket.status === 'closed' ? colors.secondary : colors.warning, fontWeight: 'bold' }}>
                  {ticket.status?.toUpperCase()}
                </span>
              </div>
            </div>
          ))
        ) : (
          <div>
            <div style={{ color: colors.textMuted, fontSize: '12px', marginBottom: '6px' }}>No support tickets found</div>
            <button
              onClick={() => alert('Create ticket feature coming soon')}
              style={{
                padding: '6px',
                backgroundColor: colors.info,
                border: 'none',
                color: colors.background,
                fontSize: '12px',
                fontWeight: 'bold',
                cursor: 'pointer'
              }}
            >
              CREATE NEW TICKET
            </button>
          </div>
        )}
      </div>
    );
  };

  const renderSettingsScreen = () => {
    return (
      <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', height: '100%', overflow: 'auto' }}>
        <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
          ACCOUNT SETTINGS - DANGER ZONE
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
        <div style={{ fontSize: '12px' }}>
          <div style={{ color: colors.warning, fontWeight: 'bold', marginBottom: '4px' }}>WARNING</div>
          <div style={{ color: colors.textMuted, marginBottom: '8px', lineHeight: '1.4' }}>
            Deleting your account is permanent and cannot be undone. All your data, subscriptions, payment history, and API access will be permanently removed.
          </div>
          <button
            onClick={deleteAccount}
            disabled={loading}
            style={{
              padding: '6px 12px',
              backgroundColor: colors.alert,
              border: 'none',
              color: colors.text,
              fontSize: '12px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            DELETE ACCOUNT PERMANENTLY
          </button>
        </div>
      </div>
    );
  };

  const renderGuestSettingsScreen = () => {
    return (
      <div style={{ backgroundColor: colors.panel, border: `1px solid ${colors.textMuted}`, padding: '4px', height: '100%', overflow: 'auto' }}>
        <div style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
          SESSION SETTINGS - DANGER ZONE
        </div>
        <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>
        <div style={{ fontSize: '12px' }}>
          <div style={{ color: colors.warning, fontWeight: 'bold', marginBottom: '4px' }}>WARNING</div>
          <div style={{ color: colors.textMuted, marginBottom: '8px', lineHeight: '1.4' }}>
            Deleting your guest session will remove all your data. This action cannot be undone.
          </div>
          <button
            onClick={async () => {
              if (confirm('Are you sure you want to delete your guest session?')) {
                if (session?.api_key) {
                  setLoading(true);
                  const result = await UserApiService.deleteGuestSession(session.api_key || '');
                  setLoading(false);
                  if (result.success) {
                    alert('Guest session deleted successfully.');
                    await logout();
                  } else {
                    alert(`Failed to delete session: ${result.error}`);
                  }
                }
              }
            }}
            disabled={loading}
            style={{
              padding: '6px 12px',
              backgroundColor: colors.alert,
              border: 'none',
              color: colors.text,
              fontSize: '12px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            DELETE SESSION PERMANENTLY
          </button>
        </div>
      </div>
    );
  };

  // Main render
  if (!session) {
    return (
      <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: colors.background, color: colors.text }}>
        <div style={{ textAlign: 'center', fontSize: '12px' }}>
          <div style={{ color: colors.alert, marginBottom: '8px', fontWeight: 'bold' }}>NO SESSION</div>
          <div style={{ color: colors.textMuted }}>Unable to load profile</div>
        </div>
      </div>
    );
  }

  return session.user_type === 'guest' ? renderGuestProfile() : renderUserProfile();
};

export default ProfileTab;
