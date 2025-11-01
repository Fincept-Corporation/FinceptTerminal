import React, { useState, useEffect } from 'react';
import { BaseWidget } from './BaseWidget';
import { useDataSource } from '@/hooks/useDataSource';
import { Database, Wifi, AlertCircle } from 'lucide-react';

interface DataSourceWidgetProps {
  id: string;
  alias: string;
  displayName?: string;
  onRemove?: () => void;
}

const BLOOMBERG_ORANGE = '#FFA500';
const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GREEN = '#00ff00';
const BLOOMBERG_RED = '#ff0000';
const BLOOMBERG_GRAY = '#787878';

export const DataSourceWidget: React.FC<DataSourceWidgetProps> = ({
  id,
  alias,
  displayName,
  onRemove
}) => {
  const { data, error, loading, connected, source, refresh } = useDataSource(alias);
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);
  const [updateCount, setUpdateCount] = useState(0);
  const [isBlinking, setIsBlinking] = useState(false);

  // Log data updates for debugging
  useEffect(() => {
    if (data) {
      console.log(`[DataSourceWidget ${alias}] Data update #${updateCount}:`, data);
    }
  }, [data, alias, updateCount]);

  // Update last update time when data changes
  useEffect(() => {
    if (data && !loading) {
      setLastUpdate(new Date());
      setUpdateCount(prev => prev + 1);

      // Blink effect on update
      setIsBlinking(true);
      const timer = setTimeout(() => setIsBlinking(false), 300);
      return () => clearTimeout(timer);
    }
  }, [data, loading]);

  const renderDataValue = (value: any): React.ReactNode => {
    if (value === null || value === undefined) return <span style={{ color: BLOOMBERG_GRAY }}>N/A</span>;

    if (typeof value === 'object') {
      return (
        <div style={{ fontSize: '9px', fontFamily: 'monospace' }}>
          {Object.entries(value).slice(0, 10).map(([key, val]) => (
            <div key={key} style={{ padding: '2px 0', borderBottom: `1px solid #1a1a1a` }}>
              <span style={{ color: BLOOMBERG_ORANGE }}>{key}:</span>{' '}
              <span style={{ color: BLOOMBERG_WHITE }}>
                {typeof val === 'object' ? JSON.stringify(val).substring(0, 50) : String(val)}
              </span>
            </div>
          ))}
        </div>
      );
    }

    return <span style={{ color: BLOOMBERG_WHITE }}>{String(value)}</span>;
  };

  const renderContent = () => {
    if (error) {
      return (
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
          padding: '20px',
          textAlign: 'center'
        }}>
          <AlertCircle size={32} color={BLOOMBERG_RED} />
          <div style={{ color: BLOOMBERG_RED, fontSize: '11px', marginTop: '8px' }}>
            {error}
          </div>
        </div>
      );
    }

    if (loading && !data) {
      return (
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
          gap: '8px'
        }}>
          <div style={{ color: BLOOMBERG_ORANGE, fontSize: '10px' }}>
            Loading data source...
          </div>
        </div>
      );
    }

    if (!data) {
      return (
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
          gap: '8px',
          color: BLOOMBERG_GRAY,
          fontSize: '10px'
        }}>
          <Database size={24} />
          <div>No data available yet</div>
          {source?.type === 'rest_api' && (
            <button
              onClick={refresh}
              style={{
                background: BLOOMBERG_ORANGE,
                color: '#000',
                border: 'none',
                padding: '6px 12px',
                fontSize: '9px',
                fontWeight: 'bold',
                cursor: 'pointer',
                marginTop: '8px'
              }}
            >
              LOAD DATA
            </button>
          )}
        </div>
      );
    }

    return (
      <div style={{ padding: '8px', fontSize: '10px' }}>
        {/* Status Bar */}
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '8px',
          padding: '4px 8px',
          backgroundColor: isBlinking ? '#1a1a1a' : '#0a0a0a',
          border: `1px solid ${connected ? BLOOMBERG_GREEN : BLOOMBERG_GRAY}`,
          fontSize: '9px',
          transition: 'background-color 0.3s ease'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            {source?.type === 'websocket' ? (
              <Wifi size={12} color={connected ? BLOOMBERG_GREEN : BLOOMBERG_GRAY} />
            ) : (
              <Database size={12} color={BLOOMBERG_ORANGE} />
            )}
            <span style={{ color: BLOOMBERG_WHITE }}>
              {source?.type === 'websocket' ? 'WebSocket' : 'REST API'}
            </span>
            <span style={{ color: connected ? BLOOMBERG_GREEN : BLOOMBERG_GRAY }}>
              {connected ? '● LIVE' : '○ IDLE'}
            </span>
            {updateCount > 0 && (
              <span style={{ color: BLOOMBERG_ORANGE, fontSize: '8px' }}>
                #{updateCount}
              </span>
            )}
          </div>

          <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px' }}>
            {lastUpdate && (
              <span>{lastUpdate.toLocaleTimeString()}</span>
            )}
          </div>
        </div>

        {/* Data Display */}
        <div style={{
          maxHeight: 'calc(100% - 60px)',
          overflowY: 'auto',
          backgroundColor: '#050505',
          border: `1px solid #1a1a1a`,
          padding: '8px'
        }}>
          {renderDataValue(data)}
        </div>

        {/* Actions */}
        {source?.type === 'rest_api' && (
          <div style={{ marginTop: '8px', textAlign: 'center' }}>
            <button
              onClick={refresh}
              disabled={loading}
              style={{
                background: loading ? BLOOMBERG_GRAY : BLOOMBERG_ORANGE,
                color: '#000',
                border: 'none',
                padding: '4px 12px',
                fontSize: '9px',
                fontWeight: 'bold',
                cursor: loading ? 'not-allowed' : 'pointer',
                width: '100%'
              }}
            >
              {loading ? 'REFRESHING...' : 'REFRESH DATA'}
            </button>
          </div>
        )}
      </div>
    );
  };

  return (
    <BaseWidget
      id={id}
      title={displayName || source?.display_name || alias}
      onRemove={onRemove}
      onRefresh={source?.type === 'rest_api' ? refresh : undefined}
      isLoading={loading && !data}
      error={null}
      headerColor={source?.type === 'websocket' ? BLOOMBERG_GREEN : BLOOMBERG_ORANGE}
    >
      {renderContent()}
    </BaseWidget>
  );
};
