import React from 'react';
import { APP_VERSION, APP_NAME } from '@/constants/version';

interface TabHeaderProps {
  /** The title of the current tab */
  title: string;
  /** Optional subtitle or description */
  subtitle?: string;
  /** Optional icon element */
  icon?: React.ReactNode;
  /** Optional actions/buttons to display on the right */
  actions?: React.ReactNode;
  /** Optional custom background color */
  backgroundColor?: string;
  /** Optional custom border color */
  borderColor?: string;
  /** Hide version info (default: false) */
  hideVersion?: boolean;
}

/**
 * Unified, customizable header component for all tabs
 * Displays consistent tab title, optional icon, subtitle, and actions
 */
export const TabHeader: React.FC<TabHeaderProps> = ({
  title,
  subtitle,
  icon,
  actions,
  backgroundColor = '#1a1a1a',
  borderColor = '#ea580c',
  hideVersion = false,
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
        borderBottom: `2px solid ${borderColor}`,
        backgroundColor: backgroundColor,
        padding: '12px 16px',
        flexShrink: 0,
        fontFamily: 'Consolas, "Courier New", monospace',
      }}
    >
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          gap: '16px',
        }}
      >
        {/* Left Side: Icon + Title + Subtitle */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flex: 1 }}>
          {icon && (
            <div style={{ color: COLORS.ORANGE, display: 'flex', alignItems: 'center' }}>
              {icon}
            </div>
          )}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span style={{ color: COLORS.WHITE, fontWeight: 'bold', fontSize: '16px' }}>
                {title}
              </span>
              {!hideVersion && (
                <>
                  <span style={{ color: COLORS.GRAY, fontSize: '12px' }}>|</span>
                  <span style={{ color: COLORS.ORANGE, fontSize: '11px', fontWeight: '600' }}>
                    {APP_NAME} v{APP_VERSION}
                  </span>
                </>
              )}
            </div>
            {subtitle && (
              <span style={{ color: COLORS.GRAY, fontSize: '11px' }}>
                {subtitle}
              </span>
            )}
          </div>
        </div>

        {/* Right Side: Actions */}
        {actions && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            {actions}
          </div>
        )}
      </div>
    </div>
  );
};

export default TabHeader;
