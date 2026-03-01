import React from 'react';
import {
  Settings as SettingsIcon,
  X,
  Copy,
  Trash2,
  BookmarkPlus,
} from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { PropertiesPanelProps } from './types';
import {
  ContentSection,
  QuoteSection,
  DisclaimerSection,
  AlignmentSection,
  FontSizeSection,
  ImageSection,
  TableSection,
  ChartSection,
  SectionSettingsSection,
  ColumnsSection,
  KpiSection,
  SparklineSection,
  LiveTableSection,
  DynamicChartSection,
  ListSection,
  TocSection,
  DividerSection,
  PageBreakSection,
  CoverPageSection,
  SubheadingSection,
  SignatureSection,
  QrCodeSection,
  WatermarkSection,
} from './PropertiesPanelSections';

export const PropertiesPanel: React.FC<PropertiesPanelProps> = ({
  selectedComponent,
  onUpdateComponent,
  onDeleteComponent,
  onDuplicateComponent,
  onClearSelection,
  onSaveAsTemplate,
  onImageUpload,
}) => {
  const { colors, fontSize } = useTerminalTheme();

  if (!selectedComponent) {
    return (
      <div className="p-4 text-center" style={{ color: colors.textMuted }}>
        <SettingsIcon size={32} className="mx-auto mb-2 opacity-30" />
        <p className="text-xs">Select a component to edit properties</p>
      </div>
    );
  }

  const sectionProps = { selectedComponent, onUpdateComponent, colors, onImageUpload };

  return (
    <div className="p-4">
      <div className="flex items-center justify-between mb-4">
        <h3 className="text-sm font-bold" style={{ color: colors.primary }}>PROPERTIES</h3>
        <button
          onClick={onClearSelection}
          className="p-1 hover:bg-[#2a2a2a] rounded"
          style={{ color: colors.textMuted }}
        >
          <X size={16} />
        </button>
      </div>

      <div className="space-y-4">
        {/* Component Type */}
        <div>
          <label className="text-xs font-semibold mb-1 block" style={{ color: colors.textMuted }}>
            Type
          </label>
          <div className="px-3 py-2 text-xs rounded bg-[#0a0a0a] capitalize">
            {selectedComponent.type}
          </div>
        </div>

        {/* Content Editor */}
        {(selectedComponent.type === 'heading' || selectedComponent.type === 'text' || selectedComponent.type === 'code') && (
          <ContentSection {...sectionProps} />
        )}

        {/* Quote Content */}
        {selectedComponent.type === 'quote' && <QuoteSection {...sectionProps} />}

        {/* Disclaimer Content */}
        {selectedComponent.type === 'disclaimer' && <DisclaimerSection {...sectionProps} />}

        {/* Alignment */}
        {(selectedComponent.type === 'heading' || selectedComponent.type === 'text') && (
          <AlignmentSection {...sectionProps} />
        )}

        {/* Font Size */}
        {(selectedComponent.type === 'heading' || selectedComponent.type === 'text') && (
          <FontSizeSection {...sectionProps} />
        )}

        {/* Image Upload */}
        {selectedComponent.type === 'image' && <ImageSection {...sectionProps} />}

        {/* Table Columns */}
        {selectedComponent.type === 'table' && <TableSection {...sectionProps} />}

        {/* Chart Type */}
        {selectedComponent.type === 'chart' && <ChartSection {...sectionProps} />}

        {/* Section Settings */}
        {selectedComponent.type === 'section' && <SectionSettingsSection {...sectionProps} />}

        {/* Columns Count */}
        {selectedComponent.type === 'columns' && <ColumnsSection {...sectionProps} />}

        {/* KPI Settings */}
        {selectedComponent.type === 'kpi' && <KpiSection {...sectionProps} />}

        {/* Sparkline Settings */}
        {selectedComponent.type === 'sparkline' && <SparklineSection {...sectionProps} />}

        {/* Live Table Settings */}
        {selectedComponent.type === 'liveTable' && <LiveTableSection {...sectionProps} />}

        {/* Dynamic Chart Settings */}
        {selectedComponent.type === 'dynamicChart' && <DynamicChartSection {...sectionProps} />}

        {/* List Settings */}
        {selectedComponent.type === 'list' && <ListSection {...sectionProps} />}

        {/* Table of Contents Settings */}
        {selectedComponent.type === 'toc' && <TocSection {...sectionProps} />}

        {/* Divider Settings */}
        {selectedComponent.type === 'divider' && <DividerSection {...sectionProps} />}

        {/* Page Break Settings */}
        {selectedComponent.type === 'pagebreak' && <PageBreakSection colors={colors} />}

        {/* Cover Page Settings */}
        {selectedComponent.type === 'coverpage' && <CoverPageSection {...sectionProps} />}

        {/* Subheading Settings */}
        {selectedComponent.type === 'subheading' && <SubheadingSection {...sectionProps} />}

        {/* Signature Settings */}
        {selectedComponent.type === 'signature' && <SignatureSection {...sectionProps} />}

        {/* QR Code Settings */}
        {selectedComponent.type === 'qrcode' && <QrCodeSection {...sectionProps} />}

        {/* Watermark Settings */}
        {selectedComponent.type === 'watermark' && <WatermarkSection {...sectionProps} />}

        {/* Actions */}
        <div className="pt-4 border-t" style={{ borderColor: colors.panel }}>
          <div className="flex gap-2 mb-2">
            <button
              onClick={() => onDuplicateComponent(selectedComponent.id)}
              className="flex-1 px-3 py-2 text-xs rounded bg-[#0a0a0a] hover:bg-[#2a2a2a] transition-colors flex items-center justify-center gap-2"
            >
              <Copy size={14} />
              Duplicate
            </button>
            <button
              onClick={() => onDeleteComponent(selectedComponent.id)}
              className="flex-1 px-3 py-2 text-xs rounded bg-red-600 hover:bg-red-700 transition-colors flex items-center justify-center gap-2"
            >
              <Trash2 size={14} />
              Delete
            </button>
          </div>
          <button
            onClick={onSaveAsTemplate}
            className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors flex items-center justify-center gap-2"
            style={{ color: colors.text }}
          >
            <BookmarkPlus size={14} />
            Save as Template
          </button>
        </div>
      </div>
    </div>
  );
};

export default PropertiesPanel;
