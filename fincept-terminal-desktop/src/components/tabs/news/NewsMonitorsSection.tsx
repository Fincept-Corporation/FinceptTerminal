/**
 * NewsMonitorsSection.tsx — Monitor CRUD panel
 * Bloomberg Terminal Design — Watchlist-style keyword monitors
 */
import React, { useState } from 'react';
import { Plus, X, Settings, Trash2 } from 'lucide-react';
import { NewsMonitor, MONITOR_COLORS, nextMonitorColor } from '@/services/news/newsMonitorService';

// ── Bloomberg-style design tokens (matching NewsTab.tsx) ─────────────────────
const C = {
  BG: '#000000', SURFACE: '#080808', PANEL: '#0D0D0D',
  RAISED: '#141414', TOOLBAR: '#1A1A1A',
  BORDER: '#1E1E1E', DIVIDER: '#2A2A2A',
  TEXT: '#D4D4D4', TEXT_DIM: '#888888', TEXT_MUTE: '#555555',
  AMBER: '#FF8800', BLUE: '#4DA6FF', CYAN: '#00D4AA',
  GREEN: '#26C281', RED: '#E55A5A', WHITE: '#FFFFFF',
} as const;
const FONT = '"IBM Plex Mono", "SF Mono", "Consolas", monospace';

interface Props {
  monitors: NewsMonitor[];
  matchCounts: Map<string, number>;
  newMatchIds: Set<string>;
  activeMonitorId: string | null;
  onActivate: (id: string | null) => void;
  onAdd: (label: string, keywords: string[]) => void;
  onToggle: (id: string, enabled: boolean) => void;
  onDelete: (id: string) => void;
  onUpdate: (id: string, label: string, keywords: string[], color: string, enabled: boolean) => void;
  colors: Record<string, string>;
}

