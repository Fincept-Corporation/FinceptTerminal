// MCP Server Card Component
// Individual server display with controls

import React from 'react';
import { Play, Square, Trash2 } from 'lucide-react';
import { MCPServerWithStats } from '../../../services/mcpManager';

interface MCPServerCardProps {
  server: MCPServerWithStats;
  onStart: () => void;
  onStop: () => void;
  onRemove: () => void;
  onToggleAutoStart: (enabled: boolean) => void;
  healthInfo?: { errorCount: number; lastError?: string; lastErrorTime?: number } | null;
}

const MCPServerCard: React.FC<MCPServerCardProps> = ({
  server,
  onStart,
  onStop,
  onRemove,
  onToggleAutoStart,
  healthInfo
}) => {
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_YELLOW = '#FFFF00';

  const statusColor = {
    running: BLOOMBERG_GREEN,
    stopped: BLOOMBERG_GRAY,
    error: BLOOMBERG_RED
  }[server.status] || BLOOMBERG_GRAY;

  const statusIcon = {
    running: '🟢',
    stopped: '🔴',
    error: '❌'
  }[server.status] || '⚪';

  const statusText = {
    running: `Running | ${server.callsToday} calls today`,
    stopped: 'Stopped',
    error: 'Connection error'
  }[server.status] || 'Unknown';

  return (
    <div style={{
      backgroundColor: BLOOMBERG_DARK_BG,
      border: `1px solid ${statusColor}`,
      padding: '8px',
      marginBottom: '6px',
      borderRadius: '2px'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'flex-start',
        marginBottom: '6px'
      }}>
        <div style={{ flex: 1 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '3px' }}>
            <span style={{ fontSize: '14px' }}>{server.icon || '🔧'}</span>
            <span style={{ fontSize: '12px' }}>{statusIcon}</span>
            <span style={{ color: BLOOMBERG_WHITE, fontSize: '11px', fontWeight: 'bold' }}>
              {server.name}
            </span>
            <span style={{
              color: BLOOMBERG_GRAY,
              fontSize: '8px',
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              padding: '2px 4px'
            }}>
              {server.toolCount} tools
            </span>
          </div>

          <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '4px' }}>
            {server.description}
          </div>

          <div style={{
            color: server.status === 'running' ? BLOOMBERG_GREEN : BLOOMBERG_GRAY,
            fontSize: '9px'
          }}>
            {statusText}
          </div>

          {/* Health Status */}
          {healthInfo && healthInfo.errorCount > 0 && (
            <div style={{
              marginTop: '4px',
              padding: '4px 6px',
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_RED}`,
              borderRadius: '2px'
            }}>
              <div style={{
                color: BLOOMBERG_RED,
                fontSize: '8px',
                fontWeight: 'bold',
                marginBottom: '2px'
              }}>
                ⚠ HEALTH WARNING: {healthInfo.errorCount} errors
              </div>
              {healthInfo.lastError && (
                <div style={{
                  color: BLOOMBERG_GRAY,
                  fontSize: '8px',
                  fontFamily: 'Consolas, monospace'
                }}>
                  Last: {healthInfo.lastError}
                </div>
              )}
              {healthInfo.lastErrorTime && (
                <div style={{
                  color: BLOOMBERG_GRAY,
                  fontSize: '7px',
                  marginTop: '2px'
                }}>
                  {new Date(healthInfo.lastErrorTime).toLocaleString()}
                </div>
              )}
            </div>
          )}
        </div>
      </div>

      {/* Command Info */}
      <div style={{
        backgroundColor: BLOOMBERG_DARK_BG,
        border: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '4px 6px',
        marginBottom: '6px',
        fontSize: '8px',
        color: BLOOMBERG_GRAY,
        fontFamily: 'Consolas, monospace',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        whiteSpace: 'nowrap'
      }}>
        {server.command} {Array.isArray(server.args) ? server.args.join(' ') : JSON.parse(server.args).join(' ')}
      </div>

      {/* Auto-start Toggle */}
      <div style={{
        marginBottom: '6px',
        display: 'flex',
        alignItems: 'center',
        gap: '6px'
      }}>
        <label style={{
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          color: BLOOMBERG_WHITE,
          fontSize: '9px',
          cursor: 'pointer'
        }}>
          <input
            type="checkbox"
            checked={server.auto_start}
            onChange={(e) => onToggleAutoStart(e.target.checked)}
            style={{ cursor: 'pointer' }}
          />
          Auto-start on launch
        </label>
      </div>

      {/* Actions */}
      <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
        {server.status === 'running' ? (
          <button
            onClick={onStop}
            style={{
              backgroundColor: BLOOMBERG_RED,
              color: BLOOMBERG_WHITE,
              border: 'none',
              padding: '4px 8px',
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '3px',
              fontWeight: 'bold'
            }}
          >
            <Square size={10} fill="white" />
            STOP
          </button>
        ) : (
          <button
            onClick={onStart}
            style={{
              backgroundColor: BLOOMBERG_GREEN,
              color: 'black',
              border: 'none',
              padding: '4px 8px',
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '3px',
              fontWeight: 'bold'
            }}
          >
            <Play size={10} fill="black" />
            START
          </button>
        )}

        <button
          onClick={onRemove}
          style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            color: BLOOMBERG_RED,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px 8px',
            fontSize: '9px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '3px'
          }}
        >
          <Trash2 size={10} />
          REMOVE
        </button>
      </div>
    </div>
  );
};

export default MCPServerCard;
