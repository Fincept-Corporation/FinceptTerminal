// File: src/components/chat-mode/components/ChatSuggestions.tsx
// Quick suggestion buttons for chat mode

import React from 'react';
import { TrendingUp, Newspaper, PieChart, Calculator, DollarSign, BarChart3 } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface ChatSuggestionsProps {
  onSuggestionClick: (suggestion: string) => void;
}

const ChatSuggestions: React.FC<ChatSuggestionsProps> = ({ onSuggestionClick }) => {
  const { colors, fontSize } = useTerminalTheme();

  const suggestions = [
    {
      icon: <TrendingUp size={16} />,
      text: 'Show me top market movers today',
      category: 'Markets'
    },
    {
      icon: <Newspaper size={16} />,
      text: 'Get latest financial news',
      category: 'News'
    },
    {
      icon: <PieChart size={16} />,
      text: 'Analyze my portfolio performance',
      category: 'Portfolio'
    },
    {
      icon: <Calculator size={16} />,
      text: 'Calculate stock valuation for AAPL',
      category: 'Analytics'
    },
    {
      icon: <DollarSign size={16} />,
      text: 'What is the current GDP and inflation rate?',
      category: 'Economics'
    },
    {
      icon: <BarChart3 size={16} />,
      text: 'Show me market trends for tech sector',
      category: 'Research'
    }
  ];

  return (
    <div
      style={{
        padding: '0 24px 20px',
        display: 'flex',
        flexDirection: 'column',
        gap: '12px'
      }}
    >
      <h3 style={{ color: colors.textMuted, fontSize: fontSize.small, fontWeight: 'bold', margin: 0 }}>
        TRY ASKING:
      </h3>
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))',
          gap: '10px'
        }}
      >
        {suggestions.map((suggestion, idx) => (
          <button
            key={idx}
            onClick={() => onSuggestionClick(suggestion.text)}
            style={{
              background: colors.panel,
              border: `1px solid ${colors.primary}33`,
              borderRadius: '8px',
              padding: '12px 14px',
              cursor: 'pointer',
              textAlign: 'left',
              display: 'flex',
              alignItems: 'center',
              gap: '10px',
              transition: 'all 0.2s',
              color: colors.text,
              fontSize: fontSize.small
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = `${colors.primary}11`;
              e.currentTarget.style.borderColor = colors.primary;
              e.currentTarget.style.transform = 'translateY(-2px)';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = colors.panel;
              e.currentTarget.style.borderColor = `${colors.primary}33`;
              e.currentTarget.style.transform = 'translateY(0)';
            }}
          >
            <div style={{ color: colors.primary }}>{suggestion.icon}</div>
            <div style={{ flex: 1 }}>
              <div style={{ fontSize: fontSize.tiny, color: colors.textMuted, marginBottom: '2px' }}>
                {suggestion.category}
              </div>
              <div>{suggestion.text}</div>
            </div>
          </button>
        ))}
      </div>
    </div>
  );
};

export default ChatSuggestions;
