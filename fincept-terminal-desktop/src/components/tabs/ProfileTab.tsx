import React, { useState, useEffect } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { useNavigation } from '@/contexts/NavigationContext';
import { UserApiService } from '@/services/userApi';

// Bloomberg Terminal Colors
const C = {
  ORANGE: '#FFA500',
  WHITE: '#FFFFFF',
  RED: '#FF0000',
  GREEN: '#00C800',
  YELLOW: '#FFFF00',
  GRAY: '#787878',
  BLUE: '#6496FA',
  PURPLE: '#C864FF',
  CYAN: '#00FFFF',
  DARK_BG: '#000000',
  PANEL_BG: '#0a0a0a'
};

type ProfileScreen = 'overview' | 'usage' | 'payments' | 'subscriptions' | 'support' | 'settings';

const ProfileTab: React.FC = () => {
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

  // Fetch data based on current screen
  useEffect(() => {
    if (!session?.api_key) return;

    const fetchData = async () => {
      setLoading(true);
      try {
        if (currentScreen === 'usage') {
          if (session.user_type === 'guest') {
            const result = await UserApiService.getGuestUsage(session.api_key || '');
            if (result.success) setUsageData(result.data);
          } else {
            const result = await UserApiService.getUserUsage(session.api_key || '');
            if (result.success) setUsageData(result.data);

            // Also fetch detailed usage
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
      } catch (error) {
        console.error('Error fetching data:', error);
      } finally {
        setLoading(false);
      }
    };

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

  const reactivateSubscription = async () => {
    if (!session?.api_key) return;
    setLoading(true);
    const result = await UserApiService.reactivateSubscription(session.api_key || '');
    setLoading(false);
    if (result.success) {
      alert('Subscription reactivated successfully!');
    } else {
      alert(`Failed to reactivate subscription: ${result.error}`);
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

  // Navigation buttons
  const renderNavigationButtons = () => {
    const buttons: { id: ProfileScreen; label: string; color: string }[] = [
      { id: 'overview', label: 'OVERVIEW', color: C.ORANGE },
      { id: 'usage', label: 'USAGE STATS', color: C.CYAN },
      { id: 'payments', label: 'PAYMENTS', color: C.GREEN },
      { id: 'subscriptions', label: 'SUBSCRIPTIONS', color: C.PURPLE },
      { id: 'support', label: 'SUPPORT', color: C.BLUE },
      { id: 'settings', label: 'SETTINGS', color: C.RED },
    ];

    return (
      <div style={{ display: 'flex', gap: '8px', marginBottom: '16px', flexWrap: 'wrap' }}>
        {buttons.map((btn) => (
          <button
            key={btn.id}
            onClick={() => setCurrentScreen(btn.id)}
            style={{
              padding: '8px 16px',
              backgroundColor: currentScreen === btn.id ? btn.color : C.DARK_BG,
              border: `2px solid ${btn.color}`,
              color: currentScreen === btn.id ? C.DARK_BG : btn.color,
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer',
              fontFamily: 'Consolas, monospace',
              transition: 'all 0.2s'
            }}
          >
            {btn.label}
          </button>
        ))}
      </div>
    );
  };

  const renderGuestProfile = () => {
    const displayDeviceId = session?.device_id?.substring(0, 30) + "..." || 'Unknown';
    const requestsToday = session?.requests_today || usageData?.requests_today || 0;
    const dailyLimit = session?.daily_limit || usageData?.daily_limit || 50;
    const remaining = Math.max(0, dailyLimit - requestsToday);
    const usagePercent = (requestsToday / dailyLimit) * 100;

    return (
      <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: C.DARK_BG }}>
        {/* Header */}
        <div style={{ backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}`, padding: '12px 16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
              <span style={{ color: C.ORANGE, fontWeight: 'bold', fontSize: '16px', fontFamily: 'Consolas, monospace' }}>
                👤 GUEST PROFILE
              </span>
              <span style={{ color: C.GRAY }}>|</span>
              <span style={{ color: C.CYAN, fontSize: '12px', fontFamily: 'Consolas, monospace' }}>
                {currentTime.toLocaleTimeString()}
              </span>
            </div>
            <button
              onClick={logoutUser}
              style={{
                padding: '6px 12px',
                backgroundColor: C.DARK_BG,
                border: `2px solid ${C.RED}`,
                color: C.RED,
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}
            >
              🚪 LOGOUT
            </button>
          </div>
        </div>

        {/* Content */}
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {renderNavigationButtons()}

          {currentScreen === 'overview' && (
            <>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '16px', marginBottom: '16px' }}>
                {/* Session Info */}
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.BLUE}`, padding: '16px' }}>
                  <div style={{ color: C.BLUE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
                    SESSION INFORMATION
                  </div>
                  <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace', lineHeight: '1.8' }}>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>Account Type:</span>
                      <span style={{ color: C.YELLOW, marginLeft: '8px', fontWeight: 'bold' }}>GUEST USER</span>
                    </div>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>Device ID:</span>
                      <div style={{ color: C.WHITE, marginTop: '4px', fontSize: '10px' }}>{displayDeviceId}</div>
                    </div>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>API Key:</span>
                      <div style={{ color: C.WHITE, marginTop: '4px', fontSize: '10px' }}>
                        {session?.api_key ? `${session.api_key.substring(0, 20)}...` : 'N/A'}
                      </div>
                    </div>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>Session Expires:</span>
                      <span style={{ color: C.ORANGE, marginLeft: '8px' }}>
                        {session?.expires_at ? formatDate(session.expires_at) : 'N/A'}
                      </span>
                    </div>
                    <button
                      onClick={extendSession}
                      disabled={loading}
                      style={{
                        marginTop: '12px',
                        padding: '8px 16px',
                        backgroundColor: C.DARK_BG,
                        border: `2px solid ${C.CYAN}`,
                        color: C.CYAN,
                        fontSize: '11px',
                        fontWeight: 'bold',
                        cursor: 'pointer',
                        fontFamily: 'Consolas, monospace',
                        width: '100%'
                      }}
                    >
                      ⏰ EXTEND SESSION
                    </button>
                  </div>
                </div>

                {/* Usage Stats */}
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.ORANGE}`, padding: '16px' }}>
                  <div style={{ color: C.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
                    DAILY USAGE
                  </div>
                  <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace', lineHeight: '1.8' }}>
                    <div style={{ marginBottom: '12px' }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                        <span style={{ color: C.GRAY }}>Today's Requests:</span>
                        <span style={{ color: C.WHITE, fontWeight: 'bold' }}>{requestsToday} / {dailyLimit}</span>
                      </div>
                      <div style={{ backgroundColor: C.DARK_BG, height: '8px', borderRadius: '4px', overflow: 'hidden' }}>
                        <div
                          style={{
                            width: `${usagePercent}%`,
                            height: '100%',
                            backgroundColor: remaining > 10 ? C.GREEN : C.RED,
                            transition: 'width 0.3s'
                          }}
                        />
                      </div>
                    </div>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>Remaining:</span>
                      <span style={{ color: remaining > 10 ? C.GREEN : C.RED, marginLeft: '8px', fontWeight: 'bold' }}>
                        {remaining} requests
                      </span>
                    </div>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>Status:</span>
                      <span style={{ color: C.GREEN, marginLeft: '8px', fontWeight: 'bold' }}>● ACTIVE</span>
                    </div>
                  </div>
                </div>
              </div>

              {/* Features */}
              <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.GREEN}`, padding: '16px', marginBottom: '16px' }}>
                <div style={{ color: C.GREEN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
                  ✓ GUEST ACCESS FEATURES
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', color: C.WHITE, fontSize: '11px', fontFamily: 'Consolas, monospace' }}>
                  <div>✓ Basic market data</div>
                  <div>✓ Real-time quotes</div>
                  <div>✓ Public databases</div>
                  <div>✓ Forum access</div>
                  <div>✓ News feeds</div>
                  <div>✓ Basic charts</div>
                </div>
              </div>

              {/* Upgrade Section */}
              <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.ORANGE}`, borderLeft: `6px solid ${C.ORANGE}`, padding: '16px' }}>
                <div style={{ color: C.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
                  🔑 UPGRADE TO FULL ACCESS
                </div>
                <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace', marginBottom: '16px', lineHeight: '1.8' }}>
                  <div style={{ color: C.YELLOW, marginBottom: '8px' }}>Create a free account to unlock:</div>
                  <div>• Unlimited API requests</div>
                  <div>• Permanent API key</div>
                  <div>• All database access</div>
                  <div>• Premium features & tools</div>
                  <div>• Data marketplace</div>
                  <div>• Advanced analytics</div>
                </div>
                <div style={{ display: 'flex', gap: '12px' }}>
                  <button
                    onClick={() => navigation.navigateToPricing()}
                    style={{
                      padding: '10px 20px',
                      backgroundColor: C.ORANGE,
                      border: 'none',
                      color: C.DARK_BG,
                      fontSize: '12px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      fontFamily: 'Consolas, monospace',
                      flex: 1
                    }}
                  >
                    📦 VIEW PLANS
                  </button>
                </div>
              </div>
            </>
          )}

          {currentScreen === 'usage' && renderUsageScreen()}
          {currentScreen === 'settings' && renderGuestSettingsScreen()}
        </div>
      </div>
    );
  };

  const renderUserProfile = () => {
    const userInfo = session?.user_info;
    const accountType = userInfo?.account_type || 'free';
    const hasSubscription = (session?.subscription as any)?.data?.has_subscription || session?.subscription?.has_subscription;
    const subscriptionPlan = (session?.subscription as any)?.data?.subscription?.plan || session?.subscription?.subscription?.plan;
    const subscriptionStatus = (session?.subscription as any)?.data?.subscription?.status || session?.subscription?.subscription?.status;

    const accountTypeColor = {
      free: C.GRAY,
      starter: C.BLUE,
      professional: C.PURPLE,
      enterprise: C.ORANGE,
      unlimited: C.YELLOW
    }[accountType] || C.WHITE;

    return (
      <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: C.DARK_BG }}>
        {/* Header */}
        <div style={{ backgroundColor: C.PANEL_BG, borderBottom: `2px solid ${C.GRAY}`, padding: '12px 16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
              <span style={{ color: C.ORANGE, fontWeight: 'bold', fontSize: '16px', fontFamily: 'Consolas, monospace' }}>
                🔑 {userInfo?.username?.toUpperCase() || 'USER'} PROFILE
              </span>
              <span style={{ color: C.GRAY }}>|</span>
              <span style={{ color: C.CYAN, fontSize: '12px', fontFamily: 'Consolas, monospace' }}>
                {currentTime.toLocaleTimeString()}
              </span>
            </div>
            <button
              onClick={logoutUser}
              style={{
                padding: '6px 12px',
                backgroundColor: C.DARK_BG,
                border: `2px solid ${C.RED}`,
                color: C.RED,
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}
            >
              🚪 LOGOUT
            </button>
          </div>
        </div>

        {/* Content */}
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {renderNavigationButtons()}

          {currentScreen === 'overview' && (
            <>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '16px', marginBottom: '16px' }}>
                {/* Account Details */}
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.BLUE}`, padding: '16px' }}>
                  <div style={{ color: C.BLUE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
                    ACCOUNT DETAILS
                  </div>
                  <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace', lineHeight: '1.8' }}>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>Username:</span>
                      <span style={{ color: C.WHITE, marginLeft: '8px', fontWeight: 'bold' }}>{userInfo?.username || 'N/A'}</span>
                    </div>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>Email:</span>
                      <span style={{ color: C.WHITE, marginLeft: '8px' }}>{userInfo?.email || 'N/A'}</span>
                    </div>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>Account Type:</span>
                      <span style={{ color: accountTypeColor, marginLeft: '8px', fontWeight: 'bold' }}>
                        {accountType.toUpperCase()}
                      </span>
                    </div>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>Member Since:</span>
                      <span style={{ color: C.WHITE, marginLeft: '8px' }}>
                        {formatDate((userInfo as any)?.created_at)}
                      </span>
                    </div>
                    <div style={{ marginBottom: '8px' }}>
                      <span style={{ color: C.GRAY }}>User ID:</span>
                      <span style={{ color: C.GRAY, marginLeft: '8px', fontSize: '10px' }}>#{(userInfo as any)?.id || 'N/A'}</span>
                    </div>
                  </div>
                </div>

                {/* Subscription Info */}
                <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${hasSubscription ? C.GREEN : C.ORANGE}`, padding: '16px' }}>
                  <div style={{ color: hasSubscription ? C.GREEN : C.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
                    {hasSubscription ? '✓ SUBSCRIPTION ACTIVE' : '📦 NO SUBSCRIPTION'}
                  </div>
                  <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace', lineHeight: '1.8' }}>
                    {hasSubscription && subscriptionPlan ? (
                      <>
                        <div style={{ marginBottom: '8px' }}>
                          <span style={{ color: C.GRAY }}>Plan:</span>
                          <span style={{ color: C.GREEN, marginLeft: '8px', fontWeight: 'bold' }}>{subscriptionPlan.name}</span>
                        </div>
                        <div style={{ marginBottom: '8px' }}>
                          <span style={{ color: C.GRAY }}>Price:</span>
                          <span style={{ color: C.WHITE, marginLeft: '8px' }}>${subscriptionPlan.price_usd / 100}/month</span>
                        </div>
                        <div style={{ marginBottom: '8px' }}>
                          <span style={{ color: C.GRAY }}>API Calls:</span>
                          <span style={{ color: C.CYAN, marginLeft: '8px', fontWeight: 'bold' }}>
                            {subscriptionPlan.api_calls_limit === -1 ? 'UNLIMITED' :
                             subscriptionPlan.api_calls_limit != null ? subscriptionPlan.api_calls_limit.toLocaleString() : 'N/A'}
                          </span>
                        </div>
                        <div style={{ marginBottom: '8px' }}>
                          <span style={{ color: C.GRAY }}>Status:</span>
                          <span style={{ color: C.GREEN, marginLeft: '8px', fontWeight: 'bold' }}>● {subscriptionStatus?.toUpperCase()}</span>
                        </div>
                        <div style={{ display: 'flex', gap: '8px', marginTop: '12px' }}>
                          <button
                            onClick={cancelSubscription}
                            disabled={loading}
                            style={{
                              flex: 1,
                              padding: '8px',
                              backgroundColor: C.DARK_BG,
                              border: `2px solid ${C.RED}`,
                              color: C.RED,
                              fontSize: '10px',
                              fontWeight: 'bold',
                              cursor: 'pointer',
                              fontFamily: 'Consolas, monospace'
                            }}
                          >
                            CANCEL
                          </button>
                          <button
                            onClick={() => navigation.navigateToPricing()}
                            style={{
                              flex: 1,
                              padding: '8px',
                              backgroundColor: C.DARK_BG,
                              border: `2px solid ${C.BLUE}`,
                              color: C.BLUE,
                              fontSize: '10px',
                              fontWeight: 'bold',
                              cursor: 'pointer',
                              fontFamily: 'Consolas, monospace'
                            }}
                          >
                            CHANGE
                          </button>
                        </div>
                      </>
                    ) : (
                      <>
                        <div style={{ color: C.YELLOW, marginBottom: '8px' }}>Upgrade to unlock:</div>
                        <div>• Unlimited API requests</div>
                        <div>• Priority support</div>
                        <div>• Advanced features</div>
                        <div>• Data marketplace access</div>
                        <button
                          onClick={() => navigation.navigateToPricing()}
                          style={{
                            marginTop: '12px',
                            padding: '10px',
                            backgroundColor: C.ORANGE,
                            border: 'none',
                            color: C.DARK_BG,
                            fontSize: '11px',
                            fontWeight: 'bold',
                            cursor: 'pointer',
                            fontFamily: 'Consolas, monospace',
                            width: '100%'
                          }}
                        >
                          📦 VIEW PLANS
                        </button>
                      </>
                    )}
                  </div>
                </div>
              </div>

              {/* API Key */}
              <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.PURPLE}`, padding: '16px', marginBottom: '16px' }}>
                <div style={{ color: C.PURPLE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
                  🔐 API AUTHENTICATION
                </div>
                <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace' }}>
                  <div style={{ marginBottom: '8px' }}>
                    <span style={{ color: C.GRAY }}>API Key:</span>
                  </div>
                  <div style={{ backgroundColor: C.DARK_BG, padding: '8px', marginBottom: '12px', border: `1px solid ${C.GRAY}`, fontSize: '10px', wordBreak: 'break-all' }}>
                    {session?.api_key || 'No API key available'}
                  </div>
                  <button
                    onClick={regenerateApiKey}
                    disabled={loading}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: C.DARK_BG,
                      border: `2px solid ${C.PURPLE}`,
                      color: C.PURPLE,
                      fontSize: '11px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      fontFamily: 'Consolas, monospace'
                    }}
                  >
                    🔄 REGENERATE API KEY
                  </button>
                </div>
              </div>

              {/* Quick Actions */}
              <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.ORANGE}`, padding: '16px' }}>
                <div style={{ color: C.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
                  ⚡ QUICK ACTIONS
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
                  <button
                    onClick={() => setCurrentScreen('usage')}
                    style={{
                      padding: '12px',
                      backgroundColor: C.DARK_BG,
                      border: `2px solid ${C.CYAN}`,
                      color: C.CYAN,
                      fontSize: '11px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      fontFamily: 'Consolas, monospace'
                    }}
                  >
                    📊 USAGE STATS
                  </button>
                  <button
                    onClick={() => setCurrentScreen('payments')}
                    style={{
                      padding: '12px',
                      backgroundColor: C.DARK_BG,
                      border: `2px solid ${C.GREEN}`,
                      color: C.GREEN,
                      fontSize: '11px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      fontFamily: 'Consolas, monospace'
                    }}
                  >
                    💳 PAYMENT HISTORY
                  </button>
                  <button
                    onClick={() => setCurrentScreen('subscriptions')}
                    style={{
                      padding: '12px',
                      backgroundColor: C.DARK_BG,
                      border: `2px solid ${C.PURPLE}`,
                      color: C.PURPLE,
                      fontSize: '11px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      fontFamily: 'Consolas, monospace'
                    }}
                  >
                    💾 SUBSCRIPTIONS
                  </button>
                  <button
                    onClick={() => setCurrentScreen('support')}
                    style={{
                      padding: '12px',
                      backgroundColor: C.DARK_BG,
                      border: `2px solid ${C.BLUE}`,
                      color: C.BLUE,
                      fontSize: '11px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      fontFamily: 'Consolas, monospace'
                    }}
                  >
                    🎫 SUPPORT
                  </button>
                </div>
              </div>
            </>
          )}

          {currentScreen === 'usage' && renderUsageScreen()}
          {currentScreen === 'payments' && renderPaymentsScreen()}
          {currentScreen === 'subscriptions' && renderSubscriptionsScreen()}
          {currentScreen === 'support' && renderSupportScreen()}
          {currentScreen === 'settings' && renderSettingsScreen()}
        </div>
      </div>
    );
  };

  const renderUsageScreen = () => {
    return (
      <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.CYAN}`, padding: '16px' }}>
        <div style={{ color: C.CYAN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
          📊 API USAGE STATISTICS
        </div>
        {loading ? (
          <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px' }}>Loading usage data...</div>
        ) : usageData ? (
          <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace', lineHeight: '1.8' }}>
            <div style={{ marginBottom: '8px' }}>
              <span style={{ color: C.GRAY }}>Total Requests:</span>
              <span style={{ color: C.WHITE, marginLeft: '8px', fontWeight: 'bold' }}>
                {usageData.total_requests?.toLocaleString() || 'N/A'}
              </span>
            </div>
            <div style={{ marginBottom: '8px' }}>
              <span style={{ color: C.GRAY }}>Today's Requests:</span>
              <span style={{ color: C.CYAN, marginLeft: '8px', fontWeight: 'bold' }}>
                {usageData.requests_today || usageData.today_requests || 0}
              </span>
            </div>
            {detailedUsage && (
              <>
                <div style={{ marginTop: '16px', color: C.ORANGE, fontWeight: 'bold' }}>Detailed Usage:</div>
                <div style={{ marginTop: '8px' }}>
                  <span style={{ color: C.GRAY }}>This Month:</span>
                  <span style={{ color: C.WHITE, marginLeft: '8px' }}>
                    {detailedUsage.this_month?.toLocaleString() || 'N/A'}
                  </span>
                </div>
                <div style={{ marginTop: '8px' }}>
                  <span style={{ color: C.GRAY }}>Last 30 Days:</span>
                  <span style={{ color: C.WHITE, marginLeft: '8px' }}>
                    {detailedUsage.last_30_days?.toLocaleString() || 'N/A'}
                  </span>
                </div>
              </>
            )}
          </div>
        ) : (
          <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px' }}>No usage data available</div>
        )}
      </div>
    );
  };

  const renderPaymentsScreen = () => {
    return (
      <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.GREEN}`, padding: '16px' }}>
        <div style={{ color: C.GREEN, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
          💳 PAYMENT HISTORY
        </div>
        {loading ? (
          <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px' }}>Loading payment history...</div>
        ) : paymentHistory.length > 0 ? (
          <div style={{ maxHeight: '400px', overflowY: 'auto' }}>
            {paymentHistory.map((payment: any, idx: number) => (
              <div
                key={idx}
                style={{
                  backgroundColor: C.DARK_BG,
                  border: `1px solid ${C.GRAY}`,
                  padding: '12px',
                  marginBottom: '8px',
                  fontFamily: 'Consolas, monospace',
                  fontSize: '11px'
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                  <span style={{ color: C.GRAY }}>Amount:</span>
                  <span style={{ color: C.GREEN, fontWeight: 'bold' }}>
                    {formatCurrency(payment.amount || payment.amount_paid || 0)}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                  <span style={{ color: C.GRAY }}>Date:</span>
                  <span style={{ color: C.WHITE }}>{formatDate(payment.created_at || payment.payment_date)}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: C.GRAY }}>Status:</span>
                  <span style={{ color: payment.status === 'completed' ? C.GREEN : C.YELLOW }}>
                    {payment.status?.toUpperCase() || 'N/A'}
                  </span>
                </div>
              </div>
            ))}
          </div>
        ) : (
          <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px' }}>No payment history found</div>
        )}
      </div>
    );
  };

  const renderSubscriptionsScreen = () => {
    return (
      <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.PURPLE}`, padding: '16px' }}>
        <div style={{ color: C.PURPLE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
          💾 DATABASE SUBSCRIPTIONS
        </div>
        {loading ? (
          <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px' }}>Loading subscriptions...</div>
        ) : subscriptions.length > 0 ? (
          <div>
            {subscriptions.map((sub: any, idx: number) => (
              <div
                key={idx}
                style={{
                  backgroundColor: C.DARK_BG,
                  border: `1px solid ${C.GRAY}`,
                  padding: '12px',
                  marginBottom: '8px',
                  fontFamily: 'Consolas, monospace',
                  fontSize: '11px'
                }}
              >
                <div style={{ color: C.PURPLE, fontWeight: 'bold', marginBottom: '4px' }}>
                  {sub.database_name || sub.name || 'Unknown Database'}
                </div>
                <div style={{ color: C.GRAY }}>
                  Subscribed: {formatDate(sub.subscribed_at || sub.created_at)}
                </div>
              </div>
            ))}
          </div>
        ) : (
          <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px' }}>No database subscriptions found</div>
        )}
      </div>
    );
  };

  const renderSupportScreen = () => {
    return (
      <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.BLUE}`, padding: '16px' }}>
        <div style={{ color: C.BLUE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
          🎫 SUPPORT TICKETS
        </div>
        {loading ? (
          <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px' }}>Loading tickets...</div>
        ) : supportTickets.length > 0 ? (
          <div style={{ maxHeight: '400px', overflowY: 'auto' }}>
            {supportTickets.map((ticket: any, idx: number) => (
              <div
                key={idx}
                style={{
                  backgroundColor: C.DARK_BG,
                  border: `1px solid ${C.GRAY}`,
                  padding: '12px',
                  marginBottom: '8px',
                  fontFamily: 'Consolas, monospace',
                  fontSize: '11px'
                }}
              >
                <div style={{ color: C.BLUE, fontWeight: 'bold', marginBottom: '4px' }}>
                  #{ticket.id} - {ticket.subject}
                </div>
                <div style={{ color: C.GRAY, marginBottom: '4px' }}>{ticket.description?.substring(0, 100)}...</div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: C.GRAY }}>Status:</span>
                  <span style={{ color: ticket.status === 'closed' ? C.GREEN : C.YELLOW }}>
                    {ticket.status?.toUpperCase()}
                  </span>
                </div>
              </div>
            ))}
          </div>
        ) : (
          <div>
            <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px', marginBottom: '16px' }}>
              No support tickets found
            </div>
            <button
              onClick={() => alert('Create ticket feature coming soon')}
              style={{
                padding: '10px 20px',
                backgroundColor: C.BLUE,
                border: 'none',
                color: C.DARK_BG,
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}
            >
              + CREATE NEW TICKET
            </button>
          </div>
        )}
      </div>
    );
  };

  const renderSettingsScreen = () => {
    return (
      <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.RED}`, padding: '16px' }}>
        <div style={{ color: C.RED, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
          ⚙️ ACCOUNT SETTINGS
        </div>
        <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace' }}>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ color: C.YELLOW, fontWeight: 'bold', marginBottom: '8px' }}>⚠️ DANGER ZONE</div>
            <div style={{ color: C.GRAY, marginBottom: '12px' }}>
              Deleting your account is permanent and cannot be undone. All your data will be lost.
            </div>
            <button
              onClick={deleteAccount}
              disabled={loading}
              style={{
                padding: '10px 20px',
                backgroundColor: C.RED,
                border: 'none',
                color: C.WHITE,
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}
            >
              🗑️ DELETE ACCOUNT
            </button>
          </div>
        </div>
      </div>
    );
  };

  const renderGuestSettingsScreen = () => {
    return (
      <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.RED}`, padding: '16px' }}>
        <div style={{ color: C.RED, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
          ⚙️ SESSION SETTINGS
        </div>
        <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace' }}>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ color: C.YELLOW, fontWeight: 'bold', marginBottom: '8px' }}>⚠️ DANGER ZONE</div>
            <div style={{ color: C.GRAY, marginBottom: '12px' }}>
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
                padding: '10px 20px',
                backgroundColor: C.RED,
                border: 'none',
                color: C.WHITE,
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}
            >
              🗑️ DELETE SESSION
            </button>
          </div>
        </div>
      </div>
    );
  };

  // Main render
  if (!session) {
    return (
      <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: C.DARK_BG, color: C.WHITE, fontFamily: 'Consolas, monospace' }}>
        <div style={{ textAlign: 'center' }}>
          <div style={{ fontSize: '16px', color: C.ORANGE, marginBottom: '12px' }}>❌ NO SESSION</div>
          <div style={{ fontSize: '12px', color: C.GRAY }}>Unable to load profile</div>
        </div>
      </div>
    );
  }

  return session.user_type === 'guest' ? renderGuestProfile() : renderUserProfile();
};

export default ProfileTab;
