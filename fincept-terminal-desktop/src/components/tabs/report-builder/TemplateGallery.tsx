import React, { useState, useEffect } from 'react';
import {
  LayoutTemplate,
  FileText,
  Download,
  Upload,
  Plus,
  Search,
  X,
  Check,
  Eye,
  Trash2,
  Copy,
} from 'lucide-react';
import { Dialog, DialogContent, DialogHeader, DialogTitle } from '@/components/ui/dialog';
import { Button } from '@/components/ui/button';
import { ReportTemplate } from '@/services/core/reportService';
import { reportService } from '@/services/core/reportService';
import { save, open as openDialog } from '@tauri-apps/plugin-dialog';
import { writeTextFile, readTextFile } from '@tauri-apps/plugin-fs';
import { toast } from 'sonner';

// Export template type for external use
export type ReportTemplateType = Partial<ReportTemplate>;

// Pre-built Templates
const PRE_BUILT_TEMPLATES: ReportTemplateType[] = [
  {
    id: 'equity-research',
    name: 'Equity Research Report',
    description: 'Professional equity research with buy/sell recommendations',
    components: [
      { id: 'c1', type: 'coverpage', content: '', config: { subtitle: 'Equity Research', backgroundColor: '#0a0a0a' } },
      { id: 'c2', type: 'pagebreak', content: '', config: {} },
      { id: 'c3', type: 'heading', content: 'Executive Summary', config: { fontSize: '2xl' } },
      { id: 'c4', type: 'text', content: 'Investment thesis and key recommendations...', config: {} },
      { id: 'c5', type: 'divider', content: '', config: {} },
      { id: 'c6', type: 'heading', content: 'Financial Analysis', config: { fontSize: '2xl' } },
      { id: 'c7', type: 'table', content: '', config: { columns: ['Metric', 'FY23', 'FY24E', 'FY25E'] } },
      { id: 'c8', type: 'chart', content: '', config: { chartType: 'line' } },
    ],
  },
  {
    id: 'quarterly-report',
    name: 'Quarterly Report',
    description: 'Standard quarterly financial report template',
    components: [
      { id: 'c1', type: 'heading', content: 'Q4 2024 Financial Report', config: { fontSize: '3xl', alignment: 'center' } },
      { id: 'c2', type: 'divider', content: '', config: {} },
      { id: 'c3', type: 'heading', content: 'Financial Highlights', config: { fontSize: 'xl' } },
      { id: 'c4', type: 'columns', content: '', config: { columnCount: 3 }, children: [] },
      { id: 'c5', type: 'heading', content: 'Revenue Analysis', config: { fontSize: 'xl' } },
      { id: 'c6', type: 'chart', content: '', config: { chartType: 'bar' } },
      { id: 'c7', type: 'heading', content: 'Outlook', config: { fontSize: 'xl' } },
      { id: 'c8', type: 'text', content: 'Management guidance and forward-looking statements...', config: {} },
    ],
  },
  {
    id: 'investment-memo',
    name: 'Investment Memo',
    description: 'Concise investment memorandum for decision makers',
    components: [
      { id: 'c1', type: 'heading', content: 'Investment Memorandum', config: { fontSize: '2xl' } },
      { id: 'c2', type: 'section', content: '', config: { sectionTitle: 'RECOMMENDATION', borderColor: '#10B981', backgroundColor: '#E8F5E9' } },
      { id: 'c3', type: 'heading', content: 'Investment Thesis', config: { fontSize: 'xl' } },
      { id: 'c4', type: 'text', content: 'Core investment rationale...', config: {} },
      { id: 'c5', type: 'columns', content: '', config: { columnCount: 2 }, children: [] },
      { id: 'c6', type: 'heading', content: 'Risk Factors', config: { fontSize: 'xl' } },
      { id: 'c7', type: 'text', content: 'Key risks to monitor...', config: {} },
    ],
  },
  {
    id: 'due-diligence',
    name: 'Due Diligence Report',
    description: 'Comprehensive due diligence checklist and analysis',
    components: [
      { id: 'c1', type: 'coverpage', content: '', config: { subtitle: 'Due Diligence Report', backgroundColor: '#1a1a2e' } },
      { id: 'c2', type: 'pagebreak', content: '', config: {} },
      { id: 'c3', type: 'heading', content: 'Company Overview', config: { fontSize: '2xl' } },
      { id: 'c4', type: 'table', content: '', config: { columns: ['Item', 'Details', 'Status'] } },
      { id: 'c5', type: 'heading', content: 'Financial Review', config: { fontSize: '2xl' } },
      { id: 'c6', type: 'text', content: 'Detailed financial analysis...', config: {} },
      { id: 'c7', type: 'heading', content: 'Legal & Compliance', config: { fontSize: '2xl' } },
      { id: 'c8', type: 'text', content: 'Legal review findings...', config: {} },
    ],
  },
  {
    id: 'market-analysis',
    name: 'Market Analysis',
    description: 'Industry and market analysis report',
    components: [
      { id: 'c1', type: 'heading', content: 'Market Analysis Report', config: { fontSize: '2xl', alignment: 'center' } },
      { id: 'c2', type: 'text', content: 'Comprehensive market overview and trends...', config: { alignment: 'center' } },
      { id: 'c3', type: 'divider', content: '', config: {} },
      { id: 'c4', type: 'heading', content: 'Market Size & Growth', config: { fontSize: 'xl' } },
      { id: 'c5', type: 'chart', content: '', config: { chartType: 'area' } },
      { id: 'c6', type: 'heading', content: 'Competitive Landscape', config: { fontSize: 'xl' } },
      { id: 'c7', type: 'chart', content: '', config: { chartType: 'pie' } },
    ],
  },
  {
    id: 'portfolio-summary',
    name: 'Portfolio Summary',
    description: 'Portfolio holdings and performance summary',
    components: [
      { id: 'c1', type: 'heading', content: 'Portfolio Summary', config: { fontSize: '2xl' } },
      { id: 'c2', type: 'columns', content: '', config: { columnCount: 3 }, children: [] },
      { id: 'c3', type: 'heading', content: 'Holdings', config: { fontSize: 'xl' } },
      { id: 'c4', type: 'table', content: '', config: { columns: ['Symbol', 'Shares', 'Value', 'Weight', 'Return'] } },
      { id: 'c5', type: 'heading', content: 'Performance', config: { fontSize: 'xl' } },
      { id: 'c6', type: 'chart', content: '', config: { chartType: 'line' } },
    ],
  },
];

