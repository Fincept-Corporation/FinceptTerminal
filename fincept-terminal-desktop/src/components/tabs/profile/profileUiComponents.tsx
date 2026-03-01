/**
 * Profile Tab — Shared UI Components (FINCEPT Design System)
 *
 * Extracted from ProfileTab.tsx.
 */

import React from 'react';
import { RefreshCw, CheckCircle, AlertCircle, Send, Check } from 'lucide-react';

// ─── FINCEPT Design System Colors ─────────────────────────────────────────────
export const F = {
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
};

export const FONT = '"IBM Plex Mono", "Consolas", monospace';

// ─── Shared UI Components ─────────────────────────────────────────────────────

export const Panel: React.FC<{ title: string; icon: React.ReactNode; children: React.ReactNode }> = ({ title, icon, children }) => (
  <div style={{ backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
    <div style={{
      padding: '12px',
      backgroundColor: F.HEADER_BG,
      borderBottom: `1px solid ${F.BORDER}`,
      display: 'flex',
      alignItems: 'center',
      gap: '8px',
    }}>
      <span style={{ color: F.ORANGE }}>{icon}</span>
      <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>{title}</span>
    </div>
    <div style={{ padding: '12px' }}>{children}</div>
  </div>
);

export const DataRow: React.FC<{ label: string; value: string; valueColor?: string }> = ({ label, value, valueColor }) => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    padding: '6px 0',
    fontSize: '10px',
    borderBottom: `1px solid ${F.BORDER}`,
    fontFamily: FONT,
  }}>
    <span style={{ color: F.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px' }}>{label}</span>
    <span style={{ color: valueColor || F.WHITE, fontWeight: 700 }}>{value}</span>
  </div>
);

export const StatBox: React.FC<{ label: string; value: number | string; isText?: boolean; color?: string }> = ({ label, value, isText, color }) => (
  <div style={{
    padding: '12px',
    backgroundColor: F.HEADER_BG,
    border: `1px solid ${F.BORDER}`,
    borderRadius: '2px',
    textAlign: 'center',
  }}>
    <div style={{ fontSize: isText ? '14px' : '24px', color: color || F.CYAN, fontWeight: 700, fontFamily: FONT }}>
      {typeof value === 'number' ? value.toLocaleString() : value}
    </div>
    <div style={{ fontSize: '9px', color: F.GRAY, marginTop: '4px', fontWeight: 700, letterSpacing: '0.5px' }}>{label}</div>
  </div>
);

export const EnabledBadge: React.FC<{ enabled: boolean }> = ({ enabled }) => (
  <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
    <span style={{
      padding: '2px 6px',
      backgroundColor: enabled ? `${F.GREEN}20` : `${F.RED}20`,
      color: enabled ? F.GREEN : F.RED,
      fontSize: '8px',
      fontWeight: 700,
      borderRadius: '2px',
    }}>
      {enabled ? 'ENABLED' : 'DISABLED'}
    </span>
    {enabled ? <CheckCircle size={12} color={F.GREEN} /> : <AlertCircle size={12} color={F.RED} />}
  </div>
);

export const SecurityRow: React.FC<{ label: string; enabled: boolean }> = ({ label, enabled }) => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: '10px 0',
    borderBottom: `1px solid ${F.BORDER}`,
  }}>
    <span style={{ color: F.WHITE, fontSize: '10px', fontFamily: FONT }}>{label}</span>
    <EnabledBadge enabled={enabled} />
  </div>
);

export const Badge: React.FC<{ label: string; value: string; color: string }> = ({ label, value, color }) => (
  <div style={{
    padding: '6px 10px',
    backgroundColor: F.PANEL_BG,
    border: `1px solid ${F.BORDER}`,
    borderRadius: '2px',
    fontFamily: FONT,
    fontSize: '9px',
  }}>
    <span style={{ color: F.GRAY, fontWeight: 700, letterSpacing: '0.5px' }}>{label} </span>
    <span style={{ color, fontWeight: 700 }}>{value}</span>
  </div>
);

export const PrimaryBtn: React.FC<{
  label: string;
  onClick: () => void;
  disabled?: boolean;
  color?: string;
  icon?: React.ReactNode;
}> = ({ label, onClick, disabled, color = F.ORANGE, icon }) => (
  <button
    onClick={onClick}
    disabled={disabled}
    style={{
      padding: '8px 16px',
      backgroundColor: color,
      color: F.DARK_BG,
      border: 'none',
      borderRadius: '2px',
      fontSize: '9px',
      fontWeight: 700,
      fontFamily: FONT,
      cursor: disabled ? 'not-allowed' : 'pointer',
      opacity: disabled ? 0.5 : 1,
      display: 'flex',
      alignItems: 'center',
      gap: '6px',
      transition: 'all 0.2s',
    }}
  >
    {icon}
    {label}
  </button>
);

export const SecondaryBtn: React.FC<{ label: string; onClick: () => void; icon?: React.ReactNode }> = ({ label, onClick, icon }) => (
  <button
    onClick={(e) => { e.preventDefault(); e.stopPropagation(); onClick(); }}
    style={{
      padding: '6px 10px',
      backgroundColor: 'transparent',
      border: `1px solid ${F.BORDER}`,
      borderRadius: '2px',
      color: F.GRAY,
      fontSize: '9px',
      fontWeight: 700,
      fontFamily: FONT,
      cursor: 'pointer',
      display: 'flex',
      alignItems: 'center',
      gap: '4px',
      transition: 'all 0.2s',
    }}
  >
    {icon}
    {label}
  </button>
);

