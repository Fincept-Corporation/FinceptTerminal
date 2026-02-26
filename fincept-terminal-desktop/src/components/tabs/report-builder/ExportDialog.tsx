import React, { useState } from 'react';
import {
  Download,
  FileText,
  Code,
  FileType,
  Printer,
  X,
  Check,
  Loader2,
  Eye,
  Package,
} from 'lucide-react';
import { Dialog, DialogContent, DialogHeader, DialogTitle } from '@/components/ui/dialog';
import { Button } from '@/components/ui/button';
import { ReportTemplate } from '@/services/core/reportService';
import { BrandKit } from '@/services/core/brandKitService';
import { reportExportService, ExportFormat } from '@/services/core/reportExportService';
import { toast } from '@/components/ui/terminal-toast';
import { ReportBuilderMode, DocxDocumentMetadata } from './types';

interface ExportDialogProps {
  open: boolean;
  onClose: () => void;
  template: ReportTemplate;
  brandKit: BrandKit | null;
  // DOCX mode props
  builderMode?: ReportBuilderMode;
  docxHtmlContent?: string;
  docxMetadata?: DocxDocumentMetadata;
}

const EXPORT_OPTIONS: { format: ExportFormat; label: string; icon: React.ReactNode; desc: string }[] = [
  { format: 'pdf', label: 'PDF', icon: <FileText size={20} />, desc: 'Best for sharing and printing' },
  { format: 'html', label: 'HTML', icon: <Code size={20} />, desc: 'For web publishing' },
  { format: 'docx', label: 'Word', icon: <FileType size={20} />, desc: 'For editing in Word' },
  { format: 'markdown', label: 'Markdown', icon: <FileText size={20} />, desc: 'For documentation' },
];

