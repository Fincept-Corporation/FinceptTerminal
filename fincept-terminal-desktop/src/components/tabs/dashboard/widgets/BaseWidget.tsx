import React from 'react';
import { X, RefreshCw, Settings } from 'lucide-react';

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

const BLOOMBERG_ORANGE = '#FFA500';
const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_RED = '#FF0000';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_PANEL_BG = '#000000';

export const BaseWidget: React.FC<BaseWidgetProps> = ({
  title,
  onRemove,
  onRefresh,
  onConfigure,
  children,
  isLoading = false,
  error = null,
  headerColor = BLOOMBERG_ORANGE
}) => {
  return (
    <div style={{
      height: '100%',
      width: '100%',
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Widget Header */}
      <div style={{
        backgroundColor: '#1a1a1a',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
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
                color: BLOOMBERG_GRAY,
                cursor: 'pointer',
                padding: '2px',
                display: 'flex',
                alignItems: 'center'
              }}
              onMouseEnter={(e) => e.currentTarget.style.color = BLOOMBERG_WHITE}
              onMouseLeave={(e) => e.currentTarget.style.color = BLOOMBERG_GRAY}
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
                color: isLoading ? BLOOMBERG_GRAY : BLOOMBERG_WHITE,
                cursor: isLoading ? 'not-allowed' : 'pointer',
                padding: '2px',
                display: 'flex',
                alignItems: 'center'
              }}
              onMouseEnter={(e) => !isLoading && (e.currentTarget.style.color = headerColor)}
              onMouseLeave={(e) => !isLoading && (e.currentTarget.style.color = BLOOMBERG_WHITE)}
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
                color: BLOOMBERG_GRAY,
                cursor: 'pointer',
                padding: '2px',
                display: 'flex',
                alignItems: 'center'
              }}
              onMouseEnter={(e) => e.currentTarget.style.color = BLOOMBERG_RED}
              onMouseLeave={(e) => e.currentTarget.style.color = BLOOMBERG_GRAY}
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
            color: BLOOMBERG_RED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            Error: {error}
          </div>
        ) : isLoading ? (
          <div style={{
            padding: '12px',
            color: BLOOMBERG_GRAY,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            Loading...
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
