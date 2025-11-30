import { invoke } from '@tauri-apps/api/core';
import { open as shellOpen } from '@tauri-apps/plugin-shell';
import { sqliteService } from './sqliteService';

export interface ReportComponent {
  id: string;
  type: 'text' | 'heading' | 'subheading' | 'chart' | 'table' | 'image' | 'divider' | 'code' | 'section' | 'columns' | 'coverpage' | 'pagebreak';
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
    // Layout configs
    columnCount?: 2 | 3;
    backgroundColor?: string;
    borderColor?: string;
    padding?: string;
    // Section configs
    sectionTitle?: string;
    sectionStyle?: 'default' | 'highlight' | 'sidebar';
    // Cover page configs
    subtitle?: string;
    logo?: string;
  };
  // For column and section types that contain other components
  children?: ReportComponent[];
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
  created_at?: string;
  updated_at?: string;
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
   * Open folder in file explorer (cross-platform)
   */
  async openInFolder(filePath: string): Promise<boolean> {
    try {
      console.log('[ReportService] Opening folder for:', filePath);

      // Extract directory from file path
      const pathParts = filePath.replace(/\\/g, '/').split('/');
      pathParts.pop(); // Remove filename
      const folderPath = pathParts.join('/');

      // Detect platform
      const platform = await this.getPlatform();
      console.log('[ReportService] Platform:', platform);

      if (platform === 'windows') {
        // Windows: Use explorer with /select flag to highlight the file
        // Need to use native Windows path separators
        const windowsPath = filePath.replace(/\//g, '\\');
        await shellOpen(`explorer /select,"${windowsPath}"`);
      } else if (platform === 'darwin') {
        // macOS: Use open with -R flag to reveal in Finder
        await shellOpen(`open -R "${filePath}"`);
      } else {
        // Linux: Open the folder
        await shellOpen(`xdg-open "${folderPath}"`);
      }

      return true;
    } catch (error) {
      console.error('Failed to open folder:', error);
      return false;
    }
  }

  /**
   * Get current platform
   */
  private async getPlatform(): Promise<string> {
    try {
      const platform = await invoke<string>('plugin:os|platform');
      return platform;
    } catch {
      // Fallback to navigator
      const userAgent = navigator.userAgent.toLowerCase();
      if (userAgent.includes('win')) return 'windows';
      if (userAgent.includes('mac')) return 'darwin';
      return 'linux';
    }
  }

  /**
   * Save template to SQLite database
   */
  async saveTemplate(template: ReportTemplate): Promise<void> {
    await sqliteService.initialize();
    const now = new Date().toISOString();

    const templateData = {
      ...template,
      updated_at: now,
      created_at: template.created_at || now
    };

    const templateJson = JSON.stringify({
      components: templateData.components,
      metadata: templateData.metadata,
      styles: templateData.styles
    });

    // For now, use localStorage until SQLite service is fully integrated
    try {
      const templates = this.getTemplatesFromLocalStorage();
      const existingIndex = templates.findIndex(t => t.id === template.id);

      if (existingIndex >= 0) {
        templates[existingIndex] = templateData;
      } else {
        templates.push(templateData);
      }

      localStorage.setItem('financial_report_templates', JSON.stringify(templates));
    } catch (error) {
      console.error('Failed to save template:', error);
      throw error;
    }
  }

  private getTemplatesFromLocalStorage(): ReportTemplate[] {
    const stored = localStorage.getItem('financial_report_templates');
    return stored ? JSON.parse(stored) : [];
  }

  /**
   * Get all saved templates
   */
  async getTemplates(): Promise<ReportTemplate[]> {
    return this.getTemplatesFromLocalStorage();
  }

  /**
   * Get template by ID
   */
  async getTemplate(id: string): Promise<ReportTemplate | null> {
    const templates = this.getTemplatesFromLocalStorage();
    return templates.find(t => t.id === id) || null;
  }

  /**
   * Delete template
   */
  async deleteTemplate(id: string): Promise<void> {
    const templates = this.getTemplatesFromLocalStorage();
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
   * Get professional equity research report template
   */
  getEquityResearchTemplate(): ReportTemplate {
    return {
      id: 'equity-research-template',
      name: 'Equity Research Report',
      description: 'Professional equity research report with multi-column layout',
      components: [
        {
          id: 'cover-page',
          type: 'coverpage',
          content: null,
          config: {
            subtitle: 'Equity Research | Technology Sector',
            backgroundColor: '#0a0a0a',
            color: '#FFA500'
          }
        },
        {
          id: 'page-break-1',
          type: 'pagebreak',
          content: null,
          config: {}
        },
        {
          id: 'exec-summary-section',
          type: 'section',
          content: null,
          config: {
            sectionTitle: 'EXECUTIVE SUMMARY',
            sectionStyle: 'highlight',
            backgroundColor: '#f8f9fa',
            borderColor: '#FFA500',
            padding: '20px'
          },
          children: [
            {
              id: 'exec-summary-content',
              type: 'text',
              content: 'We initiate coverage on XYZ Corporation with a BUY rating and 12-month price target of $185. The company demonstrates strong fundamentals with market-leading positions in cloud infrastructure and AI services. Key investment highlights include accelerating revenue growth, margin expansion, and strategic acquisitions.',
              config: { fontSize: 'medium', alignment: 'left' }
            }
          ]
        },
        {
          id: 'investment-highlights',
          type: 'columns',
          content: null,
          config: {
            columnCount: 2,
            padding: '15px'
          },
          children: [
            {
              id: 'col1-heading',
              type: 'subheading',
              content: 'Bull Case',
              config: { fontSize: 'medium', color: '#10b981', fontWeight: 'bold' }
            },
            {
              id: 'col1-text',
              type: 'text',
              content: '• Market share gains in cloud computing\n• Strong pricing power and gross margin expansion\n• Robust free cash flow generation\n• Strategic AI partnerships',
              config: { fontSize: 'small' }
            },
            {
              id: 'col2-heading',
              type: 'subheading',
              content: 'Bear Case',
              config: { fontSize: 'medium', color: '#ef4444', fontWeight: 'bold' }
            },
            {
              id: 'col2-text',
              type: 'text',
              content: '• Regulatory scrutiny in key markets\n• Intensifying competition\n• Currency headwinds\n• Execution risks on large projects',
              config: { fontSize: 'small' }
            }
          ]
        },
        {
          id: 'financial-metrics',
          type: 'heading',
          content: 'Key Financial Metrics',
          config: { fontSize: 'xl', alignment: 'left' }
        },
        {
          id: 'metrics-3col',
          type: 'columns',
          content: null,
          config: {
            columnCount: 3
          },
          children: [
            {
              id: 'metric1',
              type: 'text',
              content: 'Revenue Growth\n+23.5% YoY',
              config: { alignment: 'center', fontWeight: 'bold', fontSize: 'medium' }
            },
            {
              id: 'metric2',
              type: 'text',
              content: 'Operating Margin\n32.1%',
              config: { alignment: 'center', fontWeight: 'bold', fontSize: 'medium' }
            },
            {
              id: 'metric3',
              type: 'text',
              content: 'FCF Yield\n4.8%',
              config: { alignment: 'center', fontWeight: 'bold', fontSize: 'medium' }
            }
          ]
        }
      ],
      metadata: {
        author: 'Fincept Research',
        company: 'Fincept Corporation',
        date: new Date().toISOString().split('T')[0],
        title: 'XYZ Corp - Initiate with BUY',
      },
      styles: {
        pageSize: 'A4',
        orientation: 'portrait',
        margins: '15mm',
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
