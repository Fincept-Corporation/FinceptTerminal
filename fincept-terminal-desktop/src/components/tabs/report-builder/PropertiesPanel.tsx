import React from 'react';
import {
  Settings as SettingsIcon,
  X,
  Plus,
  Copy,
  Trash2,
  BookmarkPlus,
} from 'lucide-react';
import { PropertiesPanelProps } from './types';
import { FINCEPT_COLORS, FONT_SIZES, CHART_TYPES } from './constants';

export const PropertiesPanel: React.FC<PropertiesPanelProps> = ({
  selectedComponent,
  onUpdateComponent,
  onDeleteComponent,
  onDuplicateComponent,
  onClearSelection,
  onSaveAsTemplate,
  onImageUpload,
}) => {
  if (!selectedComponent) {
    return (
      <div className="p-4 text-center" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
        <SettingsIcon size={32} className="mx-auto mb-2 opacity-30" />
        <p className="text-xs">Select a component to edit properties</p>
      </div>
    );
  }

  return (
    <div className="p-4">
      <div className="flex items-center justify-between mb-4">
        <h3 className="text-sm font-bold" style={{ color: FINCEPT_COLORS.ORANGE }}>PROPERTIES</h3>
        <button
          onClick={onClearSelection}
          className="p-1 hover:bg-[#2a2a2a] rounded"
          style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}
        >
          <X size={16} />
        </button>
      </div>

      <div className="space-y-4">
        {/* Component Type */}
        <div>
          <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
            Type
          </label>
          <div className="px-3 py-2 text-xs rounded bg-[#0a0a0a] capitalize">
            {selectedComponent.type}
          </div>
        </div>

        {/* Content Editor */}
        {(selectedComponent.type === 'heading' || selectedComponent.type === 'text' || selectedComponent.type === 'code') && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Content
            </label>
            <textarea
              value={selectedComponent.content || ''}
              onChange={(e) => onUpdateComponent(selectedComponent.id, { content: e.target.value })}
              className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              rows={selectedComponent.type === 'code' ? 10 : 4}
              placeholder="Enter content..."
            />
          </div>
        )}

        {/* Quote Content */}
        {selectedComponent.type === 'quote' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Quote Text
              </label>
              <textarea
                value={selectedComponent.content || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, { content: e.target.value })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                rows={3}
                placeholder="Enter quote text..."
              />
            </div>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Author
              </label>
              <input
                type="text"
                value={selectedComponent.config.author || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, author: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter author name..."
              />
            </div>
          </>
        )}

        {/* Disclaimer Content */}
        {selectedComponent.type === 'disclaimer' && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Disclaimer Text
            </label>
            <textarea
              value={selectedComponent.content || ''}
              onChange={(e) => onUpdateComponent(selectedComponent.id, { content: e.target.value })}
              className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              rows={4}
              placeholder="Enter disclaimer text..."
            />
          </div>
        )}

        {/* Alignment */}
        {(selectedComponent.type === 'heading' || selectedComponent.type === 'text') && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Alignment
            </label>
            <div className="flex gap-2">
              {['left', 'center', 'right'].map((align) => (
                <button
                  key={align}
                  onClick={() => onUpdateComponent(selectedComponent.id, {
                    config: { ...selectedComponent.config, alignment: align as any }
                  })}
                  className={`flex-1 px-3 py-2 text-xs rounded transition-colors capitalize ${
                    selectedComponent.config.alignment === align
                      ? 'bg-[#FFA500] text-black'
                      : 'bg-[#0a0a0a] hover:bg-[#2a2a2a]'
                  }`}
                >
                  {align}
                </button>
              ))}
            </div>
          </div>
        )}

        {/* Font Size */}
        {(selectedComponent.type === 'heading' || selectedComponent.type === 'text') && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Font Size
            </label>
            <select
              value={selectedComponent.config.fontSize || 'base'}
              onChange={(e) => onUpdateComponent(selectedComponent.id, {
                config: { ...selectedComponent.config, fontSize: e.target.value }
              })}
              className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              {FONT_SIZES.map(size => (
                <option key={size.value} value={size.value}>{size.label}</option>
              ))}
            </select>
          </div>
        )}

        {/* Image Upload */}
        {selectedComponent.type === 'image' && (
          <div>
            <label className="text-xs font-semibold mb-2 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Image
            </label>
            {selectedComponent.config.imageUrl ? (
              <div className="space-y-2">
                <div className="text-xs text-gray-400 break-all bg-[#0a0a0a] p-2 rounded">
                  {selectedComponent.config.imageUrl.split('\\').pop()}
                </div>
                <div className="flex gap-2">
                  <button
                    onClick={onImageUpload}
                    className="flex-1 px-3 py-2 text-xs rounded bg-blue-600 hover:bg-blue-700 transition-colors"
                  >
                    Change Image
                  </button>
                  <button
                    onClick={() => onUpdateComponent(selectedComponent.id, {
                      config: { ...selectedComponent.config, imageUrl: undefined }
                    })}
                    className="flex-1 px-3 py-2 text-xs rounded bg-red-600 hover:bg-red-700 transition-colors"
                  >
                    Remove
                  </button>
                </div>
              </div>
            ) : (
              <button
                onClick={onImageUpload}
                className="w-full px-3 py-2 text-xs rounded transition-colors flex items-center justify-center gap-2"
                style={{ backgroundColor: FINCEPT_COLORS.ORANGE, color: FINCEPT_COLORS.BLACK }}
              >
                <Plus size={14} />
                Upload Image
              </button>
            )}
          </div>
        )}

        {/* Table Columns */}
        {selectedComponent.type === 'table' && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Columns (comma-separated)
            </label>
            <input
              type="text"
              value={(selectedComponent.config.columns || []).join(', ')}
              onChange={(e) => onUpdateComponent(selectedComponent.id, {
                config: { ...selectedComponent.config, columns: e.target.value.split(',').map(s => s.trim()) }
              })}
              className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              placeholder="Column 1, Column 2, Column 3"
            />
          </div>
        )}

        {/* Chart Type */}
        {selectedComponent.type === 'chart' && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Chart Type
            </label>
            <select
              value={selectedComponent.config.chartType || 'bar'}
              onChange={(e) => onUpdateComponent(selectedComponent.id, {
                config: { ...selectedComponent.config, chartType: e.target.value }
              })}
              className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              {CHART_TYPES.map(chart => (
                <option key={chart.value} value={chart.value}>{chart.label}</option>
              ))}
            </select>
          </div>
        )}

        {/* Section Settings */}
        {selectedComponent.type === 'section' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Section Title
              </label>
              <input
                type="text"
                value={selectedComponent.config.sectionTitle || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, sectionTitle: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter section title..."
              />
            </div>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Border Color
              </label>
              <input
                type="color"
                value={selectedComponent.config.borderColor || '#FFA500'}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, borderColor: e.target.value }
                })}
                className="w-full h-8 rounded cursor-pointer"
              />
            </div>
          </>
        )}

        {/* Columns Count */}
        {selectedComponent.type === 'columns' && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Number of Columns
            </label>
            <select
              value={selectedComponent.config.columnCount || 2}
              onChange={(e) => onUpdateComponent(selectedComponent.id, {
                config: { ...selectedComponent.config, columnCount: Number(e.target.value) as 2 | 3 }
              })}
              className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <option value={2}>2 Columns</option>
              <option value={3}>3 Columns</option>
            </select>
          </div>
        )}

        {/* Signature Settings */}
        {selectedComponent.type === 'signature' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Name
              </label>
              <input
                type="text"
                value={selectedComponent.config.name || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, name: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter name..."
              />
            </div>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Title
              </label>
              <input
                type="text"
                value={selectedComponent.config.title || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, title: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter title..."
              />
            </div>
          </>
        )}

        {/* QR Code Settings */}
        {selectedComponent.type === 'qrcode' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                URL/Value
              </label>
              <input
                type="text"
                value={selectedComponent.config.value || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, value: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter URL..."
              />
            </div>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Label
              </label>
              <input
                type="text"
                value={selectedComponent.config.label || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, label: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter label..."
              />
            </div>
          </>
        )}

        {/* Watermark Settings */}
        {selectedComponent.type === 'watermark' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Text
              </label>
              <input
                type="text"
                value={selectedComponent.config.text || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, text: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter watermark text..."
              />
            </div>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Opacity
              </label>
              <input
                type="range"
                min="0.05"
                max="0.3"
                step="0.05"
                value={selectedComponent.config.opacity || 0.1}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, opacity: Number(e.target.value) }
                })}
                className="w-full"
                style={{ accentColor: FINCEPT_COLORS.ORANGE }}
              />
            </div>
          </>
        )}

        {/* Actions */}
        <div className="pt-4 border-t" style={{ borderColor: FINCEPT_COLORS.BORDER }}>
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
            style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
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
