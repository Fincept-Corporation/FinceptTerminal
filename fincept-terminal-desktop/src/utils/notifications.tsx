/**
 * Global Notification Utilities
 * Replacement for alert(), confirm(), and prompt() with terminal-styled dialogs
 */

import React, { useState } from 'react';
import { toast } from '@/components/ui/terminal-toast';
import { AlertTriangle, CheckCircle, XCircle, Info, X, Edit } from 'lucide-react';

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
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  MUTED: '#4A4A4A',
};

const TYPOGRAPHY = {
  MONO: '"IBM Plex Mono", "Consolas", monospace',
};

// Confirmation Dialog Component
interface ConfirmDialogProps {
  isOpen: boolean;
  title: string;
  message: string;
  confirmText?: string;
  cancelText?: string;
  type?: 'warning' | 'danger' | 'info';
  onConfirm: () => void;
  onCancel: () => void;
}

export const ConfirmDialog: React.FC<ConfirmDialogProps> = ({
  isOpen,
  title,
  message,
  confirmText = 'Confirm',
  cancelText = 'Cancel',
  type = 'warning',
  onConfirm,
  onCancel,
}) => {
  if (!isOpen) return null;

  const colors = {
    warning: { border: FINCEPT.YELLOW, icon: AlertTriangle, bg: `${FINCEPT.YELLOW}15` },
    danger: { border: FINCEPT.RED, icon: XCircle, bg: `${FINCEPT.RED}15` },
    info: { border: FINCEPT.CYAN, icon: Info, bg: `${FINCEPT.CYAN}15` },
  };

  const config = colors[type];
  const Icon = config.icon;

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0,0,0,0.90)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 10000,
        fontFamily: TYPOGRAPHY.MONO,
      }}
      onClick={onCancel}
    >
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `2px solid ${config.border}`,
          borderRadius: '2px',
          width: '420px',
          maxWidth: '90vw',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div
          style={{
            padding: '12px 16px',
            backgroundColor: config.bg,
            borderBottom: `1px solid ${config.border}`,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Icon size={16} style={{ color: config.border }} />
            <span
              style={{
                fontSize: '11px',
                fontWeight: 700,
                color: config.border,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
              }}
            >
              {title}
            </span>
          </div>
          <button
            onClick={onCancel}
            style={{
              background: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.MUTED,
              cursor: 'pointer',
              padding: '2px 6px',
              fontSize: '9px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              borderRadius: '2px',
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: 700,
            }}
          >
            <X size={10} /> ESC
          </button>
        </div>

        {/* Message */}
        <div style={{ padding: '20px 16px' }}>
          <div
            style={{
              fontSize: '11px',
              color: FINCEPT.WHITE,
              lineHeight: '1.6',
              whiteSpace: 'pre-wrap',
            }}
          >
            {message}
          </div>
        </div>

        {/* Actions */}
        <div
          style={{
            display: 'flex',
            gap: '8px',
            padding: '0 16px 16px',
          }}
        >
          <button
            onClick={onCancel}
            style={{
              flex: 1,
              padding: '10px',
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.WHITE,
              cursor: 'pointer',
              fontFamily: TYPOGRAPHY.MONO,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
            }}
          >
            {cancelText}
          </button>
          <button
            onClick={onConfirm}
            style={{
              flex: 1,
              padding: '10px',
              backgroundColor: type === 'danger' ? FINCEPT.RED : config.border,
              border: 'none',
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              color: type === 'danger' ? FINCEPT.WHITE : FINCEPT.DARK_BG,
              cursor: 'pointer',
              fontFamily: TYPOGRAPHY.MONO,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.8';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}
          >
            {confirmText}
          </button>
        </div>
      </div>
    </div>
  );
};

// Prompt Dialog Component
interface PromptDialogProps {
  isOpen: boolean;
  title: string;
  message: string;
  defaultValue?: string;
  placeholder?: string;
  confirmText?: string;
  cancelText?: string;
  onConfirm: (value: string) => void;
  onCancel: () => void;
}

export const PromptDialog: React.FC<PromptDialogProps> = ({
  isOpen,
  title,
  message,
  defaultValue = '',
  placeholder = '',
  confirmText = 'OK',
  cancelText = 'Cancel',
  onConfirm,
  onCancel,
}) => {
  const [inputValue, setInputValue] = React.useState(defaultValue);

  React.useEffect(() => {
    setInputValue(defaultValue);
  }, [defaultValue, isOpen]);

  if (!isOpen) return null;

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    onConfirm(inputValue);
  };

  const Icon = Edit;

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0,0,0,0.90)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 10000,
        fontFamily: TYPOGRAPHY.MONO,
      }}
      onClick={onCancel}
    >
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `2px solid ${FINCEPT.ORANGE}`,
          borderRadius: '2px',
          width: '420px',
          maxWidth: '90vw',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div
          style={{
            padding: '12px 16px',
            backgroundColor: `${FINCEPT.ORANGE}15`,
            borderBottom: `1px solid ${FINCEPT.ORANGE}`,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Icon size={16} style={{ color: FINCEPT.ORANGE }} />
            <span
              style={{
                fontSize: '11px',
                fontWeight: 700,
                color: FINCEPT.ORANGE,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
              }}
            >
              {title}
            </span>
          </div>
          <button
            onClick={onCancel}
            style={{
              background: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              padding: '2px 6px',
              fontSize: '9px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              borderRadius: '2px',
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: 700,
            }}
          >
            <X size={10} /> ESC
          </button>
        </div>

        {/* Form */}
        <form onSubmit={handleSubmit}>
          {/* Message */}
          {message && (
            <div style={{ padding: '16px 16px 12px 16px' }}>
              <div
                style={{
                  fontSize: '11px',
                  color: FINCEPT.GRAY,
                  lineHeight: '1.6',
                  whiteSpace: 'pre-wrap',
                }}
              >
                {message}
              </div>
            </div>
          )}

          {/* Input */}
          <div style={{ padding: message ? '0 16px 16px 16px' : '16px' }}>
            <input
              type="text"
              value={inputValue}
              onChange={(e) => setInputValue(e.target.value)}
              placeholder={placeholder}
              autoFocus
              style={{
                width: '100%',
                padding: '10px 12px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                color: FINCEPT.WHITE,
                fontSize: '11px',
                fontFamily: TYPOGRAPHY.MONO,
                outline: 'none',
              }}
              onFocus={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              }}
              onBlur={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
              }}
            />
          </div>

          {/* Actions */}
          <div
            style={{
              padding: '12px 16px',
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              justifyContent: 'flex-end',
              gap: '8px',
            }}
          >
            <button
              type="button"
              onClick={onCancel}
              style={{
                padding: '8px 16px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                fontFamily: TYPOGRAPHY.MONO,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                transition: 'all 0.2s',
                borderRadius: '2px',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = 'transparent';
              }}
            >
              {cancelText}
            </button>
            <button
              type="submit"
              style={{
                padding: '8px 16px',
                backgroundColor: FINCEPT.ORANGE,
                border: 'none',
                color: FINCEPT.DARK_BG,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                fontFamily: TYPOGRAPHY.MONO,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                transition: 'all 0.2s',
                borderRadius: '2px',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.opacity = '0.8';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.opacity = '1';
              }}
            >
              {confirmText}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};

