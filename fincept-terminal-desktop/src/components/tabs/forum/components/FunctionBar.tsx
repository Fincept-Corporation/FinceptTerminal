// Forum Tab Function Bar Component

import React from 'react';
import type { ForumColors, ForumCategoryUI } from '../types';

interface FunctionBarProps {
  colors: ForumColors;
  activeCategory: string;
  categories: ForumCategoryUI[];
  onCategoryChange: (category: string) => void;
  onCreatePost: () => void;
  onSearch: () => void;
  onViewProfile: () => void;
  onEditProfile: () => void;
}

export const FunctionBar: React.FC<FunctionBarProps> = ({
  colors,
  activeCategory,
  categories,
  onCategoryChange,
  onCreatePost,
  onSearch,
  onViewProfile,
  onEditProfile
}) => {
  const functionKeys = [
    { key: "F1", label: "ALL", action: () => onCategoryChange('ALL') },
    { key: "F2", label: categories[1]?.name || "MARKETS", action: () => onCategoryChange(categories[1]?.name || "MARKETS") },
    { key: "F3", label: categories[2]?.name || "TECH", action: () => onCategoryChange(categories[2]?.name || "TECH") },
    { key: "F4", label: categories[3]?.name || "CRYPTO", action: () => onCategoryChange(categories[3]?.name || "CRYPTO") },
    { key: "F5", label: categories[4]?.name || "OPTIONS", action: () => onCategoryChange(categories[4]?.name || "OPTIONS") },
    { key: "F6", label: categories[5]?.name || "BANKING", action: () => onCategoryChange(categories[5]?.name || "BANKING") },
    { key: "F7", label: categories[6]?.name || "ENERGY", action: () => onCategoryChange(categories[6]?.name || "ENERGY") },
    { key: "F8", label: categories[7]?.name || "MACRO", action: () => onCategoryChange(categories[7]?.name || "MACRO") },
    { key: "F9", label: "POST", action: onCreatePost },
    { key: "F10", label: "SEARCH", action: onSearch },
    { key: "F11", label: "PROFILE", action: onViewProfile },
    { key: "F12", label: "SETTINGS", action: onEditProfile }
  ];

  return (
    <div style={{
      backgroundColor: colors.PANEL_BG,
      borderBottom: `1px solid ${colors.GRAY}`,
      padding: '2px 4px',
      flexShrink: 0
    }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '2px' }}>
        {functionKeys.map(item => (
          <button
            key={item.key}
            onClick={item.action}
            style={{
              backgroundColor: activeCategory === item.label ? colors.ORANGE : colors.DARK_BG,
              border: `1px solid ${colors.GRAY}`,
              color: activeCategory === item.label ? colors.DARK_BG : colors.WHITE,
              padding: '2px 4px',
              fontSize: '9px',
              height: '16px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontWeight: activeCategory === item.label ? 'bold' : 'normal',
              cursor: 'pointer'
            }}
          >
            <span style={{ color: colors.YELLOW }}>{item.key}:</span>
            <span style={{ marginLeft: '2px' }}>{item.label}</span>
          </button>
        ))}
      </div>
    </div>
  );
};
