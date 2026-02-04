/**
 * MonitoringPanel — Compact real-time price alert panel for bottom panels
 * Shared between Equity Trading and Crypto Trading tabs.
 * Connects to existing Rust MonitoringService via Tauri commands.
 */
import React, { useState, useEffect, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import { toast } from '@/components/ui/terminal-toast';
import {
  Bell, Plus, Trash2, RefreshCw, X, CheckCircle, AlertCircle,
  Target, Activity, DollarSign, BarChart3, TrendingUp
} from 'lucide-react';
import { notificationService } from '@/services/notifications';

// ---------- Types (matching Rust MonitorCondition / MonitorAlert) ----------

interface MonitorCondition {
  id?: number;
  provider: string;
  symbol: string;
  field: 'price' | 'volume' | 'change_percent' | 'spread';
  operator: '>' | '<' | '>=' | '<=' | '==' | 'between';
  value: number;
  value2?: number;
  enabled: boolean;
}

interface MonitorAlert {
  id?: number;
  condition_id: number;
  provider: string;
  symbol: string;
  field: string;
  triggered_value: number;
  triggered_at: number;
}

interface MonitoringPanelProps {
  provider: string;
  symbol: string;
  assetType: 'equity' | 'crypto';
}

// ---------- Color palette (matches FINCEPT constants) ----------

const C = {
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
} as const;

const FIELD_OPTIONS: { value: MonitorCondition['field']; label: string }[] = [
  { value: 'price', label: 'PRICE' },
  { value: 'volume', label: 'VOLUME' },
  { value: 'change_percent', label: '% CHG' },
  { value: 'spread', label: 'SPREAD' },
];

const OP_OPTIONS: { value: MonitorCondition['operator']; label: string }[] = [
  { value: '>', label: '>' },
  { value: '<', label: '<' },
  { value: '>=', label: '>=' },
  { value: '<=', label: '<=' },
  { value: '==', label: '==' },
  { value: 'between', label: 'BTW' },
];

const FIELD_ICON: Record<string, React.ReactNode> = {
  price: <DollarSign size={10} />,
  volume: <BarChart3 size={10} />,
  change_percent: <TrendingUp size={10} />,
  spread: <Activity size={10} />,
};

// ---------- Component ----------

export function MonitoringPanel({ provider, symbol, assetType }: MonitoringPanelProps) {
  const [conditions, setConditions] = useState<MonitorCondition[]>([]);
  const [alerts, setAlerts] = useState<MonitorAlert[]>([]);
  const [view, setView] = useState<'conditions' | 'alerts'>('conditions');
  const [showAdd, setShowAdd] = useState(false);
  const [loading, setLoading] = useState(false);

  // Form state — pre-filled from parent props
  const [form, setForm] = useState<MonitorCondition>({
    provider: provider || '',
    symbol: symbol || '',
    field: 'price',
    operator: '>',
    value: 0,
    enabled: true,
  });

  // Raw string states for decimal input (so typing "956." doesn't strip the dot)
  const [valueStr, setValueStr] = useState('');
  const [value2Str, setValue2Str] = useState('');

  // Keep form in sync with parent props
  useEffect(() => {
    setForm(prev => ({
      ...prev,
      provider: provider || prev.provider,
      symbol: symbol || prev.symbol,
    }));
  }, [provider, symbol]);

  // ---------- Data loading ----------

  const loadData = useCallback(async () => {
    setLoading(true);
    try {
      await invoke('monitor_load_conditions');
      const conds = await invoke<MonitorCondition[]>('monitor_get_conditions');
      const alertList = await invoke<MonitorAlert[]>('monitor_get_alerts', { limit: 50 });
      setConditions(conds || []);
      setAlerts(alertList || []);
    } catch (err) {
      console.warn('[MonitoringPanel] load failed:', err);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    loadData();
  }, [loadData]);

  // ---------- Alert listener + notification ----------

  useEffect(() => {
    const unlisten = listen<MonitorAlert>('monitor_alert', (event) => {
      const alert = event.payload;
      setAlerts(prev => [alert, ...prev].slice(0, 100));

      // Remove the triggered condition from the conditions list (it's already deleted in DB by Rust)
      setConditions(prev => prev.filter(c => c.id !== alert.condition_id));

      const triggerTime = new Date(alert.triggered_at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
      toast.warning(`ALERT: ${alert.symbol} — ${alert.field.toUpperCase()} = ${alert.triggered_value.toFixed(2)} at ${triggerTime}`);

      // Fire external notification (Telegram, etc.)
      console.log('[MonitoringPanel] Sending notification for alert:', alert.symbol, alert.triggered_value);
      notificationService.notify({
        type: 'warning',
        title: `Price Alert: ${alert.symbol}`,
        body: `${alert.field.toUpperCase()} = ${alert.triggered_value.toFixed(2)} on ${alert.provider} at ${triggerTime}`,
        timestamp: new Date(alert.triggered_at).toISOString(),
        metadata: { symbol: alert.symbol, provider: alert.provider, field: alert.field, time: triggerTime },
      }).catch(err => console.warn('[MonitoringPanel] Notification error:', err));
    });

    return () => { unlisten.then(fn => fn()); };
  }, []);

  // ---------- Actions ----------

  const addCondition = async () => {
    if (!form.symbol || !form.provider) return;
    try {
      await invoke('monitor_add_condition', { condition: form });
      // Optimistically add to local state so it appears immediately
      setConditions(prev => [...prev, { ...form, id: Date.now() }]);
      toast.success('Condition added');
      setShowAdd(false);
      setForm(prev => ({ ...prev, value: 0, value2: undefined, operator: '>', field: 'price' }));
      setValueStr('');
      setValue2Str('');
      // Also reload from backend to get the real ID
      loadData().catch(() => {});
    } catch (err) {
      toast.error('Failed to add condition');
      console.error(err);
    }
  };

  const deleteCondition = async (id: number) => {
    try {
      await invoke('monitor_delete_condition', { id });
      setConditions(prev => prev.filter(c => c.id !== id));
      toast.info('Condition deleted');
    } catch (err) {
      toast.error('Failed to delete condition');
      console.error(err);
    }
  };

  // ---------- Styles ----------

  const btnBase: React.CSSProperties = {
    padding: '4px 10px',
    border: `1px solid ${C.BORDER}`,
    backgroundColor: 'transparent',
    color: C.GRAY,
    cursor: 'pointer',
    fontSize: '9px',
    fontWeight: 700,
    fontFamily: '"IBM Plex Mono", monospace',
    transition: 'all 0.15s',
  };

  const inputBase: React.CSSProperties = {
    padding: '4px 8px',
    backgroundColor: C.DARK_BG,
    border: `1px solid ${C.BORDER}`,
    color: C.WHITE,
    fontSize: '10px',
    fontFamily: '"IBM Plex Mono", monospace',
    outline: 'none',
  };

  const selectBase: React.CSSProperties = {
    ...inputBase,
    cursor: 'pointer',
  };

  // ---------- Render ----------

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      overflow: 'hidden',
      fontFamily: '"IBM Plex Mono", monospace',
    }}>
      {/* Header row */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '6px 12px',
        borderBottom: `1px solid ${C.BORDER}`,
        backgroundColor: C.HEADER_BG,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
          <button
            onClick={() => setView('conditions')}
            style={{
              ...btnBase,
              backgroundColor: view === 'conditions' ? C.ORANGE : 'transparent',
              color: view === 'conditions' ? C.DARK_BG : C.GRAY,
              borderColor: view === 'conditions' ? C.ORANGE : C.BORDER,
            }}
          >
            <Target size={9} style={{ marginRight: 4, verticalAlign: 'middle' }} />
            CONDITIONS ({conditions.length})
          </button>
          <button
            onClick={() => setView('alerts')}
            style={{
              ...btnBase,
              backgroundColor: view === 'alerts' ? C.YELLOW : 'transparent',
              color: view === 'alerts' ? C.DARK_BG : C.GRAY,
              borderColor: view === 'alerts' ? C.YELLOW : C.BORDER,
            }}
          >
            <Bell size={9} style={{ marginRight: 4, verticalAlign: 'middle' }} />
            ALERTS ({alerts.length})
          </button>
        </div>

        <div style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
          <button
            onClick={() => setShowAdd(!showAdd)}
            style={{
              ...btnBase,
              backgroundColor: showAdd ? C.RED : C.GREEN,
              color: C.DARK_BG,
              borderColor: showAdd ? C.RED : C.GREEN,
            }}
          >
            {showAdd ? <X size={10} /> : <Plus size={10} />}
            <span style={{ marginLeft: 4 }}>{showAdd ? 'CANCEL' : 'ADD'}</span>
          </button>
          <button
            onClick={loadData}
            style={{ ...btnBase, padding: '4px 6px' }}
            title="Refresh"
          >
            <RefreshCw size={10} style={{ animation: loading ? 'spin 1s linear infinite' : 'none' }} />
          </button>
        </div>
      </div>

      {/* Inline Add Form */}
      {showAdd && (
        <div style={{
          display: 'flex',
          gap: '4px',
          padding: '6px 12px',
          borderBottom: `1px solid ${C.BORDER}`,
          backgroundColor: `${C.HEADER_BG}`,
          flexShrink: 0,
          alignItems: 'center',
          flexWrap: 'wrap',
        }}>
          <input
            value={form.provider}
            onChange={e => setForm(prev => ({ ...prev, provider: e.target.value }))}
            placeholder="Provider"
            style={{ ...inputBase, width: '90px' }}
          />
          <input
            value={form.symbol}
            onChange={e => setForm(prev => ({ ...prev, symbol: e.target.value.toUpperCase() }))}
            placeholder="Symbol"
            style={{ ...inputBase, width: '120px' }}
          />
          <select
            value={form.field}
            onChange={e => setForm(prev => ({ ...prev, field: e.target.value as MonitorCondition['field'] }))}
            style={{ ...selectBase, width: '80px' }}
          >
            {FIELD_OPTIONS.map(f => (
              <option key={f.value} value={f.value}>{f.label}</option>
            ))}
          </select>
          <select
            value={form.operator}
            onChange={e => setForm(prev => ({ ...prev, operator: e.target.value as MonitorCondition['operator'] }))}
            style={{ ...selectBase, width: '55px' }}
          >
            {OP_OPTIONS.map(o => (
              <option key={o.value} value={o.value}>{o.label}</option>
            ))}
          </select>
          <input
            type="text"
            inputMode="decimal"
            value={valueStr}
            onChange={e => { const v = e.target.value; setValueStr(v); setForm(prev => ({ ...prev, value: v === '' ? 0 : (parseFloat(v) || 0) })); }}
            placeholder="Value"
            style={{ ...inputBase, width: '80px' }}
          />
          {form.operator === 'between' && (
            <input
              type="text"
              inputMode="decimal"
              value={value2Str}
              onChange={e => { const v = e.target.value; setValue2Str(v); setForm(prev => ({ ...prev, value2: v === '' ? 0 : (parseFloat(v) || 0) })); }}
              placeholder="Value 2"
              style={{ ...inputBase, width: '80px' }}
            />
          )}
          <button
            onClick={addCondition}
            style={{
              ...btnBase,
              backgroundColor: C.GREEN,
              color: C.DARK_BG,
              borderColor: C.GREEN,
              padding: '4px 12px',
            }}
          >
            <CheckCircle size={10} style={{ marginRight: 4, verticalAlign: 'middle' }} />
            ADD
          </button>
        </div>
      )}

      {/* Content area */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px 0' }}>
        {view === 'conditions' ? (
          conditions.length === 0 ? (
            <div style={{
              padding: '30px',
              textAlign: 'center',
              color: C.GRAY,
              fontSize: '10px',
            }}>
              <Target size={20} color={C.BORDER} style={{ marginBottom: 8 }} />
              <div style={{ fontWeight: 600 }}>NO CONDITIONS</div>
              <div style={{ fontSize: '9px', marginTop: 4 }}>
                Click + ADD to create a {assetType} price alert
              </div>
            </div>
          ) : (
            conditions.map(cond => (
              <div
                key={cond.id}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  padding: '6px 12px',
                  borderBottom: `1px solid ${C.BORDER}30`,
                  fontSize: '10px',
                }}
                onMouseEnter={e => (e.currentTarget.style.backgroundColor = C.HOVER)}
                onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
              >
                <span style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  width: 22,
                  height: 22,
                  backgroundColor: `${C.ORANGE}20`,
                  color: C.ORANGE,
                  flexShrink: 0,
                }}>
                  {FIELD_ICON[cond.field] || <Target size={10} />}
                </span>
                <span style={{ color: C.WHITE, fontWeight: 700, minWidth: 80 }}>
                  {cond.symbol}
                </span>
                <span style={{
                  padding: '1px 6px',
                  backgroundColor: `${C.CYAN}15`,
                  color: C.CYAN,
                  fontSize: '8px',
                  fontWeight: 600,
                }}>
                  {cond.provider.toUpperCase()}
                </span>
                <span style={{ color: C.GRAY }}>
                  {cond.field.toUpperCase()} {cond.operator} {cond.value.toLocaleString()}
                  {cond.operator === 'between' && cond.value2 != null && ` — ${cond.value2.toLocaleString()}`}
                </span>
                {!cond.enabled && (
                  <span style={{
                    padding: '1px 6px',
                    backgroundColor: `${C.MUTED}30`,
                    color: C.MUTED,
                    fontSize: '8px',
                    fontWeight: 600,
                  }}>
                    OFF
                  </span>
                )}
                <div style={{ flex: 1 }} />
                <button
                  onClick={() => cond.id != null && deleteCondition(cond.id)}
                  style={{
                    background: 'none',
                    border: 'none',
                    color: C.MUTED,
                    cursor: 'pointer',
                    padding: '2px',
                    display: 'flex',
                    alignItems: 'center',
                  }}
                  onMouseEnter={e => (e.currentTarget.style.color = C.RED)}
                  onMouseLeave={e => (e.currentTarget.style.color = C.MUTED)}
                  title="Delete condition"
                >
                  <Trash2 size={12} />
                </button>
              </div>
            ))
          )
        ) : (
          alerts.length === 0 ? (
            <div style={{
              padding: '30px',
              textAlign: 'center',
              color: C.GRAY,
              fontSize: '10px',
            }}>
              <AlertCircle size={20} color={C.BORDER} style={{ marginBottom: 8 }} />
              <div style={{ fontWeight: 600 }}>NO ALERTS</div>
              <div style={{ fontSize: '9px', marginTop: 4 }}>
                Alerts fire when conditions match live WebSocket data
              </div>
            </div>
          ) : (
            alerts.map((alert, idx) => (
              <div
                key={alert.id ?? idx}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  padding: '6px 12px',
                  borderBottom: `1px solid ${C.BORDER}30`,
                  borderLeft: `3px solid ${C.YELLOW}`,
                  fontSize: '10px',
                }}
                onMouseEnter={e => (e.currentTarget.style.backgroundColor = C.HOVER)}
                onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
              >
                <Bell size={10} color={C.YELLOW} style={{ flexShrink: 0 }} />
                <span style={{ color: C.WHITE, fontWeight: 700, minWidth: 80 }}>
                  {alert.symbol}
                </span>
                <span style={{
                  padding: '1px 6px',
                  backgroundColor: `${C.CYAN}15`,
                  color: C.CYAN,
                  fontSize: '8px',
                  fontWeight: 600,
                }}>
                  {alert.provider.toUpperCase()}
                </span>
                <span style={{ color: C.YELLOW }}>
                  {alert.field.toUpperCase()}: {alert.triggered_value.toLocaleString(undefined, { maximumFractionDigits: 4 })}
                </span>
                <div style={{ flex: 1 }} />
                <span style={{ color: C.MUTED, fontSize: '9px' }}>
                  {new Date(alert.triggered_at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit', fractionalSecondDigits: 1 } as Intl.DateTimeFormatOptions)}
                </span>
              </div>
            ))
          )
        )}
      </div>
    </div>
  );
}

export default MonitoringPanel;
