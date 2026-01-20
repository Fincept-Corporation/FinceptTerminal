import { sqliteService, saveSetting, getSetting } from './sqliteService';

export interface BrandColor {
  id: string;
  name: string;
  hex: string;
  usage: 'primary' | 'secondary' | 'accent' | 'background' | 'text' | 'custom';
}

export interface BrandFont {
  id: string;
  name: string;
  family: string;
  weight: string;
  usage: 'heading' | 'body' | 'caption' | 'custom';
}

export interface BrandLogo {
  id: string;
  name: string;
  path: string;
  type: 'primary' | 'secondary' | 'icon' | 'watermark';
  width?: number;
  height?: number;
}

export interface BrandKit {
  id: string;
  name: string;
  description?: string;
  colors: BrandColor[];
  fonts: BrandFont[];
  logos: BrandLogo[];
  customCSS?: string;
  headerTemplate?: HeaderFooterTemplate;
  footerTemplate?: HeaderFooterTemplate;
  paragraphStyles: ParagraphStyles;
  created_at: string;
  updated_at: string;
  isDefault?: boolean;
}

export interface HeaderFooterTemplate {
  enabled: boolean;
  content: string;
  alignment: 'left' | 'center' | 'right';
  showPageNumbers: boolean;
  pageNumberFormat: 'numeric' | 'roman' | 'alphanumeric';
  pageNumberPosition: 'left' | 'center' | 'right';
  backgroundColor?: string;
  textColor?: string;
  fontSize?: number;
  height?: number;
  showLogo?: boolean;
  logoPosition?: 'left' | 'center' | 'right';
  showDate?: boolean;
  dateFormat?: string;
  borderBottom?: boolean;
  borderTop?: boolean;
}

export interface ParagraphStyles {
  lineHeight: number; // e.g., 1.5, 1.75, 2.0
  paragraphSpacing: number; // in px
  textIndent: number; // in px
  letterSpacing: number; // in px
  wordSpacing: number; // in px
  textAlign: 'left' | 'center' | 'right' | 'justify';
}

export interface ComponentTemplate {
  id: string;
  name: string;
  description?: string;
  category: 'text' | 'layout' | 'data' | 'media' | 'custom';
  component: {
    type: string;
    content: any;
    config: Record<string, any>;
    children?: any[];
  };
  styles?: string; // Custom CSS for this component
  thumbnail?: string;
  created_at: string;
  updated_at: string;
}

const BRAND_KITS_KEY = 'report_brand_kits';
const COMPONENT_TEMPLATES_KEY = 'report_component_templates';
const ACTIVE_BRAND_KIT_KEY = 'active_brand_kit_id';

class BrandKitService {
  private initialized = false;

  async initialize(): Promise<void> {
    if (this.initialized) return;
    await sqliteService.initialize();
    this.initialized = true;
  }

  // ============ Brand Kit Methods ============

  getDefaultBrandKit(): BrandKit {
    return {
      id: `brand-kit-${Date.now()}`,
      name: 'Default Brand Kit',
      description: 'Default styling for reports',
      colors: [
        { id: 'color-1', name: 'Primary', hex: '#FFA500', usage: 'primary' },
        { id: 'color-2', name: 'Secondary', hex: '#333333', usage: 'secondary' },
        { id: 'color-3', name: 'Accent', hex: '#4A90D9', usage: 'accent' },
        { id: 'color-4', name: 'Background', hex: '#FFFFFF', usage: 'background' },
        { id: 'color-5', name: 'Text', hex: '#1A1A1A', usage: 'text' },
      ],
      fonts: [
        { id: 'font-1', name: 'Heading Font', family: 'Georgia, serif', weight: '700', usage: 'heading' },
        { id: 'font-2', name: 'Body Font', family: 'Arial, sans-serif', weight: '400', usage: 'body' },
        { id: 'font-3', name: 'Caption Font', family: 'Arial, sans-serif', weight: '300', usage: 'caption' },
      ],
      logos: [],
      customCSS: '',
      headerTemplate: {
        enabled: true,
        content: '{company}',
        alignment: 'left',
        showPageNumbers: true,
        pageNumberFormat: 'numeric',
        pageNumberPosition: 'right',
        backgroundColor: '#FFFFFF',
        textColor: '#666666',
        fontSize: 10,
        height: 30,
        showLogo: false,
        showDate: true,
        dateFormat: 'MMMM DD, YYYY',
        borderBottom: true,
      },
      footerTemplate: {
        enabled: true,
        content: 'Confidential - {company}',
        alignment: 'center',
        showPageNumbers: true,
        pageNumberFormat: 'numeric',
        pageNumberPosition: 'center',
        backgroundColor: '#FFFFFF',
        textColor: '#999999',
        fontSize: 9,
        height: 25,
        showDate: false,
        borderTop: true,
      },
      paragraphStyles: {
        lineHeight: 1.6,
        paragraphSpacing: 12,
        textIndent: 0,
        letterSpacing: 0,
        wordSpacing: 0,
        textAlign: 'left',
      },
      created_at: new Date().toISOString(),
      updated_at: new Date().toISOString(),
      isDefault: true,
    };
  }

