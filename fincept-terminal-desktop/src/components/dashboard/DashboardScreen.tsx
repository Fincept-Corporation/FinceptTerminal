import React, { useState, useRef, useEffect } from 'react';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { Maximize, Minimize, Download, Settings, RefreshCw, User, Database, Eye, HelpCircle, LogOut, CheckCircle2, XCircle, MessageSquare, Terminal, Bot } from 'lucide-react';
import { useAuth } from '@/contexts/AuthContext';
import { useNavigation } from '@/contexts/NavigationContext';
import { InterfaceModeProvider, useInterfaceMode } from '@/contexts/InterfaceModeContext';
import { useCurrentTime, useDefaultTime } from '@/contexts/TimezoneContext';
import { ChatModeInterface } from '@/components/chat-mode';
import { APP_VERSION } from '@/constants/version';
import { useAutoUpdater } from '@/hooks/useAutoUpdater';
import { Dialog, DialogContent, DialogDescription, DialogFooter, DialogHeader, DialogTitle } from '@/components/ui/dialog';
import { Button } from '@/components/ui/button';
import { Progress } from '@/components/ui/progress';
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert';
import { CommandBar } from '@/components/command-bar';

// Eagerly loaded tabs (needed immediately)
import ForumTab from '@/components/tabs/ForumTab';
import DashboardTab from '@/components/tabs/DashboardTab';
import MarketsTab from '@/components/tabs/MarketsTab';
import NewsTab from '@/components/tabs/NewsTab';
import WatchlistTab from '@/components/tabs/WatchlistTab';
import ChatTab from '@/components/tabs/ChatTab';
import ProfileTab from '@/components/tabs/ProfileTab';
import AboutTab from '@/components/tabs/AboutTab';
import MarketplaceTab from '@/components/tabs/MarketplaceTab';
import PortfolioTab from '@/components/tabs/portfolio-tab/PortfolioTab';
import DocsTab from '@/components/tabs/docs/DocsTab';
import SettingsTab from '@/components/tabs/SettingsTab';
import DataSourcesTab from '@/components/tabs/data-sources/DataSourcesTab';
import MCPTab from '@/components/tabs/mcp';
import SupportTicketTab from '@/components/tabs/SupportTicketTab';
import RecordedContextsManager from '@/components/common/RecordedContextsManager';
import AgentConfigTab from '@/components/tabs/AgentConfigTab';
import RelationshipMapTab from '@/components/tabs/RelationshipMapTab';
import MonitoringTab from '@/components/tabs/MonitoringTab';
import { useTranslation } from 'react-i18next';

// Lazy loaded tabs (heavy/Python-dependent)
const EquityResearchTab = React.lazy(() => import('@/components/tabs/EquityResearchTab'));
const ScreenerTab = React.lazy(() => import('@/components/tabs/ScreenerTab'));
const BacktestingTab = React.lazy(() => import('@/components/tabs/BacktestingTabNew'));
const GeopoliticsTab = React.lazy(() => import('@/components/tabs/GeopoliticsTab'));
const AIQuantLabTab = React.lazy(() => import('@/components/tabs/ai-quant-lab/AIQuantLabTab'));
const CodeEditorTab = React.lazy(() => import('@/components/tabs/CodeEditorTab'));
const NodeEditorTab = React.lazy(() => import('@/components/tabs/NodeEditorTab'));
const PolygonEqTab = React.lazy(() => import('@/components/tabs/PolygonEqTab'));
const DerivativesTab = React.lazy(() => import('@/components/tabs/DerivativesTab').then(m => ({ default: m.DerivativesTab })));
const TradingTab = React.lazy(() => import('@/components/tabs/TradingTab').then(m => ({ default: m.TradingTab })));
const EquityTradingTab = React.lazy(() => import('@/components/tabs/equity-trading/EquityTradingTab'));
const DBnomicsTab = React.lazy(() => import('@/components/tabs/DBnomicsTab'));
const EconomicsTab = React.lazy(() => import('@/components/tabs/EconomicsTab'));
const MaritimeTab = React.lazy(() => import('@/components/tabs/MaritimeTabDeck'));
const ReportBuilderTab = React.lazy(() => import('@/components/tabs/ReportBuilderTab'));
const DataMappingTab = React.lazy(() => import('@/components/tabs/data-mapping/DataMappingTab'));
const ExcelTab = React.lazy(() => import('@/components/tabs/ExcelTab'));
const Visualization3DTab = React.lazy(() => import('@/components/tabs/Visualization3DTab'));
const PolymarketTab = React.lazy(() => import('@/components/tabs/PolymarketTabEnhanced'));
const TradeVisualizationTab = React.lazy(() => import('@/components/tabs/TradeVisualizationTab'));

