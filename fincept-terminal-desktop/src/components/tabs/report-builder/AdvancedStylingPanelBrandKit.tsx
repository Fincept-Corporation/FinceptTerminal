import React, { useState } from 'react';
import {
  Palette,
  Type,
  Image as ImageIcon,
  Plus,
  ChevronDown,
  ChevronRight,
  Save,
  X,
  RefreshCw,
  Trash2,
} from 'lucide-react';
import {
  brandKitService,
  BrandKit,
  BrandColor,
  BrandFont,
  ComponentTemplate,
} from '@/services/core/brandKitService';
import { toast } from '@/components/ui/terminal-toast';
import { useTerminalTheme } from '@/contexts/ThemeContext';

// ============ Brand Kit Tab ============
export interface BrandKitTabProps {
  brandKits: BrandKit[];
  activeBrandKit: BrandKit | null;
  setActiveBrandKit: (kit: BrandKit) => void;
  expandedSections: Record<string, boolean>;
  toggleSection: (section: string) => void;
  updateBrandKitColor: (colorId: string, updates: Partial<BrandColor>) => void;
  updateBrandKitFont: (fontId: string, updates: Partial<BrandFont>) => void;
  addColor: () => void;
  removeColor: (colorId: string) => void;
  handleLogoUpload: () => void;
  handleSaveBrandKit: () => void;
  handleCreateNewBrandKit: () => void;
  handleDeleteBrandKit: (id: string) => void;
}

