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
  onUpdateComponent: (id: string, updates: Partial<ReportComponent>) => void;
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
  // DOCX mode props
  builderMode: ReportBuilderMode;
  onModeChange: (mode: ReportBuilderMode) => void;
  onImportDocx: () => void;
  onExportDocx?: () => void;
  docxFileName?: string;
  isDocxDirty?: boolean;
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

// ---- DOCX Editor Types ----

// Editor mode for the Report Builder
export type ReportBuilderMode = 'builder' | 'docx';

// DOCX editor view states
export type DocxViewMode = 'edit' | 'preview' | 'split';

// DOCX editor state
export interface DocxEditorState {
  viewMode: DocxViewMode;
  filePath: string | null;
  fileName: string;
  originalArrayBuffer: ArrayBuffer | null;
  htmlContent: string;
  isDirty: boolean;
  fileId: string | null;
  metadata: DocxDocumentMetadata;
}

// DOCX document metadata
export interface DocxDocumentMetadata {
  title: string;
  author: string;
  company: string;
  date: string;
}

// DOCX Editor Toolbar props
export interface DocxEditorToolbarProps {
  editor: any; // TipTap Editor instance
  viewMode: DocxViewMode;
  onViewModeChange: (mode: DocxViewMode) => void;
  onInsertImage: () => void;
  // Watermark
  onToggleWatermark?: () => void;
  watermarkEnabled?: boolean;
  // Header / Footer
  onToggleHeader?: () => void;
  onToggleFooter?: () => void;
  headerEnabled?: boolean;
  footerEnabled?: boolean;
  // Line spacing
  lineSpacing?: string;
  onLineSpacingChange?: (spacing: string) => void;
  // Zoom
  zoom: number;
  onZoomChange: (zoom: number) => void;
}

// DOCX Editor Canvas props
export interface DocxEditorCanvasProps {
  editor: any; // TipTap Editor instance
  viewMode: DocxViewMode;
  originalArrayBuffer: ArrayBuffer | null;
  // Watermark
  watermarkText?: string;
  watermarkEnabled?: boolean;
  // Header / Footer
  headerText?: string;
  footerText?: string;
  headerEnabled?: boolean;
  footerEnabled?: boolean;
  onToggleHeader?: () => void;
  onToggleFooter?: () => void;
  onHeaderTextChange?: (text: string) => void;
  onFooterTextChange?: (text: string) => void;
  // Line spacing
  lineSpacing?: string;
  // Page margins
  margins?: { top: number; right: number; bottom: number; left: number };
  // Zoom
  zoom?: number;
}

// DOCX page layout settings
export interface DocxPageSettings {
  watermarkEnabled: boolean;
  watermarkText: string;
  headerEnabled: boolean;
  footerEnabled: boolean;
  headerText: string;
  footerText: string;
  lineSpacing: string;
  margins: { top: number; right: number; bottom: number; left: number };
}

// DOCX Properties Panel props
export interface DocxPropertiesPanelProps {
  metadata: DocxDocumentMetadata;
  onMetadataChange: (updates: Partial<DocxDocumentMetadata>) => void;
  htmlContent: string;
  filePath: string | null;
  fileId: string | null;
  isDirty: boolean;
  onCreateSnapshot: () => void;
  onRestoreSnapshot: (snapshotId: string) => void;
  snapshots: Array<{ id: string; snapshot_name: string; created_at: string }>;
  onDeleteSnapshot: (snapshotId: string) => void;
  // Page settings
  pageSettings: DocxPageSettings;
  onPageSettingsChange: (updates: Partial<DocxPageSettings>) => void;
}
