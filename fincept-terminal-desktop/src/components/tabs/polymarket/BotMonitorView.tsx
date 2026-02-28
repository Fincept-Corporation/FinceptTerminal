/**
 * BotMonitorView - Single-screen no-scroll bot dashboard
 * Layout: sidebar (bot list) | detail (stats bar + tab content fills remaining height)
 * All overflow is handled internally — nothing escapes the container.
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  Bot, Play, Pause, Square, Trash2, RefreshCw,
  CheckCircle, XCircle, AlertTriangle, Activity,
  TrendingUp, TrendingDown, ChevronDown, ChevronUp, ChevronRight,
  Zap, Shield,
} from 'lucide-react';
import {
  AreaChart, Area, XAxis, YAxis, Tooltip, ResponsiveContainer, ReferenceLine,
} from 'recharts';
import {
  polymarketBotService,
  PolymarketBot, BotDecision, BotPosition,
} from '@/services/polymarket/polymarketBotService';

// ─── Constants ────────────────────────────────────────────────────────────────

const C = {
  orange: '#FF8800', green: '#00D66F', red: '#FF3B3B',
  blue: '#0088FF', purple: '#9D4EDD', cyan: '#00E5FF', yellow: '#FFD700',
  bg: '#000000', panel: '#0A0A0A', header: '#060606', border: '#1A1A1A',
  muted: '#555555', text: '#CCCCCC', dim: '#2A2A2A', white: '#FFFFFF',
};

const STATUS_COLORS: Record<string, string> = {
  running: C.green, idle: C.muted, paused: C.cyan, stopped: C.muted, error: C.red,
};

const ACTION_COLORS: Record<string, string> = {
  buy_yes: C.green, buy_no: C.red, sell_yes: C.orange, sell_no: C.purple, hold: C.muted, skip: C.muted,
};

// ─── Helpers ──────────────────────────────────────────────────────────────────

function fmtPnl(val: number): React.ReactNode {
  const color = val >= 0 ? C.green : C.red;
  return <span style={{ color }}>{val >= 0 ? '+' : ''}{val.toFixed(2)}</span>;
}

function fmtPct(val: number): React.ReactNode {
  const color = val >= 0 ? C.green : C.red;
  return <span style={{ color }}>{val >= 0 ? '+' : ''}{val.toFixed(1)}%</span>;
}

function relTime(iso: string | null): string {
  if (!iso) return '—';
  const s = Math.floor((Date.now() - new Date(iso).getTime()) / 1000);
  if (s < 60) return `${s}s`;
  if (s < 3600) return `${Math.floor(s / 60)}m`;
  if (s < 86400) return `${Math.floor(s / 3600)}h`;
  return `${Math.floor(s / 86400)}d`;
}

function xBtn(color: string): React.CSSProperties {
  return {
    padding: '2px 6px', backgroundColor: `${color}18`, color,
    border: `1px solid ${color}35`, cursor: 'pointer',
    fontFamily: 'inherit', fontSize: '8px',
    display: 'inline-flex', alignItems: 'center', gap: '3px',
  };
}

// ─── P&L Sparkline ────────────────────────────────────────────────────────────

function buildPnlSeries(bot: PolymarketBot) {
  const closed = bot.positions
    .filter((p) => p.closedAt && p.realizedPnlUsdc != null)
    .sort((a, b) => new Date(a.closedAt!).getTime() - new Date(b.closedAt!).getTime());
  if (closed.length === 0) return [{ cycle: 0, pnl: 0 }, { cycle: 1, pnl: 0 }];
  let cum = 0;
  return [{ cycle: 0, pnl: 0 }, ...closed.map((p, i) => {
    cum += p.realizedPnlUsdc ?? 0;
    return { cycle: i + 1, pnl: parseFloat(cum.toFixed(3)) };
  })];
}

const MiniPnlChart: React.FC<{ bot: PolymarketBot }> = ({ bot }) => {
  const data = buildPnlSeries(bot);
  const last = data[data.length - 1]?.pnl ?? 0;
  const color = last >= 0 ? C.green : C.red;
  const min = Math.min(...data.map((d) => d.pnl));
  const max = Math.max(...data.map((d) => d.pnl));

  if (bot.positions.filter((p) => p.closedAt).length === 0) {
    return (
      <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
        <span style={{ fontSize: '9px', color: C.muted }}>No closed positions yet</span>
      </div>
    );
  }

  return (
    <ResponsiveContainer width="100%" height="100%">
      <AreaChart data={data} margin={{ top: 4, right: 4, left: 0, bottom: 0 }}>
        <defs>
          <linearGradient id="pnlG" x1="0" y1="0" x2="0" y2="1">
            <stop offset="5%"  stopColor={color} stopOpacity={0.25} />
            <stop offset="95%" stopColor={color} stopOpacity={0.02} />
          </linearGradient>
        </defs>
        <XAxis dataKey="cycle" hide />
        <YAxis domain={[min - 0.5, max + 0.5]} hide />
        <ReferenceLine y={0} stroke={C.dim} strokeDasharray="3 3" />
        <Tooltip
          contentStyle={{ backgroundColor: '#111', border: `1px solid ${C.border}`, fontSize: '9px', padding: '3px 6px' }}
          formatter={(v: number) => [`${v >= 0 ? '+' : ''}${v.toFixed(3)} USDC`, 'P&L']}
          labelFormatter={(l) => `#${l}`}
        />
        <Area type="monotone" dataKey="pnl" stroke={color} strokeWidth={1.5} fill="url(#pnlG)" dot={false} />
      </AreaChart>
    </ResponsiveContainer>
  );
};

// ─── Risk Bars ────────────────────────────────────────────────────────────────

const RiskBars: React.FC<{ bot: PolymarketBot }> = ({ bot }) => {
  const open = bot.positions.filter((p) => !p.closedAt);
  const exposure = open.reduce((s, p) => s + p.sizeUsdc, 0);
  const maxExp = bot.riskConfig.maxExposureUsdc;
  const expPct = Math.min((exposure / maxExp) * 100, 100);
  const dailyLoss = Math.abs(Math.min(bot.stats.dailyPnlUsdc, 0));
  const maxLoss = bot.riskConfig.maxDailyLossUsdc;
  const lossPct = Math.min((dailyLoss / maxLoss) * 100, 100);
  const expColor = expPct > 80 ? C.red : expPct > 50 ? C.yellow : C.green;
  const lossColor = lossPct > 80 ? C.red : lossPct > 50 ? C.yellow : C.green;

  return (
    <div style={{ padding: '8px 10px', height: '100%', display: 'flex', flexDirection: 'column', gap: '8px', justifyContent: 'center' }}>
      <div>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px' }}>
          <span style={{ fontSize: '7px', color: C.muted, fontWeight: 'bold' }}>CAPITAL AT RISK</span>
          <span style={{ fontSize: '8px', color: expColor, fontWeight: 'bold' }}>{exposure.toFixed(1)}/{maxExp} USDC</span>
        </div>
        <div style={{ height: '5px', backgroundColor: C.dim }}>
          <div style={{ height: '100%', width: `${expPct}%`, backgroundColor: expColor, transition: 'width 0.3s' }} />
        </div>
      </div>
      <div>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '3px' }}>
          <span style={{ fontSize: '7px', color: C.muted, fontWeight: 'bold' }}>DAILY LOSS LIMIT</span>
          <span style={{ fontSize: '8px', color: lossColor, fontWeight: 'bold' }}>{dailyLoss.toFixed(1)}/{maxLoss} USDC</span>
        </div>
        <div style={{ height: '5px', backgroundColor: C.dim }}>
          <div style={{ height: '100%', width: `${lossPct}%`, backgroundColor: lossColor, transition: 'width 0.3s' }} />
        </div>
      </div>
    </div>
  );
};

// ─── Decision Log ─────────────────────────────────────────────────────────────

const COL_DEC = '14px minmax(0,1fr) 76px 42px 48px 96px 60px';

const DecisionLog: React.FC<{
  decisions: BotDecision[];
  onApprove: (id: string) => void;
  onReject: (id: string) => void;
}> = ({ decisions, onApprove, onReject }) => {
  const [expanded, setExpanded] = useState<string | null>(null);

  if (decisions.length === 0) {
    return (
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: C.muted, fontSize: '10px' }}>
        No decisions yet — bot will analyse on next cycle
      </div>
    );
  }

  const sorted = [...decisions].sort((a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime());

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Header */}
      <div style={{
        display: 'grid', gridTemplateColumns: COL_DEC,
        padding: '4px 8px', backgroundColor: C.header,
        fontSize: '7px', fontWeight: 'bold', color: C.muted, letterSpacing: '0.06em',
        borderBottom: `1px solid ${C.border}`, flexShrink: 0,
      }}>
        <div /><div>MARKET</div><div>ACTION</div><div>CONF</div><div>PRICE</div><div>STATUS</div><div>AGO</div>
      </div>

      {/* Rows — scrollable */}
      <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
        {sorted.map((d) => {
          const isExp = expanded === d.id;
          const ac = ACTION_COLORS[d.action] ?? C.muted;
          const isPending = d.pendingApproval;

          return (
            <div key={d.id} style={{ borderBottom: `1px solid ${C.border}`, backgroundColor: isPending ? '#0A0600' : 'transparent' }}>
              <div
                style={{ display: 'grid', gridTemplateColumns: COL_DEC, padding: '5px 8px', alignItems: 'center', cursor: 'pointer', fontSize: '9px' }}
                onClick={() => setExpanded(isExp ? null : d.id)}
              >
                <div style={{ color: C.dim }}>{isExp ? <ChevronUp size={9} /> : <ChevronDown size={9} />}</div>
                <div style={{ color: C.text, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', paddingRight: '6px' }} title={d.marketQuestion}>
                  {d.marketQuestion.length > 48 ? d.marketQuestion.slice(0, 48) + '…' : d.marketQuestion}
                </div>
                <div style={{ color: ac, fontWeight: 'bold', fontSize: '8px', display: 'flex', alignItems: 'center', gap: '2px' }}>
                  {(d.action.startsWith('buy')) && <TrendingUp size={8} />}
                  {(d.action.startsWith('sell')) && <TrendingDown size={8} />}
                  {d.action.replace('_', ' ').toUpperCase()}
                </div>
                <div style={{ color: d.confidence >= 80 ? C.green : d.confidence >= 65 ? C.orange : C.muted }}>{d.confidence}%</div>
                <div style={{ color: C.text }}>{d.priceAtDecision.toFixed(3)}</div>
                <div>
                  {isPending ? (
                    <span style={{ color: C.orange, fontSize: '7px', fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: '2px' }}>
                      <AlertTriangle size={8} /> PENDING
                    </span>
                  ) : d.approved === true ? (
                    <span style={{ color: C.green, fontSize: '7px', display: 'flex', alignItems: 'center', gap: '2px' }}><CheckCircle size={8} /> APPROVED</span>
                  ) : d.approved === false ? (
                    <span style={{ color: C.red, fontSize: '7px', display: 'flex', alignItems: 'center', gap: '2px' }}><XCircle size={8} /> REJECTED</span>
                  ) : d.executed ? (
                    <span style={{ color: C.green, fontSize: '7px', display: 'flex', alignItems: 'center', gap: '2px' }}><CheckCircle size={8} /> EXECUTED</span>
                  ) : (
                    <span style={{ color: C.muted, fontSize: '7px' }}>{d.action === 'skip' ? 'BLOCKED' : 'LOGGED'}</span>
                  )}
                </div>
                <div style={{ color: C.muted, fontSize: '8px' }}>{relTime(d.timestamp)}</div>
              </div>

              {isExp && (
                <div style={{ padding: '8px 12px 10px', backgroundColor: '#060606', borderTop: `1px solid ${C.dim}` }}>
                  <div style={{ fontSize: '8px', color: C.text, lineHeight: 1.5, backgroundColor: C.panel, padding: '6px 8px', marginBottom: '6px', border: `1px solid ${C.dim}` }}>
                    <span style={{ color: C.muted, fontWeight: 'bold' }}>REASONING: </span>{d.reasoning}
                  </div>
                  {d.riskFactors.length > 0 && (
                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '3px', marginBottom: '6px' }}>
                      {d.riskFactors.map((r, i) => (
                        <span key={i} style={{ fontSize: '7px', padding: '1px 5px', backgroundColor: `${C.red}10`, color: C.red, border: `1px solid ${C.red}25` }}>⚠ {r}</span>
                      ))}
                    </div>
                  )}
                  <div style={{ display: 'flex', gap: '12px', fontSize: '8px', color: C.muted, marginBottom: isPending ? '8px' : '0' }}>
                    <span>PROB: <strong style={{ color: C.white }}>{(d.assessedProbability * 100).toFixed(1)}%</strong></span>
                    <span>SIZE: <strong style={{ color: C.white }}>{d.recommendedSizeUsdc.toFixed(2)} USDC</strong></span>
                    <span>MKT: <strong style={{ color: C.cyan }}>{d.marketId.slice(0, 10)}…</strong></span>
                  </div>
                  {isPending && (
                    <div style={{ display: 'flex', gap: '6px', marginTop: '8px' }}>
                      <button onClick={(e) => { e.stopPropagation(); onApprove(d.id); }}
                        style={{ padding: '5px 14px', backgroundColor: C.green, color: '#000', border: 'none', fontSize: '8px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'inherit', display: 'flex', alignItems: 'center', gap: '4px' }}>
                        <CheckCircle size={10} /> APPROVE
                      </button>
                      <button onClick={(e) => { e.stopPropagation(); onReject(d.id); }}
                        style={{ padding: '5px 14px', backgroundColor: '#150000', color: C.red, border: `1px solid ${C.red}50`, fontSize: '8px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'inherit', display: 'flex', alignItems: 'center', gap: '4px' }}>
                        <XCircle size={10} /> REJECT
                      </button>
                    </div>
                  )}
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
};

// ─── Positions Table ───────────────────────────────────────────────────────────

const COL_POS = 'minmax(0,1fr) 40px 50px 50px 62px 52px 80px';

const PositionsTable: React.FC<{ positions: BotPosition[] }> = ({ positions }) => {
  const open   = positions.filter((p) => !p.closedAt);
  const closed = positions.filter((p) => p.closedAt);

  if (positions.length === 0) {
    return (
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: C.muted, fontSize: '10px' }}>
        No positions yet
      </div>
    );
  }

  const Header = () => (
    <div style={{
      display: 'grid', gridTemplateColumns: COL_POS, padding: '4px 8px',
      backgroundColor: C.header, fontSize: '7px', fontWeight: 'bold', color: C.muted,
      letterSpacing: '0.06em', borderBottom: `1px solid ${C.border}`, flexShrink: 0,
    }}>
      <div>MARKET</div><div>SIDE</div><div>ENTRY</div><div>CUR</div><div>UNRL PNL</div><div>SIZE</div><div>STATUS</div>
    </div>
  );

  const Row = ({ p }: { p: BotPosition }) => (
    <div style={{
      display: 'grid', gridTemplateColumns: COL_POS, padding: '5px 8px',
      borderBottom: `1px solid ${C.border}`, fontSize: '9px', alignItems: 'center',
      backgroundColor: p.closedAt ? '#050505' : 'transparent',
    }}>
      <div style={{ color: C.text, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', paddingRight: '4px' }} title={p.marketQuestion}>
        {p.marketQuestion.length > 46 ? p.marketQuestion.slice(0, 46) + '…' : p.marketQuestion}
      </div>
      <div style={{ color: p.outcome === 'YES' ? C.green : C.red, fontWeight: 'bold', fontSize: '8px' }}>{p.outcome}</div>
      <div style={{ color: C.text }}>{p.entryPrice.toFixed(3)}</div>
      <div style={{ color: C.text }}>{p.currentPrice.toFixed(3)}</div>
      <div>{fmtPct(p.unrealizedPnlPct)}</div>
      <div style={{ color: C.text }}>{p.sizeUsdc.toFixed(2)}</div>
      <div>
        {p.closedAt ? (
          <span style={{ fontSize: '7px', color: p.realizedPnlUsdc != null && p.realizedPnlUsdc > 0 ? C.green : C.red }}>
            {p.closeReason?.toUpperCase().replace('_', ' ') ?? 'CLOSED'}
            {p.realizedPnlUsdc != null && <> ({p.realizedPnlUsdc >= 0 ? '+' : ''}{p.realizedPnlUsdc.toFixed(2)})</>}
          </span>
        ) : (
          <span style={{ fontSize: '7px', color: C.green, display: 'flex', alignItems: 'center', gap: '2px' }}><Activity size={8} /> OPEN</span>
        )}
      </div>
    </div>
  );

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      <Header />
      <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
        {open.map((p) => <Row key={p.id} p={p} />)}
        {closed.length > 0 && (
          <>
            <div style={{ padding: '4px 8px', fontSize: '7px', color: C.muted, fontWeight: 'bold', backgroundColor: '#070707', borderTop: `1px solid ${C.border}`, letterSpacing: '0.06em' }}>
              CLOSED ({closed.length}) — REALIZED:{' '}
              {(() => {
                const total = closed.reduce((s, p) => s + (p.realizedPnlUsdc ?? 0), 0);
                return <span style={{ color: total >= 0 ? C.green : C.red }}>{total >= 0 ? '+' : ''}{total.toFixed(2)} USDC</span>;
              })()}
            </div>
            {closed.slice(0, 20).map((p) => <Row key={p.id} p={p} />)}
          </>
        )}
      </div>
    </div>
  );
};

// ─── Overview Panel ───────────────────────────────────────────────────────────

const OverviewPanel: React.FC<{ bot: PolymarketBot }> = ({ bot }) => (
  <div style={{ height: '100%', display: 'grid', gridTemplateColumns: '1fr 1fr', gridTemplateRows: '1fr 1fr', gap: 0, overflow: 'hidden' }}>
    {/* P&L Chart */}
    <div style={{ borderRight: `1px solid ${C.border}`, borderBottom: `1px solid ${C.border}`, overflow: 'hidden' }}>
      <div style={{ padding: '5px 10px', borderBottom: `1px solid ${C.border}`, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <span style={{ fontSize: '8px', fontWeight: 'bold', color: C.muted, letterSpacing: '0.06em' }}>CUMULATIVE P&L</span>
        <span style={{ fontSize: '11px', fontWeight: 'bold' }}>{fmtPnl(bot.stats.totalPnlUsdc)} USDC</span>
      </div>
      <div style={{ height: 'calc(100% - 28px)' }}>
        <MiniPnlChart bot={bot} />
      </div>
    </div>

    {/* Risk */}
    <div style={{ borderBottom: `1px solid ${C.border}`, overflow: 'hidden' }}>
      <div style={{ padding: '5px 10px', borderBottom: `1px solid ${C.border}`, display: 'flex', alignItems: 'center', gap: '6px' }}>
        <Shield size={10} style={{ color: C.orange }} />
        <span style={{ fontSize: '8px', fontWeight: 'bold', color: C.muted, letterSpacing: '0.06em' }}>RISK EXPOSURE</span>
      </div>
      <RiskBars bot={bot} />
    </div>

    {/* Bot Config — spans bottom row */}
    <div style={{ gridColumn: '1 / -1', overflow: 'hidden' }}>
      <div style={{ padding: '5px 10px', borderBottom: `1px solid ${C.border}` }}>
        <span style={{ fontSize: '8px', fontWeight: 'bold', color: C.muted, letterSpacing: '0.06em' }}>BOT CONFIGURATION</span>
      </div>
      <div style={{ padding: '8px 10px', display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '4px' }}>
        {[
          ['STRATEGY',    bot.strategy.type.toUpperCase()],
          ['MODEL',       `${bot.llmConfig.provider}/${bot.llmConfig.modelId.slice(0, 12)}`],
          ['INTERVAL',    `${bot.strategy.scanIntervalMs / 60000}m`],
          ['MAX POS',     bot.strategy.maxOpenPositions],
          ['MIN CONF',    `${bot.strategy.minConfidence}%`],
          ['STOP LOSS',   `-${bot.riskConfig.stopLossPct}%`],
          ['TAKE PROFIT', `+${bot.riskConfig.takeProfitPct}%`],
          ['APPROVAL',    bot.riskConfig.requireApproval ? 'REQUIRED' : 'AUTO'],
          ['MEMORY',      bot.llmConfig.enableMemory ? 'ON' : 'OFF'],
          ['GUARDRAILS',  bot.llmConfig.enableGuardrails ? 'ON' : 'OFF'],
        ].map(([label, val]) => (
          <div key={label as string} style={{ padding: '4px 6px', backgroundColor: C.header, border: `1px solid ${C.dim}`, display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '4px', overflow: 'hidden' }}>
            <span style={{ fontSize: '7px', color: C.muted, fontWeight: 'bold', letterSpacing: '0.04em', flexShrink: 0 }}>{label}</span>
            <span style={{ fontSize: '8px', color: C.text, fontWeight: 'bold', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{val}</span>
          </div>
        ))}
      </div>
    </div>
  </div>
);

// ─── Bot List Item ────────────────────────────────────────────────────────────

const BotListItem: React.FC<{
  bot: PolymarketBot; selected: boolean;
  onSelect: () => void; onStart: () => void; onPause: () => void;
  onStop: () => void; onDelete: () => void;
}> = ({ bot, selected, onSelect, onStart, onPause, onStop, onDelete }) => {
  const sc = STATUS_COLORS[bot.status] ?? C.muted;
  return (
    <div onClick={onSelect} style={{
      padding: '8px 10px', cursor: 'pointer', userSelect: 'none',
      backgroundColor: selected ? '#0C0700' : 'transparent',
      borderLeft: `2px solid ${selected ? C.orange : 'transparent'}`,
      borderBottom: `1px solid ${C.border}`,
    }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', minWidth: 0 }}>
          <Bot size={11} style={{ color: sc, flexShrink: 0 }} />
          <span style={{ fontSize: '10px', fontWeight: 'bold', color: C.white, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            {bot.name}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px', flexShrink: 0 }}>
          <span style={{ fontSize: '7px', fontWeight: 'bold', padding: '1px 5px', backgroundColor: `${sc}18`, color: sc, border: `1px solid ${sc}35` }}>
            {bot.status.toUpperCase()}
          </span>
          <ChevronRight size={10} style={{ color: selected ? C.orange : C.dim }} />
        </div>
      </div>

      <div style={{ display: 'flex', gap: '10px', fontSize: '8px', color: C.muted, marginBottom: '5px' }}>
        <span>P&L: <strong style={{ color: bot.stats.totalPnlUsdc >= 0 ? C.green : C.red }}>{bot.stats.totalPnlUsdc >= 0 ? '+' : ''}{bot.stats.totalPnlUsdc.toFixed(2)}</strong></span>
        <span>WIN: <strong style={{ color: C.text }}>{bot.stats.winRate.toFixed(0)}%</strong></span>
        <span style={{ marginLeft: 'auto', color: C.dim }}>{relTime(bot.stats.lastCycleAt)} ago</span>
      </div>

      <div style={{ display: 'flex', gap: '3px' }} onClick={(e) => e.stopPropagation()}>
        {bot.status !== 'running' && (
          <button onClick={onStart} style={xBtn(C.green)}><Play size={8} /> START</button>
        )}
        {bot.status === 'running' && (
          <button onClick={onPause} style={xBtn(C.cyan)}><Pause size={8} /> PAUSE</button>
        )}
        {bot.status !== 'stopped' && bot.status !== 'idle' && (
          <button onClick={onStop} style={xBtn(C.muted)}><Square size={8} /> STOP</button>
        )}
        <button onClick={onDelete} style={{ ...xBtn(C.red), marginLeft: 'auto' }}><Trash2 size={8} /></button>
      </div>

      {bot.lastError && (
        <div style={{ marginTop: '4px', fontSize: '7px', color: C.red, lineHeight: 1.4 }}>⚠ {bot.lastError.slice(0, 80)}</div>
      )}
    </div>
  );
};

// ─── Main Component ───────────────────────────────────────────────────────────

interface Props {
  onCreateNew: () => void;
}

const BotMonitorView: React.FC<Props> = ({ onCreateNew }) => {
  const [bots, setBots] = useState<PolymarketBot[]>([]);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [decisions, setDecisions] = useState<BotDecision[]>([]);
  const [activeTab, setActiveTab] = useState<'overview' | 'decisions' | 'positions'>('overview');

  const loadBots = useCallback(async () => {
    const list = await polymarketBotService.getBots();
    setBots(list);
    if (!selectedId && list.length > 0) setSelectedId(list[0].id);
  }, [selectedId]);

  useEffect(() => {
    loadBots();
    const unsub = polymarketBotService.subscribe(() => loadBots());
    return unsub;
  }, []);

  useEffect(() => {
    if (!selectedId) { setDecisions([]); return; }
    polymarketBotService.getDecisions(selectedId).then(setDecisions);
  }, [selectedId, bots]);

  const bot = bots.find((b) => b.id === selectedId) ?? null;
  const pendingCount = decisions.filter((d) => d.pendingApproval).length;

  const approve = async (id: string) => {
    if (!selectedId) return;
    await polymarketBotService.approveDecision(selectedId, id);
    setDecisions(await polymarketBotService.getDecisions(selectedId));
  };
  const reject = async (id: string) => {
    if (!selectedId) return;
    await polymarketBotService.rejectDecision(selectedId, id);
    setDecisions(await polymarketBotService.getDecisions(selectedId));
  };

  // ── Empty state ──────────────────────────────────────────────────────────────
  if (bots.length === 0) {
    return (
      <div style={{
        height: '100%', display: 'flex', flexDirection: 'column',
        alignItems: 'center', justifyContent: 'center', gap: '14px',
        fontFamily: '"IBM Plex Mono","Consolas",monospace', backgroundColor: C.bg,
      }}>
        <Bot size={40} style={{ color: C.muted, opacity: 0.25 }} />
        <div style={{ fontSize: '11px', color: C.muted, letterSpacing: '0.08em' }}>NO BOTS DEPLOYED</div>
        <div style={{ fontSize: '9px', color: C.dim, textAlign: 'center', maxWidth: '240px' }}>
          Deploy your first autonomous trading agent to start analysing Polymarket prediction markets.
        </div>
        <button onClick={onCreateNew} style={{
          padding: '9px 24px', backgroundColor: C.orange, color: '#000',
          border: 'none', fontSize: '10px', fontWeight: 'bold', cursor: 'pointer',
          fontFamily: 'inherit', letterSpacing: '0.06em',
        }}>
          DEPLOY FIRST BOT
        </button>
      </div>
    );
  }

  const TABS = [
    { id: 'overview'  as const, label: 'OVERVIEW',   badge: null },
    { id: 'decisions' as const, label: 'DECISIONS',  badge: pendingCount > 0 ? pendingCount : null },
    { id: 'positions' as const, label: 'POSITIONS',  badge: null },
  ];

  return (
    <div style={{
      height: '100%', display: 'flex', overflow: 'hidden',
      fontFamily: '"IBM Plex Mono","Consolas",monospace', backgroundColor: C.bg,
    }}>
      {/* ── Bot List Sidebar ──────────────────────────────────────────────────── */}
      <div style={{ width: '220px', flexShrink: 0, borderRight: `1px solid ${C.border}`, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        <div style={{ padding: '7px 10px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexShrink: 0 }}>
          <span style={{ fontSize: '8px', fontWeight: 'bold', color: C.muted, letterSpacing: '0.08em' }}>BOTS ({bots.length})</span>
          <div style={{ display: 'flex', gap: '4px' }}>
            <button onClick={() => loadBots()} style={xBtn(C.muted)} title="Refresh"><RefreshCw size={8} /></button>
            <button onClick={onCreateNew} style={{ ...xBtn(C.orange), fontWeight: 'bold' }}>+ NEW</button>
          </div>
        </div>
        <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
          {bots.map((b) => (
            <BotListItem
              key={b.id} bot={b} selected={b.id === selectedId}
              onSelect={() => setSelectedId(b.id)}
              onStart={() => polymarketBotService.startBot(b.id)}
              onPause={() => polymarketBotService.pauseBot(b.id)}
              onStop={() => polymarketBotService.stopBot(b.id)}
              onDelete={async () => { await polymarketBotService.deleteBot(b.id); if (selectedId === b.id) setSelectedId(null); }}
            />
          ))}
        </div>
      </div>

      {/* ── Detail Panel ─────────────────────────────────────────────────────── */}
      {bot ? (
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>

          {/* Pending approval banner — compact inline strip */}
          {pendingCount > 0 && (
            <div style={{
              flexShrink: 0, backgroundColor: '#0D0700',
              borderBottom: `1px solid ${C.orange}40`,
              borderLeft: `3px solid ${C.orange}`,
              padding: '6px 12px', display: 'flex', alignItems: 'center', gap: '10px',
              flexWrap: 'wrap',
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '5px', flexShrink: 0 }}>
                <Zap size={11} style={{ color: C.orange }} />
                <span style={{ fontSize: '9px', fontWeight: 'bold', color: C.orange, letterSpacing: '0.05em' }}>
                  {pendingCount} TRADE{pendingCount > 1 ? 'S' : ''} PENDING APPROVAL
                </span>
              </div>
              {decisions.filter((d) => d.pendingApproval).slice(0, 2).map((d) => (
                <div key={d.id} style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '8px' }}>
                  <span style={{ color: C.text, maxWidth: '200px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }} title={d.marketQuestion}>
                    {d.marketQuestion.length > 30 ? d.marketQuestion.slice(0, 30) + '…' : d.marketQuestion}
                  </span>
                  <span style={{ color: ACTION_COLORS[d.action] ?? C.muted, fontWeight: 'bold' }}>{d.action.replace('_', ' ').toUpperCase()}</span>
                  <button onClick={() => approve(d.id)} style={{ ...xBtn(C.green), fontWeight: 'bold' }}><CheckCircle size={8} /> OK</button>
                  <button onClick={() => reject(d.id)} style={xBtn(C.red)}><XCircle size={8} /> NO</button>
                </div>
              ))}
              {pendingCount > 2 && <span style={{ fontSize: '8px', color: C.muted }}>+{pendingCount - 2} more in Decisions tab</span>}
            </div>
          )}

          {/* Stats bar */}
          <div style={{
            flexShrink: 0, display: 'grid',
            gridTemplateColumns: 'repeat(5, minmax(0,1fr)) auto',
            borderBottom: `1px solid ${C.border}`,
            backgroundColor: C.header, overflow: 'hidden',
          }}>
            {[
              { label: 'TOTAL P&L',    value: fmtPnl(bot.stats.totalPnlUsdc),                                 sub: `Daily: ${bot.stats.dailyPnlUsdc >= 0 ? '+' : ''}${bot.stats.dailyPnlUsdc.toFixed(2)}` },
              { label: 'WIN RATE',     value: `${bot.stats.winRate.toFixed(0)}%`,                              sub: `${bot.stats.tradesWon}W / ${bot.stats.tradesLost}L` },
              { label: 'OPEN POS',     value: bot.positions.filter((p) => !p.closedAt).length,                sub: `Max: ${bot.strategy.maxOpenPositions}` },
              { label: 'CYCLES',       value: bot.stats.totalCycles,                                          sub: relTime(bot.stats.lastCycleAt) + ' ago' },
              { label: 'DECISIONS',    value: bot.stats.totalDecisions,                                       sub: pendingCount > 0 ? <span style={{ color: C.orange }}>⏳ {pendingCount} PENDING</span> : 'none' },
            ].map((cell, i) => (
              <div key={cell.label} style={{ padding: '7px 10px', borderRight: `1px solid ${C.border}`, minWidth: 0, overflow: 'hidden' }}>
                <div style={{ fontSize: '7px', color: C.muted, fontWeight: 'bold', letterSpacing: '0.06em', marginBottom: '3px', whiteSpace: 'nowrap' }}>{cell.label}</div>
                <div style={{ fontSize: '12px', fontWeight: 'bold', color: C.white, whiteSpace: 'nowrap' }}>{cell.value}</div>
                <div style={{ fontSize: '8px', color: C.muted, marginTop: '1px', whiteSpace: 'nowrap' }}>{cell.sub}</div>
              </div>
            ))}
            {/* Status badge */}
            <div style={{ padding: '7px 12px', display: 'flex', alignItems: 'center', flexShrink: 0 }}>
              <span style={{
                fontSize: '8px', fontWeight: 'bold', padding: '3px 8px',
                backgroundColor: `${STATUS_COLORS[bot.status]}18`, color: STATUS_COLORS[bot.status],
                border: `1px solid ${STATUS_COLORS[bot.status]}35`, letterSpacing: '0.05em',
              }}>
                {bot.status.toUpperCase()}
              </span>
            </div>
          </div>

          {/* Inner tab nav */}
          <div style={{ flexShrink: 0, display: 'flex', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header }}>
            {TABS.map(({ id, label, badge }) => (
              <button key={id} onClick={() => setActiveTab(id)} style={{
                padding: '7px 16px', border: 'none', cursor: 'pointer', fontFamily: 'inherit',
                backgroundColor: 'transparent',
                color: activeTab === id ? C.orange : C.muted,
                fontSize: '8px', fontWeight: 'bold', letterSpacing: '0.06em',
                borderBottom: activeTab === id ? `2px solid ${C.orange}` : '2px solid transparent',
                display: 'flex', alignItems: 'center', gap: '5px',
              }}>
                {label}
                {badge !== null && (
                  <span style={{ backgroundColor: C.orange, color: '#000', fontSize: '7px', fontWeight: 'bold', padding: '1px 5px', borderRadius: '8px' }}>
                    {badge}
                  </span>
                )}
              </button>
            ))}
          </div>

          {/* Tab content — fills remaining height, no overflow */}
          <div style={{ flex: 1, overflow: 'hidden', minHeight: 0 }}>
            {activeTab === 'overview'  && <OverviewPanel bot={bot} />}
            {activeTab === 'decisions' && <DecisionLog decisions={decisions} onApprove={approve} onReject={reject} />}
            {activeTab === 'positions' && <PositionsTable positions={bot.positions} />}
          </div>
        </div>
      ) : (
        <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', color: C.muted, fontSize: '10px', flexDirection: 'column', gap: '8px' }}>
          <Bot size={28} style={{ opacity: 0.2 }} />
          SELECT A BOT TO VIEW DETAILS
        </div>
      )}
    </div>
  );
};

export default BotMonitorView;
