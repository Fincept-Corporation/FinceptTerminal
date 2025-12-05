// File: src/components/chat-mode/components/ChatMessageBubble.tsx
// Message bubble component for chat interface

import React from 'react';
import { Bot, User } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { ChatModeMessage } from '@/services/chatModeApi';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';

interface ChatMessageBubbleProps {
  message: ChatModeMessage;
}

const ChatMessageBubble: React.FC<ChatMessageBubbleProps> = ({ message }) => {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const isUser = message.role === 'user';
  const isAssistant = message.role === 'assistant';

  const formatTime = (date: Date) => {
    return new Date(date).toLocaleTimeString('en-US', {
      hour: '2-digit',
      minute: '2-digit'
    });
  };

  return (
    <div
      style={{
        display: 'flex',
        gap: '12px',
        alignItems: 'flex-start',
        flexDirection: isUser ? 'row-reverse' : 'row'
      }}
    >
      {/* Avatar */}
      {isUser ? (
        <div
          style={{
            width: '36px',
            height: '36px',
            borderRadius: '50%',
            background: `linear-gradient(135deg, ${colors.info} 0%, ${colors.primary} 100%)`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            flexShrink: 0,
            border: `2px solid ${colors.info}33`
          }}
        >
          <User size={18} color={colors.background} />
        </div>
      ) : (
        <img
          src="/fincept_icon.ico"
          alt="Fincept AI"
          style={{
            width: '36px',
            height: '36px',
            borderRadius: '50%',
            objectFit: 'cover',
            flexShrink: 0,
            border: `2px solid ${colors.primary}33`
          }}
        />
      )}

      {/* Message Content */}
      <div
        style={{
          flex: 1,
          maxWidth: '75%',
          display: 'flex',
          flexDirection: 'column',
          gap: '6px',
          alignItems: isUser ? 'flex-end' : 'flex-start'
        }}
      >
        {/* Header */}
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            fontSize: fontSize.tiny,
            color: colors.textMuted
          }}
        >
          <span style={{ fontWeight: 'bold', color: isUser ? colors.info : colors.primary }}>
            {isUser ? 'YOU' : 'FINCEPT AI'}
          </span>
          <span>•</span>
          <span>{formatTime(message.timestamp)}</span>
        </div>

        {/* Message Bubble */}
        <div
          style={{
            background: isUser ? colors.panel : `${colors.primary}11`,
            border: `1px solid ${isUser ? colors.textMuted : colors.primary}33`,
            borderRadius: '12px',
            padding: '14px 18px',
            fontSize: fontSize.body,
            lineHeight: '1.6',
            wordBreak: 'break-word'
          }}
        >
          {isAssistant ? (
            <div style={{ color: colors.text }}>
              <MarkdownRenderer content={message.content} />
            </div>
          ) : (
            <div style={{ color: colors.text, whiteSpace: 'pre-wrap' }}>
              {message.content}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default ChatMessageBubble;
