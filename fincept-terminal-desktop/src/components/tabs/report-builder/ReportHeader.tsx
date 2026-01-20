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
import { ReportHeaderProps } from './types';
import { FINCEPT_COLORS } from './constants';

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

  return (
    <div
      className="flex items-center justify-between px-4 py-2 border-b"
      style={{ borderColor: FINCEPT_COLORS.BORDER, backgroundColor: FINCEPT_COLORS.PANEL_BG }}
    >
      <div className="flex items-center gap-4">
        <div className="flex items-center gap-2">
          <FileText className="w-5 h-5" style={{ color: FINCEPT_COLORS.ORANGE }} />
          <span className="font-bold text-sm" style={{ color: FINCEPT_COLORS.ORANGE }}>
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
          <span className="text-[10px]" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
            {isAutoSaving ? 'Saving...' : `Saved ${lastSaved.toLocaleTimeString()}`}
          </span>
        )}
      </div>

      <div className="flex items-center gap-2">
        <button
          onClick={onTemplateGallery}
          className="px-3 py-1 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-1"
          style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
        >
          <LayoutTemplate size={14} />
          Templates
        </button>
        <button
          onClick={onLoad}
          className="px-3 py-1 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-1"
          style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
        >
          <FolderOpen size={14} />
          {t('header.load')}
        </button>
        <button
          onClick={onSave}
          disabled={isSaving}
          className="px-3 py-1 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-1 disabled:opacity-50"
          style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
        >
          <Save size={14} />
          {isSaving ? t('header.saving') : t('header.save')}
        </button>
        <button
          onClick={onExport}
          disabled={!hasComponents}
          className="px-3 py-1 text-xs rounded transition-colors flex items-center gap-1 disabled:opacity-50"
          style={{
            backgroundColor: FINCEPT_COLORS.ORANGE,
            color: FINCEPT_COLORS.BLACK,
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
