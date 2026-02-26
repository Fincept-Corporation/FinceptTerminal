import React, { useState, useCallback, useEffect, useRef, lazy, Suspense } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import {
  FolderOpen,
  Settings as SettingsIcon,
  Palette,
  Bot,
  MessageCircle,
  History,
  FileType,
} from 'lucide-react';
import { toast } from '@/components/ui/terminal-toast';
import { useTranslation } from 'react-i18next';
import { showPrompt } from '@/utils/notifications';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { ReportComponent } from '@/services/core/reportService';
import { ComponentTemplate, brandKitService, BrandKit, HeaderFooterTemplate, ParagraphStyles } from '@/services/core/brandKitService';
import { Dialog, DialogContent, DialogDescription, DialogFooter, DialogHeader, DialogTitle } from '@/components/ui/dialog';
import { Button } from '@/components/ui/button';
import { TabFooter } from '@/components/common/TabFooter';
import { reportBuilderMCPBridge } from '@/services/mcp/internal';

// Import sub-components
import { ReportHeader } from './ReportHeader';
import { ComponentToolbar } from './ComponentToolbar';
import { DocumentCanvas } from './DocumentCanvas';
import { PropertiesPanel } from './PropertiesPanel';
import { AIChatPanel } from './AIChatPanel';
import { AdvancedStylingPanel } from './AdvancedStylingPanel';
import { ExportDialog } from './ExportDialog';
import { TemplateGallery } from './TemplateGallery';
import {
  CommentsPanel,
  VersionHistoryPanel,
  useCollaboration,
} from './CollaborationFeatures';
import {
  useUndoRedo,
  useAutoSave,
  useKeyboardShortcuts,
  FindReplaceDialog,
  ShortcutsDialog,
} from './ProductivityFeatures';

// Lazy-load DOCX editor components for bundle optimization
const DocxEditorToolbar = lazy(() => import('./DocxEditorToolbar'));
const DocxEditorCanvas = lazy(() => import('./DocxEditorCanvas'));
const DocxPropertiesPanel = lazy(() => import('./DocxPropertiesPanel'));

// Import DOCX editor hook (lightweight, no heavy deps)
import { useDocxEditor } from './useDocxEditor';

// Import hooks
import {
  useReportTemplate,
  useFileOperations,
  useAIChat,
  useBrandKit,
  useImageUpload,
} from './hooks';

// Import types and constants
import { PageTheme, RightPanelView, ReportBuilderMode } from './types';

