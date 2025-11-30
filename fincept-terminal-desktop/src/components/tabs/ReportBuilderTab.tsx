import React, { useState, useCallback, useEffect, useRef } from 'react';
import {
  FileText,
  Download,
  Save,
  Type,
  BarChart3,
  Table,
  Image as ImageIcon,
  Trash2,
  Copy,
  AlignLeft,
  Code,
  Minus,
  FolderOpen,
  Plus,
  Edit2,
  Move,
  Settings as SettingsIcon,
  X,
  Columns as ColumnsIcon,
  Layout,
  BookOpen,
  FileX,
  Bold,
  Italic,
  Bot,
  Send,
  User,
  Sparkles,
  ChevronDown
} from 'lucide-react';
import { DndContext, closestCenter, KeyboardSensor, PointerSensor, useSensor, useSensors, DragEndEvent } from '@dnd-kit/core';
import { arrayMove, SortableContext, sortableKeyboardCoordinates, useSortable, verticalListSortingStrategy } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import { reportService, ReportTemplate, ReportComponent } from '@/services/reportService';
import { save, open } from '@tauri-apps/plugin-dialog';
import { invoke } from '@tauri-apps/api/core';
import { toast } from 'sonner';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { Dialog, DialogContent, DialogDescription, DialogFooter, DialogHeader, DialogTitle } from '@/components/ui/dialog';
import { Button } from '@/components/ui/button';
import { llmApiService, ChatMessage as APIMessage } from '@/services/llmApi';
import { sqliteService } from '@/services/sqliteService';
import MarkdownRenderer from '../common/MarkdownRenderer';

// Bloomberg color palette
const BLOOMBERG_COLORS = {
  ORANGE: '#FFA500',
  WHITE: '#FFFFFF',
  BLACK: '#000000',
  DARK_BG: '#0a0a0a',
  PANEL_BG: '#1a1a1a',
  BORDER: '#333333',
  HOVER: '#2a2a2a',
  TEXT_PRIMARY: '#FFFFFF',
  TEXT_SECONDARY: '#999999',
};

// Sortable component wrapper
function SortableComponent({
  component,
  isSelected,
  onClick,
  onDelete,
  onDuplicate
}: {
  component: ReportComponent;
  isSelected: boolean;
  onClick: () => void;
  onDelete: () => void;
  onDuplicate: () => void;
}) {
  const {
    attributes,
    listeners,
    setNodeRef,
    transform,
    transition,
  } = useSortable({ id: component.id });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
  };

  const getComponentIcon = () => {
    switch (component.type) {
      case 'heading': return <Type size={14} />;
      case 'text': return <AlignLeft size={14} />;
      case 'chart': return <BarChart3 size={14} />;
      case 'table': return <Table size={14} />;
      case 'image': return <ImageIcon size={14} />;
      case 'code': return <Code size={14} />;
      case 'divider': return <Minus size={14} />;
      default: return <FileText size={14} />;
    }
  };

  const getPreviewText = () => {
    if (component.content) {
      return String(component.content).substring(0, 40);
    }
    return `${component.type.charAt(0).toUpperCase() + component.type.slice(1)} component`;
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      className={`flex items-center justify-between p-2 mb-1 cursor-pointer transition-colors border-l-2 ${
        isSelected
          ? 'bg-[#2a2a2a] border-l-[#FFA500]'
          : 'bg-transparent border-l-transparent hover:bg-[#1a1a1a]'
      }`}
      onClick={onClick}
    >
      <div className="flex items-center gap-2 flex-1 min-w-0">
        <div {...attributes} {...listeners} className="cursor-move text-gray-500 hover:text-[#FFA500]">
          <Move size={14} />
        </div>
        <div className="text-[#FFA500]">
          {getComponentIcon()}
        </div>
        <span className="text-xs text-gray-400 truncate">{getPreviewText()}</span>
      </div>
      <div className="flex gap-1">
        <button
          onClick={(e) => { e.stopPropagation(); onDuplicate(); }}
          className="p-1 hover:bg-[#333333] rounded text-gray-400 hover:text-[#FFA500]"
          title="Duplicate"
        >
          <Copy size={12} />
        </button>
        <button
          onClick={(e) => { e.stopPropagation(); onDelete(); }}
          className="p-1 hover:bg-[#333333] rounded text-gray-400 hover:text-red-500"
          title="Delete"
        >
          <Trash2 size={12} />
        </button>
      </div>
    </div>
  );
}

