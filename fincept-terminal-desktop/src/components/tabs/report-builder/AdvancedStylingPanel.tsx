import React, { useState, useEffect, useCallback } from 'react';
import {
  Palette,
  Type,
  Image as ImageIcon,
  Code,
  FileText,
  Save,
  Trash2,
  Plus,
  ChevronDown,
  ChevronRight,
  Copy,
  Check,
  Edit2,
  X,
  AlignLeft,
  AlignCenter,
  AlignRight,
  AlignJustify,
  LayoutTemplate,
  Bookmark,
  Settings2,
  Layers,
  RefreshCw,
} from 'lucide-react';
import {
  brandKitService,
  BrandKit,
  BrandColor,
  BrandFont,
  BrandLogo,
  ComponentTemplate,
  HeaderFooterTemplate,
  ParagraphStyles,
} from '@/services/core/brandKitService';
import { open } from '@tauri-apps/plugin-dialog';
import { toast } from '@/components/ui/terminal-toast';
import { useTerminalTheme } from '@/contexts/ThemeContext';

// Theme colors provided by useTerminalTheme() hook

interface AdvancedStylingPanelProps {
  onBrandKitChange?: (brandKit: BrandKit) => void;
  onComponentTemplateSelect?: (template: ComponentTemplate) => void;
  onStylesChange?: (styles: {
    customCSS: string;
    headerTemplate: HeaderFooterTemplate;
    footerTemplate: HeaderFooterTemplate;
    paragraphStyles: ParagraphStyles;
  }) => void;
}

type TabType = 'brandKit' | 'templates' | 'headerFooter' | 'typography' | 'customCSS';