export const ExportDialog: React.FC<ExportDialogProps> = ({ open, onClose, template, brandKit, builderMode, docxHtmlContent, docxMetadata }) => {
  const [selectedFormats, setSelectedFormats] = useState<ExportFormat[]>(['pdf']);
  const [isExporting, setIsExporting] = useState(false);
  const [showPreview, setShowPreview] = useState(false);
  const [previewHtml, setPreviewHtml] = useState('');

  const isDocxMode = builderMode === 'docx';

  const toggleFormat = (format: ExportFormat) => {
    setSelectedFormats(prev =>
      prev.includes(format) ? prev.filter(f => f !== format) : [...prev, format]
    );
  };

  const handleExport = async () => {
    if (selectedFormats.length === 0) {
      toast.error('Select at least one format');
      return;
    }

    setIsExporting(true);
    try {
      if (isDocxMode && docxHtmlContent) {
        // DOCX mode: export from TipTap HTML content
        if (selectedFormats.length === 1) {
          const path = await reportExportService.exportDocxModeToFile(docxHtmlContent, selectedFormats[0], docxMetadata);
          if (path) toast.success(`Exported to ${path}`);
        } else {
          for (const format of selectedFormats) {
            try {
              const path = await reportExportService.exportDocxModeToFile(docxHtmlContent, format, docxMetadata);
              if (path) toast.success(`Exported ${format.toUpperCase()} to ${path}`);
            } catch (error) {
              console.error(`Failed to export ${format}:`, error);
            }
          }
        }
      } else {
        // Builder mode: existing export logic
        if (selectedFormats.length === 1) {
          const path = await reportExportService.exportToFile(template, selectedFormats[0], brandKit);
          if (path) toast.success(`Exported to ${path}`);
        } else {
          const results = await reportExportService.batchExport(template, selectedFormats, brandKit);
          const success = Object.values(results).filter(Boolean).length;
          toast.success(`Exported ${success}/${selectedFormats.length} formats`);
        }
      }
      onClose();
    } catch (error) {
      toast.error('Export failed');
    } finally {
      setIsExporting(false);
    }
  };

  const handlePreview = () => {
    let html: string;
    if (isDocxMode && docxHtmlContent) {
      // Wrap DOCX HTML content in a basic printable page
      html = `<!DOCTYPE html><html><head><style>
        body { font-family: Arial, sans-serif; max-width: 210mm; margin: 0 auto; padding: 20mm; }
        table { border-collapse: collapse; width: 100%; } th, td { border: 1px solid #ccc; padding: 8px; }
        img { max-width: 100%; } h1,h2,h3,h4,h5,h6 { margin: 0.5em 0; }
      </style></head><body>${docxHtmlContent}</body></html>`;
    } else {
      html = reportExportService.getPrintPreviewHTML(template, brandKit);
    }
    setPreviewHtml(html);
    setShowPreview(true);
  };

  const handlePrint = () => {
    let html: string;
    if (isDocxMode && docxHtmlContent) {
      html = `<!DOCTYPE html><html><head><style>
        body { font-family: Arial, sans-serif; max-width: 210mm; margin: 0 auto; padding: 20mm; }
        table { border-collapse: collapse; width: 100%; } th, td { border: 1px solid #ccc; padding: 8px; }
        img { max-width: 100%; } h1,h2,h3,h4,h5,h6 { margin: 0.5em 0; }
      </style></head><body>${docxHtmlContent}</body></html>`;
    } else {
      html = reportExportService.getPrintPreviewHTML(template, brandKit);
    }
    const printWindow = window.open('', '_blank');
    if (printWindow) {
      printWindow.document.write(html);
      printWindow.document.close();
      printWindow.print();
    }
  };

  return (
    <>
      <Dialog open={open && !showPreview} onOpenChange={onClose}>
        <DialogContent className="bg-[#1a1a1a] border-[#333333] max-w-md">
          <DialogHeader>
            <DialogTitle className="text-[#FFA500] flex items-center gap-2">
              <Download size={20} />
              Export Report
            </DialogTitle>
          </DialogHeader>

          <div className="space-y-4 py-4">
            {/* Format Selection */}
            <div className="space-y-2">
              <label className="text-xs text-gray-400">Select Format(s)</label>
              <div className="grid grid-cols-2 gap-2">
                {EXPORT_OPTIONS.map(({ format, label, icon, desc }) => (
                  <button
                    key={format}
                    onClick={() => toggleFormat(format)}
                    className={`p-3 rounded border text-left transition-all ${
                      selectedFormats.includes(format)
                        ? 'border-[#FFA500] bg-[#FFA500]/10'
                        : 'border-[#333333] hover:border-[#555555]'
                    }`}
                  >
                    <div className="flex items-center gap-2 mb-1">
                      <span className={selectedFormats.includes(format) ? 'text-[#FFA500]' : 'text-gray-400'}>
                        {icon}
                      </span>
                      <span className="text-sm font-medium text-white">{label}</span>
                      {selectedFormats.includes(format) && (
                        <Check size={14} className="ml-auto text-[#FFA500]" />
                      )}
                    </div>
                    <p className="text-[10px] text-gray-500">{desc}</p>
                  </button>
                ))}
              </div>
            </div>

            {/* Batch Export Info */}
            {selectedFormats.length > 1 && (
              <div className="flex items-center gap-2 p-2 bg-[#0a0a0a] rounded text-xs text-gray-400">
                <Package size={14} className="text-[#FFA500]" />
                Batch export: {selectedFormats.length} formats selected
              </div>
            )}

            {/* Actions */}
            <div className="flex gap-2 pt-2">
              <Button
                variant="outline"
                onClick={handlePreview}
                className="flex-1 bg-transparent border-[#333333] text-white hover:bg-[#2a2a2a]"
              >
                <Eye size={16} className="mr-2" />
                Preview
              </Button>
              <Button
                variant="outline"
                onClick={handlePrint}
                className="flex-1 bg-transparent border-[#333333] text-white hover:bg-[#2a2a2a]"
              >
                <Printer size={16} className="mr-2" />
                Print
              </Button>
            </div>

            <Button
              onClick={handleExport}
              disabled={isExporting || selectedFormats.length === 0}
              className="w-full bg-[#FFA500] text-black hover:bg-[#ff8c00]"
            >
              {isExporting ? (
                <>
                  <Loader2 size={16} className="mr-2 animate-spin" />
                  Exporting...
                </>
              ) : (
                <>
                  <Download size={16} className="mr-2" />
                  Export {selectedFormats.length > 1 ? `(${selectedFormats.length})` : ''}
                </>
              )}
            </Button>
          </div>
        </DialogContent>
      </Dialog>

      {/* Print Preview Modal */}
      <Dialog open={showPreview} onOpenChange={() => setShowPreview(false)}>
        <DialogContent className="bg-[#1a1a1a] border-[#333333] max-w-4xl max-h-[90vh]">
          <DialogHeader>
            <DialogTitle className="text-[#FFA500] flex items-center justify-between">
              <span className="flex items-center gap-2">
                <Eye size={20} />
                Print Preview
              </span>
              <div className="flex gap-2">
                <Button
                  size="sm"
                  onClick={handlePrint}
                  className="bg-[#FFA500] text-black hover:bg-[#ff8c00]"
                >
                  <Printer size={14} className="mr-1" />
                  Print
                </Button>
                <Button
                  size="sm"
                  variant="outline"
                  onClick={() => setShowPreview(false)}
                  className="bg-transparent border-[#333333] text-white"
                >
                  <X size={14} />
                </Button>
              </div>
            </DialogTitle>
          </DialogHeader>

          <div className="overflow-auto max-h-[70vh] bg-gray-200 p-4 rounded">
            <iframe
              srcDoc={previewHtml}
              className="w-full bg-white shadow-lg"
              style={{ height: '800px', border: 'none' }}
              title="Print Preview"
            />
          </div>
        </DialogContent>
      </Dialog>
    </>
  );
};

export default ExportDialog;
