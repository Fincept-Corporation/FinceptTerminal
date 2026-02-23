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
    <div style={{
      backgroundColor: F.PANEL_BG,
      border: `1px solid ${F.BORDER}`,
      borderRadius: '2px',
      overflow: 'hidden',
      transition: 'border-color 0.2s',
    }}>
      {/* ─── TOP ROW: Symbol, Status, Name ─── */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '10px',
        padding: '10px 14px',
        backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        borderLeft: `3px solid ${sc.color}`,
      }}>
        {/* Status Badge */}
        <span style={{
          padding: '3px 8px', borderRadius: '2px',
          fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
          color: sc.color, backgroundColor: sc.bg,
        }}>
          {sc.label}
        </span>

        {/* Symbol */}
        <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
          {deployment.symbol}
        </span>

        {/* Strategy name */}
        <span style={{ fontSize: '9px', color: F.MUTED }}>
          {deployment.strategy_name || deployment.strategy_id}
        </span>

        {/* Tags */}
        <div style={{ display: 'flex', gap: '4px', marginLeft: 'auto' }}>
          {/* Provider / Broker Badge */}
          <span style={{
            padding: '2px 6px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
            backgroundColor: `${providerColor}15`, color: providerColor,
            display: 'flex', alignItems: 'center', gap: '3px',
          }}>
            <Wifi size={7} />
            {providerLabel}
          </span>
          <span style={{
            padding: '2px 6px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
            backgroundColor: deployment.mode === 'live' ? `${F.RED}15` : `${F.GREEN}15`,
            color: deployment.mode === 'live' ? F.RED : F.GREEN,
          }}>
            {deployment.mode.toUpperCase()}
          </span>
          <span style={{
            padding: '2px 6px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
            backgroundColor: `${F.CYAN}15`, color: F.CYAN,
          }}>
            {deployment.timeframe}
          </span>
          <span style={{
            padding: '2px 6px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
            backgroundColor: `${F.BLUE}15`, color: F.BLUE,
          }}>
            QTY: {deployment.quantity}
          </span>
        </div>
      </div>

      {/* ─── LIVE PRICE BAR ─── */}
      {deployment.status === 'running' && (
        <div style={{
          display: 'flex', alignItems: 'center', gap: '12px',
          padding: '8px 14px',
          borderBottom: `1px solid ${F.BORDER}`,
          backgroundColor: livePrice
            ? (priceFlash === 'up' ? `${F.GREEN}12` : priceFlash === 'down' ? `${F.RED}12` : `${F.DARK_BG}`)
            : F.DARK_BG,
          transition: 'background-color 0.3s',
        }}>
          <div style={{
            display: 'flex', alignItems: 'center', gap: '5px',
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px',
          }}>
            <Zap size={8} color={livePrice ? F.GREEN : F.MUTED} />
            LIVE PRICE
          </div>
          {livePrice ? (
            <>
              <span style={{
                fontSize: '16px', fontWeight: 700,
                color: priceFlash === 'up' ? F.GREEN : priceFlash === 'down' ? F.RED : F.WHITE,
                transition: 'color 0.3s',
                fontVariantNumeric: 'tabular-nums',
              }}>
                {livePrice.price.toFixed(2)}
              </span>
              {livePrice.changePct !== null && (
                <span style={{
                  fontSize: '10px', fontWeight: 700,
                  color: (livePrice.changePct ?? 0) >= 0 ? F.GREEN : F.RED,
                  display: 'flex', alignItems: 'center', gap: '2px',
                }}>
                  {(livePrice.changePct ?? 0) >= 0 ? <TrendingUp size={9} /> : <TrendingDown size={9} />}
                  {(livePrice.changePct ?? 0) >= 0 ? '+' : ''}{(livePrice.changePct ?? 0).toFixed(2)}%
                </span>
              )}
              {livePrice.change !== null && (
                <span style={{
                  fontSize: '9px', fontWeight: 700,
                  color: (livePrice.change ?? 0) >= 0 ? F.GREEN : F.RED,
                }}>
                  ({(livePrice.change ?? 0) >= 0 ? '+' : ''}{(livePrice.change ?? 0).toFixed(2)})
                </span>
              )}
              <div style={{ display: 'flex', gap: '10px', marginLeft: 'auto', fontSize: '8px', color: F.MUTED }}>
                {livePrice.high !== null && (
                  <span>H: <span style={{ color: F.GREEN }}>{(livePrice.high ?? 0).toFixed(2)}</span></span>
                )}
                {livePrice.low !== null && (
                  <span>L: <span style={{ color: F.RED }}>{(livePrice.low ?? 0).toFixed(2)}</span></span>
                )}
                {livePrice.volume !== null && livePrice.volume > 0 && (
                  <span>V: <span style={{ color: F.CYAN }}>{formatVolume(livePrice.volume)}</span></span>
                )}
                <span style={{ display: 'flex', alignItems: 'center', gap: '2px' }}>
                  <Clock size={7} />
                  {new Date(livePrice.ts).toLocaleTimeString()}
                </span>
              </div>
            </>
          ) : (
            <span style={{ fontSize: '10px', color: F.MUTED, fontStyle: 'italic' }}>
              Waiting for ticks...
            </span>
          )}
        </div>
      )}

      {/* ─── METRICS ROW ─── */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(5, 1fr)',
        borderBottom: `1px solid ${F.BORDER}`,
      }}>
        {/* P&L */}
        <div style={{
          padding: '10px 14px',
          borderRight: `1px solid ${F.BORDER}`,
          backgroundColor: `${isPositive ? F.GREEN : F.RED}06`,
        }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED,
            letterSpacing: '0.5px', marginBottom: '4px',
            display: 'flex', alignItems: 'center', gap: '3px',
          }}>
            {isPositive ? <TrendingUp size={8} /> : <TrendingDown size={8} />}
            TOTAL P&L
          </div>
          <div style={{
            fontSize: '14px', fontWeight: 700,
            color: isPositive ? F.GREEN : F.RED,
          }}>
            {totalPnl >= 0 ? '+' : ''}{totalPnl.toFixed(2)}
          </div>
          {m.unrealized_pnl !== 0 && (
            <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>
              Unrealized: <span style={{ color: m.unrealized_pnl >= 0 ? F.GREEN : F.RED }}>
                {m.unrealized_pnl >= 0 ? '+' : ''}{m.unrealized_pnl.toFixed(2)}
              </span>
            </div>
          )}
        </div>

        {/* Trades */}
        <div style={{ padding: '10px 14px', borderRight: `1px solid ${F.BORDER}` }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '4px',
            display: 'flex', alignItems: 'center', gap: '3px',
          }}>
            <Activity size={8} />
            TRADES
          </div>
          <div style={{ fontSize: '14px', fontWeight: 700, color: F.CYAN }}>
            {m.total_trades}
          </div>
        </div>

        {/* Win Rate */}
        <div style={{ padding: '10px 14px', borderRight: `1px solid ${F.BORDER}` }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '4px',
            display: 'flex', alignItems: 'center', gap: '3px',
          }}>
            <Target size={8} />
            WIN RATE
          </div>
          <div style={{
            fontSize: '14px', fontWeight: 700,
            color: m.win_rate >= 50 ? F.GREEN : F.YELLOW,
          }}>
            {m.win_rate.toFixed(1)}%
          </div>
        </div>

        {/* Max Drawdown */}
        <div style={{ padding: '10px 14px', borderRight: `1px solid ${F.BORDER}` }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '4px',
            display: 'flex', alignItems: 'center', gap: '3px',
          }}>
            <TrendingDown size={8} />
            MAX DD
          </div>
          <div style={{ fontSize: '14px', fontWeight: 700, color: F.RED }}>
            {m.max_drawdown.toFixed(2)}
          </div>
        </div>

        {/* Position */}
        <div style={{ padding: '10px 14px' }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '4px',
            display: 'flex', alignItems: 'center', gap: '3px',
          }}>
            <BarChart3 size={8} />
            POSITION
          </div>
          {m.current_position_qty > 0 ? (
            <div style={{ fontSize: '10px', fontWeight: 700, color: F.BLUE }}>
              {m.current_position_side} {m.current_position_qty}
              <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '1px' }}>
                @ {m.current_position_entry.toFixed(2)}
              </div>
            </div>
          ) : (
            <div style={{ fontSize: '10px', color: F.MUTED }}>FLAT</div>
          )}
        </div>
      </div>

      {/* ─── ACTIONS ROW ─── */}
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '8px 14px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ fontSize: '8px', color: F.MUTED }}>
            ID: <span style={{ color: F.GRAY }}>{deployment.id.substring(0, 12)}</span>
          </span>
          {m.metrics_updated && (
            <span style={{ fontSize: '8px', color: F.MUTED, display: 'flex', alignItems: 'center', gap: '3px' }}>
              <Clock size={8} />
              {new Date(m.metrics_updated).toLocaleTimeString()}
            </span>
          )}
        </div>
        <div style={{ display: 'flex', gap: '4px' }}>
          {deployment.status === 'running' && (
            <button
              onClick={handleStop}
              disabled={stopping}
              style={{
                padding: '5px 10px', backgroundColor: `${F.RED}15`, color: F.RED,
                border: `1px solid ${F.RED}30`, borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                cursor: stopping ? 'not-allowed' : 'pointer',
                display: 'flex', alignItems: 'center', gap: '4px',
                opacity: stopping ? 0.5 : 1, transition: 'all 0.2s', letterSpacing: '0.5px',
              }}
              onMouseEnter={e => { e.currentTarget.style.backgroundColor = `${F.RED}25`; }}
              onMouseLeave={e => { e.currentTarget.style.backgroundColor = `${F.RED}15`; }}
            >
              <Square size={9} />
              {stopping ? 'STOPPING...' : 'STOP'}
            </button>
          )}
          {deployment.status !== 'running' && (
            <button
              onClick={handleDelete}
              disabled={deleting}
              style={{
                padding: '5px 10px', backgroundColor: 'transparent', color: F.MUTED,
                border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                cursor: deleting ? 'not-allowed' : 'pointer',
                display: 'flex', alignItems: 'center', gap: '4px',
                opacity: deleting ? 0.5 : 1, transition: 'all 0.2s', letterSpacing: '0.5px',
              }}
              onMouseEnter={e => { e.currentTarget.style.borderColor = F.RED; e.currentTarget.style.color = F.RED; }}
              onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.MUTED; }}
            >
              <Trash2 size={9} />
              {deleting ? 'DELETING...' : 'DELETE'}
            </button>
          )}
          <button
            onClick={() => setShowTrades(!showTrades)}
            style={{
              padding: '5px 10px', backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`, color: showTrades ? F.ORANGE : F.GRAY,
              borderRadius: '2px', cursor: 'pointer', fontSize: '8px', fontWeight: 700,
              display: 'flex', alignItems: 'center', gap: '4px',
              transition: 'all 0.2s', letterSpacing: '0.5px',
              borderColor: showTrades ? F.ORANGE : F.BORDER,
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
            onMouseLeave={e => {
              e.currentTarget.style.borderColor = showTrades ? F.ORANGE : F.BORDER;
              e.currentTarget.style.color = showTrades ? F.ORANGE : F.GRAY;
            }}
          >
            {showTrades ? <ChevronUp size={9} /> : <ChevronDown size={9} />}
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
