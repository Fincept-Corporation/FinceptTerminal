import React, { useState, useEffect, useCallback } from 'react';
import { RefreshCw, StopCircle } from 'lucide-react';
import { listen } from '@tauri-apps/api/event';
import type { AlgoDeployment } from '../types';
import { listAlgoDeployments, stopAllAlgoDeployments } from '../services/algoTradingService';
import DeploymentCard from './DeploymentCard';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', MUTED: '#4A4A4A',
  CYAN: '#00E5FF', YELLOW: '#FFD700',
};

const MonitorDashboard: React.FC = () => {
  const [deployments, setDeployments] = useState<AlgoDeployment[]>([]);
  const [loading, setLoading] = useState(true);
  const [recentEvents, setRecentEvents] = useState<{ type: string; message: string; ts: number }[]>([]);

  const loadDeployments = useCallback(async () => {
    const result = await listAlgoDeployments();
    if (result.success && result.data) {
      setDeployments(result.data);
    }
    setLoading(false);
  }, []);

  useEffect(() => {
    loadDeployments();
    const interval = setInterval(loadDeployments, 5000);
    return () => clearInterval(interval);
  }, [loadDeployments]);

  // Listen for Tauri events from the backend
  useEffect(() => {
    const addEvent = (type: string, message: string) => {
      setRecentEvents(prev => [{ type, message, ts: Date.now() }, ...prev].slice(0, 20));
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

    return () => {
      unlisteners.forEach(fn => fn());
    };
  }, [loadDeployments]);

  const handleStopAll = async () => {
    await stopAllAlgoDeployments();
    loadDeployments();
  };

  const running = deployments.filter(d => d.status === 'running');
  const stopped = deployments.filter(d => d.status !== 'running');

  const totalPnl = deployments.reduce((acc, d) => acc + d.metrics.total_pnl + d.metrics.unrealized_pnl, 0);
  const totalTrades = deployments.reduce((acc, d) => acc + d.metrics.total_trades, 0);

  const eventColor = (type: string): string => {
    if (type === 'trade') return F.GREEN;
    if (type === 'error') return F.RED;
    if (type === 'signal') return F.YELLOW;
    return F.GRAY;
  };

  return (
    <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Header + Summary */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
            DEPLOYMENTS
          </span>
          <div style={{ display: 'flex', gap: '12px', fontSize: '10px' }}>
            <span style={{ color: F.GRAY }}>
              ACTIVE: <span style={{ color: F.GREEN, fontWeight: 700 }}>{running.length}</span>
            </span>
            <span style={{ color: F.GRAY }}>
              TOTAL P&L: <span style={{ color: totalPnl >= 0 ? F.GREEN : F.RED, fontWeight: 700 }}>
                {totalPnl >= 0 ? '+' : ''}{totalPnl.toFixed(2)}
              </span>
            </span>
            <span style={{ color: F.GRAY }}>
              TRADES: <span style={{ color: F.CYAN, fontWeight: 700 }}>{totalTrades}</span>
            </span>
          </div>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={loadDeployments}
            style={{
              padding: '6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
              color: F.GRAY, cursor: 'pointer', borderRadius: '2px', transition: 'all 0.2s',
            }}
          >
            <RefreshCw size={12} />
          </button>
          {running.length > 0 && (
            <button
              onClick={handleStopAll}
              style={{
                padding: '4px 10px', backgroundColor: `${F.RED}20`, color: F.RED,
                border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
                cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px',
                transition: 'all 0.2s',
              }}
            >
              <StopCircle size={12} />
              STOP ALL
            </button>
          )}
        </div>
      </div>

      {loading ? (
        <div style={{ textAlign: 'center', padding: '32px', fontSize: '10px', color: F.MUTED }}>
          Loading deployments...
        </div>
      ) : deployments.length === 0 ? (
        <div style={{
          display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
          height: '200px', color: F.MUTED, fontSize: '10px', textAlign: 'center',
        }}>
          <span>No deployments yet</span>
          <span style={{ fontSize: '9px', color: F.MUTED, marginTop: '4px' }}>
            Create a strategy and deploy it to see it here
          </span>
        </div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          {running.map(d => (
            <DeploymentCard key={d.id} deployment={d} onStopped={loadDeployments} />
          ))}
          {stopped.length > 0 && running.length > 0 && (
            <div style={{
              fontSize: '9px', color: F.MUTED, letterSpacing: '0.5px',
              fontWeight: 700, paddingTop: '8px',
            }}>
              HISTORY
            </div>
          )}
          {stopped.map(d => (
            <DeploymentCard key={d.id} deployment={d} onStopped={loadDeployments} />
          ))}
        </div>
      )}

      {/* Recent Events */}
      {recentEvents.length > 0 && (
        <div style={{
          backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
        }}>
          <div style={{
            padding: '6px 12px', backgroundColor: F.HEADER_BG,
            borderBottom: `1px solid ${F.BORDER}`,
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
              RECENT EVENTS
            </span>
          </div>
          <div style={{ padding: '6px 12px', maxHeight: '120px', overflowY: 'auto' }}>
            {recentEvents.map((evt, i) => (
              <div key={i} style={{
                display: 'flex', gap: '8px', fontSize: '9px', padding: '2px 0',
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                <span style={{ color: F.MUTED }}>{new Date(evt.ts).toLocaleTimeString()}</span>
                <span style={{ color: eventColor(evt.type) }}>{evt.message}</span>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};

export default MonitorDashboard;