  async saveBrandKit(brandKit: BrandKit): Promise<void> {
    await this.initialize();
    const brandKits = await this.getAllBrandKits();
    const existingIndex = brandKits.findIndex(bk => bk.id === brandKit.id);

    brandKit.updated_at = new Date().toISOString();

    if (existingIndex >= 0) {
      brandKits[existingIndex] = brandKit;
    } else {
      brandKit.created_at = new Date().toISOString();
      brandKits.push(brandKit);
    }

    await saveSetting(BRAND_KITS_KEY, JSON.stringify(brandKits), 'reports');
  }

  async getAllBrandKits(): Promise<BrandKit[]> {
    await this.initialize();
    const stored = await getSetting(BRAND_KITS_KEY);
    if (!stored) {
      // Create default brand kit if none exists
      const defaultKit = this.getDefaultBrandKit();
      await this.saveBrandKit(defaultKit);
      return [defaultKit];
    }
    return JSON.parse(stored);
  }

  async getBrandKit(id: string): Promise<BrandKit | null> {
    const brandKits = await this.getAllBrandKits();
    return brandKits.find(bk => bk.id === id) || null;
  }

  async deleteBrandKit(id: string): Promise<void> {
    await this.initialize();
    const brandKits = await this.getAllBrandKits();
    const filtered = brandKits.filter(bk => bk.id !== id);
    await saveSetting(BRAND_KITS_KEY, JSON.stringify(filtered), 'reports');
  }

  async setActiveBrandKit(id: string): Promise<void> {
    await this.initialize();
    await saveSetting(ACTIVE_BRAND_KIT_KEY, id, 'reports');
  }

  async getActiveBrandKit(): Promise<BrandKit | null> {
    await this.initialize();
    const activeId = await getSetting(ACTIVE_BRAND_KIT_KEY);
    if (activeId) {
      return await this.getBrandKit(activeId);
    }
    // Return first brand kit or default
    const brandKits = await this.getAllBrandKits();
    return brandKits[0] || this.getDefaultBrandKit();
  }

  // ============ Component Template Methods ============

  async saveComponentTemplate(template: ComponentTemplate): Promise<void> {
    await this.initialize();
    const templates = await this.getAllComponentTemplates();
    const existingIndex = templates.findIndex(t => t.id === template.id);

    template.updated_at = new Date().toISOString();

    if (existingIndex >= 0) {
      templates[existingIndex] = template;
    } else {
      template.created_at = new Date().toISOString();
      templates.push(template);
    }

    await saveSetting(COMPONENT_TEMPLATES_KEY, JSON.stringify(templates), 'reports');
  }

  async getAllComponentTemplates(): Promise<ComponentTemplate[]> {
    await this.initialize();
    const stored = await getSetting(COMPONENT_TEMPLATES_KEY);
    if (!stored) {
      // Create default component templates
      const defaultTemplates = this.getDefaultComponentTemplates();
      for (const template of defaultTemplates) {
        await this.saveComponentTemplate(template);
      }
      return defaultTemplates;
    }
    return JSON.parse(stored);
  }

  async getComponentTemplate(id: string): Promise<ComponentTemplate | null> {
    const templates = await this.getAllComponentTemplates();
    return templates.find(t => t.id === id) || null;
  }

  async getComponentTemplatesByCategory(category: ComponentTemplate['category']): Promise<ComponentTemplate[]> {
    const templates = await this.getAllComponentTemplates();
    return templates.filter(t => t.category === category);
  }