export const ActionBtn: React.FC<{ label: string; onClick: () => void; danger?: boolean; disabled?: boolean }> = ({ label, onClick, danger, disabled }) => (
  <button
    onClick={onClick}
    disabled={disabled}
    style={{
      padding: '8px 16px',
      backgroundColor: disabled ? F.MUTED : (danger ? F.RED : F.ORANGE),
      color: disabled ? F.GRAY : F.DARK_BG,
      border: 'none',
      borderRadius: '2px',
      fontSize: '9px',
      fontWeight: 700,
      fontFamily: FONT,
      cursor: disabled ? 'not-allowed' : 'pointer',
      transition: 'all 0.2s',
      opacity: disabled ? 0.7 : 1,
    }}
  >
    {label}
  </button>
);

export const Label: React.FC<{ text: string; required?: boolean }> = ({ text, required }) => (
  <label style={{ color: F.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', display: 'block', marginBottom: '4px' }}>
    {text} {required && <span style={{ color: F.RED }}>*</span>}
  </label>
);

export const InputField: React.FC<{
  label: string;
  value: string;
  onChange: (v: string) => void;
  placeholder?: string;
  type?: string;
  required?: boolean;
  multiline?: boolean;
  rows?: number;
}> = ({ label, value, onChange, placeholder, type = 'text', required, multiline, rows = 4 }) => (
  <div style={{ marginBottom: '12px' }}>
    <Label text={label} required={required} />
    {multiline ? (
      <textarea
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        rows={rows}
        style={{
          width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`,
          borderRadius: '2px', color: F.WHITE, fontSize: '10px', fontFamily: FONT, resize: 'vertical',
        }}
      />
    ) : (
      <input
        type={type}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        style={{
          width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`,
          borderRadius: '2px', color: F.WHITE, fontSize: '10px', fontFamily: FONT,
        }}
      />
    )}
  </div>
);

export const SubmitBtn: React.FC<{
  onClick: () => void;
  loading: boolean;
  success: boolean;
  text: string;
  color?: string;
}> = ({ onClick, loading, success, text, color = F.ORANGE }) => (
  <button
    onClick={onClick}
    disabled={loading}
    style={{
      width: '100%',
      backgroundColor: success ? F.GREEN : color,
      color: F.DARK_BG,
      border: 'none',
      borderRadius: '2px',
      padding: '8px 16px',
      fontSize: '9px',
      fontWeight: 700,
      fontFamily: FONT,
      cursor: loading ? 'not-allowed' : 'pointer',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      gap: '8px',
      opacity: loading ? 0.7 : 1,
      transition: 'all 0.2s',
    }}
  >
    {loading ? 'SUBMITTING...' : success ? <><Check size={12} /> SUCCESS!</> : <><Send size={12} /> {text}</>}
  </button>
);

export const StatusBadge: React.FC<{ type: 'success' | 'error'; text: string }> = ({ type, text }) => (
  <div style={{
    padding: '8px 12px',
    backgroundColor: type === 'success' ? `${F.GREEN}15` : `${F.RED}15`,
    border: `1px solid ${type === 'success' ? F.GREEN : F.RED}`,
    borderRadius: '2px',
    color: type === 'success' ? F.GREEN : F.RED,
    fontSize: '9px',
    marginBottom: '12px',
    fontFamily: FONT,
    fontWeight: 700,
  }}>
    {text}
  </div>
);

export const LoadingState: React.FC<{ message: string }> = ({ message }) => (
  <div style={{
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    justifyContent: 'center',
    padding: '40px',
    color: F.MUTED,
    fontSize: '10px',
    fontFamily: FONT,
  }}>
    <RefreshCw size={20} style={{ marginBottom: '8px', opacity: 0.5 }} className="animate-spin" />
    <span>{message}</span>
  </div>
);

export const EmptyState: React.FC<{ icon: React.ElementType; message: string }> = ({ icon: Icon, message }) => (
  <div style={{
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    justifyContent: 'center',
    padding: '32px',
    color: F.MUTED,
    fontSize: '10px',
    textAlign: 'center',
    fontFamily: FONT,
  }}>
    <Icon size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
    <span>{message}</span>
  </div>
);

export const ConfirmModal: React.FC<{
  icon: React.ReactNode;
  title: string;
  message: string;
  confirmLabel: string;
  confirmColor: string;
  disabled?: boolean;
  onCancel: () => void;
  onConfirm: () => void;
  children?: React.ReactNode;
}> = ({ icon, title, message, confirmLabel, confirmColor, disabled, onCancel, onConfirm, children }) => (
  <div style={{
    position: 'fixed',
    top: 0, left: 0, right: 0, bottom: 0,
    backgroundColor: 'rgba(0,0,0,0.8)',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    zIndex: 1000,
  }}>
    <div style={{
      backgroundColor: F.PANEL_BG,
      border: `1px solid ${F.ORANGE}`,
      borderRadius: '2px',
      padding: '24px',
      maxWidth: '400px',
      width: '90%',
    }}>
      <div style={{
        color: F.ORANGE,
        fontSize: '11px',
        fontWeight: 700,
        marginBottom: '16px',
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        fontFamily: FONT,
        letterSpacing: '0.5px',
      }}>
        {icon}
        {title}
      </div>
      <div style={{ color: F.GRAY, fontSize: '10px', marginBottom: '16px', fontFamily: FONT, lineHeight: '1.5' }}>
        {message}
      </div>
      {children}
      <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
        <SecondaryBtn label="CANCEL" onClick={onCancel} />
        <PrimaryBtn label={confirmLabel} onClick={onConfirm} disabled={disabled} color={confirmColor} />
      </div>
    </div>
  </div>
);
