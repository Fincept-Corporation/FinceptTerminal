import React, { useState, useEffect } from 'react';
import { useAuth } from '@/contexts/AuthContext';

const ProfileTab: React.FC = () => {
  const { session, logout } = useAuth();

  const [lastRefresh, setLastRefresh] = useState<Date>(new Date());
  const [requestCount, setRequestCount] = useState(15);
  const [usageStats, setUsageStats] = useState({
    total_requests: 1250,
    total_credits_used: 3425
  });

  useEffect(() => {
    const timer = setInterval(() => {
      setLastRefresh(new Date());
    }, 5000);

    return () => clearInterval(timer);
  }, []);

  // Bloomberg Terminal Colors (matching DearPyGUI style)
  const COLORS = {
    success: '#64ff64',
    warning: '#ffc864',
    error: '#ff9696',
    info: '#c8ffc8',
    title: '#ffff64'
  };

  const manualRefresh = () => {
    setLastRefresh(new Date());
    setRequestCount(prev => prev + 1);
  };

  const logoutUser = async () => {
    await logout();
    window.location.href = '/';
  };

  const showSignupInfo = () => {
    alert('Sign up functionality would open here');
  };

  const showLoginInfo = () => {
    alert('Login functionality would open here');
  };

  const regenerateApiKey = () => {
    const newKey = `fk_user_${Math.random().toString(36).substring(2, 15)}`;
    setSessionData(prev => ({ ...prev, api_key: newKey }));
  };

  // Header component matching DearPyGUI style
  const Header = ({ title, lastRefresh }: { title: string, lastRefresh: Date }) => (
    <div className="mb-5">
      <div className="flex items-center gap-5 mb-2">
        <span style={{ color: COLORS.title }} className="text-lg font-mono">{title}</span>
        <span style={{ color: COLORS.info }} className="text-sm font-mono">
          🔄 Refreshed: {lastRefresh.toLocaleTimeString()}
        </span>
        <button
          onClick={manualRefresh}
          className="bg-gray-700 hover:bg-gray-600 text-white px-3 py-1 text-xs font-mono border border-gray-600"
        >
          🔄 Refresh
        </button>
        <button
          onClick={logoutUser}
          className="bg-gray-700 hover:bg-gray-600 text-white px-3 py-1 text-xs font-mono border border-gray-600"
        >
          🚪 Logout
        </button>
      </div>
      <div className="border-t border-gray-600"></div>
    </div>
  );

  // Info Widget component matching DearPyGUI child window style
  const InfoWidget = ({ title, children, width = 350, height = 450 }: {
    title: string,
    children: React.ReactNode,
    width?: number,
    height?: number
  }) => (
    <div
      className="border border-gray-600 bg-black p-3"
      style={{ width: `${width}px`, height: `${height}px`, overflow: 'auto' }}
    >
      <div style={{ color: COLORS.title }} className="text-sm font-mono mb-2">{title}</div>
      <div className="border-t border-gray-600 mb-3"></div>
      <div className="space-y-2">
        {children}
      </div>
    </div>
  );

  // Two column layout matching DearPyGUI horizontal group
  const TwoColumnLayout = ({ left, right }: { left: React.ReactNode, right: React.ReactNode }) => (
    <div className="flex gap-5">
      {left}
      {right}
    </div>
  );

  // Button group component
  const ButtonGroup = ({ buttons }: { buttons: Array<{ label: string, callback: () => void }> }) => (
    <div className="space-y-1">
      {buttons.map((button, index) => (
        <div key={index}>
          <button
            onClick={button.callback}
            className="bg-gray-700 hover:bg-gray-600 text-white px-4 py-2 text-xs font-mono border border-gray-600 w-70"
            style={{ width: '280px' }}
          >
            {button.label}
          </button>
        </div>
      ))}
    </div>
  );

  // Text line component with optional color
  const TextLine = ({ text, color }: { text: string, color?: string }) => (
    <div style={{ color: color || '#ffffff' }} className="text-xs font-mono leading-relaxed">
      {text}
    </div>
  );

  const Spacer = ({ height = 10 }: { height?: number }) => (
    <div style={{ height: `${height}px` }}></div>
  );

  // Get API key display format
  const getApiKeyInfo = (apiKey?: string, isUser = false) => {
    if (!apiKey) return "No API key";

    if (isUser) {
      return `API Key: ${apiKey.substring(0, 12)}...`;
    }

    if (apiKey.startsWith("fk_guest_")) {
      return `Guest API Key: ${apiKey.substring(0, 15)}...`;
    }

    return "No API key";
  };

  // Format date
  const formatDate = (dateStr?: string) => {
    if (!dateStr) return 'N/A';
    return new Date(dateStr).toLocaleDateString();
  };

  const renderGuestProfile = () => {
    if (!session) return null;
    const displayDeviceId = session.device_id?.substring(0, 20) + "..." || 'Unknown';
    const requests_today = session.requests_today || 0;
    const daily_limit = session.daily_limit || 50;
    const remaining = Math.max(0, daily_limit - requests_today);

    return (
      <div className="p-4">
        <Header title="👤 Guest Profile" lastRefresh={lastRefresh} />

        <TwoColumnLayout
          left={
            <InfoWidget title="Current Session Status">
              <TextLine text="Account Type: Guest User" />
              <TextLine text={`Device ID: ${displayDeviceId}`} />
              <Spacer />
              <TextLine text={getApiKeyInfo(session.api_key || undefined)} />
              <Spacer />
              <TextLine text={`Session Requests: ${requestCount}`} />
              <TextLine text={`Today's Requests: ${requests_today}/${daily_limit}`} />
              <TextLine
                text={`Remaining Today: ${remaining}`}
                color={remaining > 10 ? COLORS.success : COLORS.error}
              />
              <Spacer />
              <TextLine text="✓ Basic market data" />
              <TextLine text="✓ Real-time quotes" />
              <TextLine text="✓ Public databases" />
            </InfoWidget>
          }
          right={
            <InfoWidget title="Upgrade Your Access">
              <TextLine
                text={session.api_key ? "🔄 Current: Guest API Key" : "🔄 Current: Offline Mode"}
                color={COLORS.warning}
              />
              <Spacer />
              {session.api_key ? (
                <>
                  <TextLine text="• Temporary access (24 hours)" />
                  <TextLine text="• 50 requests per day" />
                </>
              ) : (
                <TextLine text="• No API access" />
              )}
              <Spacer />
              <TextLine text="🔑 Create Account" color={COLORS.info} />
              <TextLine text="Get unlimited access:" />
              <TextLine text="• Permanent API key" />
              <TextLine text="• Unlimited requests" />
              <TextLine text="• All databases access" />
              <TextLine text="• Premium features" />
              <Spacer height={20} />
              <ButtonGroup buttons={[
                { label: "Create Free Account", callback: showSignupInfo },
                { label: "Sign In to Account", callback: showLoginInfo }
              ]} />
            </InfoWidget>
          }
        />

        <Spacer height={20} />

        {/* Session Stats - Full Width */}
        <InfoWidget title="Session Statistics" width={750} height={150}>
          <div className="grid grid-cols-4 gap-4 text-xs font-mono">
            <div>
              <TextLine text="Request Count:" color={COLORS.info} />
              <TextLine text={requestCount.toString()} />
            </div>
            <div>
              <TextLine text="Daily Usage:" color={COLORS.info} />
              <TextLine text={`${requests_today}/${daily_limit}`} />
            </div>
            <div>
              <TextLine text="Status:" color={COLORS.info} />
              <TextLine text="Active" color={COLORS.success} />
            </div>
            <div>
              <TextLine text="Session Time:" color={COLORS.info} />
              <TextLine text="00:45:23" />
            </div>
          </div>
        </InfoWidget>
      </div>
    );
  };

  const renderUserProfile = () => {
    if (!session) return null;
    const userInfo = session.user_info;
    const creditBalance = userInfo?.credit_balance || 0;
    const accountType = userInfo?.account_type || 'free';
    const hasSubscription = session.subscription?.has_subscription;
    const subscriptionPlan = session.subscription?.subscription?.plan;

    let balanceColor, status;
    if (creditBalance > 1000) {
      balanceColor = COLORS.success;
      status = "Excellent";
    } else if (creditBalance > 100) {
      balanceColor = COLORS.warning;
      status = "Good";
    } else {
      balanceColor = COLORS.error;
      status = "Low Credits";
    }

    return (
      <div className="p-4">
        <Header title={`🔑 ${userInfo?.username || 'User'}'s Profile`} lastRefresh={lastRefresh} />

        <TwoColumnLayout
          left={
            <InfoWidget title="Account Details">
              <TextLine text={`Username: ${userInfo?.username || 'N/A'}`} />
              <TextLine text={`Email: ${userInfo?.email || 'N/A'}`} />
              <TextLine text={`Account Type: ${accountType.charAt(0).toUpperCase() + accountType.slice(1)}`} />
              <TextLine text={`Member Since: ${formatDate(userInfo?.created_at)}`} />
              <Spacer />
              {hasSubscription && subscriptionPlan && (
                <>
                  <TextLine text="Subscription:" color={COLORS.info} />
                  <TextLine text={`Plan: ${subscriptionPlan.name}`} color={COLORS.success} />
                  <TextLine text={`Price: $${subscriptionPlan.price}/month`} />
                  <TextLine text={`Status: ${session.subscription?.subscription?.status}`} color={COLORS.success} />
                  <Spacer />
                </>
              )}
              <TextLine text="Authentication:" color={COLORS.info} />
              <TextLine text={getApiKeyInfo(session.api_key || undefined, true)} />
              <Spacer />
              <TextLine text={hasSubscription ? "✓ Unlimited API requests" : "✓ Limited API requests"} />
              <TextLine text={hasSubscription ? "✓ All database access" : "✓ Basic database access"} />
              <TextLine text={hasSubscription ? "✓ Premium features" : "✓ Standard features"} />
              <Spacer height={20} />
              <ButtonGroup buttons={[
                { label: "Regenerate API Key", callback: regenerateApiKey },
                { label: "Switch Account", callback: logoutUser }
              ]} />
            </InfoWidget>
          }
          right={
            <InfoWidget title="Credits & Usage">
              <TextLine text={`Current Balance: ${creditBalance} credits`} />
              <TextLine text={`Status: ${status}`} color={balanceColor} />
              <Spacer />
              <TextLine text="Live Usage Stats:" color={COLORS.info} />
              <TextLine text={`Total Requests: ${usageStats.total_requests}`} />
              <TextLine text={`Credits Used: ${usageStats.total_credits_used}`} />
              <TextLine text={`This Session: ${requestCount}`} />
              <Spacer />
              <TextLine text="Quick Actions:" />
            </InfoWidget>
          }
        />

        <Spacer height={20} />

        {/* User Stats - Full Width */}
        <InfoWidget title="User Statistics" width={750} height={150}>
          <div className="grid grid-cols-4 gap-4 text-xs font-mono">
            <div>
              <TextLine text="Total Requests:" color={COLORS.info} />
              <TextLine text={usageStats.total_requests.toLocaleString()} />
            </div>
            <div>
              <TextLine text="Credits Balance:" color={COLORS.info} />
              <TextLine text={creditBalance.toLocaleString()} color={balanceColor} />
            </div>
            <div>
              <TextLine text="Account Status:" color={COLORS.info} />
              <TextLine text="Active" color={COLORS.success} />
            </div>
            <div>
              <TextLine text="API Status:" color={COLORS.info} />
              <TextLine text="Connected" color={COLORS.success} />
            </div>
          </div>
        </InfoWidget>
      </div>
    );
  };

  const renderUnknownProfile = () => (
    <div className="p-4">
      <Header title="❓ Unknown Session State" lastRefresh={lastRefresh} />

      <InfoWidget title="Session Status" width={500} height={200}>
        <TextLine text="Unable to determine authentication status" />
        <TextLine text="This may indicate a configuration issue." />
        <Spacer />
        <TextLine text="Try refreshing or restarting the application" color={COLORS.warning} />
        <Spacer height={40} />
        <ButtonGroup buttons={[
          { label: "🔄 Refresh Profile", callback: manualRefresh },
          { label: "Clear Session & Restart", callback: logoutUser }
        ]} />
      </InfoWidget>
    </div>
  );

  // Main render logic based on user type
  const renderContent = () => {
    if (!session) {
      return renderUnknownProfile();
    }

    switch (session.user_type) {
      case 'guest': return renderGuestProfile();
      case 'registered': return renderUserProfile();
      default: return renderUnknownProfile();
    }
  };

  return (
    <div className="h-full bg-black text-white font-mono text-xs overflow-auto">
      {renderContent()}
    </div>
  );
};

export default ProfileTab;