import React, { memo, useCallback, useMemo } from 'react';
import { Plus, Minus, Trash2, Layers, Search, ChevronDown, Globe } from 'lucide-react';
import type { Provider, Dataset, Series, DataPoint, PaginationState, SearchResult } from '../types';

// Fincept Design System Colors
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
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

// ─── Search Input ───
const SearchInput = memo(({
  value,
  onChange,
  placeholder,
}: {
  value: string;
  onChange: (val: string) => void;
  placeholder: string;
}) => (
  <div style={{ position: 'relative', padding: '6px 12px' }}>
    <Search size={10} style={{ position: 'absolute', left: '18px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.MUTED }} />
    <input
      type="text"
      value={value}
      onChange={(e) => onChange(e.target.value)}
      placeholder={placeholder}
      style={{
        width: '100%',
        padding: '8px 10px 8px 24px',
        backgroundColor: FINCEPT.DARK_BG,
        color: FINCEPT.WHITE,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        fontSize: '10px',
        fontFamily: FONT_FAMILY,
        outline: 'none',
      }}
      onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
      onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
    />
  </div>
));

SearchInput.displayName = 'SearchInput';

// ─── Load More Button ───
const LoadMoreButton = memo(({
  pagination,
  loading,
  onLoadMore,
}: {
  pagination: PaginationState;
  loading: boolean;
  onLoadMore: () => void;
}) => {
  if (!pagination.hasMore) return null;

  return (
    <button
      onClick={onLoadMore}
      disabled={loading}
      style={{
        width: '100%',
        padding: '6px',
        backgroundColor: FINCEPT.HEADER_BG,
        color: loading ? FINCEPT.MUTED : FINCEPT.CYAN,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        fontSize: '9px',
        fontWeight: 700,
        fontFamily: FONT_FAMILY,
        cursor: loading ? 'not-allowed' : 'pointer',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        gap: '4px',
        opacity: loading ? 0.5 : 1,
        transition: 'all 0.2s',
        letterSpacing: '0.5px',
      }}
    >
      <ChevronDown size={10} />
      {loading ? 'LOADING...' : `LOAD MORE (${pagination.offset} OF ${pagination.total})`}
    </button>
  );
});

LoadMoreButton.displayName = 'LoadMoreButton';

// ─── List Item ───
interface ListItemProps {
  code: string;
  name: string;
  isSelected: boolean;
  onClick: () => void;
}

const ListItem = memo(({ code, name, isSelected, onClick }: ListItemProps) => (
  <div
    onClick={onClick}
    style={{
      padding: '10px 12px',
      fontSize: '10px',
      color: FINCEPT.WHITE,
      backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : 'transparent',
      borderLeft: isSelected ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
      cursor: 'pointer',
      fontFamily: FONT_FAMILY,
      lineHeight: '1.4',
      wordWrap: 'break-word',
      whiteSpace: 'normal',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      transition: 'all 0.2s',
    }}
    onMouseEnter={(e) => {
      if (!isSelected) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
    }}
    onMouseLeave={(e) => {
      if (!isSelected) e.currentTarget.style.backgroundColor = 'transparent';
    }}
  >
    <span style={{ color: FINCEPT.CYAN, fontSize: '9px' }}>{code}</span>
    <span style={{ color: FINCEPT.GRAY }}> — </span>
    <span>{name}</span>
  </div>
));

ListItem.displayName = 'ListItem';

// ─── Selection List with Search + Pagination ───
interface SelectionListProps {
  title: string;
  items: Array<{ code: string; name: string; full_id?: string }>;
  selectedId: string | null;
  onSelect: (item: any) => void;
  idKey?: 'code' | 'full_id';
  searchValue: string;
  onSearchChange: (val: string) => void;
  searchPlaceholder: string;
  pagination?: PaginationState;
  onLoadMore?: () => void;
  loading?: boolean;
  clientSideFilter?: boolean;
}

const SelectionList = memo(({
  title,
  items,
  selectedId,
  onSelect,
  idKey = 'code',
  searchValue,
  onSearchChange,
  searchPlaceholder,
  pagination,
  onLoadMore,
  loading = false,
  clientSideFilter = false,
}: SelectionListProps) => {
  // If client-side filter, filter items by search value
  const filteredItems = useMemo(() => {
    if (!clientSideFilter || !searchValue.trim()) return items;
    const q = searchValue.toLowerCase();
    return items.filter(
      item => item.code.toLowerCase().includes(q) || item.name.toLowerCase().includes(q)
    );
  }, [items, searchValue, clientSideFilter]);

  const total = pagination?.total ?? filteredItems.length;

  return (
    <div style={{ marginBottom: '4px' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
          {title}
        </span>
        <span style={{ fontSize: '9px', color: FINCEPT.CYAN, marginLeft: '8px' }}>
          {total}
        </span>
      </div>
      <SearchInput value={searchValue} onChange={onSearchChange} placeholder={searchPlaceholder} />
      <div style={{
        height: '140px',
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderTop: 'none',
        borderRadius: '0 0 2px 2px',
        overflowY: 'auto',
        overflowX: 'hidden',
      }}>
        {filteredItems.length === 0 ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.MUTED,
            fontSize: '9px',
            fontFamily: FONT_FAMILY,
          }}>
            {searchValue ? 'NO MATCHES' : 'NO DATA'}
          </div>
        ) : (
          <>
            {filteredItems.map((item) => {
              const itemId = idKey === 'full_id' ? (item as Series).full_id : item.code;
              return (
                <ListItem
                  key={itemId}
                  code={item.code}
                  name={item.name}
                  isSelected={selectedId === itemId}
                  onClick={() => onSelect(item)}
                />
              );
            })}
            {pagination && onLoadMore && (
              <div style={{ padding: '4px 6px' }}>
                <LoadMoreButton pagination={pagination} loading={loading} onLoadMore={onLoadMore} />
              </div>
            )}
          </>
        )}
      </div>
    </div>
  );
});

SelectionList.displayName = 'SelectionList';

// ─── Global Search Result Item ───
const SearchResultItem = memo(({
  result,
  onClick,
}: {
  result: SearchResult;
  onClick: () => void;
}) => (
  <div
    onClick={onClick}
    style={{
      padding: '10px 12px',
      fontSize: '10px',
      color: FINCEPT.WHITE,
      backgroundColor: 'transparent',
      borderLeft: '2px solid transparent',
      cursor: 'pointer',
      fontFamily: FONT_FAMILY,
      lineHeight: '1.4',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      transition: 'all 0.2s',
    }}
    onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
    onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
  >
    <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginBottom: '2px' }}>
      <span style={{ color: FINCEPT.ORANGE, fontSize: '9px', fontWeight: 700 }}>{result.provider_code}</span>
      <span style={{ color: FINCEPT.GRAY }}>/</span>
      <span style={{ color: FINCEPT.CYAN, fontSize: '9px' }}>{result.dataset_code}</span>
    </div>
    <div style={{ color: FINCEPT.GRAY, fontSize: '9px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
      {result.dataset_name}
    </div>
    <div style={{ marginTop: '2px' }}>
      <span style={{
        padding: '1px 4px',
        backgroundColor: `${FINCEPT.CYAN}20`,
        color: FINCEPT.CYAN,
        fontSize: '8px',
        fontWeight: 700,
        borderRadius: '2px',
      }}>
        {result.nb_series} SERIES
      </span>
    </div>
  </div>
));

SearchResultItem.displayName = 'SearchResultItem';

// ─── Slot Item ───
interface SlotItemProps {
  slot: DataPoint[];
  slotIndex: number;
  currentData: DataPoint | null;
  onAddToSlot: (index: number) => void;
  onRemoveSlot: (index: number) => void;
  onRemoveFromSlot: (slotIndex: number, seriesIndex: number) => void;
  canRemove: boolean;
}

const SlotItem = memo(({
  slot,
  slotIndex,
  currentData,
  onAddToSlot,
  onRemoveSlot,
  onRemoveFromSlot,
  canRemove,
}: SlotItemProps) => (
  <div style={{
    marginBottom: '6px',
    backgroundColor: FINCEPT.PANEL_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
  }}>
    <div style={{
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      padding: '8px',
      borderBottom: slot.length > 0 ? `1px solid ${FINCEPT.BORDER}` : 'none',
    }}>
      <span style={{ color: FINCEPT.YELLOW, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: FONT_FAMILY }}>
        SLOT {slotIndex + 1} <span style={{ color: FINCEPT.CYAN }}>({slot.length})</span>
      </span>
      <div style={{ display: 'flex', gap: '4px' }}>
        <button
          onClick={() => onAddToSlot(slotIndex)}
          disabled={!currentData}
          style={{
            padding: '2px 6px',
            backgroundColor: 'transparent',
            color: !currentData ? FINCEPT.MUTED : FINCEPT.GREEN,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            cursor: !currentData ? 'not-allowed' : 'pointer',
            fontSize: '8px',
            fontFamily: FONT_FAMILY,
            opacity: !currentData ? 0.5 : 1,
            transition: 'all 0.2s',
          }}
        >
          <Plus size={8} />
        </button>
        {canRemove && (
          <button
            onClick={() => onRemoveSlot(slotIndex)}
            style={{
              padding: '2px 6px',
              backgroundColor: 'transparent',
              color: FINCEPT.RED,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              cursor: 'pointer',
              fontSize: '8px',
              fontFamily: FONT_FAMILY,
              transition: 'all 0.2s',
            }}
          >
            <Minus size={8} />
          </button>
        )}
      </div>
    </div>
    {slot.length > 0 && (
      <div style={{ padding: '4px 8px' }}>
        {slot.map((s, sIdx) => (
          <div
            key={sIdx}
            style={{
              fontSize: '9px',
              color: FINCEPT.GRAY,
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              padding: '2px 0',
            }}
          >
            <span
              style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}
              title={s.series_name}
            >
              {s.series_name}
            </span>
            <button
              onClick={() => onRemoveFromSlot(slotIndex, sIdx)}
              style={{
                padding: '1px 3px',
                backgroundColor: 'transparent',
                color: FINCEPT.RED,
                border: 'none',
                cursor: 'pointer',
                fontSize: '8px',
                fontFamily: FONT_FAMILY,
              }}
            >
              ×
            </button>
          </div>
        ))}
      </div>
    )}
    {slot.length === 0 && (
      <div style={{ fontSize: '8px', color: FINCEPT.MUTED, textAlign: 'center', padding: '6px', fontFamily: FONT_FAMILY }}>
        EMPTY
      </div>
    )}
  </div>
));

