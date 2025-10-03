// Fyers Broker Integration Tab
// Complete trading dashboard for Fyers broker

import React, { useState, useEffect } from 'react';
import { RefreshCw, Power, TrendingUp, Wallet, Package, FileText, Activity } from 'lucide-react';
import { fyersService } from '../../../services/fyersService';
import { fyersWebSocket } from '../../../services/fyersWebSocket';
import FyersAuthModal from './FyersAuthModal';
import FyersProfilePanel from './FyersProfilePanel';
import FyersFundsPanel from './FyersFundsPanel';
import FyersHoldingsPanel from './FyersHoldingsPanel';
import FyersPositionsPanel from './FyersPositionsPanel';
import FyersOrdersPanel from './FyersOrdersPanel';
import FyersMarketDataPanel from './FyersMarketDataPanel';
import FyersTradingPanel from './FyersTradingPanel';

const FyersTab: React.FC = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [isAuthModalOpen, setIsAuthModalOpen] = useState(false);
  const [isLoading, setIsLoading] = useState(true);
  const [activePanel, setActivePanel] = useState<'overview' | 'holdings' | 'positions' | 'orders' | 'market' | 'trading'>('overview');
  const [wsConnected, setWsConnected] = useState(false);

  // Bloomberg colors
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_RED = '#FF0000';

  useEffect(() => {
    checkConnection();
  }, []);

  const checkConnection = async () => {
    setIsLoading(true);
    try {
      // Ensure SQLite is initialized first
      const { sqliteService } = await import('../../../services/sqliteService');
      await sqliteService.initialize();

      const initialized = await fyersService.initialize();
      if (initialized) {
        // Test connection with profile fetch
        await fyersService.getProfile();
        setIsConnected(true);

        // Load symbol master in background
        fyersService.loadSymbolMaster().catch(console.error);
      } else {
        setIsConnected(false);
      }
    } catch (error) {
      console.error('[Fyers] Connection check failed:', error);
      setIsConnected(false);
    } finally {
      setIsLoading(false);
    }
  };

  const handleConnect = async () => {
    await checkConnection();
    setIsAuthModalOpen(false);
  };

  const handleDisconnect = () => {
    fyersWebSocket.disconnect();
    setWsConnected(false);
    setIsConnected(false);
  };

  const toggleWebSocket = async () => {
    // Check if market is open
    const marketOpen = fyersService.isMarketOpen();

    if (wsConnected) {
      fyersWebSocket.disconnect();
      setWsConnected(false);
    } else {
      if (!marketOpen) {
        console.warn('[Fyers] Cannot start WebSocket - market is closed');
        alert('Market is currently closed. Live updates are not available.');
        return;
      }

      try {
        // Get credentials from service (already loaded)
        const creds = await fyersService.initialize();
        if (creds) {
          // Access credentials from sqliteService
          const { sqliteService } = await import('../../../services/sqliteService');
          const savedCreds = await sqliteService.getCredentialByService('fyers');

          if (savedCreds && savedCreds.api_key && savedCreds.password) {
            fyersWebSocket.connect(savedCreds.api_key, savedCreds.password);
            setWsConnected(true);
          }
        }
      } catch (error) {
        console.error('[Fyers] WebSocket connection failed:', error);
      }
    }
  };

  if (isLoading) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: BLOOMBERG_DARK_BG,
        color: BLOOMBERG_WHITE,
        fontFamily: 'Consolas, monospace',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        fontSize: '11px'
      }}>
        <div style={{ textAlign: 'center' }}>
          <div style={{ color: BLOOMBERG_ORANGE, marginBottom: '10px' }}>Loading Fyers...</div>
          <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px' }}>Checking connection status</div>
        </div>
      </div>
    );
  }

  if (!isConnected) {
    return (
      <>
        <div style={{
          height: '100%',
          backgroundColor: BLOOMBERG_DARK_BG,
          color: BLOOMBERG_WHITE,
          fontFamily: 'Consolas, monospace',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          flexDirection: 'column',
          fontSize: '11px'
        }}>
          <div style={{
            textAlign: 'center',
            padding: '40px',
            border: `2px solid ${BLOOMBERG_ORANGE}`,
            backgroundColor: BLOOMBERG_PANEL_BG
          }}>
            <div style={{
              color: BLOOMBERG_ORANGE,
              fontSize: '14px',
              fontWeight: 'bold',
              marginBottom: '10px'
            }}>
              FYERS NOT CONNECTED
            </div>
            <div style={{
              color: BLOOMBERG_GRAY,
              fontSize: '10px',
              marginBottom: '20px',
              lineHeight: '1.5'
            }}>
              Connect your Fyers broker account to access<br />
              trading, portfolio, and market data features.
            </div>
            <button
              onClick={() => setIsAuthModalOpen(true)}
              style={{
                backgroundColor: BLOOMBERG_ORANGE,
                color: 'black',
                border: 'none',
                padding: '10px 20px',
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}
            >
              CONNECT FYERS
            </button>
          </div>
        </div>

        {isAuthModalOpen && (
          <FyersAuthModal
            onClose={() => setIsAuthModalOpen(false)}
            onSuccess={handleConnect}
          />
        )}
      </>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px'
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '8px 12px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold', fontSize: '12px' }}>
            FYERS BROKER
          </span>
          <span style={{ color: BLOOMBERG_GRAY }}>|</span>
          <span style={{
            color: wsConnected ? BLOOMBERG_GREEN : BLOOMBERG_GRAY,
            fontSize: '9px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}>
            <Activity size={10} />
            {wsConnected ? 'LIVE' : 'OFFLINE'}
          </span>
        </div>

        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={toggleWebSocket}
            style={{
              backgroundColor: wsConnected ? BLOOMBERG_RED : BLOOMBERG_GREEN,
              color: wsConnected ? BLOOMBERG_WHITE : 'black',
              border: 'none',
              padding: '4px 8px',
              fontSize: '9px',
              cursor: 'pointer',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <Activity size={10} />
            {wsConnected ? 'STOP LIVE' : 'START LIVE'}
          </button>

          <button
            onClick={() => window.location.reload()}
            style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '4px 8px',
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <RefreshCw size={10} />
            REFRESH
          </button>

          <button
            onClick={handleDisconnect}
            style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_RED}`,
              color: BLOOMBERG_RED,
              padding: '4px 8px',
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <Power size={10} />
            DISCONNECT
          </button>
        </div>
      </div>

      {/* Navigation Tabs */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '0 12px',
        display: 'flex',
        gap: '2px',
        flexShrink: 0
      }}>
        {[
          { id: 'overview', label: 'OVERVIEW', icon: TrendingUp },
          { id: 'holdings', label: 'HOLDINGS', icon: Package },
          { id: 'positions', label: 'POSITIONS', icon: Activity },
          { id: 'orders', label: 'ORDERS', icon: FileText },
          { id: 'market', label: 'MARKET', icon: Wallet },
          { id: 'trading', label: 'TRADING', icon: TrendingUp }
        ].map(({ id, label, icon: Icon }) => (
          <button
            key={id}
            onClick={() => setActivePanel(id as any)}
            style={{
              backgroundColor: activePanel === id ? BLOOMBERG_DARK_BG : 'transparent',
              border: 'none',
              borderBottom: activePanel === id ? `2px solid ${BLOOMBERG_ORANGE}` : '2px solid transparent',
              color: activePanel === id ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY,
              padding: '8px 12px',
              fontSize: '9px',
              cursor: 'pointer',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <Icon size={10} />
            {label}
          </button>
        ))}
      </div>

      {/* Content Area */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {activePanel === 'overview' && (
          <div style={{ padding: '12px' }}>
            <FyersProfilePanel />
            <FyersFundsPanel />
          </div>
        )}

        {activePanel === 'holdings' && <FyersHoldingsPanel />}
        {activePanel === 'positions' && <FyersPositionsPanel wsConnected={wsConnected} />}
        {activePanel === 'orders' && <FyersOrdersPanel wsConnected={wsConnected} />}
        {activePanel === 'market' && <FyersMarketDataPanel />}
        {activePanel === 'trading' && <FyersTradingPanel />}
      </div>
    </div>
  );
};

export default FyersTab;