const ReportBuilderTab: React.FC = () => {
  const { t } = useTranslation('reportBuilder');
  const { colors, fontSize } = useTerminalTheme();
  const [currentTime, setCurrentTime] = useState(new Date());

  // Builder mode: 'builder' (existing) or 'docx' (new DOCX editor)
  const [builderMode, setBuilderMode] = useState<ReportBuilderMode>('builder');

  // DOCX Editor hook (always initialized so TipTap editor is ready when switching modes)
  const docxEditor = useDocxEditor();

  // Template management hook
  const {
    template,
    setTemplate,
    selectedComponent,
    setSelectedComponent,
    selectedComp,
    addComponent,
    updateComponent,
    deleteComponent,
    duplicateComponent,
    handleDragEnd,
    updateMetadata,
    tocItems,
  } = useReportTemplate();

  // Style states
  const [pageTheme, setPageTheme] = useState<PageTheme>('classic');
  const [fontFamily, setFontFamily] = useState<string>('Arial, sans-serif');
  const [defaultFontSize, setDefaultFontSize] = useState<number>(11);
  const [isBold, setIsBold] = useState<boolean>(false);
  const [isItalic, setIsItalic] = useState<boolean>(false);
  const [customBgColor, setCustomBgColor] = useState<string>('#ffffff');
  const [showColorPicker, setShowColorPicker] = useState<boolean>(false);

  // UI states
  const [rightPanelView, setRightPanelView] = useState<RightPanelView>('split');
  const [showTemplateGallery, setShowTemplateGallery] = useState(false);
  const [showFindReplace, setShowFindReplace] = useState(false);
  const [showShortcuts, setShowShortcuts] = useState(false);
  const [showExportDialog, setShowExportDialog] = useState(false);

  // File operations hook
  const {
    isSaving,
    showSuccessDialog,
    setShowSuccessDialog,
    generatedPdfPath,
    saveTemplate,
    loadTemplate,
    handleShowInFolder,
  } = useFileOperations(template, setTemplate);

  // AI chat hook
  const aiChat = useAIChat(template, fontFamily, defaultFontSize, pageTheme);

  // Brand kit hook
  const {
    activeBrandKit,
    advancedStyles,
    setAdvancedStyles,
    handleBrandKitChange: onBrandKitChange,
  } = useBrandKit();

  // Image upload hook
  const { handleImageUpload } = useImageUpload(selectedComp, updateComponent);

  // Undo/Redo hook
  const {
    state: templateHistory,
    set: setTemplateWithHistory,
    undo,
    redo,
    canUndo,
    canRedo,
  } = useUndoRedo(template);

  // Auto-save hook
  const { isSaving: isAutoSaving, lastSaved, forceSave } = useAutoSave(
    template,
    async (data) => {
      try {
        const { reportService } = await import('@/services/core/reportService');
        await reportService.saveTemplate(data);
        console.log('Auto-saved report');
      } catch (error) {
        console.error('Auto-save failed:', error);
      }
    },
    { interval: 60000, enabled: true }
  );

  // Collaboration hook
  const collaboration = useCollaboration(template);

  // Keyboard shortcuts
  useKeyboardShortcuts({
    'ctrl+z': () => undo(),
    'ctrl+y': () => redo(),
    'ctrl+shift+z': () => redo(),
    'ctrl+s': () => { forceSave(); toast.success('Saved'); },
    'ctrl+f': () => setShowFindReplace(true),
    'ctrl+shift+/': () => setShowShortcuts(true),
  });

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    pageTheme,
    fontFamily,
    defaultFontSize,
    rightPanelView,
    builderMode,
  }), [pageTheme, fontFamily, defaultFontSize, rightPanelView, builderMode]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.pageTheme === 'string') setPageTheme(state.pageTheme as PageTheme);
    if (typeof state.fontFamily === 'string') setFontFamily(state.fontFamily);
    if (typeof state.defaultFontSize === 'number') setDefaultFontSize(state.defaultFontSize);
    if (typeof state.rightPanelView === 'string') setRightPanelView(state.rightPanelView as RightPanelView);
    if (typeof state.builderMode === 'string') setBuilderMode(state.builderMode as ReportBuilderMode);
  }, []);

  useWorkspaceTabState('report-builder', getWorkspaceState, setWorkspaceState);

  // Sync template when undo/redo changes history (only when user triggers undo/redo)
  const prevHistoryRef = useRef(templateHistory);
  useEffect(() => {
    // Only sync if templateHistory changed (undo/redo action), not if template changed (direct edit)
    if (prevHistoryRef.current !== templateHistory) {
      prevHistoryRef.current = templateHistory;
      setTemplate(templateHistory);
    }
  }, [templateHistory, setTemplate]);

  // Update time
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Register MCP bridge handlers on mount
  useEffect(() => {
    const registerBridge = async () => {
      try {
        console.log('[ReportBuilder] Registering MCP bridge handlers');
        await reportBuilderMCPBridge.registerHandlers({
          template,
          setTemplate,
          addComponent,
          updateComponent,
          deleteComponent,
          updateMetadata,
          saveTemplate,
          loadTemplate: async (path: string) => {
            await loadTemplate();
          },
          exportReport: async (format: string) => {
            // Export logic would be handled by export dialog
            return generatedPdfPath || '';
          },
          setPageTheme: (theme: string) => setPageTheme(theme as PageTheme),
        });
      } catch (error) {
        console.error('[ReportBuilder] Error registering MCP bridge:', error);
      }
    };

    registerBridge();

    return () => {
      try {
        console.log('[ReportBuilder] Unregistering MCP bridge handlers');
        reportBuilderMCPBridge.unregisterHandlers();
      } catch (error) {
        console.error('[ReportBuilder] Error unregistering MCP bridge:', error);
      }
    };
    // Only register once on mount
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Update template in bridge when it changes
  useEffect(() => {
    try {
      if (reportBuilderMCPBridge && reportBuilderMCPBridge.updateTemplate) {
        reportBuilderMCPBridge.updateTemplate(template);
      }
    } catch (error) {
      console.error('[ReportBuilder] Error updating template in MCP bridge:', error);
    }
  }, [template]);

  // Handle brand kit change (with font update)
  const handleBrandKitChange = useCallback((brandKit: BrandKit) => {
    const bodyFontFamily = onBrandKitChange(brandKit);
    if (bodyFontFamily) {
      setFontFamily(bodyFontFamily);
    }
  }, [onBrandKitChange]);

  // Handle component template selection
  const handleComponentTemplateSelect = useCallback((componentTemplate: ComponentTemplate) => {
    const newComponent: ReportComponent = {
      id: `component-${Date.now()}`,
      type: componentTemplate.component.type as ReportComponent['type'],
      content: componentTemplate.component.content,
      config: componentTemplate.component.config,
      children: componentTemplate.component.children,
    };
    setTemplate(prev => ({
      ...prev,
      components: [...prev.components, newComponent],
    }));
    setSelectedComponent(newComponent.id);
    toast.success(`Added template: ${componentTemplate.name}`);
  }, [setTemplate, setSelectedComponent]);

  // Handle advanced styles change
  const handleStylesChange = useCallback((styles: {
    customCSS: string;
    headerTemplate: HeaderFooterTemplate;
    footerTemplate: HeaderFooterTemplate;
    paragraphStyles: ParagraphStyles;
  }) => {
    setAdvancedStyles(styles);
  }, [setAdvancedStyles]);

  // Save component as template
  const saveAsTemplate = useCallback(async () => {
    if (!selectedComp) return;
    const templateName = await showPrompt('Enter template name:', {
      title: 'Save as Template',
      defaultValue: `${selectedComp.type} Template`
    });
    if (!templateName) return;

    const newTemplate: ComponentTemplate = {
      id: `template-${Date.now()}`,
      name: templateName,
      description: `Custom ${selectedComp.type} template`,
      category: selectedComp.type === 'section' || selectedComp.type === 'columns' ? 'layout' :
                selectedComp.type === 'image' ? 'media' :
                selectedComp.type === 'chart' || selectedComp.type === 'table' ? 'data' : 'text',
      component: {
        type: selectedComp.type,
        content: selectedComp.content,
        config: selectedComp.config,
        children: selectedComp.children,
      },
      created_at: new Date().toISOString(),
      updated_at: new Date().toISOString(),
    };

    try {
      await brandKitService.saveComponentTemplate(newTemplate);
      toast.success('Component saved as template');
    } catch (error) {
      toast.error('Failed to save template');
    }
  }, [selectedComp]);

  // Handle add component with history
  const handleAddComponent = useCallback((type: ReportComponent['type']) => {
    const newComp = addComponent(type);
    // addComponent returns the new component and updates template via setTemplate
    // We need to build the updated template for undo/redo history
    if (newComp) {
      setTemplateWithHistory({
        ...template,
        components: [...template.components, newComp],
      });
    }
  }, [addComponent, template, setTemplateWithHistory]);

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: colors.background, color: colors.text }}>
      {/* Header */}
      <ReportHeader
        canUndo={canUndo}
        canRedo={canRedo}
        onUndo={undo}
        onRedo={redo}
        onFindReplace={() => setShowFindReplace(true)}
        onShortcuts={() => setShowShortcuts(true)}
        onTemplateGallery={() => setShowTemplateGallery(true)}
        onLoad={loadTemplate}
        onSave={builderMode === 'docx' ? docxEditor.saveDocx : saveTemplate}
        onExport={builderMode === 'docx' ? docxEditor.exportDocx : () => setShowExportDialog(true)}
        isSaving={isSaving}
        isAutoSaving={isAutoSaving}
        lastSaved={lastSaved}
        hasComponents={template.components.length > 0}
        builderMode={builderMode}
        onModeChange={setBuilderMode}
        onImportDocx={docxEditor.importDocx}
        onExportDocx={docxEditor.exportDocx}
        docxFileName={docxEditor.docxState.fileName}
        isDocxDirty={docxEditor.docxState.isDirty}
      />

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden', minHeight: 0 }}>
        {builderMode === 'builder' ? (
          <>
            {/* Left Panel - Components */}
            <ComponentToolbar
              onAddComponent={handleAddComponent}
              template={template}
              selectedComponent={selectedComponent}
              onSelectComponent={setSelectedComponent}
              onDeleteComponent={deleteComponent}
              onDuplicateComponent={duplicateComponent}
              onDragEnd={handleDragEnd}
              pageTheme={pageTheme}
              onPageThemeChange={setPageTheme}
              customBgColor={customBgColor}
              onCustomBgColorChange={setCustomBgColor}
              showColorPicker={showColorPicker}
              onShowColorPickerChange={setShowColorPicker}
              fontFamily={fontFamily}
              onFontFamilyChange={setFontFamily}
              defaultFontSize={defaultFontSize}
              onDefaultFontSizeChange={setDefaultFontSize}
              isBold={isBold}
              onIsBoldChange={setIsBold}
              isItalic={isItalic}
              onIsItalicChange={setIsItalic}
            />

            {/* Center - Canvas */}
            <DocumentCanvas
              template={template}
              selectedComponent={selectedComponent}
              onSelectComponent={setSelectedComponent}
              onUpdateComponent={updateComponent}
              pageTheme={pageTheme}
              customBgColor={customBgColor}
              fontFamily={fontFamily}
              defaultFontSize={defaultFontSize}
              isBold={isBold}
              isItalic={isItalic}
              advancedStyles={advancedStyles}
              tocItems={tocItems}
            />
          </>
        ) : (
          /* DOCX Editor Mode */
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minHeight: 0, minWidth: 0, overflow: 'hidden' }}>
            <Suspense fallback={
              <div className="flex-1 flex items-center justify-center" style={{ color: colors.textMuted }}>
                Loading DOCX editor...
              </div>
            }>
              {/* DOCX Toolbar */}
              <DocxEditorToolbar
                editor={docxEditor.editor}
                viewMode={docxEditor.docxState.viewMode}
                onViewModeChange={docxEditor.setViewMode}
                onInsertImage={docxEditor.insertImage}
                onToggleWatermark={docxEditor.toggleWatermark}
                watermarkEnabled={docxEditor.pageSettings.watermarkEnabled}
                onToggleHeader={docxEditor.toggleHeader}
                onToggleFooter={docxEditor.toggleFooter}
                headerEnabled={docxEditor.pageSettings.headerEnabled}
                footerEnabled={docxEditor.pageSettings.footerEnabled}
                lineSpacing={docxEditor.pageSettings.lineSpacing}
                onLineSpacingChange={(spacing) => docxEditor.updatePageSettings({ lineSpacing: spacing })}
                zoom={docxEditor.zoom}
                onZoomChange={docxEditor.setZoom}
              />
              {/* DOCX Canvas */}
              <div style={{ flex: 1, minHeight: 0, overflow: 'hidden' }}>
                <DocxEditorCanvas
                  editor={docxEditor.editor}
                  viewMode={docxEditor.docxState.viewMode}
                  originalArrayBuffer={docxEditor.docxState.originalArrayBuffer}
                  watermarkEnabled={docxEditor.pageSettings.watermarkEnabled}
                  watermarkText={docxEditor.pageSettings.watermarkText}
                  headerEnabled={docxEditor.pageSettings.headerEnabled}
                  footerEnabled={docxEditor.pageSettings.footerEnabled}
                  headerText={docxEditor.pageSettings.headerText}
                  footerText={docxEditor.pageSettings.footerText}
                  onToggleHeader={docxEditor.toggleHeader}
                  onToggleFooter={docxEditor.toggleFooter}
                  onHeaderTextChange={(text) => docxEditor.updatePageSettings({ headerText: text })}
                  onFooterTextChange={(text) => docxEditor.updatePageSettings({ footerText: text })}
                  lineSpacing={docxEditor.pageSettings.lineSpacing}
                  margins={docxEditor.pageSettings.margins}
                  zoom={docxEditor.zoom}
                />
              </div>
            </Suspense>
          </div>
        )}

        {/* Right Panel - Properties, AI Chat, and Advanced Styling */}
        <div style={{ width: '280px', minWidth: '280px', maxWidth: '280px', borderLeft: `1px solid ${colors.panel}`, display: 'flex', flexDirection: 'column', minHeight: 0, backgroundColor: colors.panel, flexShrink: 0 }}>
          {/* Toggle Buttons */}
          <div className="flex flex-wrap border-b" style={{ borderColor: colors.panel }}>
            <button
              onClick={() => setRightPanelView('properties')}
              className={`flex-1 px-2 py-2 font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'properties' ? 'hover:opacity-90' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{
                fontSize: fontSize.tiny,
                backgroundColor: rightPanelView === 'properties' ? colors.primary : 'transparent',
                color: rightPanelView === 'properties' ? '#000' : colors.text
              }}
              title="Component Properties"
            >
              <SettingsIcon size={12} />
              Props
            </button>
            <button
              onClick={() => setRightPanelView('styling')}
              className={`flex-1 px-2 py-2 font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'styling' ? 'hover:opacity-90' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{
                fontSize: fontSize.tiny,
                backgroundColor: rightPanelView === 'styling' ? colors.primary : 'transparent',
                color: rightPanelView === 'styling' ? '#000' : colors.text
              }}
              title="Advanced Styling"
            >
              <Palette size={12} />
              Style
            </button>
            <button
              onClick={() => setRightPanelView('chat')}
              className={`flex-1 px-2 py-2 font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'chat' ? 'hover:opacity-90' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{
                fontSize: fontSize.tiny,
                backgroundColor: rightPanelView === 'chat' ? colors.primary : 'transparent',
                color: rightPanelView === 'chat' ? '#000' : colors.text
              }}
              title="AI Writing Assistant"
            >
              <Bot size={12} />
              AI
            </button>
            <button
              onClick={() => setRightPanelView('comments')}
              className={`flex-1 px-2 py-2 font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'comments' ? 'hover:opacity-90' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{
                fontSize: fontSize.tiny,
                backgroundColor: rightPanelView === 'comments' ? colors.primary : 'transparent',
                color: rightPanelView === 'comments' ? '#000' : colors.text
              }}
              title="Comments"
            >
              <MessageCircle size={12} />
            </button>
            <button
              onClick={() => setRightPanelView('history')}
              className={`flex-1 px-2 py-2 font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'history' ? 'hover:opacity-90' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{
                fontSize: fontSize.tiny,
                backgroundColor: rightPanelView === 'history' ? colors.primary : 'transparent',
                color: rightPanelView === 'history' ? '#000' : colors.text
              }}
              title="Version History"
            >
              <History size={12} />
            </button>
          </div>

          {/* Properties Section */}
          {(rightPanelView === 'properties' || rightPanelView === 'split') && (
            <div className={`${rightPanelView === 'split' ? 'flex-1 border-b' : 'flex-1'} overflow-y-auto min-h-0`} style={{ borderColor: colors.panel }}>
              {builderMode === 'docx' ? (
                <Suspense fallback={<div className="p-3 text-xs" style={{ color: colors.textMuted }}>Loading...</div>}>
                  <DocxPropertiesPanel
                    metadata={docxEditor.docxState.metadata}
                    onMetadataChange={docxEditor.updateMetadata}
                    htmlContent={docxEditor.docxState.htmlContent}
                    filePath={docxEditor.docxState.filePath}
                    fileId={docxEditor.docxState.fileId}
                    isDirty={docxEditor.docxState.isDirty}
                    onCreateSnapshot={docxEditor.createSnapshot}
                    onRestoreSnapshot={docxEditor.restoreSnapshot}
                    onDeleteSnapshot={docxEditor.deleteSnapshot}
                    snapshots={docxEditor.snapshots}
                    pageSettings={docxEditor.pageSettings}
                    onPageSettingsChange={docxEditor.updatePageSettings}
                  />
                </Suspense>
              ) : (
                <PropertiesPanel
                  selectedComponent={selectedComp}
                  onUpdateComponent={updateComponent}
                  onDeleteComponent={deleteComponent}
                  onDuplicateComponent={duplicateComponent}
                  onClearSelection={() => setSelectedComponent(null)}
                  onSaveAsTemplate={saveAsTemplate}
                  onImageUpload={handleImageUpload}
                />
              )}
            </div>
          )}

          {/* AI Writing Assistant Section */}
          {(rightPanelView === 'chat' || rightPanelView === 'split') && (
            <div className={`${rightPanelView === 'split' ? 'flex-1' : 'flex-1'} flex flex-col min-h-0`}>
              <AIChatPanel
                messages={aiChat.messages}
                input={aiChat.input}
                onInputChange={aiChat.setInput}
                onSendMessage={aiChat.sendMessage}
                onQuickAction={aiChat.handleQuickAction}
                isTyping={aiChat.isTyping}
                streamingContent={aiChat.streamingContent}
                currentProvider={aiChat.currentProvider}
                onProviderChange={aiChat.setCurrentProvider}
                availableModels={aiChat.availableModels}
              />
            </div>
          )}

          {/* Advanced Styling Panel */}
          {rightPanelView === 'styling' && (
            <div className="flex-1 overflow-hidden">
              <AdvancedStylingPanel
                onBrandKitChange={handleBrandKitChange}
                onComponentTemplateSelect={handleComponentTemplateSelect}
                onStylesChange={handleStylesChange}
              />
            </div>
          )}

          {/* Comments Panel */}
          {rightPanelView === 'comments' && (
            <div className="flex-1 overflow-hidden">
              <CommentsPanel
                comments={collaboration.comments}
                onAddComment={(componentId: string, content: string) =>
                  collaboration.addComment(componentId, content)
                }
                onResolveComment={collaboration.resolveComment}
                onDeleteComment={collaboration.deleteComment}
                onReplyComment={collaboration.replyToComment}
                selectedComponentId={selectedComponent}
              />
            </div>
          )}

          {/* Version History Panel */}
          {rightPanelView === 'history' && (
            <div className="flex-1 overflow-hidden">
              <VersionHistoryPanel
                history={collaboration.versionHistory}
                onPreview={(entry) => {
                  toast.info(`Previewing version: ${entry.description}`);
                }}
                onRestore={(entry) => {
                  if (entry.template) {
                    setTemplate(entry.template);
                    setTemplateWithHistory(entry.template);
                    toast.success(`Restored version: ${entry.description}`);
                  }
                }}
              />
            </div>
          )}
        </div>
      </div>

      {/* Success Dialog */}
      <Dialog open={showSuccessDialog} onOpenChange={setShowSuccessDialog}>
        <DialogContent className="bg-[#1a1a1a] border-[#333333]">
          <DialogHeader>
            <DialogTitle className="text-[#FFA500]">PDF Generated Successfully!</DialogTitle>
            <DialogDescription className="text-gray-400">
              Your report has been generated and saved.
            </DialogDescription>
          </DialogHeader>
          <div className="py-4">
            <p className="text-sm text-gray-300 mb-2">File saved to:</p>
            <p className="text-xs text-gray-500 break-all bg-[#0a0a0a] p-2 rounded border border-[#333333]">
              {generatedPdfPath}
            </p>
          </div>
          <DialogFooter>
            <Button
              variant="outline"
              onClick={() => setShowSuccessDialog(false)}
              className="bg-transparent border-[#333333] text-white hover:bg-[#2a2a2a]"
            >
              Close
            </Button>
            <Button
              onClick={handleShowInFolder}
              className="bg-[#FFA500] text-black hover:bg-[#ff8c00]"
            >
              <FolderOpen className="w-4 h-4 mr-2" />
              Show in Folder
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>

      {/* Export Dialog */}
      <ExportDialog
        open={showExportDialog}
        onClose={() => setShowExportDialog(false)}
        template={template}
        brandKit={activeBrandKit}
        builderMode={builderMode}
        docxHtmlContent={builderMode === 'docx' ? (docxEditor.editor?.getHTML() || docxEditor.docxState.htmlContent) : undefined}
        docxMetadata={builderMode === 'docx' ? docxEditor.docxState.metadata : undefined}
      />

      {/* Template Gallery Dialog */}
      <TemplateGallery
        open={showTemplateGallery}
        onClose={() => setShowTemplateGallery(false)}
        onSelectTemplate={(t) => {
          setTemplate(t);
          setTemplateWithHistory(t);
          toast.success(`Applied template: ${t.name}`);
        }}
        currentTemplate={template}
      />

      {/* Find & Replace Dialog */}
      <FindReplaceDialog
        open={showFindReplace}
        onClose={() => setShowFindReplace(false)}
        onReplace={(componentId, oldText, newText) => {
          updateComponent(componentId, { content: newText });
        }}
        components={template.components}
      />

      {/* Keyboard Shortcuts Dialog */}
      <ShortcutsDialog
        open={showShortcuts}
        onClose={() => setShowShortcuts(false)}
      />

      <TabFooter
        tabName="REPORT BUILDER"
        leftInfo={builderMode === 'docx' ? [
          { label: `Mode: DOCX Editor`, color: colors.primary },
          { label: docxEditor.docxState.fileName, color: colors.text },
          { label: docxEditor.docxState.isDirty ? 'Modified' : 'Saved', color: docxEditor.docxState.isDirty ? '#FFA500' : '#10b981' },
        ] : [
          { label: `Components: ${template.components.length}`, color: colors.text },
          { label: `Theme: ${pageTheme.charAt(0).toUpperCase() + pageTheme.slice(1)}`, color: colors.text },
          ...(activeBrandKit ? [{ label: `Brand: ${activeBrandKit.name}`, color: colors.primary }] : []),
        ]}
        statusInfo={`${currentTime.toLocaleTimeString()} | Multi-format Export`}
        backgroundColor={colors.panel}
        borderColor={colors.primary}
      />
    </div>
  );
};

export default ReportBuilderTab;
