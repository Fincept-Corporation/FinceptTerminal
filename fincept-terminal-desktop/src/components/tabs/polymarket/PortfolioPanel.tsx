// PortfolioPanel.tsx — standalone portfolio component for Polymarket tab
import React, { useState, useEffect, useRef } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  RefreshCw, Wallet, Wifi, WifiOff, X, AlertCircle,
  User, History, Copy, ExternalLink as ExtLink, Gift, Coins,
  Activity, LayoutList, ArrowUpRight, ArrowDownLeft, Archive,
} from 'lucide-react';
import polymarketApiService, {
  UserPosition, UserTradeHistory, UserActivity, UserProfile,
  UserPortfolioValue, WSUserUpdate, WSUserOrderEvent, WSUserTradeEvent,
  ClosedPosition,
} from '@/services/polymarket/polymarketApiService';
import { getCredentialByService } from '@/services/core/sqliteService';
import { C, fmtVol, sectionHeader, statCell } from './tokens';

// ── Props ────────────────────────────────────────────────────────────────────
export interface PortfolioPanelProps {
  /** Pre-set wallet address (from saved credentials). If empty, shows the entry form. */
  initialWallet?: string;
}

// ── Component ────────────────────────────────────────────────────────────────
const PortfolioPanel: React.FC<PortfolioPanelProps> = ({ initialWallet = '' }) => {
  type PortfolioTab = 'profile' | 'positions' | 'closed' | 'trades' | 'activity' | 'live';

  const [portfolioTab, setPortfolioTab]         = useState<PortfolioTab>('profile');
  const [walletAddress, setWalletAddress]        = useState<string>(initialWallet);
  const [walletInput, setWalletInput]            = useState<string>(initialWallet);
  const [positions, setPositions]                = useState<UserPosition[]>([]);
  const [closedPositions, setClosedPositions]    = useState<ClosedPosition[]>([]);
  const [trades, setTrades]                      = useState<UserTradeHistory[]>([]);
  const [activity, setActivity]                  = useState<UserActivity[]>([]);
  const [profile, setProfile]                    = useState<UserProfile | null>(null);
  const [portfolioValue, setPortfolioValue]      = useState<number | null>(null);
  const [usdcBalance, setUsdcBalance]            = useState<number | null>(null);
  const [loading, setLoading]                    = useState(false);
  const [error, setError]                        = useState<string | null>(null);
  const [liveOrders, setLiveOrders]              = useState<WSUserOrderEvent[]>([]);
  const [liveTrades, setLiveTrades]              = useState<WSUserTradeEvent[]>([]);
  const [wsConnected, setWsConnected]            = useState(false);
  const [credentialsReady, setCredentialsReady]  = useState(polymarketApiService.hasCredentials);
  const userWsRef                                = useRef<WebSocket | null>(null);

  // Auto-populate wallet + credentials on mount.
  // PolymarketCredentialsPanel only sets credentials when the Settings tab is visited,
  // so we must load them here too for cases where the user goes directly to Portfolio.
  useEffect(() => {
    const init = async () => {
      // 1. Load API credentials into service if not already loaded
      if (!polymarketApiService.hasCredentials) {
        try {
          const cred = await getCredentialByService('polymarket');
          if (cred && cred.api_key) {
            polymarketApiService.setCredentials({
              apiKey: cred.api_key,
              apiSecret: cred.api_secret ?? '',
              apiPassphrase: cred.password ?? '',
              walletAddress: cred.username ?? undefined,
            });
            setCredentialsReady(true);
          }
        } catch { /* non-fatal */ }
      } else {
        setCredentialsReady(true);
      }

      // 2. Determine wallet address: credentials wallet → SQLite saved address
      const fromCreds = polymarketApiService.getWalletAddress();
      if (fromCreds) {
        setWalletAddress(fromCreds);
        setWalletInput(fromCreds);
        return;
      }
      try {
        const addr = await invoke<string | null>('db_get_setting', { key: 'polymarket_wallet_address' });
        if (addr) { setWalletAddress(addr); setWalletInput(addr); }
      } catch { /* non-fatal */ }
    };
    init();
  }, []);

  // Load portfolio when wallet address is set, and persist to SQLite
  useEffect(() => {
    if (walletAddress) {
      invoke('db_save_setting', { key: 'polymarket_wallet_address', value: walletAddress, category: 'polymarket' }).catch(() => {});
      loadPortfolio(walletAddress);
    }
  }, [walletAddress]);

  // Fetch USDC balance separately once credentials become ready (may happen after wallet loads)
  useEffect(() => {
    if (credentialsReady && walletAddress) {
      polymarketApiService.getBalanceAllowance('COLLATERAL')
        .then(r => setUsdcBalance(parseFloat(r.balance)))
        .catch(() => {});
    }
  }, [credentialsReady, walletAddress]);

  // User WebSocket — runs once credentials + wallet are both ready
  useEffect(() => {
    if (!walletAddress || !credentialsReady) {
      if (userWsRef.current) { userWsRef.current.close(); userWsRef.current = null; setWsConnected(false); }
      return;
    }
    if (userWsRef.current && userWsRef.current.readyState === WebSocket.OPEN) return;
    try {
      const ws = polymarketApiService.connectUserWebSocket([], (update: WSUserUpdate) => {
        if (update.event_type === 'order') {
          const ev = update as WSUserOrderEvent;
          setLiveOrders(prev => {
            const idx = prev.findIndex(o => o.id === ev.id);
            if (idx >= 0) { const next = [...prev]; next[idx] = ev; return next; }
            return [ev, ...prev].slice(0, 100);
          });
        } else if (update.event_type === 'trade') {
          const ev = update as WSUserTradeEvent;
          setLiveTrades(prev => {
            if (prev.find(t => t.id === ev.id)) return prev;
            return [ev, ...prev].slice(0, 100);
          });
        }
      });
      userWsRef.current = ws;
      ws.onopen  = () => setWsConnected(true);
      ws.onclose = () => setWsConnected(false);
    } catch (err) { console.warn('[PortfolioPanel] WS connect failed:', err); }
    return () => { if (userWsRef.current) { userWsRef.current.close(); userWsRef.current = null; setWsConnected(false); } };
  }, [walletAddress, credentialsReady]);

  // Cleanup on unmount
  useEffect(() => () => { if (userWsRef.current) userWsRef.current.close(); }, []);

  const loadPortfolio = async (addr: string) => {
    setLoading(true); setError(null);
    try {
      const [pos, closed, trd, valArr, act, balResp] = await Promise.all([
        polymarketApiService.getUserPositions(addr, { limit: 100, sortBy: 'CASHPNL', sortDirection: 'DESC' }).catch(() => [] as UserPosition[]),
        polymarketApiService.getClosedPositions(addr, { limit: 50, sortBy: 'REALIZEDPNL', sortDirection: 'DESC' }).catch(() => [] as ClosedPosition[]),
        polymarketApiService.getUserTrades(addr, { limit: 100 }).catch(() => [] as UserTradeHistory[]),
        polymarketApiService.getPortfolioValue(addr).catch(() => [] as UserPortfolioValue[]),
        polymarketApiService.getUserActivity(addr, { limit: 100 }).catch(() => [] as UserActivity[]),
        (polymarketApiService.hasCredentials ? polymarketApiService.getBalanceAllowance('COLLATERAL') : Promise.resolve(null)).catch(() => null),
      ]);
      setPositions(pos);
      setClosedPositions(closed);
      setTrades(trd);
      setPortfolioValue(valArr[0]?.value ?? null);
      setUsdcBalance(balResp ? parseFloat(balResp.balance) : null);
      setActivity(act);
      const rec = act.find(a => a.name || a.pseudonym || a.profileImage);
      setProfile({
        proxyWallet: addr,
        name: rec?.name ?? '',
        pseudonym: rec?.pseudonym ?? '',
        bio: rec?.bio ?? '',
        profileImage: rec?.profileImage ?? '',
        profileImageOptimized: rec?.profileImageOptimized ?? '',
      });
    } catch {
      setError('Failed to load portfolio data');
    } finally {
      setLoading(false);
    }
  };

  const clear = () => {
    invoke('db_save_setting', { key: 'polymarket_wallet_address', value: '', category: 'polymarket' }).catch(() => {});
    setWalletAddress(''); setWalletInput('');
    setPositions([]); setClosedPositions([]); setTrades([]); setActivity([]); setProfile(null); setPortfolioValue(null); setUsdcBalance(null);
  };

  // ── Wallet entry form ───────────────────────────────────────────────────────
  if (!walletAddress) return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{ padding: '8px 12px', borderBottom: `2px solid ${C.orange}`, backgroundColor: C.bg, display: 'flex', alignItems: 'center', gap: 8 }}>
        <Wallet size={14} style={{ color: C.orange }} />
        <span style={{ fontSize: 11, fontWeight: 'bold', color: C.orange, fontFamily: C.font }}>PORTFOLIO</span>
      </div>
      <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ width: 420, padding: 24 }}>
          <div style={{ fontSize: 11, color: C.white, fontWeight: 'bold', marginBottom: 4, fontFamily: C.font }}>ENTER WALLET ADDRESS</div>
          <div style={{ fontSize: 9, color: C.muted, marginBottom: 16, fontFamily: C.font, lineHeight: 1.5 }}>
            View positions, trade history and activity for any Polymarket wallet.{' '}
            {credentialsReady
              ? <span style={{ color: C.green }}>Credentials connected — your wallet will auto-load.</span>
              : <span style={{ color: C.faint }}>Connect credentials in Settings → Polymarket for live trading.</span>}
          </div>
          <div style={{ display: 'flex', backgroundColor: C.bg, border: `1px solid ${C.border}`, marginBottom: 10 }}>
            <input
              type="text" placeholder="0x..."
              value={walletInput} onChange={e => setWalletInput(e.target.value)}
              onKeyPress={e => e.key === 'Enter' && walletInput.trim() && setWalletAddress(walletInput.trim())}
              style={{ flex: 1, padding: '8px 10px', backgroundColor: 'transparent', border: 'none', outline: 'none', color: C.white, fontSize: 11, fontFamily: C.font }}
            />
          </div>
          <button
            onClick={() => walletInput.trim() && setWalletAddress(walletInput.trim())}
            style={{ width: '100%', padding: '8px', backgroundColor: C.orange, color: '#000', border: 'none', fontSize: 10, fontWeight: 'bold', fontFamily: C.font, cursor: 'pointer', borderRadius: 1 }}
          >LOAD PORTFOLIO</button>
        </div>
      </div>
    </div>
  );

  // ── Derived stats ───────────────────────────────────────────────────────────
  const totalPnl       = positions.reduce((s, p) => s + (p.cashPnl ?? 0), 0);
  const openRealizedPnl = positions.reduce((s, p) => s + (p.realizedPnl ?? 0), 0);
  const closedRealizedPnl = closedPositions.reduce((s, p) => s + (p.realizedPnl ?? 0), 0);
  const totalRealizedPnl = openRealizedPnl + closedRealizedPnl;
  const totalVol    = activity.reduce((s, a) => s + (a.usdcSize ?? 0), 0);
  const shortAddr   = `${walletAddress.slice(0, 6)}...${walletAddress.slice(-4)}`;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>

      {/* Header */}
      <div style={{ padding: '8px 12px', borderBottom: `2px solid ${C.orange}`, backgroundColor: C.bg, display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexShrink: 0 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
          <Wallet size={14} style={{ color: C.orange }} />
          <span style={{ fontSize: 11, fontWeight: 'bold', color: C.orange, fontFamily: C.font }}>PORTFOLIO</span>
          <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>{shortAddr}</span>
          {wsConnected
            ? <span style={{ display: 'flex', alignItems: 'center', gap: 3, fontSize: 9, color: C.green, fontFamily: C.font }}><Wifi size={9} /> LIVE</span>
            : credentialsReady
              ? <span style={{ display: 'flex', alignItems: 'center', gap: 3, fontSize: 9, color: C.faint, fontFamily: C.font }}><WifiOff size={9} /> WS OFF</span>
              : null}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <button onClick={() => loadPortfolio(walletAddress)}
            style={{ display: 'flex', alignItems: 'center', gap: 4, padding: '3px 8px', backgroundColor: C.header, border: `1px solid ${C.border}`, color: C.muted, fontSize: 9, fontFamily: C.font, cursor: 'pointer', borderRadius: 1 }}>
            <RefreshCw size={10} className={loading ? 'animate-spin' : ''} /> REFRESH
          </button>
          <button onClick={clear}
            style={{ padding: '3px 6px', backgroundColor: 'transparent', border: `1px solid ${C.border}`, color: C.muted, cursor: 'pointer', fontFamily: C.font, borderRadius: 1 }}>
            <X size={11} />
          </button>
        </div>
      </div>

      {/* Stats bar */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(6, 1fr)', gap: 1, backgroundColor: C.border, flexShrink: 0 }}>
        {statCell('USDC BALANCE', usdcBalance != null ? fmtVol(usdcBalance) : credentialsReady ? '—' : 'NO CREDS', usdcBalance != null ? C.cyan : C.muted)}
        {statCell('POSITION VALUE', portfolioValue != null ? fmtVol(portfolioValue) : '—', C.orange)}
        {statCell('OPEN POSITIONS', String(positions.filter(p => p.size > 0).length))}
        {statCell('CASH P&L', totalPnl !== 0 ? `${totalPnl >= 0 ? '+' : ''}${fmtVol(totalPnl)}` : '—', totalPnl >= 0 ? C.green : C.red)}
        {statCell('REALIZED P&L', totalRealizedPnl !== 0 ? `${totalRealizedPnl >= 0 ? '+' : ''}${fmtVol(totalRealizedPnl)}` : '—', totalRealizedPnl >= 0 ? C.green : C.red)}
        {statCell('TOTAL VOLUME', totalVol > 0 ? fmtVol(totalVol) : '—')}
      </div>

      {/* Inner tab bar */}
      <div style={{ display: 'flex', borderBottom: `1px solid ${C.border}`, backgroundColor: C.bg, flexShrink: 0, overflowX: 'auto' }}>
        {([
          { key: 'profile',   label: 'PROFILE',                                                              icon: <User size={9} /> },
          { key: 'positions', label: `OPEN (${positions.filter(p => p.size > 0).length})`,                  icon: <LayoutList size={9} /> },
          { key: 'closed',    label: `CLOSED (${closedPositions.length})`,                                   icon: <Archive size={9} /> },
          { key: 'trades',    label: `TRADES (${trades.length})`,                                            icon: <Activity size={9} /> },
          { key: 'activity',  label: `ACTIVITY (${activity.length})`,                                       icon: <History size={9} /> },
          { key: 'live',      label: `LIVE${liveOrders.length + liveTrades.length > 0 ? ` (${liveOrders.length + liveTrades.length})` : ''}`, icon: <Wifi size={9} /> },
        ] as const).map(({ key, label, icon }) => (
          <button key={key} onClick={() => setPortfolioTab(key)}
            style={{
              display: 'flex', alignItems: 'center', gap: 4,
              padding: '6px 12px', fontSize: 9, fontWeight: 'bold', fontFamily: C.font,
              background: portfolioTab === key ? C.orange : 'transparent',
              color: portfolioTab === key ? '#000' : C.muted,
              border: 'none', cursor: 'pointer',
              borderRight: `1px solid ${C.border}`,
              textTransform: 'uppercase', letterSpacing: '0.5px',
              whiteSpace: 'nowrap', flexShrink: 0,
            }}>{icon}{label}</button>
        ))}
      </div>

      {/* Tab content */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {loading && (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: 200, flexDirection: 'column', gap: 10 }}>
            <RefreshCw size={24} className="animate-spin" style={{ color: C.orange }} />
            <div style={{ fontSize: 10, color: C.muted, fontFamily: C.font }}>LOADING PORTFOLIO...</div>
          </div>
        )}
        {!loading && error && (
          <div style={{ margin: 16, padding: 12, backgroundColor: '#330000', border: `1px solid ${C.red}`, display: 'flex', gap: 8 }}>
            <AlertCircle size={16} style={{ color: C.red, flexShrink: 0, marginTop: 1 }} />
            <div style={{ fontSize: 9, color: '#FF8888', fontFamily: C.font }}>{error}</div>
          </div>
        )}
        {!loading && portfolioTab === 'profile'   && <ProfileTab profile={profile} walletAddress={walletAddress} positions={positions} trades={trades} activity={activity} portfolioValue={portfolioValue} closedPositions={closedPositions} usdcBalance={usdcBalance} />}
        {!loading && !error && portfolioTab === 'positions' && <PositionsTab positions={positions} />}
        {!loading && !error && portfolioTab === 'closed'    && <ClosedPositionsTab closedPositions={closedPositions} />}
        {!loading && !error && portfolioTab === 'trades'    && <TradesTab trades={trades} />}
        {!loading && !error && portfolioTab === 'activity'  && <ActivityTab activity={activity} />}
        {portfolioTab === 'live' && <LiveFeedTab liveOrders={liveOrders} liveTrades={liveTrades} wsConnected={wsConnected} credentialsReady={credentialsReady} onClear={() => { setLiveOrders([]); setLiveTrades([]); }} />}
      </div>
    </div>
  );
};

