import React, { useState, useEffect, useRef } from 'react';
import {
  Square, ChevronDown, ChevronUp, TrendingUp, TrendingDown,
  Activity, Target, Clock, BarChart3, Wifi, Zap, Trash2,
} from 'lucide-react';
import { listen } from '@tauri-apps/api/event';
import type { AlgoDeployment } from '../types';
import { stopAlgoDeployment, deleteAlgoDeployment } from '../services/algoTradingService';
import TradeHistoryTable from './TradeHistoryTable';
import { F } from '../constants/theme';
import { S } from '../constants/styles';

const statusConfig: Record<string, { color: string; bg: string; label: string }> = {
  running: { color: F.GREEN, bg: `${F.GREEN}15`, label: 'RUNNING' },
  stopped: { color: F.GRAY, bg: `${F.GRAY}15`, label: 'STOPPED' },
  error: { color: F.RED, bg: `${F.RED}15`, label: 'ERROR' },
  starting: { color: F.YELLOW, bg: `${F.YELLOW}15`, label: 'STARTING' },
  pending: { color: F.BLUE, bg: `${F.BLUE}15`, label: 'PENDING' },
};

const formatVolume = (v: number | null): string => {
  if (v === null || v === 0) return '0';
  if (v >= 1_000_000) return `${(v / 1_000_000).toFixed(1)}M`;
  if (v >= 1_000) return `${(v / 1_000).toFixed(1)}K`;
  return v.toFixed(0);
};

interface DeploymentCardProps {
  deployment: AlgoDeployment;
  onStopped: () => void;
}

interface LivePrice {
  price: number;
  change: number | null;
  changePct: number | null;
  high: number | null;
  low: number | null;
  volume: number | null;
  ts: number;
}

