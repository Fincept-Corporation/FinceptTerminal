import React, { useState, useEffect, useCallback } from 'react';
import {
  RefreshCw, StopCircle, Activity, TrendingUp, TrendingDown,
  Target, BarChart3, Radio, Server, Clock,
} from 'lucide-react';
import { listen } from '@tauri-apps/api/event';
import type { AlgoDeployment } from '../types';
import { listAlgoDeployments, stopAllAlgoDeployments } from '../services/algoTradingService';
import DeploymentCard from './DeploymentCard';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', MUTED: '#4A4A4A',
  CYAN: '#00E5FF', YELLOW: '#FFD700', BLUE: '#0088FF', HOVER: '#1F1F1F',
};

const MonitorDashboard: React.FC = () => {
  const [deployments, setDeployments] = useState<AlgoDeployment[]>([]);
  const [loading, setLoading] = useState(true);
  const [recentEvents, setRecentEvents] = useState<{ type: string; message: string; ts: number }[]>([]);
  const [showEvents, setShowEvents] = useState(true);

  const loadDeployments = useCallback(async () => {
    const result = await listAlgoDeployments();
    if (result.success && result.data) setDeployments(result.data);
    setLoading(false);
  }, []);

  useEffect(() => {
    loadDeployments();
    const interval = setInterval(loadDeployments, 5000);
    return () => clearInterval(interval);
  }, [loadDeployments]);

  useEffect(() => {
    const addEvent = (type: string, message: string) => {
      setRecentEvents(prev => [{ type, message, ts: Date.now() }, ...prev].slice(0, 30));
    };
    const unlisteners: (() => void)[] = [];

    listen<{ symbol: string; timeframe: string }>('algo_candle_closed', (event) => {
      addEvent('candle', `${event.payload.symbol} [${event.payload.timeframe}] candle closed`);
    }).then(fn => unlisteners.push(fn));

    listen<{ signal_id: string; symbol: string; side: string; quantity: number; order_type: string }>(
      'algo_signal',
      (event) => {
        const p = event.payload;
        addEvent('signal', `Signal: ${p.side.toUpperCase()} ${p.quantity} ${p.symbol} (${p.order_type})`);
      }
    ).then(fn => unlisteners.push(fn));

    listen<{ signal_id: string; symbol: string; side: string; success: boolean; message: string }>(
      'algo_trade_executed',
      (event) => {
        const p = event.payload;
        addEvent(
          p.success ? 'trade' : 'error',
          `${p.side.toUpperCase()} ${p.symbol}: ${p.success ? 'filled' : 'failed'} — ${p.message}`
        );
        loadDeployments();
      }
    ).then(fn => unlisteners.push(fn));

    return () => { unlisteners.forEach(fn => fn()); };
  }, [loadDeployments]);

  const handleStopAll = async () => {
    await stopAllAlgoDeployments();
    loadDeployments();
  };

  const running = deployments.filter(d => d.status === 'running');
  const stopped = deployments.filter(d => d.status !== 'running');
  const totalPnl = deployments.reduce((acc, d) => acc + d.metrics.total_pnl + d.metrics.unrealized_pnl, 0);
  const totalTrades = deployments.reduce((acc, d) => acc + d.metrics.total_trades, 0);
  const avgWinRate = running.length > 0
    ? running.reduce((acc, d) => acc + d.metrics.win_rate, 0) / running.length : 0;

  const eventColor = (type: string): string => {
    if (type === 'trade') return F.GREEN;
    if (type === 'error') return F.RED;
    if (type === 'signal') return F.YELLOW;
    return F.GRAY;
  };

  const eventIcon = (type: string) => {
    if (type === 'trade') return <TrendingUp size={8} />;
    if (type === 'error') return <StopCircle size={8} />;
    if (type === 'signal') return <Radio size={8} />;
    return <Activity size={8} />;
  };

  return (
    <div style={{
      height: '100%', display: 'flex', flexDirection: 'column',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      {/* ─── SUMMARY CARDS ─── */}
      <div style={{
        display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)',
        borderBottom: `1px solid ${F.BORDER}`, flexShrink: 0,
      }}>
        {/* Active Deployments */}
        <div style={{ padding: '14px 16px', borderRight: `1px solid ${F.BORDER}` }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '6px',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <Server size={8} />
            ACTIVE
          </div>
          <div style={{ fontSize: '20px', fontWeight: 700, color: running.length > 0 ? F.GREEN : F.MUTED }}>
            {running.length}
          </div>
          <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>
            of {deployments.length} total
          </div>
        </div>

        {/* Total P&L */}
        <div style={{
          padding: '14px 16px', borderRight: `1px solid ${F.BORDER}`,
          backgroundColor: `${totalPnl >= 0 ? F.GREEN : F.RED}04`,
        }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '6px',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            {totalPnl >= 0 ? <TrendingUp size={8} /> : <TrendingDown size={8} />}
            TOTAL P&L
          </div>
          <div style={{
            fontSize: '20px', fontWeight: 700,
            color: totalPnl >= 0 ? F.GREEN : F.RED,
          }}>
            {totalPnl >= 0 ? '+' : ''}{totalPnl.toFixed(2)}
          </div>
        </div>

        {/* Total Trades */}
        <div style={{ padding: '14px 16px', borderRight: `1px solid ${F.BORDER}` }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '6px',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <Activity size={8} />
            TOTAL TRADES
          </div>
          <div style={{ fontSize: '20px', fontWeight: 700, color: F.CYAN }}>
            {totalTrades}
          </div>
        </div>

        {/* Avg Win Rate */}
        <div style={{ padding: '14px 16px', borderRight: `1px solid ${F.BORDER}` }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '6px',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <Target size={8} />
            AVG WIN RATE
          </div>
          <div style={{
            fontSize: '20px', fontWeight: 700,
            color: avgWinRate >= 50 ? F.GREEN : avgWinRate > 0 ? F.YELLOW : F.MUTED,
          }}>
            {avgWinRate > 0 ? `${avgWinRate.toFixed(1)}%` : '—'}
          </div>
        </div>

        {/* Events */}
        <div style={{ padding: '14px 16px' }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '6px',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <Radio size={8} />
            EVENTS
          </div>
          <div style={{ fontSize: '20px', fontWeight: 700, color: F.YELLOW }}>
            {recentEvents.length}
          </div>
          <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>
            recent signals
          </div>
        </div>
      </div>

      {/* ─── MAIN AREA: Two-panel split ─── */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden', minHeight: 0 }}>
        {/* LEFT: Deployment Cards */}
        <div style={{
          flex: 1, overflowY: 'auto', minWidth: 0,
          borderRight: `1px solid ${F.BORDER}`,
        }}>
          {/* Toolbar */}
          <div style={{
            padding: '10px 16px', backgroundColor: F.HEADER_BG,
            borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex', alignItems: 'center', justifyContent: 'space-between',
            position: 'sticky', top: 0, zIndex: 1,
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <BarChart3 size={10} color={F.ORANGE} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                DEPLOYMENTS
              </span>
            </div>
            <div style={{ display: 'flex', gap: '4px' }}>
              <button
                onClick={loadDeployments}
                style={{
                  padding: '4px 8px', backgroundColor: 'transparent',
                  border: `1px solid ${F.BORDER}`, color: F.GRAY,
                  cursor: 'pointer', borderRadius: '2px', transition: 'all 0.2s',
                  display: 'flex', alignItems: 'center', gap: '4px',
                  fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
                }}
                onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
              >
                <RefreshCw size={9} />
                REFRESH
              </button>
              {running.length > 0 && (
                <button
                  onClick={handleStopAll}
                  style={{
                    padding: '4px 10px', backgroundColor: `${F.RED}15`,
                    border: `1px solid ${F.RED}30`, color: F.RED,
                    borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                    cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px',
                    transition: 'all 0.2s', letterSpacing: '0.5px',
                  }}
                  onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.RED}25`; }}
                  onMouseLeave={e => { e.currentTarget.style.backgroundColor = `${F.RED}15`; }}
                >
                  <StopCircle size={9} />
                  STOP ALL
                </button>
              )}
            </div>
          </div>

          {/* Deployment List */}
          <div style={{ padding: '12px 16px' }}>
            {loading ? (
              <div style={{
                textAlign: 'center', padding: '48px', fontSize: '10px', color: F.MUTED,
                display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '8px',
              }}>
                <RefreshCw size={16} className="animate-spin" color={F.ORANGE} />
                <span style={{ fontWeight: 700, letterSpacing: '0.5px' }}>LOADING DEPLOYMENTS...</span>
              </div>
            ) : deployments.length === 0 ? (
              <div style={{
                display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
                padding: '48px', color: F.MUTED, textAlign: 'center',
              }}>
                <Server size={24} style={{ opacity: 0.3, marginBottom: '8px' }} />
                <span style={{ fontSize: '10px', fontWeight: 700 }}>NO DEPLOYMENTS</span>
                <span style={{ fontSize: '9px', marginTop: '4px' }}>
                  Create a strategy and deploy it to see it here
                </span>
              </div>
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {running.length > 0 && (
                  <>
                    <div style={{
                      fontSize: '8px', fontWeight: 700, color: F.GREEN,
                      letterSpacing: '0.5px', display: 'flex', alignItems: 'center', gap: '4px',
                    }}>
                      <div style={{
                        width: '6px', height: '6px', borderRadius: '50%', backgroundColor: F.GREEN,
                      }} />
                      RUNNING ({running.length})
                    </div>
                    {running.map(d => (
                      <DeploymentCard key={d.id} deployment={d} onStopped={loadDeployments} />
                    ))}
                  </>
                )}
                {stopped.length > 0 && (
                  <>
                    <div style={{
                      fontSize: '8px', fontWeight: 700, color: F.MUTED,
                      letterSpacing: '0.5px', paddingTop: running.length > 0 ? '8px' : '0',
                      borderTop: running.length > 0 ? `1px solid ${F.BORDER}` : 'none',
                      marginTop: running.length > 0 ? '4px' : '0',
                    }}>
                      HISTORY ({stopped.length})
                    </div>
                    {stopped.map(d => (
                      <DeploymentCard key={d.id} deployment={d} onStopped={loadDeployments} />
                    ))}
                  </>
                )}
              </div>
            )}
          </div>
        </div>

        {/* RIGHT: Event Log Panel */}
        <div style={{
          width: '320px', flexShrink: 0,
          display: 'flex', flexDirection: 'column',
          backgroundColor: F.PANEL_BG,
        }}>
          {/* Event Log Header */}
          <div style={{
            padding: '10px 12px', backgroundColor: F.HEADER_BG,
            borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex', alignItems: 'center', justifyContent: 'space-between',
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Radio size={10} color={F.YELLOW} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                EVENT LOG
              </span>
              {recentEvents.length > 0 && (
                <span style={{
                  padding: '1px 5px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                  backgroundColor: `${F.YELLOW}20`, color: F.YELLOW,
                }}>
                  {recentEvents.length}
                </span>
              )}
            </div>
            {recentEvents.length > 0 && (
              <button
                onClick={() => setRecentEvents([])}
                style={{
                  padding: '2px 8px', backgroundColor: 'transparent',
                  border: `1px solid ${F.BORDER}`, color: F.MUTED,
                  borderRadius: '2px', cursor: 'pointer', fontSize: '8px', fontWeight: 700,
                  letterSpacing: '0.5px',
                }}
                onMouseEnter={e => { e.currentTarget.style.color = F.WHITE; e.currentTarget.style.borderColor = F.ORANGE; }}
                onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; e.currentTarget.style.borderColor = F.BORDER; }}
              >
                CLEAR
              </button>
            )}
          </div>

          {/* Event Entries */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '4px 0' }}>
            {recentEvents.length === 0 ? (
              <div style={{
                padding: '32px 16px', textAlign: 'center',
                display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '6px',
              }}>
                <Radio size={16} color={F.MUTED} style={{ opacity: 0.3 }} />
                <span style={{ fontSize: '9px', color: F.MUTED, fontWeight: 700 }}>
                  NO EVENTS YET
                </span>
                <span style={{ fontSize: '8px', color: F.MUTED }}>
                  Events will appear here when strategies are running
                </span>
              </div>
            ) : (
              recentEvents.map((evt, i) => (
                <div
                  key={i}
                  style={{
                    display: 'flex', gap: '8px', padding: '6px 12px',
                    borderBottom: `1px solid ${F.BORDER}15`,
                    transition: 'background-color 0.15s',
                  }}
                  onMouseEnter={e => { e.currentTarget.style.backgroundColor = F.HOVER; }}
                  onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                >
                  <div style={{ color: eventColor(evt.type), flexShrink: 0, paddingTop: '1px' }}>
                    {eventIcon(evt.type)}
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{
                      fontSize: '9px', color: eventColor(evt.type),
                      lineHeight: '1.4', wordBreak: 'break-word',
                    }}>
                      {evt.message}
                    </div>
                    <div style={{
                      fontSize: '8px', color: F.MUTED, marginTop: '1px',
                      display: 'flex', alignItems: 'center', gap: '3px',
                    }}>
                      <Clock size={7} />
                      {new Date(evt.ts).toLocaleTimeString()}
                    </div>
                  </div>
                </div>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default MonitorDashboard;
