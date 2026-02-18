import React, { useState, useCallback, useEffect, useRef } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  Play, Square, SkipForward, Zap, Activity, Users, TrendingUp,
  TrendingDown, BarChart3, Newspaper, AlertTriangle, Clock,
  Settings, ChevronDown, ChevronRight, RefreshCw, Gauge,
  Shield, Target, Layers, Radio, Eye
} from 'lucide-react';

// ============================================================================
// FINCEPT Design System
// ============================================================================
const F = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

const FONT = '"IBM Plex Mono", "Consolas", monospace';

// CSS keyframes for animations (injected once)
const KEYFRAMES_ID = 'market-sim-keyframes';
if (typeof document !== 'undefined' && !document.getElementById(KEYFRAMES_ID)) {
  const style = document.createElement('style');
  style.id = KEYFRAMES_ID;
  style.textContent = `
    @keyframes spin {
      from { transform: rotate(0deg); }
      to { transform: rotate(360deg); }
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
  `;
  document.head.appendChild(style);
}

// ============================================================================
// Types (mirroring Rust types exactly)
// Price = i64 (cents), Qty = i64, Nanos = u64
// ============================================================================
interface SimResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
}

interface L1Quote {
  instrument_id: number;      // u32
  bid_price: number;          // i64 (cents)
  bid_size: number;           // i64
  ask_price: number;          // i64 (cents)
  ask_size: number;           // i64
  last_price: number;         // i64 (cents)
  last_size: number;          // i64
  volume: number;             // i64
  vwap: number;               // f64
  open: number;               // i64 (cents)
  high: number;               // i64 (cents)
  low: number;                // i64 (cents)
  timestamp: number;          // u64 (nanos)
}

interface PriceLevel {
  price: number;              // i64 (cents)
  quantity: number;           // i64
  order_count: number;        // u32
}

interface L2Snapshot {
  instrument_id: number;
  bids: PriceLevel[];
  asks: PriceLevel[];
  timestamp: number;
}

// Rust Side enum serializes as "Buy" or "Sell" string
type Side = 'Buy' | 'Sell' | string | Record<string, unknown>;

interface Trade {
  id: number;                 // u64
  instrument_id: number;      // u32
  price: number;              // i64 (cents)
  quantity: number;           // i64
  aggressor_side: Side;       // enum Side
  buyer_id: number;           // u32
  seller_id: number;          // u32
  buyer_order_id?: number;    // u64
  seller_order_id?: number;   // u64
  timestamp: number;          // u64 (nanos)
  venue_id?: number;          // u32
  is_auction_trade?: boolean;
}

// Rust ParticipantType and LatencyTier enums serialize as strings
type ParticipantType = 'MarketMaker' | 'HFT' | 'StatArb' | 'Momentum' | 'MeanReversion' |
  'NoiseTrader' | 'InformedTrader' | 'Institutional' | 'RetailTrader' | 'ToxicFlow' |
  'Spoofer' | 'Arbitrageur' | 'SniperBot' | string | Record<string, unknown>;

type LatencyTier = 'CoLocated' | 'ProximityHosted' | 'DirectConnect' | 'Retail' |
  string | Record<string, unknown>;

type MarketPhase = 'PreOpen' | 'OpeningAuction' | 'ContinuousTrading' | 'VolatilityAuction' |
  'ClosingAuction' | 'PostClose' | 'Halted' | string | Record<string, unknown>;

interface ParticipantSummary {
  id: number;                 // u32
  name: string;
  participant_type: ParticipantType;
  pnl: number;                // f64
  position: number;           // i64
  trade_count: number;        // u64
  order_count: number;        // u64
  cancel_count: number;       // u64
  is_active: boolean;
  latency_tier: LatencyTier;
}

// Helper to extract enum variant name from Rust serde serialization
const getEnumVariant = (value: unknown): string => {
  if (typeof value === 'string') return value;
  if (typeof value === 'object' && value !== null) {
    const keys = Object.keys(value);
    return keys.length > 0 ? keys[0] : 'Unknown';
  }
  return String(value);
};

interface SimulationSnapshot {
  current_time: number;           // u64 (nanos)
  market_phase: MarketPhase;      // enum MarketPhase
  quotes: L1Quote[];
  recent_trades: Trade[];
  participant_summaries: ParticipantSummary[];
  total_volume: number;           // i64
  total_trades: number;           // u64
  circuit_breaker_active: boolean;
  messages_per_second: number;    // u64
  orders_per_second: number;      // u64
}

interface InstrumentMetrics {
  instrument_id: number;
  avg_spread: number;
  total_volume: number;
  total_trades: number;
  vwap: number;
  realized_volatility: number;
  circuit_breaker_count: number;
}

interface ParticipantMetrics {
  participant_id: number;         // u32
  participant_type: ParticipantType;
  name: string;
  realized_pnl: number;           // f64
  unrealized_pnl: number;         // f64
  total_pnl: number;              // f64
  max_drawdown: number;           // f64
  total_trades: number;           // u64
  total_orders: number;           // u64
  total_cancels: number;          // u64
  order_to_trade_ratio: number;   // f64
  fill_rate: number;              // f64
  net_fees: number;               // f64
}

interface AnalyticsSummary {
  total_trades: number;
  total_volume: number;
  total_orders: number;
  total_cancels: number;
  global_otr: number;
  instruments: InstrumentMetrics[];
  participants: ParticipantMetrics[];
}