  async deleteComponentTemplate(id: string): Promise<void> {
    await this.initialize();
    const templates = await this.getAllComponentTemplates();
    const filtered = templates.filter(t => t.id !== id);
    await saveSetting(COMPONENT_TEMPLATES_KEY, JSON.stringify(filtered), 'reports');
  }

  getDefaultComponentTemplates(): ComponentTemplate[] {
    return [
      {
        id: 'template-callout-info',
        name: 'Info Callout',
        description: 'Blue information callout box',
        category: 'text',
        component: {
          type: 'section',
          content: null,
          config: {
            sectionTitle: 'Note',
            sectionStyle: 'highlight',
            backgroundColor: '#EBF5FB',
            borderColor: '#3498DB',
            padding: '15px',
          },
        },
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      },
      {
        id: 'template-callout-warning',
        name: 'Warning Callout',
        description: 'Yellow warning callout box',
        category: 'text',
        component: {
          type: 'section',
          content: null,
          config: {
            sectionTitle: 'Warning',
            sectionStyle: 'highlight',
            backgroundColor: '#FEF9E7',
            borderColor: '#F1C40F',
            padding: '15px',
          },
        },
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      },
      {
        id: 'template-callout-success',
        name: 'Success Callout',
        description: 'Green success callout box',
        category: 'text',
        component: {
          type: 'section',
          content: null,
          config: {
            sectionTitle: 'Success',
            sectionStyle: 'highlight',
            backgroundColor: '#E8F8F5',
            borderColor: '#27AE60',
            padding: '15px',
          },
        },
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      },
      {
        id: 'template-callout-danger',
        name: 'Danger Callout',
        description: 'Red danger/error callout box',
        category: 'text',
        component: {
          type: 'section',
          content: null,
          config: {
            sectionTitle: 'Important',
            sectionStyle: 'highlight',
            backgroundColor: '#FDEDEC',
            borderColor: '#E74C3C',
            padding: '15px',
          },
        },
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      },
      {
        id: 'template-two-column-comparison',
        name: 'Two Column Comparison',
        description: 'Side by side comparison layout',
        category: 'layout',
        component: {
          type: 'columns',
          content: null,
          config: {
            columnCount: 2,
            padding: '20px',
          },
          children: [],
        },
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      },
      {
        id: 'template-three-column-metrics',
        name: 'Three Column Metrics',
        description: 'Three KPI metrics side by side',
        category: 'layout',
        component: {
          type: 'columns',
          content: null,
          config: {
            columnCount: 3,
            padding: '15px',
          },
          children: [],
        },
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      },
      {
        id: 'template-quote-block',
        name: 'Quote Block',
        description: 'Styled quote with attribution',
        category: 'text',
        component: {
          type: 'section',
          content: null,
          config: {
            sectionTitle: '',
            sectionStyle: 'sidebar',
            backgroundColor: '#F8F9FA',
            borderColor: '#6C757D',
            padding: '20px',
          },
        },
        styles: `
          font-style: italic;
          border-left: 4px solid #6C757D;
          margin: 20px 40px;
        `,
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      },
      {
        id: 'template-highlight-box',
        name: 'Highlight Box',
        description: 'Emphasized content box with orange accent',
        category: 'text',
        component: {
          type: 'section',
          content: null,
          config: {
            sectionTitle: 'Key Insight',
            sectionStyle: 'highlight',
            backgroundColor: '#FFF8E1',
            borderColor: '#FFA500',
            padding: '20px',
          },
        },
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      },
    ];
  }

  // ============ CSS Generation Methods ============

