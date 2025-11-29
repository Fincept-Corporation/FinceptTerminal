import React, { useState, useCallback, useRef } from 'react';
import {
  FileText,
  Download,
  Save,
  Layout,
  Type,
  BarChart3,
  Table,
  Image as ImageIcon,
  Plus,
  Trash2,
  Eye,
  Settings,
  Copy,
  AlignLeft,
  Grid,
  Layers,
  Code
} from 'lucide-react';
import { DndContext, closestCenter, KeyboardSensor, PointerSensor, useSensor, useSensors, DragEndEvent } from '@dnd-kit/core';
import { arrayMove, SortableContext, sortableKeyboardCoordinates, useSortable, verticalListSortingStrategy } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import { reportService, ReportTemplate, ReportComponent } from '@/services/reportService';
import { save } from '@tauri-apps/plugin-dialog';
import { toast } from 'sonner';

// Sortable component wrapper
function SortableComponent({ component, onEdit, onDelete, onDuplicate }: {
  component: ReportComponent;
  onEdit: (id: string) => void;
  onDelete: (id: string) => void;
  onDuplicate: (id: string) => void;
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

  const renderComponentPreview = () => {
    switch (component.type) {
      case 'heading':
        return (
          <h2 className={`font-bold text-${component.config.fontSize || '2xl'} text-${component.config.alignment || 'left'}`}>
            {component.content || 'Heading'}
          </h2>
        );
      case 'text':
        return (
          <p className={`text-${component.config.alignment || 'left'}`}>
            {component.content || 'Text content goes here...'}
          </p>
        );
      case 'chart':
        return (
          <div className="border border-gray-600 rounded p-4 bg-gray-800/50">
            <BarChart3 className="w-full h-32 text-blue-400" />
            <p className="text-center text-sm text-gray-400 mt-2">
              {component.config.chartType || 'Chart'} Preview
            </p>
          </div>
        );
      case 'table':
        return (
          <div className="border border-gray-600 rounded overflow-hidden">
            <table className="w-full">
              <thead className="bg-gray-700">
                <tr>
                  {(component.config.columns || ['Column 1', 'Column 2', 'Column 3']).map((col, idx) => (
                    <th key={idx} className="px-4 py-2 text-left text-sm">{col}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                <tr className="border-t border-gray-600">
                  {(component.config.columns || ['Column 1', 'Column 2', 'Column 3']).map((_, idx) => (
                    <td key={idx} className="px-4 py-2 text-sm text-gray-400">Sample data</td>
                  ))}
                </tr>
              </tbody>
            </table>
          </div>
        );
      case 'image':
        return (
          <div className="border border-gray-600 rounded p-8 bg-gray-800/50 flex items-center justify-center">
            <ImageIcon className="w-16 h-16 text-gray-500" />
          </div>
        );
      case 'divider':
        return <hr className="border-gray-600" />;
      case 'code':
        return (
          <pre className="bg-gray-900 border border-gray-600 rounded p-4 overflow-x-auto">
            <code className="text-sm text-green-400">
              {component.content || '# Code block'}
            </code>
          </pre>
        );
      default:
        return <div className="text-gray-400">Unknown component type</div>;
    }
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      className="bg-gray-800/30 border border-gray-700 rounded-lg p-4 mb-3 hover:border-blue-500 transition-colors group"
    >
      <div className="flex items-center justify-between mb-2">
        <div className="flex items-center gap-2">
          <div {...attributes} {...listeners} className="cursor-move p-1 hover:bg-gray-700 rounded">
            <Grid className="w-4 h-4 text-gray-400" />
          </div>
          <span className="text-sm font-medium capitalize">{component.type}</span>
        </div>
        <div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
          <button
            onClick={() => onEdit(component.id)}
            className="p-1 hover:bg-gray-700 rounded"
            title="Edit"
          >
            <Settings className="w-4 h-4" />
          </button>
          <button
            onClick={() => onDuplicate(component.id)}
            className="p-1 hover:bg-gray-700 rounded"
            title="Duplicate"
          >
            <Copy className="w-4 h-4" />
          </button>
          <button
            onClick={() => onDelete(component.id)}
            className="p-1 hover:bg-red-700 rounded text-red-400"
            title="Delete"
          >
            <Trash2 className="w-4 h-4" />
          </button>
        </div>
      </div>
      <div className="pointer-events-none">
        {renderComponentPreview()}
      </div>
    </div>
  );
}

const ReportBuilderTab: React.FC = () => {
  const [template, setTemplate] = useState<ReportTemplate>({
    id: 'new-report',
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
  const [showPreview, setShowPreview] = useState(false);
  const [isGenerating, setIsGenerating] = useState(false);

  const sensors = useSensors(
    useSensor(PointerSensor),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    })
  );

  const addComponent = (type: ReportComponent['type']) => {
    const newComponent: ReportComponent = {
      id: `component-${Date.now()}`,
      type,
      content: '',
      config: {
        fontSize: type === 'heading' ? '2xl' : 'base',
        alignment: 'left',
      },
    };

    setTemplate(prev => ({
      ...prev,
      components: [...prev.components, newComponent],
    }));
  };

  const deleteComponent = (id: string) => {
    setTemplate(prev => ({
      ...prev,
      components: prev.components.filter(c => c.id !== id),
    }));
  };

  const duplicateComponent = (id: string) => {
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
    }
  };

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

  const updateComponent = (id: string, updates: Partial<ReportComponent>) => {
    setTemplate(prev => ({
      ...prev,
      components: prev.components.map(c =>
        c.id === id ? { ...c, ...updates } : c
      ),
    }));
  };

  const generatePDF = async () => {
    setIsGenerating(true);
    try {
      // Show file save dialog
      const outputPath = await save({
        filters: [{
          name: 'PDF',
          extensions: ['pdf']
        }],
        defaultPath: `${template.name.replace(/\s+/g, '-')}.pdf`
      });

      if (!outputPath) {
        setIsGenerating(false);
        return;
      }

      // Generate PDF using Jinja2
      const result = await reportService.generatePDF(template, outputPath);

      if (result.success) {
        toast.success('PDF generated successfully!', {
          description: `Saved to: ${result.output_path}`
        });
      } else {
        toast.error('Failed to generate PDF', {
          description: result.error || 'Unknown error'
        });
      }
    } catch (error) {
      console.error('Failed to generate PDF:', error);
      toast.error('Failed to generate PDF', {
        description: error instanceof Error ? error.message : 'Unknown error'
      });
    } finally {
      setIsGenerating(false);
    }
  };

  const saveTemplate = () => {
    const json = JSON.stringify(template, null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${template.name.replace(/\s+/g, '-')}.json`;
    document.body.appendChild(a);
    a.click();
    window.URL.revokeObjectURL(url);
    document.body.removeChild(a);
  };

  const selectedComp = selectedComponent
    ? template.components.find(c => c.id === selectedComponent)
    : null;

  return (
    <div className="h-full flex flex-col bg-gray-900 text-white">
      {/* Header */}
      <div className="flex items-center justify-between p-4 border-b border-gray-700">
        <div className="flex items-center gap-3">
          <FileText className="w-6 h-6 text-blue-400" />
          <div>
            <h1 className="text-xl font-bold">{template.name}</h1>
            <p className="text-sm text-gray-400">{template.description}</p>
          </div>
        </div>
        <div className="flex gap-2">
          <button
            onClick={() => setShowPreview(!showPreview)}
            className="px-4 py-2 bg-gray-700 hover:bg-gray-600 rounded-lg flex items-center gap-2"
          >
            <Eye className="w-4 h-4" />
            {showPreview ? 'Edit' : 'Preview'}
          </button>
          <button
            onClick={saveTemplate}
            className="px-4 py-2 bg-gray-700 hover:bg-gray-600 rounded-lg flex items-center gap-2"
          >
            <Save className="w-4 h-4" />
            Save Template
          </button>
          <button
            onClick={generatePDF}
            disabled={isGenerating}
            className="px-4 py-2 bg-blue-600 hover:bg-blue-700 rounded-lg flex items-center gap-2 disabled:opacity-50"
          >
            <Download className="w-4 h-4" />
            {isGenerating ? 'Generating...' : 'Generate PDF'}
          </button>
        </div>
      </div>

      <div className="flex-1 flex overflow-hidden">
        {/* Component Palette */}
        <div className="w-64 border-r border-gray-700 p-4 overflow-y-auto">
          <h2 className="text-sm font-semibold mb-3 flex items-center gap-2">
            <Layers className="w-4 h-4" />
            Components
          </h2>
          <div className="space-y-2">
            <button
              onClick={() => addComponent('heading')}
              className="w-full px-3 py-2 bg-gray-800 hover:bg-gray-700 rounded flex items-center gap-2 text-sm"
            >
              <Type className="w-4 h-4" />
              Heading
            </button>
            <button
              onClick={() => addComponent('text')}
              className="w-full px-3 py-2 bg-gray-800 hover:bg-gray-700 rounded flex items-center gap-2 text-sm"
            >
              <AlignLeft className="w-4 h-4" />
              Text Block
            </button>
            <button
              onClick={() => addComponent('chart')}
              className="w-full px-3 py-2 bg-gray-800 hover:bg-gray-700 rounded flex items-center gap-2 text-sm"
            >
              <BarChart3 className="w-4 h-4" />
              Chart
            </button>
            <button
              onClick={() => addComponent('table')}
              className="w-full px-3 py-2 bg-gray-800 hover:bg-gray-700 rounded flex items-center gap-2 text-sm"
            >
              <Table className="w-4 h-4" />
              Table
            </button>
            <button
              onClick={() => addComponent('image')}
              className="w-full px-3 py-2 bg-gray-800 hover:bg-gray-700 rounded flex items-center gap-2 text-sm"
            >
              <ImageIcon className="w-4 h-4" />
              Image
            </button>
            <button
              onClick={() => addComponent('code')}
              className="w-full px-3 py-2 bg-gray-800 hover:bg-gray-700 rounded flex items-center gap-2 text-sm"
            >
              <Code className="w-4 h-4" />
              Code Block
            </button>
            <button
              onClick={() => addComponent('divider')}
              className="w-full px-3 py-2 bg-gray-800 hover:bg-gray-700 rounded flex items-center gap-2 text-sm"
            >
              <Layout className="w-4 h-4" />
              Divider
            </button>
          </div>

          <div className="mt-6">
            <h2 className="text-sm font-semibold mb-3">Report Settings</h2>
            <div className="space-y-3">
              <div>
                <label className="text-xs text-gray-400">Title</label>
                <input
                  type="text"
                  value={template.metadata.title}
                  onChange={(e) => setTemplate(prev => ({
                    ...prev,
                    metadata: { ...prev.metadata, title: e.target.value }
                  }))}
                  className="w-full px-2 py-1 bg-gray-800 border border-gray-700 rounded text-sm"
                />
              </div>
              <div>
                <label className="text-xs text-gray-400">Author</label>
                <input
                  type="text"
                  value={template.metadata.author}
                  onChange={(e) => setTemplate(prev => ({
                    ...prev,
                    metadata: { ...prev.metadata, author: e.target.value }
                  }))}
                  className="w-full px-2 py-1 bg-gray-800 border border-gray-700 rounded text-sm"
                />
              </div>
              <div>
                <label className="text-xs text-gray-400">Company</label>
                <input
                  type="text"
                  value={template.metadata.company}
                  onChange={(e) => setTemplate(prev => ({
                    ...prev,
                    metadata: { ...prev.metadata, company: e.target.value }
                  }))}
                  className="w-full px-2 py-1 bg-gray-800 border border-gray-700 rounded text-sm"
                />
              </div>
              <div>
                <label className="text-xs text-gray-400">Page Size</label>
                <select
                  value={template.styles.pageSize}
                  onChange={(e) => setTemplate(prev => ({
                    ...prev,
                    styles: { ...prev.styles, pageSize: e.target.value as any }
                  }))}
                  className="w-full px-2 py-1 bg-gray-800 border border-gray-700 rounded text-sm"
                >
                  <option value="A4">A4</option>
                  <option value="Letter">Letter</option>
                  <option value="Legal">Legal</option>
                </select>
              </div>
              <div>
                <label className="text-xs text-gray-400">Orientation</label>
                <select
                  value={template.styles.orientation}
                  onChange={(e) => setTemplate(prev => ({
                    ...prev,
                    styles: { ...prev.styles, orientation: e.target.value as any }
                  }))}
                  className="w-full px-2 py-1 bg-gray-800 border border-gray-700 rounded text-sm"
                >
                  <option value="portrait">Portrait</option>
                  <option value="landscape">Landscape</option>
                </select>
              </div>
            </div>
          </div>
        </div>

        {/* Canvas */}
        <div className="flex-1 overflow-y-auto p-6">
          <div className="max-w-4xl mx-auto bg-white text-black p-12 min-h-full shadow-2xl">
            {/* Report Header */}
            <div className="mb-8 border-b-2 border-gray-800 pb-4">
              <h1 className="text-3xl font-bold mb-2">{template.metadata.title}</h1>
              {template.metadata.company && (
                <p className="text-lg text-gray-600">{template.metadata.company}</p>
              )}
              <div className="flex justify-between text-sm text-gray-500 mt-2">
                {template.metadata.author && <span>By: {template.metadata.author}</span>}
                {template.metadata.date && <span>Date: {template.metadata.date}</span>}
              </div>
            </div>

            {/* Components */}
            {template.components.length === 0 ? (
              <div className="text-center py-12 text-gray-400">
                <FileText className="w-16 h-16 mx-auto mb-4 opacity-50" />
                <p>Add components from the left sidebar to build your report</p>
              </div>
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
                      onEdit={setSelectedComponent}
                      onDelete={deleteComponent}
                      onDuplicate={duplicateComponent}
                    />
                  ))}
                </SortableContext>
              </DndContext>
            )}
          </div>
        </div>

        {/* Properties Panel */}
        {selectedComp && (
          <div className="w-80 border-l border-gray-700 p-4 overflow-y-auto">
            <div className="flex items-center justify-between mb-4">
              <h2 className="text-sm font-semibold">Component Settings</h2>
              <button
                onClick={() => setSelectedComponent(null)}
                className="text-gray-400 hover:text-white"
              >
                ×
              </button>
            </div>

            <div className="space-y-4">
              {(selectedComp.type === 'heading' || selectedComp.type === 'text') && (
                <>
                  <div>
                    <label className="text-xs text-gray-400 mb-1 block">Content</label>
                    <textarea
                      value={selectedComp.content}
                      onChange={(e) => updateComponent(selectedComp.id, { content: e.target.value })}
                      className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-sm"
                      rows={4}
                    />
                  </div>
                  <div>
                    <label className="text-xs text-gray-400 mb-1 block">Alignment</label>
                    <select
                      value={selectedComp.config.alignment}
                      onChange={(e) => updateComponent(selectedComp.id, {
                        config: { ...selectedComp.config, alignment: e.target.value as any }
                      })}
                      className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-sm"
                    >
                      <option value="left">Left</option>
                      <option value="center">Center</option>
                      <option value="right">Right</option>
                    </select>
                  </div>
                </>
              )}

              {selectedComp.type === 'chart' && (
                <div>
                  <label className="text-xs text-gray-400 mb-1 block">Chart Type</label>
                  <select
                    value={selectedComp.config.chartType}
                    onChange={(e) => updateComponent(selectedComp.id, {
                      config: { ...selectedComp.config, chartType: e.target.value }
                    })}
                    className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-sm"
                  >
                    <option value="line">Line Chart</option>
                    <option value="bar">Bar Chart</option>
                    <option value="pie">Pie Chart</option>
                    <option value="area">Area Chart</option>
                  </select>
                </div>
              )}

              {selectedComp.type === 'code' && (
                <div>
                  <label className="text-xs text-gray-400 mb-1 block">Code</label>
                  <textarea
                    value={selectedComp.content}
                    onChange={(e) => updateComponent(selectedComp.id, { content: e.target.value })}
                    className="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-sm font-mono"
                    rows={8}
                    placeholder="Enter code here..."
                  />
                </div>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default ReportBuilderTab;
