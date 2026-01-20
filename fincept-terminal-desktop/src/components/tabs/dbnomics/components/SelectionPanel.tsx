import React, { memo, useCallback, useMemo } from 'react';
import { Plus, Minus, Trash2 } from 'lucide-react';
import type { Provider, Dataset, Series, DataPoint } from '../types';

interface ListItemProps {
  code: string;
  name: string;
  isSelected: boolean;
  onClick: () => void;
  colors: any;
}

const ListItem = memo(({ code, name, isSelected, onClick, colors }: ListItemProps) => {
  const style = useMemo(() => ({
    padding: '6px 8px',
    fontSize: '10px',
    color: isSelected ? '#fff' : colors.text,
    backgroundColor: isSelected ? colors.primary : 'transparent',
    cursor: 'pointer',
    fontFamily: 'Consolas, monospace',
    lineHeight: '1.4',
    wordWrap: 'break-word' as const,
    whiteSpace: 'normal' as const,
    borderBottom: `1px solid ${colors.panel}`,
  }), [isSelected, colors]);

  return (
    <div
      onClick={onClick}
      style={style}
      onMouseEnter={(e) => {
        if (!isSelected) e.currentTarget.style.backgroundColor = '#2a2a2a';
      }}
      onMouseLeave={(e) => {
        if (!isSelected) e.currentTarget.style.backgroundColor = 'transparent';
      }}
    >
      {code} - {name}
    </div>
  );
});

ListItem.displayName = 'ListItem';

interface SelectionListProps {
  title: string;
  items: Array<{ code: string; name: string; full_id?: string }>;
  selectedId: string | null;
  onSelect: (item: any) => void;
  idKey?: 'code' | 'full_id';
  colors: any;
}

const SelectionList = memo(({ title, items, selectedId, onSelect, idKey = 'code', colors }: SelectionListProps) => {
  const containerStyle = useMemo(() => ({
    width: '100%',
    height: '140px',
    backgroundColor: '#1a1a1a',
    border: `1px solid ${colors.textMuted}`,
    marginBottom: '12px',
    overflowY: 'auto' as const,
    overflowX: 'hidden' as const,
  }), [colors.textMuted]);

  return (
    <>
      <div style={{ color: colors.primary, fontSize: '10px', marginBottom: '4px' }}>{title}</div>
      <div style={containerStyle}>
        {items.map((item) => {
          const itemId = idKey === 'full_id' ? (item as Series).full_id : item.code;
          return (
            <ListItem
              key={itemId}
              code={item.code}
              name={item.name}
              isSelected={selectedId === itemId}
              onClick={() => onSelect(item)}
              colors={colors}
            />
          );
        })}
      </div>
    </>
  );
});

SelectionList.displayName = 'SelectionList';

interface SlotItemProps {
  slot: DataPoint[];
  slotIndex: number;
  currentData: DataPoint | null;
  onAddToSlot: (index: number) => void;
  onRemoveSlot: (index: number) => void;
  onRemoveFromSlot: (slotIndex: number, seriesIndex: number) => void;
  canRemove: boolean;
  colors: any;
}

const SlotItem = memo(({
  slot,
  slotIndex,
  currentData,
  onAddToSlot,
  onRemoveSlot,
  onRemoveFromSlot,
  canRemove,
  colors,
}: SlotItemProps) => (
  <div style={{
    marginBottom: '6px',
    padding: '6px',
    backgroundColor: '#1a1a1a',
    border: `1px solid ${colors.textMuted}`,
    borderRadius: '2px',
  }}>
    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
      <span style={{ color: colors.warning, fontSize: '9px' }}>SLOT {slotIndex + 1} ({slot.length})</span>
      <div style={{ display: 'flex', gap: '2px' }}>
        <button
          onClick={() => onAddToSlot(slotIndex)}
          disabled={!currentData}
          style={{
            padding: '2px 4px',
            backgroundColor: colors.panel,
            color: colors.secondary,
            border: `1px solid ${colors.textMuted}`,
            cursor: !currentData ? 'not-allowed' : 'pointer',
            fontSize: '8px',
          }}
        >
          <Plus size={8} />
        </button>
        {canRemove && (
          <button
            onClick={() => onRemoveSlot(slotIndex)}
            style={{
              padding: '2px 4px',
              backgroundColor: colors.panel,
              color: colors.alert,
              border: `1px solid ${colors.textMuted}`,
              cursor: 'pointer',
              fontSize: '8px',
            }}
          >
            <Minus size={8} />
          </button>
        )}
      </div>
    </div>
    {slot.map((s, sIdx) => (
      <div
        key={sIdx}
        style={{
          fontSize: '8px',
          color: colors.textMuted,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '2px',
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
            color: colors.alert,
            border: 'none',
            cursor: 'pointer',
            fontSize: '8px',
          }}
        >
          ×
        </button>
      </div>
    ))}
    {slot.length === 0 && (
      <div style={{ fontSize: '8px', color: colors.textMuted, textAlign: 'center' }}>Empty</div>
    )}
  </div>
));

