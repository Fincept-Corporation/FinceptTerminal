import React from 'react';
import { LLMConfig } from '@/services/core/sqliteService';

interface ChatStatusBarProps {
  colors: any;
  fontSize: any;
  activeLLMConfig: LLMConfig | null;
  currentSessionUuid: string | null;
  messagesCount: number;
  streamingContent: string;
}

export function ChatStatusBar({
  colors,
  fontSize,
  activeLLMConfig,
  currentSessionUuid,
  messagesCount,
  streamingContent,
}: ChatStatusBarProps) {
  return (
    <div style={{
      backgroundColor: colors.panel,
      borderTop: `1px solid ${'rgba(255,255,255,0.1)'}`,
      padding: '4px 16px',
      fontSize: fontSize.small,
      color: colors.textMuted,
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      letterSpacing: '0.5px',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace'
    }}>
      <div style={{ display: 'flex', gap: '12px' }}>
        <span>TAB: AI CHAT</span>
        <span>|</span>
        <span>PROVIDER: {activeLLMConfig?.provider.toUpperCase() || 'NONE'}</span>
        <span>|</span>
        <span>SESSION: {currentSessionUuid ? 'ACTIVE' : 'NONE'}</span>
      </div>
      <div>
        MESSAGES: {messagesCount} | STATUS: {streamingContent ? 'STREAMING...' : 'READY'}
      </div>
    </div>
  );
}
