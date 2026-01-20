import { useState, useCallback, useEffect } from 'react';
import { ReportTemplate, ReportComponent, reportService } from '@/services/core/reportService';
import { sqliteService } from '@/services/core/sqliteService';
import { llmApiService, ChatMessage as APIMessage } from '@/services/chat/llmApi';
import { brandKitService, BrandKit } from '@/services/core/brandKitService';
import { save, open } from '@tauri-apps/plugin-dialog';
import { invoke } from '@tauri-apps/api/core';
import { toast } from 'sonner';
import { arrayMove } from '@dnd-kit/sortable';
import { DragEndEvent } from '@dnd-kit/core';
import { AIMessage, AIModel, AdvancedStyles, TOCItem } from './types';
import { AI_QUICK_ACTIONS } from './constants';

// Hook for managing the report template
export const useReportTemplate = () => {
  const [template, setTemplate] = useState<ReportTemplate>({
    id: `report-${Date.now()}`,
    name: 'New Financial Report',
    description: 'Professional financial research report',
    components: [],
    metadata: {
      author: '',
      company: '',
      date: new Date().toISOString().split('T')[0],
      title: 'Financial Research Report',
    },
    styles: {
      pageSize: 'A4',
      orientation: 'portrait',
      margins: '20mm',
      headerFooter: true,
    },
  });

  const [selectedComponent, setSelectedComponent] = useState<string | null>(null);

  // Get selected component data
  const selectedComp = template.components.find(c => c.id === selectedComponent);

  // Add component
  const addComponent = useCallback((type: ReportComponent['type']) => {
    const newComponent: ReportComponent = {
      id: `component-${Date.now()}`,
      type,
      content: (type === 'divider' || type === 'pagebreak' || type === 'coverpage' || type === 'section' || type === 'columns') ? '' :
        type === 'heading' ? 'New Heading' :
          type === 'subheading' ? 'New Subheading' :
            type === 'text' ? 'Enter text here...' :
              type === 'quote' ? 'Enter quote text here...' :
                type === 'disclaimer' ? 'This report is for informational purposes only.' : '',
      config: {
        fontSize: type === 'heading' ? '2xl' : type === 'subheading' ? 'xl' : 'base',
        fontWeight: type === 'heading' || type === 'subheading' ? 'bold' : 'normal',
        alignment: 'left',
        ...(type === 'columns' && { columnCount: 2 }),
        ...(type === 'section' && {
          sectionTitle: 'New Section',
          sectionStyle: 'default',
          backgroundColor: '#f8f9fa',
          borderColor: '#FFA500',
          padding: '15px'
        }),
        ...(type === 'coverpage' && {
          subtitle: '',
          backgroundColor: '#0a0a0a',
        }),
        ...(type === 'quote' && {
          quoteType: 'quote',
          author: '',
        }),
        ...(type === 'list' && {
          items: ['Item 1', 'Item 2', 'Item 3'],
          ordered: false,
        }),
        ...(type === 'toc' && {
          showPageNumbers: true,
        }),
        ...(type === 'kpi' && {
          kpis: [
            { label: 'Revenue', value: '$1.2M', change: 12.5, trend: 'up' },
            { label: 'Profit', value: '$340K', change: 8.2, trend: 'up' },
            { label: 'Margin', value: '28.3%', change: -2.1, trend: 'down' },
          ],
        }),
        ...(type === 'sparkline' && {
          data: [10, 25, 15, 30, 20, 35, 28],
          color: '#FFA500',
        }),
        ...(type === 'liveTable' && {
          dataSource: 'portfolio',
          columns: ['Symbol', 'Price', 'Change', 'Volume'],
        }),
        ...(type === 'dynamicChart' && {
          chartType: 'line',
          data: [
            { name: 'Jan', value: 100 },
            { name: 'Feb', value: 120 },
            { name: 'Mar', value: 115 },
            { name: 'Apr', value: 140 },
          ],
        }),
        ...(type === 'signature' && {
          name: 'John Doe',
          title: 'Analyst',
          showLine: true,
        }),
        ...(type === 'disclaimer' && {
          disclaimerType: 'standard',
        }),
        ...(type === 'qrcode' && {
          value: 'https://example.com',
          size: 100,
          label: 'Scan for more info',
        }),
        ...(type === 'watermark' && {
          text: 'CONFIDENTIAL',
          opacity: 0.1,
          rotation: -45,
        }),
      },
      ...(type === 'columns' || type === 'section' ? { children: [] } : {}),
    };

    setTemplate(prev => ({
      ...prev,
      components: [...prev.components, newComponent],
    }));
    setSelectedComponent(newComponent.id);
    toast.success(`${type.charAt(0).toUpperCase() + type.slice(1)} added`);

    return newComponent;
  }, []);

  // Update component
  const updateComponent = useCallback((id: string, updates: Partial<ReportComponent>) => {
    setTemplate(prev => ({
      ...prev,
      components: prev.components.map(comp =>
        comp.id === id ? { ...comp, ...updates } : comp
      ),
    }));
  }, []);

  // Delete component
  const deleteComponent = useCallback((id: string) => {
    setTemplate(prev => ({
      ...prev,
      components: prev.components.filter(comp => comp.id !== id),
    }));
    if (selectedComponent === id) {
      setSelectedComponent(null);
    }
    toast.success('Component deleted');
  }, [selectedComponent]);

  // Duplicate component
  const duplicateComponent = useCallback((id: string) => {
    const component = template.components.find(c => c.id === id);
    if (component) {
      const newComponent = {
        ...component,
        id: `component-${Date.now()}`,
      };
      setTemplate(prev => ({
        ...prev,
        components: [...prev.components, newComponent],
      }));
      toast.success('Component duplicated');
    }
  }, [template.components]);

  // Handle drag end
  const handleDragEnd = useCallback((event: DragEndEvent) => {
    const { active, over } = event;
    if (over && active.id !== over.id) {
      setTemplate(prev => {
        const oldIndex = prev.components.findIndex(c => c.id === active.id);
        const newIndex = prev.components.findIndex(c => c.id === over.id);
        return {
          ...prev,
          components: arrayMove(prev.components, oldIndex, newIndex),
        };
      });
    }
  }, []);

  // Update metadata
  const updateMetadata = useCallback((updates: Partial<ReportTemplate['metadata']>) => {
    setTemplate(prev => ({
      ...prev,
      metadata: { ...prev.metadata, ...updates },
    }));
  }, []);

  // Generate TOC items from headings
  const tocItems: TOCItem[] = template.components
    .filter(c => c.type === 'heading' || c.type === 'subheading')
    .map((c, idx) => ({
      id: c.id,
      title: String(c.content || `Section ${idx + 1}`),
      level: c.type === 'heading' ? 1 : 2,
      page: Math.floor(idx / 3) + 1,
    }));

  return {
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
  };
};