// Loading fallback component for lazy-loaded tabs
const TabLoadingFallback = () => (
  <div style={{
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    height: '100%',
    backgroundColor: '#000000',
    color: '#a3a3a3',
    fontSize: '12px',
    fontFamily: 'Consolas, "Courier New", monospace'
  }}>
    <div style={{ textAlign: 'center' }}>
      <div style={{
        width: '40px',
        height: '40px',
        border: '3px solid #333',
        borderTop: '3px solid #ea580c',
        borderRadius: '50%',
        animation: 'spin 1s linear infinite',
        margin: '0 auto 16px'
      }} />
      <div>Loading tab...</div>
      <style>{`
        @keyframes spin {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  </div>
);

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

// Header Time Display Component - uses DEFAULT timezone from settings for nav bar
const HeaderTimeDisplay = () => {
  const { formattedTime, timezone } = useDefaultTime();

  // Get current date in YYYY-MM-DD format
  const [currentDate, setCurrentDate] = React.useState(() => {
    const now = new Date();
    return now.toISOString().split('T')[0];
  });

  // Update date at midnight
  React.useEffect(() => {
    const updateDate = () => {
      const now = new Date();
      setCurrentDate(now.toISOString().split('T')[0]);
    };

    const timer = setInterval(updateDate, 60000); // Check every minute
    return () => clearInterval(timer);
  }, []);

  return (
    <span style={{ color: '#a3a3a3', fontSize: '9px' }}>
      {currentDate} {formattedTime}
    </span>
  );
};

// Internal component that uses the InterfaceMode context
function FinxeptTerminalContent() {
  const { t } = useTranslation('tabs');
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
        const { sqliteService } = await import('@/services/core/sqliteService');
        if (!sqliteService.isReady()) {
          console.log('[App] Initializing database on app start...');
          await sqliteService.initialize();
          console.log('[App] Database initialized successfully');
        }

        // Auto-start MCP servers that have auto_start enabled
        try {
          const { mcpManager } = await import('@/services/mcp/mcpManager');
          await mcpManager.startAutoStartServers();
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
              <span>[OK] Active</span>
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
    switch (item.action) {
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
          setActiveTab('backtesting');
          setStatusMessage('Navigated to Backtesting (F5)');
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
          setStatusMessage('Navigated to Crypto Trading (F9)');
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
    { label: 'Relationship Map', action: () => setActiveTab('relationship-map') },
    { label: 'Stock Screener', shortcut: 'F8', action: () => setActiveTab('screener') },
    { label: 'Polygon Data', action: () => setActiveTab('polygon'), separator: true },
    { label: 'Economics', action: () => setActiveTab('economics') },
    { label: 'DBnomics', action: () => setActiveTab('dbnomics') }
  ];

  const researchMenuItems = [
    { label: 'Equity Research', shortcut: 'F7', action: () => setActiveTab('research') },
    { label: '3D Visualization', action: () => setActiveTab('3d-viz') },
    { label: 'AI Quant Lab', action: () => setActiveTab('ai-quant-lab') },
    { label: 'Geopolitics', action: () => setActiveTab('geopolitics') },
    { label: 'Maritime Intelligence', action: () => setActiveTab('maritime') },
    { label: 'News Feed', shortcut: 'F3', action: () => setActiveTab('news') }
  ];

  const tradingMenuItems = [
    { label: 'Crypto Trading', shortcut: 'F9', action: () => setActiveTab('trading') },
    { label: 'Equity Trading', action: () => setActiveTab('equity-trading') },
    { label: 'Polymarket', action: () => setActiveTab('polymarket') },
    { label: 'Derivatives Pricing', action: () => setActiveTab('derivatives') },
    { label: 'Portfolio', shortcut: 'F4', action: () => setActiveTab('portfolio') },
    { label: 'Backtesting', shortcut: 'F5', action: () => setActiveTab('backtesting') },
    { label: 'Watchlist', shortcut: 'F6', action: () => setActiveTab('watchlist') }
  ];

  const toolsMenuItems = [
    { label: 'AI Assistant', shortcut: 'F10', action: () => setActiveTab('chat') },
    { label: 'Agent Config', action: () => setActiveTab('agents') },
    { label: 'MCP Servers', action: () => setActiveTab('mcp'), separator: true },
    { label: 'Monitoring', action: () => setActiveTab('monitoring') },
    { label: 'Trade Visualization', action: () => setActiveTab('trade-viz') },
    { label: 'Excel Workbook', action: () => setActiveTab('excel') },
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
    { label: 'About Fincept', action: () => setActiveTab('about') },
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
                  {t('navigation.dashboard')}
                </TabsTrigger>
                <TabsTrigger
                  value="markets"
                  style={activeTab === 'markets' ? tabStyles.active : tabStyles.default}
                  title="Markets (F2)"
                >
                  {t('navigation.markets')}
                </TabsTrigger>
                <TabsTrigger
                  value="news"
                  style={activeTab === 'news' ? tabStyles.active : tabStyles.default}
                  title="News (F3)"
                >
                  {t('navigation.news')}
                </TabsTrigger>
                <TabsTrigger
                  value="portfolio"
                  style={activeTab === 'portfolio' ? tabStyles.active : tabStyles.default}
                  title="Portfolio (F4)"
                >
                  {t('navigation.portfolio')}
                </TabsTrigger>
                <TabsTrigger
                  value="backtesting"
                  style={activeTab === 'backtesting' ? tabStyles.active : tabStyles.default}
                  title="Backtesting (F5)"
                >
                  {t('navigation.backtesting')}
                </TabsTrigger>
                <TabsTrigger
                  value="watchlist"
                  style={activeTab === 'watchlist' ? tabStyles.active : tabStyles.default}
                  title="Watchlist (F6)"
                >
                  {t('navigation.watchlist')}
                </TabsTrigger>
                <TabsTrigger
                  value="research"
                  style={activeTab === 'research' ? tabStyles.active : tabStyles.default}
                  title="Equity Research (F7)"
                >
                  {t('navigation.research')}
                </TabsTrigger>
                <TabsTrigger
                  value="screener"
                  style={activeTab === 'screener' ? tabStyles.active : tabStyles.default}
                  title="Screener (F8)"
                >
                  {t('navigation.screener')}
                </TabsTrigger>
                <TabsTrigger
                  value="trading"
                  style={activeTab === 'trading' ? tabStyles.active : tabStyles.default}
                  title="Crypto Trading (F9)"
                >
                  Crypto Trading
                </TabsTrigger>
                <TabsTrigger
                  value="equity-trading"
                  style={activeTab === 'equity-trading' ? tabStyles.active : tabStyles.default}
                  title="Equity Trading"
                >
                  Equity Trading
                </TabsTrigger>
                <TabsTrigger
                  value="chat"
                  style={activeTab === 'chat' ? tabStyles.active : tabStyles.default}
                  title="AI Chat (F10)"
                >
                  {t('navigation.chat')}
                </TabsTrigger>
                <TabsTrigger
                  value="agents"
                  style={activeTab === 'agents' ? tabStyles.active : tabStyles.default}
                  title="Agent Config"
                >
                  {t('navigation.agents')}
                </TabsTrigger>
                <TabsTrigger
                  value="settings"
                  style={activeTab === 'settings' ? tabStyles.active : tabStyles.default}
                  title="Settings"
                >
                  {t('navigation.settings')}
                </TabsTrigger>
                <TabsTrigger
                  value="profile"
                  style={activeTab === 'profile' ? tabStyles.active : tabStyles.default}
                  title="Profile (F12)"
                >
                  {t('navigation.profile')}
                </TabsTrigger>
              </TabsList>
            </Tabs>
          </div>
        </div>
      )}

      {/* Command Bar - Hidden in Chat Mode */}
      {/* Merged Status & Function Keys Bar - Single Line */}
      {mode !== 'chat' && (
        <div style={{
          backgroundColor: '#1a1a1a',
          borderBottom: '1px solid #404040',
          padding: '3px 8px',
          height: '22px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: '12px',
          flexShrink: 0,
          fontSize: '9px'
        }}>
          {/* Left: Branding & Status Info */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <span style={{ backgroundColor: '#ea580c', color: 'white', padding: '2px 5px', fontSize: '8px', fontWeight: 'bold' }}>FINXEPT PROFESSIONAL</span>
            <div style={{ width: '1px', height: '14px', backgroundColor: '#525252' }}></div>
            <CommandBar onExecuteCommand={setActiveTab} />
            <div style={{ width: '1px', height: '14px', backgroundColor: '#525252' }}></div>
            <HeaderTimeDisplay />
            <div style={{ width: '1px', height: '14px', backgroundColor: '#525252' }}></div>
            <span style={{ color: '#10b981', fontSize: '9px', fontWeight: 'bold' }}>v{APP_VERSION}</span>
            {isFullscreen && (
              <>
                <div style={{ width: '1px', height: '14px', backgroundColor: '#525252' }}></div>
                <span style={{ color: '#fbbf24', fontSize: '9px' }}>FULLSCREEN</span>
              </>
            )}
          </div>

          {/* Right: Function Keys */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F1:{t('functionKeys.f1')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F2:{t('functionKeys.f2')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F3:{t('functionKeys.f3')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F4:{t('functionKeys.f4')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F5:{t('functionKeys.f5')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F6:{t('functionKeys.f6')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F7:{t('functionKeys.f7')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F8:{t('functionKeys.f8')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F9:{t('functionKeys.f9')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F10:{t('functionKeys.f10')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F11:{t('functionKeys.f11')}</span>
            <span style={{ color: '#fbbf24', fontSize: '9px' }}>F12:{t('functionKeys.f12')}</span>
            <span style={{ color: '#666', marginLeft: '4px' }}>|</span>
            <span style={{ color: '#10b981', fontSize: '9px' }}>{t('labels.moreTabs')}</span>
          </div>
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
            <TabsContent value="relationship-map" className="h-full m-0 p-0">
              <RelationshipMapTab />
            </TabsContent>
            <TabsContent value="news" className="h-full m-0 p-0">
              <NewsTab />
            </TabsContent>
            <TabsContent value="monitoring" className="h-full m-0 p-0">
              <MonitoringTab />
            </TabsContent>
            <TabsContent value="forum" className="h-full m-0 p-0">
              <ForumTab />
            </TabsContent>
            <TabsContent value="watchlist" className="h-full m-0 p-0">
              <WatchlistTab />
            </TabsContent>
            <TabsContent value="geopolitics" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <GeopoliticsTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="trade-viz" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <TradeVisualizationTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="chat" className="h-full m-0 p-0">
              <ChatTab
                onNavigateToSettings={() => setActiveTab('settings')}
                onNavigateToTab={(tabName) => setActiveTab(tabName)}
              />
            </TabsContent>
            <TabsContent value="mcp" className="h-full m-0 p-0">
              <MCPTab onNavigateToTab={(tabName) => setActiveTab(tabName)} />
            </TabsContent>
            <TabsContent value="profile" className="h-full m-0 p-0">
              <ProfileTab />
            </TabsContent>
            <TabsContent value="about" className="h-full m-0 p-0">
              <AboutTab />
            </TabsContent>
            <TabsContent value="marketplace" className="h-full m-0 p-0">
              <MarketplaceTab />
            </TabsContent>
            <TabsContent value="portfolio" className="h-full m-0 p-0">
              <PortfolioTab />
            </TabsContent>
            <TabsContent value="backtesting" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <BacktestingTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="research" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <EquityResearchTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="polygon" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <PolygonEqTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="screener" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <ScreenerTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="dbnomics" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <DBnomicsTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="economics" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <EconomicsTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="code" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <CodeEditorTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="docs" className="h-full m-0 p-0">
              <DocsTab />
            </TabsContent>
            <TabsContent value="maritime" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <MaritimeTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="trading" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <TradingTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="equity-trading" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <EquityTradingTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="polymarket" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <PolymarketTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="derivatives" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <DerivativesTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="settings" className="h-full m-0 p-0">
              <SettingsTab />
            </TabsContent>
            <TabsContent value="nodes" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <NodeEditorTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="datasources" className="h-full m-0 p-0">
              <DataSourcesTab />
            </TabsContent>
            <TabsContent value="support" className="h-full m-0 p-0">
              <SupportTicketTab />
            </TabsContent>
            <TabsContent value="datamapping" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <DataMappingTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="contexts" className="h-full m-0 p-0">
              <RecordedContextsManager />
            </TabsContent>
            <TabsContent value="reportbuilder" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <ReportBuilderTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="agents" className="h-full m-0 p-0">
              <AgentConfigTab />
            </TabsContent>
            <TabsContent value="ai-quant-lab" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <AIQuantLabTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="excel" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <ExcelTab />
              </React.Suspense>
            </TabsContent>
            <TabsContent value="3d-viz" className="h-full m-0 p-0">
              <React.Suspense fallback={<TabLoadingFallback />}>
                <Visualization3DTab />
              </React.Suspense>
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