import React, { useState } from 'react';
import { Position } from 'reactflow';
import { Database, Play, Settings, Check, AlertCircle, RefreshCw } from 'lucide-react';
import { useDataSources } from '../../../../contexts/DataSourceContext';
import { createAdapter } from '../../data-sources/adapters';
import {
  FINCEPT,
  SPACING,
  FONT_FAMILY,
  BORDER_RADIUS,
  BaseNode,
  NodeHeader,
  IconButton,
  Button,
  SelectField,
  TextareaField,
  InfoPanel,
  StatusPanel,
  EmptyState,
  KeyValue,
  SettingsPanel,
  Tag,
  getStatusColor,
} from './shared';

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

  const statusColor = getStatusColor(data.status);

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
    <BaseNode
      selected={selected}
      minWidth="280px"
      maxWidth="400px"
      borderColor={statusColor}
      handles={[
        { type: 'source', position: Position.Right, color: FINCEPT.ORANGE },
      ]}
    >
      {/* Header */}
      <NodeHeader
        icon={<Database size={14} />}
        title={data.label}
        color={FINCEPT.ORANGE}
        rightActions={
          <>
            {getStatusIcon()}
            <IconButton
              icon={<Settings size={10} />}
              onClick={() => setShowSettings(!showSettings)}
              active={showSettings}
              title="Configure data source"
            />
          </>
        }
      />

      {/* Configuration Panel */}
      <SettingsPanel isOpen={showSettings}>
        {/* Connection Selector */}
        <div style={{ marginBottom: SPACING.LG }}>
          <label
            style={{
              display: 'block',
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              marginBottom: SPACING.SM,
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
          >
            DATA SOURCE CONNECTION
          </label>

          {loadState === 'loading' ? (
            <div
              style={{
                padding: SPACING.MD,
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: BORDER_RADIUS.SM,
                color: FINCEPT.GRAY,
                fontSize: '10px',
                textAlign: 'center',
              }}
            >
              Loading connections...
            </div>
          ) : availableSources.length === 0 ? (
            <EmptyState
              icon={<AlertCircle size={16} />}
              title="No data sources configured"
              subtitle='Go to Data Sources tab to add connections'
            />
          ) : (
            <select
              value={localConnection}
              onChange={(e) => setLocalConnection(e.target.value)}
              style={{
                width: '100%',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.WHITE,
                padding: SPACING.MD,
                fontSize: '10px',
                borderRadius: BORDER_RADIUS.SM,
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
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
                marginTop: SPACING.MD,
                padding: SPACING.MD,
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: BORDER_RADIUS.SM,
              }}
            >
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <Tag label={selectedConnection.type} color={FINCEPT.CYAN} />
                <Tag
                  label={selectedConnection.status}
                  color={selectedConnection.status === 'connected' ? FINCEPT.GREEN : FINCEPT.GRAY}
                />
              </div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: SPACING.XS }}>
                {selectedConnection.category}
              </div>
            </div>
          )}
        </div>

        {/* Query Editor */}
        <TextareaField
          label="QUERY"
          value={localQuery}
          onChange={setLocalQuery}
          placeholder="SELECT * FROM table WHERE condition"
          minHeight="100px"
        />

        {/* Action Buttons */}
        <div style={{ display: 'flex', gap: SPACING.MD }}>
          <Button
            label="SAVE CONFIG"
            onClick={handleSaveSettings}
            variant="primary"
            disabled={!localConnection || !localQuery.trim()}
            fullWidth
          />
          <Button
            label="CANCEL"
            onClick={() => setShowSettings(false)}
            variant="secondary"
          />
        </div>
      </SettingsPanel>

      {/* Summary View */}
      {!showSettings && (
        <div style={{ padding: SPACING.LG }}>
          {/* Connection Info */}
          {selectedConnection ? (
            <InfoPanel title="CONNECTION">
              <div style={{ color: FINCEPT.WHITE, fontSize: '10px', marginBottom: '2px' }}>
                {selectedConnection.name}
              </div>
              <div style={{ display: 'flex', gap: SPACING.MD, alignItems: 'center' }}>
                <Tag label={selectedConnection.type} color={FINCEPT.CYAN} />
                <Tag
                  label={selectedConnection.status}
                  color={selectedConnection.status === 'connected' ? FINCEPT.GREEN : FINCEPT.GRAY}
                />
              </div>
            </InfoPanel>
          ) : (
            <div
              style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: BORDER_RADIUS.SM,
                padding: SPACING.MD,
                marginBottom: SPACING.MD,
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
            <InfoPanel title="QUERY">
              <div
                style={{
                  color: FINCEPT.WHITE,
                  fontSize: '9px',
                  fontFamily: FONT_FAMILY,
                  lineHeight: '1.4',
                  maxHeight: '60px',
                  overflow: 'auto',
                  whiteSpace: 'pre-wrap',
                  wordBreak: 'break-word',
                }}
              >
                {data.query}
              </div>
            </InfoPanel>
          ) : (
            <div
              style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: BORDER_RADIUS.SM,
                padding: SPACING.MD,
                marginBottom: SPACING.MD,
                textAlign: 'center',
                color: FINCEPT.GRAY,
                fontSize: '10px',
              }}
            >
              No query defined
            </div>
          )}

          {/* Execute Button */}
          <Button
            label={data.status === 'running' || isExecuting ? 'EXECUTING...' : 'EXECUTE QUERY'}
            icon={data.status === 'running' || isExecuting ? <RefreshCw size={10} style={{ animation: 'spin 1s linear infinite' }} /> : <Play size={10} />}
            onClick={data.onExecute || handleExecuteQuery}
            variant="primary"
            disabled={!isConfigured || data.status === 'running' || isExecuting}
            fullWidth
          />

          {/* Result Display */}
          {data.status === 'completed' && data.result && (
            <StatusPanel
              type="success"
              icon={<Check size={12} />}
              message={`Query successful - ${Array.isArray(data.result) ? `${data.result.length} rows` : data.result.rowCount || 0} returned`}
            />
          )}

          {/* Error Display */}
          {data.status === 'error' && data.error && (
            <StatusPanel
              type="error"
              icon={<AlertCircle size={12} />}
              message={data.error}
            />
          )}
        </div>
      )}

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </BaseNode>
  );
};

export default DataSourceNode;