// Hook for file operations (save/load/export)
export const useFileOperations = (
  template: ReportTemplate,
  setTemplate: (t: ReportTemplate) => void
) => {
  const [isSaving, setIsSaving] = useState(false);
  const [isGenerating, setIsGenerating] = useState(false);
  const [showSuccessDialog, setShowSuccessDialog] = useState(false);
  const [generatedPdfPath, setGeneratedPdfPath] = useState('');

  // Save template
  const saveTemplate = async () => {
    try {
      setIsSaving(true);
      const filePath = await save({
        filters: [{
          name: 'JSON',
          extensions: ['json']
        }],
        defaultPath: `${template.name}.json`
      });

      if (filePath) {
        await reportService.saveTemplate(template);
        toast.success('Template saved successfully');
      }
    } catch (error) {
      console.error('Error saving template:', error);
      toast.error('Failed to save template');
    } finally {
      setIsSaving(false);
    }
  };

  // Load template
  const loadTemplate = async () => {
    try {
      await open({
        filters: [{
          name: 'JSON',
          extensions: ['json']
        }],
        multiple: false
      });

      const templates = await reportService.getTemplates();
      if (templates.length > 0) {
        setTemplate(templates[0]);
        toast.success('Template loaded successfully');
      } else {
        toast.info('No saved templates found. Use a sample template instead.');
      }
    } catch (error) {
      console.error('Error loading template:', error);
      toast.error('Failed to load template');
    }
  };

  // Generate PDF
  const generatePdf = async () => {
    try {
      setIsGenerating(true);

      const filePath = await save({
        filters: [{
          name: 'PDF',
          extensions: ['pdf']
        }],
        defaultPath: `${template.metadata.title || 'report'}.pdf`
      });

      if (filePath) {
        await invoke('generate_report_pdf', {
          templateJson: JSON.stringify(template),
          outputPath: filePath
        });

        setGeneratedPdfPath(filePath);
        setShowSuccessDialog(true);
      }
    } catch (error) {
      console.error('Error generating PDF:', error);
      toast.error('Failed to generate PDF');
    } finally {
      setIsGenerating(false);
    }
  };

  // Show in folder
  const handleShowInFolder = async () => {
    try {
      const folderPath = generatedPdfPath.substring(0, generatedPdfPath.lastIndexOf('\\'));
      await invoke('open_folder', { path: folderPath });
      setShowSuccessDialog(false);
    } catch (error) {
      console.error('Error opening folder:', error);
      toast.error('Failed to open folder');
    }
  };

  return {
    isSaving,
    isGenerating,
    showSuccessDialog,
    setShowSuccessDialog,
    generatedPdfPath,
    saveTemplate,
    loadTemplate,
    generatePdf,
    handleShowInFolder,
  };
};