SlotItem.displayName = 'SlotItem';

// ─── Selection Panel ───
interface SelectionPanelProps {
  providers: Provider[];
  datasets: Dataset[];
  seriesList: Series[];
  selectedProvider: string | null;
  selectedDataset: string | null;
  selectedSeries: string | null;
  currentData: DataPoint | null;
  slots: DataPoint[][];
  loading: boolean;
  // Search
  providerSearch: string;
  datasetSearch: string;
  seriesSearch: string;
  globalSearch: string;
  globalSearchResults: SearchResult[];
  // Pagination
  datasetsPagination: PaginationState;
  seriesPagination: PaginationState;
  searchPagination: PaginationState;
  // Callbacks
  onSelectProvider: (code: string) => void;
  onSelectDataset: (code: string) => void;
  onSelectSeries: (series: Series) => void;
  onAddToSingleView: () => void;
  onClearAll: () => void;
  onAddSlot: () => void;
  onRemoveSlot: (index: number) => void;
  onAddToSlot: (index: number) => void;
  onRemoveFromSlot: (slotIndex: number, seriesIndex: number) => void;
  onProviderSearchChange: (q: string) => void;
  onDatasetSearchChange: (q: string) => void;
  onSeriesSearchChange: (q: string) => void;
  onGlobalSearchChange: (q: string) => void;
  onSelectSearchResult: (result: SearchResult) => void;
  onLoadMoreDatasets: () => void;
  onLoadMoreSeries: () => void;
  onLoadMoreSearchResults: () => void;
}

