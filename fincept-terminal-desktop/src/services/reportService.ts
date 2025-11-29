import { invoke } from '@tauri-apps/api/core';

export interface ReportComponent {
  id: string;
  type: 'text' | 'heading' | 'chart' | 'table' | 'image' | 'divider' | 'code';
  content: any;
  config: {
    fontSize?: string;
    fontWeight?: string;
    color?: string;
    alignment?: 'left' | 'center' | 'right';
    chartType?: string;
    columns?: string[];
    imageUrl?: string;
    code?: string;
    language?: string;
  };
}

export interface ReportTemplate {
  id: string;
  name: string;
  description: string;
  components: ReportComponent[];
  metadata: {
    author?: string;
    company?: string;
    date?: string;
    title?: string;
  };
  styles: {
    pageSize?: 'A4' | 'Letter' | 'Legal';
    orientation?: 'portrait' | 'landscape';
    margins?: string;
    headerFooter?: boolean;
  };
}

export interface ReportGenerationResult {
  success: boolean;
  output_path?: string;
  file_size?: number;
  html?: string;
  error?: string;
}

class ReportService {
  /**
   * Generate HTML from report template
   */
  async generateHTML(template: ReportTemplate): Promise<ReportGenerationResult> {
    try {
      const templateJson = JSON.stringify(template);
      const result = await invoke<string>('generate_report_html', {
        templateJson
      });

      const parsed = JSON.parse(result);
      return parsed;
    } catch (error) {
      console.error('Failed to generate HTML:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  /**
   * Generate PDF from report template
   */
  async generatePDF(template: ReportTemplate, outputPath: string): Promise<ReportGenerationResult> {
    try {
      const templateJson = JSON.stringify(template);
      const result = await invoke<string>('generate_report_pdf', {
        templateJson,
        outputPath
      });

      const parsed = JSON.parse(result);
      return parsed;
    } catch (error) {
      console.error('Failed to generate PDF:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  /**
   * Create default Jinja2 template
   */
  async createDefaultTemplate(): Promise<{ success: boolean; template_path?: string; error?: string }> {
    try {
      const result = await invoke<string>('create_default_report_template');
      const parsed = JSON.parse(result);
      return parsed;
    } catch (error) {
      console.error('Failed to create default template:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  /**
   * Save template to local storage
   */
  saveTemplate(template: ReportTemplate): void {
    const templates = this.getTemplates();
    const existingIndex = templates.findIndex(t => t.id === template.id);

    if (existingIndex >= 0) {
      templates[existingIndex] = template;
    } else {
      templates.push(template);
    }

    localStorage.setItem('financial_report_templates', JSON.stringify(templates));
  }

  /**
   * Get all saved templates
   */
  getTemplates(): ReportTemplate[] {
    const stored = localStorage.getItem('financial_report_templates');
    return stored ? JSON.parse(stored) : [];
  }

  /**
   * Get template by ID
   */
  getTemplate(id: string): ReportTemplate | null {
    const templates = this.getTemplates();
    return templates.find(t => t.id === id) || null;
  }

  /**
   * Delete template
   */
  deleteTemplate(id: string): void {
    const templates = this.getTemplates();
    const filtered = templates.filter(t => t.id !== id);
    localStorage.setItem('financial_report_templates', JSON.stringify(filtered));
  }

  /**
   * Get default template structure
   */
  getDefaultTemplate(): ReportTemplate {
    return {
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
    };
  }

  /**
   * Get sample financial report template
   */
  getSampleFinancialReport(): ReportTemplate {
    return {
      id: 'sample-financial-report',
      name: 'Sample Financial Analysis Report',
      description: 'Complete financial analysis with charts and tables',
      components: [
        {
          id: 'exec-summary-heading',
          type: 'heading',
          content: 'Executive Summary',
          config: { fontSize: 'xl', alignment: 'left' }
        },
        {
          id: 'exec-summary-text',
          type: 'text',
          content: 'This report provides a comprehensive analysis of the financial performance and market position of XYZ Corporation for the fiscal year 2024. Key findings include strong revenue growth, improved profitability margins, and strategic market expansion.',
          config: { alignment: 'left' }
        },
        {
          id: 'divider-1',
          type: 'divider',
          content: null,
          config: {}
        },
        {
          id: 'financial-highlights',
          type: 'heading',
          content: 'Financial Highlights',
          config: { fontSize: 'xl', alignment: 'left' }
        },
        {
          id: 'financial-table',
          type: 'table',
          content: {
            rows: [
              ['Revenue', '$1,250M', '+15.3%'],
              ['Net Income', '$187M', '+22.1%'],
              ['EBITDA', '$325M', '+18.7%'],
              ['EPS', '$4.23', '+19.5%']
            ]
          },
          config: {
            columns: ['Metric', 'Value', 'YoY Change']
          }
        },
        {
          id: 'market-analysis',
          type: 'heading',
          content: 'Market Analysis',
          config: { fontSize: 'xl', alignment: 'left' }
        },
        {
          id: 'market-text',
          type: 'text',
          content: 'The company has demonstrated strong market performance across all key segments, with particular strength in the technology sector. Market share has increased from 23% to 27% over the past year.',
          config: { alignment: 'left' }
        }
      ],
      metadata: {
        author: 'Financial Analysis Team',
        company: 'Fincept Corporation',
        date: new Date().toISOString().split('T')[0],
        title: 'Financial Analysis Report - XYZ Corporation',
      },
      styles: {
        pageSize: 'A4',
        orientation: 'portrait',
        margins: '20mm',
        headerFooter: true,
      },
    };
  }
}

export const reportService = new ReportService();
