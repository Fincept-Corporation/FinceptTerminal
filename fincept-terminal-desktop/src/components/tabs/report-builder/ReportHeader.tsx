import React from 'react';
import {
  FileText,
  Download,
  Save,
  FolderOpen,
  Undo2,
  Redo2,
  Search,
  Keyboard,
  LayoutTemplate,
} from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { ReportHeaderProps } from './types';

export const ReportHeader: React.FC<ReportHeaderProps> = ({
  canUndo,
  canRedo,
  onUndo,
  onRedo,
  onFindReplace,
  onShortcuts,
  onTemplateGallery,
  onLoad,
  onSave,
  onExport,
  isSaving,
  isAutoSaving,
  lastSaved,
  hasComponents,
}) => {
  const { t } = useTranslation('reportBuilder');
  const { colors, fontSize } = useTerminalTheme();

  return (
    <div
      className="flex items-center justify-between px-4 py-2 border-b"
      style={{ borderColor: colors.panel, backgroundColor: colors.panel }}
    >
      <div className="flex items-center gap-4">
        <div className="flex items-center gap-2">
          <FileText className="w-5 h-5" style={{ color: colors.primary }} />
          <span className="font-bold text-sm" style={{ color: colors.primary }}>
            {t('title')}
          </span>
        </div>

        {/* Productivity Toolbar */}
        <div className="flex items-center gap-1 border-l border-[#333333] pl-3">
          <button
            onClick={onUndo}
            disabled={!canUndo}
            className="p-1.5 rounded hover:bg-[#2a2a2a] disabled:opacity-30"
            title="Undo (Ctrl+Z)"
          >
            <Undo2 size={14} />
          </button>
          <button
            onClick={onRedo}
            disabled={!canRedo}
            className="p-1.5 rounded hover:bg-[#2a2a2a] disabled:opacity-30"
            title="Redo (Ctrl+Y)"
          >
            <Redo2 size={14} />
          </button>
          <button
            onClick={onFindReplace}
            className="p-1.5 rounded hover:bg-[#2a2a2a]"
            title="Find & Replace (Ctrl+F)"
          >
            <Search size={14} />
          </button>
          <button
            onClick={onShortcuts}
            className="p-1.5 rounded hover:bg-[#2a2a2a]"
            title="Keyboard Shortcuts"
          >
            <Keyboard size={14} />
          </button>
        </div>

        {/* Auto-save status */}
        {lastSaved && (
          <span className="text-[10px]" style={{ color: colors.textMuted }}>
            {isAutoSaving ? 'Saving...' : `Saved ${lastSaved.toLocaleTimeString()}`}
          </span>
        )}
      </div>

      <div className="flex items-center gap-2">
        <button
          onClick={onTemplateGallery}
          className="px-3 py-1 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-1"
          style={{ color: colors.text }}
        >
          <LayoutTemplate size={14} />
          Templates
        </button>
        <button
          onClick={onLoad}
          className="px-3 py-1 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-1"
          style={{ color: colors.text }}
        >
          <FolderOpen size={14} />
          {t('header.load')}
        </button>
        <button
          onClick={onSave}
          disabled={isSaving}
          className="px-3 py-1 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-1 disabled:opacity-50"
          style={{ color: colors.text }}
        >
          <Save size={14} />
          {isSaving ? t('header.saving') : t('header.save')}
        </button>
        <button
          onClick={onExport}
          disabled={!hasComponents}
          className="px-3 py-1 text-xs rounded transition-colors flex items-center gap-1 disabled:opacity-50"
          style={{
            backgroundColor: colors.primary,
            color: '#000',
            fontWeight: 'bold'
          }}
        >
          <Download size={14} />
          Export
        </button>
      </div>
    </div>
  );
};

export default ReportHeader;
