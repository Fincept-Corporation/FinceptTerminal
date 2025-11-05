import React, { useState } from 'react';
import { Handle, Position } from 'reactflow';
import { Database, Play, Settings, Check, AlertCircle } from 'lucide-react';

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
  };
  selected: boolean;
}

// Mock available database connections (in real app, fetch from settings/database)
const MOCK_CONNECTIONS = [
  { id: 'postgres_1', name: 'PostgreSQL - Production', type: 'PostgreSQL' },
  { id: 'mysql_1', name: 'MySQL - Analytics', type: 'MySQL' },
  { id: 'mongodb_1', name: 'MongoDB - Orders', type: 'MongoDB' },
  { id: 'sqlite_1', name: 'SQLite - Local', type: 'SQLite' },
];

const DataSourceNode: React.FC<DataSourceNodeProps> = ({ data, selected }) => {
  const [showSettings, setShowSettings] = useState(false);
  const [localQuery, setLocalQuery] = useState(data.query || '');
  const [localConnection, setLocalConnection] = useState(data.selectedConnectionId || '');

  const handleSaveSettings = () => {
    if (localConnection) {
      data.onConnectionChange(localConnection);
    }
    if (localQuery) {
      data.onQueryChange(localQuery);
    }
    setShowSettings(false);
  };

  const getStatusColor = () => {
    switch (data.status) {
      case 'running': return '#3b82f6';
      case 'completed': return '#10b981';
      case 'error': return '#ef4444';
      default: return '#6b7280';
    }
  };

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return '🔄';
      case 'completed': return <Check size={14} color="#10b981" />;
      case 'error': return <AlertCircle size={14} color="#ef4444" />;
      default: return <Database size={14} color="#6b7280" />;
    }
  };

  const selectedConnection = MOCK_CONNECTIONS.find(c => c.id === (localConnection || data.selectedConnectionId));
  const isConfigured = data.selectedConnectionId && data.query;

  return (
    <div style={{
      background: selected ? '#252525' : '#1a1a1a',
      border: `2px solid ${selected ? '#3b82f6' : getStatusColor()}`,
      borderRadius: '8px',
      minWidth: '280px',
      maxWidth: '400px',
      fontFamily: 'Consolas, monospace',
      boxShadow: selected ? `0 0 16px #3b82f660` : '0 2px 8px rgba(0,0,0,0.3)',
      position: 'relative'
    }}>
      {/* Output Handle */}
      <Handle
        type="source"
        position={Position.Right}
        style={{
          background: '#3b82f6',
          width: '12px',
          height: '12px',
          border: '2px solid #0a0a0a',
          right: '-6px'
        }}
      />

      {/* Header */}
      <div style={{
        backgroundColor: '#0f0f0f',
        padding: '10px 12px',
        borderTopLeftRadius: '6px',
        borderTopRightRadius: '6px',
        borderBottom: '1px solid #2d2d2d',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <Database size={16} color="#3b82f6" />
          <span style={{
            color: '#fff',
            fontSize: '12px',
            fontWeight: 'bold'
          }}>
            {data.label}
          </span>
        </div>
        <div style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
          {getStatusIcon()}
          <button
            onClick={() => setShowSettings(!showSettings)}
            style={{
              backgroundColor: showSettings ? '#3b82f6' : 'transparent',
              border: `1px solid ${showSettings ? '#3b82f6' : '#404040'}`,
              color: showSettings ? '#fff' : '#6b7280',
              padding: '4px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              transition: 'all 0.2s'
            }}
            title="Configure data source"
          >
            <Settings size={12} />
          </button>
        </div>
      </div>

      {/* Configuration Panel */}
      {showSettings ? (
        <div style={{
          padding: '12px',
          backgroundColor: '#0a0a0a'
        }}>
          {/* Connection Selector */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: '#a3a3a3',
              fontSize: '10px',
              fontWeight: 'bold',
              marginBottom: '6px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px'
            }}>
              Database Connection
            </label>
            <select
              value={localConnection}
              onChange={(e) => setLocalConnection(e.target.value)}
              style={{
                width: '100%',
                backgroundColor: '#1a1a1a',
                border: '1px solid #404040',
                color: '#fff',
                padding: '8px',
                fontSize: '11px',
                borderRadius: '4px',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}
            >
              <option value="">-- Select Connection --</option>
              {MOCK_CONNECTIONS.map(conn => (
                <option key={conn.id} value={conn.id}>
                  {conn.name} ({conn.type})
                </option>
              ))}
            </select>
          </div>

          {/* Query Editor */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: '#a3a3a3',
              fontSize: '10px',
              fontWeight: 'bold',
              marginBottom: '6px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px'
            }}>
              SQL Query
            </label>
            <textarea
              value={localQuery}
              onChange={(e) => setLocalQuery(e.target.value)}
              placeholder="SELECT * FROM table WHERE condition"
              style={{
                width: '100%',
                height: '100px',
                backgroundColor: '#1a1a1a',
                border: '1px solid #404040',
                color: '#fff',
                padding: '8px',
                fontSize: '10px',
                borderRadius: '4px',
                resize: 'vertical',
                fontFamily: 'Consolas, monospace',
                lineHeight: '1.5'
              }}
            />
          </div>

          {/* Action Buttons */}
          <div style={{
            display: 'flex',
            gap: '8px'
          }}>
            <button
              onClick={handleSaveSettings}
              disabled={!localConnection || !localQuery.trim()}
              style={{
                flex: 1,
                backgroundColor: (!localConnection || !localQuery.trim()) ? '#404040' : '#10b981',
                color: '#fff',
                border: 'none',
                padding: '8px',
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: (!localConnection || !localQuery.trim()) ? 'not-allowed' : 'pointer',
                borderRadius: '4px',
                opacity: (!localConnection || !localQuery.trim()) ? 0.5 : 1
              }}
            >
              SAVE CONFIG
            </button>
            <button
              onClick={() => setShowSettings(false)}
              style={{
                backgroundColor: 'transparent',
                color: '#6b7280',
                border: '1px solid #404040',
                padding: '8px 12px',
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: 'pointer',
                borderRadius: '4px'
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
            <div style={{
              backgroundColor: '#0f0f0f',
              border: '1px solid #2d2d2d',
              borderRadius: '4px',
              padding: '8px',
              marginBottom: '8px'
            }}>
              <div style={{
                color: '#3b82f6',
                fontSize: '9px',
                fontWeight: 'bold',
                marginBottom: '4px',
                textTransform: 'uppercase',
                letterSpacing: '0.5px'
              }}>
                Connection
              </div>
              <div style={{
                color: '#fff',
                fontSize: '11px',
                marginBottom: '2px'
              }}>
                {selectedConnection.name}
              </div>
              <div style={{
                color: '#6b7280',
                fontSize: '9px'
              }}>
                {selectedConnection.type}
              </div>
            </div>
          ) : (
            <div style={{
              backgroundColor: '#1a1a1a',
              border: '1px solid #404040',
              borderRadius: '4px',
              padding: '8px',
              marginBottom: '8px',
              textAlign: 'center',
              color: '#6b7280',
              fontSize: '10px'
            }}>
              No connection selected
            </div>
          )}

          {/* Query Preview */}
          {data.query ? (
            <div style={{
              backgroundColor: '#0f0f0f',
              border: '1px solid #2d2d2d',
              borderRadius: '4px',
              padding: '8px',
              marginBottom: '8px'
            }}>
              <div style={{
                color: '#3b82f6',
                fontSize: '9px',
                fontWeight: 'bold',
                marginBottom: '4px',
                textTransform: 'uppercase',
                letterSpacing: '0.5px'
              }}>
                Query
              </div>
              <div style={{
                color: '#d4d4d4',
                fontSize: '9px',
                fontFamily: 'Consolas, monospace',
                lineHeight: '1.4',
                maxHeight: '60px',
                overflow: 'auto',
                whiteSpace: 'pre-wrap',
                wordBreak: 'break-word'
              }}>
                {data.query}
              </div>
            </div>
          ) : (
            <div style={{
              backgroundColor: '#1a1a1a',
              border: '1px solid #404040',
              borderRadius: '4px',
              padding: '8px',
              marginBottom: '8px',
              textAlign: 'center',
              color: '#6b7280',
              fontSize: '10px'
            }}>
              No query defined
            </div>
          )}

          {/* Execute Button */}
          <button
            onClick={data.onExecute}
            disabled={!isConfigured || data.status === 'running'}
            style={{
              width: '100%',
              backgroundColor: (!isConfigured || data.status === 'running') ? '#404040' : '#3b82f6',
              color: '#fff',
              border: 'none',
              padding: '8px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: (!isConfigured || data.status === 'running') ? 'not-allowed' : 'pointer',
              borderRadius: '4px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              opacity: (!isConfigured || data.status === 'running') ? 0.5 : 1
            }}
          >
            <Play size={12} />
            {data.status === 'running' ? 'EXECUTING...' : 'EXECUTE QUERY'}
          </button>

          {/* Result/Error Display */}
          {data.status === 'completed' && data.result && (
            <div style={{
              marginTop: '8px',
              backgroundColor: '#0f0f0f',
              border: '1px solid #10b981',
              borderRadius: '4px',
              padding: '8px'
            }}>
              <div style={{
                color: '#10b981',
                fontSize: '9px',
                fontWeight: 'bold',
                marginBottom: '4px'
              }}>
                ✓ Query Successful
              </div>
              <div style={{
                color: '#6b7280',
                fontSize: '9px'
              }}>
                {data.result.rowCount || 0} rows returned
              </div>
            </div>
          )}

          {data.status === 'error' && data.error && (
            <div style={{
              marginTop: '8px',
              backgroundColor: '#1a0a0a',
              border: '1px solid #ef4444',
              borderRadius: '4px',
              padding: '8px'
            }}>
              <div style={{
                color: '#ef4444',
                fontSize: '9px',
                fontWeight: 'bold',
                marginBottom: '4px'
              }}>
                ✗ Error
              </div>
              <div style={{
                color: '#d4d4d4',
                fontSize: '9px',
                lineHeight: '1.4',
                wordBreak: 'break-word'
              }}>
                {data.error}
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default DataSourceNode;
