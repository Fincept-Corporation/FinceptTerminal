/**
 * Visualization 3D Tab
 * ====================
 * This tab has been deprecated in favor of SurfaceAnalyticsTab
 * which provides real Databento integration for 3D financial visualizations.
 *
 * @deprecated Use SurfaceAnalyticsTab instead
 */

import React from 'react';
import { AlertTriangle, ArrowRight } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

// This tab is deprecated - redirect users to SurfaceAnalyticsTab
const Visualization3DTab: React.FC = () => {
  const { colors, fontFamily } = useTerminalTheme();

  return (
    <div
      style={{
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        backgroundColor: '#000000',
        color: colors.text,
        fontFamily,
        padding: '48px',
      }}
    >
      <div
        style={{
          maxWidth: '480px',
          padding: '32px',
          backgroundColor: '#0F0F0F',
          border: '1px solid #2A2A2A',
          borderRadius: '2px',
          textAlign: 'center',
        }}
      >
        <div
          style={{
            width: '64px',
            height: '64px',
            margin: '0 auto 24px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            backgroundColor: '#FFD70015',
            borderRadius: '50%',
          }}
        >
          <AlertTriangle size={32} style={{ color: '#FFD700' }} />
        </div>

        <h2
          style={{
            fontSize: '16px',
            fontWeight: 700,
            color: colors.text,
            marginBottom: '12px',
            letterSpacing: '0.5px',
          }}
        >
          TAB DEPRECATED
        </h2>

        <p
          style={{
            fontSize: '11px',
            color: '#787878',
            marginBottom: '24px',
            lineHeight: '1.6',
          }}
        >
          The 3D Visualization tab has been replaced with Surface Analytics,
          which provides real-time market data from Databento for professional
          financial analysis including volatility surfaces, correlation matrices,
          and PCA factor analysis.
        </p>

        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px',
            padding: '12px 24px',
            backgroundColor: colors.accent,
            color: '#000000',
            borderRadius: '2px',
            fontSize: '10px',
            fontWeight: 700,
            letterSpacing: '0.5px',
          }}
        >
          <span>USE SURFACE ANALYTICS TAB</span>
          <ArrowRight size={14} />
        </div>

        <p
          style={{
            marginTop: '16px',
            fontSize: '9px',
            color: '#4A4A4A',
          }}
        >
          Navigate to: Surface Analytics (in the sidebar)
        </p>
      </div>
    </div>
  );
};

export default Visualization3DTab;