// ── Profile tab ───────────────────────────────────────────────────────────────
interface ProfileTabProps {
  profile: UserProfile | null;
  walletAddress: string;
  positions: UserPosition[];
  trades: UserTradeHistory[];
  activity: UserActivity[];
  portfolioValue: number | null;
  closedPositions: ClosedPosition[];
  usdcBalance: number | null;
}

const ProfileTab: React.FC<ProfileTabProps> = ({ profile, walletAddress, positions, trades, activity, portfolioValue, closedPositions, usdcBalance }) => {
  const shortAddr      = walletAddress ? `${walletAddress.slice(0, 8)}...${walletAddress.slice(-6)}` : '—';
  const totalPnl       = positions.reduce((s, p) => s + (p.cashPnl ?? 0), 0);
  const openRealizedPnl = positions.reduce((s, p) => s + (p.realizedPnl ?? 0), 0);
  const closedRealizedPnl = closedPositions.reduce((s, p) => s + (p.realizedPnl ?? 0), 0);
  const totalRealizedPnl = openRealizedPnl + closedRealizedPnl;
  const buyTrades      = trades.filter(t => t.side === 'BUY').length;
  const totalVolumeUsdc = activity.reduce((s, a) => s + (a.usdcSize ?? 0), 0);
  const deposits       = activity.filter(a => a.type === 'DEPOSIT');
  const withdrawals    = activity.filter(a => a.type === 'WITHDRAWAL');
  const rewards        = activity.filter(a => a.type === 'REWARD');
  const totalDeposited = deposits.reduce((s, a) => s + (a.usdcSize ?? 0), 0);
  const totalWithdrawn = withdrawals.reduce((s, a) => s + (a.usdcSize ?? 0), 0);
  const totalRewards   = rewards.reduce((s, a) => s + (a.usdcSize ?? 0), 0);

  return (
    <div style={{ padding: 16, display: 'flex', flexDirection: 'column', gap: 16 }}>
      {/* Identity card */}
      <div style={{ backgroundColor: C.panel, border: `1px solid ${C.border}`, padding: 16, display: 'flex', gap: 16, alignItems: 'flex-start' }}>
        <div style={{ width: 56, height: 56, borderRadius: '50%', border: `2px solid ${C.orange}`, backgroundColor: C.header, display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0, overflow: 'hidden' }}>
          {profile?.profileImage
            ? <img src={profile.profileImage} alt="avatar" style={{ width: '100%', height: '100%', objectFit: 'cover' }} onError={e => { (e.target as HTMLImageElement).style.display = 'none'; }} />
            : <User size={24} style={{ color: C.orange }} />}
        </div>
        <div style={{ flex: 1, minWidth: 0 }}>
          <div style={{ fontSize: 15, fontWeight: 'bold', color: C.white, fontFamily: C.font, marginBottom: 2 }}>
            {profile?.name || profile?.pseudonym || 'Anonymous Trader'}
          </div>
          {profile?.pseudonym && profile.name && (
            <div style={{ fontSize: 10, color: C.muted, fontFamily: C.font, marginBottom: 4 }}>@{profile.pseudonym}</div>
          )}
          {profile?.bio && (
            <div style={{ fontSize: 10, color: C.muted, fontFamily: C.font, lineHeight: 1.4, marginBottom: 8 }}>{profile.bio}</div>
          )}
          <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
            <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font, fontWeight: 'bold' }}>WALLET</span>
            <code style={{ fontSize: 9, color: C.cyan, fontFamily: C.font }}>{shortAddr}</code>
            <button onClick={() => navigator.clipboard.writeText(walletAddress)} title="Copy address"
              style={{ background: 'none', border: 'none', cursor: 'pointer', padding: 0, display: 'flex', alignItems: 'center' }}>
              <Copy size={10} style={{ color: C.faint }} />
            </button>
            <a href={`https://polymarket.com/profile/${walletAddress}`} target="_blank" rel="noopener noreferrer"
              style={{ display: 'flex', alignItems: 'center', color: C.faint, textDecoration: 'none' }} title="View on Polymarket">
              <ExtLink size={10} />
            </a>
          </div>
        </div>
      </div>

      {/* Stats grid */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 1, backgroundColor: C.border }}>
        {statCell('USDC BALANCE', usdcBalance != null ? fmtVol(usdcBalance) : '—', C.cyan)}
        {statCell('POSITION VALUE', portfolioValue != null ? fmtVol(portfolioValue) : '—', C.orange)}
        {statCell('OPEN POSITIONS', String(positions.filter(p => p.size > 0).length))}
        {statCell('CASH P&L', totalPnl !== 0 ? `${totalPnl >= 0 ? '+' : ''}${fmtVol(totalPnl)}` : '—', totalPnl >= 0 ? C.green : C.red)}
        {statCell('TOTAL REALIZED P&L', totalRealizedPnl !== 0 ? `${totalRealizedPnl >= 0 ? '+' : ''}${fmtVol(totalRealizedPnl)}` : '—', totalRealizedPnl >= 0 ? C.green : C.red)}
        {statCell('TOTAL TRADES', String(trades.length))}
      </div>

      {/* Financial summary */}
      <div style={{ backgroundColor: C.panel, border: `1px solid ${C.border}` }}>
        {sectionHeader('FINANCIAL SUMMARY')}
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: 1, backgroundColor: C.border }}>
          {statCell('TOTAL VOLUME',    fmtVol(totalVolumeUsdc))}
          {statCell('TOTAL REWARDS',   totalRewards > 0 ? fmtVol(totalRewards) : '—', totalRewards > 0 ? C.cyan : C.muted)}
          {statCell('TOTAL DEPOSITS',  totalDeposited > 0 ? fmtVol(totalDeposited) : '—', C.green)}
          {statCell('TOTAL WITHDRAWN', totalWithdrawn > 0 ? fmtVol(totalWithdrawn) : '—', totalWithdrawn > 0 ? C.red : C.muted)}
        </div>
      </div>

      {/* Activity breakdown */}
      <div style={{ backgroundColor: C.panel, border: `1px solid ${C.border}` }}>
        {sectionHeader('ACTIVITY BREAKDOWN')}
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: 1, backgroundColor: C.border }}>
          {statCell('DEPOSITS',    String(deposits.length),    C.green)}
          {statCell('WITHDRAWALS', String(withdrawals.length), withdrawals.length > 0 ? C.red : C.muted)}
          {statCell('REWARDS',     String(rewards.length),     rewards.length > 0 ? C.cyan : C.muted)}
          {statCell('BUY / SELL',  `${activity.filter(a => a.type === 'BUY').length} / ${activity.filter(a => a.type === 'SELL').length}`)}
        </div>
      </div>

      {/* Top positions */}
      {positions.length > 0 && (
        <div style={{ backgroundColor: C.panel, border: `1px solid ${C.border}` }}>
          {sectionHeader('TOP POSITIONS BY P&L')}
          {positions.slice(0, 5).map((pos, i) => (
            <div key={i} style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '6px 10px', borderBottom: `1px solid ${C.borderFaint}` }}>
              <span style={{ fontSize: 8, fontWeight: 'bold', color: pos.outcome === 'Yes' ? C.green : C.red, fontFamily: C.font, minWidth: 28 }}>{pos.outcome?.toUpperCase()}</span>
              <span style={{ flex: 1, fontSize: 9, color: C.white, fontFamily: C.font, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>{pos.title}</span>
              <span style={{ fontSize: 10, fontWeight: 'bold', color: pos.cashPnl >= 0 ? C.green : C.red, fontFamily: C.font }}>
                {pos.cashPnl >= 0 ? '+' : ''}{fmtVol(pos.cashPnl)}
              </span>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

// ── Positions tab ─────────────────────────────────────────────────────────────
const PositionsTab: React.FC<{ positions: UserPosition[] }> = ({ positions }) => {
  if (positions.length === 0) return (
    <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}>
      <LayoutList size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
      <div style={{ fontSize: 10, fontFamily: C.font }}>NO OPEN POSITIONS</div>
    </div>
  );
  return (
    <>
      {sectionHeader(`POSITIONS (${positions.length})`)}
      <div style={{ display: 'grid', gridTemplateColumns: '3fr 70px 70px 70px 70px 70px', padding: '3px 10px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, gap: 6 }}>
        {['MARKET', 'OUTCOME', 'SIZE', 'AVG', 'CUR', 'P&L'].map((h, i) => (
          <span key={h} style={{ fontSize: 8, color: C.orange, fontWeight: 'bold', fontFamily: C.font, textAlign: i === 0 ? 'left' : 'right' }}>{h}</span>
        ))}
      </div>
      {positions.map((pos, idx) => {
        const pnlColor = pos.cashPnl > 0 ? C.green : pos.cashPnl < 0 ? C.red : C.muted;
        return (
          <div key={idx} style={{ display: 'grid', gridTemplateColumns: '3fr 70px 70px 70px 70px 70px', padding: '6px 10px', borderBottom: `1px solid ${C.borderFaint}`, gap: 6, alignItems: 'center' }}>
            <div style={{ overflow: 'hidden' }}>
              <div style={{ fontSize: 10, color: C.white, fontWeight: 'bold', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', fontFamily: C.font }}>{pos.title}</div>
              {pos.endDate && <div style={{ fontSize: 8, color: C.faint, fontFamily: C.font }}>EXP: {new Date(pos.endDate).toLocaleDateString()}</div>}
            </div>
            <span style={{ fontSize: 9, fontWeight: 'bold', color: pos.outcome === 'Yes' ? C.green : C.red, textAlign: 'right', fontFamily: C.font }}>{pos.outcome?.toUpperCase()}</span>
            <span style={{ fontSize: 10, color: C.white, textAlign: 'right', fontFamily: C.font }}>{pos.size.toFixed(0)}</span>
            <span style={{ fontSize: 10, color: C.muted,  textAlign: 'right', fontFamily: C.font }}>${pos.avgPrice.toFixed(3)}</span>
            <span style={{ fontSize: 10, color: C.white,  textAlign: 'right', fontFamily: C.font }}>${pos.curPrice.toFixed(3)}</span>
            <span style={{ fontSize: 10, fontWeight: 'bold', color: pnlColor, textAlign: 'right', fontFamily: C.font }}>{pos.cashPnl >= 0 ? '+' : ''}{fmtVol(pos.cashPnl)}</span>
          </div>
        );
      })}
    </>
  );
};

// ── Closed positions tab ──────────────────────────────────────────────────────
const ClosedPositionsTab: React.FC<{ closedPositions: ClosedPosition[] }> = ({ closedPositions }) => {
  if (closedPositions.length === 0) return (
    <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}>
      <Archive size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
      <div style={{ fontSize: 10, fontFamily: C.font }}>NO CLOSED POSITIONS</div>
    </div>
  );

  const totalRealizedPnl = closedPositions.reduce((s, p) => s + (p.realizedPnl ?? 0), 0);

  return (
    <>
      {sectionHeader(`CLOSED POSITIONS (${closedPositions.length}) — REALIZED P&L: ${totalRealizedPnl >= 0 ? '+' : ''}${fmtVol(totalRealizedPnl)}`)}
      <div style={{ display: 'grid', gridTemplateColumns: '3fr 70px 70px 70px 70px 80px', padding: '3px 10px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, gap: 6 }}>
        {['MARKET', 'OUTCOME', 'AVG BUY', 'CUR PRICE', 'REALIZED P&L', 'CLOSED'].map((h, i) => (
          <span key={h} style={{ fontSize: 8, color: C.orange, fontWeight: 'bold', fontFamily: C.font, textAlign: i === 0 ? 'left' : 'right' }}>{h}</span>
        ))}
      </div>
      {closedPositions.map((pos, idx) => {
        const pnlColor = pos.realizedPnl > 0 ? C.green : pos.realizedPnl < 0 ? C.red : C.muted;
        const ts = typeof pos.timestamp === 'number' ? pos.timestamp : Number(pos.timestamp);
        const ms = ts < 1e12 ? ts * 1000 : ts;
        return (
          <div key={idx} style={{ display: 'grid', gridTemplateColumns: '3fr 70px 70px 70px 70px 80px', padding: '6px 10px', borderBottom: `1px solid ${C.borderFaint}`, gap: 6, alignItems: 'center' }}>
            <div style={{ overflow: 'hidden' }}>
              <div style={{ fontSize: 10, color: C.white, fontWeight: 'bold', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', fontFamily: C.font }}>{pos.title}</div>
              {pos.endDate && <div style={{ fontSize: 8, color: C.faint, fontFamily: C.font }}>EXP: {new Date(pos.endDate).toLocaleDateString()}</div>}
            </div>
            <span style={{ fontSize: 9, fontWeight: 'bold', color: pos.outcome === 'Yes' ? C.green : C.red, textAlign: 'right', fontFamily: C.font }}>{pos.outcome?.toUpperCase()}</span>
            <span style={{ fontSize: 10, color: C.muted, textAlign: 'right', fontFamily: C.font }}>${pos.avgPrice.toFixed(3)}</span>
            <span style={{ fontSize: 10, color: C.white, textAlign: 'right', fontFamily: C.font }}>${pos.curPrice.toFixed(3)}</span>
            <span style={{ fontSize: 10, fontWeight: 'bold', color: pnlColor, textAlign: 'right', fontFamily: C.font }}>{pos.realizedPnl >= 0 ? '+' : ''}{fmtVol(pos.realizedPnl)}</span>
            <span style={{ fontSize: 8, color: C.faint, textAlign: 'right', fontFamily: C.font }}>{new Date(ms).toLocaleDateString()}</span>
          </div>
        );
      })}
    </>
  );
};

// ── Trades tab ────────────────────────────────────────────────────────────────
const TradesTab: React.FC<{ trades: UserTradeHistory[] }> = ({ trades }) => {
  if (trades.length === 0) return (
    <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}>
      <Activity size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
      <div style={{ fontSize: 10, fontFamily: C.font }}>NO TRADE HISTORY</div>
    </div>
  );
  return (
    <>
      {sectionHeader(`TRADE HISTORY (${trades.length})`)}
      <div style={{ display: 'grid', gridTemplateColumns: '40px 3fr 60px 70px 70px 80px', padding: '3px 10px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, gap: 6 }}>
        {['SIDE', 'MARKET', 'OUTCOME', 'SIZE', 'PRICE', 'TIME'].map((h, i) => (
          <span key={h} style={{ fontSize: 8, color: C.orange, fontWeight: 'bold', fontFamily: C.font, textAlign: i < 2 ? 'left' : 'right' }}>{h}</span>
        ))}
      </div>
      {trades.map((trade, idx) => {
        const ts = typeof trade.timestamp === 'number' ? trade.timestamp : Number(trade.timestamp);
        const ms = ts < 1e12 ? ts * 1000 : ts;
        return (
          <div key={idx} style={{ display: 'grid', gridTemplateColumns: '40px 3fr 60px 70px 70px 80px', padding: '5px 10px', borderBottom: `1px solid ${C.borderFaint}`, gap: 6, alignItems: 'center' }}>
            <span style={{
              fontSize: 8, fontWeight: 'bold', fontFamily: C.font, padding: '1px 4px', textAlign: 'center',
              backgroundColor: trade.side === 'BUY' ? '#00D66F20' : '#FF3B3B20',
              color: trade.side === 'BUY' ? C.green : C.red,
              border: `1px solid ${trade.side === 'BUY' ? C.green : C.red}44`,
            }}>{trade.side}</span>
            <span style={{ fontSize: 9, color: C.white, fontFamily: C.font, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', display: 'block' }}>{trade.title}</span>
            <span style={{ fontSize: 9, color: trade.outcome === 'Yes' ? C.green : C.red, fontFamily: C.font, textAlign: 'right', fontWeight: 'bold' }}>{trade.outcome?.toUpperCase()}</span>
            <span style={{ fontSize: 10, color: C.white, textAlign: 'right', fontFamily: C.font }}>{typeof trade.size === 'number' ? trade.size.toFixed(2) : trade.size}</span>
            <span style={{ fontSize: 10, color: C.white, fontWeight: 'bold', textAlign: 'right', fontFamily: C.font }}>${typeof trade.price === 'number' ? trade.price.toFixed(4) : trade.price}</span>
            <span style={{ fontSize: 8, color: C.faint, textAlign: 'right', fontFamily: C.font }}>{new Date(ms).toLocaleDateString()}</span>
          </div>
        );
      })}
    </>
  );
};

// ── Activity tab ──────────────────────────────────────────────────────────────
const ActivityTab: React.FC<{ activity: UserActivity[] }> = ({ activity }) => {
  if (activity.length === 0) return (
    <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}>
      <History size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
      <div style={{ fontSize: 10, fontFamily: C.font }}>NO ACTIVITY FOUND</div>
    </div>
  );

  const typeIcon = (type: string) => {
    switch (type) {
      case 'DEPOSIT':    return <ArrowDownLeft size={10} style={{ color: C.green }} />;
      case 'WITHDRAWAL': return <ArrowUpRight  size={10} style={{ color: C.red }} />;
      case 'REWARD':     return <Gift          size={10} style={{ color: C.cyan }} />;
      case 'BUY':        return <ArrowDownLeft size={10} style={{ color: C.green }} />;
      case 'SELL':       return <ArrowUpRight  size={10} style={{ color: C.orange }} />;
      default:           return <Coins         size={10} style={{ color: C.muted }} />;
    }
  };

  const typeColor = (type: string) => {
    switch (type) {
      case 'DEPOSIT':    return C.green;
      case 'WITHDRAWAL': return C.red;
      case 'REWARD':     return C.cyan;
      case 'BUY':        return C.green;
      case 'SELL':       return C.orange;
      default:           return C.muted;
    }
  };

  return (
    <>
      {sectionHeader(`ACTIVITY HISTORY (${activity.length})`)}
      <div style={{ display: 'grid', gridTemplateColumns: '70px 3fr 80px 80px 80px', padding: '3px 10px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, gap: 6 }}>
        {['TYPE', 'DESCRIPTION', 'USDC', 'TOKENS', 'TIME'].map((h, i) => (
          <span key={h} style={{ fontSize: 8, color: C.orange, fontWeight: 'bold', fontFamily: C.font, textAlign: i < 2 ? 'left' : 'right' }}>{h}</span>
        ))}
      </div>
      {activity.map((act, idx) => {
        const ts    = typeof act.timestamp === 'number' ? act.timestamp : Number(act.timestamp);
        const ms    = ts < 1e12 ? ts * 1000 : ts;
        const label = act.title || (act.type === 'DEPOSIT' ? 'USDC Deposit' : act.type === 'WITHDRAWAL' ? 'USDC Withdrawal' : act.type === 'REWARD' ? 'Reward' : act.outcome || act.type);
        const txShort = act.transactionHash ? `${act.transactionHash.slice(0, 8)}...` : '';
        return (
          <div key={idx} style={{ display: 'grid', gridTemplateColumns: '70px 3fr 80px 80px 80px', padding: '5px 10px', borderBottom: `1px solid ${C.borderFaint}`, gap: 6, alignItems: 'center' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
              {typeIcon(act.type)}
              <span style={{ fontSize: 8, fontWeight: 'bold', color: typeColor(act.type), fontFamily: C.font }}>{act.type}</span>
            </div>
            <div style={{ overflow: 'hidden' }}>
              <div style={{ fontSize: 9, color: C.white, fontFamily: C.font, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>{label}</div>
              {txShort && (
                <div style={{ display: 'flex', alignItems: 'center', gap: 3, marginTop: 1 }}>
                  <span style={{ fontSize: 8, color: C.faint, fontFamily: C.font }}>{txShort}</span>
                  {act.transactionHash && (
                    <a href={`https://polygonscan.com/tx/${act.transactionHash}`} target="_blank" rel="noopener noreferrer"
                      style={{ display: 'flex', alignItems: 'center', color: C.faint, textDecoration: 'none' }}>
                      <ExtLink size={8} />
                    </a>
                  )}
                </div>
              )}
            </div>
            <span style={{ fontSize: 10, fontWeight: 'bold', color: act.usdcSize > 0 ? typeColor(act.type) : C.muted, textAlign: 'right', fontFamily: C.font }}>
              {act.usdcSize > 0 ? fmtVol(act.usdcSize) : '—'}
            </span>
            <span style={{ fontSize: 10, color: C.white, textAlign: 'right', fontFamily: C.font }}>
              {act.size > 0 ? act.size.toFixed(2) : '—'}
            </span>
            <span style={{ fontSize: 8, color: C.faint, textAlign: 'right', fontFamily: C.font }}>
              {new Date(ms).toLocaleDateString()}<br />
              <span style={{ fontSize: 7 }}>{new Date(ms).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}</span>
            </span>
          </div>
        );
      })}
    </>
  );
};

// ── Live feed tab ─────────────────────────────────────────────────────────────
interface LiveFeedTabProps {
  liveOrders: WSUserOrderEvent[];
  liveTrades: WSUserTradeEvent[];
  wsConnected: boolean;
  credentialsReady: boolean;
  onClear: () => void;
}

const LiveFeedTab: React.FC<LiveFeedTabProps> = ({ liveOrders, liveTrades, wsConnected, credentialsReady, onClear }) => {
  if (!credentialsReady) return (
    <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}>
      <WifiOff size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
      <div style={{ fontSize: 10, fontFamily: C.font, marginBottom: 6 }}>API CREDENTIALS REQUIRED</div>
      <div style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>Configure credentials in Settings → Polymarket to enable live order/trade streaming</div>
    </div>
  );

  type FeedItem = { kind: 'order'; item: WSUserOrderEvent } | { kind: 'trade'; item: WSUserTradeEvent };
  const feed: FeedItem[] = [
    ...liveOrders.map(o => ({ kind: 'order' as const, item: o })),
    ...liveTrades.map(t => ({ kind: 'trade' as const, item: t })),
  ].sort((a, b) => Number(b.item.timestamp) - Number(a.item.timestamp));

  if (feed.length === 0) return (
    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: 200, flexDirection: 'column', gap: 10 }}>
      {wsConnected
        ? <><Wifi size={16} style={{ color: C.green }} /><span style={{ fontSize: 10, color: C.green, fontFamily: C.font }}>CONNECTED — WAITING FOR EVENTS</span></>
        : <><WifiOff size={16} style={{ color: C.faint }} /><span style={{ fontSize: 10, color: C.faint, fontFamily: C.font }}>CONNECTING...</span></>}
    </div>
  );

  return (
    <>
      {sectionHeader(`LIVE FEED (${feed.length})`, (
        <button onClick={onClear}
          style={{ fontSize: 8, color: C.faint, background: 'transparent', border: `1px solid ${C.border}`, cursor: 'pointer', padding: '1px 6px', fontFamily: C.font, borderRadius: 1 }}>
          CLEAR
        </button>
      ))}
      <div style={{ display: 'grid', gridTemplateColumns: '50px 50px 3fr 60px 70px 70px 80px', padding: '3px 10px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, gap: 6 }}>
        {['TYPE', 'SIDE', 'MARKET', 'OUTCOME', 'SIZE', 'PRICE', 'STATUS'].map((h, i) => (
          <span key={h} style={{ fontSize: 8, color: C.orange, fontWeight: 'bold', fontFamily: C.font, textAlign: i < 3 ? 'left' : 'right' }}>{h}</span>
        ))}
      </div>
      {feed.slice(0, 100).map((entry, idx) => {
        if (entry.kind === 'order') {
          const o = entry.item;
          const typeColor = o.type === 'PLACEMENT' ? C.cyan : o.type === 'CANCELLATION' ? C.red : C.muted;
          return (
            <div key={`o-${o.id}-${idx}`} style={{ display: 'grid', gridTemplateColumns: '50px 50px 3fr 60px 70px 70px 80px', padding: '5px 10px', borderBottom: `1px solid ${C.borderFaint}`, gap: 6, alignItems: 'center' }}>
              <span style={{ fontSize: 8, color: typeColor, fontWeight: 'bold', fontFamily: C.font }}>ORDER</span>
              <span style={{ fontSize: 8, fontWeight: 'bold', fontFamily: C.font, color: o.side === 'BUY' ? C.green : C.red }}>{o.side}</span>
              <span style={{ fontSize: 9, color: C.white, fontFamily: C.font, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>{o.market.slice(0, 12)}...</span>
              <span style={{ fontSize: 9, color: o.outcome === 'Yes' ? C.green : C.red, textAlign: 'right', fontFamily: C.font, fontWeight: 'bold' }}>{o.outcome?.toUpperCase()}</span>
              <span style={{ fontSize: 10, color: C.white, textAlign: 'right', fontFamily: C.font }}>{parseFloat(o.original_size).toFixed(0)}</span>
              <span style={{ fontSize: 10, color: C.white, fontWeight: 'bold', textAlign: 'right', fontFamily: C.font }}>${parseFloat(o.price).toFixed(4)}</span>
              <span style={{ fontSize: 8, color: typeColor, textAlign: 'right', fontFamily: C.font }}>{o.type}</span>
            </div>
          );
        }
        const t = entry.item;
        const statusColor = t.status === 'CONFIRMED' ? C.green : t.status === 'FAILED' ? C.red : C.orange;
        return (
          <div key={`t-${t.id}-${idx}`} style={{ display: 'grid', gridTemplateColumns: '50px 50px 3fr 60px 70px 70px 80px', padding: '5px 10px', borderBottom: `1px solid ${C.borderFaint}`, gap: 6, alignItems: 'center' }}>
            <span style={{ fontSize: 8, color: C.orange, fontWeight: 'bold', fontFamily: C.font }}>TRADE</span>
            <span style={{ fontSize: 8, fontWeight: 'bold', fontFamily: C.font, color: t.side === 'BUY' ? C.green : C.red }}>{t.side}</span>
            <span style={{ fontSize: 9, color: C.white, fontFamily: C.font, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>{t.market.slice(0, 12)}...</span>
            <span style={{ fontSize: 9, color: t.outcome === 'Yes' ? C.green : C.red, textAlign: 'right', fontFamily: C.font, fontWeight: 'bold' }}>{t.outcome?.toUpperCase()}</span>
            <span style={{ fontSize: 10, color: C.white, textAlign: 'right', fontFamily: C.font }}>{parseFloat(t.size).toFixed(2)}</span>
            <span style={{ fontSize: 10, color: C.white, fontWeight: 'bold', textAlign: 'right', fontFamily: C.font }}>${parseFloat(t.price).toFixed(4)}</span>
            <span style={{ fontSize: 8, color: statusColor, textAlign: 'right', fontFamily: C.font }}>{t.status}</span>
          </div>
        );
      })}
    </>
  );
};

export default PortfolioPanel;
