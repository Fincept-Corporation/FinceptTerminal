import { ReportTemplate, ReportComponent } from '@/services/core/reportService';
import { BrandKit, HeaderFooterTemplate, ParagraphStyles } from '@/services/core/brandKitService';

// Page theme type
export type PageTheme = 'classic' | 'modern' | 'geometric' | 'gradient' | 'minimal' | 'pattern';

// Right panel view options
export type RightPanelView = 'split' | 'properties' | 'chat' | 'styling' | 'comments' | 'history';

// AI Message interface
export interface AIMessage {
  role: string;
  content: string;
}

// AI Model info interface
export interface AIModel {
  id: string;
  name: string;
  provider: string;
}

// Advanced styles interface
export interface AdvancedStyles {
  customCSS: string;
  headerTemplate: HeaderFooterTemplate | null;
  footerTemplate: HeaderFooterTemplate | null;
  paragraphStyles: ParagraphStyles | null;
}

// Table of Contents item interface
export interface TOCItem {
  id: string;
  title: string;
  level: number;
  page: number;
}

// Report builder state interface
export interface ReportBuilderState {
  template: ReportTemplate;
  selectedComponent: string | null;
  isGenerating: boolean;
  isSaving: boolean;
  showSuccessDialog: boolean;
  generatedPdfPath: string;
  showExportDialog: boolean;
  pageTheme: PageTheme;
  fontFamily: string;
  defaultFontSize: number;
  isBold: boolean;
  isItalic: boolean;
  customBgColor: string;
  showColorPicker: boolean;
  rightPanelView: RightPanelView;
  showTemplateGallery: boolean;
  showFindReplace: boolean;
  showShortcuts: boolean;
  revisionModeEnabled: boolean;
  activeBrandKit: BrandKit | null;
  advancedStyles: AdvancedStyles;
}

// Component props interfaces
export interface SortableComponentProps {
  component: ReportComponent;
  isSelected: boolean;
  onClick: () => void;
  onDelete: () => void;
  onDuplicate: () => void;
}

export interface ComponentToolbarProps {
  onAddComponent: (type: ReportComponent['type']) => void;
  template: ReportTemplate;
  selectedComponent: string | null;
  onSelectComponent: (id: string | null) => void;
  onDeleteComponent: (id: string) => void;
  onDuplicateComponent: (id: string) => void;
  onDragEnd: (event: any) => void;
  pageTheme: PageTheme;
  onPageThemeChange: (theme: PageTheme) => void;
  customBgColor: string;
  onCustomBgColorChange: (color: string) => void;
  showColorPicker: boolean;
  onShowColorPickerChange: (show: boolean) => void;
  fontFamily: string;
  onFontFamilyChange: (font: string) => void;
  defaultFontSize: number;
  onDefaultFontSizeChange: (size: number) => void;
  isBold: boolean;
  onIsBoldChange: (bold: boolean) => void;
  isItalic: boolean;
  onIsItalicChange: (italic: boolean) => void;
}

export interface DocumentCanvasProps {
  template: ReportTemplate;
  selectedComponent: string | null;
  onSelectComponent: (id: string | null) => void;
  pageTheme: PageTheme;
  customBgColor: string;
  fontFamily: string;
  defaultFontSize: number;
  isBold: boolean;
  isItalic: boolean;
  advancedStyles: AdvancedStyles;
  tocItems: TOCItem[];
}

export interface PropertiesPanelProps {
  selectedComponent: ReportComponent | undefined;
  onUpdateComponent: (id: string, updates: Partial<ReportComponent>) => void;
  onDeleteComponent: (id: string) => void;
  onDuplicateComponent: (id: string) => void;
  onClearSelection: () => void;
  onSaveAsTemplate: () => void;
  onImageUpload: () => void;
}

export interface AIChatPanelProps {
  messages: AIMessage[];
  input: string;
  onInputChange: (value: string) => void;
  onSendMessage: () => void;
  onQuickAction: (action: string) => void;
  isTyping: boolean;
  streamingContent: string;
  currentProvider: string;
  onProviderChange: (provider: string) => void;
  availableModels: AIModel[];
}

export interface ReportHeaderProps {
  canUndo: boolean;
  canRedo: boolean;
  onUndo: () => void;
  onRedo: () => void;
  onFindReplace: () => void;
  onShortcuts: () => void;
  onTemplateGallery: () => void;
  onLoad: () => void;
  onSave: () => void;
  onExport: () => void;
  isSaving: boolean;
  isAutoSaving: boolean;
  lastSaved: Date | null;
  hasComponents: boolean;
}

// Theme background style result
export interface ThemeBackgroundStyle {
  backgroundColor?: string;
  background?: string;
  backgroundSize?: string;
  backgroundPosition?: string;
  backgroundImage?: string;
  color: string;
}