// ============================================================================
// Helpers
// ============================================================================
const formatPrice = (p: number) => (p / 100).toFixed(2);
const formatQty = (q: number) => q.toLocaleString();
const formatPnl = (pnl: number) => {
  const sign = pnl >= 0 ? '+' : '';
  return `${sign}$${(pnl / 100).toFixed(2)}`;
};
const formatPct = (p: number) => `${(p * 100).toFixed(2)}%`;
const formatTime = (nanos: number) => {
  const totalSeconds = Math.floor(nanos / 1_000_000_000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
};

const PARTICIPANT_COLORS: Record<string, string> = {
  MarketMaker: F.BLUE,
  HFT: F.PURPLE,
  StatArb: F.CYAN,
  Momentum: F.YELLOW,
  MeanReversion: F.GREEN,
  NoiseTrader: F.GRAY,
  InformedTrader: F.ORANGE,
  Institutional: '#00BFFF',
  RetailTrader: F.MUTED,
  ToxicFlow: F.RED,
  Spoofer: '#FF6B6B',
  Arbitrageur: '#7B68EE',
  SniperBot: '#FF4500',
};

const PHASE_LABELS: Record<string, { label: string; color: string }> = {
  PreOpen: { label: 'PRE-OPEN', color: F.MUTED },
  OpeningAuction: { label: 'OPENING AUCTION', color: F.YELLOW },
  ContinuousTrading: { label: 'CONTINUOUS TRADING', color: F.GREEN },
  VolatilityAuction: { label: 'VOLATILITY AUCTION', color: F.RED },
  ClosingAuction: { label: 'CLOSING AUCTION', color: F.YELLOW },
  PostClose: { label: 'POST CLOSE', color: F.MUTED },
  Halted: { label: 'HALTED', color: F.RED },
};

// ============================================================================
// Sub-components
// ============================================================================

const SectionHeader: React.FC<{ title: string; icon?: React.ReactNode; right?: React.ReactNode }> = ({ title, icon, right }) => (
  <div style={{
    padding: '8px 12px',
    backgroundColor: F.HEADER_BG,
    borderBottom: `1px solid ${F.BORDER}`,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'space-between',
  }}>
    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
      {icon}
      <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
        {title}
      </span>
    </div>
    {right}
  </div>
);

const StatBox: React.FC<{ label: string; value: string; color?: string; small?: boolean }> = ({ label, value, color = F.CYAN, small }) => (
  <div style={{ padding: small ? '4px 8px' : '6px 10px' }}>
    <div style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '2px' }}>
      {label}
    </div>
    <div style={{ fontSize: small ? '10px' : '12px', fontWeight: 700, color, fontFamily: FONT }}>
      {value}
    </div>
  </div>
);

const OrderBookRow: React.FC<{
  level: PriceLevel;
  side: 'bid' | 'ask';
  maxQty: number;
}> = ({ level, side, maxQty }) => {
  const pct = maxQty > 0 ? (level.quantity / maxQty) * 100 : 0;
  const color = side === 'bid' ? F.GREEN : F.RED;
  return (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      padding: '2px 8px',
      position: 'relative',
      fontSize: '10px',
      fontFamily: FONT,
    }}>
      <div style={{
        position: 'absolute',
        [side === 'bid' ? 'right' : 'left']: 0,
        top: 0,
        bottom: 0,
        width: `${pct}%`,
        backgroundColor: `${color}10`,
        transition: 'width 0.2s',
      }} />
      <span style={{ flex: 1, color, textAlign: side === 'bid' ? 'left' : 'right', zIndex: 1 }}>
        {formatPrice(level.price)}
      </span>
      <span style={{ width: '70px', textAlign: 'right', color: F.WHITE, zIndex: 1 }}>
        {formatQty(level.quantity)}
      </span>
      <span style={{ width: '30px', textAlign: 'right', color: F.MUTED, fontSize: '8px', zIndex: 1 }}>
        {level.order_count}
      </span>
    </div>
  );
};

