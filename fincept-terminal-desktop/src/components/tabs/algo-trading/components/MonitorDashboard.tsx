import React, { useState, useEffect, useCallback } from 'react';
import {
  RefreshCw, StopCircle, Activity, TrendingUp, TrendingDown,
  Target, BarChart3, Radio, Server, Clock,
} from 'lucide-react';
import { listen } from '@tauri-apps/api/event';
import type { AlgoDeployment } from '../types';
import { listAlgoDeployments, stopAllAlgoDeployments } from '../services/algoTradingService';
import DeploymentCard from './DeploymentCard';
import { F } from '../constants/theme';
import { S } from '../constants/styles';

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
    if (type === 'trade') return <TrendingUp size={10} />;
    if (type === 'error') return <StopCircle size={10} />;
    if (type === 'signal') return <Radio size={10} />;
    return <Activity size={10} />;
  };

  return (
    <div
      className="h-full flex flex-col"
      style={{ fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}
    >
      {/* ─── SUMMARY CARDS ─── */}
      <div
        className="grid grid-cols-5 shrink-0"
        style={{ borderBottom: `1px solid ${F.BORDER}` }}
      >
        {/* Active Deployments */}
        <div className="p-4" style={{ borderRight: `1px solid ${F.BORDER}` }}>
          <div className={S.metricLabel}>
            <Server size={10} />
            ACTIVE
          </div>
          <div className="text-[22px] font-bold" style={{ color: running.length > 0 ? F.GREEN : F.MUTED }}>
            {running.length}
          </div>
          <div className={S.muted + ' mt-1'}>
            of {deployments.length} total
          </div>
        </div>

        {/* Total P&L */}
        <div
          className="p-4"
          style={{
            borderRight: `1px solid ${F.BORDER}`,
            backgroundColor: `${totalPnl >= 0 ? F.GREEN : F.RED}04`,
          }}
        >
          <div className={S.metricLabel}>
            {totalPnl >= 0 ? <TrendingUp size={10} /> : <TrendingDown size={10} />}
            TOTAL P&L
          </div>
          <div className="text-[22px] font-bold" style={{ color: totalPnl >= 0 ? F.GREEN : F.RED }}>
            {totalPnl >= 0 ? '+' : ''}{totalPnl.toFixed(2)}
          </div>
        </div>

        {/* Total Trades */}
        <div className="p-4" style={{ borderRight: `1px solid ${F.BORDER}` }}>
          <div className={S.metricLabel}>
            <Activity size={10} />
            TOTAL TRADES
          </div>
          <div className="text-[22px] font-bold" style={{ color: F.CYAN }}>
            {totalTrades}
          </div>
        </div>

        {/* Avg Win Rate */}
        <div className="p-4" style={{ borderRight: `1px solid ${F.BORDER}` }}>
          <div className={S.metricLabel}>
            <Target size={10} />
            AVG WIN RATE
          </div>
          <div className="text-[22px] font-bold" style={{
            color: avgWinRate >= 50 ? F.GREEN : avgWinRate > 0 ? F.YELLOW : F.MUTED,
          }}>
            {avgWinRate > 0 ? `${avgWinRate.toFixed(1)}%` : '—'}
          </div>
        </div>

        {/* Events */}
        <div className="p-4">
          <div className={S.metricLabel}>
            <Radio size={10} />
            EVENTS
          </div>
          <div className="text-[22px] font-bold" style={{ color: F.YELLOW }}>
            {recentEvents.length}
          </div>
          <div className={S.muted + ' mt-1'}>
            recent signals
          </div>
        </div>
      </div>

      {/* ─── MAIN AREA: Two-panel split ─── */}
      <div className="flex-1 flex overflow-hidden min-h-0">
        {/* LEFT: Deployment Cards */}
        <div
          className="flex-1 overflow-y-auto min-w-0"
          style={{ borderRight: `1px solid ${F.BORDER}` }}
        >
          {/* Toolbar */}
          <div
            className="flex items-center justify-between px-4 py-2.5 sticky top-0 z-[1]"
            style={{ backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}
          >
            <div className="flex items-center gap-2">
              <BarChart3 size={12} style={{ color: F.ORANGE }} />
              <span className="text-[10px] font-bold tracking-wide" style={{ color: F.WHITE }}>
                DEPLOYMENTS
              </span>
            </div>
            <div className="flex gap-1.5">
              <button
                onClick={loadDeployments}
                className={S.btnOutline}
                onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
              >
                <RefreshCw size={10} />
                REFRESH
              </button>
              {running.length > 0 && (
                <button
                  onClick={handleStopAll}
                  className={S.btnDanger}
                  style={{ backgroundColor: `${F.RED}15`, borderColor: `${F.RED}30`, color: F.RED }}
                  onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.RED}25`; }}
                  onMouseLeave={e => { e.currentTarget.style.backgroundColor = `${F.RED}15`; }}
                >
                  <StopCircle size={10} />
                  STOP ALL
                </button>
              )}
            </div>
          </div>

          {/* Deployment List */}
          <div className="p-4">
            {loading ? (
              <div className="flex flex-col items-center gap-2 py-12 text-center">
                <RefreshCw size={18} className="animate-spin" style={{ color: F.ORANGE }} />
                <span className="text-[11px] font-bold tracking-wide" style={{ color: F.MUTED }}>LOADING DEPLOYMENTS...</span>
              </div>
            ) : deployments.length === 0 ? (
              <div className="flex flex-col items-center justify-center py-12 text-center" style={{ color: F.MUTED }}>
                <Server size={28} className="opacity-30 mb-2" />
                <span className="text-[11px] font-bold">NO DEPLOYMENTS</span>
                <span className="text-[10px] mt-1">Create a strategy and deploy it to see it here</span>
              </div>
            ) : (
              <div className="flex flex-col gap-3">
                {running.length > 0 && (
                  <>
                    <div className="flex items-center gap-1.5 text-[9px] font-bold tracking-wide" style={{ color: F.GREEN }}>
                      <div className="w-1.5 h-1.5 rounded-full" style={{ backgroundColor: F.GREEN }} />
                      RUNNING ({running.length})
                    </div>
                    {running.map(d => (
                      <DeploymentCard key={d.id} deployment={d} onStopped={loadDeployments} />
                    ))}
                  </>
                )}
                {stopped.length > 0 && (
                  <>
                    <div
                      className="text-[9px] font-bold tracking-wide"
                      style={{
                        color: F.MUTED,
                        paddingTop: running.length > 0 ? '8px' : '0',
                        borderTop: running.length > 0 ? `1px solid ${F.BORDER}` : 'none',
                        marginTop: running.length > 0 ? '4px' : '0',
                      }}
                    >
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
        <div
          className="flex flex-col shrink-0"
          style={{ width: '340px', backgroundColor: F.PANEL_BG }}
        >
          {/* Event Log Header */}
          <div
            className="flex items-center justify-between px-4 py-2.5"
            style={{ backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}
          >
            <div className="flex items-center gap-2">
              <Radio size={12} style={{ color: F.YELLOW }} />
              <span className="text-[10px] font-bold tracking-wide" style={{ color: F.WHITE }}>
                EVENT LOG
              </span>
              {recentEvents.length > 0 && (
                <span
                  className={S.badge}
                  style={{ backgroundColor: `${F.YELLOW}20`, color: F.YELLOW }}
                >
                  {recentEvents.length}
                </span>
              )}
            </div>
            {recentEvents.length > 0 && (
              <button
                onClick={() => setRecentEvents([])}
                className={S.btnOutline}
                onMouseEnter={e => { e.currentTarget.style.color = F.WHITE; e.currentTarget.style.borderColor = F.ORANGE; }}
                onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; e.currentTarget.style.borderColor = F.BORDER; }}
              >
                CLEAR
              </button>
            )}
          </div>

          {/* Event Entries */}
          <div className="flex-1 overflow-y-auto py-1">
            {recentEvents.length === 0 ? (
              <div className="flex flex-col items-center gap-2 py-8 px-4 text-center">
                <Radio size={20} style={{ color: F.MUTED, opacity: 0.3 }} />
                <span className="text-[10px] font-bold" style={{ color: F.MUTED }}>
                  NO EVENTS YET
                </span>
                <span className={S.muted}>
                  Events will appear here when strategies are running
                </span>
              </div>
            ) : (
              recentEvents.map((evt, i) => (
                <div
                  key={i}
                  className="flex gap-2 px-4 py-2 transition-colors duration-150"
                  style={{ borderBottom: `1px solid ${F.BORDER}15` }}
                  onMouseEnter={e => { e.currentTarget.style.backgroundColor = F.HOVER; }}
                  onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                >
                  <div className="shrink-0 pt-0.5" style={{ color: eventColor(evt.type) }}>
                    {eventIcon(evt.type)}
                  </div>
                  <div className="flex-1 min-w-0">
                    <div
                      className="text-[10px] leading-relaxed break-words"
                      style={{ color: eventColor(evt.type) }}
                    >
                      {evt.message}
                    </div>
                    <div className="flex items-center gap-1 mt-0.5 text-[9px]" style={{ color: F.MUTED }}>
                      <Clock size={8} />
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
