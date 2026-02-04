import React from 'react';
import {
  Settings as SettingsIcon,
  X,
  Plus,
  Copy,
  Trash2,
  BookmarkPlus,
  FileX,
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
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Style
              </label>
              <select
                value={selectedComponent.config.quoteType || 'quote'}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, quoteType: e.target.value as 'quote' | 'info' | 'warning' | 'success' | 'error' }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              >
                <option value="quote">Quote</option>
                <option value="info">Info</option>
                <option value="warning">Warning</option>
                <option value="success">Success</option>
                <option value="error">Error</option>
              </select>
            </div>
          </>
        )}

        {/* Disclaimer Content */}
        {selectedComponent.type === 'disclaimer' && (
          <>
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
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Disclaimer Type
              </label>
              <div className="flex gap-2">
                {(['standard', 'legal', 'confidential'] as const).map((dtype) => (
                  <button
                    key={dtype}
                    onClick={() => onUpdateComponent(selectedComponent.id, {
                      config: { ...selectedComponent.config, disclaimerType: dtype }
                    })}
                    className={`flex-1 px-2 py-2 text-xs rounded transition-colors capitalize ${
                      (selectedComponent.config.disclaimerType || 'standard') === dtype
                        ? 'bg-[#FFA500] text-black'
                        : 'bg-[#0a0a0a] hover:bg-[#2a2a2a]'
                    }`}
                  >
                    {dtype}
                  </button>
                ))}
              </div>
            </div>
          </>
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
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Background Color
              </label>
              <input
                type="color"
                value={selectedComponent.config.backgroundColor || '#f8f9fa'}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, backgroundColor: e.target.value }
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

        {/* KPI Settings */}
        {selectedComponent.type === 'kpi' && (
          <div>
            <label className="text-xs font-semibold mb-2 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              KPI Cards
            </label>
            <div className="space-y-3">
              {(selectedComponent.config.kpis || []).map((kpi: any, idx: number) => (
                <div key={idx} className="p-3 bg-[#0a0a0a] rounded border border-[#333333] space-y-2">
                  <div className="flex items-center justify-between mb-2">
                    <span className="text-xs font-semibold" style={{ color: FINCEPT_COLORS.ORANGE }}>KPI {idx + 1}</span>
                    <button
                      onClick={() => {
                        const newKpis = [...(selectedComponent.config.kpis || [])];
                        newKpis.splice(idx, 1);
                        onUpdateComponent(selectedComponent.id, {
                          config: { ...selectedComponent.config, kpis: newKpis }
                        });
                      }}
                      className="p-1 hover:bg-red-600 rounded text-red-400 hover:text-white transition-colors"
                    >
                      <X size={12} />
                    </button>
                  </div>
                  <input
                    type="text"
                    value={kpi.label || ''}
                    onChange={(e) => {
                      const newKpis = [...(selectedComponent.config.kpis || [])];
                      newKpis[idx] = { ...newKpis[idx], label: e.target.value };
                      onUpdateComponent(selectedComponent.id, {
                        config: { ...selectedComponent.config, kpis: newKpis }
                      });
                    }}
                    className="w-full px-2 py-1 text-xs rounded bg-[#1a1a1a] border border-[#333333] focus:border-[#FFA500] outline-none"
                    style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                    placeholder="Label (e.g., Revenue)"
                  />
                  <input
                    type="text"
                    value={kpi.value || ''}
                    onChange={(e) => {
                      const newKpis = [...(selectedComponent.config.kpis || [])];
                      newKpis[idx] = { ...newKpis[idx], value: e.target.value };
                      onUpdateComponent(selectedComponent.id, {
                        config: { ...selectedComponent.config, kpis: newKpis }
                      });
                    }}
                    className="w-full px-2 py-1 text-xs rounded bg-[#1a1a1a] border border-[#333333] focus:border-[#FFA500] outline-none"
                    style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                    placeholder="Value (e.g., $1.2M)"
                  />
                  <div className="flex gap-2">
                    <input
                      type="text"
                      inputMode="decimal"
                      value={String(kpi.change || 0)}
                      onChange={(e) => {
                        const v = e.target.value;
                        if (v === '' || /^\d*\.?\d*$/.test(v)) {
                          const newKpis = [...(selectedComponent.config.kpis || [])];
                          newKpis[idx] = { ...newKpis[idx], change: v === '' ? 0 : Number(v) };
                          onUpdateComponent(selectedComponent.id, {
                            config: { ...selectedComponent.config, kpis: newKpis }
                          });
                        }
                      }}
                      className="flex-1 px-2 py-1 text-xs rounded bg-[#1a1a1a] border border-[#333333] focus:border-[#FFA500] outline-none"
                      style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                      placeholder="Change %"
                    />
                    <select
                      value={kpi.trend || 'up'}
                      onChange={(e) => {
                        const newKpis = [...(selectedComponent.config.kpis || [])];
                        newKpis[idx] = { ...newKpis[idx], trend: e.target.value as 'up' | 'down' };
                        onUpdateComponent(selectedComponent.id, {
                          config: { ...selectedComponent.config, kpis: newKpis }
                        });
                      }}
                      className="flex-1 px-2 py-1 text-xs rounded bg-[#1a1a1a] border border-[#333333] focus:border-[#FFA500] outline-none"
                      style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                    >
                      <option value="up">Up</option>
                      <option value="down">Down</option>
                    </select>
                  </div>
                </div>
              ))}
              <button
                onClick={() => {
                  const newKpis = [...(selectedComponent.config.kpis || []), { label: 'New KPI', value: '$0', change: 0, trend: 'up' as 'up' | 'down' }];
                  onUpdateComponent(selectedComponent.id, {
                    config: { ...selectedComponent.config, kpis: newKpis }
                  });
                }}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors flex items-center justify-center gap-2"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              >
                <Plus size={14} />
                Add KPI Card
              </button>
            </div>
          </div>
        )}

        {/* Sparkline Settings */}
        {selectedComponent.type === 'sparkline' && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Data (comma-separated numbers)
            </label>
            <input
              type="text"
              value={(selectedComponent.config.data as number[] || []).join(', ')}
              onChange={(e) => {
                const data = e.target.value.split(',').map(v => parseFloat(v.trim())).filter(v => !isNaN(v));
                onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, data }
                });
              }}
              className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              placeholder="10, 25, 15, 30, 20, 35, 28"
            />
            <div className="mt-2">
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Color
              </label>
              <input
                type="color"
                value={selectedComponent.config.color || '#FFA500'}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, color: e.target.value }
                })}
                className="w-full h-8 rounded cursor-pointer"
              />
            </div>
          </div>
        )}

        {/* Live Table Settings */}
        {selectedComponent.type === 'liveTable' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Data Source
              </label>
              <select
                value={selectedComponent.config.dataSource || 'portfolio'}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, dataSource: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              >
                <option value="portfolio">Portfolio</option>
                <option value="watchlist">Watchlist</option>
                <option value="market">Market</option>
              </select>
            </div>
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
                placeholder="Symbol, Price, Change"
              />
            </div>
          </>
        )}

        {/* Dynamic Chart Settings */}
        {selectedComponent.type === 'dynamicChart' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Chart Type
              </label>
              <select
                value={selectedComponent.config.chartType || 'line'}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, chartType: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              >
                <option value="line">Line Chart</option>
                <option value="bar">Bar Chart</option>
                <option value="area">Area Chart</option>
                <option value="pie">Pie Chart</option>
              </select>
            </div>
            <div>
              <label className="text-xs font-semibold mb-2 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Chart Data
              </label>
              <div className="space-y-2">
                {(selectedComponent.config.data as Array<{ name: string; value: number }> || []).map((item: any, idx: number) => (
                  <div key={idx} className="flex gap-2 items-center">
                    <input
                      type="text"
                      value={item.name || ''}
                      onChange={(e) => {
                        const newData = [...(selectedComponent.config.data as any[] || [])];
                        newData[idx] = { ...newData[idx], name: e.target.value };
                        onUpdateComponent(selectedComponent.id, {
                          config: { ...selectedComponent.config, data: newData }
                        });
                      }}
                      className="flex-1 px-2 py-1 text-xs rounded bg-[#1a1a1a] border border-[#333333] focus:border-[#FFA500] outline-none"
                      style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                      placeholder="Label"
                    />
                    <input
                      type="text"
                      inputMode="decimal"
                      value={String(item.value || 0)}
                      onChange={(e) => {
                        const v = e.target.value;
                        if (v === '' || /^\d*\.?\d*$/.test(v)) {
                          const newData = [...(selectedComponent.config.data as any[] || [])];
                          newData[idx] = { ...newData[idx], value: v === '' ? 0 : Number(v) };
                          onUpdateComponent(selectedComponent.id, {
                            config: { ...selectedComponent.config, data: newData }
                          });
                        }
                      }}
                      className="w-20 px-2 py-1 text-xs rounded bg-[#1a1a1a] border border-[#333333] focus:border-[#FFA500] outline-none"
                      style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                      placeholder="Value"
                    />
                    <button
                      onClick={() => {
                        const newData = [...(selectedComponent.config.data as any[] || [])];
                        newData.splice(idx, 1);
                        onUpdateComponent(selectedComponent.id, {
                          config: { ...selectedComponent.config, data: newData }
                        });
                      }}
                      className="p-1 hover:bg-red-600 rounded text-red-400 hover:text-white transition-colors"
                    >
                      <X size={12} />
                    </button>
                  </div>
                ))}
                <button
                  onClick={() => {
                    const newData = [...(selectedComponent.config.data as any[] || []), { name: `Item ${(selectedComponent.config.data as any[] || []).length + 1}`, value: 0 }];
                    onUpdateComponent(selectedComponent.id, {
                      config: { ...selectedComponent.config, data: newData }
                    });
                  }}
                  className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors flex items-center justify-center gap-2"
                  style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                >
                  <Plus size={14} />
                  Add Data Point
                </button>
              </div>
            </div>
          </>
        )}

        {/* List Settings */}
        {selectedComponent.type === 'list' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                List Type
              </label>
              <div className="flex gap-2">
                <button
                  onClick={() => onUpdateComponent(selectedComponent.id, {
                    config: { ...selectedComponent.config, ordered: false }
                  })}
                  className={`flex-1 px-3 py-2 text-xs rounded transition-colors ${
                    !selectedComponent.config.ordered
                      ? 'bg-[#FFA500] text-black'
                      : 'bg-[#0a0a0a] hover:bg-[#2a2a2a]'
                  }`}
                >
                  Bulleted
                </button>
                <button
                  onClick={() => onUpdateComponent(selectedComponent.id, {
                    config: { ...selectedComponent.config, ordered: true }
                  })}
                  className={`flex-1 px-3 py-2 text-xs rounded transition-colors ${
                    selectedComponent.config.ordered
                      ? 'bg-[#FFA500] text-black'
                      : 'bg-[#0a0a0a] hover:bg-[#2a2a2a]'
                  }`}
                >
                  Numbered
                </button>
              </div>
            </div>
            <div>
              <label className="text-xs font-semibold mb-2 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Items
              </label>
              <div className="space-y-2">
                {(selectedComponent.config.items || []).map((item: string, idx: number) => (
                  <div key={idx} className="flex gap-2">
                    <input
                      type="text"
                      value={item}
                      onChange={(e) => {
                        const newItems = [...(selectedComponent.config.items || [])];
                        newItems[idx] = e.target.value;
                        onUpdateComponent(selectedComponent.id, {
                          config: { ...selectedComponent.config, items: newItems }
                        });
                      }}
                      className="flex-1 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                      style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                      placeholder={`Item ${idx + 1}`}
                    />
                    <button
                      onClick={() => {
                        const newItems = [...(selectedComponent.config.items || [])];
                        newItems.splice(idx, 1);
                        onUpdateComponent(selectedComponent.id, {
                          config: { ...selectedComponent.config, items: newItems }
                        });
                      }}
                      className="p-1 hover:bg-red-600 rounded text-red-400 hover:text-white transition-colors"
                    >
                      <X size={12} />
                    </button>
                  </div>
                ))}
                <button
                  onClick={() => {
                    const newItems = [...(selectedComponent.config.items || []), ''];
                    onUpdateComponent(selectedComponent.id, {
                      config: { ...selectedComponent.config, items: newItems }
                    });
                  }}
                  className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors flex items-center justify-center gap-2"
                  style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                >
                  <Plus size={14} />
                  Add Item
                </button>
              </div>
            </div>
          </>
        )}

        {/* Table of Contents Settings */}
        {selectedComponent.type === 'toc' && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Show Page Numbers
            </label>
            <div className="flex gap-2">
              <button
                onClick={() => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, showPageNumbers: true }
                })}
                className={`flex-1 px-3 py-2 text-xs rounded transition-colors ${
                  selectedComponent.config.showPageNumbers !== false
                    ? 'bg-[#FFA500] text-black'
                    : 'bg-[#0a0a0a] hover:bg-[#2a2a2a]'
                }`}
              >
                Show
              </button>
              <button
                onClick={() => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, showPageNumbers: false }
                })}
                className={`flex-1 px-3 py-2 text-xs rounded transition-colors ${
                  selectedComponent.config.showPageNumbers === false
                    ? 'bg-[#FFA500] text-black'
                    : 'bg-[#0a0a0a] hover:bg-[#2a2a2a]'
                }`}
              >
                Hide
              </button>
            </div>
            <p className="text-xs text-gray-500 mt-2">
              Table of Contents auto-generates from headings in your report
            </p>
          </div>
        )}

        {/* Divider Settings */}
        {selectedComponent.type === 'divider' && (
          <div>
            <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
              Color
            </label>
            <input
              type="color"
              value={selectedComponent.config.color || '#333333'}
              onChange={(e) => onUpdateComponent(selectedComponent.id, {
                config: { ...selectedComponent.config, color: e.target.value }
              })}
              className="w-full h-8 rounded cursor-pointer"
            />
            <p className="text-xs text-gray-500 mt-2">
              Horizontal divider line
            </p>
          </div>
        )}

        {/* Page Break Settings */}
        {selectedComponent.type === 'pagebreak' && (
          <div className="text-center py-4">
            <div className="inline-flex items-center justify-center w-12 h-12 rounded-full bg-[#0a0a0a] mb-2">
              <FileX size={24} style={{ color: FINCEPT_COLORS.ORANGE }} />
            </div>
            <p className="text-xs text-gray-400">
              Page Break
            </p>
            <p className="text-xs text-gray-500 mt-1">
              Forces content after this to start on a new page when exported
            </p>
          </div>
        )}

        {/* Cover Page Settings */}
        {selectedComponent.type === 'coverpage' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Title
              </label>
              <input
                type="text"
                value={selectedComponent.content || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, { content: e.target.value })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter cover page title..."
              />
            </div>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Subtitle
              </label>
              <input
                type="text"
                value={selectedComponent.config.subtitle || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, subtitle: e.target.value }
                })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter subtitle..."
              />
            </div>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Background Color
              </label>
              <input
                type="color"
                value={selectedComponent.config.backgroundColor || '#0a0a0a'}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, backgroundColor: e.target.value }
                })}
                className="w-full h-8 rounded cursor-pointer"
              />
            </div>
            <div>
              <label className="text-xs font-semibold mb-2 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Logo
              </label>
              {selectedComponent.config.logo ? (
                <div className="space-y-2">
                  <div className="text-xs text-gray-400 break-all bg-[#0a0a0a] p-2 rounded">
                    {selectedComponent.config.logo.split('\\').pop()}
                  </div>
                  <div className="flex gap-2">
                    <button
                      onClick={onImageUpload}
                      className="flex-1 px-3 py-2 text-xs rounded bg-blue-600 hover:bg-blue-700 transition-colors"
                    >
                      Change Logo
                    </button>
                    <button
                      onClick={() => onUpdateComponent(selectedComponent.id, {
                        config: { ...selectedComponent.config, logo: undefined }
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
                  Upload Logo
                </button>
              )}
            </div>
          </>
        )}

        {/* Subheading Settings */}
        {selectedComponent.type === 'subheading' && (
          <>
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Content
              </label>
              <input
                type="text"
                value={selectedComponent.content || ''}
                onChange={(e) => onUpdateComponent(selectedComponent.id, { content: e.target.value })}
                className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Enter subheading..."
              />
            </div>
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
          </>
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
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Signature Line
              </label>
              <div className="flex gap-2">
                <button
                  onClick={() => onUpdateComponent(selectedComponent.id, {
                    config: { ...selectedComponent.config, showLine: true }
                  })}
                  className={`flex-1 px-3 py-2 text-xs rounded transition-colors ${
                    selectedComponent.config.showLine !== false
                      ? 'bg-[#FFA500] text-black'
                      : 'bg-[#0a0a0a] hover:bg-[#2a2a2a]'
                  }`}
                >
                  Show
                </button>
                <button
                  onClick={() => onUpdateComponent(selectedComponent.id, {
                    config: { ...selectedComponent.config, showLine: false }
                  })}
                  className={`flex-1 px-3 py-2 text-xs rounded transition-colors ${
                    selectedComponent.config.showLine === false
                      ? 'bg-[#FFA500] text-black'
                      : 'bg-[#0a0a0a] hover:bg-[#2a2a2a]'
                  }`}
                >
                  Hide
                </button>
              </div>
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
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Size
              </label>
              <div className="flex items-center gap-2">
                <input
                  type="range"
                  min="60"
                  max="200"
                  step="10"
                  value={selectedComponent.config.size || 100}
                  onChange={(e) => onUpdateComponent(selectedComponent.id, {
                    config: { ...selectedComponent.config, size: Number(e.target.value) }
                  })}
                  className="flex-1"
                  style={{ accentColor: FINCEPT_COLORS.ORANGE }}
                />
                <span className="text-xs" style={{ color: FINCEPT_COLORS.TEXT_PRIMARY, minWidth: '35px' }}>
                  {selectedComponent.config.size || 100}px
                </span>
              </div>
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
                Opacity ({Math.round((selectedComponent.config.opacity || 0.1) * 100)}%)
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
            <div>
              <label className="text-xs font-semibold mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Rotation ({selectedComponent.config.rotation || -45}°)
              </label>
              <input
                type="range"
                min="-90"
                max="90"
                step="5"
                value={selectedComponent.config.rotation || -45}
                onChange={(e) => onUpdateComponent(selectedComponent.id, {
                  config: { ...selectedComponent.config, rotation: Number(e.target.value) }
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