// Global state management for dialogs
let globalConfirmResolver: ((result: boolean) => void) | null = null;
let globalSetDialogState: ((state: any) => void) | null = null;
let globalPromptResolver: ((result: string | null) => void) | null = null;
let globalSetPromptState: ((state: any) => void) | null = null;

export const NotificationProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [dialogState, setDialogState] = useState<{
    isOpen: boolean;
    title: string;
    message: string;
    confirmText?: string;
    cancelText?: string;
    type?: 'warning' | 'danger' | 'info';
  }>({
    isOpen: false,
    title: '',
    message: '',
  });

  const [promptState, setPromptState] = useState<{
    isOpen: boolean;
    title: string;
    message: string;
    defaultValue?: string;
    placeholder?: string;
    confirmText?: string;
    cancelText?: string;
  }>({
    isOpen: false,
    title: '',
    message: '',
  });

  React.useEffect(() => {
    globalSetDialogState = setDialogState;
    globalSetPromptState = setPromptState;
  }, []);

  const handleConfirm = () => {
    if (globalConfirmResolver) {
      globalConfirmResolver(true);
      globalConfirmResolver = null;
    }
    setDialogState({ ...dialogState, isOpen: false });
  };

  const handleCancel = () => {
    if (globalConfirmResolver) {
      globalConfirmResolver(false);
      globalConfirmResolver = null;
    }
    setDialogState({ ...dialogState, isOpen: false });
  };

  const handlePromptConfirm = (value: string) => {
    if (globalPromptResolver) {
      globalPromptResolver(value);
      globalPromptResolver = null;
    }
    setPromptState({ ...promptState, isOpen: false });
  };

  const handlePromptCancel = () => {
    if (globalPromptResolver) {
      globalPromptResolver(null);
      globalPromptResolver = null;
    }
    setPromptState({ ...promptState, isOpen: false });
  };

  return (
    <>
      {children}
      <ConfirmDialog
        isOpen={dialogState.isOpen}
        title={dialogState.title}
        message={dialogState.message}
        confirmText={dialogState.confirmText}
        cancelText={dialogState.cancelText}
        type={dialogState.type}
        onConfirm={handleConfirm}
        onCancel={handleCancel}
      />
      <PromptDialog
        isOpen={promptState.isOpen}
        title={promptState.title}
        message={promptState.message}
        defaultValue={promptState.defaultValue}
        placeholder={promptState.placeholder}
        confirmText={promptState.confirmText}
        cancelText={promptState.cancelText}
        onConfirm={handlePromptConfirm}
        onCancel={handlePromptCancel}
      />
    </>
  );
};

