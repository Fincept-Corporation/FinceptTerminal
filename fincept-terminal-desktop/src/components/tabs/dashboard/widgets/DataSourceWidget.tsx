import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { useDataSource } from '@/hooks/useDataSource';
import { Database, Wifi, AlertCircle } from 'lucide-react';

interface DataSourceWidgetProps {
  id: string;
  alias: string;
  displayName?: string;
  onRemove?: () => void;
}

export const DataSourceWidget: React.FC<DataSourceWidgetProps> = ({
  id,
  alias,
  displayName,
  onRemove
}) => {
  const { t } = useTranslation('dashboard');
  const { dataSources, loading, error, refreshDataSources } = useDataSource();
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);
  const [updateCount, setUpdateCount] = useState(0);
  const [isBlinking, setIsBlinking] = useState(false);

  // Find the specific data source by alias
  const source = dataSources.find(ds => ds.alias === alias);
  const connected = source?.enabled || false;
  const data = null; // Data will be handled separately

  // Log data updates for debugging
  useEffect(() => {
    if (source) {
      console.log(`[DataSourceWidget ${alias}] Data source:`, source);
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
    if (value === null || value === undefined) return <span style={{ color: 'var(--ft-color-text-muted)' }}>N/A</span>;

    if (typeof value === 'object') {
      return (
        <div style={{ fontSize: 'var(--ft-font-size-tiny)', fontFamily: 'monospace' }}>
          {Object.entries(value).slice(0, 10).map(([key, val]) => (
            <div key={key} style={{ padding: '2px 0', borderBottom: '1px solid var(--ft-border-color)' }}>
              <span style={{ color: 'var(--ft-color-primary)' }}>{key}:</span>{' '}
              <span style={{ color: 'var(--ft-color-text)' }}>
                {typeof val === 'object' ? JSON.stringify(val).substring(0, 50) : String(val)}
              </span>
            </div>
          ))}
        </div>
      );
    }

    return <span style={{ color: 'var(--ft-color-text)' }}>{String(value)}</span>;
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
          <AlertCircle size={32} color="var(--ft-color-alert)" />
          <div style={{ color: 'var(--ft-color-alert)', fontSize: 'var(--ft-font-size-body)', marginTop: '8px' }}>
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
          <div style={{ color: 'var(--ft-color-primary)', fontSize: 'var(--ft-font-size-small)' }}>
            {t('widgets.loading')}
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
          color: 'var(--ft-color-text-muted)',
          fontSize: 'var(--ft-font-size-small)'
        }}>
          <Database size={24} />
          <div>{t('widgets.noData')}</div>
          {source?.type === 'rest_api' && (
            <button
              onClick={refreshDataSources}
              style={{
                background: 'var(--ft-color-primary)',
                color: '#000',
                border: 'none',
                padding: '6px 12px',
                fontSize: 'var(--ft-font-size-tiny)',
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
      <div style={{ padding: '8px', fontSize: 'var(--ft-font-size-small)' }}>
        {/* Status Bar */}
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '8px',
          padding: '4px 8px',
          backgroundColor: isBlinking ? 'var(--ft-color-panel)' : 'var(--ft-color-panel)',
          border: `1px solid ${connected ? 'var(--ft-color-success)' : 'var(--ft-color-text-muted)'}`,
          fontSize: 'var(--ft-font-size-tiny)',
          transition: 'background-color 0.3s ease'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            {source?.type === 'websocket' ? (
              <Wifi size={12} color={connected ? 'var(--ft-color-success)' : 'var(--ft-color-text-muted)'} />
            ) : (
              <Database size={12} color="var(--ft-color-primary)" />
            )}
            <span style={{ color: 'var(--ft-color-text)' }}>
              {source?.type === 'websocket' ? 'WebSocket' : 'REST API'}
            </span>
            <span style={{ color: connected ? 'var(--ft-color-success)' : 'var(--ft-color-text-muted)' }}>
              {connected ? '● LIVE' : '○ IDLE'}
            </span>
            {updateCount > 0 && (
              <span style={{ color: 'var(--ft-color-primary)', fontSize: 'var(--ft-font-size-tiny)' }}>
                #{updateCount}
              </span>
            )}
          </div>

          <div style={{ color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-tiny)' }}>
            {lastUpdate && (
              <span>{lastUpdate.toLocaleTimeString()}</span>
            )}
          </div>
        </div>

        {/* Data Display */}
        <div style={{
          maxHeight: 'calc(100% - 60px)',
          overflowY: 'auto',
          backgroundColor: 'var(--ft-color-panel)',
          border: '1px solid var(--ft-border-color)',
          padding: '8px'
        }}>
          {renderDataValue(data)}
        </div>

        {/* Actions */}
        {source?.type === 'rest_api' && (
          <div style={{ marginTop: '8px', textAlign: 'center' }}>
            <button
              onClick={refreshDataSources}
              disabled={loading}
              style={{
                background: loading ? 'var(--ft-color-text-muted)' : 'var(--ft-color-primary)',
                color: '#000',
                border: 'none',
                padding: '4px 12px',
                fontSize: 'var(--ft-font-size-tiny)',
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
      onRefresh={source?.type === 'rest_api' ? refreshDataSources : undefined}
      isLoading={loading && !data}
      error={null}
      headerColor={source?.type === 'websocket' ? 'var(--ft-color-success)' : 'var(--ft-color-primary)'}
    >
      {renderContent()}
    </BaseWidget>
  );
};
