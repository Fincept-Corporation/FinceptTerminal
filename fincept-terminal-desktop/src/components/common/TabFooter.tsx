import React from 'react';
import { APP_VERSION, APP_NAME } from '@/constants/version';

interface TabFooterProps {
  /** The name of the current tab/section */
  tabName: string;
  /** Optional status or info message to display on the right */
  statusInfo?: string | React.ReactNode;
  /** Optional left-side additional info items */
  leftInfo?: Array<{ label: string; value?: string; color?: string }>;
  /** Optional custom background color */
  backgroundColor?: string;
  /** Optional custom border color */
  borderColor?: string;
}

/**
 * Unified, customizable footer component for all tabs
 * Displays consistent branding, version info, and tab-specific status
 */
export const TabFooter: React.FC<TabFooterProps> = ({
  tabName,
  statusInfo,
  leftInfo = [],
  backgroundColor = '#1a1a1a',
  borderColor = '#ea580c',
}) => {
  const COLORS = {
    ORANGE: '#ea580c',
    CYAN: '#06b6d4',
    GREEN: '#10b981',
    GRAY: '#6b7280',
    WHITE: '#ffffff',
  };

  return (
    <div
      style={{
        borderTop: `2px solid ${borderColor}`,
        backgroundColor: backgroundColor,
        padding: '8px 16px',
        fontSize: '11px',
        flexShrink: 0,
        fontFamily: 'Consolas, "Courier New", monospace',
      }}
    >
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexWrap: 'wrap',
          gap: '12px',
        }}
      >
        {/* Left Side: Branding + Tab Name + Additional Info */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flexWrap: 'wrap' }}>
          <span style={{ color: COLORS.ORANGE, fontWeight: 'bold', fontSize: '12px' }}>
            {APP_NAME.toUpperCase()} v{APP_VERSION}
          </span>
          <span style={{ color: COLORS.GRAY }}>|</span>
          <span style={{ color: COLORS.CYAN, fontWeight: '600' }}>{tabName}</span>

          {leftInfo.length > 0 && (
            <>
              <span style={{ color: COLORS.GRAY }}>|</span>
              {leftInfo.map((info, index) => (
                <React.Fragment key={index}>
                  <span style={{ color: info.color || COLORS.WHITE }}>
                    {info.label}{info.value ? `: ${info.value}` : ''}
                  </span>
                  {index < leftInfo.length - 1 && <span style={{ color: COLORS.GRAY }}>|</span>}
                </React.Fragment>
              ))}
            </>
          )}
        </div>

        {/* Right Side: Status/Info */}
        {statusInfo && (
          <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>
            {statusInfo}
          </div>
        )}
      </div>
    </div>
  );
};

export default TabFooter;
