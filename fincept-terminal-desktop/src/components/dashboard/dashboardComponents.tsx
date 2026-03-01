/**
 * Dashboard Screen — Sub-components
 *
 * Small, self-contained components extracted from DashboardScreen.tsx:
 * - TabLoadingFallback
 * - DropdownMenu
 * - HeaderTimeDisplay
 */

import React, { useState, useRef, useEffect } from 'react';
import { useCurrentTime } from '@/contexts/TimezoneContext';

// ==================== TAB LOADING FALLBACK ====================

export const TabLoadingFallback = () => (
  <div style={{
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    height: '100%',
    backgroundColor: '#000000',
    color: '#a3a3a3',
    fontSize: '12px',
    fontFamily: 'Consolas, "Courier New", monospace'
  }}>
    <div style={{ textAlign: 'center' }}>
      <div style={{
        width: '40px',
        height: '40px',
        border: '3px solid #333',
        borderTop: '3px solid #ea580c',
        borderRadius: '50%',
        animation: 'spin 1s linear infinite',
        margin: '0 auto 16px'
      }} />
      <div>Loading tab...</div>
      <style>{`
        @keyframes spin {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  </div>
);

// ==================== DROPDOWN MENU ====================

export const DropdownMenu = ({ label, items, onItemClick }: { label: string; items: any[]; onItemClick: (item: any) => void }) => {
  const [isOpen, setIsOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(event.target as Node)) {
        setIsOpen(false);
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  return (
    <div ref={menuRef} style={{ position: 'relative', display: 'inline-block' }}>
      <span
        style={{ cursor: 'pointer', padding: '2px 4px', borderRadius: '2px', backgroundColor: isOpen ? '#404040' : 'transparent' }}
        onClick={() => setIsOpen(!isOpen)}
      >
        {label}
      </span>
      {isOpen && (
        <div style={{
          position: 'absolute',
          top: '100%',
          left: 0,
          backgroundColor: '#2d2d2d',
          border: '1px solid #404040',
          borderRadius: '2px',
          minWidth: '180px',
          maxHeight: '70vh',
          overflowY: 'auto',
          zIndex: 1000,
          boxShadow: '0 2px 8px rgba(0,0,0,0.5)'
        }}>
          {items.map((item: any, index: number) => {
            if (item.header) {
              return (
                <div
                  key={index}
                  style={{
                    padding: '6px 12px 3px',
                    color: '#ea580c',
                    fontSize: '9px',
                    fontWeight: 'bold',
                    textTransform: 'uppercase',
                    letterSpacing: '0.5px',
                    borderTop: index > 0 ? '1px solid #404040' : 'none',
                    marginTop: index > 0 ? '2px' : '0'
                  }}
                >
                  {item.label}
                </div>
              );
            }
            return (
              <div
                key={index}
                style={{
                  padding: '5px 12px 5px 20px',
                  cursor: item.disabled ? 'not-allowed' : 'pointer',
                  color: item.disabled ? '#666' : '#a3a3a3',
                  fontSize: '11px',
                  borderBottom: item.separator ? '1px solid #404040' : 'none'
                }}
                onMouseEnter={(e) => {
                  if (!item.disabled) {
                    (e.target as HTMLDivElement).style.backgroundColor = '#404040';
                    (e.target as HTMLDivElement).style.color = '#fff';
                  }
                }}
                onMouseLeave={(e) => {
                  (e.target as HTMLDivElement).style.backgroundColor = 'transparent';
                  (e.target as HTMLDivElement).style.color = item.disabled ? '#666' : '#a3a3a3';
                }}
                onClick={() => {
                  if (!item.disabled && onItemClick) {
                    onItemClick(item);
                    setIsOpen(false);
                  }
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  {item.icon}
                  <span>{item.label}</span>
                  {item.shortcut && (
                    <span style={{ marginLeft: 'auto', fontSize: '10px', color: '#666' }}>
                      {item.shortcut}
                    </span>
                  )}
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
};

// ==================== HEADER TIME DISPLAY ====================

// Header Time Display Component - uses DEFAULT timezone from settings for nav bar
export const HeaderTimeDisplay = () => {
  const { formattedTime, timezone } = useCurrentTime();

  // Get current date in YYYY-MM-DD format
  const [currentDate, setCurrentDate] = React.useState(() => {
    const now = new Date();
    return now.toISOString().split('T')[0];
  });

  // Update date at midnight
  React.useEffect(() => {
    const updateDate = () => {
      const now = new Date();
      setCurrentDate(now.toISOString().split('T')[0]);
    };

    const timer = setInterval(updateDate, 60000); // Check every minute
    return () => clearInterval(timer);
  }, []);

  return (
    <span style={{ color: '#a3a3a3', fontSize: '11px', fontWeight: 500 }}>
      {currentDate} {formattedTime} <span style={{ color: '#ea580c', fontWeight: 600 }}>{timezone.shortLabel}</span>
    </span>
  );
};
