// File: src/components/tabs/quantlib-core/shared.tsx
// Shared design tokens, UI primitives, and hooks for QuantLib Core panels

import React, { useState, useCallback } from 'react';
import { ChevronDown, ChevronUp, Copy, RefreshCw, Zap } from 'lucide-react';

// ── Fincept design tokens ──────────────────────────────────────────
export const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B',
  GREEN: '#00D66F', GRAY: '#787878', DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F', HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A',
  HOVER: '#1F1F1F', MUTED: '#4A4A4A', CYAN: '#00E5FF',
  YELLOW: '#FFD700', BLUE: '#0088FF',
};

export const FONT = '"IBM Plex Mono", Consolas, monospace';

// ── Reusable UI primitives ─────────────────────────────────────────
export const Label: React.FC<{ text: string }> = ({ text }) => (
  <label style={{ color: F.GRAY, fontSize: '9px', fontWeight: 700, textTransform: 'uppercase', letterSpacing: '0.5px' }}>{text}</label>
);

export const Input: React.FC<{
  value: string | number;
  onChange: (v: string) => void;
  type?: string;
  placeholder?: string;
  style?: React.CSSProperties;
}> = ({ value, onChange, type = 'text', placeholder, style }) => (
  <input
    type={type}
    value={value}
    onChange={e => onChange(e.target.value)}
    placeholder={placeholder}
    style={{
      backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, color: F.WHITE,
      padding: '8px 10px', borderRadius: '2px', fontSize: '10px', width: '100%',
      fontFamily: FONT, outline: 'none', ...style,
    }}
  />
);

export const Select: React.FC<{
  value: string; onChange: (v: string) => void; options: string[];
}> = ({ value, onChange, options }) => (
  <select
    value={value}
    onChange={e => onChange(e.target.value)}
    style={{
      backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, color: F.WHITE,
      padding: '8px 10px', borderRadius: '2px', fontSize: '10px', width: '100%',
      fontFamily: FONT, outline: 'none',
    }}
  >
    {options.map(o => <option key={o} value={o}>{o}</option>)}
  </select>
);

export const RunButton: React.FC<{ onClick: () => void; loading: boolean; label?: string }> = ({ onClick, loading, label = 'Run' }) => (
  <button
    onClick={onClick}
    disabled={loading}
    style={{
      backgroundColor: loading ? F.MUTED : F.ORANGE, color: F.DARK_BG, border: 'none',
      padding: '8px 16px', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
      cursor: loading ? 'not-allowed' : 'pointer', display: 'flex',
      alignItems: 'center', gap: 4, fontFamily: FONT,
    }}
  >
    {loading ? <RefreshCw size={12} className="animate-spin" /> : <Zap size={12} />}
    {loading ? 'Running...' : label}
  </button>
);

export const ResultPanel: React.FC<{ data: any; error: string | null }> = ({ data, error }) => {
  if (error) {
    return (
      <div style={{ background: `${F.RED}20`, border: `1px solid ${F.RED}`, borderRadius: '2px', padding: 10, marginTop: 8 }}>
        <span style={{ color: F.RED, fontSize: '10px', fontFamily: FONT, whiteSpace: 'pre-wrap' }}>{error}</span>
      </div>
    );
  }
  if (data === null || data === undefined) return null;
  const text = typeof data === 'object' ? JSON.stringify(data, null, 2) : String(data);
  return (
    <div style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', padding: 10, marginTop: 8, position: 'relative' }}>
      <button
        onClick={() => navigator.clipboard.writeText(text)}
        style={{ position: 'absolute', top: 6, right: 6, background: 'none', border: 'none', color: F.GRAY, cursor: 'pointer' }}
        title="Copy"
      >
        <Copy size={12} />
      </button>
      <pre style={{ color: F.CYAN, fontSize: '10px', fontFamily: FONT, whiteSpace: 'pre-wrap', margin: 0, maxHeight: 300, overflow: 'auto' }}>{text}</pre>
    </div>
  );
};

export const EndpointCard: React.FC<{ title: string; description?: string; children: React.ReactNode }> = ({ title, description, children }) => {
  const [open, setOpen] = useState(false);
  return (
    <div style={{ backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', marginBottom: 8 }}>
      <div
        onClick={() => setOpen(!open)}
        style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '8px 12px', cursor: 'pointer', userSelect: 'none', transition: 'all 0.2s' }}
      >
        <div>
          <span style={{ color: F.WHITE, fontSize: '11px', fontWeight: 700 }}>{title}</span>
          {description && <span style={{ color: F.GRAY, fontSize: '9px', marginLeft: 8 }}>{description}</span>}
        </div>
        {open ? <ChevronUp size={14} color={F.GRAY} /> : <ChevronDown size={14} color={F.GRAY} />}
      </div>
      {open && <div style={{ padding: '0 12px 12px', display: 'flex', flexDirection: 'column', gap: 8 }}>{children}</div>}
    </div>
  );
};

export const Row: React.FC<{ children: React.ReactNode }> = ({ children }) => (
  <div style={{ display: 'flex', gap: 8, alignItems: 'flex-end', flexWrap: 'wrap' }}>{children}</div>
);

export const Field: React.FC<{ label: string; children: React.ReactNode; width?: string }> = ({ label, children, width = '140px' }) => (
  <div style={{ display: 'flex', flexDirection: 'column', gap: 2, width }}><Label text={label} />{children}</div>
);

// ── Hook for endpoint state ────────────────────────────────────────
export function useEndpoint<T = any>() {
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<T | null>(null);
  const [error, setError] = useState<string | null>(null);
  const run = useCallback(async (fn: () => Promise<T>) => {
    setLoading(true); setError(null); setResult(null);
    try { const r = await fn(); setResult(r); }
    catch (e: any) { setError(e.message || String(e)); }
    finally { setLoading(false); }
  }, []);
  return { loading, result, error, run };
}