export const BrandKitTab: React.FC<BrandKitTabProps> = ({
  brandKits,
  activeBrandKit,
  setActiveBrandKit,
  expandedSections,
  toggleSection,
  updateBrandKitColor,
  updateBrandKitFont,
  addColor,
  removeColor,
  handleLogoUpload,
  handleSaveBrandKit,
  handleCreateNewBrandKit,
  handleDeleteBrandKit,
}) => {
  const { colors } = useTerminalTheme();
  const availableFonts = brandKitService.getAvailableFonts();
  const presetKits = brandKitService.getPresetBrandKits();

  return (
    <div className="space-y-4">
      {/* Brand Kit Selector */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs font-semibold" style={{ color: colors.primary }}>
            SELECT BRAND KIT
          </label>
          <button
            onClick={handleCreateNewBrandKit}
            className="p-1 rounded hover:bg-[#2a2a2a] transition-colors"
            title="Create New"
          >
            <Plus size={14} style={{ color: colors.primary }} />
          </button>
        </div>
        <select
          value={activeBrandKit?.id || ''}
          onChange={(e) => {
            const kit = brandKits.find(k => k.id === e.target.value);
            if (kit) setActiveBrandKit(kit);
          }}
          className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
        >
          {brandKits.map(kit => (
            <option key={kit.id} value={kit.id}>{kit.name}</option>
          ))}
        </select>
      </div>

      {/* Preset Brand Kits */}
      <div className="space-y-2">
        <label className="text-xs font-semibold" style={{ color: colors.textMuted }}>
          QUICK PRESETS
        </label>
        <div className="grid grid-cols-2 gap-2">
          {presetKits.map((preset, idx) => (
            <button
              key={idx}
              onClick={() => {
                if (activeBrandKit) {
                  setActiveBrandKit({
                    ...activeBrandKit,
                    colors: preset.colors as BrandColor[],
                    fonts: preset.fonts as BrandFont[],
                  });
                }
              }}
              className="px-2 py-2 text-[10px] rounded border border-[#333333] hover:border-[#FFA500] transition-colors text-left"
            >
              <div className="flex gap-1 mb-1">
                {preset.colors?.slice(0, 4).map((c, i) => (
                  <div
                    key={i}
                    className="w-3 h-3 rounded-full"
                    style={{ backgroundColor: c.hex }}
                  />
                ))}
              </div>
              <span className="text-gray-300">{preset.name}</span>
            </button>
          ))}
        </div>
      </div>

      {activeBrandKit && (
        <>
          {/* Brand Kit Name */}
          <div className="space-y-1">
            <label className="text-xs" style={{ color: colors.textMuted }}>
              Brand Kit Name
            </label>
            <input
              type="text"
              value={activeBrandKit.name}
              onChange={(e) => setActiveBrandKit({ ...activeBrandKit, name: e.target.value })}
              className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
            />
          </div>

          {/* Colors Section */}
          <div className="border border-[#333333] rounded">
            <button
              onClick={() => toggleSection('colors')}
              className="w-full flex items-center justify-between p-3 hover:bg-[#2a2a2a] transition-colors"
            >
              <div className="flex items-center gap-2">
                <Palette size={14} style={{ color: colors.primary }} />
                <span className="text-xs font-semibold text-white">Colors</span>
              </div>
              {expandedSections.colors ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
            </button>
            {expandedSections.colors && (
              <div className="p-3 pt-0 space-y-2">
                {activeBrandKit.colors.map((color) => (
                  <div key={color.id} className="flex items-center gap-2">
                    <input
                      type="color"
                      value={color.hex}
                      onChange={(e) => updateBrandKitColor(color.id, { hex: e.target.value })}
                      className="w-8 h-8 rounded cursor-pointer border-0"
                    />
                    <input
                      type="text"
                      value={color.name}
                      onChange={(e) => updateBrandKitColor(color.id, { name: e.target.value })}
                      className="flex-1 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                    />
                    <select
                      value={color.usage}
                      onChange={(e) => updateBrandKitColor(color.id, { usage: e.target.value as any })}
                      className="px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                    >
                      <option value="primary">Primary</option>
                      <option value="secondary">Secondary</option>
                      <option value="accent">Accent</option>
                      <option value="background">Background</option>
                      <option value="text">Text</option>
                      <option value="custom">Custom</option>
                    </select>
                    <button
                      onClick={() => removeColor(color.id)}
                      className="p-1 hover:bg-[#333333] rounded text-red-500"
                    >
                      <X size={14} />
                    </button>
                  </div>
                ))}
                <button
                  onClick={addColor}
                  className="w-full py-2 text-xs rounded border border-dashed border-[#333333] hover:border-[#FFA500] transition-colors text-gray-400 hover:text-[#FFA500]"
                >
                  + Add Color
                </button>
              </div>
            )}
          </div>

          {/* Fonts Section */}
          <div className="border border-[#333333] rounded">
            <button
              onClick={() => toggleSection('fonts')}
              className="w-full flex items-center justify-between p-3 hover:bg-[#2a2a2a] transition-colors"
            >
              <div className="flex items-center gap-2">
                <Type size={14} style={{ color: colors.primary }} />
                <span className="text-xs font-semibold text-white">Fonts</span>
              </div>
              {expandedSections.fonts ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
            </button>
            {expandedSections.fonts && (
              <div className="p-3 pt-0 space-y-3">
                {activeBrandKit.fonts.map((font) => (
                  <div key={font.id} className="space-y-1">
                    <label className="text-[10px] text-gray-400 capitalize">{font.usage} Font</label>
                    <div className="flex gap-2">
                      <select
                        value={font.family}
                        onChange={(e) => updateBrandKitFont(font.id, { family: e.target.value })}
                        className="flex-1 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                      >
                        {availableFonts.map(f => (
                          <option key={f.family} value={f.family}>{f.name}</option>
                        ))}
                      </select>
                      <select
                        value={font.weight}
                        onChange={(e) => updateBrandKitFont(font.id, { weight: e.target.value })}
                        className="w-24 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                      >
                        <option value="300">Light</option>
                        <option value="400">Regular</option>
                        <option value="500">Medium</option>
                        <option value="600">Semi-Bold</option>
                        <option value="700">Bold</option>
                        <option value="800">Extra Bold</option>
                      </select>
                    </div>
                    <div
                      className="p-2 bg-[#0a0a0a] rounded text-sm"
                      style={{ fontFamily: font.family, fontWeight: font.weight }}
                    >
                      The quick brown fox jumps over the lazy dog
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* Logos Section */}
          <div className="border border-[#333333] rounded">
            <button
              onClick={() => toggleSection('logos')}
              className="w-full flex items-center justify-between p-3 hover:bg-[#2a2a2a] transition-colors"
            >
              <div className="flex items-center gap-2">
                <ImageIcon size={14} style={{ color: colors.primary }} />
                <span className="text-xs font-semibold text-white">Logos</span>
              </div>
              {expandedSections.logos ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
            </button>
            {expandedSections.logos && (
              <div className="p-3 pt-0 space-y-2">
                {activeBrandKit.logos.length === 0 ? (
                  <p className="text-xs text-gray-500 text-center py-2">No logos added</p>
                ) : (
                  activeBrandKit.logos.map((logo) => (
                    <div key={logo.id} className="flex items-center gap-2 p-2 bg-[#0a0a0a] rounded">
                      <ImageIcon size={16} className="text-gray-400" />
                      <span className="flex-1 text-xs text-gray-300 truncate">{logo.name}</span>
                      <span className="text-[10px] text-gray-500 capitalize">{logo.type}</span>
                    </div>
                  ))
                )}
                <button
                  onClick={handleLogoUpload}
                  className="w-full py-2 text-xs rounded border border-dashed border-[#333333] hover:border-[#FFA500] transition-colors text-gray-400 hover:text-[#FFA500]"
                >
                  + Upload Logo
                </button>
              </div>
            )}
          </div>

          {/* Save Button */}
          <button
            onClick={handleSaveBrandKit}
            className="w-full py-2 text-xs font-semibold rounded transition-colors flex items-center justify-center gap-2"
            style={{ backgroundColor: colors.primary, color: '#000' }}
          >
            <Save size={14} />
            Save Brand Kit
          </button>
        </>
      )}
    </div>
  );
};

// ============ Component Templates Tab ============
export interface ComponentTemplatesTabProps {
  templates: ComponentTemplate[];
  onSelect?: (template: ComponentTemplate) => void;
  onRefresh: () => void;
}

export const ComponentTemplatesTab: React.FC<ComponentTemplatesTabProps> = ({
  templates,
  onSelect,
  onRefresh,
}) => {
  const { colors } = useTerminalTheme();
  const [selectedCategory, setSelectedCategory] = useState<string>('all');

  const categories = ['all', 'text', 'layout', 'data', 'media', 'custom'];
  const filteredTemplates = selectedCategory === 'all'
    ? templates
    : templates.filter(t => t.category === selectedCategory);

  const handleSaveAsTemplate = async (template: ComponentTemplate) => {
    try {
      await brandKitService.saveComponentTemplate(template);
      toast.success('Template saved');
      onRefresh();
    } catch (error) {
      toast.error('Failed to save template');
    }
  };

  const handleDeleteTemplate = async (id: string) => {
    try {
      await brandKitService.deleteComponentTemplate(id);
      toast.success('Template deleted');
      onRefresh();
    } catch (error) {
      toast.error('Failed to delete template');
    }
  };

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <label className="text-xs font-semibold" style={{ color: colors.primary }}>
          COMPONENT TEMPLATES
        </label>
        <button
          onClick={onRefresh}
          className="p-1 rounded hover:bg-[#2a2a2a] transition-colors"
          title="Refresh"
        >
          <RefreshCw size={14} className="text-gray-400" />
        </button>
      </div>

      {/* Category Filter */}
      <div className="flex flex-wrap gap-1">
        {categories.map(cat => (
          <button
            key={cat}
            onClick={() => setSelectedCategory(cat)}
            className={`px-2 py-1 text-[10px] rounded capitalize transition-colors ${
              selectedCategory === cat
                ? 'bg-[#FFA500] text-black'
                : 'bg-[#0a0a0a] text-gray-400 hover:bg-[#2a2a2a]'
            }`}
          >
            {cat}
          </button>
        ))}
      </div>

      {/* Templates Grid */}
      <div className="space-y-2">
        {filteredTemplates.length === 0 ? (
          <p className="text-xs text-gray-500 text-center py-4">No templates in this category</p>
        ) : (
          filteredTemplates.map(template => (
            <div
              key={template.id}
              className="p-3 bg-[#0a0a0a] rounded border border-[#333333] hover:border-[#FFA500] transition-colors cursor-pointer"
              onClick={() => onSelect?.(template)}
            >
              <div className="flex items-start justify-between mb-2">
                <div>
                  <h4 className="text-xs font-semibold text-white">{template.name}</h4>
                  <p className="text-[10px] text-gray-500">{template.description}</p>
                </div>
                <div className="flex gap-1">
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      onSelect?.(template);
                    }}
                    className="p-1 rounded hover:bg-[#2a2a2a]"
                    title="Use Template"
                  >
                    <Plus size={12} style={{ color: colors.primary }} />
                  </button>
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      handleDeleteTemplate(template.id);
                    }}
                    className="p-1 rounded hover:bg-[#2a2a2a]"
                    title="Delete"
                  >
                    <Trash2 size={12} className="text-red-500" />
                  </button>
                </div>
              </div>
              <div className="flex items-center gap-2">
                <span className="text-[9px] px-2 py-0.5 rounded bg-[#1a1a1a] text-gray-400 capitalize">
                  {template.category}
                </span>
                <span className="text-[9px] px-2 py-0.5 rounded bg-[#1a1a1a] text-gray-400">
                  {template.component.type}
                </span>
              </div>
            </div>
          ))
        )}
      </div>

      {/* Info */}
      <div className="p-3 bg-[#0a0a0a] rounded border border-[#333333]">
        <p className="text-[10px] text-gray-500">
          Click on a template to add it to your report. You can also save custom components as templates
          from the Properties panel.
        </p>
      </div>
    </div>
  );
};