interface TemplateGalleryProps {
  open: boolean;
  onClose: () => void;
  onSelectTemplate: (template: ReportTemplate) => void;
  currentTemplate?: ReportTemplate;
}

export const TemplateGallery: React.FC<TemplateGalleryProps> = ({
  open,
  onClose,
  onSelectTemplate,
  currentTemplate,
}) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [savedTemplates, setSavedTemplates] = useState<ReportTemplate[]>([]);
  const [selectedCategory, setSelectedCategory] = useState<'all' | 'prebuilt' | 'saved'>('all');
  const [previewTemplate, setPreviewTemplate] = useState<Partial<ReportTemplate> | null>(null);

  useEffect(() => {
    loadSavedTemplates();
  }, [open]);

  const loadSavedTemplates = async () => {
    try {
      const templates = await reportService.getTemplates();
      setSavedTemplates(templates);
    } catch (error) {
      console.error('Failed to load templates:', error);
    }
  };

  const handleUseTemplate = (template: Partial<ReportTemplate>) => {
    const fullTemplate: ReportTemplate = {
      id: `report-${Date.now()}`,
      name: template.name || 'New Report',
      description: template.description || '',
      components: template.components || [],
      metadata: {
        author: '',
        company: '',
        date: new Date().toISOString().split('T')[0],
        title: template.name || 'New Report',
      },
      styles: {
        pageSize: 'A4',
        orientation: 'portrait',
        margins: '20mm',
        headerFooter: true,
      },
    };
    onSelectTemplate(fullTemplate);
    onClose();
    toast.success(`Template "${template.name}" applied`);
  };

  const handleExportTemplate = async () => {
    if (!currentTemplate) return;
    try {
      const filePath = await save({
        filters: [{ name: 'JSON', extensions: ['json'] }],
        defaultPath: `${currentTemplate.name}.json`,
      });
      if (filePath) {
        await writeTextFile(filePath, JSON.stringify(currentTemplate, null, 2));
        toast.success('Template exported');
      }
    } catch (error) {
      toast.error('Export failed');
    }
  };

  const handleImportTemplate = async () => {
    try {
      const filePath = await openDialog({
        filters: [{ name: 'JSON', extensions: ['json'] }],
        multiple: false,
      });
      if (filePath && typeof filePath === 'string') {
        const content = await readTextFile(filePath);
        const template = JSON.parse(content) as ReportTemplate;
        template.id = `report-${Date.now()}`;
        onSelectTemplate(template);
        onClose();
        toast.success('Template imported');
      }
    } catch (error) {
      toast.error('Import failed');
    }
  };

  const handleDeleteSaved = async (id: string) => {
    try {
      await reportService.deleteTemplate(id);
      loadSavedTemplates();
      toast.success('Template deleted');
    } catch (error) {
      toast.error('Delete failed');
    }
  };

  const filteredPrebuilt = PRE_BUILT_TEMPLATES.filter(t =>
    t.name?.toLowerCase().includes(searchQuery.toLowerCase()) ||
    t.description?.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const filteredSaved = savedTemplates.filter(t =>
    t.name.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const showPrebuilt = selectedCategory === 'all' || selectedCategory === 'prebuilt';
  const showSaved = selectedCategory === 'all' || selectedCategory === 'saved';

  return (
    <Dialog open={open} onOpenChange={onClose}>
      <DialogContent className="bg-[#1a1a1a] border-[#333333] max-w-4xl max-h-[85vh]">
        <DialogHeader>
          <DialogTitle className="text-[#FFA500] flex items-center gap-2">
            <LayoutTemplate size={20} />
            Template Gallery
          </DialogTitle>
        </DialogHeader>

        <div className="flex flex-col gap-4">
          {/* Top Bar */}
          <div className="flex items-center gap-3">
            {/* Search */}
            <div className="flex-1 relative">
              <Search size={14} className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-500" />
              <input
                type="text"
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                placeholder="Search templates..."
                className="w-full pl-9 pr-3 py-2 text-xs bg-[#0a0a0a] border border-[#333333] rounded text-white focus:border-[#FFA500] outline-none"
              />
            </div>

            {/* Category Filter */}
            <div className="flex gap-1">
              {(['all', 'prebuilt', 'saved'] as const).map(cat => (
                <button
                  key={cat}
                  onClick={() => setSelectedCategory(cat)}
                  className={`px-3 py-2 text-xs rounded capitalize ${
                    selectedCategory === cat
                      ? 'bg-[#FFA500] text-black'
                      : 'bg-[#0a0a0a] text-gray-400 hover:bg-[#2a2a2a]'
                  }`}
                >
                  {cat}
                </button>
              ))}
            </div>

            {/* Import/Export */}
            <Button
              size="sm"
              variant="outline"
              onClick={handleImportTemplate}
              className="bg-transparent border-[#333333] text-white text-xs"
            >
              <Upload size={14} className="mr-1" />
              Import
            </Button>
            {currentTemplate && (
              <Button
                size="sm"
                variant="outline"
                onClick={handleExportTemplate}
                className="bg-transparent border-[#333333] text-white text-xs"
              >
                <Download size={14} className="mr-1" />
                Export
              </Button>
            )}
          </div>

          {/* Templates Grid */}
          <div className="overflow-y-auto max-h-[55vh] pr-2">
            {/* Pre-built Templates */}
            {showPrebuilt && filteredPrebuilt.length > 0 && (
              <div className="mb-6">
                <h3 className="text-xs font-semibold text-[#FFA500] mb-3">PRE-BUILT TEMPLATES</h3>
                <div className="grid grid-cols-3 gap-3">
                  {filteredPrebuilt.map(template => (
                    <TemplateCard
                      key={template.id}
                      template={template}
                      onUse={() => handleUseTemplate(template)}
                      onPreview={() => setPreviewTemplate(template)}
                    />
                  ))}
                </div>
              </div>
            )}

            {/* Saved Templates */}
            {showSaved && (
              <div>
                <h3 className="text-xs font-semibold text-[#FFA500] mb-3">SAVED TEMPLATES</h3>
                {filteredSaved.length === 0 ? (
                  <p className="text-xs text-gray-500 text-center py-8">No saved templates</p>
                ) : (
                  <div className="grid grid-cols-3 gap-3">
                    {filteredSaved.map(template => (
                      <TemplateCard
                        key={template.id}
                        template={template}
                        onUse={() => handleUseTemplate(template)}
                        onPreview={() => setPreviewTemplate(template)}
                        onDelete={() => handleDeleteSaved(template.id)}
                        isSaved
                      />
                    ))}
                  </div>
                )}
              </div>
            )}
          </div>
        </div>

        {/* Preview Modal */}
        {previewTemplate && (
          <Dialog open={!!previewTemplate} onOpenChange={() => setPreviewTemplate(null)}>
            <DialogContent className="bg-[#1a1a1a] border-[#333333] max-w-2xl">
              <DialogHeader>
                <DialogTitle className="text-white">{previewTemplate.name}</DialogTitle>
              </DialogHeader>
              <div className="space-y-3">
                <p className="text-xs text-gray-400">{previewTemplate.description}</p>
                <div className="bg-[#0a0a0a] rounded p-4 max-h-[400px] overflow-y-auto">
                  <p className="text-xs text-gray-500 mb-2">Components:</p>
                  {previewTemplate.components?.map((comp, idx) => (
                    <div key={idx} className="flex items-center gap-2 py-1 text-xs text-gray-300">
                      <span className="w-4 h-4 rounded bg-[#FFA500]/20 text-[#FFA500] flex items-center justify-center text-[10px]">
                        {idx + 1}
                      </span>
                      <span className="capitalize">{comp.type}</span>
                      {comp.content && typeof comp.content === 'string' && (
                        <span className="text-gray-500 truncate max-w-[200px]">- {comp.content}</span>
                      )}
                    </div>
                  ))}
                </div>
                <div className="flex justify-end gap-2">
                  <Button
                    variant="outline"
                    onClick={() => setPreviewTemplate(null)}
                    className="bg-transparent border-[#333333] text-white"
                  >
                    Cancel
                  </Button>
                  <Button
                    onClick={() => {
                      handleUseTemplate(previewTemplate);
                      setPreviewTemplate(null);
                    }}
                    className="bg-[#FFA500] text-black"
                  >
                    <Check size={14} className="mr-1" />
                    Use Template
                  </Button>
                </div>
              </div>
            </DialogContent>
          </Dialog>
        )}
      </DialogContent>
    </Dialog>
  );
};

// Template Card Component
interface TemplateCardProps {
  template: Partial<ReportTemplate>;
  onUse: () => void;
  onPreview: () => void;
  onDelete?: () => void;
  isSaved?: boolean;
}

const TemplateCard: React.FC<TemplateCardProps> = ({ template, onUse, onPreview, onDelete, isSaved }) => (
  <div className="bg-[#0a0a0a] border border-[#333333] rounded-lg p-3 hover:border-[#FFA500] transition-all group">
    {/* Preview Area */}
    <div className="h-24 bg-[#1a1a1a] rounded mb-3 flex items-center justify-center relative overflow-hidden">
      <FileText size={32} className="text-[#333333]" />
      <div className="absolute inset-0 bg-black/50 opacity-0 group-hover:opacity-100 transition-opacity flex items-center justify-center gap-2">
        <button
          onClick={onPreview}
          className="p-2 bg-[#2a2a2a] rounded hover:bg-[#3a3a3a]"
          title="Preview"
        >
          <Eye size={14} className="text-white" />
        </button>
        <button
          onClick={onUse}
          className="p-2 bg-[#FFA500] rounded hover:bg-[#ff8c00]"
          title="Use Template"
        >
          <Check size={14} className="text-black" />
        </button>
      </div>
    </div>

    {/* Info */}
    <h4 className="text-sm font-semibold text-white mb-1 truncate">{template.name}</h4>
    <p className="text-[10px] text-gray-500 line-clamp-2 h-8">{template.description}</p>

    {/* Footer */}
    <div className="flex items-center justify-between mt-2 pt-2 border-t border-[#333333]">
      <span className="text-[10px] text-gray-500">
        {template.components?.length || 0} components
      </span>
      {isSaved && onDelete && (
        <button
          onClick={(e) => { e.stopPropagation(); onDelete(); }}
          className="p-1 text-gray-500 hover:text-red-500"
        >
          <Trash2 size={12} />
        </button>
      )}
    </div>
  </div>
);

export default TemplateGallery;
