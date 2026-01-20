// Notes Tab Category Sidebar Component

import React from 'react';
import { FINCEPT, CATEGORIES } from '../constants';
import type { NoteStatistics } from '../types';

interface CategorySidebarProps {
  selectedCategory: string;
  statistics: NoteStatistics | null;
  onSelectCategory: (categoryId: string) => void;
}

export const CategorySidebar: React.FC<CategorySidebarProps> = ({
  selectedCategory,
  statistics,
  onSelectCategory
}) => {
  return (
    <div style={{
      width: '220px',
      backgroundColor: FINCEPT.PANEL_BG,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      <div style={{
        padding: '8px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '10px',
        fontWeight: 700,
        color: FINCEPT.ORANGE,
        letterSpacing: '0.5px'
      }}>
        CATEGORIES
      </div>

      <div style={{ flex: 1, overflow: 'auto' }}>
        {CATEGORIES.map(category => {
          const Icon = category.icon;
          const count = statistics?.byCategory?.find((s: any) => s.category === category.id)?.count || 0;
          const isSelected = selectedCategory === category.id;

          return (
            <div
              key={category.id}
              onClick={() => onSelectCategory(category.id)}
              style={{
                padding: '10px 12px',
                cursor: 'pointer',
                backgroundColor: isSelected ? `${FINCEPT.ORANGE}20` : 'transparent',
                borderLeft: isSelected ? `3px solid ${FINCEPT.ORANGE}` : '3px solid transparent',
                transition: 'all 0.2s',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between'
              }}
              onMouseEnter={(e) => {
                if (!isSelected) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }
              }}
              onMouseLeave={(e) => {
                if (!isSelected) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <Icon size={14} color={isSelected ? FINCEPT.ORANGE : FINCEPT.GRAY} />
                <span style={{
                  fontSize: '10px',
                  fontWeight: 600,
                  color: isSelected ? FINCEPT.ORANGE : FINCEPT.WHITE
                }}>
                  {category.label}
                </span>
              </div>
              {category.id !== 'ALL' && (
                <span style={{
                  fontSize: '9px',
                  color: FINCEPT.CYAN,
                  fontWeight: 700
                }}>
                  {count}
                </span>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
};
