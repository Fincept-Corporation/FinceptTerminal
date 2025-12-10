import React, { useState, useRef, useEffect } from 'react';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { Maximize, Minimize, Download, Settings, RefreshCw, User, Database, Eye, HelpCircle, LogOut, CheckCircle2, XCircle, MessageSquare, Terminal } from 'lucide-react';
import { useAuth } from '@/contexts/AuthContext';
import { useNavigation } from '@/contexts/NavigationContext';
import { InterfaceModeProvider, useInterfaceMode } from '@/contexts/InterfaceModeContext';
import { ChatModeInterface } from '@/components/chat-mode';
import { APP_VERSION } from '@/constants/version';
import { useAutoUpdater } from '@/hooks/useAutoUpdater';
import { Dialog, DialogContent, DialogDescription, DialogFooter, DialogHeader, DialogTitle } from '@/components/ui/dialog';
import { Button } from '@/components/ui/button';
import { Progress } from '@/components/ui/progress';
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert';
import ForumTab from '@/components/tabs/ForumTab';
import DashboardTab from '@/components/tabs/DashboardTab';
import MarketsTab from '@/components/tabs/MarketsTab';
import NewsTab from '@/components/tabs/NewsTab';
import WatchlistTab from '@/components/tabs/WatchlistTab';
import GeopoliticsTab from '@/components/tabs/GeopoliticsTab';
import ChatTab from '@/components/tabs/ChatTab';
import ProfileTab from '@/components/tabs/ProfileTab';
import MarketplaceTab from '@/components/tabs/MarketplaceTab';
import PortfolioTab from '@/components/tabs/PortfolioTab';
import AnalyticsTab from '@/components/tabs/AnalyticsTab';
import EquityResearchTab from '@/components/tabs/EquityResearchTab';
import ScreenerTab from '@/components/tabs/ScreenerTab';
import DBnomicsTab from '@/components/tabs/DBnomicsTab';
import EconomicsTab from '@/components/tabs/EconomicsTab';
import CodeEditorTab from '@/components/tabs/CodeEditorTab';
import DocsTab from '@/components/tabs/DocsTab';
import MaritimeTab from '@/components/tabs/MaritimeTab';
import SettingsTab from '@/components/tabs/SettingsTab';
import NodeEditorTab from '@/components/tabs/NodeEditorTab';
import DataSourcesTab from '@/components/tabs/data-sources/DataSourcesTab';
import DataMappingTab from '@/components/tabs/data-mapping/DataMappingTab';
import MCPTab from '@/components/tabs/mcp';
import FyersTab from '@/components/tabs/fyers';
import SupportTicketTab from '@/components/tabs/SupportTicketTab';
import PolygonEqTab from '@/components/tabs/PolygonEqTab';
import { TradingTab } from '@/components/tabs/TradingTab';
import ReportBuilderTab from '@/components/tabs/ReportBuilderTab';
import BacktestingTab from '@/components/tabs/BacktestingTab';
import RecordedContextsManager from '@/components/common/RecordedContextsManager';

