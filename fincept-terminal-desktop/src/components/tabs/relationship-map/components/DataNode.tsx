import React, { memo, useMemo, useCallback, useState } from 'react';
import { Handle, Position } from 'reactflow';
import {
  Activity,
  Calendar,
  DollarSign,
  BarChart3,
  Award,
  PieChart,
  TrendingUp,
  Briefcase,
  Users,
  FileText,
  ExternalLink,
} from 'lucide-react';
import { open } from '@tauri-apps/plugin-shell';
import { NODE_TYPE_COLORS } from '../constants';
import type { DataNodeProps, DataNodeType, DataNodeItem } from '../types';

// Memoized icon getter - returns the icon component
const getIconComponent = (type: DataNodeType) => {
  switch (type) {
    case 'events':
      return Activity;
    case 'filings':
      return Calendar;
    case 'financials':
      return DollarSign;
    case 'balance':
      return BarChart3;
    case 'analysts':
      return Award;
    case 'ownership':
      return PieChart;
    case 'valuation':
      return TrendingUp;
    case 'info':
      return Briefcase;
    case 'executives':
      return Users;
    default:
      return FileText;
  }
};

// Get node color based on type
const getNodeColor = (type: DataNodeType, accentColor: string): string => {
  if (type === 'balance') return accentColor;
  return NODE_TYPE_COLORS[type] || '#6b7280';
};

// Memoized data item row
const DataItemRow = memo(
  ({
    item,
    isClickable,
    textMutedColor,
    textColor,
    onClick,
  }: {
    item: DataNodeItem;
    isClickable: boolean;
    textMutedColor: string;
    textColor: string;
    onClick: (e: React.MouseEvent) => void;
  }) => (
    <div
      className="flex justify-between gap-3 hover:bg-white/5 rounded px-1 py-0.5 transition-colors"
      onClick={onClick}
      style={{ cursor: isClickable && item.url ? 'pointer' : 'default' }}
    >
      <span className="truncate" style={{ color: textMutedColor }}>
        {item.label}
      </span>
      <span className="font-medium text-right flex items-center gap-1" style={{ color: textColor }}>
        {item.value}
        {isClickable && item.url && <ExternalLink className="w-3 h-3 opacity-50" />}
      </span>
    </div>
  )
);

DataItemRow.displayName = 'DataItemRow';

const DataNode = memo(({ data }: DataNodeProps) => {
  const { colors, type, title, items } = data;
  const [expanded, setExpanded] = useState(false);

  const nodeColor = useMemo(() => getNodeColor(type, colors.accent), [type, colors.accent]);
  const IconComponent = useMemo(() => getIconComponent(type), [type]);
  const hasClickableItems = type === 'events' || type === 'filings';
  const displayItems = expanded ? items : items?.slice(0, 4);
  const hasMoreItems = items?.length > 4;

  // Memoize styles
  const containerStyle = useMemo(
    () => ({
      background: '#1a1a1a',
      border: `2px solid ${nodeColor}`,
      borderRadius: '8px',
      padding: '14px 18px',
      minWidth: '260px',
      maxWidth: '320px',
      boxShadow: `0 0 20px ${nodeColor}50, 0 4px 12px rgba(0,0,0,0.5)`,
      cursor: hasMoreItems ? 'pointer' : 'default',
      transition: 'all 0.2s',
    }),
    [nodeColor, hasMoreItems]
  );

  const handleStyle = useMemo(() => ({ background: nodeColor }), [nodeColor]);
  const handleStyleHidden = useMemo(() => ({ background: nodeColor, opacity: 0 }), [nodeColor]);

  const badgeStyle = useMemo(
    () => ({
      backgroundColor: `${nodeColor}20`,
      color: nodeColor,
    }),
    [nodeColor]
  );

  const handleContainerClick = useCallback(() => {
    if (hasMoreItems) {
      setExpanded((prev) => !prev);
    }
  }, [hasMoreItems]);

  const handleItemClick = useCallback(
    async (e: React.MouseEvent, item: DataNodeItem) => {
      e.stopPropagation();
      if (hasClickableItems && item.url) {
        await open(item.url);
      }
    },
    [hasClickableItems]
  );

  const handleMouseEnter = useCallback(
    (e: React.MouseEvent<HTMLDivElement>) => {
      e.currentTarget.style.transform = 'scale(1.02)';
      e.currentTarget.style.boxShadow = `0 0 30px ${nodeColor}70, 0 6px 16px rgba(0,0,0,0.6)`;
    },
    [nodeColor]
  );

  const handleMouseLeave = useCallback(
    (e: React.MouseEvent<HTMLDivElement>) => {
      e.currentTarget.style.transform = 'scale(1)';
      e.currentTarget.style.boxShadow = `0 0 20px ${nodeColor}50, 0 4px 12px rgba(0,0,0,0.5)`;
    },
    [nodeColor]
  );

  return (
    <div
      style={containerStyle}
      onClick={handleContainerClick}
      onMouseEnter={handleMouseEnter}
      onMouseLeave={handleMouseLeave}
    >
      <Handle type="target" position={Position.Top} style={handleStyle} />

      <div className="flex items-center gap-2 mb-3">
        <div style={{ color: nodeColor }}>
          <IconComponent className="w-5 h-5" />
        </div>
        <h4 className="font-semibold text-sm" style={{ color: colors.text }}>
          {title}
        </h4>
        {hasMoreItems && (
          <span className="text-xs ml-auto px-1.5 py-0.5 rounded" style={badgeStyle}>
            {items.length}
          </span>
        )}
      </div>

      <div className="space-y-1.5 text-xs">
        {displayItems?.map((item, idx) => (
          <DataItemRow
            key={idx}
            item={item}
            isClickable={hasClickableItems}
            textMutedColor={colors.textMuted}
            textColor={colors.text}
            onClick={(e) => handleItemClick(e, item)}
          />
        ))}
        {!expanded && hasMoreItems && (
          <div className="text-center pt-1 text-xs font-semibold" style={{ color: nodeColor }}>
            Click to see +{items.length - 4} more
          </div>
        )}
      </div>

      <Handle type="source" position={Position.Bottom} style={handleStyleHidden} />
    </div>
  );
});

DataNode.displayName = 'DataNode';

export default DataNode;