const ReportBuilderTab: React.FC = () => {
  const { colors } = useTerminalTheme();
  const [currentTime, setCurrentTime] = useState(new Date());
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
  const [isGenerating, setIsGenerating] = useState(false);
  const [isSaving, setIsSaving] = useState(false);
  const [showSuccessDialog, setShowSuccessDialog] = useState(false);
  const [generatedPdfPath, setGeneratedPdfPath] = useState<string>('');
  const [pageTheme, setPageTheme] = useState<'classic' | 'modern' | 'geometric' | 'gradient' | 'minimal' | 'pattern'>('classic');
  const [fontFamily, setFontFamily] = useState<string>('Arial, sans-serif');
  const [defaultFontSize, setDefaultFontSize] = useState<number>(11);
  const [isBold, setIsBold] = useState<boolean>(false);
  const [isItalic, setIsItalic] = useState<boolean>(false);
  const [customBgColor, setCustomBgColor] = useState<string>('#ffffff');
  const [showColorPicker, setShowColorPicker] = useState<boolean>(false);

  // AI Chat state
  const [aiMessages, setAiMessages] = useState<{role: string, content: string}[]>([]);
  const [aiInput, setAiInput] = useState('');
  const [isAiTyping, setIsAiTyping] = useState(false);
  const [currentProvider, setCurrentProvider] = useState('ollama');
  const [availableModels, setAvailableModels] = useState<{id: string, name: string, provider: string}[]>([]);
  const [streamingContent, setStreamingContent] = useState('');
  const aiMessagesEndRef = useRef<HTMLDivElement>(null);

  const sensors = useSensors(
    useSensor(PointerSensor),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    })
  );

  // Update time
  React.useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

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

        // Set active provider
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

  // Auto-scroll AI chat
  useEffect(() => {
    aiMessagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [aiMessages, streamingContent]);

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
               type === 'text' ? 'Enter text here...' : '',
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
      },
      ...(type === 'columns' || type === 'section' ? { children: [] } : {}),
    };

    setTemplate(prev => ({
      ...prev,
      components: [...prev.components, newComponent],
    }));

    setSelectedComponent(newComponent.id);
    toast.success(`${type.charAt(0).toUpperCase() + type.slice(1)} added`);
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
  const handleDragEnd = (event: DragEndEvent) => {
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
  };

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
      const filePath = await open({
        filters: [{
          name: 'JSON',
          extensions: ['json']
        }],
        multiple: false
      });

      // For now, we'll just load from saved templates in localStorage
      // In the future, could add file system loading
      const templates = await reportService.getTemplates();
      if (templates.length > 0) {
        // Use the first template or show a selection dialog
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

  // Handle show in folder
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

  // AI Chat - Send message
  const handleAiSendMessage = async () => {
    if (!aiInput.trim()) return;

    const userMessage = { role: 'user', content: aiInput.trim() };
    setAiMessages(prev => [...prev, userMessage]);
    setAiInput('');
    setIsAiTyping(true);
    setStreamingContent('');

    try {
      // Get active LLM config
      const activeConfig = await sqliteService.getActiveLLMConfig();
      if (!activeConfig) {
        setAiMessages(prev => [...prev, {
          role: 'assistant',
          content: 'Please configure your LLM settings first in the Chat tab or Settings.'
        }]);
        setIsAiTyping(false);
        return;
      }

      // Add context about the current report
      const contextMessage = `I'm working on a report titled "${template.metadata.title}". ${
        template.components.length > 0
          ? `It has ${template.components.length} components including ${template.components.map(c => c.type).join(', ')}.`
          : 'It\'s empty so far.'
      } The report uses ${fontFamily} font at ${defaultFontSize}pt with ${pageTheme} theme.`;

      const conversationHistory: APIMessage[] = [
        { role: 'system', content: `You are a professional writing assistant helping to create financial reports. ${contextMessage}` },
        ...aiMessages.map(msg => ({
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

      setAiMessages(prev => [...prev, {
        role: 'assistant',
        content: fullResponse
      }]);
      setStreamingContent('');
    } catch (error) {
      console.error('AI Error:', error);
      setAiMessages(prev => [...prev, {
        role: 'assistant',
        content: 'Sorry, I encountered an error. Please try again.'
      }]);
    } finally {
      setIsAiTyping(false);
    }
  };

  // AI Quick actions
  const handleAiQuickAction = (action: string) => {
    const prompts: Record<string, string> = {
      improve: 'Can you help me improve the content of this report? Suggest better wording and structure.',
      expand: 'Can you help me expand on the current content with more details and insights?',
      summarize: 'Can you help me create a concise executive summary for this report?',
      grammar: 'Can you check the grammar and suggest improvements for professional writing?'
    };

    setAiInput(prompts[action] || '');
  };

  // Get theme background style
  const getThemeBackground = () => {
    switch (pageTheme) {
      case 'modern':
        return {
          background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
          backgroundSize: 'cover',
          color: '#ffffff',
        };
      case 'geometric':
        return {
          backgroundColor: '#ffffff',
          backgroundImage: `
            linear-gradient(30deg, rgba(255,165,0,0.08) 12%, transparent 12.5%, transparent 87%, rgba(255,165,0,0.08) 87.5%, rgba(255,165,0,0.08)),
            linear-gradient(150deg, rgba(255,165,0,0.08) 12%, transparent 12.5%, transparent 87%, rgba(255,165,0,0.08) 87.5%, rgba(255,165,0,0.08)),
            linear-gradient(30deg, rgba(255,165,0,0.05) 12%, transparent 12.5%, transparent 87%, rgba(255,165,0,0.05) 87.5%, rgba(255,165,0,0.05)),
            linear-gradient(150deg, rgba(255,165,0,0.05) 12%, transparent 12.5%, transparent 87%, rgba(255,165,0,0.05) 87.5%, rgba(255,165,0,0.05))
          `,
          backgroundSize: '80px 140px',
          backgroundPosition: '0 0, 0 0, 40px 70px, 40px 70px',
          color: '#000000',
        };
      case 'gradient':
        return {
          background: 'linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%)',
          color: '#ffffff',
        };
      case 'minimal':
        return {
          backgroundColor: '#ffffff',
          backgroundImage: `
            linear-gradient(rgba(200,200,200,0.3) 1px, transparent 1px),
            linear-gradient(90deg, rgba(200,200,200,0.3) 1px, transparent 1px)
          `,
          backgroundSize: '20px 20px',
          color: '#000000',
        };
      case 'pattern':
        return {
          backgroundColor: '#ffffff',
          backgroundImage: `
            repeating-linear-gradient(45deg, transparent, transparent 10px, rgba(255,165,0,0.02) 10px, rgba(255,165,0,0.02) 20px),
            repeating-linear-gradient(-45deg, transparent, transparent 10px, rgba(255,165,0,0.02) 10px, rgba(255,165,0,0.02) 20px),
            radial-gradient(circle at 10% 10%, rgba(255,165,0,0.03) 0%, transparent 50%),
            radial-gradient(circle at 90% 90%, rgba(255,165,0,0.03) 0%, transparent 50%)
          `,
          backgroundSize: 'cover',
          color: '#000000',
        };
      case 'classic':
      default:
        return {
          backgroundColor: customBgColor,
          color: '#000000',
        };
    }
  };

  // Handle image upload
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

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: BLOOMBERG_COLORS.DARK_BG, color: BLOOMBERG_COLORS.TEXT_PRIMARY }}>
      {/* Header */}
      <div className="flex items-center justify-between px-4 py-2 border-b" style={{ borderColor: BLOOMBERG_COLORS.BORDER, backgroundColor: BLOOMBERG_COLORS.PANEL_BG }}>
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2">
            <FileText className="w-5 h-5" style={{ color: BLOOMBERG_COLORS.ORANGE }} />
            <span className="font-bold text-sm" style={{ color: BLOOMBERG_COLORS.ORANGE }}>REPORT BUILDER</span>
          </div>
          <span className="text-xs" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
            {currentTime.toLocaleTimeString()}
          </span>
        </div>
        <div className="flex items-center gap-2">
          <button
            onClick={loadTemplate}
            className="px-3 py-1 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-1"
            style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
          >
            <FolderOpen size={14} />
            Load
          </button>
          <button
            onClick={saveTemplate}
            disabled={isSaving}
            className="px-3 py-1 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-1 disabled:opacity-50"
            style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
          >
            <Save size={14} />
            {isSaving ? 'Saving...' : 'Save'}
          </button>
          <button
            onClick={generatePdf}
            disabled={isGenerating || template.components.length === 0}
            className="px-3 py-1 text-xs rounded transition-colors flex items-center gap-1 disabled:opacity-50"
            style={{
              backgroundColor: BLOOMBERG_COLORS.ORANGE,
              color: BLOOMBERG_COLORS.BLACK,
              fontWeight: 'bold'
            }}
          >
            <Download size={14} />
            {isGenerating ? 'Generating...' : 'Export PDF'}
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left Panel - Components */}
        <div className="w-1/5 border-r overflow-y-auto" style={{ borderColor: BLOOMBERG_COLORS.BORDER, backgroundColor: BLOOMBERG_COLORS.PANEL_BG }}>
          <div className="p-3">
            {/* Add Components */}
            <div className="mb-4">
              <h3 className="text-xs font-bold mb-2" style={{ color: BLOOMBERG_COLORS.ORANGE }}>ADD COMPONENTS</h3>
              <div className="space-y-1">
                <button
                  onClick={() => addComponent('heading')}
                  className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <Type size={14} />
                  Heading
                </button>
                <button
                  onClick={() => addComponent('text')}
                  className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <AlignLeft size={14} />
                  Text
                </button>
                <button
                  onClick={() => addComponent('chart')}
                  className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <BarChart3 size={14} />
                  Chart
                </button>
                <button
                  onClick={() => addComponent('table')}
                  className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <Table size={14} />
                  Table
                </button>
                <button
                  onClick={() => addComponent('image')}
                  className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <ImageIcon size={14} />
                  Image
                </button>
                <button
                  onClick={() => addComponent('code')}
                  className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <Code size={14} />
                  Code Block
                </button>
                <button
                  onClick={() => addComponent('divider')}
                  className="w-full px-3 py-2 text-xs text-left rounded hover:bg-[#2a2a2a] transition-colors flex items-center gap-2"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <Minus size={14} />
                  Divider
                </button>
              </div>
            </div>

            {/* Layout Components */}
            <div className="mb-4">
              <h3 className="text-xs font-bold mb-2" style={{ color: BLOOMBERG_COLORS.ORANGE }}>LAYOUT</h3>
              <div className="grid grid-cols-2 gap-1">
                <button
                  onClick={() => addComponent('section')}
                  className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <Layout size={14} />
                  <span className="text-[9px]">Section</span>
                </button>
                <button
                  onClick={() => addComponent('columns')}
                  className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <ColumnsIcon size={14} />
                  <span className="text-[9px]">Columns</span>
                </button>
                <button
                  onClick={() => addComponent('coverpage')}
                  className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <BookOpen size={14} />
                  <span className="text-[9px]">Cover</span>
                </button>
                <button
                  onClick={() => addComponent('pagebreak')}
                  className="px-2 py-2 text-xs rounded hover:bg-[#2a2a2a] transition-colors flex flex-col items-center gap-1"
                  style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                >
                  <FileX size={14} />
                  <span className="text-[9px]">Break</span>
                </button>
              </div>
            </div>

            {/* Settings */}
            <div className="mb-4">
              <h3 className="text-xs font-bold mb-2" style={{ color: BLOOMBERG_COLORS.ORANGE }}>SETTINGS</h3>
              <div className="space-y-2">
                <div>
                  <label className="text-[9px] block mb-1" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                    Page Theme
                  </label>
                  <select
                    value={pageTheme}
                    onChange={(e) => setPageTheme(e.target.value as any)}
                    className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                    style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                  >
                    <option value="classic">Classic White</option>
                    <option value="minimal">Minimal Grid</option>
                    <option value="geometric">Geometric Triangles</option>
                    <option value="pattern">Diagonal Pattern</option>
                    <option value="modern">Modern Gradient (Dark)</option>
                    <option value="gradient">Professional Gradient (Dark)</option>
                  </select>
                </div>

                {/* Color Picker for Classic Theme */}
                {pageTheme === 'classic' && (
                  <div>
                    <div className="flex items-center justify-between mb-1">
                      <label className="text-[9px]" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                        Background Color
                      </label>
                      <button
                        onClick={() => setShowColorPicker(!showColorPicker)}
                        className="text-[9px] px-2 py-1 rounded hover:bg-[#2a2a2a]"
                        style={{ color: BLOOMBERG_COLORS.ORANGE }}
                      >
                        {showColorPicker ? 'Hide Picker' : 'Show Picker'}
                      </button>
                    </div>

                    {/* Current Color Display */}
                    <div className="flex items-center gap-2 mb-2">
                      <div
                        className="w-8 h-8 rounded border-2 border-[#333333]"
                        style={{ backgroundColor: customBgColor }}
                      />
                      <input
                        type="text"
                        value={customBgColor}
                        onChange={(e) => setCustomBgColor(e.target.value)}
                        className="flex-1 px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                        style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                        placeholder="#ffffff"
                      />
                    </div>

                    {/* Advanced Color Picker */}
                    {showColorPicker && (
                      <div className="p-3 rounded bg-[#0a0a0a] border border-[#333333] space-y-3">
                        {/* Main Gradient Palette */}
                        <div>
                          <label className="text-[9px] mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                            Color Palette
                          </label>
                          <div
                            className="w-full h-32 rounded cursor-crosshair border border-[#333333] relative"
                            style={{
                              background: `
                                linear-gradient(to bottom, transparent, black),
                                linear-gradient(to right, white, hsl(${customBgColor.startsWith('#') ? 0 : 120}, 100%, 50%))
                              `
                            }}
                            onClick={(e) => {
                              const rect = e.currentTarget.getBoundingClientRect();
                              const x = e.clientX - rect.left;
                              const y = e.clientY - rect.top;
                              const percentX = x / rect.width;
                              const percentY = y / rect.height;

                              // Calculate color based on position
                              const hue = percentX * 360;
                              const saturation = 100;
                              const lightness = 100 - (percentY * 50); // Top is white (100%), bottom is mid-dark (50%)

                              const hslToHex = (h: number, s: number, l: number) => {
                                l /= 100;
                                const a = s * Math.min(l, 1 - l) / 100;
                                const f = (n: number) => {
                                  const k = (n + h / 30) % 12;
                                  const color = l - a * Math.max(Math.min(k - 3, 9 - k, 1), -1);
                                  return Math.round(255 * color).toString(16).padStart(2, '0');
                                };
                                return `#${f(0)}${f(8)}${f(4)}`;
                              };

                              setCustomBgColor(hslToHex(hue, saturation, lightness));
                            }}
                          >
                            {/* Current color indicator */}
                            <div
                              className="absolute w-3 h-3 border-2 border-white rounded-full shadow-lg pointer-events-none"
                              style={{
                                left: '50%',
                                top: '10%',
                                transform: 'translate(-50%, -50%)'
                              }}
                            />
                          </div>
                        </div>

                        {/* Hue Slider */}
                        <div>
                          <label className="text-[9px] mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                            Hue
                          </label>
                          <div
                            className="w-full h-6 rounded cursor-pointer border border-[#333333]"
                            style={{
                              background: 'linear-gradient(to right, #ff0000, #ffff00, #00ff00, #00ffff, #0000ff, #ff00ff, #ff0000)'
                            }}
                            onClick={(e) => {
                              const rect = e.currentTarget.getBoundingClientRect();
                              const x = e.clientX - rect.left;
                              const percent = x / rect.width;
                              const hue = Math.round(percent * 360);

                              const hslToHex = (h: number, s: number, l: number) => {
                                l /= 100;
                                const a = s * Math.min(l, 1 - l) / 100;
                                const f = (n: number) => {
                                  const k = (n + h / 30) % 12;
                                  const color = l - a * Math.max(Math.min(k - 3, 9 - k, 1), -1);
                                  return Math.round(255 * color).toString(16).padStart(2, '0');
                                };
                                return `#${f(0)}${f(8)}${f(4)}`;
                              };

                              setCustomBgColor(hslToHex(hue, 100, 50));
                            }}
                          />
                        </div>

                        {/* Lightness Slider */}
                        <div>
                          <label className="text-[9px] mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                            Lightness
                          </label>
                          <div
                            className="w-full h-6 rounded cursor-pointer border border-[#333333]"
                            style={{
                              background: 'linear-gradient(to right, #000000, #808080, #ffffff)'
                            }}
                            onClick={(e) => {
                              const rect = e.currentTarget.getBoundingClientRect();
                              const x = e.clientX - rect.left;
                              const percent = x / rect.width;
                              const lightness = Math.round(percent * 100);

                              const hslToHex = (h: number, s: number, l: number) => {
                                l /= 100;
                                const a = s * Math.min(l, 1 - l) / 100;
                                const f = (n: number) => {
                                  const k = (n + h / 30) % 12;
                                  const color = l - a * Math.max(Math.min(k - 3, 9 - k, 1), -1);
                                  return Math.round(255 * color).toString(16).padStart(2, '0');
                                };
                                return `#${f(0)}${f(8)}${f(4)}`;
                              };

                              setCustomBgColor(hslToHex(120, 0, lightness));
                            }}
                          />
                        </div>

                        {/* Preset Colors */}
                        <div>
                          <label className="text-[9px] mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                            Preset Colors
                          </label>
                          <div className="grid grid-cols-8 gap-1">
                            {[
                              '#ffffff', '#f8f9fa', '#e9ecef', '#dee2e6', '#ced4da', '#adb5bd', '#6c757d', '#495057',
                              '#fff3cd', '#d1ecf1', '#d4edda', '#f8d7da', '#e2e3e5', '#d6d8db', '#fef5e7', '#ebf5fb',
                              '#ff0000', '#ff7f00', '#ffff00', '#00ff00', '#00ffff', '#0000ff', '#8b00ff', '#ff00ff',
                              '#fff5f5', '#ffe6e6', '#ffd6cc', '#ffe5cc', '#fff4cc', '#ffffcc', '#f0fff4', '#f0f9ff'
                            ].map(color => (
                              <button
                                key={color}
                                onClick={() => setCustomBgColor(color)}
                                className={`w-full aspect-square rounded border-2 transition-all hover:scale-110 ${
                                  customBgColor.toLowerCase() === color.toLowerCase()
                                    ? 'border-[#FFA500] ring-2 ring-[#FFA500]'
                                    : 'border-[#333333] hover:border-[#666]'
                                }`}
                                style={{ backgroundColor: color }}
                                title={color}
                              />
                            ))}
                          </div>
                        </div>
                      </div>
                    )}
                  </div>
                )}
                <div>
                  <label className="text-[9px] block mb-1" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                    Report Title
                  </label>
                  <input
                    type="text"
                    value={template.metadata.title}
                    onChange={(e) => setTemplate(prev => ({
                      ...prev,
                      metadata: { ...prev.metadata, title: e.target.value }
                    }))}
                    className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                    style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                    placeholder="Enter report title..."
                  />
                </div>
                <div>
                  <label className="text-[9px] block mb-1" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                    Author
                  </label>
                  <input
                    type="text"
                    value={template.metadata.author}
                    onChange={(e) => setTemplate(prev => ({
                      ...prev,
                      metadata: { ...prev.metadata, author: e.target.value }
                    }))}
                    className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                    style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                    placeholder="Enter author name..."
                  />
                </div>
                <div>
                  <label className="text-[9px] block mb-1" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                    Company
                  </label>
                  <input
                    type="text"
                    value={template.metadata.company}
                    onChange={(e) => setTemplate(prev => ({
                      ...prev,
                      metadata: { ...prev.metadata, company: e.target.value }
                    }))}
                    className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                    style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                    placeholder="Enter company name..."
                  />
                </div>
                <div>
                  <label className="text-[9px] block mb-1" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                    Font Family
                  </label>
                  <select
                    value={fontFamily}
                    onChange={(e) => setFontFamily(e.target.value)}
                    className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                    style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                  >
                    <option value="Arial, sans-serif">Arial</option>
                    <option value="'Times New Roman', serif">Times New Roman</option>
                    <option value="Georgia, serif">Georgia</option>
                    <option value="'Courier New', monospace">Courier New</option>
                    <option value="Verdana, sans-serif">Verdana</option>
                    <option value="'Trebuchet MS', sans-serif">Trebuchet MS</option>
                    <option value="'Palatino Linotype', serif">Palatino</option>
                    <option value="Garamond, serif">Garamond</option>
                    <option value="'Comic Sans MS', cursive">Comic Sans MS</option>
                    <option value="'Lucida Console', monospace">Lucida Console</option>
                  </select>
                </div>
                <div>
                  <label className="text-[9px] block mb-1" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                    Base Font Size
                  </label>
                  <div className="flex items-center gap-2">
                    <input
                      type="range"
                      min="8"
                      max="16"
                      value={defaultFontSize}
                      onChange={(e) => setDefaultFontSize(Number(e.target.value))}
                      className="flex-1"
                      style={{ accentColor: BLOOMBERG_COLORS.ORANGE }}
                    />
                    <span className="text-xs" style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY, minWidth: '30px' }}>
                      {defaultFontSize}pt
                    </span>
                  </div>
                </div>
                <div>
                  <label className="text-[9px] block mb-1" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                    Text Style
                  </label>
                  <div className="flex gap-2">
                    <button
                      onClick={() => setIsBold(!isBold)}
                      className={`flex-1 px-3 py-2 rounded border transition-all ${
                        isBold
                          ? 'bg-[#FFA500] border-[#FFA500] text-black'
                          : 'bg-[#0a0a0a] border-[#333333] text-gray-400 hover:border-[#FFA500]'
                      }`}
                      title="Toggle Bold"
                    >
                      <Bold size={16} className="mx-auto" />
                    </button>
                    <button
                      onClick={() => setIsItalic(!isItalic)}
                      className={`flex-1 px-3 py-2 rounded border transition-all ${
                        isItalic
                          ? 'bg-[#FFA500] border-[#FFA500] text-black'
                          : 'bg-[#0a0a0a] border-[#333333] text-gray-400 hover:border-[#FFA500]'
                      }`}
                      title="Toggle Italic"
                    >
                      <Italic size={16} className="mx-auto" />
                    </button>
                  </div>
                </div>
              </div>
            </div>

            {/* Document Structure */}
            <div>
              <h3 className="text-xs font-bold mb-2" style={{ color: BLOOMBERG_COLORS.ORANGE }}>DOCUMENT STRUCTURE</h3>
              <div className="space-y-0">
                {template.components.length === 0 ? (
                  <p className="text-xs text-center py-4" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                    No components yet
                  </p>
                ) : (
                  <DndContext
                    sensors={sensors}
                    collisionDetection={closestCenter}
                    onDragEnd={handleDragEnd}
                  >
                    <SortableContext
                      items={template.components.map(c => c.id)}
                      strategy={verticalListSortingStrategy}
                    >
                      {template.components.map((component) => (
                        <SortableComponent
                          key={component.id}
                          component={component}
                          isSelected={selectedComponent === component.id}
                          onClick={() => setSelectedComponent(component.id)}
                          onDelete={() => deleteComponent(component.id)}
                          onDuplicate={() => duplicateComponent(component.id)}
                        />
                      ))}
                    </SortableContext>
                  </DndContext>
                )}
              </div>
            </div>
          </div>
        </div>

        {/* Center - Canvas */}
        <div className="w-3/5 overflow-y-auto p-6" style={{ backgroundColor: '#0f0f0f' }}>
          <div
            className="mx-auto shadow-lg"
            style={{
              width: '210mm',
              minHeight: '297mm',
              padding: '20mm',
              fontFamily: fontFamily,
              fontSize: `${defaultFontSize}pt`,
              fontWeight: isBold ? 'bold' : 'normal',
              fontStyle: isItalic ? 'italic' : 'normal',
              ...getThemeBackground(),
            }}
          >
            {/* Document Metadata */}
            <div className="mb-8 border-b border-gray-300 pb-4">
              <h1 className="text-3xl font-bold mb-2">{template.metadata.title}</h1>
              {template.metadata.company && (
                <p className="text-lg text-gray-600">{template.metadata.company}</p>
              )}
              {template.metadata.author && (
                <p className="text-sm text-gray-500">By {template.metadata.author}</p>
              )}
              <p className="text-sm text-gray-500">{template.metadata.date}</p>
            </div>

            {/* Components */}
            {template.components.length === 0 ? (
              <div className="text-center py-16 text-gray-400">
                <FileText className="w-16 h-16 mx-auto mb-4 opacity-30" />
                <p className="text-lg">Your report is empty</p>
                <p className="text-sm mt-2">Add components from the left panel to start building your report</p>
              </div>
            ) : (
              <div className="space-y-4">
                {template.components.map((component) => {
                  const isSelected = selectedComponent === component.id;
                  return (
                    <div
                      key={component.id}
                      onClick={() => setSelectedComponent(component.id)}
                      className={`cursor-pointer transition-all ${
                        isSelected ? 'ring-2 ring-orange-500 ring-offset-2' : 'hover:ring-1 hover:ring-gray-300'
                      }`}
                    >
                      {/* Render component based on type */}
                      {component.type === 'heading' && (
                        <h2 className={`font-bold text-2xl text-${component.config.alignment || 'left'}`}>
                          {component.content || 'Heading'}
                        </h2>
                      )}
                      {component.type === 'text' && (
                        <p className={`text-${component.config.alignment || 'left'}`}>
                          {component.content || 'Text content'}
                        </p>
                      )}
                      {component.type === 'chart' && (
                        <div className="border border-gray-300 rounded p-8 bg-gray-50 text-center">
                          <BarChart3 className="w-16 h-16 mx-auto text-gray-400" />
                          <p className="text-sm text-gray-500 mt-2">Chart Placeholder</p>
                        </div>
                      )}
                      {component.type === 'table' && (
                        <div className="border border-gray-300 rounded overflow-hidden">
                          <table className="w-full">
                            <thead className="bg-gray-100">
                              <tr>
                                {(component.config.columns || ['Column 1', 'Column 2', 'Column 3']).map((col, idx) => (
                                  <th key={idx} className="px-4 py-2 text-left text-sm font-semibold">{col}</th>
                                ))}
                              </tr>
                            </thead>
                            <tbody>
                              <tr className="border-t border-gray-200">
                                {(component.config.columns || ['Column 1', 'Column 2', 'Column 3']).map((_, idx) => (
                                  <td key={idx} className="px-4 py-2 text-sm">Sample data</td>
                                ))}
                              </tr>
                            </tbody>
                          </table>
                        </div>
                      )}
                      {component.type === 'image' && (
                        <div className="border border-gray-300 rounded p-4 bg-gray-50">
                          {component.config.imageUrl ? (
                            <img
                              src={`file://${component.config.imageUrl}`}
                              alt="Report Image"
                              className="max-w-full h-auto"
                            />
                          ) : (
                            <div className="text-center py-8 text-gray-400">
                              <ImageIcon className="w-12 h-12 mx-auto mb-2" />
                              <p className="text-sm">No image selected</p>
                            </div>
                          )}
                        </div>
                      )}
                      {component.type === 'code' && (
                        <pre className="bg-gray-900 text-green-400 p-4 rounded overflow-x-auto text-sm font-mono">
                          <code>{component.content || '// Code block'}</code>
                        </pre>
                      )}
                      {component.type === 'divider' && (
                        <hr className="border-t-2 border-gray-300 my-4" />
                      )}
                      {component.type === 'section' && (
                        <div className="border-l-4 p-4 my-4" style={{
                          borderColor: component.config.borderColor || '#FFA500',
                          backgroundColor: component.config.backgroundColor || '#f8f9fa'
                        }}>
                          <h3 className="font-bold text-lg mb-2" style={{ color: component.config.borderColor || '#FFA500' }}>
                            {component.config.sectionTitle || 'SECTION'}
                          </h3>
                          <p className="text-sm text-gray-600">Section content area - add child components here</p>
                        </div>
                      )}
                      {component.type === 'columns' && (
                        <div className={`grid gap-4 my-4 ${component.config.columnCount === 3 ? 'grid-cols-3' : 'grid-cols-2'}`}>
                          {Array.from({ length: component.config.columnCount || 2 }).map((_, idx) => (
                            <div key={idx} className="border border-gray-300 rounded p-4 bg-gray-50">
                              <p className="text-sm text-gray-500 text-center">Column {idx + 1}</p>
                            </div>
                          ))}
                        </div>
                      )}
                      {component.type === 'coverpage' && (
                        <div className="min-h-[400px] flex flex-col items-center justify-center text-center p-12" style={{
                          backgroundColor: component.config.backgroundColor || '#0a0a0a',
                          color: '#FFA500'
                        }}>
                          <h1 className="text-4xl font-bold mb-4">{template.metadata.title}</h1>
                          {component.config.subtitle && (
                            <p className="text-xl mb-8">{component.config.subtitle}</p>
                          )}
                          <div className="text-sm text-gray-400 mt-auto">
                            {template.metadata.company}<br />
                            {template.metadata.author}<br />
                            {template.metadata.date}
                          </div>
                        </div>
                      )}
                      {component.type === 'pagebreak' && (
                        <div className="my-8 py-4 border-t-2 border-dashed border-gray-400 text-center">
                          <span className="text-xs text-gray-400 bg-white px-3 py-1 rounded-full">Page Break</span>
                        </div>
                      )}
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        </div>

        {/* Right Panel - Properties and AI Chat */}
        <div className="w-1/5 border-l flex flex-col" style={{ borderColor: BLOOMBERG_COLORS.BORDER, backgroundColor: BLOOMBERG_COLORS.PANEL_BG }}>
          {/* Properties Section - Top Half */}
          <div className="flex-1 overflow-y-auto border-b" style={{ borderColor: BLOOMBERG_COLORS.BORDER }}>
            {selectedComp ? (
              <div className="p-4">
                <div className="flex items-center justify-between mb-4">
                  <h3 className="text-sm font-bold" style={{ color: BLOOMBERG_COLORS.ORANGE }}>PROPERTIES</h3>
                  <button
                    onClick={() => setSelectedComponent(null)}
                    className="p-1 hover:bg-[#2a2a2a] rounded"
                    style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}
                  >
                    <X size={16} />
                  </button>
                </div>

                <div className="space-y-4">
                {/* Component Type */}
                <div>
                  <label className="text-xs font-semibold mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                    Type
                  </label>
                  <div className="px-3 py-2 text-xs rounded bg-[#0a0a0a] capitalize">
                    {selectedComp.type}
                  </div>
                </div>

                {/* Content Editor */}
                {(selectedComp.type === 'heading' || selectedComp.type === 'text' || selectedComp.type === 'code') && (
                  <div>
                    <label className="text-xs font-semibold mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                      Content
                    </label>
                    <textarea
                      value={selectedComp.content || ''}
                      onChange={(e) => updateComponent(selectedComp.id, { content: e.target.value })}
                      className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                      style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                      rows={selectedComp.type === 'code' ? 10 : 4}
                      placeholder="Enter content..."
                    />
                  </div>
                )}

                {/* Alignment */}
                {(selectedComp.type === 'heading' || selectedComp.type === 'text') && (
                  <div>
                    <label className="text-xs font-semibold mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                      Alignment
                    </label>
                    <div className="flex gap-2">
                      {['left', 'center', 'right'].map((align) => (
                        <button
                          key={align}
                          onClick={() => updateComponent(selectedComp.id, {
                            config: { ...selectedComp.config, alignment: align as any }
                          })}
                          className={`flex-1 px-3 py-2 text-xs rounded transition-colors capitalize ${
                            selectedComp.config.alignment === align
                              ? 'bg-[#FFA500] text-black'
                              : 'bg-[#0a0a0a] hover:bg-[#2a2a2a]'
                          }`}
                        >
                          {align}
                        </button>
                      ))}
                    </div>
                  </div>
                )}

                {/* Font Size */}
                {(selectedComp.type === 'heading' || selectedComp.type === 'text') && (
                  <div>
                    <label className="text-xs font-semibold mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                      Font Size
                    </label>
                    <select
                      value={selectedComp.config.fontSize || 'base'}
                      onChange={(e) => updateComponent(selectedComp.id, {
                        config: { ...selectedComp.config, fontSize: e.target.value }
                      })}
                      className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                      style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                    >
                      <option value="xs">Extra Small</option>
                      <option value="sm">Small</option>
                      <option value="base">Base</option>
                      <option value="lg">Large</option>
                      <option value="xl">Extra Large</option>
                      <option value="2xl">2X Large</option>
                      <option value="3xl">3X Large</option>
                    </select>
                  </div>
                )}

                {/* Image Upload */}
                {selectedComp.type === 'image' && (
                  <div>
                    <label className="text-xs font-semibold mb-2 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                      Image
                    </label>
                    {selectedComp.config.imageUrl ? (
                      <div className="space-y-2">
                        <div className="text-xs text-gray-400 break-all bg-[#0a0a0a] p-2 rounded">
                          {selectedComp.config.imageUrl.split('\\').pop()}
                        </div>
                        <div className="flex gap-2">
                          <button
                            onClick={handleImageUpload}
                            className="flex-1 px-3 py-2 text-xs rounded bg-blue-600 hover:bg-blue-700 transition-colors"
                          >
                            Change Image
                          </button>
                          <button
                            onClick={() => updateComponent(selectedComp.id, {
                              config: { ...selectedComp.config, imageUrl: undefined }
                            })}
                            className="flex-1 px-3 py-2 text-xs rounded bg-red-600 hover:bg-red-700 transition-colors"
                          >
                            Remove
                          </button>
                        </div>
                      </div>
                    ) : (
                      <button
                        onClick={handleImageUpload}
                        className="w-full px-3 py-2 text-xs rounded transition-colors flex items-center justify-center gap-2"
                        style={{ backgroundColor: BLOOMBERG_COLORS.ORANGE, color: BLOOMBERG_COLORS.BLACK }}
                      >
                        <Plus size={14} />
                        Upload Image
                      </button>
                    )}
                  </div>
                )}

                {/* Table Columns */}
                {selectedComp.type === 'table' && (
                  <div>
                    <label className="text-xs font-semibold mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                      Columns (comma-separated)
                    </label>
                    <input
                      type="text"
                      value={(selectedComp.config.columns || []).join(', ')}
                      onChange={(e) => updateComponent(selectedComp.id, {
                        config: { ...selectedComp.config, columns: e.target.value.split(',').map(s => s.trim()) }
                      })}
                      className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                      style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                      placeholder="Column 1, Column 2, Column 3"
                    />
                  </div>
                )}

                {/* Chart Type */}
                {selectedComp.type === 'chart' && (
                  <div>
                    <label className="text-xs font-semibold mb-1 block" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                      Chart Type
                    </label>
                    <select
                      value={selectedComp.config.chartType || 'bar'}
                      onChange={(e) => updateComponent(selectedComp.id, {
                        config: { ...selectedComp.config, chartType: e.target.value }
                      })}
                      className="w-full px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
                      style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
                    >
                      <option value="bar">Bar Chart</option>
                      <option value="line">Line Chart</option>
                      <option value="pie">Pie Chart</option>
                      <option value="area">Area Chart</option>
                    </select>
                  </div>
                )}

                {/* Actions */}
                <div className="pt-4 border-t" style={{ borderColor: BLOOMBERG_COLORS.BORDER }}>
                  <div className="flex gap-2">
                    <button
                      onClick={() => duplicateComponent(selectedComp.id)}
                      className="flex-1 px-3 py-2 text-xs rounded bg-[#0a0a0a] hover:bg-[#2a2a2a] transition-colors flex items-center justify-center gap-2"
                    >
                      <Copy size={14} />
                      Duplicate
                    </button>
                    <button
                      onClick={() => deleteComponent(selectedComp.id)}
                      className="flex-1 px-3 py-2 text-xs rounded bg-red-600 hover:bg-red-700 transition-colors flex items-center justify-center gap-2"
                    >
                      <Trash2 size={14} />
                      Delete
                    </button>
                  </div>
                </div>
              </div>
              </div>
            ) : (
              <div className="p-4 text-center" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                <SettingsIcon size={32} className="mx-auto mb-2 opacity-30" />
                <p className="text-xs">Select a component to edit properties</p>
              </div>
            )}
          </div>

          {/* AI Writing Assistant Section - Bottom Half */}
          <div className="flex-1 flex flex-col" style={{ backgroundColor: BLOOMBERG_COLORS.PANEL_BG }}>
        {/* AI Assistant Header */}
        <div className="p-4 border-b border-[#333333]">
          <div className="flex items-center justify-between mb-3">
            <div className="flex items-center gap-2">
              <Bot size={18} style={{ color: BLOOMBERG_COLORS.ORANGE }} />
              <h3 className="text-sm font-bold" style={{ color: BLOOMBERG_COLORS.ORANGE }}>
                AI WRITING ASSISTANT
              </h3>
            </div>
            <Sparkles size={16} style={{ color: BLOOMBERG_COLORS.ORANGE }} />
          </div>

          {/* Model Selector */}
          <div className="mb-3">
            <label className="text-[9px] block mb-1" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
              AI Model
            </label>
            <select
              value={currentProvider}
              onChange={(e) => setCurrentProvider(e.target.value)}
              className="w-full px-2 py-1 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
              style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
            >
              {availableModels.map(model => (
                <option key={model.id} value={model.provider}>
                  {model.name} ({model.provider})
                </option>
              ))}
              {availableModels.length === 0 && (
                <option value="ollama">Configure in Settings</option>
              )}
            </select>
          </div>

          {/* Quick Actions */}
          <div className="grid grid-cols-2 gap-1">
            <button
              onClick={() => handleAiQuickAction('improve')}
              className="px-2 py-1 text-[9px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors"
              style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
            >
              ✨ Improve
            </button>
            <button
              onClick={() => handleAiQuickAction('expand')}
              className="px-2 py-1 text-[9px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors"
              style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
            >
              📝 Expand
            </button>
            <button
              onClick={() => handleAiQuickAction('summarize')}
              className="px-2 py-1 text-[9px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors"
              style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
            >
              📋 Summary
            </button>
            <button
              onClick={() => handleAiQuickAction('grammar')}
              className="px-2 py-1 text-[9px] rounded bg-[#0a0a0a] border border-[#333333] hover:border-[#FFA500] transition-colors"
              style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
            >
              ✓ Grammar
            </button>
          </div>
        </div>

        {/* AI Chat Messages */}
        <div className="flex-1 overflow-y-auto p-4 space-y-3">
          {aiMessages.length === 0 && (
            <div className="text-center py-8">
              <Bot size={32} className="mx-auto mb-3" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }} />
              <p className="text-xs" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                Ask me to help you write better reports!
              </p>
              <p className="text-[9px] mt-2" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                Try: "Help me write an introduction"
              </p>
            </div>
          )}

          {aiMessages.map((msg, idx) => (
            <div key={idx} className={`flex gap-2 ${msg.role === 'user' ? 'justify-end' : 'justify-start'}`}>
              {msg.role === 'assistant' && (
                <div className="flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center" style={{ backgroundColor: BLOOMBERG_COLORS.ORANGE }}>
                  <Bot size={14} style={{ color: '#000' }} />
                </div>
              )}
              <div
                className={`max-w-[85%] rounded-lg p-3 ${
                  msg.role === 'user'
                    ? 'bg-[#FFA500] text-black'
                    : 'bg-[#1a1a1a] border border-[#333333]'
                }`}
              >
                <div className="text-xs" style={{ color: msg.role === 'user' ? '#000' : BLOOMBERG_COLORS.TEXT_PRIMARY }}>
                  {msg.role === 'assistant' ? (
                    <MarkdownRenderer content={msg.content} />
                  ) : (
                    msg.content
                  )}
                </div>
              </div>
              {msg.role === 'user' && (
                <div className="flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center bg-[#333333]">
                  <User size={14} style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }} />
                </div>
              )}
            </div>
          ))}

          {isAiTyping && streamingContent && (
            <div className="flex gap-2 justify-start">
              <div className="flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center" style={{ backgroundColor: BLOOMBERG_COLORS.ORANGE }}>
                <Bot size={14} style={{ color: '#000' }} />
              </div>
              <div className="max-w-[85%] rounded-lg p-3 bg-[#1a1a1a] border border-[#333333]">
                <div className="text-xs" style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}>
                  <MarkdownRenderer content={streamingContent} />
                </div>
              </div>
            </div>
          )}

          {isAiTyping && !streamingContent && (
            <div className="flex gap-2 justify-start">
              <div className="flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center" style={{ backgroundColor: BLOOMBERG_COLORS.ORANGE }}>
                <Bot size={14} style={{ color: '#000' }} />
              </div>
              <div className="max-w-[85%] rounded-lg p-3 bg-[#1a1a1a] border border-[#333333]">
                <div className="text-xs flex gap-1" style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
                  <span className="animate-bounce">●</span>
                  <span className="animate-bounce delay-100">●</span>
                  <span className="animate-bounce delay-200">●</span>
                </div>
              </div>
            </div>
          )}

          <div ref={aiMessagesEndRef} />
        </div>

        {/* AI Input */}
        <div className="p-4 border-t border-[#333333]">
          <div className="flex gap-2">
            <input
              type="text"
              value={aiInput}
              onChange={(e) => setAiInput(e.target.value)}
              onKeyPress={(e) => e.key === 'Enter' && handleAiSendMessage()}
              placeholder="Ask AI to help with your report..."
              className="flex-1 px-3 py-2 text-xs rounded bg-[#0a0a0a] border border-[#333333] focus:border-[#FFA500] outline-none"
              style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}
              disabled={isAiTyping}
            />
            <button
              onClick={handleAiSendMessage}
              disabled={isAiTyping || !aiInput.trim()}
              className="px-3 py-2 rounded transition-all disabled:opacity-50 disabled:cursor-not-allowed"
              style={{
                backgroundColor: BLOOMBERG_COLORS.ORANGE,
                color: '#000'
              }}
            >
              <Send size={16} />
            </button>
          </div>
        </div>
        </div>
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

      {/* Footer */}
      <div style={{
        borderTop: `2px solid ${BLOOMBERG_COLORS.ORANGE}`,
        padding: '8px 16px',
        background: `linear-gradient(180deg, ${BLOOMBERG_COLORS.PANEL_BG} 0%, #0a0a0a 100%)`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0,
        flexWrap: 'wrap',
        gap: '12px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '9px' }}>
          <span style={{ color: BLOOMBERG_COLORS.ORANGE, fontWeight: 'bold' }}>REPORT BUILDER v1.0.0</span>
          <span style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>|</span>
          <span style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}>Components: {template.components.length}</span>
          <span style={{ color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>|</span>
          <span style={{ color: BLOOMBERG_COLORS.TEXT_PRIMARY }}>Theme: {pageTheme.charAt(0).toUpperCase() + pageTheme.slice(1)}</span>
        </div>
        <div style={{ fontSize: '9px', color: BLOOMBERG_COLORS.TEXT_SECONDARY }}>
          {currentTime.toLocaleTimeString()} | PDF Generator: Rust printpdf
        </div>
      </div>
    </div>
  );
};

export default ReportBuilderTab;
