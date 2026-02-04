// Terminal-styled toast notifications matching Fincept design system
import { toast as sonnerToast, ExternalToast } from 'sonner';
import React from 'react';
import { notificationService } from '@/services/notifications';

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
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
};

const TYPOGRAPHY = {
  MONO: '"IBM Plex Mono", "Consolas", monospace',
};

interface ToastHeader {
  icon?: React.ReactNode;
  label: string;
  color?: string;
}

interface ToastAction {
  label: string;
  onClick: () => void;
}

interface TerminalToastOptions extends ExternalToast {
  header?: ToastHeader;
  message: string;
  metadata?: Array<{ label: string; value: string; color?: string }>;
  actions?: ToastAction[];
  borderColor?: string;
  type?: 'default' | 'success' | 'error' | 'warning' | 'info';
}

const getTypeColors = (type: TerminalToastOptions['type']) => {
  switch (type) {
    case 'success':
      return { border: FINCEPT.GREEN, headerColor: FINCEPT.GREEN };
    case 'error':
      return { border: FINCEPT.RED, headerColor: FINCEPT.RED };
    case 'warning':
      return { border: FINCEPT.YELLOW, headerColor: FINCEPT.YELLOW };
    case 'info':
      return { border: FINCEPT.CYAN, headerColor: FINCEPT.CYAN };
    default:
      return { border: FINCEPT.ORANGE, headerColor: FINCEPT.ORANGE };
  }
};

export const terminalToast = (options: TerminalToastOptions) => {
  const {
    header,
    message,
    metadata,
    actions,
    borderColor,
    type = 'default',
    ...sonnerOptions
  } = options;

  const colors = getTypeColors(type);
  const finalBorderColor = borderColor || colors.border;
  const finalHeaderColor = header?.color || colors.headerColor;

  const content = (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        fontFamily: TYPOGRAPHY.MONO,
        width: '100%',
      }}
    >
      {/* Header */}
      {header && (
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            marginBottom: '8px',
            paddingBottom: '6px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}
        >
          {header.icon && (
            <span style={{ color: finalHeaderColor, fontSize: '12px' }}>
              {header.icon}
            </span>
          )}
          <span
            style={{
              fontSize: '9px',
              fontWeight: 700,
              color: finalHeaderColor,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
            }}
          >
            {header.label}
          </span>
        </div>
      )}

      {/* Message */}
      <div
        style={{
          fontSize: '11px',
          color: FINCEPT.WHITE,
          lineHeight: '1.5',
          marginBottom: metadata || actions ? '8px' : '0',
        }}
      >
        {message}
      </div>

      {/* Metadata */}
      {metadata && metadata.length > 0 && (
        <div
          style={{
            display: 'flex',
            flexWrap: 'wrap',
            gap: '8px',
            fontSize: '9px',
            color: FINCEPT.MUTED,
            marginBottom: actions ? '8px' : '0',
            paddingTop: '6px',
            borderTop: `1px solid ${FINCEPT.BORDER}`,
          }}
        >
          {metadata.map((item, index) => (
            <div key={index} style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={{ color: FINCEPT.GRAY }}>{item.label}:</span>
              <span style={{ color: item.color || FINCEPT.CYAN, fontWeight: 600 }}>
                {item.value}
              </span>
              {index < metadata.length - 1 && (
                <span style={{ color: FINCEPT.BORDER }}>•</span>
              )}
            </div>
          ))}
        </div>
      )}

      {/* Actions */}
      {actions && actions.length > 0 && (
        <div
          style={{
            display: 'flex',
            gap: '6px',
            paddingTop: '8px',
            borderTop: `1px solid ${FINCEPT.BORDER}`,
          }}
        >
          {actions.map((action, index) => (
            <button
              key={index}
              onClick={action.onClick}
              style={{
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '4px 10px',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.CYAN,
                fontFamily: TYPOGRAPHY.MONO,
                cursor: 'pointer',
                transition: 'all 0.2s',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                e.currentTarget.style.borderColor = FINCEPT.CYAN;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
              }}
            >
              {action.label}
            </button>
          ))}
        </div>
      )}
    </div>
  );

  // Route to external notification providers (fire-and-forget)
  try {
    const eventType = type === 'default' ? 'info' : type;
    notificationService.notify({
      type: eventType as 'success' | 'error' | 'warning' | 'info',
      title: header?.label || type.charAt(0).toUpperCase() + type.slice(1),
      body: message,
      timestamp: new Date().toISOString(),
    });
  } catch (_) { /* never block UI */ }

  return sonnerToast(content, {
    duration: 8000,
    position: 'top-right',
    style: {
      backgroundColor: FINCEPT.PANEL_BG,
      border: `2px solid ${finalBorderColor}`,
      borderRadius: '2px',
      padding: '12px',
      boxShadow: `0 4px 16px ${finalBorderColor}40`,
      minWidth: '320px',
      maxWidth: '420px',
    },
    ...sonnerOptions,
  });
};

// Preset functions for common notification types
export const toast = {
  success: (message: string, options?: Partial<TerminalToastOptions>) =>
    terminalToast({
      type: 'success',
      header: { label: 'Success', color: FINCEPT.GREEN },
      message,
      ...options,
    }),

  error: (message: string, options?: Partial<TerminalToastOptions>) =>
    terminalToast({
      type: 'error',
      header: { label: 'Error', color: FINCEPT.RED },
      message,
      ...options,
    }),

  warning: (message: string, options?: Partial<TerminalToastOptions>) =>
    terminalToast({
      type: 'warning',
      header: { label: 'Warning', color: FINCEPT.YELLOW },
      message,
      ...options,
    }),

  info: (message: string, options?: Partial<TerminalToastOptions>) =>
    terminalToast({
      type: 'info',
      header: { label: 'Info', color: FINCEPT.CYAN },
      message,
      ...options,
    }),

  custom: (options: TerminalToastOptions) => terminalToast(options),
};
