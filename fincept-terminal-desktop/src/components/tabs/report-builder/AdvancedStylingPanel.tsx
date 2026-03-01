import React, { useState, useEffect } from 'react';
import {
  Palette,
  Type,
  Code,
  Layers,
  LayoutTemplate,
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
import { BrandKitTab, ComponentTemplatesTab } from './AdvancedStylingPanelBrandKit';
import { HeaderFooterTab, TypographyTab, CustomCSSTab } from './AdvancedStylingPanelStyles';

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

export default AdvancedStylingPanel;