function SelectionPanel({
  providers,
  datasets,
  seriesList,
  selectedProvider,
  selectedDataset,
  selectedSeries,
  currentData,
  slots,
  loading,
  providerSearch,
  datasetSearch,
  seriesSearch,
  globalSearch,
  globalSearchResults,
  datasetsPagination,
  seriesPagination,
  searchPagination,
  onSelectProvider,
  onSelectDataset,
  onSelectSeries,
  onAddToSingleView,
  onClearAll,
  onAddSlot,
  onRemoveSlot,
  onAddToSlot,
  onRemoveFromSlot,
  onProviderSearchChange,
  onDatasetSearchChange,
  onSeriesSearchChange,
  onGlobalSearchChange,
  onSelectSearchResult,
  onLoadMoreDatasets,
  onLoadMoreSeries,
  onLoadMoreSearchResults,
}: SelectionPanelProps) {
  const handleProviderSelect = useCallback((provider: Provider) => {
    onSelectProvider(provider.code);
  }, [onSelectProvider]);

  const handleDatasetSelect = useCallback((dataset: Dataset) => {
    onSelectDataset(dataset.code);
  }, [onSelectDataset]);

  return (
    <div style={{
      width: '280px',
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.PANEL_BG,
    }}>
      <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>

        {/* ─── Global Search ─── */}
        <div style={{ marginBottom: '4px' }}>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}>
            <Globe size={10} style={{ color: FINCEPT.ORANGE }} />
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              GLOBAL SEARCH
            </span>
            {globalSearchResults.length > 0 && (
              <span style={{ fontSize: '9px', color: FINCEPT.CYAN, marginLeft: 'auto' }}>
                {searchPagination.total}
              </span>
            )}
          </div>
          <SearchInput
            value={globalSearch}
            onChange={onGlobalSearchChange}
            placeholder="Search all DBnomics (e.g. GDP France)"
          />
          {globalSearchResults.length > 0 && (
            <div style={{
              maxHeight: '180px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderTop: 'none',
              borderRadius: '0 0 2px 2px',
              overflowY: 'auto',
            }}>
              {globalSearchResults.map((result, idx) => (
                <SearchResultItem
                  key={`${result.provider_code}/${result.dataset_code}-${idx}`}
                  result={result}
                  onClick={() => onSelectSearchResult(result)}
                />
              ))}
              <div style={{ padding: '4px 6px' }}>
                <LoadMoreButton pagination={searchPagination} loading={loading} onLoadMore={onLoadMoreSearchResults} />
              </div>
            </div>
          )}
        </div>

        {/* ─── Provider List ─── */}
        <SelectionList
          title="1. PROVIDER"
          items={providers}
          selectedId={selectedProvider}
          onSelect={handleProviderSelect}
          searchValue={providerSearch}
          onSearchChange={onProviderSearchChange}
          searchPlaceholder="Filter providers..."
          clientSideFilter
        />

        {/* ─── Dataset List ─── */}
        <SelectionList
          title="2. DATASET"
          items={datasets}
          selectedId={selectedDataset}
          onSelect={handleDatasetSelect}
          searchValue={datasetSearch}
          onSearchChange={onDatasetSearchChange}
          searchPlaceholder="Filter datasets..."
          pagination={datasetsPagination}
          onLoadMore={onLoadMoreDatasets}
          loading={loading}
          clientSideFilter
        />

        {/* ─── Series List ─── */}
        <SelectionList
          title="3. SERIES"
          items={seriesList}
          selectedId={selectedSeries}
          onSelect={onSelectSeries}
          idKey="full_id"
          searchValue={seriesSearch}
          onSearchChange={onSeriesSearchChange}
          searchPlaceholder="Search series (server-side)..."
          pagination={seriesPagination}
          onLoadMore={onLoadMoreSeries}
          loading={loading}
        />

        {/* Divider */}
        <div style={{ height: '1px', backgroundColor: FINCEPT.BORDER, margin: '4px 0' }} />

        {/* Actions */}
        <div style={{ padding: '8px 12px' }}>
          <div style={{ display: 'flex', gap: '4px', marginBottom: '8px' }}>
            <button
              onClick={onAddToSingleView}
              disabled={!currentData}
              style={{
                flex: 1,
                padding: '8px 16px',
                backgroundColor: !currentData ? 'transparent' : FINCEPT.ORANGE,
                color: !currentData ? FINCEPT.MUTED : FINCEPT.DARK_BG,
                border: !currentData ? `1px solid ${FINCEPT.BORDER}` : 'none',
                borderRadius: '2px',
                cursor: !currentData ? 'not-allowed' : 'pointer',
                fontSize: '9px',
                fontWeight: 700,
                fontFamily: FONT_FAMILY,
                opacity: !currentData ? 0.5 : 1,
                transition: 'all 0.2s',
              }}
            >
              ADD TO SINGLE
            </button>
            <button
              onClick={onClearAll}
              style={{
                padding: '8px',
                backgroundColor: 'transparent',
                color: FINCEPT.RED,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                cursor: 'pointer',
                fontSize: '9px',
                fontFamily: FONT_FAMILY,
                transition: 'all 0.2s',
              }}
            >
              <Trash2 size={10} />
            </button>
          </div>

          {/* Comparison Slots Section Header */}
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            marginBottom: '8px',
            borderRadius: '2px 2px 0 0',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Layers size={10} style={{ color: FINCEPT.ORANGE }} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                COMPARISON SLOTS
              </span>
            </div>
            <button
              onClick={onAddSlot}
              style={{
                padding: '2px 6px',
                backgroundColor: 'transparent',
                color: FINCEPT.GREEN,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                cursor: 'pointer',
                fontSize: '9px',
                fontFamily: FONT_FAMILY,
                transition: 'all 0.2s',
              }}
            >
              <Plus size={10} />
            </button>
          </div>

          <div style={{ maxHeight: '300px', overflowY: 'auto' }}>
            {slots.map((slot, idx) => (
              <SlotItem
                key={idx}
                slot={slot}
                slotIndex={idx}
                currentData={currentData}
                onAddToSlot={onAddToSlot}
                onRemoveSlot={onRemoveSlot}
                onRemoveFromSlot={onRemoveFromSlot}
                canRemove={slots.length > 1}
              />
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}

export default memo(SelectionPanel);
