import React, { useState, useCallback, useEffect, useRef } from 'react';
import {
  FolderOpen,
  Settings as SettingsIcon,
  Palette,
  Bot,
  MessageCircle,
  History,
} from 'lucide-react';
import { toast } from 'sonner';
import { useTranslation } from 'react-i18next';
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

// Import hooks
import {
  useReportTemplate,
  useFileOperations,
  useAIChat,
  useBrandKit,
  useImageUpload,
} from './hooks';

// Import types and constants
import { PageTheme, RightPanelView } from './types';
import { FINCEPT_COLORS } from './constants';

const ReportBuilderTab: React.FC = () => {
  const { t } = useTranslation('reportBuilder');
  const [currentTime, setCurrentTime] = useState(new Date());

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
    const templateName = prompt('Enter template name:', `${selectedComp.type} Template`);
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
    <div className="h-full flex flex-col" style={{ backgroundColor: FINCEPT_COLORS.DARK_BG, color: FINCEPT_COLORS.TEXT_PRIMARY }}>
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
        onSave={saveTemplate}
        onExport={() => setShowExportDialog(true)}
        isSaving={isSaving}
        isAutoSaving={isAutoSaving}
        lastSaved={lastSaved}
        hasComponents={template.components.length > 0}
      />

      {/* Main Content */}
      <div className="flex-1 flex overflow-hidden">
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

        {/* Right Panel - Properties, AI Chat, and Advanced Styling */}
        <div className="w-1/5 border-l flex flex-col min-h-0" style={{ borderColor: FINCEPT_COLORS.BORDER, backgroundColor: FINCEPT_COLORS.PANEL_BG }}>
          {/* Toggle Buttons */}
          <div className="flex flex-wrap border-b" style={{ borderColor: FINCEPT_COLORS.BORDER }}>
            <button
              onClick={() => setRightPanelView('properties')}
              className={`flex-1 px-2 py-2 text-[10px] font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'properties' ? 'bg-[#FFA500] text-black' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{ color: rightPanelView === 'properties' ? '#000' : FINCEPT_COLORS.TEXT_PRIMARY }}
              title="Component Properties"
            >
              <SettingsIcon size={12} />
              Props
            </button>
            <button
              onClick={() => setRightPanelView('styling')}
              className={`flex-1 px-2 py-2 text-[10px] font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'styling' ? 'bg-[#FFA500] text-black' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{ color: rightPanelView === 'styling' ? '#000' : FINCEPT_COLORS.TEXT_PRIMARY }}
              title="Advanced Styling"
            >
              <Palette size={12} />
              Style
            </button>
            <button
              onClick={() => setRightPanelView('chat')}
              className={`flex-1 px-2 py-2 text-[10px] font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'chat' ? 'bg-[#FFA500] text-black' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{ color: rightPanelView === 'chat' ? '#000' : FINCEPT_COLORS.TEXT_PRIMARY }}
              title="AI Writing Assistant"
            >
              <Bot size={12} />
              AI
            </button>
            <button
              onClick={() => setRightPanelView('comments')}
              className={`flex-1 px-2 py-2 text-[10px] font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'comments' ? 'bg-[#FFA500] text-black' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{ color: rightPanelView === 'comments' ? '#000' : FINCEPT_COLORS.TEXT_PRIMARY }}
              title="Comments"
            >
              <MessageCircle size={12} />
            </button>
            <button
              onClick={() => setRightPanelView('history')}
              className={`flex-1 px-2 py-2 text-[10px] font-semibold transition-colors flex items-center justify-center gap-1 ${
                rightPanelView === 'history' ? 'bg-[#FFA500] text-black' : 'bg-transparent hover:bg-[#2a2a2a]'
              }`}
              style={{ color: rightPanelView === 'history' ? '#000' : FINCEPT_COLORS.TEXT_PRIMARY }}
              title="Version History"
            >
              <History size={12} />
            </button>
          </div>

          {/* Properties Section */}
          {(rightPanelView === 'properties' || rightPanelView === 'split') && (
            <div className={`${rightPanelView === 'split' ? 'flex-1 border-b' : 'flex-1'} overflow-y-auto min-h-0`} style={{ borderColor: FINCEPT_COLORS.BORDER }}>
              <PropertiesPanel
                selectedComponent={selectedComp}
                onUpdateComponent={updateComponent}
                onDeleteComponent={deleteComponent}
                onDuplicateComponent={duplicateComponent}
                onClearSelection={() => setSelectedComponent(null)}
                onSaveAsTemplate={saveAsTemplate}
                onImageUpload={handleImageUpload}
              />
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
        leftInfo={[
          { label: `Components: ${template.components.length}`, color: FINCEPT_COLORS.TEXT_PRIMARY },
          { label: `Theme: ${pageTheme.charAt(0).toUpperCase() + pageTheme.slice(1)}`, color: FINCEPT_COLORS.TEXT_PRIMARY },
          ...(activeBrandKit ? [{ label: `Brand: ${activeBrandKit.name}`, color: FINCEPT_COLORS.ORANGE }] : []),
        ]}
        statusInfo={`${currentTime.toLocaleTimeString()} | Multi-format Export`}
        backgroundColor={FINCEPT_COLORS.PANEL_BG}
        borderColor={FINCEPT_COLORS.ORANGE}
      />
    </div>
  );
};

export default ReportBuilderTab;