// Hook for AI chat functionality
export const useAIChat = (template: ReportTemplate, fontFamily: string, defaultFontSize: number, pageTheme: string) => {
  const [messages, setMessages] = useState<AIMessage[]>([]);
  const [input, setInput] = useState('');
  const [isTyping, setIsTyping] = useState(false);
  const [streamingContent, setStreamingContent] = useState('');
  const [currentProvider, setCurrentProvider] = useState('ollama');
  const [availableModels, setAvailableModels] = useState<AIModel[]>([]);

  // Load available AI models
  useEffect(() => {
    const loadModels = async () => {
      try {
        const configs = await sqliteService.getLLMConfigs();
        const models = configs.map((config: any) => ({
          id: config.config_name,
          name: config.model_name || config.config_name,
          provider: config.provider
        }));
        setAvailableModels(models);

        const activeConfig = await sqliteService.getActiveLLMConfig();
        if (activeConfig) {
          setCurrentProvider(activeConfig.provider);
        }
      } catch (error) {
        console.error('Failed to load models:', error);
      }
    };
    loadModels();
  }, []);

  // Send message
  const sendMessage = async () => {
    if (!input.trim()) return;

    const userMessage = { role: 'user', content: input.trim() };
    setMessages(prev => [...prev, userMessage]);
    setInput('');
    setIsTyping(true);
    setStreamingContent('');

    try {
      const activeConfig = await sqliteService.getActiveLLMConfig();
      if (!activeConfig) {
        setMessages(prev => [...prev, {
          role: 'assistant',
          content: 'Please configure your LLM settings first in the Chat tab or Settings.'
        }]);
        setIsTyping(false);
        return;
      }

      const contextMessage = `I'm working on a report titled "${template.metadata.title}". ${
        template.components.length > 0
          ? `It has ${template.components.length} components including ${template.components.map(c => c.type).join(', ')}.`
          : 'It\'s empty so far.'
      } The report uses ${fontFamily} font at ${defaultFontSize}pt with ${pageTheme} theme.`;

      const conversationHistory: APIMessage[] = [
        { role: 'system', content: `You are a professional writing assistant helping to create financial reports. ${contextMessage}` },
        ...messages.map(msg => ({
          role: msg.role as 'user' | 'assistant',
          content: msg.content
        })),
        { role: 'user', content: userMessage.content }
      ];

      let fullResponse = '';

      const response = await llmApiService.chat(
        userMessage.content,
        conversationHistory,
        (chunk, done) => {
          if (!done) {
            fullResponse += chunk;
            setStreamingContent(fullResponse);
          }
        }
      );

      if (response.error) {
        throw new Error(response.error);
      }

      fullResponse = response.content;

      setMessages(prev => [...prev, {
        role: 'assistant',
        content: fullResponse
      }]);
      setStreamingContent('');
    } catch (error) {
      console.error('AI Error:', error);
      setMessages(prev => [...prev, {
        role: 'assistant',
        content: 'Sorry, I encountered an error. Please try again.'
      }]);
    } finally {
      setIsTyping(false);
    }
  };

  // Quick action
  const handleQuickAction = (action: string) => {
    setInput(AI_QUICK_ACTIONS[action] || '');
  };

  return {
    messages,
    input,
    setInput,
    isTyping,
    streamingContent,
    currentProvider,
    setCurrentProvider,
    availableModels,
    sendMessage,
    handleQuickAction,
  };
};

