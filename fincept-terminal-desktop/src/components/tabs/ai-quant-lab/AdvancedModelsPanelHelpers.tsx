/**
 * AdvancedModelsPanel - Helper Components
 */

import React from 'react';

// Help Section Component
export function HelpSection({ title, content, colors }: { title: string; content: string; colors: any }) {
  return (
    <div style={{ marginBottom: '20px' }}>
      <div style={{
        fontSize: '11px',
        fontWeight: 700,
        color: colors.purple,
        marginBottom: '8px',
        letterSpacing: '0.5px'
      }}>
        {title}
      </div>
      <div style={{
        fontSize: '10px',
        color: colors.text,
        opacity: 0.8,
        whiteSpace: 'pre-wrap'
      }}>
        {content}
      </div>
    </div>
  );
}
