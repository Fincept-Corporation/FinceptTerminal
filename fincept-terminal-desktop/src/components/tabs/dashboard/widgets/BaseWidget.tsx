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

const FINCEPT_ORANGE = '#FFA500';
const FINCEPT_WHITE = '#FFFFFF';
const FINCEPT_RED = '#FF0000';
const FINCEPT_GRAY = '#787878';
const FINCEPT_PANEL_BG = '#000000';

export const BaseWidget: React.FC<BaseWidgetProps> = ({
  title,
  onRemove,
  onRefresh,
  onConfigure,
  children,
  isLoading = false,
  error = null,
  headerColor = FINCEPT_ORANGE
}) => {
  const { t } = useTranslation('dashboard');
  return (
    <div style={{
      height: '100%',
      width: '100%',
      backgroundColor: FINCEPT_PANEL_BG,
      border: `1px solid ${FINCEPT_GRAY}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Widget Header */}
      <div style={{
        backgroundColor: '#1a1a1a',
        borderBottom: `1px solid ${FINCEPT_GRAY}`,
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
            color: headerColor,
            fontSize: '11px',
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
                color: FINCEPT_GRAY,
                cursor: 'pointer',
                padding: '2px',
                display: 'flex',
                alignItems: 'center'
              }}
              onMouseEnter={(e) => e.currentTarget.style.color = FINCEPT_WHITE}
              onMouseLeave={(e) => e.currentTarget.style.color = FINCEPT_GRAY}
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
                color: isLoading ? FINCEPT_GRAY : FINCEPT_WHITE,
                cursor: isLoading ? 'not-allowed' : 'pointer',
                padding: '2px',
                display: 'flex',
                alignItems: 'center'
              }}
              onMouseEnter={(e) => !isLoading && (e.currentTarget.style.color = headerColor)}
              onMouseLeave={(e) => !isLoading && (e.currentTarget.style.color = FINCEPT_WHITE)}
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
                color: FINCEPT_GRAY,
                cursor: 'pointer',
                padding: '2px',
                display: 'flex',
                alignItems: 'center'
              }}
              onMouseEnter={(e) => e.currentTarget.style.color = FINCEPT_RED}
              onMouseLeave={(e) => e.currentTarget.style.color = FINCEPT_GRAY}
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
            color: FINCEPT_RED,
            fontSize: '10px',
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
              border: '3px solid #404040',
              borderTop: '3px solid #ea580c',
              borderRadius: '50%',
              animation: 'spin 1s linear infinite'
            }} />
            <div style={{
              color: FINCEPT_GRAY,
              fontSize: '9px',
              textAlign: 'center'
            }}>
              {t('widgets.loading')}
            </div>
          </div>
        ) : (
          children
        )}
      </div>

      {/* Global spin animation */}
      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
};
