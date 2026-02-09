import React, { useRef, useEffect } from 'react';
import {
  Bot,
  Send,
  User,
  Sparkles,
} from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { AIChatPanelProps } from './types';
import MarkdownRenderer from '../../common/MarkdownRenderer';

export const AIChatPanel: React.FC<AIChatPanelProps> = ({
  messages,
  input,
  onInputChange,
  onSendMessage,
  onQuickAction,
  isTyping,
  streamingContent,
  currentProvider,
  onProviderChange,
  availableModels,
}) => {
  const { colors, fontSize } = useTerminalTheme();
  const messagesEndRef = useRef<HTMLDivElement>(null);

  // Auto-scroll to bottom when messages change
  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages, streamingContent]);

  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      onSendMessage();
    }
  };

  return (
    <div className="flex flex-col h-full min-h-0" style={{ backgroundColor: colors.panel }}>
      {/* AI Assistant Header */}
      <div className="p-4 border-b border-[#333333] flex-shrink-0">
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center gap-2">
            <Bot size={18} style={{ color: colors.primary }} />
            <h3 className="text-sm font-bold" style={{ color: colors.primary }}>
              AI WRITING ASSISTANT
            </h3>
          </div>
          <Sparkles size={16} style={{ color: colors.primary }} />
        </div>

        {/* Model Selector */}
        <div className="mb-3">
          <label className="text-[9px] block mb-1" style={{ color: colors.textMuted }}>
            AI Model
          </label>
          <select
            value={currentProvider}
            onChange={(e) => onProviderChange(e.target.value)}
            className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
            style={{ color: colors.text }}
          >
            {availableModels.map(model => (
              <option key={model.id} value={model.provider}>
                {model.name} ({model.provider})
              </option>
            ))}
            {availableModels.length === 0 && (
              <option value="ollama">Configure in Settings</option>
            )}
          </select>
        </div>

        {/* Quick Actions */}
        <div className="grid grid-cols-2 gap-1">
          <button
            onClick={() => onQuickAction('improve')}
            className="px-2 py-1 text-[9px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors"
            style={{ color: colors.text }}
          >
            ✨ Improve
          </button>
          <button
            onClick={() => onQuickAction('expand')}
            className="px-2 py-1 text-[9px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors"
            style={{ color: colors.text }}
          >
            📝 Expand
          </button>
          <button
            onClick={() => onQuickAction('summarize')}
            className="px-2 py-1 text-[9px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors"
            style={{ color: colors.text }}
          >
            📋 Summary
          </button>
          <button
            onClick={() => onQuickAction('grammar')}
            className="px-2 py-1 text-[9px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors"
            style={{ color: colors.text }}
          >
            [OK] Grammar
          </button>
        </div>
      </div>

      {/* AI Chat Messages */}
      <div className="flex-1 overflow-y-auto p-4 space-y-3 min-h-0">
        {messages.length === 0 && (
          <div className="text-center py-8">
            <Bot size={32} className="mx-auto mb-3" style={{ color: colors.textMuted }} />
            <p className="text-xs" style={{ color: colors.textMuted }}>
              Ask me to help you write better reports!
            </p>
            <p className="text-[9px] mt-2" style={{ color: colors.textMuted }}>
              Try: "Help me write an introduction"
            </p>
          </div>
        )}

        {messages.map((msg, idx) => (
          <div key={idx} className={`flex gap-2 ${msg.role === 'user' ? 'justify-end' : 'justify-start'}`}>
            {msg.role === 'assistant' && (
              <div className="flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center" style={{ backgroundColor: colors.primary }}>
                <Bot size={14} style={{ color: '#000' }} />
              </div>
            )}
            <div
              className={`max-w-[85%] rounded-lg p-3 ${
                msg.role === 'user'
                  ? 'bg-[#FFA500] text-black'
                  : 'bg-[#1a1a1a] border border-[#333333]'
              }`}
            >
              <div className="text-xs break-words" style={{ color: msg.role === 'user' ? '#000' : colors.text }}>
                {msg.role === 'assistant' ? (
                  <MarkdownRenderer content={msg.content} />
                ) : (
                  msg.content
                )}
              </div>
            </div>
            {msg.role === 'user' && (
              <div className="flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center bg-[#333333]">
                <User size={14} style={{ color: colors.text }} />
              </div>
            )}
          </div>
        ))}

        {/* Streaming content */}
        {isTyping && streamingContent && (
          <div className="flex gap-2 justify-start">
            <div className="flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center" style={{ backgroundColor: colors.primary }}>
              <Bot size={14} style={{ color: '#000' }} />
            </div>
            <div className="max-w-[85%] rounded-lg p-3 bg-[#1a1a1a] border border-[#333333]">
              <div className="text-xs break-words" style={{ color: colors.text }}>
                <MarkdownRenderer content={streamingContent} />
              </div>
            </div>
          </div>
        )}

        {/* Typing indicator */}
        {isTyping && !streamingContent && (
          <div className="flex gap-2 justify-start">
            <div className="flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center" style={{ backgroundColor: colors.primary }}>
              <Bot size={14} style={{ color: '#000' }} />
            </div>
            <div className="max-w-[85%] rounded-lg p-3 bg-[#1a1a1a] border border-[#333333]">
              <div className="text-xs flex gap-1" style={{ color: colors.textMuted }}>
                <span className="animate-bounce">●</span>
                <span className="animate-bounce delay-100">●</span>
                <span className="animate-bounce delay-200">●</span>
              </div>
            </div>
          </div>
        )}

        <div ref={messagesEndRef} />
      </div>

      {/* AI Input */}
      <div className="p-4 border-t border-[#333333] flex-shrink-0">
        <div className="flex gap-2">
          <input
            type="text"
            value={input}
            onChange={(e) => onInputChange(e.target.value)}
            onKeyPress={handleKeyPress}
            placeholder="Ask AI to help with your report..."
            className="flex-1 px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
            style={{ color: colors.text }}
            disabled={isTyping}
          />
          <button
            onClick={onSendMessage}
            disabled={isTyping || !input.trim()}
            className="px-3 py-2 rounded transition-all disabled:opacity-50 disabled:cursor-not-allowed"
            style={{
              backgroundColor: colors.primary,
              color: '#000'
            }}
          >
            <Send size={16} />
          </button>
        </div>
      </div>
    </div>
  );
};

export default AIChatPanel;
