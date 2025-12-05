// File: src/components/chat-mode/ChatModeInterface.tsx
// Main Chat Mode interface - Clean implementation with Fincept API

import React, { useState, useEffect, useRef } from 'react';
import { Send, Sparkles, Loader2 } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { chatModeApiService, ChatModeMessage } from '@/services/chatModeApi';
import ChatMessageBubble from './components/ChatMessageBubble';
import ChatSuggestions from './components/ChatSuggestions';

const ChatModeInterface: React.FC = () => {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [messages, setMessages] = useState<ChatModeMessage[]>([]);
  const [input, setInput] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLTextAreaElement>(null);

  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  useEffect(() => {
    // Initialize with welcome message
    const welcomeMessage: ChatModeMessage = {
      id: 'welcome',
      role: 'assistant',
      content: `# Welcome to Fincept Terminal Chat Mode

I'm your AI financial assistant. I can help you with:

**📊 Market Analysis**
- Real-time quotes and price movements
- Technical and fundamental analysis
- Market trends and insights

**💼 Portfolio Management**
- Portfolio analytics and performance
- Risk assessment and optimization
- Asset allocation recommendations

**📰 News & Research**
- Latest financial news
- Company research and analysis
- Economic indicators and trends

**📈 Advanced Analytics**
- Financial modeling
- Valuation analysis
- Custom calculations

**How can I assist you today?**`,
      timestamp: new Date()
    };
    setMessages([welcomeMessage]);
  }, []);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const handleSendMessage = async () => {
    if (!input.trim() || isLoading) return;

    const userMessage: ChatModeMessage = {
      id: `user-${Date.now()}`,
      role: 'user',
      content: input.trim(),
      timestamp: new Date()
    };

    setMessages(prev => [...prev, userMessage]);
    setInput('');
    setIsLoading(true);

    try {
      // Call quick chat API
      const response = await chatModeApiService.quickChat(userMessage.content);

      if (response.success && response.data) {
        const assistantMessage: ChatModeMessage = {
          id: `assistant-${Date.now()}`,
          role: 'assistant',
          content: response.data.ai_response,
          timestamp: new Date(response.data.timestamp)
        };

        setMessages(prev => [...prev, assistantMessage]);
      } else {
        throw new Error(response.error || 'Failed to get response');
      }
    } catch (error) {
      console.error('Chat error:', error);
      const errorMessage: ChatModeMessage = {
        id: `error-${Date.now()}`,
        role: 'assistant',
        content: 'I apologize, but I encountered an error processing your request. Please try again or rephrase your question.',
        timestamp: new Date()
      };
      setMessages(prev => [...prev, errorMessage]);
    } finally {
      setIsLoading(false);
    }
  };

  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSendMessage();
    }
  };

  const handleSuggestionClick = (suggestion: string) => {
    setInput(suggestion);
    inputRef.current?.focus();
  };

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        backgroundColor: colors.background,
        color: colors.text,
        fontFamily: fontFamily,
        fontSize: fontSize.body,
        overflow: 'hidden'
      }}
    >
      {/* Header */}
      <div
        style={{
          padding: '16px 24px',
          borderBottom: `1px solid ${colors.primary}33`,
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          background: `linear-gradient(135deg, ${colors.background} 0%, ${colors.panel} 100%)`
        }}
      >
        <img
          src="/fincept_icon.ico"
          alt="Fincept"
          style={{
            width: '40px',
            height: '40px',
            borderRadius: '50%',
            objectFit: 'cover'
          }}
        />
        <div style={{ flex: 1 }}>
          <h1 style={{ fontSize: fontSize.heading, fontWeight: 'bold', margin: 0, color: colors.primary }}>
            FINCEPT AI ASSISTANT
          </h1>
          <p style={{ fontSize: fontSize.tiny, margin: 0, color: colors.textMuted }}>
            Powered by Natural Language Processing • Real-time Market Data
          </p>
        </div>
        <div
          style={{
            padding: '6px 12px',
            borderRadius: '12px',
            background: `${colors.success}22`,
            border: `1px solid ${colors.success}`,
            fontSize: fontSize.tiny,
            color: colors.success,
            fontWeight: 'bold'
          }}
        >
          ● ONLINE
        </div>
      </div>

      {/* Messages Area */}
      <div
        style={{
          flex: 1,
          overflowY: 'auto',
          padding: '24px',
          display: 'flex',
          flexDirection: 'column',
          gap: '20px'
        }}
      >
        {messages.map((message) => (
          <ChatMessageBubble key={message.id} message={message} />
        ))}

        {isLoading && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', color: colors.textMuted }}>
            <Loader2 size={16} className="animate-spin" />
            <span style={{ fontSize: fontSize.small }}>Processing...</span>
          </div>
        )}

        <div ref={messagesEndRef} />
      </div>

      {/* Suggestions (show when no messages or few messages) */}
      {messages.length <= 1 && (
        <ChatSuggestions onSuggestionClick={handleSuggestionClick} />
      )}

      {/* Input Area */}
      <div
        style={{
          padding: '20px 24px',
          borderTop: `1px solid ${colors.primary}33`,
          background: colors.panel
        }}
      >
        <div
          style={{
            display: 'flex',
            gap: '12px',
            alignItems: 'flex-end',
            background: colors.background,
            border: `2px solid ${colors.primary}33`,
            borderRadius: '12px',
            padding: '12px',
            transition: 'border-color 0.2s'
          }}
        >
          <textarea
            ref={inputRef}
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyPress={handleKeyPress}
            placeholder="Ask me anything about markets, your portfolio, news, analytics..."
            disabled={isLoading}
            style={{
              flex: 1,
              background: 'transparent',
              border: 'none',
              color: colors.text,
              fontSize: fontSize.body,
              fontFamily: fontFamily,
              resize: 'none',
              outline: 'none',
              minHeight: '24px',
              maxHeight: '120px',
              lineHeight: '24px'
            }}
            rows={1}
            onInput={(e) => {
              const target = e.target as HTMLTextAreaElement;
              target.style.height = '24px';
              target.style.height = `${target.scrollHeight}px`;
            }}
          />
          <button
            onClick={handleSendMessage}
            disabled={!input.trim() || isLoading}
            style={{
              background: input.trim() && !isLoading ? colors.primary : colors.textMuted,
              border: 'none',
              borderRadius: '8px',
              padding: '10px 16px',
              cursor: input.trim() && !isLoading ? 'pointer' : 'not-allowed',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              color: colors.background,
              fontSize: fontSize.small,
              fontWeight: 'bold',
              transition: 'all 0.2s',
              opacity: input.trim() && !isLoading ? 1 : 0.5
            }}
          >
            {isLoading ? (
              <Loader2 size={16} className="animate-spin" />
            ) : (
              <Send size={16} />
            )}
            SEND
          </button>
        </div>
        <p style={{ fontSize: fontSize.tiny, color: colors.textMuted, margin: '8px 0 0 0', textAlign: 'center' }}>
          Press Enter to send • Shift + Enter for new line
        </p>
      </div>
    </div>
  );
};

export default ChatModeInterface;
