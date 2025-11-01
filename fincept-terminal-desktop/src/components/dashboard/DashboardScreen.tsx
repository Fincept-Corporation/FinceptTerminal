import React, { useState, useRef, useEffect } from 'react';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { Maximize, Minimize, Download, Settings, RefreshCw, User, Database, Eye, HelpCircle, LogOut } from 'lucide-react';
import { useAuth } from '@/contexts/AuthContext';
import { useNavigation } from '@/contexts/NavigationContext';
import { APP_VERSION } from '@/constants/version';
import { useAutoUpdater } from '@/hooks/useAutoUpdater';
import { Dialog, DialogContent, DialogDescription, DialogFooter, DialogHeader, DialogTitle } from '@/components/ui/dialog';
import { Button } from '@/components/ui/button';
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
import OptionsTab from '@/components/tabs/OptionsTab';
import EconomicCalendarTab from '@/components/tabs/EconomicCalendarTab';
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
import KrakenTab from '@/components/tabs/KrakenTab';

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

export default function FinxeptTerminal() {
  const { session, logout } = useAuth();
  const navigation = useNavigation();
  const [activeTab, setActiveTab] = useState("dashboard");
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [statusMessage, setStatusMessage] = useState("");
  const { updateAvailable, updateInfo, installUpdate, dismissUpdate } = useAutoUpdater();

  React.useEffect(() => {
    document.body.style.margin = '0';
    document.body.style.padding = '0';
    document.body.style.backgroundColor = '#000000';
    document.body.style.overflow = 'hidden';
    document.body.style.overscrollBehavior = 'none';
    document.documentElement.style.backgroundColor = '#000000';
    document.documentElement.style.overflow = 'hidden';
    document.documentElement.style.overscrollBehavior = 'none';

    // Initialize database on app start
    (async () => {
      try {
        const { sqliteService } = await import('@/services/sqliteService');
        if (!sqliteService.isReady()) {
          console.log('Initializing database on app start...');
          await sqliteService.initialize();
          console.log('Database initialized successfully');
        }
      } catch (error) {
        console.error('Failed to initialize database on app start:', error);
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
    setStatusMessage(`${item.label} clicked`);

    // Handle specific actions
    switch(item.action) {
      case 'fullscreen':
        toggleFullscreen();
        break;
      case 'refresh':
        window.location.reload();
        break;
      case 'export':
        setStatusMessage("Export functionality not implemented");
        break;
      case 'logout':
        setStatusMessage("Logging out...");
        await logout();
        // Redirect to login will be handled by App.tsx
        window.location.href = '/';
        break;
      default:
        setTimeout(() => setStatusMessage(""), 3000);
    }
  };

  // Menu configurations
  const fileMenuItems = [
    { label: 'New Workspace', shortcut: 'Ctrl+N', icon: null },
    { label: 'Open Workspace', shortcut: 'Ctrl+O', icon: null },
    { label: 'Save Workspace', shortcut: 'Ctrl+S', icon: null },
    { label: 'Export Data', shortcut: 'Ctrl+E', icon: <Download size={12} />, action: 'export', separator: true },
    { label: 'Import Settings', icon: null },
    { label: 'Exit', shortcut: 'Alt+F4', icon: null, separator: true }
  ];

  const tabsMenuItems = [
    { label: 'New Tab', shortcut: 'Ctrl+T', icon: null },
    { label: 'Close Tab', shortcut: 'Ctrl+W', icon: null },
    { label: 'Close Other Tabs', icon: null },
    { label: 'Duplicate Tab', icon: null, separator: true },
    { label: 'Tab Settings', icon: <Settings size={12} /> }
  ];

  const sessionMenuItems = [
    { label: 'Session Info', icon: <User size={12} /> },
    { label: 'Switch User', icon: null },
    { label: 'Extend Session', icon: null, separator: true },
    { label: 'Logout', icon: <LogOut size={12} />, action: 'logout' }
  ];

  const apiMenuItems = [
    { label: 'API Status', icon: null },
    { label: 'Rate Limits', icon: null },
    { label: 'API Keys', icon: null, separator: true },
    { label: 'Documentation', icon: null },
    { label: 'Test Connection', icon: <RefreshCw size={12} />, action: 'refresh' }
  ];

  const databaseMenuItems = [
    { label: 'Connect Database', icon: <Database size={12} /> },
    { label: 'Query Builder', icon: null },
    { label: 'Schema Browser', icon: null, separator: true },
    { label: 'Backup Data', icon: null },
    { label: 'Import Data', icon: null }
  ];

  const viewMenuItems = [
    { label: 'Fullscreen', shortcut: 'F11', icon: isFullscreen ? <Minimize size={12} /> : <Maximize size={12} />, action: 'fullscreen' },
    { label: 'Zoom In', shortcut: 'Ctrl++', icon: null },
    { label: 'Zoom Out', shortcut: 'Ctrl+-', icon: null },
    { label: 'Reset Zoom', shortcut: 'Ctrl+0', icon: null, separator: true },
    { label: 'Show Toolbar', icon: <Eye size={12} /> },
    { label: 'Show Sidebar', icon: null }
  ];

  const toolsMenuItems = [
    { label: 'Calculator', icon: null },
    { label: 'Screen Capture', icon: null },
    { label: 'Color Picker', icon: null, separator: true },
    { label: 'Settings', icon: <Settings size={12} /> },
    { label: 'Preferences', shortcut: 'Ctrl+,', icon: null }
  ];

  const helpMenuItems = [
    { label: 'User Manual', icon: <HelpCircle size={12} /> },
    { label: 'Keyboard Shortcuts', shortcut: 'Ctrl+?', icon: null },
    { label: 'Video Tutorials', icon: null, separator: true },
    { label: 'Contact Support', icon: null },
    { label: 'Report Bug', icon: null, separator: true },
    { label: `About Finxept v${APP_VERSION}`, icon: null }
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
          <DropdownMenu label="Tabs" items={tabsMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="Session" items={sessionMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="API" items={apiMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="Database" items={databaseMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="View" items={viewMenuItems} onItemClick={handleMenuAction} />
          <DropdownMenu label="Tools" items={toolsMenuItems} onItemClick={handleMenuAction} />
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

      {/* Main Navigation Bar with Tabs */}
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
            <TabsTrigger 
              value="dashboard" 
              style={activeTab === 'dashboard' ? tabStyles.active : tabStyles.default}
            >
              Dashboard
            </TabsTrigger>
            <TabsTrigger 
              value="markets" 
              style={activeTab === 'markets' ? tabStyles.active : tabStyles.default}
            >
              Markets
            </TabsTrigger>
            <TabsTrigger 
              value="news" 
              style={activeTab === 'news' ? tabStyles.active : tabStyles.default}
            >
              News
            </TabsTrigger>
            <TabsTrigger
              value="forum"
              style={activeTab === 'forum' ? tabStyles.active : tabStyles.default}
            >
              Forum
            </TabsTrigger>
            <TabsTrigger
              value="watchlist"
              style={activeTab === 'watchlist' ? tabStyles.active : tabStyles.default}
            >
              Watchlist
            </TabsTrigger>
            <TabsTrigger
              value="geopolitics"
              style={activeTab === 'geopolitics' ? tabStyles.active : tabStyles.default}
            >
              Geopolitics
            </TabsTrigger>
            <TabsTrigger
              value="chat"
              style={activeTab === 'chat' ? tabStyles.active : tabStyles.default}
            >
              AI Chat
            </TabsTrigger>
            <TabsTrigger
              value="fyers"
              style={activeTab === 'fyers' ? tabStyles.active : tabStyles.default}
            >
              Fyers
            </TabsTrigger>
            <TabsTrigger
              value="mcp"
              style={activeTab === 'mcp' ? tabStyles.active : tabStyles.default}
            >
              MCP
            </TabsTrigger>
            <TabsTrigger
              value="profile"
              style={activeTab === 'profile' ? tabStyles.active : tabStyles.default}
            >
              Profile
            </TabsTrigger>
            <TabsTrigger
              value="marketplace"
              style={activeTab === 'marketplace' ? tabStyles.active : tabStyles.default}
            >
              Marketplace
            </TabsTrigger>
            <TabsTrigger
              value="portfolio"
              style={activeTab === 'portfolio' ? tabStyles.active : tabStyles.default}
            >
              Portfolio
            </TabsTrigger>
            <TabsTrigger
              value="analytics"
              style={activeTab === 'analytics' ? tabStyles.active : tabStyles.default}
            >
              Analytics
            </TabsTrigger>
            <TabsTrigger
              value="research"
              style={activeTab === 'research' ? tabStyles.active : tabStyles.default}
            >
              Equity Research
            </TabsTrigger>
            <TabsTrigger
              value="polygon"
              style={activeTab === 'polygon' ? tabStyles.active : tabStyles.default}
            >
              Polygon EQ
            </TabsTrigger>
            <TabsTrigger
              value="options"
              style={activeTab === 'options' ? tabStyles.active : tabStyles.default}
            >
              Options
            </TabsTrigger>
            <TabsTrigger
              value="calendar"
              style={activeTab === 'calendar' ? tabStyles.active : tabStyles.default}
            >
              Economic Calendar
            </TabsTrigger>
            <TabsTrigger
              value="screener"
              style={activeTab === 'screener' ? tabStyles.active : tabStyles.default}
            >
              Screener
            </TabsTrigger>
            <TabsTrigger
              value="dbnomics"
              style={activeTab === 'dbnomics' ? tabStyles.active : tabStyles.default}
            >
              DBnomics
            </TabsTrigger>
            <TabsTrigger
              value="economics"
              style={activeTab === 'economics' ? tabStyles.active : tabStyles.default}
            >
              Economics
            </TabsTrigger>
            <TabsTrigger
              value="code"
              style={activeTab === 'code' ? tabStyles.active : tabStyles.default}
            >
              Code Editor
            </TabsTrigger>
            <TabsTrigger
              value="docs"
              style={activeTab === 'docs' ? tabStyles.active : tabStyles.default}
            >
              Docs
            </TabsTrigger>
            <TabsTrigger
              value="maritime"
              style={activeTab === 'maritime' ? tabStyles.active : tabStyles.default}
            >
              Maritime
            </TabsTrigger>
            <TabsTrigger
              value="kraken"
              style={activeTab === 'kraken' ? tabStyles.active : tabStyles.default}
            >
              Kraken
            </TabsTrigger>
            <TabsTrigger
              value="settings"
              style={activeTab === 'settings' ? tabStyles.active : tabStyles.default}
            >
              Settings
            </TabsTrigger>
            <TabsTrigger
              value="nodes"
              style={activeTab === 'nodes' ? tabStyles.active : tabStyles.default}
            >
              Node Editor
            </TabsTrigger>
            <TabsTrigger
              value="datasources"
              style={activeTab === 'datasources' ? tabStyles.active : tabStyles.default}
            >
              Data Sources
            </TabsTrigger>
            <TabsTrigger
              value="datamapping"
              style={activeTab === 'datamapping' ? tabStyles.active : tabStyles.default}
            >
              Data Mapping
            </TabsTrigger>
            <TabsTrigger
              value="support"
              style={activeTab === 'support' ? tabStyles.active : tabStyles.default}
            >
              Support
            </TabsTrigger>
          </TabsList>
        </Tabs>
        </div>
      </div>

      {/* Command Bar */}
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

      {/* Function Keys Bar */}
      <div style={{
        backgroundColor: '#1a1a1a',
        borderBottom: '1px solid #404040',
        padding: '2px 8px',
        height: '20px',
        display: 'flex',
        alignItems: 'center',
        gap: '16px',
        flexShrink: 0
      }}>
        <span style={{ color: '#fbbf24', fontSize: '10px' }}>F1:HELP</span>
        <span style={{ color: '#fbbf24', fontSize: '10px' }}>F2:MARKETS</span>
        <span style={{ color: '#fbbf24', fontSize: '10px' }}>F3:NEWS</span>
        <span style={{ color: '#fbbf24', fontSize: '10px' }}>F4:PORT</span>
        <span style={{ color: '#fbbf24', fontSize: '10px' }}>F5:MOVERS</span>
        <span style={{ color: '#fbbf24', fontSize: '10px' }}>F6:ECON</span>
        <span style={{ color: '#fbbf24', fontSize: '10px' }}>F11:FULLSCREEN</span>
      </div>

      {/* Main Content Area with Tab Content */}
      <div style={{
        flex: 1,
        backgroundColor: '#000000',
        minHeight: 0,
        overflow: 'hidden'
      }}>
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
          <TabsContent value="research" className="h-full m-0 p-0">
            <EquityResearchTab />
          </TabsContent>
          <TabsContent value="polygon" className="h-full m-0 p-0">
            <PolygonEqTab />
          </TabsContent>
          <TabsContent value="options" className="h-full m-0 p-0">
            <OptionsTab />
          </TabsContent>
          <TabsContent value="calendar" className="h-full m-0 p-0">
            <EconomicCalendarTab />
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
          <TabsContent value="kraken" className="h-full m-0 p-0">
            <KrakenTab />
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
        </Tabs>
      </div>

      {/* Update Available Dialog */}
      <Dialog open={updateAvailable} onOpenChange={(open) => !open && dismissUpdate()}>
        <DialogContent className="sm:max-w-[425px]">
          <DialogHeader>
            <DialogTitle>Update Available</DialogTitle>
            <DialogDescription>
              A new version of FinceptTerminal is available.
            </DialogDescription>
          </DialogHeader>
          <div className="py-4">
            <div className="mb-4">
              <p className="text-sm font-semibold mb-2">Version: {updateInfo?.version}</p>
              {updateInfo?.body && (
                <div className="text-sm text-muted-foreground">
                  <p className="font-semibold mb-1">Release Notes:</p>
                  <div className="max-h-48 overflow-y-auto bg-muted p-3 rounded-md">
                    <pre className="whitespace-pre-wrap text-xs">{updateInfo.body}</pre>
                  </div>
                </div>
              )}
            </div>
            <p className="text-xs text-muted-foreground">
              The update will download and install automatically. Your app will restart after installation.
            </p>
          </div>
          <DialogFooter>
            <Button variant="outline" onClick={dismissUpdate}>
              Later
            </Button>
            <Button onClick={installUpdate}>
              Update Now
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </div>
  );
}