// MCP Server Logs Viewer Component
// Shows stdout/stderr logs from MCP servers

import React, { useState, useEffect } from 'react';
import { X, Download, Trash2 } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';

interface MCPServerLogsModalProps {
  serverId: string;
  serverName: string;
  onClose: () => void;
}

interface LogEntry {
  timestamp: string;
  type: 'stdout' | 'stderr' | 'system';
  message: string;
}

const MCPServerLogsModal: React.FC<MCPServerLogsModalProps> = ({
  serverId,
  serverName,
  onClose
}) => {
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [autoScroll, setAutoScroll] = useState(true);
  const logsEndRef = React.useRef<HTMLDivElement>(null);

  const FINCEPT_ORANGE = '#FFA500';
  const FINCEPT_WHITE = '#FFFFFF';
  const FINCEPT_GRAY = '#787878';
  const FINCEPT_DARK_BG = '#000000';
  const FINCEPT_PANEL_BG = '#0a0a0a';
  const FINCEPT_GREEN = '#00C800';
  const FINCEPT_RED = '#FF0000';
  const FINCEPT_YELLOW = '#FFFF00';

  useEffect(() => {
    loadLogs();

    // Refresh logs every second
    const interval = setInterval(() => {
      loadLogs();
    }, 1000);

    return () => clearInterval(interval);
  }, [serverId]);

  useEffect(() => {
    if (autoScroll) {
      logsEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logs, autoScroll]);

  const loadLogs = async () => {
    try {
      // Get logs from Tauri (this would need to be implemented in Rust)
      // For now, we'll use mock data
      // In production, you'd call: invoke('get_mcp_server_logs', { serverId })

      // Mock implementation
      const mockLogs: LogEntry[] = [
        {
          timestamp: new Date().toISOString(),
          type: 'system',
          message: `Server ${serverId} initialized`
        },
        {
          timestamp: new Date().toISOString(),
          type: 'stdout',
          message: 'MCP server listening on stdio'
        }
      ];

      setLogs(mockLogs);
    } catch (error) {
      console.error('Failed to load logs:', error);
    }
  };

  const clearLogs = () => {
    setLogs([]);
  };

  const downloadLogs = () => {
    const logText = logs
      .map(log => `[${log.timestamp}] [${log.type}] ${log.message}`)
      .join('\n');

    const blob = new Blob([logText], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${serverId}-logs-${Date.now()}.txt`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const getLogColor = (type: LogEntry['type']) => {
    switch (type) {
      case 'stdout':
        return FINCEPT_WHITE;
      case 'stderr':
        return FINCEPT_RED;
      case 'system':
        return FINCEPT_YELLOW;
      default:
        return FINCEPT_GRAY;
    }
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
        backgroundColor: FINCEPT_PANEL_BG,
        border: `2px solid ${FINCEPT_ORANGE}`,
        width: '80%',
        height: '80%',
        display: 'flex',
        flexDirection: 'column'
      }}>
        {/* Header */}
        <div style={{
          backgroundColor: FINCEPT_DARK_BG,
          borderBottom: `1px solid ${FINCEPT_ORANGE}`,
          padding: '10px 12px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexShrink: 0
        }}>
          <span style={{
            color: FINCEPT_ORANGE,
            fontSize: '12px',
            fontWeight: 'bold'
          }}>
            SERVER LOGS: {serverName}
          </span>

          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
            <label style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              color: FINCEPT_WHITE,
              fontSize: '10px'
            }}>
              <input
                type="checkbox"
                checked={autoScroll}
                onChange={(e) => setAutoScroll(e.target.checked)}
              />
              Auto-scroll
            </label>

            <button
              onClick={clearLogs}
              style={{
                backgroundColor: FINCEPT_DARK_BG,
                border: `1px solid ${FINCEPT_GRAY}`,
                color: FINCEPT_WHITE,
                padding: '4px 8px',
                fontSize: '10px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Trash2 size={12} />
              Clear
            </button>

            <button
              onClick={downloadLogs}
              style={{
                backgroundColor: FINCEPT_DARK_BG,
                border: `1px solid ${FINCEPT_GRAY}`,
                color: FINCEPT_WHITE,
                padding: '4px 8px',
                fontSize: '10px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Download size={12} />
              Download
            </button>

            <button
              onClick={onClose}
              style={{
                backgroundColor: 'transparent',
                border: 'none',
                color: FINCEPT_WHITE,
                cursor: 'pointer',
                padding: '4px'
              }}
            >
              <X size={16} />
            </button>
          </div>
        </div>

        {/* Logs Content */}
        <div style={{
          flex: 1,
          backgroundColor: FINCEPT_DARK_BG,
          padding: '10px',
          overflow: 'auto',
          fontFamily: 'Consolas, monospace',
          fontSize: '10px'
        }}>
          {logs.length === 0 ? (
            <div style={{
              color: FINCEPT_GRAY,
              textAlign: 'center',
              padding: '40px'
            }}>
              No logs available. Server may not be running.
            </div>
          ) : (
            logs.map((log, index) => (
              <div
                key={index}
                style={{
                  marginBottom: '4px',
                  display: 'flex',
                  gap: '8px',
                  color: getLogColor(log.type)
                }}
              >
                <span style={{ color: FINCEPT_GRAY, minWidth: '180px' }}>
                  [{new Date(log.timestamp).toLocaleTimeString()}]
                </span>
                <span style={{
                  color: FINCEPT_YELLOW,
                  minWidth: '60px',
                  fontWeight: 'bold'
                }}>
                  [{log.type.toUpperCase()}]
                </span>
                <span style={{ flex: 1, whiteSpace: 'pre-wrap' }}>
                  {log.message}
                </span>
              </div>
            ))
          )}
          <div ref={logsEndRef} />
        </div>

        {/* Footer */}
        <div style={{
          backgroundColor: FINCEPT_PANEL_BG,
          borderTop: `1px solid ${FINCEPT_GRAY}`,
          padding: '6px 12px',
          fontSize: '9px',
          color: FINCEPT_GRAY,
          flexShrink: 0
        }}>
          {logs.length} log entries | Server ID: {serverId}
        </div>
      </div>
    </div>
  );
};

export default MCPServerLogsModal;
