// Main component
export { default as ReportBuilderTab } from './ReportBuilderTab';
export { default } from './ReportBuilderTab';

// Sub-components
export { ReportHeader } from './ReportHeader';
export { ComponentToolbar } from './ComponentToolbar';
export { DocumentCanvas, getThemeBackground } from './DocumentCanvas';
export { PropertiesPanel } from './PropertiesPanel';
export { AIChatPanel } from './AIChatPanel';
export { SortableComponent } from './SortableComponent';

// Existing components (already in folder)
export { AdvancedStylingPanel } from './AdvancedStylingPanel';
export { ExportDialog } from './ExportDialog';
export { TemplateGallery } from './TemplateGallery';
export * from './DataComponents';
export * from './AdditionalComponents';
export * from './CollaborationFeatures';
export * from './ProductivityFeatures';

// DOCX Editor components
export { DocxEditorToolbar } from './DocxEditorToolbar';
export { DocxEditorCanvas } from './DocxEditorCanvas';
export { DocxPropertiesPanel } from './DocxPropertiesPanel';

// Hooks
export {
  useReportTemplate,
  useFileOperations,
  useAIChat,
  useBrandKit,
  useImageUpload,
} from './hooks';
export { useDocxEditor } from './useDocxEditor';

// Types
export * from './types';

// Constants
export * from './constants';
