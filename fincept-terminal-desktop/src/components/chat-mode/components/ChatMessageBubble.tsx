// File: src/components/chat-mode/components/ChatMessageBubble.tsx
// Bloomberg-style message bubble component

import React from 'react';
import { Terminal, User } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { ChatModeMessage } from '@/services/chatModeApi';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';

interface ChatMessageBubbleProps {
  message: ChatModeMessage;
}

const ChatMessageBubble: React.FC<ChatMessageBubbleProps> = ({ message }) => {
  const { colors } = useTerminalTheme();
  const isUser = message.role === 'user';

  const formatTime = (date: Date) => {
    return new Date(date).toLocaleTimeString('en-US', {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  };

  return (
    <div
      style={{
        marginBottom: '12px',
        display: 'flex',
        flexDirection: 'column',
        gap: '4px'
      }}
    >
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          fontSize: '10px',
          color: '#666',
          fontFamily: 'Consolas, "Courier New", monospace'
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          {isUser ? (
            <User size={12} style={{ color: '#06b6d4' }} />
          ) : (
            <Terminal size={12} style={{ color: '#ea580c' }} />
          )}
          <span style={{ color: isUser ? '#06b6d4' : '#ea580c', fontWeight: 'bold' }}>
            {isUser ? 'USER' : 'FINCEPT AI'}
          </span>
        </div>
        <span style={{ color: '#333' }}>|</span>
        <span>{formatTime(message.timestamp)}</span>
      </div>

      {/* Message Content */}
      <div
        style={{
          background: isUser ? '#0a0a0a' : '#000000',
          border: `1px solid ${isUser ? '#333' : '#1a1a1a'}`,
          borderLeft: `3px solid ${isUser ? '#06b6d4' : '#ea580c'}`,
          padding: '12px 14px',
          fontSize: '12px',
          lineHeight: '1.6',
          fontFamily: 'Consolas, "Courier New", monospace',
          color: colors.text
        }}
      >
        {isUser ? (
          <div style={{ whiteSpace: 'pre-wrap' }}>
            {message.content}
          </div>
        ) : (
          <div className="terminal-markdown">
            <MarkdownRenderer content={message.content} />
          </div>
        )}
      </div>

      <style>{`
        .terminal-markdown h1,
        .terminal-markdown h2,
        .terminal-markdown h3 {
          color: #ea580c;
          font-weight: bold;
          margin: 8px 0 6px 0;
        }
        .terminal-markdown h1 { font-size: 14px; }
        .terminal-markdown h2 { font-size: 13px; }
        .terminal-markdown h3 { font-size: 12px; }
        .terminal-markdown p {
          margin: 6px 0;
        }
        .terminal-markdown ul,
        .terminal-markdown ol {
          margin: 6px 0 6px 20px;
        }
        .terminal-markdown li {
          margin: 3px 0;
        }
        .terminal-markdown strong {
          color: #ea580c;
          font-weight: bold;
        }
        .terminal-markdown code {
          background: #1a1a1a;
          padding: 2px 6px;
          border-radius: 2px;
          color: #22c55e;
          font-family: 'Consolas', 'Courier New', monospace;
          font-size: 11px;
        }
        .terminal-markdown pre {
          background: #0a0a0a;
          border: 1px solid #333;
          border-left: 3px solid #22c55e;
          padding: 10px;
          margin: 8px 0;
          overflow-x: auto;
          border-radius: 2px;
        }
        .terminal-markdown pre code {
          background: transparent;
          padding: 0;
        }
        .terminal-markdown a {
          color: #06b6d4;
          text-decoration: none;
          border-bottom: 1px dotted #06b6d4;
        }
        .terminal-markdown a:hover {
          color: #22d3ee;
          border-bottom-color: #22d3ee;
        }
        .terminal-markdown blockquote {
          border-left: 3px solid #fbbf24;
          padding-left: 12px;
          margin: 8px 0;
          color: #999;
        }
        .terminal-markdown table {
          border-collapse: collapse;
          margin: 8px 0;
          width: 100%;
        }
        .terminal-markdown th,
        .terminal-markdown td {
          border: 1px solid #333;
          padding: 6px 10px;
          text-align: left;
        }
        .terminal-markdown th {
          background: #1a1a1a;
          color: #ea580c;
          font-weight: bold;
        }
        .terminal-markdown hr {
          border: none;
          border-top: 1px solid #333;
          margin: 12px 0;
        }
      `}</style>
    </div>
  );
};

export default ChatMessageBubble;
