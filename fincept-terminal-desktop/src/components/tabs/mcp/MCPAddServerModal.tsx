// MCP Add Server Modal Component
// Add custom MCP servers manually - Fincept UI Design System

import React, { useState } from 'react';
import { X, Plus } from 'lucide-react';
import { showWarning } from '@/utils/notifications';

interface MCPAddServerModalProps {
  onClose: () => void;
  onAdd: (serverConfig: any) => void;
}

const FINCEPT = {
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
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

const MCPAddServerModal: React.FC<MCPAddServerModalProps> = ({ onClose, onAdd }) => {
  const [formData, setFormData] = useState({
    name: '',
    description: '',
    command: 'bunx',
    args: '',
    envVars: '',
    category: 'custom',
    icon: '🔧',
  });

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();

    if (!formData.name || !formData.command || !formData.args) {
      showWarning('Please fill in all required fields');
      return;
    }

    const args = formData.args
      .trim()
      .split(/[\s,]+/)
      .filter(arg => arg.length > 0);

    const env: Record<string, string> = {};
    if (formData.envVars.trim()) {
      formData.envVars.split('\n').forEach(line => {
        const [key, ...valueParts] = line.trim().split('=');
        if (key && valueParts.length > 0) {
          env[key.trim()] = valueParts.join('=').trim();
        }
      });
    }

    const id = formData.name.toLowerCase().replace(/[^a-z0-9]/g, '-');

    const serverConfig = {
      id,
      name: formData.name,
      description: formData.description || `Custom MCP server: ${formData.name}`,
      command: formData.command,
      args,
      env,
      category: formData.category,
      icon: formData.icon,
    };

    onAdd(serverConfig);
  };

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '10px',
    fontFamily: FONT_FAMILY,
    outline: 'none',
  };

  const labelStyle: React.CSSProperties = {
    display: 'block',
    fontSize: '9px',
    fontWeight: 700,
    color: FINCEPT.GRAY,
    letterSpacing: '0.5px',
    marginBottom: '6px',
  };

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000,
      fontFamily: FONT_FAMILY,
    }}>
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.ORANGE}`,
        borderRadius: '2px',
        width: '480px',
        maxHeight: '80vh',
        overflow: 'auto',
        display: 'flex',
        flexDirection: 'column',
      }}>
        {/* Header */}
        <div style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `2px solid ${FINCEPT.ORANGE}`,
          padding: '12px 16px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexShrink: 0,
        }}>
          <span style={{
            color: FINCEPT.ORANGE,
            fontSize: '11px',
            fontWeight: 700,
            letterSpacing: '0.5px',
          }}>
            ADD CUSTOM MCP SERVER
          </span>
          <button
            onClick={onClose}
            style={{
              backgroundColor: 'transparent',
              border: 'none',
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              padding: '4px',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => { e.currentTarget.style.color = FINCEPT.WHITE; }}
            onMouseLeave={(e) => { e.currentTarget.style.color = FINCEPT.GRAY; }}
          >
            <X size={14} />
          </button>
        </div>

        {/* Form */}
        <form onSubmit={handleSubmit} style={{ padding: '16px', overflow: 'auto', flex: 1 }}>
          {/* Server Name */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>SERVER NAME *</label>
            <input
              type="text"
              value={formData.name}
              onChange={(e) => setFormData({ ...formData, name: e.target.value })}
              placeholder="e.g., My Custom Server"
              required
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
            />
          </div>

          {/* Description */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>DESCRIPTION</label>
            <input
              type="text"
              value={formData.description}
              onChange={(e) => setFormData({ ...formData, description: e.target.value })}
              placeholder="What does this server do?"
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
            />
          </div>

          {/* Command */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>COMMAND *</label>
            <input
              type="text"
              value={formData.command}
              onChange={(e) => setFormData({ ...formData, command: e.target.value })}
              placeholder="bunx, node, python, etc."
              required
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
            />
          </div>

          {/* Arguments */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>ARGUMENTS * (SPACE OR COMMA SEPARATED)</label>
            <input
              type="text"
              value={formData.args}
              onChange={(e) => setFormData({ ...formData, args: e.target.value })}
              placeholder="e.g., -y @modelcontextprotocol/server-example"
              required
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
            />
            <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '4px' }}>
              Example: -y @modelcontextprotocol/server-postgres
            </div>
          </div>

          {/* Environment Variables */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>ENVIRONMENT VARIABLES (ONE PER LINE)</label>
            <textarea
              value={formData.envVars}
              onChange={(e) => setFormData({ ...formData, envVars: e.target.value })}
              placeholder={'DATABASE_URL=postgresql://...\nAPI_KEY=your-key-here'}
              rows={3}
              style={{
                ...inputStyle,
                resize: 'vertical',
              }}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
            />
            <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '4px' }}>
              Format: KEY=VALUE (one per line)
            </div>
          </div>

          {/* Icon */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>ICON (EMOJI)</label>
            <input
              type="text"
              value={formData.icon}
              onChange={(e) => setFormData({ ...formData, icon: e.target.value })}
              placeholder="🔧"
              maxLength={2}
              style={{
                ...inputStyle,
                width: '60px',
                fontSize: '16px',
                textAlign: 'center',
              }}
            />
          </div>

          {/* Actions */}
          <div style={{
            display: 'flex',
            gap: '8px',
            justifyContent: 'flex-end',
            paddingTop: '12px',
            borderTop: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <button
              type="button"
              onClick={onClose}
              style={{
                padding: '6px 10px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                borderRadius: '2px',
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
                transition: 'all 0.2s',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                e.currentTarget.style.color = FINCEPT.WHITE;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
                e.currentTarget.style.color = FINCEPT.GRAY;
              }}
            >
              CANCEL
            </button>
            <button
              type="submit"
              style={{
                padding: '8px 16px',
                backgroundColor: FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                fontFamily: FONT_FAMILY,
              }}
            >
              <Plus size={10} />
              ADD SERVER
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};

export default MCPAddServerModal;