// Utility functions to replace alert() and confirm()

/**
 * Show a confirmation dialog (replacement for confirm())
 * @returns Promise<boolean> - true if confirmed, false if cancelled
 */
export const showConfirm = (
  message: string,
  options?: {
    title?: string;
    confirmText?: string;
    cancelText?: string;
    type?: 'warning' | 'danger' | 'info';
  }
): Promise<boolean> => {
  return new Promise((resolve) => {
    globalConfirmResolver = resolve;

    if (globalSetDialogState) {
      globalSetDialogState({
        isOpen: true,
        title: options?.title || 'Confirm Action',
        message,
        confirmText: options?.confirmText,
        cancelText: options?.cancelText,
        type: options?.type || 'warning',
      });
    } else {
      // Fallback to native confirm if provider not mounted
      console.warn('NotificationProvider not mounted, using native confirm');
      resolve(window.confirm(message));
    }
  });
};

/**
 * Show a success notification (replacement for alert())
 */
export const showSuccess = (message: string, metadata?: Array<{ label: string; value: string; color?: string }>) => {
  toast.success(message, { metadata, duration: 5000 });
};

/**
 * Show an error notification (replacement for alert())
 */
export const showError = (message: string, metadata?: Array<{ label: string; value: string; color?: string }>) => {
  toast.error(message, { metadata, duration: 8000 });
};

/**
 * Show an info notification (replacement for alert())
 */
export const showInfo = (message: string, metadata?: Array<{ label: string; value: string; color?: string }>) => {
  toast.info(message, { metadata, duration: 5000 });
};

/**
 * Show a warning notification (replacement for alert())
 */
export const showWarning = (message: string, metadata?: Array<{ label: string; value: string; color?: string }>) => {
  toast.warning(message, { metadata, duration: 6000 });
};

/**
 * Generic alert replacement - auto-detects type from message
 */
export const showAlert = (message: string) => {
  const lowerMsg = message.toLowerCase();

  if (lowerMsg.includes('success') || lowerMsg.includes('complete') || lowerMsg.includes('[ok]')) {
    showSuccess(message);
  } else if (lowerMsg.includes('error') || lowerMsg.includes('fail') || lowerMsg.includes('[fail]')) {
    showError(message);
  } else if (lowerMsg.includes('warn') || lowerMsg.includes('[warn]')) {
    showWarning(message);
  } else {
    showInfo(message);
  }
};

/**
 * Show an input prompt dialog (replacement for prompt())
 * @returns Promise<string | null> - user input string if confirmed, null if cancelled
 */
export const showPrompt = (
  message: string,
  options?: {
    title?: string;
    defaultValue?: string;
    placeholder?: string;
    confirmText?: string;
    cancelText?: string;
  }
): Promise<string | null> => {
  return new Promise((resolve) => {
    globalPromptResolver = resolve;

    if (globalSetPromptState) {
      globalSetPromptState({
        isOpen: true,
        title: options?.title || 'Enter Value',
        message,
        defaultValue: options?.defaultValue || '',
        placeholder: options?.placeholder || '',
        confirmText: options?.confirmText,
        cancelText: options?.cancelText,
      });
    } else {
      // Fallback to native prompt if provider not mounted
      console.warn('NotificationProvider not mounted, using native prompt');
      resolve(window.prompt(message, options?.defaultValue || '') || null);
    }
  });
};