export const AdvancedStylingPanel: React.FC<AdvancedStylingPanelProps> = ({
  onBrandKitChange,
  onComponentTemplateSelect,
  onStylesChange,
}) => {
  const { colors, fontSize } = useTerminalTheme();
  const [activeTab, setActiveTab] = useState<TabType>('brandKit');
  const [brandKits, setBrandKits] = useState<BrandKit[]>([]);
  const [activeBrandKit, setActiveBrandKit] = useState<BrandKit | null>(null);
  const [componentTemplates, setComponentTemplates] = useState<ComponentTemplate[]>([]);
  const [isEditing, setIsEditing] = useState(false);
  const [expandedSections, setExpandedSections] = useState<Record<string, boolean>>({
    colors: true,
    fonts: true,
    logos: false,
  });

  // Load data on mount
  useEffect(() => {
    loadData();
  }, []);

  const loadData = async () => {
    try {
      const kits = await brandKitService.getAllBrandKits();
      setBrandKits(kits);

      const active = await brandKitService.getActiveBrandKit();
      setActiveBrandKit(active);

      const templates = await brandKitService.getAllComponentTemplates();
      setComponentTemplates(templates);
    } catch (error) {
      console.error('Error loading styling data:', error);
      toast.error('Failed to load styling data');
    }
  };

  const handleSaveBrandKit = async () => {
    if (!activeBrandKit) return;
    try {
      await brandKitService.saveBrandKit(activeBrandKit);
      await brandKitService.setActiveBrandKit(activeBrandKit.id);
      toast.success('Brand kit saved');
      onBrandKitChange?.(activeBrandKit);
      loadData();
    } catch (error) {
      toast.error('Failed to save brand kit');
    }
  };

  const handleCreateNewBrandKit = async () => {
    const newKit = brandKitService.getDefaultBrandKit();
    newKit.name = `Brand Kit ${brandKits.length + 1}`;
    newKit.isDefault = false;
    setActiveBrandKit(newKit);
    setIsEditing(true);
  };

  const handleDeleteBrandKit = async (id: string) => {
    try {
      await brandKitService.deleteBrandKit(id);
      toast.success('Brand kit deleted');
      loadData();
    } catch (error) {
      toast.error('Failed to delete brand kit');
    }
  };

  const toggleSection = (section: string) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  const updateBrandKitColor = (colorId: string, updates: Partial<BrandColor>) => {
    if (!activeBrandKit) return;
    setActiveBrandKit({
      ...activeBrandKit,
      colors: activeBrandKit.colors.map(c =>
        c.id === colorId ? { ...c, ...updates } : c
      ),
    });
  };

  const updateBrandKitFont = (fontId: string, updates: Partial<BrandFont>) => {
    if (!activeBrandKit) return;
    setActiveBrandKit({
      ...activeBrandKit,
      fonts: activeBrandKit.fonts.map(f =>
        f.id === fontId ? { ...f, ...updates } : f
      ),
    });
  };

  const addColor = () => {
    if (!activeBrandKit) return;
    const newColor: BrandColor = {
      id: `color-${Date.now()}`,
      name: 'New Color',
      hex: '#888888',
      usage: 'custom',
    };
    setActiveBrandKit({
      ...activeBrandKit,
      colors: [...activeBrandKit.colors, newColor],
    });
  };

  const removeColor = (colorId: string) => {
    if (!activeBrandKit) return;
    setActiveBrandKit({
      ...activeBrandKit,
      colors: activeBrandKit.colors.filter(c => c.id !== colorId),
    });
  };

  const handleLogoUpload = async () => {
    try {
      const selected = await open({
        multiple: false,
        filters: [{ name: 'Images', extensions: ['png', 'jpg', 'jpeg', 'svg'] }],
      });

      if (selected && typeof selected === 'string' && activeBrandKit) {
        const newLogo: BrandLogo = {
          id: `logo-${Date.now()}`,
          name: selected.split(/[/\\]/).pop() || 'Logo',
          path: selected,
          type: 'primary',
        };
        setActiveBrandKit({
          ...activeBrandKit,
          logos: [...activeBrandKit.logos, newLogo],
        });
        toast.success('Logo added');
      }
    } catch (error) {
      toast.error('Failed to upload logo');
    }
  };

  const renderTabButton = (tab: TabType, icon: React.ReactNode, label: string) => (
    <button
      onClick={() => setActiveTab(tab)}
      className={`flex items-center gap-2 px-3 py-2 text-xs rounded transition-colors ${
        activeTab === tab
          ? 'bg-[#FFA500] text-black font-semibold'
          : 'bg-transparent hover:bg-[#2a2a2a] text-gray-300'
      }`}
    >
      {icon}
      <span>{label}</span>
    </button>
  );

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: colors.panel }}>
      {/* Tab Navigation */}
      <div className="flex flex-wrap gap-1 p-2 border-b" style={{ borderColor: colors.panel }}>
        {renderTabButton('brandKit', <Palette size={14} />, 'Brand Kit')}
        {renderTabButton('templates', <LayoutTemplate size={14} />, 'Templates')}
        {renderTabButton('headerFooter', <Layers size={14} />, 'Header/Footer')}
        {renderTabButton('typography', <Type size={14} />, 'Typography')}
        {renderTabButton('customCSS', <Code size={14} />, 'Custom CSS')}
      </div>

      {/* Content Area */}
      <div className="flex-1 overflow-y-auto p-3">
        {activeTab === 'brandKit' && (
          <BrandKitTab
            brandKits={brandKits}
            activeBrandKit={activeBrandKit}
            setActiveBrandKit={setActiveBrandKit}
            expandedSections={expandedSections}
            toggleSection={toggleSection}
            updateBrandKitColor={updateBrandKitColor}
            updateBrandKitFont={updateBrandKitFont}
            addColor={addColor}
            removeColor={removeColor}
            handleLogoUpload={handleLogoUpload}
            handleSaveBrandKit={handleSaveBrandKit}
            handleCreateNewBrandKit={handleCreateNewBrandKit}
            handleDeleteBrandKit={handleDeleteBrandKit}
          />
        )}

        {activeTab === 'templates' && (
          <ComponentTemplatesTab
            templates={componentTemplates}
            onSelect={onComponentTemplateSelect}
            onRefresh={loadData}
          />
        )}

        {activeTab === 'headerFooter' && (
          <HeaderFooterTab
            brandKit={activeBrandKit}
            onUpdate={(header, footer) => {
              if (activeBrandKit) {
                const updated = {
                  ...activeBrandKit,
                  headerTemplate: header,
                  footerTemplate: footer,
                };
                setActiveBrandKit(updated);
                onStylesChange?.({
                  customCSS: updated.customCSS || '',
                  headerTemplate: header,
                  footerTemplate: footer,
                  paragraphStyles: updated.paragraphStyles,
                });
              }
            }}
          />
        )}

        {activeTab === 'typography' && (
          <TypographyTab
            brandKit={activeBrandKit}
            onUpdate={(paragraphStyles) => {
              if (activeBrandKit) {
                const updated = {
                  ...activeBrandKit,
                  paragraphStyles,
                };
                setActiveBrandKit(updated);
                onStylesChange?.({
                  customCSS: updated.customCSS || '',
                  headerTemplate: updated.headerTemplate!,
                  footerTemplate: updated.footerTemplate!,
                  paragraphStyles,
                });
              }
            }}
          />
        )}

        {activeTab === 'customCSS' && (
          <CustomCSSTab
            brandKit={activeBrandKit}
            onUpdate={(css) => {
              if (activeBrandKit) {
                const updated = { ...activeBrandKit, customCSS: css };
                setActiveBrandKit(updated);
                onStylesChange?.({
                  customCSS: css,
                  headerTemplate: updated.headerTemplate!,
                  footerTemplate: updated.footerTemplate!,
                  paragraphStyles: updated.paragraphStyles,
                });
              }
            }}
          />
        )}
      </div>
    </div>
  );
};

