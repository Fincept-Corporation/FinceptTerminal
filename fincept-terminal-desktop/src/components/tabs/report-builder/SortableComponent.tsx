import React from 'react';
import { useSortable } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import {
  Type,
  AlignLeft,
  BarChart3,
  Table,
  Image as ImageIcon,
  Code,
  Minus,
  Quote,
  List,
  Hash,
  TrendingUp,
  Activity,
  Database,
  PenTool,
  AlertTriangle,
  QrCode,
  Stamp,
  FileText,
  Move,
  Copy,
  Trash2,
} from 'lucide-react';
import { ReportComponent } from '@/services/core/reportService';
import { SortableComponentProps } from './types';

// Get icon for component type
const getComponentIcon = (type: ReportComponent['type']) => {
  switch (type) {
    case 'heading': return <Type size={14} />;
    case 'text': return <AlignLeft size={14} />;
    case 'chart': return <BarChart3 size={14} />;
    case 'table': return <Table size={14} />;
    case 'image': return <ImageIcon size={14} />;
    case 'code': return <Code size={14} />;
    case 'divider': return <Minus size={14} />;
    case 'quote': return <Quote size={14} />;
    case 'list': return <List size={14} />;
    case 'toc': return <Hash size={14} />;
    case 'kpi': return <TrendingUp size={14} />;
    case 'sparkline': return <Activity size={14} />;
    case 'liveTable': return <Database size={14} />;
    case 'signature': return <PenTool size={14} />;
    case 'disclaimer': return <AlertTriangle size={14} />;
    case 'qrcode': return <QrCode size={14} />;
    case 'watermark': return <Stamp size={14} />;
    default: return <FileText size={14} />;
  }
};

// Get preview text for component
const getPreviewText = (component: ReportComponent): string => {
  if (component.content) {
    return String(component.content).substring(0, 40);
  }
  return `${component.type.charAt(0).toUpperCase() + component.type.slice(1)} component`;
};

export const SortableComponent: React.FC<SortableComponentProps> = ({
  component,
  isSelected,
  onClick,
  onDelete,
  onDuplicate,
}) => {
  const {
    attributes,
    listeners,
    setNodeRef,
    transform,
    transition,
  } = useSortable({ id: component.id });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      className={`flex items-center justify-between p-2 mb-1 cursor-pointer transition-colors border-l-2 ${
        isSelected
          ? 'bg-[#2a2a2a] border-l-[#FFA500]'
          : 'bg-transparent border-l-transparent hover:bg-[#1a1a1a]'
      }`}
      onClick={onClick}
    >
      <div className="flex items-center gap-2 flex-1 min-w-0">
        <div {...attributes} {...listeners} className="cursor-move text-gray-500 hover:text-[#FFA500]">
          <Move size={14} />
        </div>
        <div className="text-[#FFA500]">
          {getComponentIcon(component.type)}
        </div>
        <span className="text-xs text-gray-400 truncate">{getPreviewText(component)}</span>
      </div>
      <div className="flex gap-1">
        <button
          onClick={(e) => { e.stopPropagation(); onDuplicate(); }}
          className="p-1 hover:bg-[#333333] rounded text-gray-400 hover:text-[#FFA500]"
          title="Duplicate"
        >
          <Copy size={12} />
        </button>
        <button
          onClick={(e) => { e.stopPropagation(); onDelete(); }}
          className="p-1 hover:bg-[#333333] rounded text-gray-400 hover:text-red-500"
          title="Delete"
        >
          <Trash2 size={12} />
        </button>
      </div>
    </div>
  );
};

export default SortableComponent;
