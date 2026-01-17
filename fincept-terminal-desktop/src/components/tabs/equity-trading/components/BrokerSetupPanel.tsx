/**
 * Broker Setup Panel - Bloomberg Style
 *
 * Combined panel showing both Configuration and Authentication tabs
 */

import React, { useState } from 'react';
import { Settings, KeyRound } from 'lucide-react';
import { BrokerConfigPanel } from './BrokerConfigPanel';
import { BrokerAuthPanel } from './BrokerAuthPanel';

// Bloomberg color palette
const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  WHITE: '#FFFFFF',
};

type SetupTab = 'config' | 'auth';

export function BrokerSetupPanel() {
  const [activeTab, setActiveTab] = useState<SetupTab>('config');

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {/* Tab Navigation */}
      <div style={{
        display: 'flex',
        gap: '0',
        backgroundColor: COLORS.DARK_BG,
        border: `1px solid ${COLORS.BORDER}`,
        padding: '2px'
      }}>
        <button
          onClick={() => setActiveTab('config')}
          style={{
            flex: 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px',
            padding: '10px 16px',
            backgroundColor: activeTab === 'config' ? COLORS.ORANGE : 'transparent',
            border: 'none',
            color: activeTab === 'config' ? COLORS.DARK_BG : COLORS.GRAY,
            cursor: 'pointer',
            fontSize: '11px',
            fontWeight: 700,
            fontFamily: '"IBM Plex Mono", monospace',
            letterSpacing: '0.5px',
            transition: 'all 0.15s ease'
          }}
          onMouseEnter={(e) => {
            if (activeTab !== 'config') {
              e.currentTarget.style.backgroundColor = COLORS.HEADER_BG;
              e.currentTarget.style.color = COLORS.WHITE;
            }
          }}
          onMouseLeave={(e) => {
            if (activeTab !== 'config') {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = COLORS.GRAY;
            }
          }}
        >
          <Settings size={14} />
          CONFIGURATION
        </button>
        <button
          onClick={() => setActiveTab('auth')}
          style={{
            flex: 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px',
            padding: '10px 16px',
            backgroundColor: activeTab === 'auth' ? COLORS.ORANGE : 'transparent',
            border: 'none',
            color: activeTab === 'auth' ? COLORS.DARK_BG : COLORS.GRAY,
            cursor: 'pointer',
            fontSize: '11px',
            fontWeight: 700,
            fontFamily: '"IBM Plex Mono", monospace',
            letterSpacing: '0.5px',
            transition: 'all 0.15s ease'
          }}
          onMouseEnter={(e) => {
            if (activeTab !== 'auth') {
              e.currentTarget.style.backgroundColor = COLORS.HEADER_BG;
              e.currentTarget.style.color = COLORS.WHITE;
            }
          }}
          onMouseLeave={(e) => {
            if (activeTab !== 'auth') {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = COLORS.GRAY;
            }
          }}
        >
          <KeyRound size={14} />
          AUTHENTICATION
        </button>
      </div>

      {/* Tab Content */}
      <div>
        {activeTab === 'config' && (
          <BrokerConfigPanel
            onConfigSaved={() => {
              // Auto-switch to auth tab after successful config
              setTimeout(() => setActiveTab('auth'), 1500);
            }}
          />
        )}
        {activeTab === 'auth' && <BrokerAuthPanel />}
      </div>
    </div>
  );
}
