import React, { useState, useEffect } from 'react';
import { Search, HelpCircle } from 'lucide-react';
import { getColors } from './constants';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';

export default function DocsTab() {
  const { colors: themeColors } = useTerminalTheme();
  const COLORS = getColors();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [searchQuery, setSearchQuery] = useState('');

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  return (
    <div
      style={{
        height: '100%',
        backgroundColor: COLORS.DARK_BG,
        color: COLORS.WHITE,
        fontFamily: 'Consolas, monospace',
        overflow: 'hidden',
        display: 'flex',
        flexDirection: 'column'
      }}
    >
      <style>{`
        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        *::-webkit-scrollbar-track {
          background: #1a1a1a;
        }
        *::-webkit-scrollbar-thumb {
          background: #404040;
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: #525252;
        }

        .docs-content h1 {
          color: ${COLORS.ORANGE};
          font-size: 18px;
          font-weight: bold;
          margin: 0 0 16px 0;
          font-family: 'Consolas', monospace;
          letter-spacing: 0.5px;
          text-transform: uppercase;
        }
        .docs-content h2 {
          color: ${COLORS.CYAN};
          font-size: 14px;
          font-weight: bold;
          margin: 16px 0 8px 0;
          font-family: 'Consolas', monospace;
          text-transform: uppercase;
        }
        .docs-content h3 {
          color: ${COLORS.WHITE};
          font-size: 12px;
          font-weight: bold;
          margin: 12px 0 6px 0;
          font-family: 'Consolas', monospace;
        }
        .docs-content p {
          line-height: 1.6;
          margin: 0 0 12px 0;
          font-size: 11px;
          font-family: 'Consolas', monospace;
          color: ${COLORS.WHITE};
        }
        .docs-content ul {
          margin: 12px 0;
          padding-left: 0;
          list-style: none;
        }
        .docs-content li {
          margin: 6px 0;
          padding-left: 20px;
          line-height: 1.5;
          font-size: 11px;
          font-family: 'Consolas', monospace;
          position: relative;
          color: ${COLORS.WHITE};
        }
        .docs-content li::before {
          content: '■';
          position: absolute;
          left: 8px;
          color: ${COLORS.ORANGE};
          font-size: 8px;
        }
        .docs-content strong {
          color: ${COLORS.CYAN};
          font-weight: bold;
        }
        .docs-content code {
          background: rgba(255, 165, 0, 0.1);
          padding: 2px 4px;
          border-radius: 2px;
          font-family: 'Consolas', monospace;
          font-size: 11px;
          color: ${COLORS.ORANGE};
        }
      `}</style>

      {/* Header Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 12px',
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: `1px solid ${COLORS.GRAY}`,
        fontSize: '13px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.ORANGE, fontWeight: 'bold' }}>FINCEPT</span>
          <span style={{ color: COLORS.WHITE }}>DOCUMENTATION TERMINAL</span>
          <span style={{ color: COLORS.GRAY }}>|</span>
          <span style={{ color: COLORS.WHITE, fontSize: '11px' }}>
            {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.GRAY, fontSize: '11px' }}>SECTIONS:</span>
          <span style={{ color: COLORS.CYAN, fontSize: '11px', fontWeight: 'bold' }}>0</span>
          <span style={{ color: COLORS.GRAY }}>|</span>
          <span style={{ color: COLORS.GREEN, fontSize: '11px' }}>TOPICS:</span>
          <span style={{ color: COLORS.GREEN, fontSize: '11px', fontWeight: 'bold' }}>0</span>
        </div>
      </div>

      {/* Control Panel */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 12px',
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: `1px solid ${COLORS.GRAY}`,
        fontSize: '12px',
        gap: '8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flex: 1 }}>
          <Search size={14} style={{ color: COLORS.ORANGE, flexShrink: 0 }} />
          <input
            type="text"
            placeholder="Search documentation..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              flex: 1,
              backgroundColor: COLORS.DARK_BG,
              border: `1px solid ${COLORS.GRAY}`,
              color: COLORS.WHITE,
              fontSize: '11px',
              fontFamily: 'Consolas, monospace',
              padding: '4px 8px',
              outline: 'none',
              maxWidth: '400px'
            }}
          />
          {searchQuery && (
            <button
              onClick={() => setSearchQuery('')}
              style={{
                backgroundColor: COLORS.GRAY,
                color: 'black',
                border: 'none',
                padding: '4px 8px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer'
              }}
            >
              CLEAR
            </button>
          )}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.GREEN, fontSize: '14px' }}>●</span>
          <span style={{ color: COLORS.GREEN, fontSize: '11px', fontWeight: 'bold' }}>
            READY
          </span>
        </div>
      </div>

      {/* Main Content Area */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>
        {/* Sidebar Navigation */}
        <div style={{
          width: '280px',
          borderRight: `1px solid ${COLORS.GRAY}`,
          backgroundColor: COLORS.PANEL_BG,
          overflowY: 'auto',
          flexShrink: 0
        }}>
          {/* Section Header */}
          <div style={{
            color: COLORS.ORANGE,
            fontSize: '12px',
            fontWeight: 'bold',
            padding: '12px',
            borderBottom: `1px solid ${COLORS.GRAY}`,
            letterSpacing: '0.5px'
          }}>
            NAVIGATION
          </div>

          {/* Empty state for navigation */}
          <div style={{
            padding: '24px 12px',
            textAlign: 'center',
            color: COLORS.GRAY,
            fontSize: '11px'
          }}>
            No documentation sections available
          </div>
        </div>

        {/* Content Area */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          <div style={{
            flex: 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: '#0a0a0a',
            padding: '40px'
          }}>
            <div style={{
              textAlign: 'center',
              maxWidth: '600px'
            }}>
              <HelpCircle size={64} style={{ marginBottom: '24px', opacity: 0.3, color: COLORS.ORANGE }} />
              <div style={{
                fontSize: '16px',
                fontWeight: 'bold',
                marginBottom: '12px',
                color: COLORS.ORANGE,
                fontFamily: 'Consolas, monospace',
                letterSpacing: '0.5px'
              }}>
                FINCEPT TERMINAL DOCUMENTATION
              </div>
              <div style={{
                fontSize: '11px',
                lineHeight: '1.6',
                color: COLORS.GRAY,
                fontFamily: 'Consolas, monospace'
              }}>
                Documentation content will be populated here.
              </div>
            </div>
          </div>
        </div>
      </div>

      <TabFooter
        tabName="DOCUMENTATION"
        leftInfo={[
          { label: 'Complete API Reference & Guides', color: COLORS.GRAY },
          { label: 'Sections: 0', color: COLORS.GRAY },
          { label: 'Topics: 0', color: COLORS.GRAY },
        ]}
        statusInfo="Ready to populate documentation"
        backgroundColor={COLORS.PANEL_BG}
        borderColor={COLORS.GRAY}
      />
    </div>
  );
}
