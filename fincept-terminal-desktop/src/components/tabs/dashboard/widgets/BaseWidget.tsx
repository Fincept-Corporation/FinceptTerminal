// BaseWidget - Terminal-style widget container
// Consistent widget frame with drag handle, controls, and loading/error states

import React, { useState } from 'react';
import { X, RefreshCw, Settings, Maximize2, Minimize2, GripHorizontal } from 'lucide-react';
import { useTranslation } from 'react-i18next';

const FC = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
};

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
  const [isHovered, setIsHovered] = useState(false);

  return (
    <div
      style={{
        height: '100%',
        width: '100%',
        backgroundColor: FC.PANEL_BG,
        border: `1px solid ${isHovered ? FC.ORANGE + '40' : FC.BORDER}`,
        borderRadius: '2px',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        transition: 'border-color 0.2s',
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}
      onMouseEnter={() => setIsHovered(true)}
      onMouseLeave={() => setIsHovered(false)}
    >
      {/* Widget Header */}
      <div style={{
        backgroundColor: FC.HEADER_BG,
        borderBottom: `1px solid ${FC.BORDER}`,
        padding: '0 8px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        minHeight: '26px',
        flexShrink: 0,
      }}>
        {/* Left: Drag handle + Title */}
        <div
          className="widget-drag-handle"
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            cursor: 'move',
            flex: 1,
            overflow: 'hidden',
            userSelect: 'none',
            padding: '4px 0',
          }}
        >
          <GripHorizontal size={10} color={FC.MUTED} style={{ flexShrink: 0, opacity: isHovered ? 0.8 : 0.3 }} />
          <div style={{
            width: '2px',
            height: '10px',
            backgroundColor: headerColor || FC.ORANGE,
            borderRadius: '1px',
            flexShrink: 0,
          }} />
          <span style={{
            color: headerColor || FC.ORANGE,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
            textTransform: 'uppercase',
          }}>
            {title}
          </span>

          {/* Loading indicator inline */}
          {isLoading && (
            <div style={{
              width: '8px', height: '8px',
              border: `1.5px solid ${FC.MUTED}`,
              borderTop: `1.5px solid ${FC.ORANGE}`,
              borderRadius: '50%',
              animation: 'ftSpin 0.8s linear infinite',
              flexShrink: 0,
            }} />
          )}
        </div>

        {/* Right: Controls */}
        <div style={{
          display: 'flex',
          gap: '2px',
          flexShrink: 0,
          opacity: isHovered ? 1 : 0.3,
          transition: 'opacity 0.2s',
        }}>
          {onConfigure && (
            <WidgetButton onClick={onConfigure} title="Configure">
              <Settings size={10} />
            </WidgetButton>
          )}
          {onRefresh && (
            <WidgetButton onClick={onRefresh} disabled={isLoading} title="Refresh">
              <RefreshCw size={10} style={{ animation: isLoading ? 'ftSpin 1s linear infinite' : 'none' }} />
            </WidgetButton>
          )}
          {onRemove && (
            <WidgetButton onClick={onRemove} title="Remove" danger>
              <X size={10} />
            </WidgetButton>
          )}
        </div>
      </div>

      {/* Widget Content */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        position: 'relative',
      }}>
        {error ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            padding: '12px',
            gap: '6px',
          }}>
            <div style={{
              width: '24px', height: '24px',
              borderRadius: '50%',
              backgroundColor: `${FC.RED}15`,
              display: 'flex', alignItems: 'center', justifyContent: 'center',
            }}>
              <X size={12} color={FC.RED} />
            </div>
            <div style={{ color: FC.RED, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px' }}>ERROR</div>
            <div style={{ color: FC.MUTED, fontSize: '8px', textAlign: 'center', lineHeight: '1.3' }}>
              {error}
            </div>
            {onRefresh && (
              <button
                onClick={onRefresh}
                style={{
                  background: 'transparent',
                  border: `1px solid ${FC.BORDER}`,
                  color: FC.GRAY,
                  padding: '3px 8px',
                  fontSize: '8px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  borderRadius: '2px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  marginTop: '4px',
                }}
              >
                RETRY
              </button>
            )}
          </div>
        ) : isLoading ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            gap: '8px',
          }}>
            <div style={{
              width: '24px', height: '24px',
              border: `2px solid ${FC.BORDER}`,
              borderTop: `2px solid ${FC.ORANGE}`,
              borderRadius: '50%',
              animation: 'ftSpin 0.8s linear infinite',
            }} />
            <div style={{ color: FC.MUTED, fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px' }}>
              LOADING DATA...
            </div>
          </div>
        ) : (
          children
        )}
      </div>

      {/* Keyframes (injected once) */}
      <style>{`
        @keyframes ftSpin { to { transform: rotate(360deg); } }
      `}</style>
    </div>
  );
};

// Small button component for widget controls
const WidgetButton: React.FC<{
  onClick: () => void;
  disabled?: boolean;
  title?: string;
  danger?: boolean;
  children: React.ReactNode;
}> = ({ onClick, disabled, title, danger, children }) => (
  <button
    onClick={onClick}
    disabled={disabled}
    title={title}
    style={{
      background: 'none',
      border: 'none',
      color: danger ? FC.RED : FC.GRAY,
      cursor: disabled ? 'not-allowed' : 'pointer',
      padding: '3px',
      display: 'flex',
      alignItems: 'center',
      borderRadius: '2px',
      transition: 'all 0.15s',
      opacity: disabled ? 0.4 : 1,
    }}
    onMouseEnter={(e) => {
      if (!disabled) {
        (e.currentTarget as HTMLElement).style.backgroundColor = danger ? `${FC.RED}15` : FC.HOVER;
        (e.currentTarget as HTMLElement).style.color = danger ? FC.RED : FC.WHITE;
      }
    }}
    onMouseLeave={(e) => {
      (e.currentTarget as HTMLElement).style.backgroundColor = 'transparent';
      (e.currentTarget as HTMLElement).style.color = danger ? FC.RED : FC.GRAY;
    }}
  >
    {children}
  </button>
);
