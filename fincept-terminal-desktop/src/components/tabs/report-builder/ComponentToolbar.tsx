import React from 'react';
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
  Layout,
  Columns as ColumnsIcon,
  BookOpen,
  FileX,
  Bold,
  Italic,
} from 'lucide-react';
import { DndContext, closestCenter, KeyboardSensor, PointerSensor, useSensor, useSensors, DragEndEvent } from '@dnd-kit/core';
import { SortableContext, sortableKeyboardCoordinates, verticalListSortingStrategy } from '@dnd-kit/sortable';
import { useTranslation } from 'react-i18next';
import { SortableComponent } from './SortableComponent';
import { ComponentToolbarProps } from './types';
import { FINCEPT_COLORS, PAGE_THEMES, FONT_FAMILIES, PRESET_COLORS } from './constants';

export const ComponentToolbar: React.FC<ComponentToolbarProps> = ({
  onAddComponent,
  template,
  selectedComponent,
  onSelectComponent,
  onDeleteComponent,
  onDuplicateComponent,
  onDragEnd,
  pageTheme,
  onPageThemeChange,
  customBgColor,
  onCustomBgColorChange,
  showColorPicker,
  onShowColorPickerChange,
  fontFamily,
  onFontFamilyChange,
  defaultFontSize,
  onDefaultFontSizeChange,
  isBold,
  onIsBoldChange,
  isItalic,
  onIsItalicChange,
}) => {
  const { t } = useTranslation('reportBuilder');

  const sensors = useSensors(
    useSensor(PointerSensor),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    })
  );

  // HSL to Hex conversion helper
  const hslToHex = (h: number, s: number, l: number) => {
    l /= 100;
    const a = s * Math.min(l, 1 - l) / 100;
    const f = (n: number) => {
      const k = (n + h / 30) % 12;
      const color = l - a * Math.max(Math.min(k - 3, 9 - k, 1), -1);
      return Math.round(255 * color).toString(16).padStart(2, '0');
    };
    return `#${f(0)}${f(8)}${f(4)}`;
  };

  return (
    <div className="w-1/5 border-r overflow-y-auto" style={{ borderColor: FINCEPT_COLORS.BORDER, backgroundColor: FINCEPT_COLORS.PANEL_BG }}>
      <div className="p-3">
        {/* Add Components */}
        <div className="mb-4">
          <h3 className="text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.ORANGE }}>{t('components.title')}</h3>
          <div className="space-y-1">
            <button
              onClick={() => onAddComponent('heading')}
              className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Type size={14} />
              Heading
            </button>
            <button
              onClick={() => onAddComponent('text')}
              className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <AlignLeft size={14} />
              Text
            </button>
            <button
              onClick={() => onAddComponent('chart')}
              className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <BarChart3 size={14} />
              Chart
            </button>
            <button
              onClick={() => onAddComponent('table')}
              className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Table size={14} />
              Table
            </button>
            <button
              onClick={() => onAddComponent('image')}
              className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <ImageIcon size={14} />
              Image
            </button>
            <button
              onClick={() => onAddComponent('code')}
              className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Code size={14} />
              Code Block
            </button>
            <button
              onClick={() => onAddComponent('divider')}
              className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Minus size={14} />
              Divider
            </button>
            <button
              onClick={() => onAddComponent('quote' as any)}
              className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Quote size={14} />
              Quote/Callout
            </button>
            <button
              onClick={() => onAddComponent('list' as any)}
              className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <List size={14} />
              List
            </button>
          </div>
        </div>

        {/* Data Components */}
        <div className="mb-4">
          <h3 className="text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.ORANGE }}>DATA</h3>
          <div className="grid grid-cols-2 gap-1">
            <button
              onClick={() => onAddComponent('kpi' as any)}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <TrendingUp size={14} />
              <span className="text-[9px]">KPI</span>
            </button>
            <button
              onClick={() => onAddComponent('sparkline' as any)}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Activity size={14} />
              <span className="text-[9px]">Sparkline</span>
            </button>
            <button
              onClick={() => onAddComponent('liveTable' as any)}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Database size={14} />
              <span className="text-[9px]">Data Table</span>
            </button>
            <button
              onClick={() => onAddComponent('dynamicChart' as any)}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <BarChart3 size={14} />
              <span className="text-[9px]">Chart</span>
            </button>
          </div>
        </div>

        {/* Document Components */}
        <div className="mb-4">
          <h3 className="text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.ORANGE }}>DOCUMENT</h3>
          <div className="grid grid-cols-2 gap-1">
            <button
              onClick={() => onAddComponent('toc' as any)}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Hash size={14} />
              <span className="text-[9px]">TOC</span>
            </button>
            <button
              onClick={() => onAddComponent('signature' as any)}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <PenTool size={14} />
              <span className="text-[9px]">Signature</span>
            </button>
            <button
              onClick={() => onAddComponent('disclaimer' as any)}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <AlertTriangle size={14} />
              <span className="text-[9px]">Disclaimer</span>
            </button>
            <button
              onClick={() => onAddComponent('qrcode' as any)}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <QrCode size={14} />
              <span className="text-[9px]">QR Code</span>
            </button>
            <button
              onClick={() => onAddComponent('watermark' as any)}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Stamp size={14} />
              <span className="text-[9px]">Watermark</span>
            </button>
          </div>
        </div>

        {/* Layout Components */}
        <div className="mb-4">
          <h3 className="text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.ORANGE }}>{t('layout.title')}</h3>
          <div className="grid grid-cols-2 gap-1">
            <button
              onClick={() => onAddComponent('section')}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <Layout size={14} />
              <span className="text-[9px]">Section</span>
            </button>
            <button
              onClick={() => onAddComponent('columns')}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <ColumnsIcon size={14} />
              <span className="text-[9px]">Columns</span>
            </button>
            <button
              onClick={() => onAddComponent('coverpage')}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <BookOpen size={14} />
              <span className="text-[9px]">Cover</span>
            </button>
            <button
              onClick={() => onAddComponent('pagebreak')}
              className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
              style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
            >
              <FileX size={14} />
              <span className="text-[9px]">Break</span>
            </button>
          </div>
        </div>

        {/* Settings */}
        <div className="mb-4">
          <h3 className="text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.ORANGE }}>{t('settings.title')}</h3>
          <div className="space-y-2">
            <div>
              <label className="text-[9px] block mb-1" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Page Theme
              </label>
              <select
                value={pageTheme}
                onChange={(e) => onPageThemeChange(e.target.value as any)}
                className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              >
                {Object.entries(PAGE_THEMES).map(([value, label]) => (
                  <option key={value} value={value}>{label}</option>
                ))}
              </select>
            </div>

            {/* Color Picker for Classic Theme */}
            {pageTheme === 'classic' && (
              <div>
                <div className="flex items-center justify-between mb-1">
                  <label className="text-[9px]" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                    Background Color
                  </label>
                  <button
                    onClick={() => onShowColorPickerChange(!showColorPicker)}
                    className="text-[9px] px-2 py-1 rounded hover:bg-[#2a2a2a]"
                    style={{ color: FINCEPT_COLORS.ORANGE }}
                  >
                    {showColorPicker ? 'Hide Picker' : 'Show Picker'}
                  </button>
                </div>

                {/* Current Color Display */}
                <div className="flex items-center gap-2 mb-2">
                  <div
                    className="w-8 h-8 rounded border-2 border-[#333333]"
                    style={{ backgroundColor: customBgColor }}
                  />
                  <input
                    type="text"
                    value={customBgColor}
                    onChange={(e) => onCustomBgColorChange(e.target.value)}
                    className="flex-1 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                    style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                    placeholder="#ffffff"
                  />
                </div>

                {/* Advanced Color Picker */}
                {showColorPicker && (
                  <div className="p-3 rounded bg-[#0a0a0a] border border-[#333333] space-y-3">
                    {/* Main Gradient Palette */}
                    <div>
                      <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                        Color Palette
                      </label>
                      <div
                        className="w-full h-32 rounded cursor-crosshair border border-[#333333] relative"
                        style={{
                          background: `
                            linear-gradient(to bottom, transparent, black),
                            linear-gradient(to right, white, hsl(${customBgColor.startsWith('#') ? 0 : 120}, 100%, 50%))
                          `
                        }}
                        onClick={(e) => {
                          const rect = e.currentTarget.getBoundingClientRect();
                          const x = e.clientX - rect.left;
                          const y = e.clientY - rect.top;
                          const percentX = x / rect.width;
                          const percentY = y / rect.height;
                          const hue = percentX * 360;
                          const lightness = 100 - (percentY * 50);
                          onCustomBgColorChange(hslToHex(hue, 100, lightness));
                        }}
                      >
                        <div
                          className="absolute w-3 h-3 border-2 border-white rounded-full shadow-lg pointer-events-none"
                          style={{
                            left: '50%',
                            top: '10%',
                            transform: 'translate(-50%, -50%)'
                          }}
                        />
                      </div>
                    </div>

                    {/* Hue Slider */}
                    <div>
                      <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                        Hue
                      </label>
                      <div
                        className="w-full h-6 rounded cursor-pointer border border-[#333333]"
                        style={{
                          background: 'linear-gradient(to right, #ff0000, #ffff00, #00ff00, #00ffff, #0000ff, #ff00ff, #ff0000)'
                        }}
                        onClick={(e) => {
                          const rect = e.currentTarget.getBoundingClientRect();
                          const x = e.clientX - rect.left;
                          const percent = x / rect.width;
                          const hue = Math.round(percent * 360);
                          onCustomBgColorChange(hslToHex(hue, 100, 50));
                        }}
                      />
                    </div>

                    {/* Lightness Slider */}
                    <div>
                      <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                        Lightness
                      </label>
                      <div
                        className="w-full h-6 rounded cursor-pointer border border-[#333333]"
                        style={{
                          background: 'linear-gradient(to right, #000000, #808080, #ffffff)'
                        }}
                        onClick={(e) => {
                          const rect = e.currentTarget.getBoundingClientRect();
                          const x = e.clientX - rect.left;
                          const percent = x / rect.width;
                          const lightness = Math.round(percent * 100);
                          onCustomBgColorChange(hslToHex(120, 0, lightness));
                        }}
                      />
                    </div>

                    {/* Preset Colors */}
                    <div>
                      <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                        Preset Colors
                      </label>
                      <div className="grid grid-cols-8 gap-1">
                        {PRESET_COLORS.map(color => (
                          <button
                            key={color}
                            onClick={() => onCustomBgColorChange(color)}
                            className={`w-full aspect-square rounded border-2 transition-all hover:scale-110 ${
                              customBgColor.toLowerCase() === color.toLowerCase()
                                ? 'border-[#FFA500] ring-2 ring-[#FFA500]'
                                : 'border-[#333333] hover:border-[#666]'
                            }`}
                            style={{ backgroundColor: color }}
                            title={color}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                )}
              </div>
            )}

            <div>
              <label className="text-[9px] block mb-1" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Report Title
              </label>
              <input
                type="text"
                value={template.metadata.title}
                readOnly
                className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] outline-none opacity-60"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
                placeholder="Edit in Properties panel..."
              />
            </div>

            <div>
              <label className="text-[9px] block mb-1" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Font Family
              </label>
              <select
                value={fontFamily}
                onChange={(e) => onFontFamilyChange(e.target.value)}
                className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                style={{ color: FINCEPT_COLORS.TEXT_PRIMARY }}
              >
                {FONT_FAMILIES.map(font => (
                  <option key={font.value} value={font.value}>{font.label}</option>
                ))}
              </select>
            </div>

            <div>
              <label className="text-[9px] block mb-1" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Base Font Size
              </label>
              <div className="flex items-center gap-2">
                <input
                  type="range"
                  min="8"
                  max="16"
                  value={defaultFontSize}
                  onChange={(e) => onDefaultFontSizeChange(Number(e.target.value))}
                  className="flex-1"
                  style={{ accentColor: FINCEPT_COLORS.ORANGE }}
                />
                <span className="text-xs" style={{ color: FINCEPT_COLORS.TEXT_PRIMARY, minWidth: '30px' }}>
                  {defaultFontSize}pt
                </span>
              </div>
            </div>

            <div>
              <label className="text-[9px] block mb-1" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                Text Style
              </label>
              <div className="flex gap-2">
                <button
                  onClick={() => onIsBoldChange(!isBold)}
                  className={`flex-1 px-3 py-2 rounded border transition-all ${
                    isBold
                      ? 'bg-[#FFA500] border-[#FFA500] text-black'
                      : 'bg-[#0a0a0a] border-[#333333] text-gray-400 hover:border-[#FFA500]'
                  }`}
                  title="Toggle Bold"
                >
                  <Bold size={16} className="mx-auto" />
                </button>
                <button
                  onClick={() => onIsItalicChange(!isItalic)}
                  className={`flex-1 px-3 py-2 rounded border transition-all ${
                    isItalic
                      ? 'bg-[#FFA500] border-[#FFA500] text-black'
                      : 'bg-[#0a0a0a] border-[#333333] text-gray-400 hover:border-[#FFA500]'
                  }`}
                  title="Toggle Italic"
                >
                  <Italic size={16} className="mx-auto" />
                </button>
              </div>
            </div>
          </div>
        </div>

        {/* Document Structure */}
        <div>
          <h3 className="text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.ORANGE }}>DOCUMENT STRUCTURE</h3>
          <div className="space-y-0">
            {template.components.length === 0 ? (
              <p className="text-xs text-center py-4" style={{ color: FINCEPT_COLORS.TEXT_SECONDARY }}>
                No components yet
              </p>
            ) : (
              <DndContext
                sensors={sensors}
                collisionDetection={closestCenter}
                onDragEnd={onDragEnd}
              >
                <SortableContext
                  items={template.components.map(c => c.id)}
                  strategy={verticalListSortingStrategy}
                >
                  {template.components.map((component) => (
                    <SortableComponent
                      key={component.id}
                      component={component}
                      isSelected={selectedComponent === component.id}
                      onClick={() => onSelectComponent(component.id)}
                      onDelete={() => onDeleteComponent(component.id)}
                      onDuplicate={() => onDuplicateComponent(component.id)}
                    />
                  ))}
                </SortableContext>
              </DndContext>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default ComponentToolbar;
