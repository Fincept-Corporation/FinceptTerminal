import React, { useState } from 'react';
import { Handle, Position } from 'reactflow';
import { Database, Play, Settings, Check, AlertCircle, RefreshCw } from 'lucide-react';
import { useDataSources } from '../../../../contexts/DataSourceContext';
import { createAdapter } from '../../data-sources/adapters';

// Design system colors
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
  CYAN: '#00E5FF',
  BLUE: '#0088FF',
};

interface DataSourceNodeProps {
  data: {
    label: string;
    selectedConnectionId?: string;
    query: string;
    status: 'idle' | 'running' | 'completed' | 'error';
    result?: any;
    error?: string;
    onConnectionChange: (connectionId: string) => void;
    onQueryChange: (query: string) => void;
    onExecute: () => void;
    onResultUpdate?: (result: any) => void;
    onStatusUpdate?: (status: 'idle' | 'running' | 'completed' | 'error', error?: string) => void;
  };
  selected: boolean;
}

const DataSourceNode: React.FC<DataSourceNodeProps> = ({ data, selected }) => {
  const { connections, loadState } = useDataSources();
  const [showSettings, setShowSettings] = useState(false);
  const [localQuery, setLocalQuery] = useState(data.query || '');
  const [localConnection, setLocalConnection] = useState(data.selectedConnectionId || '');
  const [isExecuting, setIsExecuting] = useState(false);

  const handleSaveSettings = () => {
    if (localConnection) {
      data.onConnectionChange(localConnection);
    }
    if (localQuery) {
      data.onQueryChange(localQuery);
    }
    setShowSettings(false);
  };

  const handleExecuteQuery = async () => {
    if (!data.selectedConnectionId || !data.query) return;

    const connection = connections.find((c) => c.id === data.selectedConnectionId);
    if (!connection) {
      data.onStatusUpdate?.('error', 'Connection not found');
      return;
    }

    setIsExecuting(true);
    data.onStatusUpdate?.('running');

    try {
      const adapter = createAdapter(connection);

      if (!adapter) {
        throw new Error(`No adapter available for ${connection.type}`);
      }

      const result = await adapter.query(data.query);
      data.onResultUpdate?.(result);
      data.onStatusUpdate?.('completed');
    } catch (error) {
      data.onStatusUpdate?.('error', error instanceof Error ? error.message : 'Unknown error');
    } finally {
      setIsExecuting(false);
    }
  };

  const getStatusColor = () => {
    switch (data.status) {
      case 'running':
        return FINCEPT.BLUE;
      case 'completed':
        return FINCEPT.GREEN;
      case 'error':
        return FINCEPT.RED;
      default:
        return FINCEPT.GRAY;
    }
  };

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running':
        return <RefreshCw size={12} color={FINCEPT.BLUE} style={{ animation: 'spin 1s linear infinite' }} />;
      case 'completed':
        return <Check size={12} color={FINCEPT.GREEN} />;
      case 'error':
        return <AlertCircle size={12} color={FINCEPT.RED} />;
      default:
        return <Database size={12} color={FINCEPT.GRAY} />;
    }
  };

  const selectedConnection = connections.find((c) => c.id === (localConnection || data.selectedConnectionId));
  const isConfigured = data.selectedConnectionId && data.query;
  const availableSources = connections.filter((c) => c.status === 'connected' || c.status === 'disconnected');

  return (
    <div
      style={{
        background: selected ? FINCEPT.HEADER_BG : FINCEPT.PANEL_BG,
        border: `2px solid ${selected ? FINCEPT.ORANGE : getStatusColor()}`,
        borderRadius: '2px',
        minWidth: '280px',
        maxWidth: '400px',
        fontFamily: '"IBM Plex Mono", Consolas, monospace',
        boxShadow: selected ? `0 0 12px ${FINCEPT.ORANGE}40` : '0 2px 8px rgba(0,0,0,0.3)',
        position: 'relative',
      }}
    >
      {/* Output Handle */}
      <Handle
        type="source"
        position={Position.Right}
        style={{
          background: FINCEPT.ORANGE,
          width: '10px',
          height: '10px',
          border: `2px solid ${FINCEPT.DARK_BG}`,
          right: '-6px',
        }}
      />

      {/* Header */}
      <div
        style={{
          backgroundColor: FINCEPT.DARK_BG,
          padding: '8px 12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Database size={14} color={FINCEPT.ORANGE} />
          <span style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700 }}>{data.label}</span>
        </div>
        <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
          {getStatusIcon()}
          <button
            onClick={() => setShowSettings(!showSettings)}
            style={{
              backgroundColor: showSettings ? FINCEPT.ORANGE : 'transparent',
              border: `1px solid ${showSettings ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
              color: showSettings ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              padding: '4px',
              cursor: 'pointer',
              borderRadius: '2px',
              display: 'flex',
              alignItems: 'center',
              transition: 'all 0.2s',
            }}
            title="Configure data source"
          >
            <Settings size={10} />
          </button>
        </div>
      </div>

      {/* Configuration Panel */}
      {showSettings ? (
        <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG }}>
          {/* Connection Selector */}
          <div style={{ marginBottom: '12px' }}>
            <label
              style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}
            >
              DATA SOURCE CONNECTION
            </label>

            {loadState === 'loading' ? (
              <div
                style={{
                  padding: '8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  color: FINCEPT.GRAY,
                  fontSize: '10px',
                  textAlign: 'center',
                }}
              >
                Loading connections...
              </div>
            ) : availableSources.length === 0 ? (
              <div
                style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  textAlign: 'center',
                }}
              >
                <AlertCircle size={16} color={FINCEPT.ORANGE} style={{ marginBottom: '8px' }} />
                <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginBottom: '8px' }}>
                  No data sources configured
                </div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
                  Go to <span style={{ color: FINCEPT.ORANGE }}>Data Sources</span> tab to add connections
                </div>
              </div>
            ) : (
              <select
                value={localConnection}
                onChange={(e) => setLocalConnection(e.target.value)}
                style={{
                  width: '100%',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  padding: '8px',
                  fontSize: '10px',
                  borderRadius: '2px',
                  cursor: 'pointer',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                <option value="">-- Select Connection --</option>
                {availableSources.map((conn) => (
                  <option key={conn.id} value={conn.id}>
                    {conn.name} ({conn.type}) {conn.status === 'connected' ? '[OK]' : ''}
                  </option>
                ))}
              </select>
            )}

            {/* Selected connection details */}
            {localConnection && selectedConnection && (
              <div
                style={{
                  marginTop: '8px',
                  padding: '8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ color: FINCEPT.CYAN, fontSize: '9px', textTransform: 'uppercase' }}>
                    {selectedConnection.type}
                  </span>
                  <span
                    style={{
                      padding: '2px 6px',
                      backgroundColor:
                        selectedConnection.status === 'connected' ? `${FINCEPT.GREEN}20` : `${FINCEPT.GRAY}20`,
                      color: selectedConnection.status === 'connected' ? FINCEPT.GREEN : FINCEPT.GRAY,
                      fontSize: '8px',
                      fontWeight: 700,
                      borderRadius: '2px',
                    }}
                  >
                    {selectedConnection.status.toUpperCase()}
                  </span>
                </div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '4px' }}>
                  {selectedConnection.category}
                </div>
              </div>
            )}
          </div>

          {/* Query Editor */}
          <div style={{ marginBottom: '12px' }}>
            <label
              style={{
                display: 'block',
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}
            >
              QUERY
            </label>
            <textarea
              value={localQuery}
              onChange={(e) => setLocalQuery(e.target.value)}
              placeholder="SELECT * FROM table WHERE condition"
              style={{
                width: '100%',
                height: '100px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.WHITE,
                padding: '8px',
                fontSize: '10px',
                borderRadius: '2px',
                resize: 'vertical',
                fontFamily: '"IBM Plex Mono", monospace',
                lineHeight: '1.5',
                boxSizing: 'border-box',
              }}
            />
          </div>

          {/* Action Buttons */}
          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={handleSaveSettings}
              disabled={!localConnection || !localQuery.trim()}
              style={{
                flex: 1,
                backgroundColor: !localConnection || !localQuery.trim() ? FINCEPT.BORDER : FINCEPT.GREEN,
                color: FINCEPT.DARK_BG,
                border: 'none',
                padding: '8px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: !localConnection || !localQuery.trim() ? 'not-allowed' : 'pointer',
                borderRadius: '2px',
                opacity: !localConnection || !localQuery.trim() ? 0.5 : 1,
              }}
            >
              SAVE CONFIG
            </button>
            <button
              onClick={() => setShowSettings(false)}
              style={{
                backgroundColor: 'transparent',
                color: FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`,
                padding: '8px 12px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                borderRadius: '2px',
              }}
            >
              CANCEL
            </button>
          </div>
        </div>
      ) : (
        /* Summary View */
        <div style={{ padding: '12px' }}>
          {/* Connection Info */}
          {selectedConnection ? (
            <div
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '8px',
                marginBottom: '8px',
              }}
            >
              <div
                style={{
                  color: FINCEPT.ORANGE,
                  fontSize: '9px',
                  fontWeight: 700,
                  marginBottom: '4px',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                }}
              >
                CONNECTION
              </div>
              <div style={{ color: FINCEPT.WHITE, fontSize: '10px', marginBottom: '2px' }}>
                {selectedConnection.name}
              </div>
              <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                <span style={{ color: FINCEPT.CYAN, fontSize: '9px' }}>{selectedConnection.type}</span>
                <span
                  style={{
                    padding: '1px 4px',
                    backgroundColor:
                      selectedConnection.status === 'connected' ? `${FINCEPT.GREEN}20` : `${FINCEPT.GRAY}20`,
                    color: selectedConnection.status === 'connected' ? FINCEPT.GREEN : FINCEPT.GRAY,
                    fontSize: '8px',
                    borderRadius: '2px',
                  }}
                >
                  {selectedConnection.status.toUpperCase()}
                </span>
              </div>
            </div>
          ) : (
            <div
              style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '8px',
                marginBottom: '8px',
                textAlign: 'center',
                color: FINCEPT.GRAY,
                fontSize: '10px',
              }}
            >
              No connection selected
            </div>
          )}

          {/* Query Preview */}
          {data.query ? (
            <div
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '8px',
                marginBottom: '8px',
              }}
            >
              <div
                style={{
                  color: FINCEPT.ORANGE,
                  fontSize: '9px',
                  fontWeight: 700,
                  marginBottom: '4px',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                }}
              >
                QUERY
              </div>
              <div
                style={{
                  color: FINCEPT.WHITE,
                  fontSize: '9px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  lineHeight: '1.4',
                  maxHeight: '60px',
                  overflow: 'auto',
                  whiteSpace: 'pre-wrap',
                  wordBreak: 'break-word',
                }}
              >
                {data.query}
              </div>
            </div>
          ) : (
            <div
              style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '8px',
                marginBottom: '8px',
                textAlign: 'center',
                color: FINCEPT.GRAY,
                fontSize: '10px',
              }}
            >
              No query defined
            </div>
          )}

          {/* Execute Button */}
          <button
            onClick={data.onExecute || handleExecuteQuery}
            disabled={!isConfigured || data.status === 'running' || isExecuting}
            style={{
              width: '100%',
              backgroundColor: !isConfigured || data.status === 'running' || isExecuting ? FINCEPT.BORDER : FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              padding: '8px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: !isConfigured || data.status === 'running' || isExecuting ? 'not-allowed' : 'pointer',
              borderRadius: '2px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              opacity: !isConfigured || data.status === 'running' || isExecuting ? 0.5 : 1,
            }}
          >
            {data.status === 'running' || isExecuting ? (
              <>
                <RefreshCw size={10} style={{ animation: 'spin 1s linear infinite' }} />
                EXECUTING...
              </>
            ) : (
              <>
                <Play size={10} />
                EXECUTE QUERY
              </>
            )}
          </button>

          {/* Result Display */}
          {data.status === 'completed' && data.result && (
            <div
              style={{
                marginTop: '8px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.GREEN}`,
                borderRadius: '2px',
                padding: '8px',
              }}
            >
              <div style={{ color: FINCEPT.GREEN, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                QUERY SUCCESSFUL
              </div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
                {Array.isArray(data.result) ? `${data.result.length} rows` : data.result.rowCount || 0} returned
              </div>
            </div>
          )}

          {/* Error Display */}
          {data.status === 'error' && data.error && (
            <div
              style={{
                marginTop: '8px',
                backgroundColor: `${FINCEPT.RED}10`,
                border: `1px solid ${FINCEPT.RED}`,
                borderRadius: '2px',
                padding: '8px',
              }}
            >
              <div style={{ color: FINCEPT.RED, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>ERROR</div>
              <div style={{ color: FINCEPT.WHITE, fontSize: '9px', lineHeight: '1.4', wordBreak: 'break-word' }}>
                {data.error}
              </div>
            </div>
          )}
        </div>
      )}

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
};

export default DataSourceNode;
