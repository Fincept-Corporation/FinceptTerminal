import React from 'react';
import { X, RefreshCw, Settings } from 'lucide-react';
import { useTranslation } from 'react-i18next';

export interface BaseWidgetProps {
  id: string;
  title: string;
  onRemove?: () => void;
  onRefresh?: () => void;
  onConfigure?: () => void;
  children: React.ReactNode;
  isLoading?: boolean;
  error?: string | null;
  headerColor?: string;
}

export const BaseWidget: React.FC<BaseWidgetProps> = ({
  title,
  onRemove,
  onRefresh,
  onConfigure,
  children,
  isLoading = false,
  error = null,
  headerColor
}) => {
  const { t } = useTranslation('dashboard');
  return (
    <div style={{
      height: '100%',
      width: '100%',
      backgroundColor: 'var(--ft-color-background)',
      border: 'var(--ft-border-width) solid var(--ft-border-color)',
      borderRadius: 'var(--ft-border-radius)',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      opacity: 'var(--ft-widget-opacity)'
    }}>
      {/* Widget Header */}
      <div style={{
        backgroundColor: 'var(--ft-color-panel)',
        borderBottom: 'var(--ft-border-width) solid var(--ft-border-color)',
        padding: '4px 8px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        minHeight: '28px',
        flexShrink: 0
      }}>
        <div
          className="widget-drag-handle"
          style={{
            color: headerColor || 'var(--ft-color-primary)',
            fontSize: 'var(--ft-font-size-body)',
            fontWeight: 'bold',
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
            cursor: 'move',
            flex: 1,
            userSelect: 'none'
          }}
        >
          {title}
        </div>
        <div style={{ display: 'flex', gap: '4px', flexShrink: 0 }}>
          {onConfigure && (
            <button
              onClick={onConfigure}
              style={{
                background: 'none',
                border: 'none',
                color: 'var(--ft-color-text-muted)',
                cursor: 'pointer',
                padding: '2px',
                display: 'flex',
                alignItems: 'center'
              }}
              title="Configure"
            >
              <Settings size={12} />
            </button>
          )}
          {onRefresh && (
            <button
              onClick={onRefresh}
              disabled={isLoading}
              style={{
                background: 'none',
                border: 'none',
                color: isLoading ? 'var(--ft-color-text-muted)' : 'var(--ft-color-text)',
                cursor: isLoading ? 'not-allowed' : 'pointer',
                padding: '2px',
                display: 'flex',
                alignItems: 'center'
              }}
              title="Refresh"
            >
              <RefreshCw size={12} style={{ animation: isLoading ? 'spin 1s linear infinite' : 'none' }} />
            </button>
          )}
          {onRemove && (
            <button
              onClick={onRemove}
              style={{
                background: 'none',
                border: 'none',
                color: 'var(--ft-color-text-muted)',
                cursor: 'pointer',
                padding: '2px',
                display: 'flex',
                alignItems: 'center'
              }}
              title="Remove"
            >
              <X size={12} />
            </button>
          )}
        </div>
      </div>

      {/* Widget Content */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        position: 'relative'
      }}>
        {error ? (
          <div style={{
            padding: '12px',
            color: 'var(--ft-color-alert)',
            fontSize: 'var(--ft-font-size-small)',
            textAlign: 'center'
          }}>
            {t('widgets.error')}: {error}
          </div>
        ) : isLoading ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            gap: '8px'
          }}>
            <div style={{
              width: '30px',
              height: '30px',
              border: '3px solid var(--ft-color-text-muted)',
              borderTop: '3px solid var(--ft-color-primary)',
              borderRadius: '50%',
              animation: 'spin 1s linear infinite'
            }} />
            <div style={{
              color: 'var(--ft-color-text-muted)',
              fontSize: 'var(--ft-font-size-tiny)',
              textAlign: 'center'
            }}>
              {t('widgets.loading')}
            </div>
          </div>
        ) : (
          children
        )}
      </div>
    </div>
  );
};
