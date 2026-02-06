/**
 * Tauri Events Monitor Tab
 *
 * Real-time monitoring of Tauri events and WebSocket connections
 * Shows active listeners, event flow, and performance metrics
 */

import React, { useState, useEffect, useRef } from 'react';
import { listen, UnlistenFn } from '@tauri-apps/api/event';
import { Activity, Zap, Circle, Wifi, WifiOff, TrendingUp, Clock, Database, AlertCircle, Eye, EyeOff, Trash2 } from 'lucide-react';
// Note: Broker contexts are optional and may not be available in all tabs
// We'll infer connection status from event activity instead

// Event data structures
interface EventLog {
  id: string;
  timestamp: number;
  eventName: string;
  payload: any;
  source: 'stock' | 'crypto' | 'system';
}

interface EventStats {
  eventName: string;
  count: number;
  lastReceived: number;
  avgFrequency: number; // events per second
  source: 'stock' | 'crypto' | 'system';
}

interface WebSocketConnection {
  id: string;
  broker: string;
  type: 'stock' | 'crypto';
  status: 'connected' | 'disconnected' | 'error';
  subscribedSymbols: string[];
  lastActivity: number;
  totalTicks: number;
}

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
};

export default function TauriEventsMonitorTab() {
  // Note: We don't use broker contexts here since they may not be available
  // Instead, we infer connection status from event activity

  // State
  const [eventLogs, setEventLogs] = useState<EventLog[]>([]);
  const [eventStats, setEventStats] = useState<Map<string, EventStats>>(new Map());
  const [wsConnections, setWsConnections] = useState<WebSocketConnection[]>([]);
  const [activeListeners, setActiveListeners] = useState<string[]>([]);
  const [isPaused, setIsPaused] = useState(false);
  const [showPayload, setShowPayload] = useState(false);
  const [maxLogs, setMaxLogs] = useState(100);
  const [filterSource, setFilterSource] = useState<'all' | 'stock' | 'crypto' | 'system'>('all');

  const unlistenersRef = useRef<UnlistenFn[]>([]);
  const statsIntervalRef = useRef<NodeJS.Timeout | null>(null);
  const isPausedRef = useRef(false);

  // Sync isPaused state with ref
  useEffect(() => {
    isPausedRef.current = isPaused;
  }, [isPaused]);

  // List of known Tauri events to monitor
  const MONITORED_EVENTS = [
    // ============================================================================
    // CRYPTO TRADING (via websocketBridge - unified system)
    // ============================================================================
    'ws_ticker',        // Price updates from any crypto exchange
    'ws_orderbook',     // Order book updates
    'ws_trade',         // Trade executions
    'ws_candle',        // OHLC candle data
    'ws_status',        // WebSocket connection status

    // ============================================================================
    // INDIAN STOCK BROKERS (individual broker events)
    // ============================================================================
    'zerodha_ticker',
    'zerodha_status',
    'zerodha_orderbook',
    'zerodha_trade',
    'zerodha_candle',

    'fyers_ticker',
    'fyers_orderbook',
    'fyers_trade',
    'fyers_status',
    'fyers_candle',

    'angelone_ticker',
    'angelone_orderbook',
    'angelone_trade',
    'angelone_status',
    'angelone_candle',

    'upstox_ticker',
    'upstox_orderbook',
    'upstox_trade',
    'upstox_status',
    'upstox_candle',

    'dhan_ticker',
    'dhan_orderbook',
    'dhan_trade',
    'dhan_status',
    'dhan_candle',

    'kotak_ticker',
    'kotak_orderbook',
    'kotak_trade',
    'kotak_status',
    'kotak_candle',

    'groww_ticker',
    'groww_orderbook',
    'groww_trade',
    'groww_status',
    'groww_candle',

    'aliceblue_ticker',
    'aliceblue_orderbook',
    'aliceblue_trade',
    'aliceblue_status',
    'aliceblue_candle',

    'fivepaisa_ticker',
    'fivepaisa_orderbook',
    'fivepaisa_trade',
    'fivepaisa_status',
    'fivepaisa_candle',

    'iifl_ticker',
    'iifl_orderbook',
    'iifl_trade',
    'iifl_status',
    'iifl_candle',

    'motilal_ticker',
    'motilal_orderbook',
    'motilal_trade',
    'motilal_status',
    'motilal_candle',

    'shoonya_ticker',
    'shoonya_orderbook',
    'shoonya_trade',
    'shoonya_status',
    'shoonya_candle',

    // ============================================================================
    // US STOCK BROKERS
    // ============================================================================
    'alpaca_trade',
    'alpaca_quote',
    'alpaca_bar',
    'alpaca_status',

    'ibkr_ticker',
    'ibkr_orderbook',
    'ibkr_trade',
    'ibkr_status',

    'tradier_ticker',
    'tradier_orderbook',
    'tradier_trade',
    'tradier_status',

    // ============================================================================
    // MONITORING & ALERTS
    // ============================================================================
    'monitor_alert',    // Price/condition alerts from monitoring system

    // ============================================================================
    // AI AGENT STREAMING
    // ============================================================================
    // Note: Agent streaming uses dynamic event names: agent_stream_{session_id}
    // We can't listen to these specifically, but they show activity

    // ============================================================================
    // ALPHA ARENA (AI Trading Competition)
    // ============================================================================
    'alpha-arena-progress',  // Competition setup/execution progress

    // ============================================================================
    // DATABENTO (Market Data Provider)
    // ============================================================================
    // Note: Databento uses dynamic event names: databento-live-{stream_id}

    // ============================================================================
    // SETUP & SYSTEM
    // ============================================================================
    'setup-progress',   // Initial app setup progress (Python, Bun, packages)

    // ============================================================================
    // TAURI SYSTEM EVENTS
    // ============================================================================
    'tauri://update-status',
    'tauri://file-drop',
    'tauri://window-created',
    'tauri://focus',
    'tauri://blur',
  ];

  // Initialize event listeners
  useEffect(() => {
    const setupListeners = async () => {
      const unlisteners: UnlistenFn[] = [];

      for (const eventName of MONITORED_EVENTS) {
        try {
          const unlisten = await listen(eventName, (event) => {
            if (isPausedRef.current) return;

            // Determine event source
            const source: 'stock' | 'crypto' | 'system' = (() => {
              // Crypto events (ws_* prefix for unified crypto system)
              if (eventName.startsWith('ws_')) return 'crypto';

              // Stock broker events (specific broker names)
              if (
                eventName.includes('alpaca') || eventName.includes('zerodha') || eventName.includes('fyers') ||
                eventName.includes('angelone') || eventName.includes('upstox') || eventName.includes('dhan') ||
                eventName.includes('ibkr') || eventName.includes('tradier') || eventName.includes('kotak') ||
                eventName.includes('groww') || eventName.includes('aliceblue') || eventName.includes('fivepaisa') ||
                eventName.includes('iifl') || eventName.includes('motilal') || eventName.includes('shoonya')
              ) {
                return 'stock';
              }

              // Everything else is system
              return 'system';
            })();

            // Add to event logs
            const log: EventLog = {
              id: `${Date.now()}-${Math.random()}`,
              timestamp: Date.now(),
              eventName,
              payload: event.payload,
              source,
            };

            setEventLogs(prev => {
              const updated = [log, ...prev].slice(0, maxLogs);
              return updated;
            });

            // Update stats
            setEventStats(prev => {
              const newStats = new Map(prev);
              const existing = newStats.get(eventName);

              if (existing) {
                const timeDiff = (Date.now() - existing.lastReceived) / 1000;
                const newFreq = timeDiff > 0 ? 1 / timeDiff : 0;
                const avgFreq = (existing.avgFrequency * existing.count + newFreq) / (existing.count + 1);

                newStats.set(eventName, {
                  ...existing,
                  count: existing.count + 1,
                  lastReceived: Date.now(),
                  avgFrequency: avgFreq,
                });
              } else {
                newStats.set(eventName, {
                  eventName,
                  count: 1,
                  lastReceived: Date.now(),
                  avgFrequency: 0,
                  source,
                });
              }

              return newStats;
            });
          });

          unlisteners.push(unlisten);
        } catch (err) {
          console.warn(`Failed to listen to ${eventName}:`, err);
        }
      }

      unlistenersRef.current = unlisteners;
      setActiveListeners(MONITORED_EVENTS);
    };

    setupListeners();

    return () => {
      unlistenersRef.current.forEach(unlisten => unlisten());
    };
  }, [isPaused, maxLogs]);

  // Update WebSocket connections info based on event activity
  useEffect(() => {
    const updateConnections = () => {
      const connections: WebSocketConnection[] = [];

      // Infer stock broker connections from event activity
      const stockEvents = Array.from(eventStats.values()).filter(s => s.source === 'stock');
      if (stockEvents.length > 0) {
        const recentActivity = stockEvents.some(e => Date.now() - e.lastReceived < 30000); // Active in last 30s
        const totalStockTicks = stockEvents.reduce((sum, s) => sum + s.count, 0);

        // Group by broker type (inferred from event name)
        const brokerGroups = new Map<string, EventStats[]>();
        stockEvents.forEach(event => {
          const brokerName = event.eventName.split('_')[0]; // e.g., 'zerodha' from 'zerodha_ticker'
          if (!brokerGroups.has(brokerName)) {
            brokerGroups.set(brokerName, []);
          }
          brokerGroups.get(brokerName)!.push(event);
        });

        brokerGroups.forEach((events, brokerName) => {
          const isActive = events.some(e => Date.now() - e.lastReceived < 30000);
          const totalTicks = events.reduce((sum, e) => sum + e.count, 0);

          connections.push({
            id: `stock-${brokerName}`,
            broker: brokerName.charAt(0).toUpperCase() + brokerName.slice(1),
            type: 'stock',
            status: isActive ? 'connected' : 'disconnected',
            subscribedSymbols: [],
            lastActivity: Math.max(...events.map(e => e.lastReceived)),
            totalTicks,
          });
        });
      }

      // Infer crypto broker connections from event activity
      const cryptoEvents = Array.from(eventStats.values()).filter(s => s.source === 'crypto');
      if (cryptoEvents.length > 0) {
        const brokerGroups = new Map<string, EventStats[]>();
        cryptoEvents.forEach(event => {
          const brokerName = event.eventName.split('_')[0]; // e.g., 'binance' from 'binance_ticker'
          if (!brokerGroups.has(brokerName)) {
            brokerGroups.set(brokerName, []);
          }
          brokerGroups.get(brokerName)!.push(event);
        });

        brokerGroups.forEach((events, brokerName) => {
          const isActive = events.some(e => Date.now() - e.lastReceived < 30000);
          const totalTicks = events.reduce((sum, e) => sum + e.count, 0);

          connections.push({
            id: `crypto-${brokerName}`,
            broker: brokerName.charAt(0).toUpperCase() + brokerName.slice(1),
            type: 'crypto',
            status: isActive ? 'connected' : 'disconnected',
            subscribedSymbols: [],
            lastActivity: Math.max(...events.map(e => e.lastReceived)),
            totalTicks,
          });
        });
      }

      setWsConnections(connections);
    };

    updateConnections();
    const interval = setInterval(updateConnections, 2000);
    statsIntervalRef.current = interval;

    return () => {
      if (statsIntervalRef.current) {
        clearInterval(statsIntervalRef.current);
      }
    };
  }, [eventStats]);

  // Format timestamp
  const formatTime = (timestamp: number) => {
    const date = new Date(timestamp);
    const ms = String(date.getMilliseconds()).padStart(3, '0');
    return date.toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' }) + `.${ms}`;
  };

  // Clear logs
  const clearLogs = () => {
    setEventLogs([]);
    setEventStats(new Map());
  };

  // Filter logs
  const filteredLogs = filterSource === 'all'
    ? eventLogs
    : eventLogs.filter(log => log.source === filterSource);

  const filteredStats = Array.from(eventStats.values())
    .filter(stat => filterSource === 'all' || stat.source === filterSource)
    .sort((a, b) => b.count - a.count);

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      display: 'flex',
      flexDirection: 'column',
      fontFamily: 'Consolas, "Courier New", monospace',
      fontSize: '11px',
    }}>
      {/* Header */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Activity size={16} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '13px', fontWeight: 'bold' }}>Tauri Events Monitor</span>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginLeft: '8px' }}>
            <Circle size={8} color={FINCEPT.GREEN} fill={FINCEPT.GREEN} />
            <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
              {activeListeners.length} listeners active
            </span>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {/* Filter */}
          <select
            value={filterSource}
            onChange={(e) => setFilterSource(e.target.value as any)}
            style={{
              backgroundColor: FINCEPT.PANEL_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '3px',
              padding: '4px 8px',
              fontSize: '10px',
            }}
          >
            <option value="all">All Sources</option>
            <option value="stock">Stock Brokers</option>
            <option value="crypto">Crypto Exchanges</option>
            <option value="system">System Events</option>
          </select>

          {/* Toggle payload */}
          <button
            onClick={() => setShowPayload(!showPayload)}
            style={{
              backgroundColor: showPayload ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '3px',
              padding: '4px 8px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            {showPayload ? <Eye size={12} /> : <EyeOff size={12} />}
            <span>Payload</span>
          </button>

          {/* Pause/Resume */}
          <button
            onClick={() => setIsPaused(!isPaused)}
            style={{
              backgroundColor: isPaused ? FINCEPT.RED : FINCEPT.GREEN,
              color: FINCEPT.WHITE,
              border: 'none',
              borderRadius: '3px',
              padding: '4px 12px',
              cursor: 'pointer',
              fontWeight: 'bold',
            }}
          >
            {isPaused ? 'PAUSED' : 'LIVE'}
          </button>

          {/* Clear */}
          <button
            onClick={clearLogs}
            style={{
              backgroundColor: FINCEPT.PANEL_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '3px',
              padding: '4px 8px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <Trash2 size={12} />
            <span>Clear</span>
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', gap: '8px', padding: '8px', overflow: 'hidden' }}>
        {/* Left Panel - WebSocket Connections & Stats */}
        <div style={{ width: '400px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
          {/* WebSocket Connections */}
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            padding: '12px',
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <Wifi size={14} color={FINCEPT.CYAN} />
              <span style={{ fontWeight: 'bold' }}>WebSocket Connections</span>
            </div>

            {wsConnections.length === 0 ? (
              <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '20px' }}>
                No active WebSocket connections
              </div>
            ) : (
              wsConnections.map(conn => (
                <div
                  key={conn.id}
                  style={{
                    padding: '8px',
                    marginBottom: '8px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '3px',
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <Circle
                        size={8}
                        color={conn.status === 'connected' ? FINCEPT.GREEN : FINCEPT.RED}
                        fill={conn.status === 'connected' ? FINCEPT.GREEN : FINCEPT.RED}
                      />
                      <span style={{ fontWeight: 'bold' }}>{conn.broker}</span>
                    </div>
                    <span style={{
                      fontSize: '9px',
                      color: FINCEPT.WHITE,
                      backgroundColor: conn.type === 'stock' ? FINCEPT.CYAN : FINCEPT.ORANGE,
                      padding: '2px 6px',
                      borderRadius: '2px',
                    }}>
                      {conn.type.toUpperCase()}
                    </span>
                  </div>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                    <div>Status: <span style={{ color: conn.status === 'connected' ? FINCEPT.GREEN : FINCEPT.RED }}>
                      {conn.status.toUpperCase()}
                    </span></div>
                    <div>Total Ticks: <span style={{ color: FINCEPT.WHITE }}>{conn.totalTicks.toLocaleString()}</span></div>
                  </div>
                </div>
              ))
            )}
          </div>

          {/* Event Statistics */}
          <div style={{
            flex: 1,
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            padding: '12px',
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              marginBottom: '12px',
              paddingBottom: '8px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <TrendingUp size={14} color={FINCEPT.YELLOW} />
              <span style={{ fontWeight: 'bold' }}>Event Statistics</span>
            </div>

            <div style={{ flex: 1, overflowY: 'auto' }}>
              {filteredStats.length === 0 ? (
                <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '20px' }}>
                  No events received yet
                </div>
              ) : (
                filteredStats.map(stat => (
                  <div
                    key={stat.eventName}
                    style={{
                      padding: '8px',
                      marginBottom: '6px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '3px',
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                      <span style={{
                        fontWeight: 'bold',
                        color: stat.source === 'stock' ? FINCEPT.CYAN :
                               stat.source === 'crypto' ? FINCEPT.ORANGE : FINCEPT.YELLOW
                      }}>
                        {stat.eventName}
                      </span>
                      <span style={{ color: FINCEPT.WHITE }}>{stat.count}</span>
                    </div>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                      Avg: {stat.avgFrequency.toFixed(2)} events/s | Last: {formatTime(stat.lastReceived)}
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>
        </div>

        {/* Right Panel - Event Logs */}
        <div style={{
          flex: 1,
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '4px',
          padding: '12px',
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: '12px',
            paddingBottom: '8px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Database size={14} color={FINCEPT.GREEN} />
              <span style={{ fontWeight: 'bold' }}>Event Stream</span>
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                ({filteredLogs.length} / {maxLogs} max)
              </span>
            </div>
          </div>

          <div style={{
            flex: 1,
            overflowY: 'auto',
            fontFamily: 'Consolas, "Courier New", monospace',
            fontSize: '10px',
          }}>
            {filteredLogs.length === 0 ? (
              <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '20px' }}>
                {isPaused ? 'Monitoring paused' : 'Waiting for events...'}
              </div>
            ) : (
              filteredLogs.map(log => (
                <div
                  key={log.id}
                  style={{
                    padding: '8px',
                    marginBottom: '4px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '3px',
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                    <Clock size={10} color={FINCEPT.GRAY} />
                    <span style={{ color: FINCEPT.GRAY }}>{formatTime(log.timestamp)}</span>
                    <Zap size={10} color={
                      log.source === 'stock' ? FINCEPT.CYAN :
                      log.source === 'crypto' ? FINCEPT.ORANGE : FINCEPT.YELLOW
                    } />
                    <span style={{
                      fontWeight: 'bold',
                      color: log.source === 'stock' ? FINCEPT.CYAN :
                             log.source === 'crypto' ? FINCEPT.ORANGE : FINCEPT.YELLOW
                    }}>
                      {log.eventName}
                    </span>
                  </div>

                  {showPayload && (
                    <pre style={{
                      margin: 0,
                      padding: '8px',
                      backgroundColor: '#000',
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      fontSize: '9px',
                      color: FINCEPT.GRAY,
                      maxHeight: '200px',
                      overflow: 'auto',
                    }}>
                      {JSON.stringify(log.payload, null, 2)}
                    </pre>
                  )}
                </div>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
