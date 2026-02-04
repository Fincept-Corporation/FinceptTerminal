/**
 * AlgoTradingPanel - Floating/collapsible panel for deploying Fincept strategies
 * Shared component for both Equity Trading and Crypto Trading tabs.
 *
 * Features:
 *  - Browse 400+ strategies from _registry.py (card/list view toggle)
 *  - Deploy to paper or live trading (uses current broker mode)
 *  - Track deployed strategies with live P&L, status, trades
 *  - SQLite persistence via Tauri commands
 *  - User-defined risk params (stop-loss, take-profit, position size, max loss)
 *  - "Stop All" emergency button
 */

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  Play, Square, ChevronDown, ChevronUp, Search, LayoutGrid, List,
  AlertTriangle, Zap, Activity, TrendingUp, DollarSign, XCircle,
  Settings, RefreshCw, Pause, RotateCcw,
} from 'lucide-react';

// ─── Colors (matches Fincept palette) ──────────────────────────────────────
const C = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', CYAN: '#00E5FF', YELLOW: '#FFD700',
  BLUE: '#0088FF', PURPLE: '#9D4EDD', BORDER: '#2A2A2A',
  HOVER: '#1F1F1F', MUTED: '#4A4A4A',
};

// ─── Types ─────────────────────────────────────────────────────────────────
interface RegistryStrategy {
  id: string;
  name: string;
  category: string;
  path: string;
}

type StrategyStatus = 'running' | 'stopped' | 'paused' | 'error' | 'waiting';

interface DeployedStrategy {
  deployId: string;
  strategyId: string;
  strategyName: string;
  symbol: string;
  mode: 'paper' | 'live';
  status: StrategyStatus;
  timeframe: string;
  quantity: number;
  stopLoss: number | null;
  takeProfit: number | null;
  maxDailyLoss: number | null;
  pnl: number;
  trades: number;
  winRate: number;
  deployedAt: string;
  error?: string;
}

interface AlgoTradingPanelProps {
  /** 'equity' or 'crypto' – determines which broker adapter path is used */
  assetType: 'equity' | 'crypto';
  /** Current symbol from the parent trading tab */
  symbol: string;
  /** Current market price */
  currentPrice: number;
  /** Active broker id */
  activeBroker: string | null;
  /** Whether user is in paper mode */
  isPaper: boolean;
}

type ViewMode = 'cards' | 'list';