// Dropdown Menu Component
const DropdownMenu = ({ label, items, onItemClick }: { label: string; items: any[]; onItemClick: (item: any) => void }) => {
  const [isOpen, setIsOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(event.target as Node)) {
        setIsOpen(false);
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  return (
    <div ref={menuRef} style={{ position: 'relative', display: 'inline-block' }}>
      <span 
        style={{ cursor: 'pointer', padding: '2px 4px', borderRadius: '2px', backgroundColor: isOpen ? '#404040' : 'transparent' }}
        onClick={() => setIsOpen(!isOpen)}
      >
        {label}
      </span>
      {isOpen && (
        <div style={{
          position: 'absolute',
          top: '100%',
          left: 0,
          backgroundColor: '#2d2d2d',
          border: '1px solid #404040',
          borderRadius: '2px',
          minWidth: '150px',
          zIndex: 1000,
          boxShadow: '0 2px 8px rgba(0,0,0,0.5)'
        }}>
          {items.map((item: any, index: number) => (
            <div
              key={index}
              style={{
                padding: '6px 12px',
                cursor: item.disabled ? 'not-allowed' : 'pointer',
                color: item.disabled ? '#666' : '#a3a3a3',
                fontSize: '11px',
                borderBottom: item.separator ? '1px solid #404040' : 'none'
              }}
              onMouseEnter={(e) => {
                if (!item.disabled) {
                  (e.target as HTMLDivElement).style.backgroundColor = '#404040';
                  (e.target as HTMLDivElement).style.color = '#fff';
                }
              }}
              onMouseLeave={(e) => {
                (e.target as HTMLDivElement).style.backgroundColor = 'transparent';
                (e.target as HTMLDivElement).style.color = item.disabled ? '#666' : '#a3a3a3';
              }}
              onClick={() => {
                if (!item.disabled && onItemClick) {
                  onItemClick(item);
                  setIsOpen(false);
                }
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                {item.icon}
                <span>{item.label}</span>
                {item.shortcut && (
                  <span style={{ marginLeft: 'auto', fontSize: '10px', color: '#666' }}>
                    {item.shortcut}
                  </span>
                )}
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

// Internal component that uses the InterfaceMode context
function FinxeptTerminalContent() {
  const { session, logout } = useAuth();
  const navigation = useNavigation();
  const { mode, toggleMode } = useInterfaceMode();
  const [activeTab, setActiveTab] = useState("dashboard");
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [statusMessage, setStatusMessage] = useState("");
  const { updateAvailable, updateInfo, isInstalling, installProgress, error, installUpdate, dismissUpdate } = useAutoUpdater();

  React.useEffect(() => {
    document.body.style.margin = '0';
    document.body.style.padding = '0';
    document.body.style.backgroundColor = '#000000';
    document.body.style.overflow = 'hidden';
    document.body.style.overscrollBehavior = 'none';
    document.documentElement.style.backgroundColor = '#000000';
    document.documentElement.style.overflow = 'hidden';
    document.documentElement.style.overscrollBehavior = 'none';

    // Initialize database and MCP servers on app start
    (async () => {
      try {
        // Initialize database first
        const { sqliteService } = await import('@/services/sqliteService');
        if (!sqliteService.isReady()) {
          console.log('[App] Initializing database on app start...');
          await sqliteService.initialize();
          console.log('[App] Database initialized successfully');
        }

        // Auto-start MCP servers that have auto_start enabled
        try {
          const { mcpManager } = await import('@/services/mcpManager');
          console.log('[App] Starting MCP auto-start servers...');
          await mcpManager.startAutoStartServers();
          console.log('[App] MCP servers auto-started successfully');
        } catch (mcpError) {
          console.warn('[App] MCP auto-start failed (this is OK if no servers are configured):', mcpError);
        }
      } catch (error) {
        console.error('[App] Failed to initialize on app start:', error);
      }
    })();

    return () => {
      document.body.style.overflow = 'auto';
      document.body.style.overscrollBehavior = 'auto';
      document.documentElement.style.overflow = 'auto';
      document.documentElement.style.overscrollBehavior = 'auto';
    };
  }, []);

  // Get clickable session display component
  const getClickableSessionDisplay = () => {
    if (!session) return <span style={{ color: '#fbbf24', fontSize: '10px' }}>Loading...</span>;

    if (session.user_type === 'guest') {
      // Guest user display
      let timeText = '';
      if (session.expires_at) {
        const expiryTime = new Date(session.expires_at).getTime();
        const now = new Date().getTime();
        const hoursRemaining = Math.floor((expiryTime - now) / (1000 * 60 * 60));
        const minutesRemaining = Math.floor(((expiryTime - now) % (1000 * 60 * 60)) / (1000 * 60));

        if (hoursRemaining > 0) {
          timeText = ` (${hoursRemaining}h ${minutesRemaining}m remaining)`;
        } else if (minutesRemaining > 0) {
          timeText = ` (${minutesRemaining}m remaining)`;
        } else {
          timeText = ' (Expired)';
        }
      }

      return (
        <span style={{ color: '#fbbf24', fontSize: '10px', display: 'flex', alignItems: 'center', gap: '4px' }}>
          <span
            onClick={() => setActiveTab('profile')}
            style={{ cursor: 'pointer', textDecoration: 'underline' }}
            title="Go to Profile"
          >
            👤 Guest
          </span>
          <span>{timeText}</span>
          <span style={{ color: '#787878' }}>|</span>
          <span
            onClick={() => navigation.navigateToPricing()}
            style={{ cursor: 'pointer', textDecoration: 'underline' }}
            title="View Plans"
          >
            📦 View Plans
          </span>
        </span>
      );
    } else if (session.user_type === 'registered') {
      const username = session.user_info?.username || session.user_info?.email || 'User';
      const accountType = session.user_info?.account_type || 'free';
      const creditBalance = session.user_info?.credit_balance ?? 0;
      const hasSubscription = (session.subscription as any)?.data?.has_subscription || session.subscription?.has_subscription;
      const planName = hasSubscription
        ? ((session.subscription as any)?.data?.subscription?.plan?.name || session.subscription?.subscription?.plan?.name || accountType)
        : (accountType === 'free' ? 'Free Plan' : accountType.charAt(0).toUpperCase() + accountType.slice(1));

      return (
        <span style={{ color: '#fbbf24', fontSize: '10px', display: 'flex', alignItems: 'center', gap: '4px' }}>
          <span
            onClick={() => setActiveTab('profile')}
            style={{ cursor: 'pointer', textDecoration: 'underline' }}
            title="Go to Profile"
          >
            👤 {username}
          </span>
          <span style={{ color: '#787878' }}>|</span>
          <span
            onClick={() => setActiveTab('profile')}
            style={{ cursor: 'pointer', textDecoration: 'underline', color: creditBalance > 0 ? '#10b981' : '#ef4444' }}
            title="View Credits"
          >
            💳 {creditBalance.toFixed(2)} Credits
          </span>
          <span style={{ color: '#787878' }}>|</span>
          <span
            onClick={() => navigation.navigateToPricing()}
            style={{ cursor: 'pointer', textDecoration: 'underline' }}
            title={hasSubscription ? 'Change Plan' : 'View Plans'}
          >
            📦 {planName}
          </span>
          {hasSubscription && (
            <>
              <span style={{ color: '#787878' }}>|</span>
              <span>✅ Active</span>
            </>
          )}
        </span>
      );
    }

    return <span style={{ color: '#fbbf24', fontSize: '10px' }}>Unknown Session</span>;
  };

  const toggleFullscreen = () => {
    if (!document.fullscreenElement) {
      document.documentElement.requestFullscreen().then(() => {
        setIsFullscreen(true);
        setStatusMessage("Entered fullscreen mode");
      }).catch(() => {
        setStatusMessage("Failed to enter fullscreen mode");
      });
    } else {
      document.exitFullscreen().then(() => {
        setIsFullscreen(false);
        setStatusMessage("Exited fullscreen mode");
      });
    }
    
    // Clear status message after 3 seconds
    setTimeout(() => setStatusMessage(""), 3000);
  };

  const handleMenuAction = async (item: any) => {
    // If action is a function (tab navigation), execute it directly
    if (typeof item.action === 'function') {
      item.action();
      setStatusMessage(`Navigated to ${item.label}`);
      setTimeout(() => setStatusMessage(''), 2000);
      return;
    }

    setStatusMessage(`${item.label} clicked`);

    // Handle specific string actions
    switch(item.action) {
      case 'fullscreen':
        toggleFullscreen();
        break;
      case 'refresh':
      case 'refresh_all':
        setStatusMessage("Refreshing all data...");
        window.location.reload();
        break;
      case 'export':
      case 'export_portfolio':
        setStatusMessage("Export Portfolio - Feature coming soon");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'import_data':
        setStatusMessage("Import Data - Feature coming soon");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'new_workspace':
        setStatusMessage("New Workspace - Feature coming soon");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'open_workspace':
        setStatusMessage("Open Workspace - Feature coming soon");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'save_workspace':
        setStatusMessage("Save Workspace - Feature coming soon");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'zoom_in':
        setStatusMessage("Zoom In - Use Ctrl++ in browser");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'zoom_out':
        setStatusMessage("Zoom Out - Use Ctrl+- in browser");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'zoom_reset':
        setStatusMessage("Reset Zoom - Use Ctrl+0 in browser");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'toggle_theme':
        setStatusMessage("Toggle Theme - Feature coming soon");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'show_shortcuts':
        setStatusMessage("Keyboard Shortcuts - Press F1-F12 to navigate tabs");
        setTimeout(() => setStatusMessage(''), 5000);
        break;
      case 'show_about':
        setStatusMessage(`Fincept Terminal v${APP_VERSION} - Professional Financial Platform`);
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      case 'check_updates':
        setStatusMessage("Checking for updates...");
        setTimeout(() => setStatusMessage("You are running the latest version"), 3000);
        break;
      case 'logout':
        setStatusMessage("Logging out...");
        await logout();
        window.location.href = '/';
        break;
      case 'exit':
        setStatusMessage("Use Alt+F4 to close the application");
        setTimeout(() => setStatusMessage(''), 3000);
        break;
      default:
        setTimeout(() => setStatusMessage(''), 3000);
    }
  };

  // Keyboard shortcuts handler
  useEffect(() => {
    const handleKeyPress = (event: KeyboardEvent) => {
      // Don't trigger if user is typing in an input field
      const target = event.target as HTMLElement;
      if (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA' || target.isContentEditable) {
        return;
      }

      // Only trigger function keys without modifiers (except F11 which browser handles)
      if (event.ctrlKey || event.altKey || event.shiftKey || event.metaKey) {
        return;
      }

      switch (event.key) {
        case 'F1':
          event.preventDefault();
          setActiveTab('dashboard');
          setStatusMessage('Navigated to Dashboard (F1)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F2':
          event.preventDefault();
          setActiveTab('markets');
          setStatusMessage('Navigated to Markets (F2)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F3':
          event.preventDefault();
          setActiveTab('news');
          setStatusMessage('Navigated to News (F3)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F4':
          event.preventDefault();
          setActiveTab('portfolio');
          setStatusMessage('Navigated to Portfolio (F4)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F5':
          event.preventDefault();
          setActiveTab('analytics');
          setStatusMessage('Navigated to Analytics (F5)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F6':
          event.preventDefault();
          setActiveTab('watchlist');
          setStatusMessage('Navigated to Watchlist (F6)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F7':
          event.preventDefault();
          setActiveTab('research');
          setStatusMessage('Navigated to Equity Research (F7)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F8':
          event.preventDefault();
          setActiveTab('screener');
          setStatusMessage('Navigated to Screener (F8)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F9':
          event.preventDefault();
          setActiveTab('trading');
          setStatusMessage('Navigated to Trading (F9)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F10':
          event.preventDefault();
          setActiveTab('chat');
          setStatusMessage('Navigated to AI Chat (F10)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
        case 'F11':
          // Let browser handle F11 for fullscreen
          break;
        case 'F12':
          event.preventDefault();
          setActiveTab('profile');
          setStatusMessage('Navigated to Profile (F12)');
          setTimeout(() => setStatusMessage(''), 2000);
          break;
      }
    };

    window.addEventListener('keydown', handleKeyPress);
    return () => window.removeEventListener('keydown', handleKeyPress);
  }, []);

  // Menu configurations with tab navigation
  const fileMenuItems = [
    { label: 'New Workspace', shortcut: 'Ctrl+N', icon: null, action: 'new_workspace' },
    { label: 'Open Workspace', shortcut: 'Ctrl+O', icon: null, action: 'open_workspace' },
    { label: 'Save Workspace', shortcut: 'Ctrl+S', icon: null, action: 'save_workspace' },
    { label: 'Export Portfolio', shortcut: 'Ctrl+E', icon: <Download size={12} />, action: 'export', separator: true },
    { label: 'Import Data', icon: null, action: 'import_data' },
    { label: 'Exit', shortcut: 'Alt+F4', icon: null, action: 'exit', separator: true }
  ];

  const marketsMenuItems = [
    { label: 'Live Markets', shortcut: 'F2', action: () => setActiveTab('markets') },
    { label: 'Stock Screener', shortcut: 'F8', action: () => setActiveTab('screener') },
    { label: 'Polygon Data', action: () => setActiveTab('polygon'), separator: true },
    { label: 'Economics', action: () => setActiveTab('economics') },
    { label: 'DBnomics', action: () => setActiveTab('dbnomics') }
  ];

  const researchMenuItems = [
    { label: 'Equity Research', shortcut: 'F7', action: () => setActiveTab('research') },
    { label: 'Geopolitics', action: () => setActiveTab('geopolitics') },
    { label: 'Maritime Intelligence', action: () => setActiveTab('maritime') },
    { label: 'News Feed', shortcut: 'F3', action: () => setActiveTab('news') }
  ];

  const tradingMenuItems = [
    { label: 'Trading Desk', shortcut: 'F9', action: () => setActiveTab('trading') },
    { label: 'Fyers Broker', action: () => setActiveTab('fyers') },
    { label: 'Backtesting', action: () => setActiveTab('backtesting'), separator: true },
    { label: 'Portfolio', shortcut: 'F4', action: () => setActiveTab('portfolio') },
    { label: 'Analytics', shortcut: 'F5', action: () => setActiveTab('analytics') },
    { label: 'Watchlist', shortcut: 'F6', action: () => setActiveTab('watchlist') }
  ];

  const toolsMenuItems = [
    { label: 'AI Assistant', shortcut: 'F10', action: () => setActiveTab('chat') },
    { label: 'MCP Servers', action: () => setActiveTab('mcp'), separator: true },
    { label: 'Node Editor', action: () => setActiveTab('nodes') },
    { label: 'Code Editor', action: () => setActiveTab('code') },
    { label: 'Data Sources', action: () => setActiveTab('datasources'), separator: true },
    { label: 'Data Mapping', action: () => setActiveTab('datamapping') },
    { label: 'Report Builder', action: () => setActiveTab('reportbuilder') },
    { label: 'Recorded Contexts', action: () => setActiveTab('contexts') },
    { label: 'Settings', action: () => setActiveTab('settings'), separator: true }
  ];

  const communityMenuItems = [
    { label: 'Forum', action: () => setActiveTab('forum') },
    { label: 'Marketplace', action: () => setActiveTab('marketplace') },
    { label: 'Documentation', action: () => setActiveTab('docs') }
  ];

  const viewMenuItems = [
    { label: 'Fullscreen', shortcut: 'F11', icon: isFullscreen ? <Minimize size={12} /> : <Maximize size={12} />, action: 'fullscreen' },
    { label: 'Zoom In', shortcut: 'Ctrl++', icon: null, action: 'zoom_in' },
    { label: 'Zoom Out', shortcut: 'Ctrl+-', icon: null, action: 'zoom_out' },
    { label: 'Reset Zoom', shortcut: 'Ctrl+0', icon: null, action: 'zoom_reset', separator: true },
    { label: 'Toggle Theme', icon: <Eye size={12} />, action: 'toggle_theme' }
  ];

  const helpMenuItems = [
    { label: 'Documentation', action: () => setActiveTab('docs') },
    { label: 'Support Tickets', action: () => setActiveTab('support') },
    { label: 'Keyboard Shortcuts', shortcut: 'Ctrl+/', action: 'show_shortcuts', separator: true },
    { label: 'About Fincept', action: 'show_about' },
    { label: 'Check for Updates', action: 'check_updates' },
    { label: 'Logout', icon: <LogOut size={12} />, action: 'logout', separator: true }
  ];

  const tabStyles = {
    default: {
      backgroundColor: 'transparent',
      color: '#d4d4d4',
      border: 'none',
      padding: '3px 8px',
      fontSize: '10px',
      cursor: 'pointer',
      borderRadius: '0',
      whiteSpace: 'nowrap' as const,
      flexShrink: 0
    },
    active: {
      backgroundColor: '#ea580c',
      color: 'white',
      border: 'none',
      padding: '3px 8px',
      fontSize: '10px',
      cursor: 'pointer',
      borderRadius: '0',
      whiteSpace: 'nowrap' as const,
      flexShrink: 0
    }
  };

  return (
    <div style={{
      width: '100vw',
      height: '100vh',
      margin: 0,
      padding: 0,
      backgroundColor: '#000000',
      fontFamily: 'Consolas, "Courier New", monospace',
      overflow: 'hidden',
      fontSize: '11px',
      position: 'fixed',
      top: 0,
      left: 0,
      overscrollBehavior: 'none',
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Top Menu Bar with Functional Dropdowns */}
      <div style={{
        backgroundColor: '#2d2d2d',
        color: '#a3a3a3',
        fontSize: '11px',
        borderBottom: '1px solid #404040',
        padding: '2px 8px',
        flexShrink: 0,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        height: '20px'
      }}>
        <div style={{ display: 'flex', gap: '16px' }}>
          <DropdownMenu label="File" items={fileMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="Markets" items={marketsMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="Research" items={researchMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="Trading" items={tradingMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="Tools" items={toolsMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="Community" items={communityMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="View" items={viewMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="Help" items={helpMenuItems} onItemClick={handleMenuAction} />
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span style={{ color: '#ea580c', fontSize: '11px', fontWeight: 'bold', letterSpacing: '0.5px' }}>FINCEPT PROFESSIONAL</span>
          <div style={{ width: '1px', height: '14px', backgroundColor: '#525252' }}></div>
          <button
            onClick={toggleFullscreen}
            style={{
              background: 'none',
              border: 'none',
              color: '#a3a3a3',
              cursor: 'pointer',
              padding: '2px',
              borderRadius: '2px'
            }}
            onMouseEnter={(e) => {
              (e.target as HTMLButtonElement).style.backgroundColor = '#404040';
              (e.target as HTMLButtonElement).style.color = '#fff';
            }}
            onMouseLeave={(e) => {
              (e.target as HTMLButtonElement).style.backgroundColor = 'transparent';
              (e.target as HTMLButtonElement).style.color = '#a3a3a3';
            }}
            title={isFullscreen ? 'Exit Fullscreen (F11)' : 'Enter Fullscreen (F11)'}
          >
            {isFullscreen ? <Minimize size={14} /> : <Maximize size={14} />}
          </button>
          <div style={{ width: '1px', height: '14px', backgroundColor: '#525252' }}></div>
          <button
            onClick={toggleMode}
            style={{
              background: mode === 'chat' ? '#ea580c' : 'none',
              border: mode === 'chat' ? '1px solid #ea580c' : 'none',
              color: mode === 'chat' ? '#fff' : '#a3a3a3',
              cursor: 'pointer',
              padding: '3px 8px',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontSize: '10px',
              fontWeight: 'bold',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (mode === 'terminal') {
                (e.target as HTMLButtonElement).style.backgroundColor = '#404040';
                (e.target as HTMLButtonElement).style.color = '#fff';
              }
            }}
            onMouseLeave={(e) => {
              if (mode === 'terminal') {
                (e.target as HTMLButtonElement).style.backgroundColor = 'transparent';
                (e.target as HTMLButtonElement).style.color = '#a3a3a3';
              }
            }}
            title={mode === 'terminal' ? 'Switch to Chat Mode' : 'Switch to Terminal Mode'}
          >
            {mode === 'chat' ? (
              <>
                <Terminal size={14} />
                <span>TERMINAL</span>
              </>
            ) : (
              <>
                <MessageSquare size={14} />
                <span>CHAT MODE</span>
              </>
            )}
          </button>
          {getClickableSessionDisplay()}
          <button
            onClick={async () => {
              setStatusMessage("Logging out...");
              await logout();
              window.location.href = '/';
            }}
            style={{
              backgroundColor: 'transparent',
              color: '#ff6b6b',
              border: '1px solid #ff6b6b',
              padding: '3px 8px',
              fontSize: '10px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontWeight: 'bold'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#ff6b6b';
              e.currentTarget.style.color = '#fff';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#ff6b6b';
            }}
            title="Logout"
          >
            <LogOut size={12} />
            LOGOUT
          </button>
          <span style={{ color: '#737373', cursor: 'pointer' }} title="Help">?</span>
        </div>
      </div>

      {/* Status Message Bar */}
      {statusMessage && (
        <div style={{
          backgroundColor: '#ea580c',
          color: 'white',
          fontSize: '10px',
          padding: '2px 8px',
          borderBottom: '1px solid #404040',
          height: '16px',
          display: 'flex',
          alignItems: 'center',
          flexShrink: 0
        }}>
          {statusMessage}
        </div>
      )}

      {/* Main Navigation Bar with Tabs - Hidden in Chat Mode */}
      {mode !== 'chat' && (
      <div style={{
        backgroundColor: '#3f3f3f',
        borderBottom: '1px solid #404040',
        height: '26px',
        position: 'relative',
        flexShrink: 0
      }}>
        <div
          style={{
            overflowX: 'auto',
            overflowY: 'hidden',
            height: '100%',
            scrollbarWidth: 'none',
            msOverflowStyle: 'none'
          }}
          className="hide-scrollbar"
          onWheel={(e) => {
            e.preventDefault();
            const container = e.currentTarget;
            container.scrollLeft += e.deltaY;
          }}
        >
          <style>{`
            .hide-scrollbar::-webkit-scrollbar {
              display: none;
            }
          `}</style>
          <Tabs value={activeTab} onValueChange={setActiveTab} className="h-full">
            <TabsList style={{
              backgroundColor: 'transparent',
              border: 'none',
              padding: '2px 4px',
              height: '100%',
              justifyContent: 'flex-start',
              borderRadius: '0',
              flexWrap: 'nowrap',
              width: 'max-content',
              minWidth: '100%'
            }}>
            {/* PRIMARY TABS - F1 to F12 */}
            <TabsTrigger
              value="dashboard"
              style={activeTab === 'dashboard' ? tabStyles.active : tabStyles.default}
              title="Dashboard (F1)"
            >
              Dashboard
            </TabsTrigger>
            <TabsTrigger
              value="markets"
              style={activeTab === 'markets' ? tabStyles.active : tabStyles.default}
              title="Markets (F2)"
            >
              Markets
            </TabsTrigger>
            <TabsTrigger
              value="news"
              style={activeTab === 'news' ? tabStyles.active : tabStyles.default}
              title="News (F3)"
            >
              News
            </TabsTrigger>
            <TabsTrigger
              value="portfolio"
              style={activeTab === 'portfolio' ? tabStyles.active : tabStyles.default}
              title="Portfolio (F4)"
            >
              Portfolio
            </TabsTrigger>
            <TabsTrigger
              value="analytics"
              style={activeTab === 'analytics' ? tabStyles.active : tabStyles.default}
              title="Analytics (F5)"
            >
              Analytics
            </TabsTrigger>
            <TabsTrigger
              value="watchlist"
              style={activeTab === 'watchlist' ? tabStyles.active : tabStyles.default}
              title="Watchlist (F6)"
            >
              Watchlist
            </TabsTrigger>
            <TabsTrigger
              value="research"
              style={activeTab === 'research' ? tabStyles.active : tabStyles.default}
              title="Equity Research (F7)"
            >
              Research
            </TabsTrigger>
            <TabsTrigger
              value="screener"
              style={activeTab === 'screener' ? tabStyles.active : tabStyles.default}
              title="Screener (F8)"
            >
              Screener
            </TabsTrigger>
            <TabsTrigger
              value="trading"
              style={activeTab === 'trading' ? tabStyles.active : tabStyles.default}
              title="Trading (F9)"
            >
              Trading
            </TabsTrigger>
            <TabsTrigger
              value="chat"
              style={activeTab === 'chat' ? tabStyles.active : tabStyles.default}
              title="AI Chat (F10)"
            >
              AI Chat
            </TabsTrigger>
            <TabsTrigger
              value="settings"
              style={activeTab === 'settings' ? tabStyles.active : tabStyles.default}
              title="Settings"
            >
              Settings
            </TabsTrigger>
            <TabsTrigger
              value="profile"
              style={activeTab === 'profile' ? tabStyles.active : tabStyles.default}
              title="Profile (F12)"
            >
              Profile
            </TabsTrigger>
          </TabsList>
        </Tabs>
        </div>
      </div>
      )}

      {/* Command Bar - Hidden in Chat Mode */}
      {mode !== 'chat' && (
      <div style={{
        backgroundColor: '#2d2d2d',
        borderBottom: '1px solid #404040',
        padding: '4px 8px',
        height: '24px',
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        flexShrink: 0
      }}>
        <span style={{ backgroundColor: '#ea580c', color: 'white', padding: '2px 6px', fontSize: '9px', fontWeight: 'bold' }}>FINXEPT PROFESSIONAL</span>
        <span style={{ color: '#a3a3a3', fontSize: '10px' }}>Enter Command</span>
        <div style={{ width: '1px', height: '16px', backgroundColor: '#525252' }}></div>
        <span style={{ color: '#a3a3a3', fontSize: '10px' }}>Search</span>
        <div style={{ width: '1px', height: '16px', backgroundColor: '#525252' }}></div>
        <span style={{ color: '#a3a3a3', fontSize: '10px' }}>2025-09-26 14:00:49</span>
        <div style={{ width: '1px', height: '16px', backgroundColor: '#525252' }}></div>
        <span style={{ color: '#10b981', fontSize: '10px', fontWeight: 'bold' }}>v{APP_VERSION}</span>
        {isFullscreen && (
          <>
            <div style={{ width: '1px', height: '16px', backgroundColor: '#525252' }}></div>
            <span style={{ color: '#fbbf24', fontSize: '10px' }}>FULLSCREEN MODE</span>
          </>
        )}
      </div>
      )}

      {/* Function Keys Bar - Hidden in Chat Mode */}
      {mode !== 'chat' && (
      <div style={{
        backgroundColor: '#1a1a1a',
        borderBottom: '1px solid #404040',
        padding: '2px 8px',
        height: '20px',
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        flexShrink: 0,
        fontSize: '10px'
      }}>
        <span style={{ color: '#fbbf24' }}>F1:DASH</span>
        <span style={{ color: '#fbbf24' }}>F2:MKTS</span>
        <span style={{ color: '#fbbf24' }}>F3:NEWS</span>
        <span style={{ color: '#fbbf24' }}>F4:PORT</span>
        <span style={{ color: '#fbbf24' }}>F5:ANLY</span>
        <span style={{ color: '#fbbf24' }}>F6:WATCH</span>
        <span style={{ color: '#fbbf24' }}>F7:RSRCH</span>
        <span style={{ color: '#fbbf24' }}>F8:SCRN</span>
        <span style={{ color: '#fbbf24' }}>F9:TRADE</span>
        <span style={{ color: '#fbbf24' }}>F10:AI</span>
        <span style={{ color: '#fbbf24' }}>F11:FULL</span>
        <span style={{ color: '#fbbf24' }}>F12:PROF</span>
        <span style={{ color: '#666', marginLeft: '8px' }}>|</span>
        <span style={{ color: '#10b981' }}>More tabs in toolbar menus</span>
      </div>
      )}

      {/* Main Content Area with Tab Content */}
      <div style={{
        flex: 1,
        backgroundColor: '#000000',
        minHeight: 0,
        overflow: 'hidden'
      }}>
        {mode === 'chat' ? (
          <ChatModeInterface />
        ) : (
          <Tabs value={activeTab} className="h-full">
            <TabsContent value="dashboard" className="h-full m-0 p-0">
              <DashboardTab onNavigateToTab={setActiveTab} />
            </TabsContent>
          <TabsContent value="markets" className="h-full m-0 p-0">
            <MarketsTab />
          </TabsContent>
          <TabsContent value="news" className="h-full m-0 p-0">
            <NewsTab />
          </TabsContent>
          <TabsContent value="forum" className="h-full m-0 p-0">
            <ForumTab />
          </TabsContent>
          <TabsContent value="watchlist" className="h-full m-0 p-0">
            <WatchlistTab />
          </TabsContent>
          <TabsContent value="geopolitics" className="h-full m-0 p-0">
            <GeopoliticsTab />
          </TabsContent>
          <TabsContent value="chat" className="h-full m-0 p-0">
            <ChatTab onNavigateToSettings={() => setActiveTab('settings')} />
          </TabsContent>
          <TabsContent value="fyers" className="h-full m-0 p-0">
            <FyersTab />
          </TabsContent>
          <TabsContent value="mcp" className="h-full m-0 p-0">
            <MCPTab />
          </TabsContent>
          <TabsContent value="profile" className="h-full m-0 p-0">
            <ProfileTab />
          </TabsContent>
          <TabsContent value="marketplace" className="h-full m-0 p-0">
            <MarketplaceTab />
          </TabsContent>
          <TabsContent value="portfolio" className="h-full m-0 p-0">
            <PortfolioTab />
          </TabsContent>
          <TabsContent value="analytics" className="h-full m-0 p-0">
            <AnalyticsTab />
          </TabsContent>
          <TabsContent value="backtesting" className="h-full m-0 p-0">
            <BacktestingTab />
          </TabsContent>
          <TabsContent value="research" className="h-full m-0 p-0">
            <EquityResearchTab />
          </TabsContent>
          <TabsContent value="polygon" className="h-full m-0 p-0">
            <PolygonEqTab />
          </TabsContent>
          <TabsContent value="screener" className="h-full m-0 p-0">
            <ScreenerTab />
          </TabsContent>
          <TabsContent value="dbnomics" className="h-full m-0 p-0">
            <DBnomicsTab />
          </TabsContent>
          <TabsContent value="economics" className="h-full m-0 p-0">
            <EconomicsTab />
          </TabsContent>
          <TabsContent value="code" className="h-full m-0 p-0">
            <CodeEditorTab />
          </TabsContent>
          <TabsContent value="docs" className="h-full m-0 p-0">
            <DocsTab />
          </TabsContent>
          <TabsContent value="maritime" className="h-full m-0 p-0">
            <MaritimeTab />
          </TabsContent>
          <TabsContent value="trading" className="h-full m-0 p-0">
            <TradingTab />
          </TabsContent>
          <TabsContent value="settings" className="h-full m-0 p-0">
            <SettingsTab />
          </TabsContent>
          <TabsContent value="nodes" className="h-full m-0 p-0">
            <NodeEditorTab />
          </TabsContent>
          <TabsContent value="datasources" className="h-full m-0 p-0">
            <DataSourcesTab />
          </TabsContent>
          <TabsContent value="support" className="h-full m-0 p-0">
            <SupportTicketTab />
          </TabsContent>
          <TabsContent value="datamapping" className="h-full m-0 p-0">
            <DataMappingTab />
          </TabsContent>
          <TabsContent value="contexts" className="h-full m-0 p-0">
            <RecordedContextsManager />
          </TabsContent>
          <TabsContent value="reportbuilder" className="h-full m-0 p-0">
            <ReportBuilderTab />
          </TabsContent>
        </Tabs>
        )}
      </div>

      {/* Update Available Dialog */}
      <Dialog open={updateAvailable} onOpenChange={(open) => !open && dismissUpdate()}>
        <DialogContent className="sm:max-w-[500px]">
          <DialogHeader>
            <DialogTitle className="flex items-center gap-2">
              <Download className="h-5 w-5" />
              Update Available
            </DialogTitle>
            <DialogDescription>
              A new version of FinceptTerminal is available.
            </DialogDescription>
          </DialogHeader>

          <div className="space-y-4 py-4">
            {updateInfo && (
              <div className="space-y-2">
                <div className="flex justify-between text-sm">
                  <span className="text-muted-foreground">Current Version:</span>
                  <span className="font-mono">{updateInfo.currentVersion}</span>
                </div>
                <div className="flex justify-between text-sm">
                  <span className="text-muted-foreground">New Version:</span>
                  <span className="font-mono font-bold text-green-600">{updateInfo.version}</span>
                </div>
                {updateInfo.date && (
                  <div className="flex justify-between text-sm">
                    <span className="text-muted-foreground">Release Date:</span>
                    <span>{new Date(updateInfo.date).toLocaleDateString()}</span>
                  </div>
                )}
              </div>
            )}

            {updateInfo?.body && (
              <div className="rounded-md bg-muted p-3 text-sm">
                <p className="font-semibold mb-2">Release Notes:</p>
                <div className="max-h-48 overflow-y-auto">
                  <p className="text-muted-foreground whitespace-pre-wrap text-xs">{updateInfo.body}</p>
                </div>
              </div>
            )}

            {isInstalling && (
              <div className="space-y-2">
                <div className="flex items-center justify-between text-sm">
                  <span>{installProgress === 100 ? 'Installing...' : 'Downloading...'}</span>
                  <span>{installProgress.toFixed(0)}%</span>
                </div>
                <Progress value={installProgress} />
              </div>
            )}

            {installProgress === 100 && isInstalling && (
              <Alert className="border-green-500 bg-green-50 dark:bg-green-950">
                <CheckCircle2 className="h-4 w-4 text-green-600" />
                <AlertTitle>Update Ready!</AlertTitle>
                <AlertDescription>
                  The application will restart in a moment...
                </AlertDescription>
              </Alert>
            )}

            {error && (
              <Alert variant="destructive">
                <XCircle className="h-4 w-4" />
                <AlertTitle>Error</AlertTitle>
                <AlertDescription>{error}</AlertDescription>
              </Alert>
            )}

            {!isInstalling && (
              <p className="text-xs text-muted-foreground">
                The update will download and install automatically. Your app will restart after installation.
              </p>
            )}
          </div>

          <DialogFooter>
            {!isInstalling && !error && (
              <>
                <Button variant="outline" onClick={dismissUpdate}>
                  Later
                </Button>
                <Button onClick={installUpdate}>
                  <Download className="mr-2 h-4 w-4" />
                  Update Now
                </Button>
              </>
            )}
            {isInstalling && (
              <Button disabled>
                <RefreshCw className="mr-2 h-4 w-4 animate-spin" />
                {installProgress === 100 ? 'Installing...' : 'Downloading...'}
              </Button>
            )}
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </div>
  );
}

// Wrapper component that provides InterfaceMode context
export default function FinxeptTerminal() {
  return (
    <InterfaceModeProvider>
      <FinxeptTerminalContent />
    </InterfaceModeProvider>
  );
}