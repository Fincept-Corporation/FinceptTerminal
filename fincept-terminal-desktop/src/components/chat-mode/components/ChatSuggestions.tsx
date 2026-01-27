// File: src/components/chat-mode/components/ChatSuggestions.tsx
// Quick suggestion buttons for chat mode

import React from 'react';
import { TrendingUp, Newspaper, PieChart, Calculator, DollarSign, BarChart3 } from 'lucide-react';

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

interface ChatSuggestionsProps {
  onSuggestionClick: (suggestion: string) => void;
}

const ChatSuggestions: React.FC<ChatSuggestionsProps> = ({ onSuggestionClick }) => {

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
        padding: '12px 16px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`
      }}
    >
      <div style={{
        fontSize: '9px',
        fontWeight: 700,
        color: FINCEPT.GRAY,
        letterSpacing: '0.5px',
        marginBottom: '12px'
      }}>
        TRY ASKING:
      </div>
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))',
          gap: '8px'
        }}
      >
        {suggestions.map((suggestion, idx) => (
          <button
            key={idx}
            onClick={() => onSuggestionClick(suggestion.text)}
            style={{
              padding: '10px 12px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              cursor: 'pointer',
              textAlign: 'left',
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              transition: 'all 0.2s',
              color: FINCEPT.WHITE,
              fontSize: '10px',
              fontFamily: '"IBM Plex Mono", "Consolas", monospace'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              e.currentTarget.style.borderColor = FINCEPT.ORANGE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
            }}
          >
            <div style={{ color: FINCEPT.ORANGE }}>{suggestion.icon}</div>
            <div style={{ flex: 1 }}>
              <div style={{
                fontSize: '8px',
                color: FINCEPT.GRAY,
                marginBottom: '2px',
                letterSpacing: '0.5px',
                fontWeight: 700
              }}>
                {suggestion.category.toUpperCase()}
              </div>
              <div style={{ fontSize: '10px', lineHeight: '1.4' }}>{suggestion.text}</div>
            </div>
          </button>
        ))}
      </div>
    </div>
  );
};

export default ChatSuggestions;