const DeploymentCard: React.FC<DeploymentCardProps> = ({ deployment, onStopped }) => {
  const [showTrades, setShowTrades] = useState(false);
  const [stopping, setStopping] = useState(false);
  const [deleting, setDeleting] = useState(false);
  const [livePrice, setLivePrice] = useState<LivePrice | null>(null);
  const prevPriceRef = useRef<number | null>(null);
  const [priceFlash, setPriceFlash] = useState<'up' | 'down' | null>(null);

  // Listen to unified algo_live_ticker event for live price from ANY connected broker
  useEffect(() => {
    if (deployment.status !== 'running') return;

    // Normalize the deployment symbol for matching (strip exchange prefixes like "NSE:")
    const deploySymNorm = deployment.symbol.replace(/^[A-Z]+:/i, '').toUpperCase();

    let unlisten: (() => void) | null = null;

    listen<{
      provider: string; symbol: string; price: number;
      change?: number; change_percent?: number;
      high?: number; low?: number; volume?: number;
      timestamp?: number;
    }>('algo_live_ticker', (event) => {
      const p = event.payload;
      // Normalize incoming symbol for comparison
      const tickSym = (p.symbol || '').replace(/^[A-Z]+:/i, '').toUpperCase();
      if (tickSym !== deploySymNorm && !tickSym.includes(deploySymNorm) && !deploySymNorm.includes(tickSym)) return;

      // Flash effect on price change
      if (prevPriceRef.current !== null && p.price !== prevPriceRef.current) {
        setPriceFlash(p.price > prevPriceRef.current ? 'up' : 'down');
        setTimeout(() => setPriceFlash(null), 400);
      }
      prevPriceRef.current = p.price;

      setLivePrice({
        price: p.price,
        change: p.change ?? null,
        changePct: p.change_percent ?? null,
        high: p.high ?? null,
        low: p.low ?? null,
        volume: p.volume ?? null,
        ts: Date.now(),
      });
    }).then(fn => { unlisten = fn; });

    return () => { unlisten?.(); };
  }, [deployment.symbol, deployment.status]);

  const handleStop = async () => {
    setStopping(true);
    await stopAlgoDeployment(deployment.id);
    setStopping(false);
    onStopped();
  };

  const handleDelete = async () => {
    setDeleting(true);
    await deleteAlgoDeployment(deployment.id);
    setDeleting(false);
    onStopped();
  };

  const m = deployment.metrics;
  const totalPnl = m.total_pnl + m.unrealized_pnl;
  const isPositive = totalPnl >= 0;
  const sc = statusConfig[deployment.status] || statusConfig.pending;

  // Derive broker display info
  const providerLabel = (deployment.provider || '').toUpperCase() || 'N/A';
  const isCryptoBroker = ['kraken', 'hyperliquid', 'binance'].includes((deployment.provider || '').toLowerCase());
  const providerColor = isCryptoBroker ? F.CYAN : F.PURPLE;

  return (
    <div
      className="rounded overflow-hidden transition-colors duration-200"
      style={{ backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}` }}
    >
      {/* ─── TOP ROW: Symbol, Status, Name ─── */}
      <div
        className="flex items-center gap-3 px-4 py-3"
        style={{
          backgroundColor: F.HEADER_BG,
          borderBottom: `1px solid ${F.BORDER}`,
          borderLeft: `3px solid ${sc.color}`,
        }}
      >
        {/* Status Badge */}
        <span
          className={S.badge}
          style={{ color: sc.color, backgroundColor: sc.bg }}
        >
          {sc.label}
        </span>

        {/* Symbol */}
        <span className="text-[13px] font-bold tracking-wide" style={{ color: F.WHITE }}>
          {deployment.symbol}
        </span>

        {/* Strategy name */}
        <span className="text-[10px]" style={{ color: F.MUTED }}>
          {deployment.strategy_name || deployment.strategy_id}
        </span>

        {/* Tags */}
        <div className="flex gap-1.5 ml-auto">
          {/* Provider / Broker Badge */}
          <span
            className={S.badge}
            style={{ backgroundColor: `${providerColor}15`, color: providerColor }}
          >
            <Wifi size={8} />
            {providerLabel}
          </span>
          <span
            className={S.badge}
            style={{
              backgroundColor: deployment.mode === 'live' ? `${F.RED}15` : `${F.GREEN}15`,
              color: deployment.mode === 'live' ? F.RED : F.GREEN,
            }}
          >
            {deployment.mode.toUpperCase()}
          </span>
          <span
            className={S.badge}
            style={{ backgroundColor: `${F.CYAN}15`, color: F.CYAN }}
          >
            {deployment.timeframe}
          </span>
          <span
            className={S.badge}
            style={{ backgroundColor: `${F.BLUE}15`, color: F.BLUE }}
          >
            QTY: {deployment.quantity}
          </span>
        </div>
      </div>

      {/* ─── LIVE PRICE BAR ─── */}
      {deployment.status === 'running' && (
        <div
          className="flex items-center gap-3 px-4 py-2.5 transition-colors duration-300"
          style={{
            borderBottom: `1px solid ${F.BORDER}`,
            backgroundColor: livePrice
              ? (priceFlash === 'up' ? `${F.GREEN}12` : priceFlash === 'down' ? `${F.RED}12` : F.DARK_BG)
              : F.DARK_BG,
          }}
        >
          <div className="flex items-center gap-1.5 text-[9px] font-bold tracking-wide" style={{ color: F.MUTED }}>
            <Zap size={10} style={{ color: livePrice ? F.GREEN : F.MUTED }} />
            LIVE PRICE
          </div>
          {livePrice ? (
            <>
              <span
                className="text-[20px] font-bold transition-colors duration-300"
                style={{
                  color: priceFlash === 'up' ? F.GREEN : priceFlash === 'down' ? F.RED : F.WHITE,
                  fontVariantNumeric: 'tabular-nums',
                }}
              >
                {livePrice.price.toFixed(2)}
              </span>
              {livePrice.changePct !== null && (
                <span className="flex items-center gap-1 text-[11px] font-bold" style={{
                  color: (livePrice.changePct ?? 0) >= 0 ? F.GREEN : F.RED,
                }}>
                  {(livePrice.changePct ?? 0) >= 0 ? <TrendingUp size={11} /> : <TrendingDown size={11} />}
                  {(livePrice.changePct ?? 0) >= 0 ? '+' : ''}{(livePrice.changePct ?? 0).toFixed(2)}%
                </span>
              )}
              {livePrice.change !== null && (
                <span className="text-[10px] font-bold" style={{
                  color: (livePrice.change ?? 0) >= 0 ? F.GREEN : F.RED,
                }}>
                  ({(livePrice.change ?? 0) >= 0 ? '+' : ''}{(livePrice.change ?? 0).toFixed(2)})
                </span>
              )}
              <div className="flex gap-3 ml-auto text-[9px]" style={{ color: F.MUTED }}>
                {livePrice.high !== null && (
                  <span>H: <span style={{ color: F.GREEN }}>{(livePrice.high ?? 0).toFixed(2)}</span></span>
                )}
                {livePrice.low !== null && (
                  <span>L: <span style={{ color: F.RED }}>{(livePrice.low ?? 0).toFixed(2)}</span></span>
                )}
                {livePrice.volume !== null && livePrice.volume > 0 && (
                  <span>V: <span style={{ color: F.CYAN }}>{formatVolume(livePrice.volume)}</span></span>
                )}
                <span className="flex items-center gap-1">
                  <Clock size={8} />
                  {new Date(livePrice.ts).toLocaleTimeString()}
                </span>
              </div>
            </>
          ) : (
            <span className="text-[11px] italic" style={{ color: F.MUTED }}>
              Waiting for ticks...
            </span>
          )}
        </div>
      )}

      {/* ─── METRICS ROW ─── */}
      <div
        className="grid grid-cols-5"
        style={{ borderBottom: `1px solid ${F.BORDER}` }}
      >
        {/* P&L */}
        <div
          className="p-4"
          style={{
            borderRight: `1px solid ${F.BORDER}`,
            backgroundColor: `${isPositive ? F.GREEN : F.RED}06`,
          }}
        >
          <div className={S.metricLabel}>
            {isPositive ? <TrendingUp size={10} /> : <TrendingDown size={10} />}
            TOTAL P&L
          </div>
          <div className={S.metricValue} style={{ color: isPositive ? F.GREEN : F.RED }}>
            {totalPnl >= 0 ? '+' : ''}{totalPnl.toFixed(2)}
          </div>
          {m.unrealized_pnl !== 0 && (
            <div className="text-[9px] mt-1" style={{ color: F.MUTED }}>
              Unrealized: <span style={{ color: m.unrealized_pnl >= 0 ? F.GREEN : F.RED }}>
                {m.unrealized_pnl >= 0 ? '+' : ''}{m.unrealized_pnl.toFixed(2)}
              </span>
            </div>
          )}
        </div>

        {/* Trades */}
        <div className="p-4" style={{ borderRight: `1px solid ${F.BORDER}` }}>
          <div className={S.metricLabel}>
            <Activity size={10} />
            TRADES
          </div>
          <div className={S.metricValue} style={{ color: F.CYAN }}>
            {m.total_trades}
          </div>
        </div>

        {/* Win Rate */}
        <div className="p-4" style={{ borderRight: `1px solid ${F.BORDER}` }}>
          <div className={S.metricLabel}>
            <Target size={10} />
            WIN RATE
          </div>
          <div className={S.metricValue} style={{ color: m.win_rate >= 50 ? F.GREEN : F.YELLOW }}>
            {m.win_rate.toFixed(1)}%
          </div>
        </div>

        {/* Max Drawdown */}
        <div className="p-4" style={{ borderRight: `1px solid ${F.BORDER}` }}>
          <div className={S.metricLabel}>
            <TrendingDown size={10} />
            MAX DD
          </div>
          <div className={S.metricValue} style={{ color: F.RED }}>
            {m.max_drawdown.toFixed(2)}
          </div>
        </div>

        {/* Position */}
        <div className="p-4">
          <div className={S.metricLabel}>
            <BarChart3 size={10} />
            POSITION
          </div>
          {m.current_position_qty > 0 ? (
            <div>
              <div className="text-[11px] font-bold" style={{ color: F.BLUE }}>
                {m.current_position_side} {m.current_position_qty}
              </div>
              <div className="text-[9px] mt-0.5" style={{ color: F.MUTED }}>
                @ {m.current_position_entry.toFixed(2)}
              </div>
            </div>
          ) : (
            <div className="text-[11px]" style={{ color: F.MUTED }}>FLAT</div>
          )}
        </div>
      </div>

      {/* ─── ACTIONS ROW ─── */}
      <div className="flex items-center justify-between px-4 py-2.5">
        <div className="flex items-center gap-2">
          <span className="text-[9px]" style={{ color: F.MUTED }}>
            ID: <span style={{ color: F.GRAY }}>{deployment.id.substring(0, 12)}</span>
          </span>
          {m.metrics_updated && (
            <span className="flex items-center gap-1 text-[9px]" style={{ color: F.MUTED }}>
              <Clock size={9} />
              {new Date(m.metrics_updated).toLocaleTimeString()}
            </span>
          )}
        </div>
        <div className="flex gap-1.5">
          {deployment.status === 'running' && (
            <button
              onClick={handleStop}
              disabled={stopping}
              className={S.btnDanger}
              style={{
                backgroundColor: `${F.RED}15`,
                borderColor: `${F.RED}30`,
                color: F.RED,
                opacity: stopping ? 0.5 : 1,
                cursor: stopping ? 'not-allowed' : 'pointer',
              }}
              onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.RED}25`; }}
              onMouseLeave={e => { e.currentTarget.style.backgroundColor = `${F.RED}15`; }}
            >
              <Square size={10} />
              {stopping ? 'STOPPING...' : 'STOP'}
            </button>
          )}
          {deployment.status !== 'running' && (
            <button
              onClick={handleDelete}
              disabled={deleting}
              className={S.btnOutline}
              style={{
                opacity: deleting ? 0.5 : 1,
                cursor: deleting ? 'not-allowed' : 'pointer',
              }}
              onMouseEnter={e => { e.currentTarget.style.borderColor = F.RED; e.currentTarget.style.color = F.RED; }}
              onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
            >
              <Trash2 size={10} />
              {deleting ? 'DELETING...' : 'DELETE'}
            </button>
          )}
          <button
            onClick={() => setShowTrades(!showTrades)}
            className={S.btnOutline}
            style={{
              color: showTrades ? F.ORANGE : F.GRAY,
              borderColor: showTrades ? F.ORANGE : F.BORDER,
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
            onMouseLeave={e => {
              e.currentTarget.style.borderColor = showTrades ? F.ORANGE : F.BORDER;
              e.currentTarget.style.color = showTrades ? F.ORANGE : F.GRAY;
            }}
          >
            {showTrades ? <ChevronUp size={10} /> : <ChevronDown size={10} />}
            TRADES
          </button>
        </div>
      </div>

      {/* ─── TRADE HISTORY (Expanded) ─── */}
      {showTrades && (
        <div style={{ borderTop: `1px solid ${F.BORDER}` }}>
          <TradeHistoryTable deploymentId={deployment.id} />
        </div>
      )}
    </div>
  );
};

export default DeploymentCard;
