// Notes Tab Toolbar Component

import React from 'react';
import { Search, Plus, BookOpen, Star, Filter, Archive } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { FINCEPT } from '../constants';

interface ToolbarProps {
  searchQuery: string;
  showFavorites: boolean;
  showArchived: boolean;
  showFilters: boolean;
  onSearchChange: (query: string) => void;
  onToggleFavorites: () => void;
  onToggleArchived: () => void;
  onToggleFilters: () => void;
  onShowTemplates: () => void;
  onCreateNew: () => void;
}

export const Toolbar: React.FC<ToolbarProps> = ({
  searchQuery,
  showFavorites,
  showArchived,
  showFilters,
  onSearchChange,
  onToggleFavorites,
  onToggleArchived,
  onToggleFilters,
  onShowTemplates,
  onCreateNew
}) => {
  const { t } = useTranslation('notes');

  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      padding: '8px 12px',
      display: 'flex',
      alignItems: 'center',
      gap: '12px',
      flexShrink: 0
    }}>
      {/* Search */}
      <div style={{ position: 'relative', flex: 1, maxWidth: '400px' }}>
        <Search size={14} style={{ position: 'absolute', left: '8px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.GRAY }} />
        <input
          type="text"
          placeholder={t('toolbar.search')}
          value={searchQuery}
          onChange={(e) => onSearchChange(e.target.value)}
          style={{
            width: '100%',
            padding: '6px 8px 6px 32px',
            backgroundColor: FINCEPT.HEADER_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.WHITE,
            fontSize: '11px',
            outline: 'none'
          }}
        />
      </div>

      {/* Action Buttons */}
      <button
        onClick={onCreateNew}
        style={{
          padding: '6px 12px',
          backgroundColor: FINCEPT.GREEN,
          border: 'none',
          color: FINCEPT.DARK_BG,
          cursor: 'pointer',
          fontSize: '10px',
          fontWeight: 700,
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}
      >
        <Plus size={14} />
        {t('toolbar.newNote')}
      </button>

      <button
        onClick={onShowTemplates}
        style={{
          padding: '6px 12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          color: FINCEPT.GRAY,
          cursor: 'pointer',
          fontSize: '10px',
          fontWeight: 700,
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}
      >
        <BookOpen size={14} />
        {t('toolbar.templates')}
      </button>

      <button
        onClick={onToggleFavorites}
        style={{
          padding: '6px 12px',
          backgroundColor: showFavorites ? FINCEPT.YELLOW : FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          color: showFavorites ? FINCEPT.DARK_BG : FINCEPT.GRAY,
          cursor: 'pointer',
          fontSize: '10px',
          fontWeight: 700,
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}
      >
        <Star size={14} fill={showFavorites ? FINCEPT.DARK_BG : 'none'} />
        {t('toolbar.favorites')}
      </button>

      <button
        onClick={onToggleFilters}
        style={{
          padding: '6px 12px',
          backgroundColor: showFilters ? FINCEPT.CYAN : FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          color: showFilters ? FINCEPT.DARK_BG : FINCEPT.GRAY,
          cursor: 'pointer',
          fontSize: '10px',
          fontWeight: 700,
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}
      >
        <Filter size={14} />
        {t('toolbar.filters')}
      </button>

      <button
        onClick={onToggleArchived}
        style={{
          padding: '6px 12px',
          backgroundColor: showArchived ? FINCEPT.PURPLE : FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          color: showArchived ? FINCEPT.WHITE : FINCEPT.GRAY,
          cursor: 'pointer',
          fontSize: '10px',
          fontWeight: 700,
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}
      >
        <Archive size={14} />
        {t('toolbar.archived')}
      </button>
    </div>
  );
};
