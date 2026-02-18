import React, { useState, useMemo } from 'react';
import { ChevronUp, ChevronDown } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY } from '../../portfolio-tab/finceptStyles';

interface Column {
  key: string;
  label: string;
  width?: string;
  align?: 'left' | 'center' | 'right';
  format?: (value: any, row: any) => React.ReactNode;
  sortable?: boolean;
}

interface MADataTableProps {
  columns: Column[];
  data: Record<string, any>[];
  accentColor?: string;
  compact?: boolean;
  maxHeight?: string;
  onRowClick?: (row: any, index: number) => void;
  selectedIndex?: number;
  emptyMessage?: string;
}

export const MADataTable: React.FC<MADataTableProps> = ({
  columns,
  data,
  accentColor = FINCEPT.ORANGE,
  compact = false,
  maxHeight = '400px',
  onRowClick,
  selectedIndex,
  emptyMessage = 'No data available',
}) => {
  const [sortKey, setSortKey] = useState<string | null>(null);
  const [sortDir, setSortDir] = useState<'asc' | 'desc'>('asc');

  const sortedData = useMemo(() => {
    if (!sortKey) return data;
    return [...data].sort((a, b) => {
      const aVal = a[sortKey];
      const bVal = b[sortKey];
      if (aVal == null && bVal == null) return 0;
      if (aVal == null) return 1;
      if (bVal == null) return -1;
      const cmp = typeof aVal === 'number' ? aVal - bVal : String(aVal).localeCompare(String(bVal));
      return sortDir === 'asc' ? cmp : -cmp;
    });
  }, [data, sortKey, sortDir]);

  const handleSort = (key: string) => {
    if (sortKey === key) {
      setSortDir(d => d === 'asc' ? 'desc' : 'asc');
    } else {
      setSortKey(key);
      setSortDir('asc');
    }
  };

  const cellPad = compact ? '4px 8px' : '6px 12px';
  const rowHeight = compact ? '26px' : '32px';

  return (
    <div style={{
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
      }}>
        {columns.map((col) => (
          <div
            key={col.key}
            onClick={col.sortable !== false ? () => handleSort(col.key) : undefined}
            style={{
              flex: col.width ? `0 0 ${col.width}` : 1,
              padding: cellPad,
              fontSize: '9px',
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: TYPOGRAPHY.BOLD,
              color: accentColor,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase' as const,
              textAlign: col.align || 'left',
              cursor: col.sortable !== false ? 'pointer' : 'default',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              userSelect: 'none' as const,
              whiteSpace: 'nowrap' as const,
            }}
          >
            {col.label}
            {sortKey === col.key && (
              sortDir === 'asc'
                ? <ChevronUp size={10} />
                : <ChevronDown size={10} />
            )}
          </div>
        ))}
      </div>

      {/* Rows */}
      <div style={{ maxHeight, overflowY: 'auto' }}>
        {sortedData.length === 0 ? (
          <div style={{
            padding: '20px',
            textAlign: 'center',
            fontSize: '10px',
            fontFamily: TYPOGRAPHY.MONO,
            color: FINCEPT.MUTED,
          }}>
            {emptyMessage}
          </div>
        ) : (
          sortedData.map((row, idx) => (
            <div
              key={idx}
              onClick={onRowClick ? () => onRowClick(row, idx) : undefined}
              style={{
                display: 'flex',
                minHeight: rowHeight,
                backgroundColor: selectedIndex === idx
                  ? `${accentColor}15`
                  : idx % 2 === 0
                    ? 'transparent'
                    : `${FINCEPT.CHARCOAL}40`,
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                cursor: onRowClick ? 'pointer' : 'default',
                transition: 'background-color 0.1s',
              }}
              onMouseEnter={(e) => {
                if (selectedIndex !== idx) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }
              }}
              onMouseLeave={(e) => {
                if (selectedIndex !== idx) {
                  e.currentTarget.style.backgroundColor =
                    idx % 2 === 0 ? 'transparent' : `${FINCEPT.CHARCOAL}40`;
                }
              }}
            >
              {columns.map((col) => (
                <div
                  key={col.key}
                  style={{
                    flex: col.width ? `0 0 ${col.width}` : 1,
                    padding: cellPad,
                    fontSize: compact ? '9px' : '10px',
                    fontFamily: TYPOGRAPHY.MONO,
                    color: FINCEPT.WHITE,
                    textAlign: col.align || 'left',
                    display: 'flex',
                    alignItems: 'center',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap' as const,
                  }}
                >
                  {col.format ? col.format(row[col.key], row) : row[col.key]}
                </div>
              ))}
            </div>
          ))
        )}
      </div>
    </div>
  );
};