  generateCSSFromBrandKit(brandKit: BrandKit): string {
    const { colors, fonts, paragraphStyles, customCSS } = brandKit;

    let css = `/* Brand Kit: ${brandKit.name} */\n\n`;

    // CSS Variables for colors
    css += `:root {\n`;
    colors.forEach(color => {
      css += `  --brand-${color.usage}: ${color.hex};\n`;
    });
    css += `}\n\n`;

    // Typography
    const headingFont = fonts.find(f => f.usage === 'heading');
    const bodyFont = fonts.find(f => f.usage === 'body');
    const captionFont = fonts.find(f => f.usage === 'caption');

    if (headingFont) {
      css += `h1, h2, h3, h4, h5, h6, .heading {\n`;
      css += `  font-family: ${headingFont.family};\n`;
      css += `  font-weight: ${headingFont.weight};\n`;
      css += `}\n\n`;
    }

    if (bodyFont) {
      css += `body, p, .body-text {\n`;
      css += `  font-family: ${bodyFont.family};\n`;
      css += `  font-weight: ${bodyFont.weight};\n`;
      css += `}\n\n`;
    }

    if (captionFont) {
      css += `.caption, figcaption, .small-text {\n`;
      css += `  font-family: ${captionFont.family};\n`;
      css += `  font-weight: ${captionFont.weight};\n`;
      css += `}\n\n`;
    }

    // Paragraph styles
    css += `p, .paragraph {\n`;
    css += `  line-height: ${paragraphStyles.lineHeight};\n`;
    css += `  margin-bottom: ${paragraphStyles.paragraphSpacing}px;\n`;
    css += `  text-indent: ${paragraphStyles.textIndent}px;\n`;
    css += `  letter-spacing: ${paragraphStyles.letterSpacing}px;\n`;
    css += `  word-spacing: ${paragraphStyles.wordSpacing}px;\n`;
    css += `  text-align: ${paragraphStyles.textAlign};\n`;
    css += `}\n\n`;

    // Custom CSS
    if (customCSS) {
      css += `/* Custom CSS */\n${customCSS}\n`;
    }

    return css;
  }

  generateHeaderFooterCSS(template: HeaderFooterTemplate, type: 'header' | 'footer'): string {
    if (!template.enabled) return '';

    let css = `.report-${type} {\n`;
    css += `  background-color: ${template.backgroundColor || '#FFFFFF'};\n`;
    css += `  color: ${template.textColor || '#666666'};\n`;
    css += `  font-size: ${template.fontSize || 10}pt;\n`;
    css += `  height: ${template.height || 30}px;\n`;
    css += `  text-align: ${template.alignment};\n`;
    css += `  padding: 5px 20px;\n`;
    css += `  display: flex;\n`;
    css += `  align-items: center;\n`;
    css += `  justify-content: space-between;\n`;

    if (type === 'header' && template.borderBottom) {
      css += `  border-bottom: 1px solid #E0E0E0;\n`;
    }
    if (type === 'footer' && template.borderTop) {
      css += `  border-top: 1px solid #E0E0E0;\n`;
    }

    css += `}\n`;

    return css;
  }

  // ============ Utility Methods ============

  hexToRgb(hex: string): { r: number; g: number; b: number } | null {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : null;
  }

  rgbToHex(r: number, g: number, b: number): string {
    return '#' + [r, g, b].map(x => {
      const hex = x.toString(16);
      return hex.length === 1 ? '0' + hex : hex;
    }).join('');
  }

  // Common font families for selection
  getAvailableFonts(): { name: string; family: string }[] {
    return [
      { name: 'Arial', family: 'Arial, sans-serif' },
      { name: 'Helvetica', family: 'Helvetica, Arial, sans-serif' },
      { name: 'Times New Roman', family: '"Times New Roman", Times, serif' },
      { name: 'Georgia', family: 'Georgia, serif' },
      { name: 'Verdana', family: 'Verdana, sans-serif' },
      { name: 'Trebuchet MS', family: '"Trebuchet MS", sans-serif' },
      { name: 'Palatino', family: '"Palatino Linotype", Palatino, serif' },
      { name: 'Garamond', family: 'Garamond, serif' },
      { name: 'Courier New', family: '"Courier New", Courier, monospace' },
      { name: 'Lucida Console', family: '"Lucida Console", Monaco, monospace' },
      { name: 'Impact', family: 'Impact, sans-serif' },
      { name: 'Comic Sans MS', family: '"Comic Sans MS", cursive' },
      { name: 'Roboto', family: 'Roboto, sans-serif' },
      { name: 'Open Sans', family: '"Open Sans", sans-serif' },
      { name: 'Lato', family: 'Lato, sans-serif' },
      { name: 'Montserrat', family: 'Montserrat, sans-serif' },
      { name: 'Source Sans Pro', family: '"Source Sans Pro", sans-serif' },
      { name: 'Playfair Display', family: '"Playfair Display", serif' },
      { name: 'Merriweather', family: 'Merriweather, serif' },
      { name: 'Inter', family: 'Inter, sans-serif' },
    ];
  }

