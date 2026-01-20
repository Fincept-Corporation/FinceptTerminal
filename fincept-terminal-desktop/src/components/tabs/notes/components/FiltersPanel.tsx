// Notes Tab Filters Panel Component

import React from 'react';
import { FINCEPT, PRIORITIES, SENTIMENTS, getPriorityColor, getSentimentColor } from '../constants';

interface FiltersPanelProps {
  filterPriority: string | null;
  filterSentiment: string | null;
  onPriorityChange: (priority: string | null) => void;
  onSentimentChange: (sentiment: string | null) => void;
  onClearFilters: () => void;
}

export const FiltersPanel: React.FC<FiltersPanelProps> = ({
  filterPriority,
  filterSentiment,
  onPriorityChange,
  onSentimentChange,
  onClearFilters
}) => {
  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      padding: '8px 12px',
      display: 'flex',
      alignItems: 'center',
      gap: '16px',
      flexShrink: 0,
      fontSize: '10px'
    }}>
      <span style={{ color: FINCEPT.ORANGE, fontWeight: 700 }}>FILTERS:</span>

      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
        <span style={{ color: FINCEPT.GRAY }}>Priority:</span>
        {PRIORITIES.map(priority => (
          <button
            key={priority}
            onClick={() => onPriorityChange(filterPriority === priority ? null : priority)}
            style={{
              padding: '4px 8px',
              backgroundColor: filterPriority === priority ? getPriorityColor(priority) : FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: filterPriority === priority ? FINCEPT.DARK_BG : getPriorityColor(priority),
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700
            }}
          >
            {priority}
          </button>
        ))}
      </div>

      <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
        <span style={{ color: FINCEPT.GRAY }}>Sentiment:</span>
        {SENTIMENTS.map(sentiment => (
          <button
            key={sentiment}
            onClick={() => onSentimentChange(filterSentiment === sentiment ? null : sentiment)}
            style={{
              padding: '4px 8px',
              backgroundColor: filterSentiment === sentiment ? getSentimentColor(sentiment) : FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: filterSentiment === sentiment ? FINCEPT.DARK_BG : getSentimentColor(sentiment),
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700
            }}
          >
            {sentiment}
          </button>
        ))}
      </div>

      <button
        onClick={onClearFilters}
        style={{
          marginLeft: 'auto',
          padding: '4px 8px',
          backgroundColor: FINCEPT.RED,
          border: 'none',
          color: FINCEPT.WHITE,
          cursor: 'pointer',
          fontSize: '9px',
          fontWeight: 700
        }}
      >
        CLEAR FILTERS
      </button>
    </div>
  );
};