// ─── Component ─────────────────────────────────────────────────────────────
export function AlgoTradingPanel({
  assetType,
  symbol,
  currentPrice,
  activeBroker,
  isPaper,
}: AlgoTradingPanelProps) {
  // Panel state
  const [isCollapsed, setIsCollapsed] = useState(false);
  const [viewMode, setViewMode] = useState<ViewMode>('cards');

  // Strategy browser
  const [strategies, setStrategies] = useState<RegistryStrategy[]>([]);
  const [filteredStrategies, setFilteredStrategies] = useState<RegistryStrategy[]>([]);
  const [searchQuery, setSearchQuery] = useState('');
  const [categoryFilter, setCategoryFilter] = useState('all');
  const [categories, setCategories] = useState<string[]>([]);
  const [isLoadingStrategies, setIsLoadingStrategies] = useState(false);

  // Deploy form
  const [selectedStrategy, setSelectedStrategy] = useState<RegistryStrategy | null>(null);
  const [deploySymbol, setDeploySymbol] = useState(symbol);
  const [deployTimeframe, setDeployTimeframe] = useState('1d');
  const [deployQuantity, setDeployQuantity] = useState(1);
  const [deployStopLoss, setDeployStopLoss] = useState<number | null>(null);
  const [deployTakeProfit, setDeployTakeProfit] = useState<number | null>(null);
  const [deployMaxDailyLoss, setDeployMaxDailyLoss] = useState<number | null>(null);
  const [isDeploying, setIsDeploying] = useState(false);
  const [deployError, setDeployError] = useState<string | null>(null);

  // Active deployments
  const [deployed, setDeployed] = useState<DeployedStrategy[]>([]);
  const [showDeployed, setShowDeployed] = useState(true);
  const pollRef = useRef<ReturnType<typeof setInterval> | null>(null);

  // Sync symbol from parent
  useEffect(() => { setDeploySymbol(symbol); }, [symbol]);

  // ─── Load strategies from _registry.py ───────────────────────────────
  useEffect(() => {
    setIsLoadingStrategies(true);
    invoke<string>('list_strategies')
      .then((res) => {
        const parsed = JSON.parse(res);
        if (parsed.success && parsed.data) {
          const list: RegistryStrategy[] = parsed.data;
          setStrategies(list);
          setFilteredStrategies(list);
          const cats = [...new Set(list.map((s) => s.category))].sort();
          setCategories(cats);
        }
      })
      .catch((err) => console.error('Failed to load strategies:', err))
      .finally(() => setIsLoadingStrategies(false));
  }, []);

  // ─── Filter strategies ───────────────────────────────────────────────
  useEffect(() => {
    let filtered = strategies;
    if (categoryFilter !== 'all') {
      filtered = filtered.filter((s) => s.category === categoryFilter);
    }
    if (searchQuery.trim()) {
      const q = searchQuery.toLowerCase();
      filtered = filtered.filter(
        (s) => s.name.toLowerCase().includes(q) || s.id.toLowerCase().includes(q)
      );
    }
    setFilteredStrategies(filtered);
  }, [strategies, categoryFilter, searchQuery]);

  // ─── Load deployed strategies from backend ───────────────────────────
  const loadDeployed = useCallback(async () => {
    try {
      const res = await invoke<string>('list_deployed_strategies', { assetType });
      const parsed = JSON.parse(res);
      if (parsed.success && parsed.data) {
        // Map snake_case DB fields to camelCase UI fields
        const mapped: DeployedStrategy[] = parsed.data.map((d: Record<string, unknown>) => ({
          deployId: d.deploy_id as string,
          strategyId: d.strategy_id as string,
          strategyName: d.strategy_name as string,
          symbol: d.symbol as string,
          mode: d.mode as 'paper' | 'live',
          status: d.status as StrategyStatus,
          timeframe: (() => { try { const p = JSON.parse((d.params as string) || '{}'); return p.timeframe || '1d'; } catch { return '1d'; } })(),
          quantity: (() => { try { const p = JSON.parse((d.params as string) || '{}'); return p.quantity || 1; } catch { return 1; } })(),
          stopLoss: (() => { try { const p = JSON.parse((d.params as string) || '{}'); return p.stop_loss || null; } catch { return null; } })(),
          takeProfit: (() => { try { const p = JSON.parse((d.params as string) || '{}'); return p.take_profit || null; } catch { return null; } })(),
          maxDailyLoss: (() => { try { const p = JSON.parse((d.params as string) || '{}'); return p.max_daily_loss || null; } catch { return null; } })(),
          pnl: (d.total_pnl as number) || 0,
          trades: (d.total_trades as number) || 0,
          winRate: (d.win_rate as number) || 0,
          deployedAt: d.created_at as string,
          error: (() => { try { const p = JSON.parse((d.params as string) || '{}'); return p._error || undefined; } catch { return undefined; } })(),
        }));
        setDeployed(mapped);
      }
    } catch {
      // Backend command may not exist yet - silent
    }
  }, [assetType]);

  useEffect(() => { loadDeployed(); }, [loadDeployed]);

  // Poll deployed strategies for live updates
  useEffect(() => {
    pollRef.current = setInterval(loadDeployed, 5000);
    return () => { if (pollRef.current) clearInterval(pollRef.current); };
  }, [loadDeployed]);

  // Stop all strategies when component unmounts (tab close)
  useEffect(() => {
    return () => {
      invoke<string>('stop_all_strategies', { assetType }).catch(() => {});
    };
  }, [assetType]);

  // ─── Deploy ──────────────────────────────────────────────────────────
  const handleDeploy = async () => {
    if (!selectedStrategy || !deploySymbol || !activeBroker) return;

    // Check duplicate (local check first, backend also validates)
    const dup = deployed.find(
      (d) => d.strategyId === selectedStrategy.id && d.symbol === deploySymbol && d.status !== 'stopped'
    );
    if (dup) {
      setDeployError(`Strategy ${selectedStrategy.name} is already deployed on ${deploySymbol}`);
      return;
    }

    setIsDeploying(true);
    setDeployError(null);
    try {
      const params = JSON.stringify({
        strategy_id: selectedStrategy.id,
        strategy_name: selectedStrategy.name,
        symbol: deploySymbol,
        asset_type: assetType,
        broker_id: activeBroker,
        mode: isPaper ? 'paper' : 'live',
        timeframe: deployTimeframe,
        quantity: deployQuantity,
        stop_loss: deployStopLoss,
        take_profit: deployTakeProfit,
        max_daily_loss: deployMaxDailyLoss,
      });
      const res = await invoke<string>('deploy_strategy', { params });
      const parsed = JSON.parse(res);
      if (!parsed.success) {
        setDeployError(parsed.error || 'Deploy failed');
      } else {
        await loadDeployed();
        setSelectedStrategy(null);
        setDeployError(null);
      }
    } catch (err: any) {
      console.error('Deploy failed:', err);
      setDeployError(`Deploy failed: ${err}`);
    } finally {
      setIsDeploying(false);
    }
  };

  // ─── Stop / Pause / Resume ───────────────────────────────────────────
  const handleStop = async (deployId: string) => {
    try {
      await invoke<string>('stop_strategy', { deployId });
      await loadDeployed();
    } catch (err) { console.error('Stop failed:', err); }
  };

  const handleStopAll = async () => {
    try {
      await invoke<string>('stop_all_strategies', { assetType });
      await loadDeployed();
    } catch (err) { console.error('Stop all failed:', err); }
  };

  // ─── Render helpers ──────────────────────────────────────────────────
  const statusColor = (s: StrategyStatus) => {
    switch (s) {
      case 'running': return C.GREEN;
      case 'waiting': return C.YELLOW;
      case 'paused': return C.BLUE;
      case 'stopped': return C.GRAY;
      case 'error': return C.RED;
    }
  };

  const activeCount = deployed.filter((d) => d.status === 'running' || d.status === 'waiting').length;

  // ─── JSX ─────────────────────────────────────────────────────────────
  return (
    <div style={{
      display: 'flex', flexDirection: 'column', height: '100%',
      backgroundColor: C.PANEL_BG, fontFamily: '"IBM Plex Mono", monospace',
      overflow: 'hidden',
    }}>
      {/* ── Header ──────────────────────────────────────────────────── */}
      <div style={{
        padding: '8px 12px', backgroundColor: C.HEADER_BG,
        borderBottom: `1px solid ${C.BORDER}`,
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Zap size={14} color={C.ORANGE} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: C.ORANGE, letterSpacing: '0.5px' }}>
            ALGO TRADING
          </span>
          {activeCount > 0 && (
            <span style={{
              padding: '1px 6px', backgroundColor: `${C.GREEN}20`, color: C.GREEN,
              fontSize: '9px', fontWeight: 700, border: `1px solid ${C.GREEN}40`,
            }}>
              {activeCount} ACTIVE
            </span>
          )}
          <span style={{
            padding: '1px 6px', fontSize: '8px', fontWeight: 600,
            backgroundColor: isPaper ? `${C.YELLOW}20` : `${C.GREEN}20`,
            color: isPaper ? C.YELLOW : C.GREEN,
            border: `1px solid ${isPaper ? C.YELLOW : C.GREEN}40`,
          }}>
            {isPaper ? 'PAPER' : 'LIVE'}
          </span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          {activeCount > 0 && (
            <button onClick={handleStopAll} style={{
              padding: '3px 8px', backgroundColor: `${C.RED}20`, border: `1px solid ${C.RED}40`,
              color: C.RED, fontSize: '8px', fontWeight: 700, cursor: 'pointer',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              <Square size={9} /> STOP ALL
            </button>
          )}
          <button onClick={() => setIsCollapsed(!isCollapsed)} style={{
            padding: '3px 8px', backgroundColor: 'transparent', border: `1px solid ${C.BORDER}`,
            color: C.GRAY, cursor: 'pointer', fontSize: '9px', fontWeight: 600,
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            {isCollapsed ? <ChevronUp size={12} /> : <ChevronDown size={12} />}
            {isCollapsed ? 'EXPAND' : 'COLLAPSE'}
          </button>
        </div>
      </div>

      {!isCollapsed && (
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
          {/* ── Left: Strategy Browser ────────────────────────────── */}
          <div style={{
            width: '320px', minWidth: '280px', borderRight: `1px solid ${C.BORDER}`,
            display: 'flex', flexDirection: 'column', overflow: 'hidden',
          }}>
            {/* Search & Filters */}
            <div style={{ padding: '8px', borderBottom: `1px solid ${C.BORDER}` }}>
              <div style={{ display: 'flex', gap: '4px', marginBottom: '6px' }}>
                <div style={{
                  flex: 1, display: 'flex', alignItems: 'center', gap: '4px',
                  padding: '4px 8px', backgroundColor: C.DARK_BG, border: `1px solid ${C.BORDER}`,
                }}>
                  <Search size={11} color={C.GRAY} />
                  <input
                    value={searchQuery}
                    onChange={(e) => setSearchQuery(e.target.value)}
                    placeholder="Search strategies..."
                    style={{
                      flex: 1, background: 'none', border: 'none', color: C.WHITE,
                      fontSize: '10px', outline: 'none', fontFamily: '"IBM Plex Mono", monospace',
                    }}
                  />
                </div>
                <button onClick={() => setViewMode(viewMode === 'cards' ? 'list' : 'cards')} style={{
                  padding: '4px 6px', backgroundColor: 'transparent', border: `1px solid ${C.BORDER}`,
                  color: C.GRAY, cursor: 'pointer',
                }}>
                  {viewMode === 'cards' ? <List size={12} /> : <LayoutGrid size={12} />}
                </button>
              </div>
              <div style={{ display: 'flex', gap: '3px', flexWrap: 'wrap' }}>
                <button
                  onClick={() => setCategoryFilter('all')}
                  style={{
                    padding: '2px 6px', fontSize: '8px', fontWeight: 600, cursor: 'pointer',
                    backgroundColor: categoryFilter === 'all' ? C.ORANGE : 'transparent',
                    color: categoryFilter === 'all' ? C.DARK_BG : C.GRAY,
                    border: `1px solid ${categoryFilter === 'all' ? C.ORANGE : C.BORDER}`,
                  }}
                >ALL ({strategies.length})</button>
                {categories.slice(0, 8).map((cat) => {
                  const count = strategies.filter((s) => s.category === cat).length;
                  return (
                    <button
                      key={cat}
                      onClick={() => setCategoryFilter(cat)}
                      style={{
                        padding: '2px 6px', fontSize: '8px', fontWeight: 600, cursor: 'pointer',
                        backgroundColor: categoryFilter === cat ? C.ORANGE : 'transparent',
                        color: categoryFilter === cat ? C.DARK_BG : C.GRAY,
                        border: `1px solid ${categoryFilter === cat ? C.ORANGE : C.BORDER}`,
                      }}
                    >{cat.toUpperCase()} ({count})</button>
                  );
                })}
              </div>
            </div>

            {/* Strategy List */}
            <div style={{ flex: 1, overflow: 'auto', padding: '4px' }}>
              {isLoadingStrategies ? (
                <div style={{ padding: '20px', textAlign: 'center', color: C.GRAY, fontSize: '10px' }}>
                  <RefreshCw size={16} style={{ animation: 'spin 1s linear infinite', marginBottom: '8px' }} />
                  <div>Loading strategies...</div>
                </div>
              ) : filteredStrategies.length === 0 ? (
                <div style={{ padding: '20px', textAlign: 'center', color: C.GRAY, fontSize: '10px' }}>
                  No strategies found
                </div>
              ) : viewMode === 'cards' ? (
                <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '4px' }}>
                  {filteredStrategies.map((s) => (
                    <div
                      key={s.id}
                      onClick={() => setSelectedStrategy(s)}
                      style={{
                        padding: '8px', cursor: 'pointer',
                        backgroundColor: selectedStrategy?.id === s.id ? `${C.ORANGE}15` : C.DARK_BG,
                        border: `1px solid ${selectedStrategy?.id === s.id ? C.ORANGE : C.BORDER}`,
                        transition: 'all 0.15s',
                      }}
                      onMouseEnter={(e) => { if (selectedStrategy?.id !== s.id) e.currentTarget.style.borderColor = C.MUTED; }}
                      onMouseLeave={(e) => { if (selectedStrategy?.id !== s.id) e.currentTarget.style.borderColor = C.BORDER; }}
                    >
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                        <span style={{ fontSize: '10px', fontWeight: 600, color: C.WHITE }}>{s.name}</span>
                      </div>
                      <div style={{ display: 'flex', gap: '6px', marginTop: '4px' }}>
                        <span style={{
                          padding: '1px 4px', fontSize: '7px', fontWeight: 600,
                          backgroundColor: `${C.BLUE}20`, color: C.BLUE, border: `1px solid ${C.BLUE}30`,
                        }}>{s.category}</span>
                        <span style={{ fontSize: '8px', color: C.MUTED }}>{s.id}</span>
                      </div>
                    </div>
                  ))}
                </div>
              ) : (
                /* List view */
                <table style={{ width: '100%', fontSize: '9px', borderCollapse: 'collapse' }}>
                  <thead>
                    <tr style={{ borderBottom: `1px solid ${C.BORDER}` }}>
                      <th style={{ padding: '4px 6px', textAlign: 'left', color: C.GRAY, fontWeight: 600 }}>NAME</th>
                      <th style={{ padding: '4px 6px', textAlign: 'left', color: C.GRAY, fontWeight: 600 }}>CATEGORY</th>
                    </tr>
                  </thead>
                  <tbody>
                    {filteredStrategies.map((s) => (
                      <tr
                        key={s.id}
                        onClick={() => setSelectedStrategy(s)}
                        style={{
                          cursor: 'pointer', borderBottom: `1px solid ${C.BORDER}30`,
                          backgroundColor: selectedStrategy?.id === s.id ? `${C.ORANGE}15` : 'transparent',
                        }}
                        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = C.HOVER}
                        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = selectedStrategy?.id === s.id ? `${C.ORANGE}15` : 'transparent'}
                      >
                        <td style={{ padding: '4px 6px', color: C.WHITE }}>{s.name}</td>
                        <td style={{ padding: '4px 6px', color: C.BLUE }}>{s.category}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
            </div>
          </div>

          {/* ── Center: Deploy Config ─────────────────────────────── */}
          <div style={{
            flex: 1, display: 'flex', flexDirection: 'column', overflow: 'auto',
            padding: '12px', gap: '10px',
          }}>
            {selectedStrategy ? (
              <>
                <div style={{
                  padding: '10px', backgroundColor: C.HEADER_BG, border: `1px solid ${C.BORDER}`,
                }}>
                  <div style={{ fontSize: '12px', fontWeight: 700, color: C.ORANGE, marginBottom: '4px' }}>
                    {selectedStrategy.name}
                  </div>
                  <div style={{ display: 'flex', gap: '8px' }}>
                    <span style={{ fontSize: '9px', color: C.BLUE }}>{selectedStrategy.category}</span>
                    <span style={{ fontSize: '9px', color: C.MUTED }}>{selectedStrategy.id}</span>
                  </div>
                </div>

                {/* Deploy Form */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                  {/* Symbol */}
                  <div>
                    <label style={{ fontSize: '8px', color: C.GRAY, fontWeight: 600, letterSpacing: '0.5px' }}>SYMBOL</label>
                    <input value={deploySymbol} onChange={(e) => setDeploySymbol(e.target.value.toUpperCase())} style={{
                      width: '100%', padding: '6px 8px', backgroundColor: C.DARK_BG,
                      border: `1px solid ${C.BORDER}`, color: C.WHITE, fontSize: '11px',
                      fontFamily: '"IBM Plex Mono", monospace', outline: 'none', marginTop: '2px',
                    }} />
                  </div>
                  {/* Timeframe */}
                  <div>
                    <label style={{ fontSize: '8px', color: C.GRAY, fontWeight: 600, letterSpacing: '0.5px' }}>TIMEFRAME</label>
                    <select value={deployTimeframe} onChange={(e) => setDeployTimeframe(e.target.value)} style={{
                      width: '100%', padding: '6px 8px', backgroundColor: C.DARK_BG,
                      border: `1px solid ${C.BORDER}`, color: C.WHITE, fontSize: '11px',
                      fontFamily: '"IBM Plex Mono", monospace', outline: 'none', marginTop: '2px',
                    }}>
                      <option value="1m">1 Minute</option>
                      <option value="5m">5 Minutes</option>
                      <option value="15m">15 Minutes</option>
                      <option value="1h">1 Hour</option>
                      <option value="4h">4 Hours</option>
                      <option value="1d">Daily</option>
                      <option value="1w">Weekly</option>
                    </select>
                  </div>
                  {/* Quantity */}
                  <div>
                    <label style={{ fontSize: '8px', color: C.GRAY, fontWeight: 600, letterSpacing: '0.5px' }}>QUANTITY</label>
                    <input type="text" inputMode="decimal" value={deployQuantity} onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setDeployQuantity(Number(v) || 0); }} style={{
                      width: '100%', padding: '6px 8px', backgroundColor: C.DARK_BG,
                      border: `1px solid ${C.BORDER}`, color: C.WHITE, fontSize: '11px',
                      fontFamily: '"IBM Plex Mono", monospace', outline: 'none', marginTop: '2px',
                    }} />
                  </div>
                  {/* Mode indicator */}
                  <div>
                    <label style={{ fontSize: '8px', color: C.GRAY, fontWeight: 600, letterSpacing: '0.5px' }}>MODE</label>
                    <div style={{
                      padding: '6px 8px', marginTop: '2px',
                      backgroundColor: isPaper ? `${C.YELLOW}15` : `${C.GREEN}15`,
                      border: `1px solid ${isPaper ? C.YELLOW : C.GREEN}40`,
                      color: isPaper ? C.YELLOW : C.GREEN,
                      fontSize: '11px', fontWeight: 700,
                    }}>
                      {isPaper ? 'PAPER TRADING' : 'LIVE TRADING'}
                    </div>
                  </div>
                </div>

                {/* Risk Management */}
                <div style={{
                  padding: '8px', backgroundColor: `${C.RED}08`, border: `1px solid ${C.BORDER}`,
                }}>
                  <div style={{ fontSize: '9px', fontWeight: 700, color: C.RED, marginBottom: '6px', display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <AlertTriangle size={11} /> RISK MANAGEMENT
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '6px' }}>
                    <div>
                      <label style={{ fontSize: '7px', color: C.GRAY }}>STOP LOSS (%)</label>
                      <input type="text" inputMode="decimal" value={deployStopLoss ?? ''} onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setDeployStopLoss(v ? Number(v) : null); }}
                        placeholder="Optional" style={{
                          width: '100%', padding: '4px 6px', backgroundColor: C.DARK_BG,
                          border: `1px solid ${C.BORDER}`, color: C.WHITE, fontSize: '10px',
                          fontFamily: '"IBM Plex Mono", monospace', outline: 'none', marginTop: '2px',
                        }} />
                    </div>
                    <div>
                      <label style={{ fontSize: '7px', color: C.GRAY }}>TAKE PROFIT (%)</label>
                      <input type="text" inputMode="decimal" value={deployTakeProfit ?? ''} onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setDeployTakeProfit(v ? Number(v) : null); }}
                        placeholder="Optional" style={{
                          width: '100%', padding: '4px 6px', backgroundColor: C.DARK_BG,
                          border: `1px solid ${C.BORDER}`, color: C.WHITE, fontSize: '10px',
                          fontFamily: '"IBM Plex Mono", monospace', outline: 'none', marginTop: '2px',
                        }} />
                    </div>
                    <div>
                      <label style={{ fontSize: '7px', color: C.GRAY }}>MAX DAILY LOSS</label>
                      <input type="text" inputMode="decimal" value={deployMaxDailyLoss ?? ''} onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setDeployMaxDailyLoss(v ? Number(v) : null); }}
                        placeholder="Optional" style={{
                          width: '100%', padding: '4px 6px', backgroundColor: C.DARK_BG,
                          border: `1px solid ${C.BORDER}`, color: C.WHITE, fontSize: '10px',
                          fontFamily: '"IBM Plex Mono", monospace', outline: 'none', marginTop: '2px',
                        }} />
                    </div>
                  </div>
                </div>

                {/* Error Display */}
                {deployError && (
                  <div style={{
                    padding: '8px', backgroundColor: `${C.RED}15`, border: `1px solid ${C.RED}40`,
                    color: C.RED, fontSize: '10px', display: 'flex', alignItems: 'center', gap: '6px',
                  }}>
                    <AlertTriangle size={12} />
                    <span>{deployError}</span>
                    <button onClick={() => setDeployError(null)} style={{
                      marginLeft: 'auto', background: 'none', border: 'none', color: C.RED, cursor: 'pointer',
                    }}>
                      <XCircle size={12} />
                    </button>
                  </div>
                )}

                {/* Deploy Button */}
                <button onClick={handleDeploy} disabled={isDeploying || !activeBroker} style={{
                  padding: '10px', border: 'none', cursor: isDeploying || !activeBroker ? 'not-allowed' : 'pointer',
                  backgroundColor: isDeploying || !activeBroker ? C.MUTED : C.ORANGE,
                  color: C.DARK_BG, fontSize: '12px', fontWeight: 700, letterSpacing: '1px',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px',
                  opacity: isDeploying || !activeBroker ? 0.5 : 1,
                  fontFamily: '"IBM Plex Mono", monospace',
                }}>
                  <Play size={14} />
                  {isDeploying ? 'DEPLOYING...' : !activeBroker ? 'CONNECT BROKER FIRST' : 'DEPLOY STRATEGY'}
                </button>
              </>
            ) : (
              <div style={{
                flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center',
                flexDirection: 'column', color: C.GRAY,
              }}>
                <Zap size={28} color={C.BORDER} style={{ marginBottom: '12px' }} />
                <div style={{ fontSize: '11px', fontWeight: 600, marginBottom: '4px' }}>SELECT A STRATEGY</div>
                <div style={{ fontSize: '9px' }}>Choose from {strategies.length} strategies on the left</div>
              </div>
            )}
          </div>

          {/* ── Right: Active Deployments ─────────────────────────── */}
          <div style={{
            width: '300px', minWidth: '260px', borderLeft: `1px solid ${C.BORDER}`,
            display: 'flex', flexDirection: 'column', overflow: 'hidden',
          }}>
            <div style={{
              padding: '8px 10px', backgroundColor: C.HEADER_BG,
              borderBottom: `1px solid ${C.BORDER}`,
              display: 'flex', justifyContent: 'space-between', alignItems: 'center',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: C.WHITE, letterSpacing: '0.5px' }}>
                DEPLOYED ({deployed.length})
              </span>
              <button onClick={loadDeployed} style={{
                padding: '2px 6px', backgroundColor: 'transparent', border: `1px solid ${C.BORDER}`,
                color: C.GRAY, cursor: 'pointer',
              }}>
                <RefreshCw size={10} />
              </button>
            </div>

            <div style={{ flex: 1, overflow: 'auto', padding: '4px' }}>
              {deployed.length === 0 ? (
                <div style={{ padding: '20px', textAlign: 'center', color: C.GRAY, fontSize: '10px' }}>
                  <Activity size={20} color={C.BORDER} style={{ marginBottom: '8px' }} />
                  <div>No active strategies</div>
                </div>
              ) : (
                deployed.map((d) => (
                  <div key={d.deployId} style={{
                    padding: '8px', marginBottom: '4px', backgroundColor: C.DARK_BG,
                    border: `1px solid ${d.status === 'error' ? C.RED : C.BORDER}`,
                  }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                      <span style={{ fontSize: '10px', fontWeight: 600, color: C.WHITE }}>{d.strategyName}</span>
                      <span style={{
                        padding: '1px 5px', fontSize: '7px', fontWeight: 700,
                        backgroundColor: `${statusColor(d.status)}20`,
                        color: statusColor(d.status),
                        border: `1px solid ${statusColor(d.status)}40`,
                      }}>
                        {d.status.toUpperCase()}
                      </span>
                    </div>

                    <div style={{ display: 'flex', gap: '6px', marginBottom: '4px', fontSize: '9px' }}>
                      <span style={{ color: C.CYAN }}>{d.symbol}</span>
                      <span style={{ color: C.MUTED }}>{d.timeframe}</span>
                      <span style={{
                        color: d.mode === 'paper' ? C.YELLOW : C.GREEN,
                        fontWeight: 600,
                      }}>{d.mode.toUpperCase()}</span>
                    </div>

                    <div style={{ display: 'flex', gap: '8px', fontSize: '9px', marginBottom: '6px' }}>
                      <span style={{ color: d.pnl >= 0 ? C.GREEN : C.RED }}>
                        P&L: {d.pnl >= 0 ? '+' : ''}{d.pnl.toFixed(2)}
                      </span>
                      <span style={{ color: C.GRAY }}>Trades: {d.trades}</span>
                      <span style={{ color: C.GRAY }}>WR: {d.winRate.toFixed(0)}%</span>
                    </div>

                    {d.error && (
                      <div style={{ fontSize: '8px', color: C.RED, marginBottom: '4px' }}>
                        {d.error}
                      </div>
                    )}

                    <div style={{ display: 'flex', gap: '4px' }}>
                      {(d.status === 'running' || d.status === 'waiting') && (
                        <button onClick={() => handleStop(d.deployId)} style={{
                          flex: 1, padding: '4px', border: `1px solid ${C.RED}40`,
                          backgroundColor: `${C.RED}15`, color: C.RED,
                          fontSize: '8px', fontWeight: 700, cursor: 'pointer',
                          display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '3px',
                        }}>
                          <Square size={9} /> STOP
                        </button>
                      )}
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

export default AlgoTradingPanel;