  // Preset brand kits for quick selection
  getPresetBrandKits(): Partial<BrandKit>[] {
    return [
      {
        name: 'Corporate Blue',
        colors: [
          { id: 'c1', name: 'Primary', hex: '#1E3A8A', usage: 'primary' },
          { id: 'c2', name: 'Secondary', hex: '#3B82F6', usage: 'secondary' },
          { id: 'c3', name: 'Accent', hex: '#FBBF24', usage: 'accent' },
          { id: 'c4', name: 'Background', hex: '#F8FAFC', usage: 'background' },
          { id: 'c5', name: 'Text', hex: '#1E293B', usage: 'text' },
        ],
        fonts: [
          { id: 'f1', name: 'Heading', family: '"Roboto", sans-serif', weight: '700', usage: 'heading' },
          { id: 'f2', name: 'Body', family: '"Open Sans", sans-serif', weight: '400', usage: 'body' },
        ],
      },
      {
        name: 'Modern Green',
        colors: [
          { id: 'c1', name: 'Primary', hex: '#059669', usage: 'primary' },
          { id: 'c2', name: 'Secondary', hex: '#10B981', usage: 'secondary' },
          { id: 'c3', name: 'Accent', hex: '#F59E0B', usage: 'accent' },
          { id: 'c4', name: 'Background', hex: '#ECFDF5', usage: 'background' },
          { id: 'c5', name: 'Text', hex: '#064E3B', usage: 'text' },
        ],
        fonts: [
          { id: 'f1', name: 'Heading', family: '"Montserrat", sans-serif', weight: '600', usage: 'heading' },
          { id: 'f2', name: 'Body', family: '"Lato", sans-serif', weight: '400', usage: 'body' },
        ],
      },
      {
        name: 'Classic Serif',
        colors: [
          { id: 'c1', name: 'Primary', hex: '#7C3AED', usage: 'primary' },
          { id: 'c2', name: 'Secondary', hex: '#A78BFA', usage: 'secondary' },
          { id: 'c3', name: 'Accent', hex: '#F472B6', usage: 'accent' },
          { id: 'c4', name: 'Background', hex: '#FFFBEB', usage: 'background' },
          { id: 'c5', name: 'Text', hex: '#44403C', usage: 'text' },
        ],
        fonts: [
          { id: 'f1', name: 'Heading', family: '"Playfair Display", serif', weight: '700', usage: 'heading' },
          { id: 'f2', name: 'Body', family: '"Merriweather", serif', weight: '400', usage: 'body' },
        ],
      },
      {
        name: 'Minimal Dark',
        colors: [
          { id: 'c1', name: 'Primary', hex: '#FFFFFF', usage: 'primary' },
          { id: 'c2', name: 'Secondary', hex: '#A3A3A3', usage: 'secondary' },
          { id: 'c3', name: 'Accent', hex: '#FFA500', usage: 'accent' },
          { id: 'c4', name: 'Background', hex: '#171717', usage: 'background' },
          { id: 'c5', name: 'Text', hex: '#FAFAFA', usage: 'text' },
        ],
        fonts: [
          { id: 'f1', name: 'Heading', family: '"Inter", sans-serif', weight: '600', usage: 'heading' },
          { id: 'f2', name: 'Body', family: '"Inter", sans-serif', weight: '400', usage: 'body' },
        ],
      },
      {
        name: 'Financial Pro',
        colors: [
          { id: 'c1', name: 'Primary', hex: '#0F172A', usage: 'primary' },
          { id: 'c2', name: 'Secondary', hex: '#334155', usage: 'secondary' },
          { id: 'c3', name: 'Accent', hex: '#22C55E', usage: 'accent' },
          { id: 'c4', name: 'Background', hex: '#FFFFFF', usage: 'background' },
          { id: 'c5', name: 'Text', hex: '#0F172A', usage: 'text' },
        ],
        fonts: [
          { id: 'f1', name: 'Heading', family: 'Georgia, serif', weight: '700', usage: 'heading' },
          { id: 'f2', name: 'Body', family: 'Arial, sans-serif', weight: '400', usage: 'body' },
        ],
      },
    ];
  }
}

export const brandKitService = new BrandKitService();