SlotItem.displayName = 'SlotItem';

interface SelectionPanelProps {
  providers: Provider[];
  datasets: Dataset[];
  seriesList: Series[];
  selectedProvider: string | null;
  selectedDataset: string | null;
  selectedSeries: string | null;
  currentData: DataPoint | null;
  slots: DataPoint[][];
  onSelectProvider: (code: string) => void;
  onSelectDataset: (code: string) => void;
  onSelectSeries: (series: Series) => void;
  onAddToSingleView: () => void;
  onClearAll: () => void;
  onAddSlot: () => void;
  onRemoveSlot: (index: number) => void;
  onAddToSlot: (index: number) => void;
  onRemoveFromSlot: (slotIndex: number, seriesIndex: number) => void;
  colors: any;
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
  onSelectProvider,
  onSelectDataset,
  onSelectSeries,
  onAddToSingleView,
  onClearAll,
  onAddSlot,
  onRemoveSlot,
  onAddToSlot,
  onRemoveFromSlot,
  colors,
}: SelectionPanelProps) {
  const handleProviderSelect = useCallback((provider: Provider) => {
    onSelectProvider(provider.code);
  }, [onSelectProvider]);

  const handleDatasetSelect = useCallback((dataset: Dataset) => {
    onSelectDataset(dataset.code);
  }, [onSelectDataset]);

  const containerStyle = useMemo(() => ({
    width: '320px',
    borderRight: `1px solid ${colors.textMuted}`,
    display: 'flex',
    flexDirection: 'column' as const,
    backgroundColor: colors.panel,
  }), [colors.textMuted, colors.panel]);

  return (
    <div style={containerStyle}>
      <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden', padding: '8px' }}>
        <SelectionList
          title="1. PROVIDER"
          items={providers}
          selectedId={selectedProvider}
          onSelect={handleProviderSelect}
          colors={colors}
        />

        <SelectionList
          title="2. DATASET"
          items={datasets}
          selectedId={selectedDataset}
          onSelect={handleDatasetSelect}
          colors={colors}
        />

        <SelectionList
          title="3. SERIES"
          items={seriesList}
          selectedId={selectedSeries}
          onSelect={onSelectSeries}
          idKey="full_id"
          colors={colors}
        />

        <div style={{ height: '1px', backgroundColor: colors.textMuted, margin: '8px 0' }} />

        {/* Actions */}
        <div style={{ display: 'flex', gap: '4px', marginBottom: '8px' }}>
          <button
            onClick={onAddToSingleView}
            disabled={!currentData}
            style={{
              flex: 1,
              padding: '4px',
              backgroundColor: '#1a1a1a',
              color: colors.text,
              border: `1px solid ${colors.textMuted}`,
              cursor: !currentData ? 'not-allowed' : 'pointer',
              fontSize: '9px',
            }}
          >
            ADD TO SINGLE
          </button>
          <button
            onClick={onClearAll}
            style={{
              padding: '4px 8px',
              backgroundColor: '#1a1a1a',
              color: colors.alert,
              border: `1px solid ${colors.textMuted}`,
              cursor: 'pointer',
              fontSize: '9px',
            }}
          >
            <Trash2 size={10} />
          </button>
        </div>

        <div style={{
          color: colors.primary,
          fontSize: '10px',
          marginBottom: '6px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}>
          <span>COMPARISON SLOTS</span>
          <button
            onClick={onAddSlot}
            style={{
              padding: '2px 6px',
              backgroundColor: '#1a1a1a',
              color: colors.secondary,
              border: `1px solid ${colors.textMuted}`,
              cursor: 'pointer',
              fontSize: '9px',
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
              colors={colors}
            />
          ))}
        </div>
      </div>
    </div>
  );
}

export default memo(SelectionPanel);
