// MCP Add Server Modal Component
// Add custom MCP servers manually

import React, { useState } from 'react';
import { X } from 'lucide-react';

interface MCPAddServerModalProps {
  onClose: () => void;
  onAdd: (serverConfig: any) => void;
}

const MCPAddServerModal: React.FC<MCPAddServerModalProps> = ({ onClose, onAdd }) => {
  const [formData, setFormData] = useState({
    name: '',
    description: '',
    command: 'bunx',
    args: '',
    envVars: '',
    category: 'custom',
    icon: '🔧'
  });

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';
  const BLOOMBERG_RED = '#FF0000';

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();

    if (!formData.name || !formData.command || !formData.args) {
      alert('Please fill in all required fields');
      return;
    }

    // Parse args (space or comma separated)
    const args = formData.args
      .trim()
      .split(/[\s,]+/)
      .filter(arg => arg.length > 0);

    // Parse env vars (KEY=VALUE format, one per line)
    const env: Record<string, string> = {};
    if (formData.envVars.trim()) {
      formData.envVars.split('\n').forEach(line => {
        const [key, ...valueParts] = line.trim().split('=');
        if (key && valueParts.length > 0) {
          env[key.trim()] = valueParts.join('=').trim();
        }
      });
    }

    // Generate ID from name
    const id = formData.name.toLowerCase().replace(/[^a-z0-9]/g, '-');

    const serverConfig = {
      id,
      name: formData.name,
      description: formData.description || `Custom MCP server: ${formData.name}`,
      command: formData.command,
      args,
      env,
      category: formData.category,
      icon: formData.icon
    };

    onAdd(serverConfig);
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
      fontFamily: 'Consolas, monospace'
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        width: '500px',
        maxHeight: '80vh',
        overflow: 'auto'
      }}>
        {/* Header */}
        <div style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          borderBottom: `1px solid ${BLOOMBERG_ORANGE}`,
          padding: '10px 12px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <span style={{
            color: BLOOMBERG_ORANGE,
            fontSize: '12px',
            fontWeight: 'bold'
          }}>
            ADD CUSTOM MCP SERVER
          </span>
          <button
            onClick={onClose}
            style={{
              backgroundColor: 'transparent',
              border: 'none',
              color: BLOOMBERG_WHITE,
              cursor: 'pointer',
              padding: '4px'
            }}
          >
            <X size={16} />
          </button>
        </div>

        {/* Form */}
        <form onSubmit={handleSubmit} style={{ padding: '12px' }}>
          {/* Server Name */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_ORANGE,
              fontSize: '10px',
              marginBottom: '4px',
              fontWeight: 'bold'
            }}>
              SERVER NAME *
            </label>
            <input
              type="text"
              value={formData.name}
              onChange={(e) => setFormData({ ...formData, name: e.target.value })}
              placeholder="e.g., My Custom Server"
              required
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '6px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace'
              }}
            />
          </div>

          {/* Description */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_ORANGE,
              fontSize: '10px',
              marginBottom: '4px',
              fontWeight: 'bold'
            }}>
              DESCRIPTION
            </label>
            <input
              type="text"
              value={formData.description}
              onChange={(e) => setFormData({ ...formData, description: e.target.value })}
              placeholder="What does this server do?"
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '6px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace'
              }}
            />
          </div>

          {/* Command */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_ORANGE,
              fontSize: '10px',
              marginBottom: '4px',
              fontWeight: 'bold'
            }}>
              COMMAND *
            </label>
            <input
              type="text"
              value={formData.command}
              onChange={(e) => setFormData({ ...formData, command: e.target.value })}
              placeholder="bunx, node, python, etc."
              required
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '6px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace'
              }}
            />
          </div>

          {/* Arguments */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_ORANGE,
              fontSize: '10px',
              marginBottom: '4px',
              fontWeight: 'bold'
            }}>
              ARGUMENTS * (space or comma separated)
            </label>
            <input
              type="text"
              value={formData.args}
              onChange={(e) => setFormData({ ...formData, args: e.target.value })}
              placeholder="e.g., -y @modelcontextprotocol/server-example"
              required
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '6px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace'
              }}
            />
            <div style={{
              color: BLOOMBERG_GRAY,
              fontSize: '8px',
              marginTop: '2px'
            }}>
              Example: -y @modelcontextprotocol/server-postgres
            </div>
          </div>

          {/* Environment Variables */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_ORANGE,
              fontSize: '10px',
              marginBottom: '4px',
              fontWeight: 'bold'
            }}>
              ENVIRONMENT VARIABLES (one per line)
            </label>
            <textarea
              value={formData.envVars}
              onChange={(e) => setFormData({ ...formData, envVars: e.target.value })}
              placeholder={'DATABASE_URL=postgresql://...\nAPI_KEY=your-key-here'}
              rows={3}
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '6px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace',
                resize: 'vertical'
              }}
            />
            <div style={{
              color: BLOOMBERG_GRAY,
              fontSize: '8px',
              marginTop: '2px'
            }}>
              Format: KEY=VALUE (one per line)
            </div>
          </div>

          {/* Icon */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_ORANGE,
              fontSize: '10px',
              marginBottom: '4px',
              fontWeight: 'bold'
            }}>
              ICON (emoji)
            </label>
            <input
              type="text"
              value={formData.icon}
              onChange={(e) => setFormData({ ...formData, icon: e.target.value })}
              placeholder="🔧"
              maxLength={2}
              style={{
                width: '60px',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '6px',
                fontSize: '16px',
                textAlign: 'center'
              }}
            />
          </div>

          {/* Actions */}
          <div style={{
            display: 'flex',
            gap: '8px',
            justifyContent: 'flex-end',
            marginTop: '16px',
            paddingTop: '12px',
            borderTop: `1px solid ${BLOOMBERG_GRAY}`
          }}>
            <button
              type="button"
              onClick={onClose}
              style={{
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '6px 12px',
                fontSize: '10px',
                cursor: 'pointer'
              }}
            >
              CANCEL
            </button>
            <button
              type="submit"
              style={{
                backgroundColor: BLOOMBERG_ORANGE,
                border: 'none',
                color: 'black',
                padding: '6px 12px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer'
              }}
            >
              ADD SERVER
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};

export default MCPAddServerModal;
