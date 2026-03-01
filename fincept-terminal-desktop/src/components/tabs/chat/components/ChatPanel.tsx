import React from 'react';
import { Send, Bot } from 'lucide-react';
import { ChatMessage, LLMConfig } from '@/services/core/sqliteService';
import { ChatSession } from '@/services/core/sqliteService';
import { WelcomeScreen, ChatMessageItem, StreamingMessage } from './MessageRenderer';

interface ChatPanelProps {
  colors: any;
  fontSize: any;
  messages: ChatMessage[];
  streamingContent: string;
  isTyping: boolean;
  messageInput: string;
  currentSessionUuid: string | null;
  sessions: ChatSession[];
  activeLLMConfig: LLMConfig | null;
  systemStatus: string;
  activeToolCalls: Array<{ name: string; args: any; status: 'calling' | 'success' | 'error' }>;
  messagesEndRef: React.RefObject<HTMLDivElement | null>;
  t: (key: string) => string;
  formatTime: (date: Date | string | undefined) => string;
  onMessageInputChange: (v: string) => void;
  onSendMessage: () => void;
  onClearCurrentChat: () => void;
  onCreateNewSession: () => void;
}

export function ChatPanel({
  colors,
  fontSize,
  messages,
  streamingContent,
  isTyping,
  messageInput,
  currentSessionUuid,
  sessions,
  activeLLMConfig,
  systemStatus,
  activeToolCalls,
  messagesEndRef,
  t,
  formatTime,
  onMessageInputChange,
  onSendMessage,
  onClearCurrentChat,
  onCreateNewSession,
}: ChatPanelProps) {
  return (
    <div style={{
      flex: 1,
      backgroundColor: colors.panel,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Chat Header */}
      <div style={{
        padding: '12px',
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <span style={{
          fontSize: fontSize.small,
          fontWeight: 700,
          color: colors.textMuted,
          letterSpacing: '0.5px'
        }}>
          {currentSessionUuid ? (sessions.find(s => s.session_uuid === currentSessionUuid)?.title || 'CHAT').toUpperCase() : 'NO ACTIVE SESSION'}
        </span>
        {currentSessionUuid && (
          <button
            onClick={onClearCurrentChat}
            style={{
              padding: '2px 6px',
              backgroundColor: `${colors.alert}20`,
              color: colors.alert,
              border: 'none',
              fontSize: fontSize.tiny,
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              letterSpacing: '0.5px',
              transition: 'all 0.2s'
            }}
          >
            CLEAR
          </button>
        )}
      </div>

      {/* Messages Area with scrollbar */}
      <div style={{
        flex: 1,
        padding: '16px',
        overflow: 'auto',
        backgroundColor: colors.background
      }}>
        {messages.length === 0 && !streamingContent ? (
          <WelcomeScreen
            colors={colors}
            fontSize={fontSize}
            activeLLMConfig={activeLLMConfig}
            t={t}
            onCreateNewSession={onCreateNewSession}
          />
        ) : (
          <div>
            {messages.map(message => (
              <ChatMessageItem
                key={message.id}
                message={message}
                colors={colors}
                fontSize={fontSize}
                formatTime={formatTime}
              />
            ))}
            {streamingContent && (
              <StreamingMessage
                colors={colors}
                fontSize={fontSize}
                streamingContent={streamingContent}
                activeToolCalls={activeToolCalls}
                t={t}
              />
            )}
            {isTyping && !streamingContent && (
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                color: colors.textMuted,
                fontSize: fontSize.small,
                letterSpacing: '0.5px'
              }}>
                <Bot size={12} color={colors.textMuted} />
                <span>{t('messages.thinking').toUpperCase()}</span>
              </div>
            )}
            <div ref={messagesEndRef} />
          </div>
        )}
      </div>

      {/* Input Area */}
      <div style={{
        padding: '12px',
        backgroundColor: colors.panel,
        borderTop: `1px solid ${'rgba(255,255,255,0.1)'}`
      }}>
        <div style={{ display: 'flex', gap: '8px' }}>
          <textarea
            value={messageInput}
            onChange={(e) => onMessageInputChange(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                onSendMessage();
              }
            }}
            placeholder={t('input.placeholder')}
            style={{
              flex: 1,
              padding: '8px 10px',
              backgroundColor: colors.background,
              color: colors.text,
              border: `1px solid ${'rgba(255,255,255,0.1)'}`,
              borderRadius: '2px',
              fontSize: fontSize.small,
              resize: 'none',
              height: '60px',
              fontFamily: '"IBM Plex Mono", "Consolas", monospace'
            }}
          />
          <button
            onClick={onSendMessage}
            disabled={!messageInput.trim() || isTyping}
            style={{
              padding: '8px 16px',
              backgroundColor: messageInput.trim() && !isTyping ? colors.primary : colors.textMuted,
              color: colors.background,
              border: 'none',
              borderRadius: '2px',
              fontSize: fontSize.small,
              fontWeight: 700,
              cursor: messageInput.trim() && !isTyping ? 'pointer' : 'not-allowed',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              letterSpacing: '0.5px',
              transition: 'all 0.2s',
              opacity: messageInput.trim() && !isTyping ? 1 : 0.5
            }}
          >
            <Send size={12} />
            {t('input.send').toUpperCase()}
          </button>
        </div>
        <div style={{
          color: colors.textMuted,
          fontSize: fontSize.small,
          marginTop: '6px',
          letterSpacing: '0.5px',
          fontFamily: '"IBM Plex Mono", "Consolas", monospace'
        }}>
          {messageInput.length > 0 ? `${messageInput.length} CHARS` : systemStatus}
        </div>
      </div>
    </div>
  );
}