// ============================================================================
// Main Component
// ============================================================================
export const MarketSimTab: React.FC = () => {
  // State
  const [isRunning, setIsRunning] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [snapshot, setSnapshot] = useState<SimulationSnapshot | null>(null);
  const [orderbook, setOrderbook] = useState<L2Snapshot | null>(null);
  const [analytics, setAnalytics] = useState<AnalyticsSummary | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [speed, setSpeed] = useState(100); // ticks per step
  const [activePanel, setActivePanel] = useState<'orderbook' | 'trades' | 'analytics'>('orderbook');
  const [leftSection, setLeftSection] = useState<'controls' | 'agents' | 'news'>('controls');
  const [tradeHistory, setTradeHistory] = useState<Trade[]>([]);
  const [priceHistory, setPriceHistory] = useState<{ time: number; price: number }[]>([]);
  const [newsHeadline, setNewsHeadline] = useState('');
  const [newsSentiment, setNewsSentiment] = useState(0.5);
  const [newsMagnitude, setNewsMagnitude] = useState(0.02);

  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const isMountedRef = useRef(true);

  // ---- API Calls ----
  const startSimulation = useCallback(async () => {
    try {
      setError(null);
      setIsLoading(true);
      console.log('[MarketSim] Starting simulation...');
      const resp = await invoke<SimResponse<SimulationSnapshot>>('market_sim_start', {
        request: { name: 'Live Simulation', seed: Date.now() },
      });
      console.log('[MarketSim] Start response:', resp);
      if (resp.success && resp.data) {
        setSnapshot(resp.data);
        setIsRunning(true);
        setIsPaused(false);
        setTradeHistory([]);
        setPriceHistory([]);
        setIsLoading(false);
        // Auto-start continuous running after successful start
        setTimeout(() => {
          if (intervalRef.current === null) {
            runContinuousInternal();
          }
        }, 100);
      } else {
        setIsLoading(false);
        setError(resp.error || 'Failed to start simulation');
      }
    } catch (e: unknown) {
      console.error('[MarketSim] Start error:', e);
      setIsLoading(false);
      setError(e instanceof Error ? e.message : String(e));
    }
  }, []);

  const stepSimulation = useCallback(async () => {
    try {
      const resp = await invoke<SimResponse<SimulationSnapshot>>('market_sim_step', { ticks: speed });
      if (resp.success && resp.data) {
        setSnapshot(resp.data);
        setError(null); // Clear any previous errors on successful step
        // Accumulate trades
        if (resp.data.recent_trades && resp.data.recent_trades.length > 0) {
          setTradeHistory(prev => [...resp.data!.recent_trades.slice(0, 20), ...prev].slice(0, 200));
        }
        // Track price
        const q = resp.data.quotes?.[0];
        if (q && q.last_price > 0) {
          setPriceHistory(prev => [...prev, { time: resp.data!.current_time, price: q.last_price }].slice(-500));
        }
      } else if (resp.error) {
        console.warn('[MarketSim] Step warning:', resp.error);
        // Don't stop for non-critical errors
      }
      // Fetch orderbook
      try {
        const obResp = await invoke<SimResponse<L2Snapshot>>('market_sim_orderbook', {
          instrumentId: 1,
          depth: 15,
        });
        if (obResp.success && obResp.data) {
          setOrderbook(obResp.data);
        }
      } catch (obError) {
        console.warn('[MarketSim] Orderbook fetch failed:', obError);
      }
    } catch (e: unknown) {
      console.error('[MarketSim] Step error:', e);
      setError(e instanceof Error ? e.message : String(e));
      stopSimulation();
    }
  }, [speed]);

  // Internal function to start continuous running
  const runContinuousInternal = useCallback(() => {
    if (intervalRef.current) return;
    setIsPaused(false);
    intervalRef.current = setInterval(() => {
      stepSimulation();
    }, 100); // 10 updates per second
  }, [stepSimulation]);

  const runContinuous = useCallback(() => {
    runContinuousInternal();
  }, [runContinuousInternal]);

  const pauseSimulation = useCallback(() => {
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
    setIsPaused(true);
  }, []);

  const stopSimulation = useCallback(async () => {
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
    try {
      await invoke<SimResponse<string>>('market_sim_stop', {});
    } catch (err) {
      console.warn('[MarketSim] Stop error:', err);
    }
    if (isMountedRef.current) {
      setIsRunning(false);
      setIsPaused(false);
    }
  }, []);

  const fetchAnalytics = useCallback(async () => {
    try {
      const resp = await invoke<SimResponse<AnalyticsSummary>>('market_sim_analytics', {});
      if (resp.success && resp.data) {
        setAnalytics(resp.data);
      }
    } catch (_) {}
  }, []);

  const injectNews = useCallback(async () => {
    if (!newsHeadline) return;
    try {
      await invoke<SimResponse<string>>('market_sim_inject_news', {
        headline: newsHeadline,
        sentiment: newsSentiment * 2 - 1, // convert 0-1 to -1..1
        magnitude: newsMagnitude,
        instrumentIds: [1],
      });
      setNewsHeadline('');
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, [newsHeadline, newsSentiment, newsMagnitude]);

  // Cleanup
  useEffect(() => {
    isMountedRef.current = true;
    return () => {
      isMountedRef.current = false;
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, []);

  // Draw price chart
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || priceHistory.length < 2) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    const prices = priceHistory.map(p => p.price);
    const minP = Math.min(...prices);
    const maxP = Math.max(...prices);
    const range = maxP - minP || 1;

    // Grid
    ctx.strokeStyle = F.BORDER;
    ctx.lineWidth = 0.5;
    for (let i = 0; i < 5; i++) {
      const y = (h / 5) * i;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }

    // Price labels
    ctx.fillStyle = F.MUTED;
    ctx.font = '9px "IBM Plex Mono"';
    ctx.textAlign = 'right';
    for (let i = 0; i <= 4; i++) {
      const price = maxP - (range * i) / 4;
      const y = (h / 4) * i;
      ctx.fillText(formatPrice(price), w - 4, y + 10);
    }

    // Price line
    const firstPrice = prices[0];
    const lastPrice = prices[prices.length - 1];
    ctx.strokeStyle = lastPrice >= firstPrice ? F.GREEN : F.RED;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    for (let i = 0; i < prices.length; i++) {
      const x = (w / (prices.length - 1)) * i;
      const y = h - ((prices[i] - minP) / range) * h;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Fill gradient
    ctx.lineTo(w, h);
    ctx.lineTo(0, h);
    ctx.closePath();
    const grad = ctx.createLinearGradient(0, 0, 0, h);
    const baseColor = lastPrice >= firstPrice ? F.GREEN : F.RED;
    grad.addColorStop(0, `${baseColor}20`);
    grad.addColorStop(1, `${baseColor}05`);
    ctx.fillStyle = grad;
    ctx.fill();
  }, [priceHistory]);

  // ---- Render helpers ----
  const quote = snapshot?.quotes[0];
  const phaseKey = snapshot ? getEnumVariant(snapshot.market_phase) : null;
  const phase = phaseKey ? PHASE_LABELS[phaseKey] || { label: phaseKey, color: F.GRAY } : null;

  const renderTopNav = () => (
    <div style={{
      backgroundColor: F.HEADER_BG,
      borderBottom: `2px solid ${F.ORANGE}`,
      padding: '8px 16px',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      boxShadow: `0 2px 8px ${F.ORANGE}20`,
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <Activity size={14} color={F.ORANGE} />
        <span style={{ fontSize: '12px', fontWeight: 700, color: F.WHITE, fontFamily: FONT }}>
          MARKET SIMULATION ENGINE
        </span>
        {phase && (
          <span style={{
            padding: '2px 8px',
            backgroundColor: `${phase.color}20`,
            color: phase.color,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
            letterSpacing: '0.5px',
          }}>
            {phase.label}
          </span>
        )}
        {snapshot?.circuit_breaker_active && (
          <span style={{
            padding: '2px 8px',
            backgroundColor: `${F.RED}20`,
            color: F.RED,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
            animation: 'pulse 1s infinite',
          }}>
            CIRCUIT BREAKER
          </span>
        )}
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        {!isRunning ? (
          <button
            onClick={startSimulation}
            disabled={isLoading}
            style={{
              padding: '6px 14px',
              backgroundColor: isLoading ? F.MUTED : F.GREEN,
              color: F.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: isLoading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: isLoading ? 0.7 : 1,
            }}
          >
            {isLoading ? (
              <>
                <RefreshCw size={10} style={{ animation: 'spin 1s linear infinite' }} /> STARTING...
              </>
            ) : (
              <>
                <Play size={10} /> START
              </>
            )}
          </button>
        ) : (
          <>
            {isPaused ? (
              <button onClick={runContinuous} style={{
                padding: '6px 14px', backgroundColor: F.GREEN, color: F.DARK_BG,
                border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
                cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px',
              }}>
                <Play size={10} /> RESUME
              </button>
            ) : (
              <button onClick={pauseSimulation} style={{
                padding: '6px 14px', backgroundColor: F.YELLOW, color: F.DARK_BG,
                border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
                cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px',
              }}>
                <Square size={10} /> PAUSE
              </button>
            )}
            <button onClick={stepSimulation} style={{
              padding: '6px 10px', backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`, color: F.GRAY,
              borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              <SkipForward size={10} /> STEP
            </button>
            <button onClick={stopSimulation} style={{
              padding: '6px 10px', backgroundColor: 'transparent',
              border: `1px solid ${F.RED}`, color: F.RED,
              borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              <Square size={10} /> STOP
            </button>
          </>
        )}
        <button onClick={fetchAnalytics} style={{
          padding: '6px 10px', backgroundColor: 'transparent',
          border: `1px solid ${F.BORDER}`, color: F.GRAY,
          borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
          display: 'flex', alignItems: 'center', gap: '4px',
        }}>
          <BarChart3 size={10} /> ANALYTICS
        </button>
      </div>
    </div>
  );

  const renderLeftPanel = () => (
    <div style={{
      width: '280px',
      backgroundColor: F.PANEL_BG,
      borderRight: `1px solid ${F.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Left panel tabs */}
      <div style={{ display: 'flex', borderBottom: `1px solid ${F.BORDER}` }}>
        {(['controls', 'agents', 'news'] as const).map(tab => (
          <button
            key={tab}
            onClick={() => setLeftSection(tab)}
            style={{
              flex: 1, padding: '8px 4px',
              backgroundColor: leftSection === tab ? F.ORANGE : 'transparent',
              color: leftSection === tab ? F.DARK_BG : F.GRAY,
              border: 'none', fontSize: '8px', fontWeight: 700,
              letterSpacing: '0.5px', cursor: 'pointer',
            }}
          >
            {tab.toUpperCase()}
          </button>
        ))}
      </div>

      <div style={{ flex: 1, overflowY: 'auto' }}>
        {leftSection === 'controls' && renderControls()}
        {leftSection === 'agents' && renderAgentList()}
        {leftSection === 'news' && renderNewsInjector()}
      </div>
    </div>
  );

  const renderControls = () => (
    <div>
      <SectionHeader title="SIMULATION SPEED" icon={<Gauge size={10} color={F.GRAY} />} />
      <div style={{ padding: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
          <span style={{ fontSize: '9px', color: F.GRAY, fontWeight: 700 }}>TICKS/STEP</span>
          <input
            type="range"
            min={10}
            max={1000}
            step={10}
            value={speed}
            onChange={e => setSpeed(Number(e.target.value))}
            style={{ flex: 1, accentColor: F.ORANGE }}
          />
          <span style={{ fontSize: '10px', color: F.CYAN, fontFamily: FONT, minWidth: '40px', textAlign: 'right' }}>
            {speed}
          </span>
        </div>
      </div>

      {quote && (
        <>
          <SectionHeader title="INSTRUMENT" icon={<Target size={10} color={F.GRAY} />} />
          <div style={{ padding: '12px' }}>
            <div style={{ fontSize: '14px', fontWeight: 700, color: F.WHITE, fontFamily: FONT, marginBottom: '8px' }}>
              SIM-AAPL
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
              <StatBox label="LAST" value={formatPrice(quote.last_price)} color={F.WHITE} small />
              <StatBox label="VOLUME" value={formatQty(quote.volume)} color={F.CYAN} small />
              <StatBox label="BID" value={`${formatPrice(quote.bid_price)} x ${formatQty(quote.bid_size)}`} color={F.GREEN} small />
              <StatBox label="ASK" value={`${formatPrice(quote.ask_price)} x ${formatQty(quote.ask_size)}`} color={F.RED} small />
              <StatBox label="OPEN" value={formatPrice(quote.open)} small />
              <StatBox label="VWAP" value={quote.vwap > 0 ? formatPrice(quote.vwap) : '--'} color={F.YELLOW} small />
              <StatBox label="HIGH" value={formatPrice(quote.high)} color={F.GREEN} small />
              <StatBox label="LOW" value={quote.low > 0 ? formatPrice(quote.low) : '--'} color={F.RED} small />
            </div>
          </div>
        </>
      )}

      {snapshot && (
        <>
          <SectionHeader title="ENGINE METRICS" icon={<Zap size={10} color={F.GRAY} />} />
          <div style={{ padding: '12px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
            <StatBox label="TOTAL TRADES" value={formatQty(snapshot.total_trades)} small />
            <StatBox label="TOTAL VOLUME" value={formatQty(snapshot.total_volume)} small />
            <StatBox label="MSG/SEC" value={formatQty(snapshot.messages_per_second)} color={F.YELLOW} small />
            <StatBox label="ORD/SEC" value={formatQty(snapshot.orders_per_second)} color={F.YELLOW} small />
            <StatBox label="TIME" value={formatTime(snapshot.current_time)} color={F.WHITE} small />
            <StatBox label="PARTICIPANTS" value={snapshot.participant_summaries.length.toString()} small />
          </div>
        </>
      )}
    </div>
  );

  const renderAgentList = () => (
    <div>
      <SectionHeader title="PARTICIPANTS" icon={<Users size={10} color={F.GRAY} />}
        right={<span style={{ fontSize: '8px', color: F.MUTED }}>{snapshot?.participant_summaries.length || 0}</span>}
      />
      {snapshot?.participant_summaries
        .sort((a, b) => b.pnl - a.pnl)
        .map((p, idx) => {
          const participantType = getEnumVariant(p.participant_type);
          const color = PARTICIPANT_COLORS[participantType] || F.GRAY;
          const pnlColor = p.pnl >= 0 ? F.GREEN : F.RED;
          return (
            <div key={p.id} style={{
              padding: '8px 12px',
              borderBottom: `1px solid ${F.BORDER}`,
              opacity: p.is_active ? 1 : 0.4,
            }}>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <span style={{ fontSize: '9px', fontWeight: 700, color: F.WHITE }}>{idx + 1}.</span>
                  <span style={{ fontSize: '9px', fontWeight: 700, color: F.WHITE }}>{p.name}</span>
                </div>
                <span style={{
                  padding: '1px 5px', backgroundColor: `${color}20`, color,
                  fontSize: '7px', fontWeight: 700, borderRadius: '2px',
                }}>
                  {participantType.replace(/([A-Z])/g, ' $1').trim().toUpperCase()}
                </span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', fontFamily: FONT }}>
                <span style={{ color: pnlColor }}>{formatPnl(p.pnl)}</span>
                <span style={{ color: F.MUTED }}>
                  POS: <span style={{ color: p.position > 0 ? F.GREEN : p.position < 0 ? F.RED : F.GRAY }}>
                    {p.position > 0 ? '+' : ''}{p.position}
                  </span>
                </span>
                <span style={{ color: F.MUTED }}>T:{p.trade_count}</span>
              </div>
              {!p.is_active && (
                <span style={{ fontSize: '7px', color: F.RED, fontWeight: 700 }}>KILL SWITCH</span>
              )}
            </div>
          );
        })}
    </div>
  );

  const renderNewsInjector = () => (
    <div>
      <SectionHeader title="NEWS INJECTION" icon={<Newspaper size={10} color={F.GRAY} />} />
      <div style={{ padding: '12px', display: 'flex', flexDirection: 'column', gap: '10px' }}>
        <div>
          <label style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', display: 'block', marginBottom: '4px' }}>
            HEADLINE
          </label>
          <input
            value={newsHeadline}
            onChange={e => setNewsHeadline(e.target.value)}
            placeholder="e.g. Fed raises rates by 50bps"
            style={{
              width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG,
              color: F.WHITE, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
              fontSize: '10px', fontFamily: FONT, boxSizing: 'border-box',
            }}
            onFocus={e => (e.target.style.borderColor = F.ORANGE)}
            onBlur={e => (e.target.style.borderColor = F.BORDER)}
          />
        </div>
        <div>
          <label style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', display: 'block', marginBottom: '4px' }}>
            SENTIMENT (BEARISH → BULLISH)
          </label>
          <input
            type="range" min={0} max={1} step={0.01} value={newsSentiment}
            onChange={e => setNewsSentiment(Number(e.target.value))}
            style={{ width: '100%', accentColor: newsSentiment > 0.5 ? F.GREEN : F.RED }}
          />
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px', color: F.MUTED }}>
            <span style={{ color: F.RED }}>BEARISH</span>
            <span style={{ color: F.CYAN }}>{(newsSentiment * 2 - 1).toFixed(2)}</span>
            <span style={{ color: F.GREEN }}>BULLISH</span>
          </div>
        </div>
        <div>
          <label style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', display: 'block', marginBottom: '4px' }}>
            MAGNITUDE
          </label>
          <input
            type="range" min={0.001} max={0.10} step={0.001} value={newsMagnitude}
            onChange={e => setNewsMagnitude(Number(e.target.value))}
            style={{ width: '100%', accentColor: F.ORANGE }}
          />
          <span style={{ fontSize: '9px', color: F.CYAN, fontFamily: FONT }}>{(newsMagnitude * 100).toFixed(1)}%</span>
        </div>
        <button
          onClick={injectNews}
          disabled={!newsHeadline || !isRunning}
          style={{
            padding: '8px 16px', backgroundColor: newsHeadline && isRunning ? F.ORANGE : F.MUTED,
            color: F.DARK_BG, border: 'none', borderRadius: '2px',
            fontSize: '9px', fontWeight: 700, cursor: newsHeadline && isRunning ? 'pointer' : 'not-allowed',
          }}
        >
          INJECT NEWS EVENT
        </button>
      </div>
    </div>
  );

  const renderCenterPanel = () => (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Price chart */}
      <div style={{
        height: '200px',
        backgroundColor: F.PANEL_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        position: 'relative',
      }}>
        <SectionHeader title="PRICE" icon={<TrendingUp size={10} color={F.GRAY} />}
          right={quote && (
            <span style={{ fontSize: '14px', fontWeight: 700, color: F.WHITE, fontFamily: FONT }}>
              ${formatPrice(quote.last_price)}
            </span>
          )}
        />
        <canvas
          ref={canvasRef}
          width={800}
          height={150}
          style={{ width: '100%', height: 'calc(100% - 33px)', display: 'block' }}
        />
        {priceHistory.length < 2 && (
          <div style={{
            position: 'absolute', top: '50%', left: '50%',
            transform: 'translate(-50%, -50%)',
            color: F.MUTED, fontSize: '10px',
          }}>
            {isRunning ? 'Waiting for price data...' : 'Start simulation to see price chart'}
          </div>
        )}
      </div>

      {/* Center sub-tabs */}
      <div style={{ display: 'flex', borderBottom: `1px solid ${F.BORDER}`, backgroundColor: F.HEADER_BG }}>
        {(['orderbook', 'trades', 'analytics'] as const).map(tab => (
          <button
            key={tab}
            onClick={() => {
              setActivePanel(tab);
              if (tab === 'analytics') fetchAnalytics();
            }}
            style={{
              padding: '6px 16px',
              backgroundColor: activePanel === tab ? F.ORANGE : 'transparent',
              color: activePanel === tab ? F.DARK_BG : F.GRAY,
              border: 'none', fontSize: '9px', fontWeight: 700,
              letterSpacing: '0.5px', cursor: 'pointer',
            }}
          >
            {tab === 'orderbook' ? 'ORDER BOOK' : tab.toUpperCase()}
          </button>
        ))}
      </div>

      {/* Center content */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {activePanel === 'orderbook' && renderOrderBook()}
        {activePanel === 'trades' && renderTradeTape()}
        {activePanel === 'analytics' && renderAnalyticsPanel()}
      </div>
    </div>
  );

  const renderOrderBook = () => {
    if (!orderbook) {
      return (
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '10px' }}>
          <Layers size={20} style={{ marginRight: '8px', opacity: 0.5 }} />
          {isRunning ? 'Loading order book...' : 'Start simulation to see order book'}
        </div>
      );
    }

    const maxBidQty = Math.max(...orderbook.bids.map(l => l.quantity), 1);
    const maxAskQty = Math.max(...orderbook.asks.map(l => l.quantity), 1);
    const maxQty = Math.max(maxBidQty, maxAskQty);

    return (
      <div style={{ display: 'flex', height: '100%' }}>
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
          {/* Header */}
          <div style={{
            display: 'flex', padding: '4px 8px', fontSize: '8px',
            fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px',
            borderBottom: `1px solid ${F.BORDER}`,
          }}>
            <span style={{ flex: 1 }}>PRICE</span>
            <span style={{ width: '70px', textAlign: 'right' }}>SIZE</span>
            <span style={{ width: '30px', textAlign: 'right' }}>#</span>
          </div>

          {/* Asks (reversed: best ask at bottom) */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', justifyContent: 'flex-end', overflow: 'hidden' }}>
            {orderbook.asks.slice().reverse().map((level, i) => (
              <OrderBookRow key={`a-${i}`} level={level} side="ask" maxQty={maxQty} />
            ))}
          </div>

          {/* Spread */}
          <div style={{
            padding: '6px 8px', backgroundColor: F.HEADER_BG,
            borderTop: `1px solid ${F.BORDER}`, borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex', justifyContent: 'space-between', fontSize: '9px',
          }}>
            <span style={{ color: F.MUTED }}>SPREAD</span>
            <span style={{ color: F.YELLOW, fontFamily: FONT, fontWeight: 700 }}>
              {orderbook.bids[0] && orderbook.asks[0]
                ? formatPrice(orderbook.asks[0].price - orderbook.bids[0].price)
                : '--'}
            </span>
          </div>

          {/* Bids */}
          <div style={{ flex: 1, overflow: 'hidden' }}>
            {orderbook.bids.map((level, i) => (
              <OrderBookRow key={`b-${i}`} level={level} side="bid" maxQty={maxQty} />
            ))}
          </div>
        </div>
      </div>
    );
  };

  const renderTradeTape = () => (
    <div>
      <div style={{
        display: 'flex', padding: '4px 12px', fontSize: '8px',
        fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px',
        borderBottom: `1px solid ${F.BORDER}`, position: 'sticky', top: 0,
        backgroundColor: F.PANEL_BG,
      }}>
        <span style={{ width: '60px' }}>ID</span>
        <span style={{ flex: 1 }}>PRICE</span>
        <span style={{ width: '80px', textAlign: 'right' }}>QTY</span>
        <span style={{ width: '50px', textAlign: 'right' }}>SIDE</span>
        <span style={{ width: '60px', textAlign: 'right' }}>BUYER</span>
        <span style={{ width: '60px', textAlign: 'right' }}>SELLER</span>
      </div>
      {tradeHistory.length === 0 ? (
        <div style={{ padding: '20px', textAlign: 'center', color: F.MUTED, fontSize: '10px' }}>
          No trades yet
        </div>
      ) : (
        tradeHistory.map((t, i) => {
          const sideStr = getEnumVariant(t.aggressor_side);
          const isBuy = sideStr === 'Buy';
          const sideColor = isBuy ? F.GREEN : F.RED;
          return (
            <div key={`${t.id}-${i}`} style={{
              display: 'flex', padding: '3px 12px', fontSize: '9px', fontFamily: FONT,
              borderBottom: `1px solid ${F.BORDER}08`,
              backgroundColor: i === 0 ? `${sideColor}08` : 'transparent',
            }}>
              <span style={{ width: '60px', color: F.MUTED }}>{t.id}</span>
              <span style={{ flex: 1, color: sideColor, fontWeight: 700 }}>{formatPrice(t.price)}</span>
              <span style={{ width: '80px', textAlign: 'right', color: F.WHITE }}>{formatQty(t.quantity)}</span>
              <span style={{ width: '50px', textAlign: 'right', color: sideColor, fontWeight: 700 }}>
                {isBuy ? 'BUY' : 'SELL'}
              </span>
              <span style={{ width: '60px', textAlign: 'right', color: F.MUTED }}>P{t.buyer_id}</span>
              <span style={{ width: '60px', textAlign: 'right', color: F.MUTED }}>P{t.seller_id}</span>
            </div>
          );
        })
      )}
    </div>
  );

  const renderAnalyticsPanel = () => {
    if (!analytics) {
      return (
        <div style={{ padding: '20px', textAlign: 'center', color: F.MUTED, fontSize: '10px' }}>
          Click ANALYTICS to load detailed metrics
        </div>
      );
    }
    return (
      <div style={{ padding: '12px' }}>
        {/* Global stats */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>
            GLOBAL METRICS
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '8px' }}>
            <StatBox label="TRADES" value={formatQty(analytics.total_trades)} />
            <StatBox label="VOLUME" value={formatQty(analytics.total_volume)} />
            <StatBox label="ORDERS" value={formatQty(analytics.total_orders)} />
            <StatBox label="OTR" value={analytics.global_otr.toFixed(1)} color={F.YELLOW} />
          </div>
        </div>

        {/* Instrument metrics */}
        {analytics.instruments.map(im => (
          <div key={im.instrument_id} style={{ marginBottom: '16px' }}>
            <div style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>
              INSTRUMENT {im.instrument_id} QUALITY
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '8px' }}>
              <StatBox label="AVG SPREAD" value={formatPrice(im.avg_spread)} />
              <StatBox label="VWAP" value={im.vwap > 0 ? formatPrice(im.vwap) : '--'} color={F.YELLOW} />
              <StatBox label="VOL" value={formatPct(im.realized_volatility)} color={F.PURPLE} />
              <StatBox label="CB EVENTS" value={im.circuit_breaker_count.toString()} color={im.circuit_breaker_count > 0 ? F.RED : F.GREEN} />
            </div>
          </div>
        ))}

        {/* Participant leaderboard */}
        <div style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>
          PARTICIPANT PERFORMANCE
        </div>
        <div style={{ border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
          <div style={{
            display: 'grid', gridTemplateColumns: '30px 1fr 80px 80px 60px 60px 60px 60px',
            padding: '6px 8px', fontSize: '8px', fontWeight: 700, color: F.MUTED,
            letterSpacing: '0.5px', borderBottom: `1px solid ${F.BORDER}`,
            backgroundColor: F.HEADER_BG,
          }}>
            <span>#</span><span>NAME</span><span style={{ textAlign: 'right' }}>P&L</span>
            <span style={{ textAlign: 'right' }}>DRAWDOWN</span>
            <span style={{ textAlign: 'right' }}>TRADES</span>
            <span style={{ textAlign: 'right' }}>OTR</span>
            <span style={{ textAlign: 'right' }}>FILL%</span>
            <span style={{ textAlign: 'right' }}>FEES</span>
          </div>
          {analytics.participants
            .sort((a, b) => b.total_pnl - a.total_pnl)
            .map((p, idx) => {
              const participantType = getEnumVariant(p.participant_type);
              const color = PARTICIPANT_COLORS[participantType] || F.GRAY;
              const pnlColor = p.total_pnl >= 0 ? F.GREEN : F.RED;
              return (
                <div key={p.participant_id} style={{
                  display: 'grid', gridTemplateColumns: '30px 1fr 80px 80px 60px 60px 60px 60px',
                  padding: '5px 8px', fontSize: '9px', fontFamily: FONT,
                  borderBottom: `1px solid ${F.BORDER}08`,
                }}>
                  <span style={{ color: F.MUTED }}>{idx + 1}</span>
                  <span style={{ color }}>
                    {p.name}
                  </span>
                  <span style={{ textAlign: 'right', color: pnlColor, fontWeight: 700 }}>
                    {formatPnl(p.total_pnl)}
                  </span>
                  <span style={{ textAlign: 'right', color: p.max_drawdown > 0 ? F.RED : F.GRAY }}>
                    {formatPnl(-p.max_drawdown)}
                  </span>
                  <span style={{ textAlign: 'right', color: F.WHITE }}>{p.total_trades}</span>
                  <span style={{ textAlign: 'right', color: F.YELLOW }}>{p.order_to_trade_ratio.toFixed(1)}</span>
                  <span style={{ textAlign: 'right', color: F.CYAN }}>
                    {(p.fill_rate * 100).toFixed(0)}%
                  </span>
                  <span style={{ textAlign: 'right', color: p.net_fees > 0 ? F.RED : F.GREEN }}>
                    {formatPnl(-p.net_fees)}
                  </span>
                </div>
              );
            })}
        </div>
      </div>
    );
  };

  const renderRightPanel = () => (
    <div style={{
      width: '300px',
      backgroundColor: F.PANEL_BG,
      borderLeft: `1px solid ${F.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      <SectionHeader title="LEADERBOARD" icon={<TrendingUp size={10} color={F.GRAY} />} />
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {!snapshot ? (
          <div style={{
            display: 'flex', flexDirection: 'column', alignItems: 'center',
            justifyContent: 'center', height: '200px', color: F.MUTED, fontSize: '10px',
          }}>
            <Users size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>Start simulation to see leaderboard</span>
          </div>
        ) : (
          <>
            {/* Top performers */}
            {snapshot.participant_summaries
              .sort((a, b) => b.pnl - a.pnl)
              .slice(0, 5)
              .map((p, idx) => {
                const participantType = getEnumVariant(p.participant_type);
                const latencyTier = getEnumVariant(p.latency_tier);
                const color = PARTICIPANT_COLORS[participantType] || F.GRAY;
                const pnlColor = p.pnl >= 0 ? F.GREEN : F.RED;
                return (
                  <div key={p.id} style={{
                    padding: '10px 12px',
                    borderBottom: `1px solid ${F.BORDER}`,
                    backgroundColor: idx === 0 ? `${F.ORANGE}08` : 'transparent',
                    borderLeft: idx === 0 ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                  }}>
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                        <span style={{
                          fontSize: '11px', fontWeight: 700,
                          color: idx === 0 ? F.ORANGE : idx === 1 ? F.WHITE : idx === 2 ? '#CD7F32' : F.GRAY,
                        }}>
                          #{idx + 1}
                        </span>
                        <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE }}>{p.name}</span>
                      </div>
                      <span style={{
                        padding: '1px 5px', backgroundColor: `${color}20`, color,
                        fontSize: '7px', fontWeight: 700, borderRadius: '2px',
                      }}>
                        {latencyTier.replace(/([A-Z])/g, ' $1').trim().toUpperCase()}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', fontFamily: FONT }}>
                      <span style={{ color: pnlColor, fontWeight: 700 }}>{formatPnl(p.pnl)}</span>
                      <span style={{ color: F.MUTED, fontSize: '9px' }}>
                        {p.trade_count} trades
                      </span>
                    </div>
                  </div>
                );
              })}

            {/* Bottom performers */}
            <SectionHeader title="WORST PERFORMERS" icon={<TrendingDown size={10} color={F.RED} />} />
            {snapshot.participant_summaries
              .sort((a, b) => a.pnl - b.pnl)
              .slice(0, 3)
              .map(p => {
                const participantType = getEnumVariant(p.participant_type);
                const color = PARTICIPANT_COLORS[participantType] || F.GRAY;
                return (
                  <div key={`w-${p.id}`} style={{
                    padding: '8px 12px',
                    borderBottom: `1px solid ${F.BORDER}`,
                  }}>
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                      <span style={{ fontSize: '9px', color }}>{p.name}</span>
                      <span style={{ fontSize: '9px', color: F.RED, fontWeight: 700, fontFamily: FONT }}>
                        {formatPnl(p.pnl)}
                      </span>
                    </div>
                  </div>
                );
              })}

            {/* Market depth summary */}
            <SectionHeader title="DEPTH SUMMARY" icon={<Layers size={10} color={F.GRAY} />} />
            {orderbook && (
              <div style={{ padding: '12px' }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                  <StatBox
                    label="BID DEPTH"
                    value={formatQty(orderbook.bids.reduce((s, l) => s + l.quantity, 0))}
                    color={F.GREEN}
                    small
                  />
                  <StatBox
                    label="ASK DEPTH"
                    value={formatQty(orderbook.asks.reduce((s, l) => s + l.quantity, 0))}
                    color={F.RED}
                    small
                  />
                  <StatBox
                    label="BID LEVELS"
                    value={orderbook.bids.length.toString()}
                    small
                  />
                  <StatBox
                    label="ASK LEVELS"
                    value={orderbook.asks.length.toString()}
                    small
                  />
                </div>
                {/* Depth imbalance bar */}
                {(() => {
                  const bidTotal = orderbook.bids.reduce((s, l) => s + l.quantity, 0);
                  const askTotal = orderbook.asks.reduce((s, l) => s + l.quantity, 0);
                  const total = bidTotal + askTotal;
                  const bidPct = total > 0 ? (bidTotal / total) * 100 : 50;
                  return (
                    <div style={{ marginTop: '8px' }}>
                      <div style={{ fontSize: '8px', color: F.MUTED, fontWeight: 700, letterSpacing: '0.5px', marginBottom: '4px' }}>
                        BOOK IMBALANCE
                      </div>
                      <div style={{ display: 'flex', height: '6px', borderRadius: '2px', overflow: 'hidden' }}>
                        <div style={{ width: `${bidPct}%`, backgroundColor: F.GREEN, transition: 'width 0.3s' }} />
                        <div style={{ width: `${100 - bidPct}%`, backgroundColor: F.RED, transition: 'width 0.3s' }} />
                      </div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px', marginTop: '2px' }}>
                        <span style={{ color: F.GREEN }}>{bidPct.toFixed(0)}% BID</span>
                        <span style={{ color: F.RED }}>{(100 - bidPct).toFixed(0)}% ASK</span>
                      </div>
                    </div>
                  );
                })()}
              </div>
            )}
          </>
        )}
      </div>
    </div>
  );

  const renderStatusBar = () => (
    <div style={{
      backgroundColor: F.HEADER_BG,
      borderTop: `1px solid ${F.BORDER}`,
      padding: '4px 16px',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      fontSize: '9px',
      color: F.GRAY,
      fontFamily: FONT,
    }}>
      <div style={{ display: 'flex', gap: '16px' }}>
        <span>
          <Radio size={8} style={{ marginRight: '4px', color: isRunning ? F.GREEN : F.MUTED }} />
          {isRunning ? (isPaused ? 'PAUSED' : 'RUNNING') : 'STOPPED'}
        </span>
        <span>TICK RATE: {speed}/step</span>
        {snapshot && <span>TIME: {formatTime(snapshot.current_time)}</span>}
      </div>
      <div style={{ display: 'flex', gap: '16px' }}>
        {error && (
          <span style={{ color: F.RED }}>
            <AlertTriangle size={8} style={{ marginRight: '4px' }} />
            {error}
          </span>
        )}
        <span>MARKET SIMULATION v1.0</span>
      </div>
    </div>
  );

  // ---- Main render ----
  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: F.DARK_BG,
      fontFamily: FONT,
      color: F.WHITE,
    }}>
      {renderTopNav()}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {renderLeftPanel()}
        {renderCenterPanel()}
        {renderRightPanel()}
      </div>
      {renderStatusBar()}
    </div>
  );
};

export default MarketSimTab;
