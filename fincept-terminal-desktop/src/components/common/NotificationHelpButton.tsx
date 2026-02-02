// Notification Help Button for TabHeader
import React, { useState } from 'react';
import { Bell, HelpCircle, X } from 'lucide-react';
import { toast } from '@/components/ui/terminal-toast';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  CYAN: '#00E5FF',
  MUTED: '#4A4A4A',
};

const TYPOGRAPHY = {
  MONO: '"IBM Plex Mono", "Consolas", monospace',
};

// Help Modal Component
const NotificationHelpModal: React.FC<{
  isOpen: boolean;
  onClose: () => void;
}> = ({ isOpen, onClose }) => {
  if (!isOpen) return null;

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0,0,0,0.85)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 9999,
      }}
      onClick={onClose}
    >
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `2px solid ${FINCEPT.ORANGE}`,
          borderRadius: '2px',
          padding: '16px',
          width: '500px',
          maxHeight: '80vh',
          overflow: 'auto',
          fontFamily: TYPOGRAPHY.MONO,
        }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Bell size={14} style={{ color: FINCEPT.ORANGE }} />
            <h3 style={{ color: FINCEPT.ORANGE, fontSize: '12px', fontWeight: 700, margin: 0, letterSpacing: '0.5px', textTransform: 'uppercase' }}>
              Notification System Help
            </h3>
          </div>
          <button
            onClick={onClose}
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
            }}
          >
            <X size={10} /> CLOSE
          </button>
        </div>
        <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}`, marginBottom: '12px' }}></div>

        {/* Content */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
          {/* Overview */}
          <div>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN, marginBottom: '8px', letterSpacing: '0.5px', textTransform: 'uppercase' }}>
              Overview
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.WHITE, lineHeight: '1.6' }}>
              Fincept Terminal features a notification system that displays important alerts, updates, and messages.
              Notifications appear in the top-right corner of your screen with terminal-styled design.
            </div>
          </div>

          {/* Notification Types */}
          <div>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN, marginBottom: '8px', letterSpacing: '0.5px', textTransform: 'uppercase' }}>
              Notification Types
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {[
                { type: 'SUCCESS', color: '#00D66F', desc: 'Operations completed successfully' },
                { type: 'ERROR', color: '#FF3B3B', desc: 'Failed operations or errors' },
                { type: 'WARNING', color: '#FFD700', desc: 'Important warnings or alerts' },
                { type: 'INFO', color: '#00E5FF', desc: 'General information and updates' },
              ].map((item) => (
                <div key={item.type} style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
                  <span style={{ color: item.color, fontSize: '10px', lineHeight: '16px', fontWeight: 700, minWidth: '70px' }}>
                    {item.type}
                  </span>
                  <span style={{ color: FINCEPT.GRAY, fontSize: '10px', lineHeight: '16px' }}>
                    {item.desc}
                  </span>
                </div>
              ))}
            </div>
          </div>

          {/* Features */}
          <div>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN, marginBottom: '8px', letterSpacing: '0.5px', textTransform: 'uppercase' }}>
              Features
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {[
                'Terminal-styled design matching Fincept aesthetic',
                'Auto-dismiss after 8 seconds (customizable)',
                'Action buttons for quick responses',
                'Metadata display for contextual information',
                'Position: top-right corner',
                'Stacking support for multiple notifications',
              ].map((feature, idx) => (
                <div key={idx} style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
                  <span style={{ color: FINCEPT.ORANGE, fontSize: '10px', lineHeight: '16px' }}>■</span>
                  <span style={{ color: FINCEPT.GRAY, fontSize: '10px', lineHeight: '16px' }}>{feature}</span>
                </div>
              ))}
            </div>
          </div>

          {/* Test Notifications */}
          <div>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN, marginBottom: '8px', letterSpacing: '0.5px', textTransform: 'uppercase' }}>
              Test Notifications
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              {[
                { label: 'SUCCESS', color: '#00D66F', type: 'success' as const },
                { label: 'ERROR', color: '#FF3B3B', type: 'error' as const },
                { label: 'WARNING', color: '#FFD700', type: 'warning' as const },
                { label: 'INFO', color: '#00E5FF', type: 'info' as const },
              ].map((btn) => (
                <button
                  key={btn.type}
                  onClick={() => {
                    toast[btn.type](`This is a ${btn.type} notification test`, {
                      metadata: [
                        { label: 'Type', value: btn.type.toUpperCase() },
                        { label: 'Status', value: 'Test', color: btn.color },
                      ],
                    });
                  }}
                  style={{
                    backgroundColor: FINCEPT.HEADER_BG,
                    border: `1px solid ${btn.color}`,
                    borderRadius: '2px',
                    padding: '8px',
                    fontSize: '9px',
                    fontWeight: 700,
                    color: btn.color,
                    fontFamily: TYPOGRAPHY.MONO,
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                    letterSpacing: '0.5px',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = btn.color;
                    e.currentTarget.style.color = '#000000';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
                    e.currentTarget.style.color = btn.color;
                  }}
                >
                  TEST {btn.label}
                </button>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

// Main Button Component
export const NotificationHelpButton: React.FC = () => {
  const [showModal, setShowModal] = useState(false);

  const buttonStyle: React.CSSProperties = {
    backgroundColor: 'transparent',
    color: FINCEPT.MUTED,
    border: `1px solid ${FINCEPT.BORDER}`,
    padding: '4px 10px',
    fontSize: '9px',
    cursor: 'pointer',
    borderRadius: '2px',
    display: 'flex',
    alignItems: 'center',
    gap: '6px',
    fontWeight: 700,
    fontFamily: TYPOGRAPHY.MONO,
    transition: 'all 0.2s',
    letterSpacing: '0.5px',
    textTransform: 'uppercase',
  };

  return (
    <>
      <button
        onClick={() => setShowModal(true)}
        style={buttonStyle}
        onMouseEnter={(e) => {
          e.currentTarget.style.backgroundColor = FINCEPT.ORANGE;
          e.currentTarget.style.borderColor = FINCEPT.ORANGE;
          e.currentTarget.style.color = FINCEPT.WHITE;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.backgroundColor = 'transparent';
          e.currentTarget.style.borderColor = FINCEPT.BORDER;
          e.currentTarget.style.color = FINCEPT.MUTED;
        }}
        title="Notification System Help & Test"
      >
        <HelpCircle size={12} />
        Notifications
      </button>

      <NotificationHelpModal isOpen={showModal} onClose={() => setShowModal(false)} />
    </>
  );
};

export default NotificationHelpButton;
