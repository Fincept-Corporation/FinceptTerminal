// File: src/components/chat-mode/components/ChatMessageBubble.tsx
// Fincept-style message bubble component

import React from 'react';
import { Terminal, User, Clock } from 'lucide-react';
import { ChatModeMessage } from '@/services/data-sources/chatModeApi';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';

// Fincept Terminal Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

interface ChatMessageBubbleProps {
  message: ChatModeMessage;
}

const ChatMessageBubble: React.FC<ChatMessageBubbleProps> = ({ message }) => {
  const isUser = message.role === 'user';

  const formatTime = (date: Date) => {
    return new Date(date).toLocaleTimeString('en-US', {
      hour12: false,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  };

  return (
    <div style={{ marginBottom: '12px' }}>
      <div style={{
        display: 'flex',
        justifyContent: isUser ? 'flex-end' : 'flex-start',
        marginBottom: '4px'
      }}>
        <div style={{
          maxWidth: '85%',
          minWidth: '120px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${isUser ? FINCEPT.YELLOW : FINCEPT.ORANGE}`,
          borderRadius: '2px',
          padding: '10px'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            marginBottom: '8px',
            paddingBottom: '6px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            {isUser ? (
              <User size={12} color={FINCEPT.YELLOW} />
            ) : (
              <Terminal size={12} color={FINCEPT.ORANGE} />
            )}
            <span style={{
              color: isUser ? FINCEPT.YELLOW : FINCEPT.ORANGE,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px'
            }}>
              {isUser ? 'YOU' : 'FINCEPT AI'}
            </span>
            <Clock size={10} color={FINCEPT.GRAY} />
            <span style={{
              color: FINCEPT.GRAY,
              fontSize: '9px',
              letterSpacing: '0.5px',
              fontFamily: '"IBM Plex Mono", "Consolas", monospace'
            }}>
              {formatTime(message.timestamp)}
            </span>
          </div>
          {isUser ? (
            <div style={{
              color: FINCEPT.WHITE,
              fontSize: '11px',
              lineHeight: '1.6',
              whiteSpace: 'pre-wrap',
              fontFamily: '"IBM Plex Mono", "Consolas", monospace'
            }}>
              {message.content}
            </div>
          ) : (
            <MarkdownRenderer content={message.content} />
          )}
        </div>
      </div>
    </div>
  );
};

export default ChatMessageBubble;