export const NewsMonitorsSection: React.FC<Props> = ({
  monitors, matchCounts, newMatchIds, activeMonitorId,
  onActivate, onAdd, onToggle, onDelete, onUpdate,
}) => {
  const [showAdd, setShowAdd] = useState(false);
  const [addLabel, setAddLabel] = useState('');
  const [addKw, setAddKw] = useState('');
  const [editMon, setEditMon] = useState<NewsMonitor | null>(null);
  const [editLabel, setEditLabel] = useState('');
  const [editKw, setEditKw] = useState('');
  const [editColor, setEditColor] = useState('');

  const inputStyle: React.CSSProperties = {
    backgroundColor: C.BG, border: `1px solid ${C.BORDER}`,
    color: C.WHITE, borderRadius: '2px', padding: '4px 8px',
    fontSize: '9px', width: '100%', outline: 'none',
    boxSizing: 'border-box', fontFamily: FONT,
  };

  const handleAdd = () => {
    const label = addLabel.trim();
    const kws = addKw.split(',').map(k => k.trim()).filter(Boolean);
    if (!label || kws.length === 0) return;
    onAdd(label, kws);
    setAddLabel(''); setAddKw(''); setShowAdd(false);
  };

  const openEdit = (m: NewsMonitor) => {
    setEditMon(m); setEditLabel(m.label);
    setEditKw(m.keywords.join(', ')); setEditColor(m.color);
  };

  const saveEdit = () => {
    if (!editMon) return;
    const kws = editKw.split(',').map(k => k.trim()).filter(Boolean);
    onUpdate(editMon.id, editLabel.trim(), kws, editColor, editMon.enabled);
    setEditMon(null);
  };

  return (
    <div style={{ fontFamily: FONT }}>
      {/* Header */}
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '4px 10px', backgroundColor: C.TOOLBAR,
        borderBottom: `1px solid ${C.BORDER}`,
      }}>
        <span style={{ fontSize: '8px', fontWeight: 700, color: C.TEXT_MUTE, letterSpacing: '1px' }}>MONITORS</span>
        <button onClick={() => setShowAdd(f => !f)} style={{
          background: showAdd ? `${C.AMBER}20` : 'none',
          border: `1px solid ${showAdd ? C.AMBER : C.BORDER}`,
          color: showAdd ? C.AMBER : C.TEXT_MUTE,
          fontSize: '8px', fontWeight: 700, padding: '2px 6px',
          borderRadius: '2px', cursor: 'pointer', fontFamily: FONT,
          display: 'flex', alignItems: 'center', gap: '3px',
        }}>
          {showAdd ? <X size={8} /> : <Plus size={8} />}
          {showAdd ? 'CLOSE' : 'ADD'}
        </button>
      </div>

      {/* Add form */}
      {showAdd && (
        <div style={{ padding: '6px 10px', borderBottom: `1px solid ${C.BORDER}`, display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <input placeholder="LABEL (e.g. FED RATE)" value={addLabel}
            onChange={e => setAddLabel(e.target.value)} style={inputStyle} />
          <input placeholder="KEYWORDS (comma, separated)" value={addKw}
            onChange={e => setAddKw(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && handleAdd()} style={inputStyle} />
          <button onClick={handleAdd} style={{
            padding: '4px', fontSize: '8px', fontWeight: 700,
            backgroundColor: `${C.AMBER}20`, border: `1px solid ${C.AMBER}50`,
            color: C.AMBER, borderRadius: '2px', cursor: 'pointer', fontFamily: FONT,
          }}>CREATE MONITOR</button>
        </div>
      )}

      {/* Monitor list */}
      <div style={{ padding: '2px 0' }}>
        {monitors.length === 0 && !showAdd && (
          <div style={{ color: C.TEXT_MUTE, fontSize: '8px', textAlign: 'center', padding: '10px 0' }}>
            NO MONITORS CONFIGURED
          </div>
        )}

        {monitors.map(m => {
          const count = matchCounts.get(m.id) ?? 0;
          const hasNew = newMatchIds.has(m.id);
          const isActive = activeMonitorId === m.id;

          return (
            <div key={m.id} style={{
              display: 'flex', alignItems: 'center', gap: '6px',
              padding: '3px 10px',
              backgroundColor: isActive ? `${m.color}10` : 'transparent',
              borderLeft: isActive ? `2px solid ${m.color}` : '2px solid transparent',
              cursor: 'pointer', transition: 'all 0.08s',
            }}>
              {/* Enable dot */}
              <div onClick={() => onToggle(m.id, !m.enabled)}
                title={m.enabled ? 'Disable' : 'Enable'}
                style={{
                  width: '5px', height: '5px', borderRadius: '50%',
                  backgroundColor: m.enabled ? m.color : C.BORDER, flexShrink: 0,
                  boxShadow: m.enabled ? `0 0 3px ${m.color}` : 'none',
                  cursor: 'pointer',
                }} />

              {/* Label */}
              <span onClick={() => onActivate(isActive ? null : m.id)}
                style={{
                  fontSize: '9px', fontWeight: isActive ? 700 : 500,
                  color: isActive ? m.color : '#B0B0B0',
                  flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                }}>{m.label}</span>

              {/* Match count */}
              <span style={{
                fontSize: '8px', fontWeight: count > 0 ? 700 : 400,
                color: count > 0 ? m.color : C.TEXT_MUTE, flexShrink: 0,
              }}>{count}</span>

              {/* New badge */}
              {hasNew && (
                <span style={{
                  fontSize: '6px', fontWeight: 700, padding: '1px 4px',
                  backgroundColor: m.color, color: C.BG, borderRadius: '2px',
                }}>NEW</span>
              )}

              {/* Edit */}
              <button onClick={() => openEdit(m)} style={{
                background: 'none', border: 'none', color: C.TEXT_MUTE,
                cursor: 'pointer', padding: '0 2px', display: 'flex', flexShrink: 0,
              }}><Settings size={9} /></button>
            </div>
          );
        })}
      </div>

      {/* Edit modal */}
      {editMon && (
        <div style={{
          position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.85)',
          display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 9000,
        }} onClick={() => setEditMon(null)}>
          <div onClick={e => e.stopPropagation()} style={{
            backgroundColor: C.PANEL, border: `1px solid ${C.AMBER}`,
            borderRadius: '2px', padding: '14px', width: '280px',
            display: 'flex', flexDirection: 'column', gap: '8px', fontFamily: FONT,
          }}>
            <div style={{ fontSize: '9px', fontWeight: 700, color: C.AMBER, letterSpacing: '0.5px' }}>
              EDIT MONITOR
            </div>
            <input placeholder="Label" value={editLabel} onChange={e => setEditLabel(e.target.value)} style={inputStyle} />
            <input placeholder="Keywords" value={editKw} onChange={e => setEditKw(e.target.value)} style={inputStyle} />
            <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
              {MONITOR_COLORS.map(c => (
                <div key={c} onClick={() => setEditColor(c)} style={{
                  width: '14px', height: '14px', borderRadius: '50%', backgroundColor: c,
                  cursor: 'pointer', border: editColor === c ? `2px solid ${C.WHITE}` : '2px solid transparent',
                  boxShadow: editColor === c ? `0 0 4px ${c}` : 'none',
                }} />
              ))}
            </div>
            <div style={{ display: 'flex', gap: '5px', marginTop: '4px' }}>
              <button onClick={saveEdit} style={{
                flex: 1, padding: '4px', fontSize: '8px', fontWeight: 700,
                backgroundColor: `${C.AMBER}20`, border: `1px solid ${C.AMBER}50`,
                color: C.AMBER, borderRadius: '2px', cursor: 'pointer', fontFamily: FONT,
              }}>SAVE</button>
              <button onClick={() => { onDelete(editMon.id); setEditMon(null); }} style={{
                padding: '4px 10px', fontSize: '8px', fontWeight: 700,
                backgroundColor: `${C.RED}15`, border: `1px solid ${C.RED}40`,
                color: C.RED, borderRadius: '2px', cursor: 'pointer', fontFamily: FONT,
                display: 'flex', alignItems: 'center', gap: '3px',
              }}><Trash2 size={9} />DEL</button>
              <button onClick={() => setEditMon(null)} style={{
                padding: '4px 10px', fontSize: '8px',
                background: 'none', border: `1px solid ${C.BORDER}`,
                color: C.TEXT_DIM, borderRadius: '2px', cursor: 'pointer', fontFamily: FONT,
              }}>CANCEL</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
