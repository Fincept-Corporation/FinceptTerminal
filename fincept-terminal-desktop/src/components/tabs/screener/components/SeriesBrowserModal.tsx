import React, { memo, useCallback, useState, useEffect } from 'react';
import { Search, FolderTree, X, Plus, ArrowLeft, ChevronRight, Loader2, TrendingUp } from 'lucide-react';
import type { SearchResult, Category, CategoryPath } from '../types';
import { POPULAR_CATEGORIES, SEARCH_DEBOUNCE_MS } from '../constants';

interface SeriesBrowserModalProps {
  isOpen: boolean;
  onClose: () => void;
  onAddSeries: (id: string) => void;
  currentSeriesIds: Set<string>;
  searchResults: SearchResult[];
  searchLoading: boolean;
  categories: Category[];
  categoryLoading: boolean;
  categorySeriesResults: SearchResult[];
  categorySeriesLoading: boolean;
  onSearch: (query: string) => void;
  onLoadCategories: (categoryId: number) => void;
  onLoadCategorySeries: (categoryId: number) => void;
  colors: any;
  fontSize: any;
}

const SearchResultItem = memo(({
  result,
  isAdded,
  onAdd,
  colors
}: {
  result: SearchResult;
  isAdded: boolean;
  onAdd: () => void;
  colors: any;
}) => (
  <div
    style={{
      background: colors.panel,
      border: `1px solid ${colors.border || '#1a1a1a'}`,
      borderRadius: '4px',
      padding: '12px',
      display: 'flex',
      alignItems: 'flex-start',
      justifyContent: 'space-between',
      gap: '12px'
    }}
  >
    <div style={{ flex: 1, minWidth: 0 }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
        <span style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', fontFamily: 'monospace' }}>
          {result.id}
        </span>
        <span style={{ color: colors.textMuted, fontSize: '9px' }}>
          Pop: {result.popularity}
        </span>
      </div>
      <p style={{ color: colors.text, fontSize: '10px', marginBottom: '4px' }}>
        {result.title}
      </p>
      <div style={{ display: 'flex', gap: '12px', fontSize: '9px', color: colors.textMuted }}>
        <span>Freq: {result.frequency}</span>
        <span>Units: {result.units}</span>
        <span>Updated: {result.last_updated?.split('T')[0]}</span>
      </div>
    </div>
    <button
      onClick={onAdd}
      disabled={isAdded}
      style={{
        background: isAdded ? colors.success + '33' : colors.primary,
        border: 'none',
        color: isAdded ? colors.success : colors.background,
        padding: '6px 12px',
        fontSize: '10px',
        cursor: isAdded ? 'not-allowed' : 'pointer',
        borderRadius: '3px',
        fontWeight: 'bold',
        display: 'flex',
        alignItems: 'center',
        gap: '4px',
        whiteSpace: 'nowrap',
        opacity: isAdded ? 0.7 : 1
      }}
    >
      <Plus size={12} />
      {isAdded ? 'ADDED' : 'ADD'}
    </button>
  </div>
));

SearchResultItem.displayName = 'SearchResultItem';

const CategoryButton = memo(({
  category,
  onClick,
  colors
}: {
  category: { id: number; name: string };
  onClick: () => void;
  colors: any;
}) => (
  <button
    onClick={onClick}
    style={{
      background: colors.background,
      border: `1px solid ${colors.border || '#2a2a2a'}`,
      color: colors.text,
      padding: '8px 10px',
      fontSize: '10px',
      cursor: 'pointer',
      borderRadius: '3px',
      textAlign: 'left',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between'
    }}
  >
    <span>{category.name}</span>
    <ChevronRight size={12} color={colors.textMuted} />
  </button>
));

CategoryButton.displayName = 'CategoryButton';

function SeriesBrowserModal({
  isOpen,
  onClose,
  onAddSeries,
  currentSeriesIds,
  searchResults,
  searchLoading,
  categories,
  categoryLoading,
  categorySeriesResults,
  categorySeriesLoading,
  onSearch,
  onLoadCategories,
  onLoadCategorySeries,
  colors,
  fontSize,
}: SeriesBrowserModalProps) {
  const [searchQuery, setSearchQuery] = useState('');
  const [currentCategory, setCurrentCategory] = useState(0);
  const [categoryPath, setCategoryPath] = useState<CategoryPath[]>([{ id: 0, name: 'All Categories' }]);

  // Search debounce
  useEffect(() => {
    const timer = setTimeout(() => {
      if (searchQuery && isOpen) {
        onSearch(searchQuery);
      }
    }, SEARCH_DEBOUNCE_MS);
    return () => clearTimeout(timer);
  }, [searchQuery, isOpen, onSearch]);

  // Load initial categories
  useEffect(() => {
    if (isOpen && categories.length === 0) {
      onLoadCategories(0);
    }
  }, [isOpen, categories.length, onLoadCategories]);

  const handleNavigateToCategory = useCallback((cat: { id: number; name: string }) => {
    setCurrentCategory(cat.id);
    setCategoryPath(prev => [...prev, cat]);
    setSearchQuery('');

    // Load both subcategories and series
    onLoadCategories(cat.id);
    onLoadCategorySeries(cat.id);
  }, [onLoadCategories, onLoadCategorySeries]);

  const handleNavigateBack = useCallback(() => {
    if (categoryPath.length > 1) {
      const newPath = categoryPath.slice(0, -1);
      setCategoryPath(newPath);
      const parentId = newPath[newPath.length - 1].id;
      setCurrentCategory(parentId);
      onLoadCategories(parentId);
    }
  }, [categoryPath, onLoadCategories]);

  const handleAddSeries = useCallback((id: string) => {
    onAddSeries(id);
  }, [onAddSeries]);

  if (!isOpen) return null;

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      background: 'rgba(0,0,0,0.95)',
      zIndex: 1000,
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      padding: '20px',
      backdropFilter: 'blur(4px)'
    }}>
      <div style={{
        background: colors.background,
        border: `1px solid ${colors.primary}`,
        borderRadius: '4px',
        width: '90%',
        maxWidth: '1200px',
        height: '85vh',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        boxShadow: `0 8px 32px ${colors.primary}33`
      }}>
        {/* Modal Header */}
        <div style={{
          borderBottom: `1px solid ${colors.primary}`,
          padding: '12px 20px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          background: colors.panel
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <FolderTree size={16} color={colors.primary} />
            <span style={{ color: colors.primary, fontSize: fontSize.heading, fontWeight: 'bold', letterSpacing: '0.5px' }}>
              FRED SERIES BROWSER
            </span>
          </div>
          <button
            onClick={onClose}
            style={{
              background: 'transparent',
              border: 'none',
              color: colors.textMuted,
              cursor: 'pointer',
              padding: '4px',
              display: 'flex',
              alignItems: 'center'
            }}
          >
            <X size={18} />
          </button>
        </div>

        {/* Search Bar */}
        <div style={{ padding: '12px 20px', borderBottom: `1px solid ${colors.panel}` }}>
          <div style={{ position: 'relative' }}>
            <Search size={14} color={colors.textMuted} style={{ position: 'absolute', left: '12px', top: '50%', transform: 'translateY(-50%)' }} />
            <input
              type="text"
              placeholder="Search FRED series by keyword (e.g., 'unemployment', 'inflation', 'GDP')..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              style={{
                width: '100%',
                background: colors.panel,
                border: `1px solid ${colors.textMuted}33`,
                color: colors.text,
                padding: '10px 40px 10px 40px',
                fontSize: fontSize.small,
                borderRadius: '3px',
                outline: 'none',
                transition: 'border-color 0.2s'
              }}
            />
            {searchLoading && (
              <Loader2 size={14} className="animate-spin" style={{ position: 'absolute', right: '12px', top: '50%', transform: 'translateY(-50%)', color: colors.primary }} />
            )}
          </div>
        </div>

        {/* Content Area */}
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
          {/* Left Panel - Categories */}
          {!searchQuery && (
            <div style={{
              width: '350px',
              borderRight: `1px solid ${colors.border || '#1a1a1a'}`,
              display: 'flex',
              flexDirection: 'column',
              background: colors.background
            }}>
              {/* Breadcrumb */}
              <div style={{
                padding: '12px 16px',
                borderBottom: `1px solid ${colors.border || '#1a1a1a'}`,
                display: 'flex',
                alignItems: 'center',
                gap: '8px'
              }}>
                {categoryPath.length > 1 && (
                  <button
                    onClick={handleNavigateBack}
                    style={{
                      background: colors.panel,
                      border: `1px solid ${colors.border || '#2a2a2a'}`,
                      color: colors.primary,
                      padding: '4px 8px',
                      fontSize: '10px',
                      cursor: 'pointer',
                      borderRadius: '3px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px'
                    }}
                  >
                    <ArrowLeft size={12} />
                    BACK
                  </button>
                )}
                <span style={{ color: colors.textMuted, fontSize: '10px', flex: 1 }}>
                  {categoryPath.map((p, i) => (
                    <span key={`${p.id}-${i}`}>
                      {i > 0 && ' > '}
                      {p.name}
                    </span>
                  ))}
                </span>
              </div>

              {/* Popular Categories (only at root) */}
              {currentCategory === 0 && (
                <div style={{ padding: '12px', borderBottom: `1px solid ${colors.border || '#1a1a1a'}`, maxHeight: '200px', overflowY: 'auto' }}>
                  <h4 style={{ color: colors.primary, fontSize: '10px', marginBottom: '8px', fontWeight: 'bold' }}>
                    POPULAR CATEGORIES
                  </h4>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                    {POPULAR_CATEGORIES.map((cat, idx) => (
                      <CategoryButton
                        key={`pop-${cat.id}-${idx}`}
                        category={{ id: cat.id, name: cat.name }}
                        onClick={() => handleNavigateToCategory({ id: cat.id, name: cat.name })}
                        colors={colors}
                      />
                    ))}
                  </div>
                </div>
              )}

              {/* Category List */}
              <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
                <h4 style={{ color: colors.textMuted, fontSize: '9px', marginBottom: '8px', fontWeight: 'bold' }}>
                  {categoryLoading ? 'LOADING...' : `SUBCATEGORIES (${categories.length})`}
                </h4>
                {categories.length === 0 && !categoryLoading && currentCategory !== 0 && (
                  <p style={{ color: colors.textMuted, fontSize: '10px' }}>No subcategories found</p>
                )}
                <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                  {categories.map((cat, idx) => (
                    <CategoryButton
                      key={`cat-${cat.id}-${idx}`}
                      category={cat}
                      onClick={() => handleNavigateToCategory(cat)}
                      colors={colors}
                    />
                  ))}
                </div>
              </div>
            </div>
          )}

          {/* Right Panel - Results */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '16px 20px' }}>
            {categorySeriesLoading && !searchQuery ? (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '16px' }}>
                <Loader2 size={32} color={colors.primary} className="animate-spin" />
                <p style={{ color: colors.textMuted, fontSize: fontSize.small }}>
                  Loading series in category...
                </p>
              </div>
            ) : searchQuery ? (
              <>
                <h3 style={{ color: colors.primary, fontSize: '11px', marginBottom: '12px', fontWeight: 'bold' }}>
                  SEARCH RESULTS ({searchResults.length})
                </h3>
                {searchResults.length === 0 && !searchLoading && (
                  <p style={{ color: colors.textMuted, fontSize: '10px' }}>No results found</p>
                )}
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {searchResults.map((result) => (
                    <SearchResultItem
                      key={result.id}
                      result={result}
                      isAdded={currentSeriesIds.has(result.id)}
                      onAdd={() => handleAddSeries(result.id)}
                      colors={colors}
                    />
                  ))}
                </div>
              </>
            ) : categorySeriesResults.length > 0 ? (
              <>
                <h3 style={{ color: colors.primary, fontSize: '11px', marginBottom: '12px', fontWeight: 'bold' }}>
                  SERIES IN THIS CATEGORY ({categorySeriesResults.length})
                </h3>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {categorySeriesResults.map((result) => (
                    <SearchResultItem
                      key={result.id}
                      result={result}
                      isAdded={currentSeriesIds.has(result.id)}
                      onAdd={() => handleAddSeries(result.id)}
                      colors={colors}
                    />
                  ))}
                </div>
              </>
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '16px' }}>
                <TrendingUp size={48} color={colors.textMuted} style={{ opacity: 0.3 }} />
                <div style={{ textAlign: 'center', maxWidth: '500px' }}>
                  <p style={{ color: colors.textMuted, fontSize: '12px', marginBottom: '8px' }}>
                    <strong>Search for any economic data series</strong>
                  </p>
                  <p style={{ color: colors.textMuted, fontSize: '10px', lineHeight: '1.5', opacity: 0.7 }}>
                    Try searching for terms like "unemployment", "inflation", "GDP", "interest rates", "housing", etc.
                    Or browse through categories on the left to discover available data series.
                  </p>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Modal Footer */}
        <div style={{
          borderTop: `1px solid ${colors.border || '#1a1a1a'}`,
          padding: '12px 20px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          background: colors.panel
        }}>
          <span style={{ color: colors.textMuted, fontSize: '9px' }}>
            Federal Reserve Economic Data (FRED) | 800,000+ series available
          </span>
          <button
            onClick={onClose}
            style={{
              background: colors.primary,
              border: 'none',
              color: colors.background,
              padding: '8px 16px',
              fontSize: '10px',
              cursor: 'pointer',
              borderRadius: '3px',
              fontWeight: 'bold'
            }}
          >
            DONE
          </button>
        </div>
      </div>
    </div>
  );
}

export default memo(SeriesBrowserModal);