// ============ Brand Kit Tab ============
interface BrandKitTabProps {
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

const BrandKitTab: React.FC<BrandKitTabProps> = ({
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
interface ComponentTemplatesTabProps {
  templates: ComponentTemplate[];
  onSelect?: (template: ComponentTemplate) => void;
  onRefresh: () => void;
}

const ComponentTemplatesTab: React.FC<ComponentTemplatesTabProps> = ({
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

// ============ Header/Footer Tab ============
interface HeaderFooterTabProps {
  brandKit: BrandKit | null;
  onUpdate: (header: HeaderFooterTemplate, footer: HeaderFooterTemplate) => void;
}

const HeaderFooterTab: React.FC<HeaderFooterTabProps> = ({ brandKit, onUpdate }) => {
  const { colors } = useTerminalTheme();
  const [editingSection, setEditingSection] = useState<'header' | 'footer'>('header');

  if (!brandKit) {
    return (
      <div className="text-center py-8 text-gray-500 text-xs">
        Select a brand kit first
      </div>
    );
  }

  const template = editingSection === 'header' ? brandKit.headerTemplate : brandKit.footerTemplate;

  const updateTemplate = (updates: Partial<HeaderFooterTemplate>) => {
    const newHeader = editingSection === 'header'
      ? { ...brandKit.headerTemplate!, ...updates }
      : brandKit.headerTemplate!;
    const newFooter = editingSection === 'footer'
      ? { ...brandKit.footerTemplate!, ...updates }
      : brandKit.footerTemplate!;
    onUpdate(newHeader, newFooter);
  };

  return (
    <div className="space-y-4">
      <label className="text-xs font-semibold" style={{ color: colors.primary }}>
        HEADER & FOOTER
      </label>

      {/* Section Toggle */}
      <div className="flex gap-2">
        <button
          onClick={() => setEditingSection('header')}
          className={`flex-1 py-2 text-xs rounded transition-colors ${
            editingSection === 'header'
              ? 'bg-[#FFA500] text-black font-semibold'
              : 'bg-[#0a0a0a] text-gray-400'
          }`}
        >
          Header
        </button>
        <button
          onClick={() => setEditingSection('footer')}
          className={`flex-1 py-2 text-xs rounded transition-colors ${
            editingSection === 'footer'
              ? 'bg-[#FFA500] text-black font-semibold'
              : 'bg-[#0a0a0a] text-gray-400'
          }`}
        >
          Footer
        </button>
      </div>

      {template && (
        <div className="space-y-3">
          {/* Enable Toggle */}
          <div className="flex items-center justify-between">
            <label className="text-xs text-gray-300">Enable {editingSection}</label>
            <button
              onClick={() => updateTemplate({ enabled: !template.enabled })}
              className={`w-10 h-5 rounded-full transition-colors ${
                template.enabled ? 'bg-[#FFA500]' : 'bg-[#333333]'
              }`}
            >
              <div
                className={`w-4 h-4 rounded-full bg-white transition-transform ${
                  template.enabled ? 'translate-x-5' : 'translate-x-0.5'
                }`}
              />
            </button>
          </div>

          {template.enabled && (
            <>
              {/* Content */}
              <div className="space-y-1">
                <label className="text-[10px] text-gray-500">
                  Content (use {'{company}'}, {'{date}'}, {'{title}'})
                </label>
                <input
                  type="text"
                  value={template.content}
                  onChange={(e) => updateTemplate({ content: e.target.value })}
                  className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                  placeholder="Enter content..."
                />
              </div>

              {/* Alignment */}
              <div className="space-y-1">
                <label className="text-[10px] text-gray-500">Text Alignment</label>
                <div className="flex gap-2">
                  {(['left', 'center', 'right'] as const).map(align => (
                    <button
                      key={align}
                      onClick={() => updateTemplate({ alignment: align })}
                      className={`flex-1 py-2 rounded transition-colors ${
                        template.alignment === align
                          ? 'bg-[#FFA500] text-black'
                          : 'bg-[#0a0a0a] text-gray-400'
                      }`}
                    >
                      {align === 'left' && <AlignLeft size={14} className="mx-auto" />}
                      {align === 'center' && <AlignCenter size={14} className="mx-auto" />}
                      {align === 'right' && <AlignRight size={14} className="mx-auto" />}
                    </button>
                  ))}
                </div>
              </div>

              {/* Page Numbers */}
              <div className="space-y-2">
                <div className="flex items-center justify-between">
                  <label className="text-xs text-gray-300">Show Page Numbers</label>
                  <button
                    onClick={() => updateTemplate({ showPageNumbers: !template.showPageNumbers })}
                    className={`w-10 h-5 rounded-full transition-colors ${
                      template.showPageNumbers ? 'bg-[#FFA500]' : 'bg-[#333333]'
                    }`}
                  >
                    <div
                      className={`w-4 h-4 rounded-full bg-white transition-transform ${
                        template.showPageNumbers ? 'translate-x-5' : 'translate-x-0.5'
                      }`}
                    />
                  </button>
                </div>

                {template.showPageNumbers && (
                  <div className="grid grid-cols-2 gap-2">
                    <div>
                      <label className="text-[10px] text-gray-500">Format</label>
                      <select
                        value={template.pageNumberFormat}
                        onChange={(e) => updateTemplate({ pageNumberFormat: e.target.value as any })}
                        className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                      >
                        <option value="numeric">1, 2, 3...</option>
                        <option value="roman">I, II, III...</option>
                        <option value="alphanumeric">A, B, C...</option>
                      </select>
                    </div>
                    <div>
                      <label className="text-[10px] text-gray-500">Position</label>
                      <select
                        value={template.pageNumberPosition}
                        onChange={(e) => updateTemplate({ pageNumberPosition: e.target.value as any })}
                        className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                      >
                        <option value="left">Left</option>
                        <option value="center">Center</option>
                        <option value="right">Right</option>
                      </select>
                    </div>
                  </div>
                )}
              </div>

              {/* Colors */}
              <div className="grid grid-cols-2 gap-2">
                <div className="space-y-1">
                  <label className="text-[10px] text-gray-500">Background</label>
                  <div className="flex gap-2">
                    <input
                      type="color"
                      value={template.backgroundColor || '#FFFFFF'}
                      onChange={(e) => updateTemplate({ backgroundColor: e.target.value })}
                      className="w-8 h-8 rounded cursor-pointer"
                    />
                    <input
                      type="text"
                      value={template.backgroundColor || '#FFFFFF'}
                      onChange={(e) => updateTemplate({ backgroundColor: e.target.value })}
                      className="flex-1 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                    />
                  </div>
                </div>
                <div className="space-y-1">
                  <label className="text-[10px] text-gray-500">Text Color</label>
                  <div className="flex gap-2">
                    <input
                      type="color"
                      value={template.textColor || '#666666'}
                      onChange={(e) => updateTemplate({ textColor: e.target.value })}
                      className="w-8 h-8 rounded cursor-pointer"
                    />
                    <input
                      type="text"
                      value={template.textColor || '#666666'}
                      onChange={(e) => updateTemplate({ textColor: e.target.value })}
                      className="flex-1 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                    />
                  </div>
                </div>
              </div>

              {/* Font Size & Height */}
              <div className="grid grid-cols-2 gap-2">
                <div className="space-y-1">
                  <label className="text-[10px] text-gray-500">Font Size (pt)</label>
                  <input
                    type="text"
                    inputMode="numeric"
                    value={template.fontSize || 10}
                    onChange={(e) => {
                      const v = e.target.value;
                      if (v === '' || /^\d*\.?\d*$/.test(v)) {
                        updateTemplate({ fontSize: v === '' ? 10 : Number(v) });
                      }
                    }}
                    className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                  />
                </div>
                <div className="space-y-1">
                  <label className="text-[10px] text-gray-500">Height (px)</label>
                  <input
                    type="text"
                    inputMode="numeric"
                    value={template.height || 30}
                    onChange={(e) => {
                      const v = e.target.value;
                      if (v === '' || /^\d*\.?\d*$/.test(v)) {
                        updateTemplate({ height: v === '' ? 30 : Number(v) });
                      }
                    }}
                    className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-white"
                  />
                </div>
              </div>

              {/* Border */}
              <div className="flex items-center justify-between">
                <label className="text-xs text-gray-300">
                  {editingSection === 'header' ? 'Border Bottom' : 'Border Top'}
                </label>
                <button
                  onClick={() => {
                    if (editingSection === 'header') {
                      updateTemplate({ borderBottom: !template.borderBottom });
                    } else {
                      updateTemplate({ borderTop: !template.borderTop });
                    }
                  }}
                  className={`w-10 h-5 rounded-full transition-colors ${
                    (editingSection === 'header' ? template.borderBottom : template.borderTop)
                      ? 'bg-[#FFA500]'
                      : 'bg-[#333333]'
                  }`}
                >
                  <div
                    className={`w-4 h-4 rounded-full bg-white transition-transform ${
                      (editingSection === 'header' ? template.borderBottom : template.borderTop)
                        ? 'translate-x-5'
                        : 'translate-x-0.5'
                    }`}
                  />
                </button>
              </div>

              {/* Show Date */}
              <div className="flex items-center justify-between">
                <label className="text-xs text-gray-300">Show Date</label>
                <button
                  onClick={() => updateTemplate({ showDate: !template.showDate })}
                  className={`w-10 h-5 rounded-full transition-colors ${
                    template.showDate ? 'bg-[#FFA500]' : 'bg-[#333333]'
                  }`}
                >
                  <div
                    className={`w-4 h-4 rounded-full bg-white transition-transform ${
                      template.showDate ? 'translate-x-5' : 'translate-x-0.5'
                    }`}
                  />
                </button>
              </div>
            </>
          )}
        </div>
      )}

      {/* Preview */}
      <div className="space-y-2">
        <label className="text-[10px] text-gray-500">Preview</label>
        <div className="bg-white p-4 rounded">
          {brandKit.headerTemplate?.enabled && (
            <div
              className="mb-4 pb-2"
              style={{
                borderBottom: brandKit.headerTemplate.borderBottom ? '1px solid #E0E0E0' : 'none',
                fontSize: `${brandKit.headerTemplate.fontSize}pt`,
                color: brandKit.headerTemplate.textColor,
                backgroundColor: brandKit.headerTemplate.backgroundColor,
                textAlign: brandKit.headerTemplate.alignment,
                minHeight: brandKit.headerTemplate.height,
                display: 'flex',
                alignItems: 'center',
                justifyContent: brandKit.headerTemplate.alignment === 'center' ? 'center' :
                  brandKit.headerTemplate.alignment === 'right' ? 'flex-end' : 'flex-start',
              }}
            >
              <span>{brandKit.headerTemplate.content.replace('{company}', 'Your Company')}</span>
              {brandKit.headerTemplate.showPageNumbers && brandKit.headerTemplate.pageNumberPosition !== brandKit.headerTemplate.alignment && (
                <span className="ml-auto">Page 1</span>
              )}
            </div>
          )}
          <div className="h-20 bg-gray-100 rounded flex items-center justify-center text-gray-400 text-xs">
            Report Content
          </div>
          {brandKit.footerTemplate?.enabled && (
            <div
              className="mt-4 pt-2"
              style={{
                borderTop: brandKit.footerTemplate.borderTop ? '1px solid #E0E0E0' : 'none',
                fontSize: `${brandKit.footerTemplate.fontSize}pt`,
                color: brandKit.footerTemplate.textColor,
                backgroundColor: brandKit.footerTemplate.backgroundColor,
                textAlign: brandKit.footerTemplate.alignment,
                minHeight: brandKit.footerTemplate.height,
                display: 'flex',
                alignItems: 'center',
                justifyContent: brandKit.footerTemplate.alignment === 'center' ? 'center' :
                  brandKit.footerTemplate.alignment === 'right' ? 'flex-end' : 'flex-start',
              }}
            >
              <span>{brandKit.footerTemplate.content.replace('{company}', 'Your Company')}</span>
              {brandKit.footerTemplate.showPageNumbers && (
                <span className={brandKit.footerTemplate.pageNumberPosition === 'center' ? '' : 'ml-auto'}>
                  {brandKit.footerTemplate.pageNumberPosition === 'center' ? ' - Page 1 -' : 'Page 1'}
                </span>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

// ============ Typography Tab ============
interface TypographyTabProps {
  brandKit: BrandKit | null;
  onUpdate: (styles: ParagraphStyles) => void;
}

const TypographyTab: React.FC<TypographyTabProps> = ({ brandKit, onUpdate }) => {
  const { colors } = useTerminalTheme();
  if (!brandKit) {
    return (
      <div className="text-center py-8 text-gray-500 text-xs">
        Select a brand kit first
      </div>
    );
  }

  const styles = brandKit.paragraphStyles;

  const updateStyles = (updates: Partial<ParagraphStyles>) => {
    onUpdate({ ...styles, ...updates });
  };

  return (
    <div className="space-y-4">
      <label className="text-xs font-semibold" style={{ color: colors.primary }}>
        PARAGRAPH & LINE SPACING
      </label>

      {/* Line Height */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">Line Height</label>
          <span className="text-xs text-[#FFA500]">{styles.lineHeight.toFixed(2)}</span>
        </div>
        <input
          type="range"
          min="1"
          max="3"
          step="0.1"
          value={styles.lineHeight}
          onChange={(e) => updateStyles({ lineHeight: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
        <div className="flex justify-between text-[10px] text-gray-500">
          <span>Tight (1.0)</span>
          <span>Normal (1.5)</span>
          <span>Loose (3.0)</span>
        </div>
      </div>

      {/* Paragraph Spacing */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">Paragraph Spacing</label>
          <span className="text-xs text-[#FFA500]">{styles.paragraphSpacing}px</span>
        </div>
        <input
          type="range"
          min="0"
          max="48"
          step="4"
          value={styles.paragraphSpacing}
          onChange={(e) => updateStyles({ paragraphSpacing: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
      </div>

      {/* Text Indent */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">First Line Indent</label>
          <span className="text-xs text-[#FFA500]">{styles.textIndent}px</span>
        </div>
        <input
          type="range"
          min="0"
          max="50"
          step="5"
          value={styles.textIndent}
          onChange={(e) => updateStyles({ textIndent: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
      </div>

      {/* Letter Spacing */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">Letter Spacing</label>
          <span className="text-xs text-[#FFA500]">{styles.letterSpacing}px</span>
        </div>
        <input
          type="range"
          min="-2"
          max="10"
          step="0.5"
          value={styles.letterSpacing}
          onChange={(e) => updateStyles({ letterSpacing: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
      </div>

      {/* Word Spacing */}
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <label className="text-xs text-gray-300">Word Spacing</label>
          <span className="text-xs text-[#FFA500]">{styles.wordSpacing}px</span>
        </div>
        <input
          type="range"
          min="0"
          max="20"
          step="1"
          value={styles.wordSpacing}
          onChange={(e) => updateStyles({ wordSpacing: Number(e.target.value) })}
          className="w-full"
          style={{ accentColor: colors.primary }}
        />
      </div>

      {/* Text Alignment */}
      <div className="space-y-2">
        <label className="text-xs text-gray-300">Default Text Alignment</label>
        <div className="flex gap-2">
          {([
            { value: 'left', icon: AlignLeft },
            { value: 'center', icon: AlignCenter },
            { value: 'right', icon: AlignRight },
            { value: 'justify', icon: AlignJustify },
          ] as const).map(({ value, icon: Icon }) => (
            <button
              key={value}
              onClick={() => updateStyles({ textAlign: value })}
              className={`flex-1 py-2 rounded transition-colors ${
                styles.textAlign === value
                  ? 'bg-[#FFA500] text-black'
                  : 'bg-[#0a0a0a] text-gray-400 hover:bg-[#2a2a2a]'
              }`}
              title={value.charAt(0).toUpperCase() + value.slice(1)}
            >
              <Icon size={14} className="mx-auto" />
            </button>
          ))}
        </div>
      </div>

      {/* Preview */}
      <div className="space-y-2">
        <label className="text-[10px] text-gray-500">Preview</label>
        <div className="bg-white p-4 rounded text-black">
          <p style={{
            lineHeight: styles.lineHeight,
            marginBottom: `${styles.paragraphSpacing}px`,
            textIndent: `${styles.textIndent}px`,
            letterSpacing: `${styles.letterSpacing}px`,
            wordSpacing: `${styles.wordSpacing}px`,
            textAlign: styles.textAlign,
          }}>
            This is a sample paragraph demonstrating the typography settings. Notice how the line height,
            paragraph spacing, and text alignment affect the overall readability and appearance of the text.
          </p>
          <p style={{
            lineHeight: styles.lineHeight,
            marginBottom: `${styles.paragraphSpacing}px`,
            textIndent: `${styles.textIndent}px`,
            letterSpacing: `${styles.letterSpacing}px`,
            wordSpacing: `${styles.wordSpacing}px`,
            textAlign: styles.textAlign,
          }}>
            A second paragraph shows the spacing between paragraphs. Professional reports often use
            consistent typography to maintain visual coherence throughout the document.
          </p>
        </div>
      </div>

      {/* Presets */}
      <div className="space-y-2">
        <label className="text-xs text-gray-500">Quick Presets</label>
        <div className="grid grid-cols-2 gap-2">
          <button
            onClick={() => updateStyles({
              lineHeight: 1.5,
              paragraphSpacing: 12,
              textIndent: 0,
              letterSpacing: 0,
              wordSpacing: 0,
              textAlign: 'left',
            })}
            className="px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Default
          </button>
          <button
            onClick={() => updateStyles({
              lineHeight: 2.0,
              paragraphSpacing: 16,
              textIndent: 20,
              letterSpacing: 0,
              wordSpacing: 0,
              textAlign: 'justify',
            })}
            className="px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Academic
          </button>
          <button
            onClick={() => updateStyles({
              lineHeight: 1.4,
              paragraphSpacing: 8,
              textIndent: 0,
              letterSpacing: 0.5,
              wordSpacing: 0,
              textAlign: 'left',
            })}
            className="px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Compact
          </button>
          <button
            onClick={() => updateStyles({
              lineHeight: 1.8,
              paragraphSpacing: 20,
              textIndent: 0,
              letterSpacing: 0,
              wordSpacing: 2,
              textAlign: 'left',
            })}
            className="px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Relaxed
          </button>
        </div>
      </div>
    </div>
  );
};

// ============ Custom CSS Tab ============
interface CustomCSSTabProps {
  brandKit: BrandKit | null;
  onUpdate: (css: string) => void;
}

const CustomCSSTab: React.FC<CustomCSSTabProps> = ({ brandKit, onUpdate }) => {
  const { colors } = useTerminalTheme();
  const [localCSS, setLocalCSS] = useState(brandKit?.customCSS || '');
  const [showGenerated, setShowGenerated] = useState(false);

  useEffect(() => {
    setLocalCSS(brandKit?.customCSS || '');
  }, [brandKit]);

  if (!brandKit) {
    return (
      <div className="text-center py-8 text-gray-500 text-xs">
        Select a brand kit first
      </div>
    );
  }

  const generatedCSS = brandKitService.generateCSSFromBrandKit(brandKit);

  const handleApply = () => {
    onUpdate(localCSS);
    toast.success('Custom CSS applied');
  };

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <label className="text-xs font-semibold" style={{ color: colors.primary }}>
          CUSTOM CSS
        </label>
        <button
          onClick={() => setShowGenerated(!showGenerated)}
          className="text-[10px] px-2 py-1 rounded bg-[#0a0a0a] hover:bg-[#2a2a2a] text-gray-400"
        >
          {showGenerated ? 'Hide Generated' : 'Show Generated'}
        </button>
      </div>

      {showGenerated && (
        <div className="space-y-2">
          <label className="text-[10px] text-gray-500">Generated from Brand Kit (read-only)</label>
          <pre className="p-3 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] text-green-400 overflow-x-auto max-h-48 overflow-y-auto">
            {generatedCSS}
          </pre>
          <button
            onClick={() => {
              navigator.clipboard.writeText(generatedCSS);
              toast.success('CSS copied to clipboard');
            }}
            className="text-[10px] px-2 py-1 rounded bg-[#0a0a0a] hover:bg-[#2a2a2a] text-gray-400 flex items-center gap-1"
          >
            <Copy size={12} />
            Copy Generated CSS
          </button>
        </div>
      )}

      {/* Custom CSS Editor */}
      <div className="space-y-2">
        <label className="text-[10px] text-gray-500">Additional Custom CSS</label>
        <textarea
          value={localCSS}
          onChange={(e) => setLocalCSS(e.target.value)}
          className="w-full h-64 px-3 py-2 text-xs font-mono rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none text-green-400 resize-none"
          placeholder="/* Add your custom CSS here */&#10;&#10;.report-title {&#10;  color: #FFA500;&#10;  font-weight: bold;&#10;}"
          spellCheck={false}
        />
      </div>

      {/* CSS Snippets */}
      <div className="space-y-2">
        <label className="text-[10px] text-gray-500">Quick Snippets</label>
        <div className="grid grid-cols-2 gap-2">
          <button
            onClick={() => setLocalCSS(prev => prev + '\n\n/* Highlight Box */\n.highlight {\n  background: #FFF8E1;\n  border-left: 4px solid #FFA500;\n  padding: 15px;\n  margin: 10px 0;\n}')}
            className="px-2 py-2 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Highlight Box
          </button>
          <button
            onClick={() => setLocalCSS(prev => prev + '\n\n/* Shadow Card */\n.shadow-card {\n  box-shadow: 0 4px 6px rgba(0,0,0,0.1);\n  border-radius: 8px;\n  padding: 20px;\n}')}
            className="px-2 py-2 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Shadow Card
          </button>
          <button
            onClick={() => setLocalCSS(prev => prev + '\n\n/* Striped Table */\ntable tr:nth-child(even) {\n  background-color: #f8f9fa;\n}')}
            className="px-2 py-2 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Striped Table
          </button>
          <button
            onClick={() => setLocalCSS(prev => prev + '\n\n/* Print Styles */\n@media print {\n  .no-print { display: none; }\n  body { font-size: 12pt; }\n}')}
            className="px-2 py-2 text-[10px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors text-gray-300"
          >
            Print Styles
          </button>
        </div>
      </div>

      {/* Apply Button */}
      <button
        onClick={handleApply}
        className="w-full py-2 text-xs font-semibold rounded transition-colors flex items-center justify-center gap-2"
        style={{ backgroundColor: colors.primary, color: '#000' }}
      >
        <Check size={14} />
        Apply Custom CSS
      </button>

      {/* Help Text */}
      <div className="p-3 bg-[#0a0a0a] rounded border border-[#333333]">
        <p className="text-[10px] text-gray-500">
          Custom CSS will be applied to the generated PDF. Use standard CSS properties.
          The generated CSS from your brand kit settings is automatically included.
        </p>
      </div>
    </div>
  );
};

export default AdvancedStylingPanel;