// Hook for brand kit and advanced styles
export const useBrandKit = () => {
  const [activeBrandKit, setActiveBrandKit] = useState<BrandKit | null>(null);
  const [advancedStyles, setAdvancedStyles] = useState<AdvancedStyles>({
    customCSS: '',
    headerTemplate: null,
    footerTemplate: null,
    paragraphStyles: null,
  });

  // Load active brand kit on mount
  useEffect(() => {
    const loadBrandKit = async () => {
      try {
        const kit = await brandKitService.getActiveBrandKit();
        if (kit) {
          setActiveBrandKit(kit);
          setAdvancedStyles({
            customCSS: kit.customCSS || '',
            headerTemplate: kit.headerTemplate || null,
            footerTemplate: kit.footerTemplate || null,
            paragraphStyles: kit.paragraphStyles || null,
          });
        }
      } catch (error) {
        console.error('Failed to load brand kit:', error);
      }
    };
    loadBrandKit();
  }, []);

  // Handle brand kit change
  const handleBrandKitChange = useCallback((brandKit: BrandKit) => {
    setActiveBrandKit(brandKit);
    setAdvancedStyles({
      customCSS: brandKit.customCSS || '',
      headerTemplate: brandKit.headerTemplate || null,
      footerTemplate: brandKit.footerTemplate || null,
      paragraphStyles: brandKit.paragraphStyles || null,
    });
    toast.success(`Applied brand kit: ${brandKit.name}`);

    return brandKit.fonts.find(f => f.usage === 'body')?.family;
  }, []);

  return {
    activeBrandKit,
    advancedStyles,
    setAdvancedStyles,
    handleBrandKitChange,
  };
};

// Hook for image upload
export const useImageUpload = (
  selectedComp: ReportComponent | undefined,
  updateComponent: (id: string, updates: Partial<ReportComponent>) => void
) => {
  const handleImageUpload = async () => {
    try {
      const selected = await open({
        multiple: false,
        filters: [{
          name: 'Images',
          extensions: ['png', 'jpg', 'jpeg', 'gif', 'bmp', 'webp']
        }]
      });

      if (selected && typeof selected === 'string' && selectedComp) {
        updateComponent(selectedComp.id, {
          config: { ...selectedComp.config, imageUrl: selected }
        });
        toast.success('Image uploaded');
      }
    } catch (error) {
      toast.error('Failed to upload image');
    }
  };

  return { handleImageUpload };
};
