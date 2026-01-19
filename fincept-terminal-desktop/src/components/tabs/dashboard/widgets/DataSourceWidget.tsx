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

const FINCEPT_ORANGE = '#FFA500';
const FINCEPT_WHITE = '#FFFFFF';
const FINCEPT_GREEN = '#00ff00';
const FINCEPT_RED = '#ff0000';
const FINCEPT_GRAY = '#787878';

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
    if (value === null || value === undefined) return <span style={{ color: FINCEPT_GRAY }}>N/A</span>;

    if (typeof value === 'object') {
      return (
        <div style={{ fontSize: '9px', fontFamily: 'monospace' }}>
          {Object.entries(value).slice(0, 10).map(([key, val]) => (
            <div key={key} style={{ padding: '2px 0', borderBottom: `1px solid #1a1a1a` }}>
              <span style={{ color: FINCEPT_ORANGE }}>{key}:</span>{' '}
              <span style={{ color: FINCEPT_WHITE }}>
                {typeof val === 'object' ? JSON.stringify(val).substring(0, 50) : String(val)}
              </span>
            </div>
          ))}
        </div>
      );
    }

    return <span style={{ color: FINCEPT_WHITE }}>{String(value)}</span>;
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
          <AlertCircle size={32} color={FINCEPT_RED} />
          <div style={{ color: FINCEPT_RED, fontSize: '11px', marginTop: '8px' }}>
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
          <div style={{ color: FINCEPT_ORANGE, fontSize: '10px' }}>
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
          color: FINCEPT_GRAY,
          fontSize: '10px'
        }}>
          <Database size={24} />
          <div>{t('widgets.noData')}</div>
          {source?.type === 'rest_api' && (
            <button
              onClick={refreshDataSources}
              style={{
                background: FINCEPT_ORANGE,
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
          border: `1px solid ${connected ? FINCEPT_GREEN : FINCEPT_GRAY}`,
          fontSize: '9px',
          transition: 'background-color 0.3s ease'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            {source?.type === 'websocket' ? (
              <Wifi size={12} color={connected ? FINCEPT_GREEN : FINCEPT_GRAY} />
            ) : (
              <Database size={12} color={FINCEPT_ORANGE} />
            )}
            <span style={{ color: FINCEPT_WHITE }}>
              {source?.type === 'websocket' ? 'WebSocket' : 'REST API'}
            </span>
            <span style={{ color: connected ? FINCEPT_GREEN : FINCEPT_GRAY }}>
              {connected ? '● LIVE' : '○ IDLE'}
            </span>
            {updateCount > 0 && (
              <span style={{ color: FINCEPT_ORANGE, fontSize: '8px' }}>
                #{updateCount}
              </span>
            )}
          </div>

          <div style={{ color: FINCEPT_GRAY, fontSize: '8px' }}>
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
              onClick={refreshDataSources}
              disabled={loading}
              style={{
                background: loading ? FINCEPT_GRAY : FINCEPT_ORANGE,
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
      onRefresh={source?.type === 'rest_api' ? refreshDataSources : undefined}
      isLoading={loading && !data}
      error={null}
      headerColor={source?.type === 'websocket' ? FINCEPT_GREEN : FINCEPT_ORANGE}
    >
      {renderContent()}
    </BaseWidget>
  );
};
