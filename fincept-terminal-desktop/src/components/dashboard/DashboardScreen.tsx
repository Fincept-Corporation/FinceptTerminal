import React, { useState, useRef, useEffect } from 'react';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { Maximize, Minimize, Download, Settings, RefreshCw, User, Database, Eye, HelpCircle } from 'lucide-react';
import ForumTab from '@/components/tabs/ForumTab';
import DashboardTab from '@/components/tabs/DashboardTab';
import MarketsTab from '@/components/tabs/MarketsTab';
import NewsTab from '@/components/tabs/NewsTab';
import AdvancedTab from '@/components/tabs/AdvancedTab';
import WatchlistTab from '@/components/tabs/WatchlistTab';
import GeopoliticsTab from '@/components/tabs/GeopoliticsTab';

// Dropdown Menu Component
const DropdownMenu = ({ label, items, onItemClick }) => {
  const [isOpen, setIsOpen] = useState(false);
  const menuRef = useRef(null);

  useEffect(() => {
    const handleClickOutside = (event) => {
      if (menuRef.current && !menuRef.current.contains(event.target)) {
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
          {items.map((item, index) => (
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
                  e.target.style.backgroundColor = '#404040';
                  e.target.style.color = '#fff';
                }
              }}
              onMouseLeave={(e) => {
                e.target.style.backgroundColor = 'transparent';
                e.target.style.color = item.disabled ? '#666' : '#a3a3a3';
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
  const [activeTab, setActiveTab] = useState("dashboard");
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [statusMessage, setStatusMessage] = useState("");

  React.useEffect(() => {
    document.body.style.margin = '0';
    document.body.style.padding = '0';
    document.body.style.backgroundColor = '#000000';
    document.body.style.overflow = 'hidden';
    document.body.style.overscrollBehavior = 'none';
    document.documentElement.style.backgroundColor = '#000000';
    document.documentElement.style.overflow = 'hidden';
    document.documentElement.style.overscrollBehavior = 'none';
    
    return () => {
      document.body.style.overflow = 'auto';
      document.body.style.overscrollBehavior = 'auto';
      document.documentElement.style.overflow = 'auto';
      document.documentElement.style.overscrollBehavior = 'auto';
    };
  }, []);

  const toggleFullscreen = () => {
    if (!document.fullscreenElement) {
      document.documentElement.requestFullscreen().then(() => {
        setIsFullscreen(true);
        setStatusMessage("Entered fullscreen mode");
      }).catch(err => {
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

  const handleMenuAction = (item) => {
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
    { label: 'Login', icon: <User size={12} /> },
    { label: 'Switch User', icon: null },
    { label: 'Session Info', icon: null, separator: true },
    { label: 'Extend Session', icon: null },
    { label: 'Logout', icon: null }
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
    { label: 'About Finxept', icon: null }
  ];

  const tabStyles = {
    default: {
      backgroundColor: 'transparent',
      color: '#d4d4d4',
      border: 'none',
      padding: '3px 8px',
      fontSize: '10px',
      cursor: 'pointer',
      borderRadius: '0'
    },
    active: {
      backgroundColor: '#ea580c',
      color: 'white',
      border: 'none',
      padding: '3px 8px',
      fontSize: '10px',
      cursor: 'pointer',
      borderRadius: '0'
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
      overscrollBehavior: 'none'
    }}>
      {/* Top Menu Bar with Functional Dropdowns */}
      <div style={{
        backgroundColor: '#2d2d2d',
        color: '#a3a3a3',
        fontSize: '11px',
        borderBottom: '1px solid #404040',
        padding: '2px 8px',
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
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
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
              e.target.style.backgroundColor = '#404040';
              e.target.style.color = '#fff';
            }}
            onMouseLeave={(e) => {
              e.target.style.backgroundColor = 'transparent';
              e.target.style.color = '#a3a3a3';
            }}
            title={isFullscreen ? 'Exit Fullscreen (F11)' : 'Enter Fullscreen (F11)'}
          >
            {isFullscreen ? <Minimize size={14} /> : <Maximize size={14} />}
          </button>
          <span style={{ color: '#fbbf24' }}>Guest (18h remaining)</span>
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
          alignItems: 'center'
        }}>
          {statusMessage}
        </div>
      )}

      {/* Main Navigation Bar with Tabs */}
      <div style={{
        backgroundColor: '#3f3f3f',
        borderBottom: '1px solid #404040',
        height: '26px'
      }}>
        <Tabs value={activeTab} onValueChange={setActiveTab} className="h-full">
          <TabsList style={{
            backgroundColor: 'transparent',
            border: 'none',
            padding: '2px 4px',
            height: '100%',
            justifyContent: 'flex-start',
            borderRadius: '0'
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
              value="advanced" 
              style={activeTab === 'advanced' ? tabStyles.active : tabStyles.default}
            >
              Advanced
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
          </TabsList>
        </Tabs>
      </div>

      {/* Command Bar */}
      <div style={{
        backgroundColor: '#2d2d2d',
        borderBottom: '1px solid #404040',
        padding: '4px 8px',
        height: '24px',
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <span style={{ backgroundColor: '#ea580c', color: 'white', padding: '2px 6px', fontSize: '9px', fontWeight: 'bold' }}>FINXEPT PROFESSIONAL</span>
        <span style={{ color: '#a3a3a3', fontSize: '10px' }}>Enter Command</span>
        <div style={{ width: '1px', height: '16px', backgroundColor: '#525252' }}></div>
        <span style={{ color: '#a3a3a3', fontSize: '10px' }}>Search</span>
        <div style={{ width: '1px', height: '16px', backgroundColor: '#525252' }}></div>
        <span style={{ color: '#a3a3a3', fontSize: '10px' }}>2025-09-26 14:00:49</span>
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
        gap: '16px'
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
        height: statusMessage ? 'calc(100vh - 123px)' : 'calc(100vh - 107px)',
        backgroundColor: '#000000'
      }}>
        <Tabs value={activeTab} className="h-full">
          <TabsContent value="dashboard" className="h-full m-0 p-0">
            <DashboardTab />
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
          <TabsContent value="advanced" className="h-full m-0 p-0">
            <AdvancedTab />
          </TabsContent>
          <TabsContent value="watchlist" className="h-full m-0 p-0">
            <WatchlistTab />
          </TabsContent>
          <TabsContent value="geopolitics" className="h-full m-0 p-0">
            <GeopoliticsTab />
          </TabsContent>
        </Tabs>
      </div>
    </div>
  );
}